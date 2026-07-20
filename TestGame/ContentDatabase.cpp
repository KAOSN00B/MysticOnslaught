#include "ContentDatabase.h"
#include "ContentSerializers.h"

#include <filesystem>

namespace fs = std::filesystem;

ContentDatabase::ContentDatabase(std::string contentRootFolder)
    : _contentRootFolder(std::move(contentRootFolder))
{
}

// -- Paths ---------------------------------------------------------------------

std::string ContentDatabase::GameplayAssetFilePath(const ContentId& id) const
{
    return _contentRootFolder + "/GameplayAssets/" + id + ".gasset";
}

std::string ContentDatabase::ProjectileFilePath(const ContentId& id) const
{
    return _contentRootFolder + "/Projectiles/" + id + ".projectile";
}

std::string ContentDatabase::EnemyFilePath(const ContentId& id) const
{
    return _contentRootFolder + "/Enemies/" + id + ".enemy";
}

// -- Scanning ------------------------------------------------------------------

// Loads every file with `extension` in `folder`, forwarding each parsed
// document to `loadOne`. Broken files are recorded and skipped.
template <typename LoadOneFunction>
static void ScanFolder(const std::string& folder, const std::string& extension,
                       std::vector<ContentError>& outErrors, LoadOneFunction loadOne)
{
    std::error_code filesystemError;
    if (!fs::exists(folder, filesystemError))
        return;   // an absent folder simply means no content of this type yet

    for (const fs::directory_entry& entry : fs::directory_iterator(folder, filesystemError))
    {
        if (!entry.is_regular_file() || entry.path().extension() != extension)
            continue;
        ContentDocument document;
        ContentError error;
        if (!document.LoadFromFile(entry.path().string(), error))
        {
            outErrors.push_back(error);
            continue;
        }
        loadOne(document, entry.path().string());
    }
}

int ContentDatabase::ScanAll(std::vector<ContentError>& outErrors)
{
    _gameplayAssets.clear();
    _projectiles.clear();
    _enemies.clear();

    ScanFolder(_contentRootFolder + "/GameplayAssets", ".gasset", outErrors,
        [&](ContentDocument& document, const std::string& filePath)
        {
            GameplayAssetDefinition definition;
            ContentError error;
            if (!LoadGameplayAssetFromDocument(document, definition, error))
            {
                error.filePath = filePath;
                outErrors.push_back(error);
                return;
            }
            if (_gameplayAssets.count(definition.id))
            {
                outErrors.push_back({ "duplicate gameplay asset id '" + definition.id + "'", filePath, 0 });
                return;
            }
            _gameplayAssets[definition.id] = definition;
        });

    ScanFolder(_contentRootFolder + "/Projectiles", ".projectile", outErrors,
        [&](ContentDocument& document, const std::string& filePath)
        {
            ProjectileDefinition definition;
            ContentError error;
            if (!LoadProjectileFromDocument(document, definition, error))
            {
                error.filePath = filePath;
                outErrors.push_back(error);
                return;
            }
            if (_projectiles.count(definition.id))
            {
                outErrors.push_back({ "duplicate projectile id '" + definition.id + "'", filePath, 0 });
                return;
            }
            _projectiles[definition.id] = definition;
        });

    ScanFolder(_contentRootFolder + "/Enemies", ".enemy", outErrors,
        [&](ContentDocument& document, const std::string& filePath)
        {
            EnemyDefinition definition;
            ContentError error;
            if (!LoadEnemyFromDocument(document, definition, error))
            {
                error.filePath = filePath;
                outErrors.push_back(error);
                return;
            }
            if (_enemies.count(definition.id))
            {
                outErrors.push_back({ "duplicate enemy id '" + definition.id + "'", filePath, 0 });
                return;
            }
            _enemies[definition.id] = definition;
        });

    return GameplayAssetCount() + ProjectileCount() + EnemyCount();
}

// -- Lookup --------------------------------------------------------------------

const GameplayAssetDefinition* ContentDatabase::FindGameplayAsset(const ContentId& id) const
{
    auto found = _gameplayAssets.find(id);
    return found != _gameplayAssets.end() ? &found->second : nullptr;
}

