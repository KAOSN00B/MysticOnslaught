#pragma once

// ContentDatabase -- the single authority for resolving ContentIds (authoring
// engine design doc, "Stable Identity And References"). It scans the content
// root, owns every loaded definition, answers reference queries through the
// dependency index, and performs the atomic save transaction.
//
// Runtime code stores stable IDs, never raw pointers held across a Reload.

#include "ContentTypes.h"
#include "ContentValidation.h"

#include <map>
#include <string>
#include <vector>

// One "X references Y" edge for delete-safety queries and editor navigation.
struct ContentReference
{
    ContentId   fromId;       // the asset holding the reference
    std::string fromType;     // "gasset", "projectile", "enemy"
    std::string fieldName;    // where inside the asset ("emitter.projectile")
};

class ContentDatabase
{
public:
    // Folder layout under the root (created on demand by saves):
    //   <root>/GameplayAssets/*.gasset
    //   <root>/Projectiles/*.projectile
    //   <root>/Enemies/*.enemy
    // The default root matches the game's working directory ("Content").
    explicit ContentDatabase(std::string contentRootFolder = "Content");

    // Scans every content folder and loads all definitions. Files that fail to
    // parse are skipped and reported; one broken file never blocks the rest.
    // Returns the number of definitions loaded.
    int ScanAll(std::vector<ContentError>& outErrors);

    // -- Lookup (the only sanctioned way to resolve an id) -------------------
    const GameplayAssetDefinition* FindGameplayAsset(const ContentId& id) const;
    const ProjectileDefinition*    FindProjectile(const ContentId& id) const;
    const EnemyDefinition*         FindEnemy(const ContentId& id) const;

    int GameplayAssetCount() const { return (int)_gameplayAssets.size(); }
    int ProjectileCount()    const { return (int)_projectiles.size(); }
    int EnemyCount()         const { return (int)_enemies.size(); }
    std::vector<ContentId> AllGameplayAssetIds() const;
    std::vector<ContentId> AllProjectileIds() const;

    // -- Dependency index ----------------------------------------------------
    // Every asset that still references `id`. Deleting an asset must consult
    // this list first so no room or emitter is left pointing at nothing.
    std::vector<ContentReference> FindReferencesTo(const ContentId& id) const;

    // -- Validation ----------------------------------------------------------
    // Per-definition checks plus cross-reference checks (missing projectiles,
    // duplicate ids are caught at scan time and reported here too).
    ContentValidationReport ValidateAll() const;

    // -- Atomic saves --------------------------------------------------------
    // The full save transaction from the design doc: serialize to a temporary
    // file, parse it back through the production loader, validate, keep the
    // previous file as a .bak recovery copy, replace the destination, then
    // reload the in-memory entry. Any failure restores the previous file and
    // leaves the database unchanged. Returns false with the error filled.
    bool SaveGameplayAsset(const GameplayAssetDefinition& definition, ContentError& error);
    bool SaveProjectile(const ProjectileDefinition& definition, ContentError& error);
    bool SaveEnemy(const EnemyDefinition& definition, ContentError& error);

    // Reloads one definition from disk (hot reload after an external edit).
    bool Reload(const ContentId& id, ContentError& error);

    // -- Paths ---------------------------------------------------------------
    std::string GameplayAssetFilePath(const ContentId& id) const;
    std::string ProjectileFilePath(const ContentId& id) const;
    std::string EnemyFilePath(const ContentId& id) const;
    const std::string& ContentRootFolder() const { return _contentRootFolder; }

private:
    // Shared transaction body used by all three typed save methods.
    bool SaveDocumentAtomically(const ContentDocument& document,
                                const std::string& destinationPath,
                                const ContentId& id,
                                ContentError& error);

    std::string _contentRootFolder;

    std::map<ContentId, GameplayAssetDefinition> _gameplayAssets;
    std::map<ContentId, ProjectileDefinition>    _projectiles;
    std::map<ContentId, EnemyDefinition>         _enemies;
};