const ProjectileDefinition* ContentDatabase::FindProjectile(const ContentId& id) const
{
    auto found = _projectiles.find(id);
    return found != _projectiles.end() ? &found->second : nullptr;
}

const EnemyDefinition* ContentDatabase::FindEnemy(const ContentId& id) const
{
    auto found = _enemies.find(id);
    return found != _enemies.end() ? &found->second : nullptr;
}

std::vector<ContentId> ContentDatabase::AllGameplayAssetIds() const
{
    std::vector<ContentId> ids;
    for (const auto& pair : _gameplayAssets)
        ids.push_back(pair.first);
    return ids;
}

std::vector<ContentId> ContentDatabase::AllProjectileIds() const
{
    std::vector<ContentId> ids;
    for (const auto& pair : _projectiles)
        ids.push_back(pair.first);
    return ids;
}

// -- Dependency index ----------------------------------------------------------

std::vector<ContentReference> ContentDatabase::FindReferencesTo(const ContentId& id) const
{
    std::vector<ContentReference> references;

    // Gameplay assets reference projectiles through their emitter.
    for (const auto& pair : _gameplayAssets)
    {
        const GameplayAssetDefinition& asset = pair.second;
        if (asset.emitter && asset.emitter->projectileId == id)
            references.push_back({ asset.id, "gasset", "emitter.projectile" });
    }

    // Enemies reference projectiles through their reference list.
    for (const auto& pair : _enemies)
    {
        const EnemyDefinition& enemy = pair.second;
        for (const ContentId& projectileId : enemy.projectileIds)
            if (projectileId == id)
                references.push_back({ enemy.id, "enemy", "projectiles" });
    }

    // Room instances join this index in Project 5 when RoomBlueprint gains its
    // gameplay-object collection.
    return references;
}

// -- Validation ----------------------------------------------------------------

ContentValidationReport ContentDatabase::ValidateAll() const
{
    ContentValidationReport report;

    for (const auto& pair : _gameplayAssets)
    {
        const GameplayAssetDefinition& asset = pair.second;
        ValidateGameplayAssetDefinition(asset, report);
        // Cross-reference check: the emitter's projectile must exist.
        if (asset.emitter && !asset.emitter->projectileId.empty() &&
            !FindProjectile(asset.emitter->projectileId))
        {
            report.AddError(asset.id, "emitter.projectile",
                            "references missing projectile '" + asset.emitter->projectileId + "'");
        }
    }

    for (const auto& pair : _projectiles)
        ValidateProjectileDefinition(pair.second, report);

    for (const auto& pair : _enemies)
    {
        const EnemyDefinition& enemy = pair.second;
        ValidateEnemyDefinition(enemy, report);
        for (const ContentId& projectileId : enemy.projectileIds)
            if (!FindProjectile(projectileId))
                report.AddError(enemy.id, "projectiles",
                                "references missing projectile '" + projectileId + "'");
    }
    return report;
}

// -- Atomic save transaction ---------------------------------------------------

// Steps (design doc, "File Format And Safe Persistence"):
//   1. serialize to <destination>.tmp
//   2. load the temporary file back through the production parser
//   3. (caller already validated the typed definition)
//   4. preserve the previous valid file as <destination>.bak
//   5. replace the destination
//   6. caller reloads its in-memory entry
//   7. restore the backup if replacement fails
bool ContentDatabase::SaveDocumentAtomically(const ContentDocument& document,
                                             const std::string& destinationPath,
                                             const ContentId& id,
                                             ContentError& error)
{
    std::error_code filesystemError;
    fs::create_directories(fs::path(destinationPath).parent_path(), filesystemError);

    const std::string temporaryPath = destinationPath + ".tmp";
    const std::string backupPath    = destinationPath + ".bak";

    // 1. Serialize to the temporary file.
    if (!document.SaveToFile(temporaryPath, error))
        return false;

    // 2. Round-trip the temporary file through the production parser. A file
    //    that cannot be read back must never replace a valid one.
    ContentDocument reparsed;
    if (!reparsed.LoadFromFile(temporaryPath, error))
    {
        fs::remove(temporaryPath, filesystemError);
        error.message = "saved file failed to parse back, save aborted: " + error.message;
        return false;
    }

    // 4. Keep the previous valid file as the recovery backup.
    const bool destinationExisted = fs::exists(destinationPath, filesystemError);
    if (destinationExisted)
    {
        fs::remove(backupPath, filesystemError);
        fs::copy_file(destinationPath, backupPath, filesystemError);
        if (filesystemError)
        {
            fs::remove(temporaryPath, filesystemError);
            error.message  = "could not create recovery backup, save aborted";
            error.filePath = backupPath;
            return false;
        }
    }

    // 5. Replace the destination with the verified temporary file.
    fs::rename(temporaryPath, destinationPath, filesystemError);
    if (filesystemError)
    {
        // 7. Restore the backup so the previous valid file survives.
        if (destinationExisted)
            fs::copy_file(backupPath, destinationPath, fs::copy_options::overwrite_existing, filesystemError);
        fs::remove(temporaryPath, filesystemError);
        error.message  = "could not replace destination file, previous version restored";
        error.filePath = destinationPath;
        return false;
    }
    return true;
}

bool ContentDatabase::SaveGameplayAsset(const GameplayAssetDefinition& definition, ContentError& error)
{
    // 3. Validate before anything touches disk -- errors block the save.
    ContentValidationReport report;
    ValidateGameplayAssetDefinition(definition, report);
    if (report.HasErrors())
    {
        error.message = "validation failed: " + report.issues.front().message;
        return false;
    }

    ContentDocument document;
    SaveGameplayAssetToDocument(definition, document);
    if (!SaveDocumentAtomically(document, GameplayAssetFilePath(definition.id), definition.id, error))
        return false;

    // 6. Reload the in-memory entry from the file that actually landed.
    _gameplayAssets[definition.id] = definition;
    return Reload(definition.id, error);
}

bool ContentDatabase::SaveProjectile(const ProjectileDefinition& definition, ContentError& error)
{
    ContentValidationReport report;
    ValidateProjectileDefinition(definition, report);
    if (report.HasErrors())
    {
        error.message = "validation failed: " + report.issues.front().message;
        return false;
    }

    ContentDocument document;
    SaveProjectileToDocument(definition, document);
    if (!SaveDocumentAtomically(document, ProjectileFilePath(definition.id), definition.id, error))
        return false;
    _projectiles[definition.id] = definition;
    return Reload(definition.id, error);
}

bool ContentDatabase::SaveEnemy(const EnemyDefinition& definition, ContentError& error)
{
    ContentValidationReport report;
    ValidateEnemyDefinition(definition, report);
    if (report.HasErrors())
    {
        error.message = "validation failed: " + report.issues.front().message;
        return false;
    }

    ContentDocument document;
    SaveEnemyToDocument(definition, document);
    if (!SaveDocumentAtomically(document, EnemyFilePath(definition.id), definition.id, error))
        return false;
    _enemies[definition.id] = definition;
    return Reload(definition.id, error);
}

// -- Hot reload ----------------------------------------------------------------

bool ContentDatabase::Reload(const ContentId& id, ContentError& error)
{
    // Try each typed container; the id decides which file we look for.
    if (_gameplayAssets.count(id) || fs::exists(GameplayAssetFilePath(id)))
    {
        ContentDocument document;
        if (!document.LoadFromFile(GameplayAssetFilePath(id), error))
            return false;
        GameplayAssetDefinition definition;
        if (!LoadGameplayAssetFromDocument(document, definition, error))
            return false;
        _gameplayAssets[definition.id] = definition;
        return true;
    }
    if (_projectiles.count(id) || fs::exists(ProjectileFilePath(id)))
    {
        ContentDocument document;
        if (!document.LoadFromFile(ProjectileFilePath(id), error))
            return false;
        ProjectileDefinition definition;
        if (!LoadProjectileFromDocument(document, definition, error))
            return false;
        _projectiles[definition.id] = definition;
        return true;
    }
    if (_enemies.count(id) || fs::exists(EnemyFilePath(id)))
    {
        ContentDocument document;
        if (!document.LoadFromFile(EnemyFilePath(id), error))
            return false;
        EnemyDefinition definition;
        if (!LoadEnemyFromDocument(document, definition, error))
            return false;
        _enemies[definition.id] = definition;
        return true;
    }
    error.message = "no content with id '" + id + "' exists on disk";
    return false;
}
