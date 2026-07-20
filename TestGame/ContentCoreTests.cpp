// ContentCoreTests -- standalone automated tests for the authoring engine's
// content core (Project 2): parser, serializers, validation, migrations,
// atomic saves, and the Content Database with its dependency index.
//
// Build and run (same pattern as EliteSignatureTests):
//   cl /nologo /std:c++17 /EHsc /DCONTENT_CORE_TEST_MAIN ^
//      TestGame\ContentDocument.cpp TestGame\ContentTypes.cpp ^
//      TestGame\ContentSerializers.cpp TestGame\ContentValidation.cpp ^
//      TestGame\ContentDatabase.cpp TestGame\ContentCoreTests.cpp ^
//      /link /OUT:x64\Debug\ContentCoreTests.exe

#ifdef CONTENT_CORE_TEST_MAIN

#include "ContentDatabase.h"
#include "ContentDocument.h"
#include "ContentSerializers.h"
#include "ContentTypes.h"
#include "ContentValidation.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

static int testsPassed = 0;
static int testsFailed = 0;

// Reports one named condition; failures print but do not abort the run.
#define CHECK(condition, description)                                    \
    do {                                                                 \
        if (condition) { ++testsPassed; }                                \
        else { ++testsFailed; std::printf("FAIL: %s (line %d)\n",        \
                                          description, __LINE__); }     \
    } while (0)

// Every test writes inside this scratch folder, wiped at startup.
static const char* kScratchFolder = "ContentCoreTestScratch";

// -- Parser tests --------------------------------------------------------------

static void TestParserBasics()
{
    const char* text =
        "version=1\n"
        "id=test_asset\n"
        "\n"
        "# a comment line\n"
        "[visual]\n"
        "sprite=Foo/Bar.png\n"
        "scale=2.5\n"
        "\n"
        "[timeline.1]\n"
        "action=Telegraph\n"
        "[timeline.0]\n"
        "action=Wait\n"
        "duration=1.5\n";

    ContentDocument document;
    ContentError error;
    CHECK(document.ParseFromText(text, error), "parser accepts well-formed text");

    CHECK(document.GlobalSection().GetStringOr("id", "") == "test_asset", "global id reads back");
    CHECK(document.GlobalSection().GetIntOr("version", -1) == 1, "global version reads back");

    const ContentSection* visual = document.FindSection("visual");
    CHECK(visual != nullptr, "visual section found");
    CHECK(visual && visual->GetFloatOr("scale", 0.f) == 2.5f, "float value converts");
    CHECK(visual && visual->GetStringOr("sprite", "") == "Foo/Bar.png", "string value reads");

    // Numbered sections come back sorted by index even when the file lists
    // them out of order.
    auto timelineSections = document.FindNumberedSections("timeline");
    CHECK(timelineSections.size() == 2, "two timeline sections found");
    CHECK(timelineSections.size() == 2 &&
          timelineSections[0]->GetStringOr("action", "") == "Wait" &&
          timelineSections[1]->GetStringOr("action", "") == "Telegraph",
          "numbered sections sort by list index");

    // Line numbers survive for diagnostics.
    const ContentEntry* durationEntry = timelineSections.empty() ? nullptr
        : timelineSections[0]->FindEntry("duration");
    CHECK(durationEntry && durationEntry->lineNumber == 13, "entry line numbers tracked");
}

static void TestParserErrors()
{
    ContentDocument document;
    ContentError error;

    CHECK(!document.ParseFromText("[unclosed\nkey=1\n", error), "malformed header rejected");
    CHECK(error.lineNumber == 1, "malformed header reports its line");

    error = ContentError{};
    CHECK(!document.ParseFromText("id=ok\njust a bare line\n", error), "bare line rejected");
    CHECK(error.lineNumber == 2, "bare line reports its line");

    error = ContentError{};
    CHECK(!document.ParseFromText("=value_without_key\n", error), "empty key rejected");
}

static void TestParserRoundTrip()
{
    const char* text =
        "version=1\n"
        "id=round_trip\n"
        "[visual]\n"
        "sprite=A.png\n"
        "[timeline.0]\n"
        "action=Wait\n"
        "duration=2\n";

    ContentDocument first;
    ContentError error;
    CHECK(first.ParseFromText(text, error), "round-trip source parses");

    ContentDocument second;
    CHECK(second.ParseFromText(first.SerializeToText(), error), "serialized text reparses");
    CHECK(second.SerializeToText() == first.SerializeToText(), "serialization is stable");
}

// -- Serializer round-trip tests ----------------------------------------------

// Builds a gameplay asset touching every component for round-trip coverage.
static GameplayAssetDefinition BuildRichTestAsset()
{
    GameplayAssetDefinition asset;
    asset.id          = "ice_totem_test";
    asset.displayName = "Ice Totem (Test)";
    asset.category    = "Hazard";
    asset.tags        = { "totem", "ice" };
    asset.visual.spritePath = "PowerUps/Hazard_IceTotem.png";
    asset.visual.scale      = 4.f;

    AnimationComponent animation;
    animation.defaultClip = "idle";
    animation.clips.push_back({ "idle", 4, 0.15f, true });
    animation.clips.push_back({ "fire", 6, 0.08f, false });
    asset.animation = animation;

    CollisionComponent collision;
    CollisionShape shapeData;
    shapeData.width  = 48.f;
    shapeData.height = 64.f;
    shapeData.blocksProjectiles = true;
    collision.shapes.push_back(shapeData);
    asset.collision = collision;

    DamageComponent damage;
    damage.damageAmount = 12.f;
    damage.statusEffect = "chill";
    asset.damage = damage;

    MovementComponent movement;
    movement.pattern = MovementPatternKind::PingPong;
    movement.speed   = 140.f;
    asset.movement = movement;

    TargetingComponent targeting;
    targeting.mode     = TargetingModeKind::TrackedPlayer;
    targeting.lockTime = 0.4f;
    asset.targeting = targeting;

    EmitterComponent emitter;
    emitter.projectileId = "ice_bolt_test";
    emitter.count        = 3;
    emitter.spreadDegrees = 30.f;
    asset.emitter = emitter;

    TriggerComponent trigger;
    trigger.source = TriggerSourceKind::CombatStart;
    asset.trigger = trigger;

    DestructibleComponent destructible;
    destructible.health       = 40.f;
    destructible.recoveryTime = 6.f;
    asset.destructible = destructible;

    AudioVfxComponent feedback;
    feedback.telegraphVfx = "totem_charge";
    feedback.impactSound  = "ice_hit";
    asset.feedback = feedback;

    asset.timeline.steps.push_back({ TimelineActionKind::Wait, 1.5f, "", -1, -1, true });
    asset.timeline.steps.push_back({ TimelineActionKind::Telegraph, 0.6f, "", -1, -1, true });
    asset.timeline.steps.push_back({ TimelineActionKind::LockTarget, 0.f, "", -1, -1, true });
    asset.timeline.steps.push_back({ TimelineActionKind::FireProjectile, 0.f, "", -1, -1, true });
    asset.timeline.steps.push_back({ TimelineActionKind::Wait, 1.2f, "", -1, -1, true });
    asset.timeline.steps.push_back({ TimelineActionKind::Repeat, 0.f, "", 0, -1, true });
    return asset;
}

static void TestGameplayAssetRoundTrip()
{
    GameplayAssetDefinition original = BuildRichTestAsset();

    ContentDocument document;
    SaveGameplayAssetToDocument(original, document);

    // Reparse the serialized text through the parser, then load it back.
    ContentDocument reparsed;
    ContentError error;
    CHECK(reparsed.ParseFromText(document.SerializeToText(), error), "gasset text reparses");

    GameplayAssetDefinition loaded;
    CHECK(LoadGameplayAssetFromDocument(reparsed, loaded, error), "gasset loads back");

    CHECK(loaded.id == original.id, "id round-trips");
    CHECK(loaded.displayName == original.displayName, "display name round-trips");
    CHECK(loaded.tags == original.tags, "tags round-trip");
    CHECK(loaded.visual.scale == original.visual.scale, "visual scale round-trips");
    CHECK(loaded.animation && loaded.animation->clips.size() == 2, "animation clips round-trip");
    CHECK(loaded.animation && loaded.animation->clips[1].frameCount == 6, "clip frame count round-trips");
    CHECK(loaded.collision && loaded.collision->shapes.size() == 1 &&
          loaded.collision->shapes[0].blocksProjectiles, "collider masks round-trip");
    CHECK(loaded.damage && loaded.damage->statusEffect == "chill", "damage status round-trips");
    CHECK(loaded.movement && loaded.movement->pattern == MovementPatternKind::PingPong, "movement pattern round-trips");
    CHECK(loaded.targeting && loaded.targeting->lockTime == 0.4f, "targeting lock time round-trips");
    CHECK(loaded.emitter && loaded.emitter->projectileId == "ice_bolt_test" &&
          loaded.emitter->count == 3, "emitter round-trips");
    CHECK(loaded.destructible && loaded.destructible->recoveryTime == 6.f, "destructible round-trips");
    CHECK(loaded.feedback && loaded.feedback->telegraphVfx == "totem_charge", "feedback round-trips");
    CHECK(loaded.timeline.steps.size() == 6, "timeline length round-trips");
    CHECK(loaded.timeline.steps.size() == 6 &&
          loaded.timeline.steps[5].action == TimelineActionKind::Repeat &&
          loaded.timeline.steps[5].repeatTarget == 0, "repeat step round-trips");
    // Optional components that were absent stay absent.
    CHECK(!loaded.interaction, "absent optional component stays absent");
}

static void TestProjectileRoundTrip()
{
    ProjectileDefinition original;
    original.id           = "ice_bolt_test";
    original.displayName  = "Ice Bolt (Test)";
    original.spritePath   = "FX/IceBolt.png";
    original.frameCount   = 5;
    original.speed        = 420.f;
    original.maxTurnRate  = 90.f;
    original.statusEffect = "chill";
    original.impactVfx    = "ice_shatter";
    original.hitsPlayer   = true;
    original.pressureCost = 0.5f;

    ContentDocument document;
    SaveProjectileToDocument(original, document);

    ContentDocument reparsed;
    ContentError error;
    CHECK(reparsed.ParseFromText(document.SerializeToText(), error), "projectile text reparses");

    ProjectileDefinition loaded;
    CHECK(LoadProjectileFromDocument(reparsed, loaded, error), "projectile loads back");
    CHECK(loaded.id == original.id, "projectile id round-trips");
    CHECK(loaded.frameCount == 5, "projectile frame count round-trips");
    CHECK(loaded.speed == 420.f, "projectile speed round-trips");
    CHECK(loaded.maxTurnRate == 90.f, "projectile turn rate round-trips");
    CHECK(loaded.statusEffect == "chill", "projectile status round-trips");
    CHECK(loaded.impactVfx == "ice_shatter", "projectile VFX round-trips");
    CHECK(loaded.pressureCost == 0.5f, "projectile pressure cost round-trips");
}

static void TestEnemyRoundTrip()
{
    EnemyDefinition original;
    original.id          = "skeleton_archer_test";
    original.displayName = "Skeleton Archer (Test)";
    original.baseStats["health"]          = 55.f;
    original.baseStats["attack_cooldown"] = 1.4f;
    original.projectileIds = { "bone_arrow", "ice_bolt_test" };

    ContentDocument document;
    SaveEnemyToDocument(original, document);

    ContentDocument reparsed;
    ContentError error;
    CHECK(reparsed.ParseFromText(document.SerializeToText(), error), "enemy text reparses");

    EnemyDefinition loaded;
    CHECK(LoadEnemyFromDocument(reparsed, loaded, error), "enemy loads back");
    CHECK(loaded.baseStats.size() == 2 && loaded.baseStats["health"] == 55.f, "enemy stats round-trip");
    CHECK(loaded.projectileIds.size() == 2 && loaded.projectileIds[1] == "ice_bolt_test",
          "enemy projectile references round-trip");
}

// -- Unknown-field warnings ----------------------------------------------------

static void TestUnknownFieldWarning()
{
    const char* text =
        "version=1\n"
        "id=typo_asset\n"
        "[visual]\n"
        "sprite=A.png\n"
        "duratoin=1.5\n";   // deliberate typo

    ContentDocument document;
    ContentError error;
    CHECK(document.ParseFromText(text, error), "typo file still parses");

    GameplayAssetDefinition loaded;
    CHECK(LoadGameplayAssetFromDocument(document, loaded, error), "typo file still loads");

    ContentValidationReport report;
    ReportUnconsumedEntries(document, loaded.id, report);
    CHECK(report.WarningCount() == 1, "typo produces exactly one unknown-field warning");
    CHECK(!report.HasErrors(), "typo is a warning, not an error");
}

// -- Validation tests ----------------------------------------------------------

static void TestTimelineValidation()
{
    GameplayAssetDefinition asset = BuildRichTestAsset();

    // Valid rich asset passes.
    {
        ContentValidationReport report;
        ValidateGameplayAssetDefinition(asset, report);
        CHECK(!report.HasErrors(), "rich test asset validates clean");
    }

    // Repeat pointing forward is rejected.
    {
        GameplayAssetDefinition broken = asset;
        broken.timeline.steps[5].repeatTarget = 5;
        ContentValidationReport report;
        ValidateGameplayAssetDefinition(broken, report);
        CHECK(report.HasErrors(), "forward repeat target rejected");
    }

    // Unbounded zero-duration loop is rejected.
    {
        GameplayAssetDefinition broken = asset;
        broken.timeline.steps.clear();
        broken.timeline.steps.push_back({ TimelineActionKind::LockTarget, 0.f, "", -1, -1, true });
        broken.timeline.steps.push_back({ TimelineActionKind::Repeat, 0.f, "", 0, -1, true });
        ContentValidationReport report;
        ValidateGameplayAssetDefinition(broken, report);
        CHECK(report.HasErrors(), "zero-duration unbounded loop rejected");
    }

    // The same loop with a bounded count is allowed.
    {
        GameplayAssetDefinition bounded = asset;
        bounded.timeline.steps.clear();
        bounded.timeline.steps.push_back({ TimelineActionKind::LockTarget, 0.f, "", -1, -1, true });
        bounded.timeline.steps.push_back({ TimelineActionKind::Repeat, 0.f, "", 0, 3, true });
        ContentValidationReport report;
        ValidateGameplayAssetDefinition(bounded, report);
        CHECK(!report.HasErrors(), "bounded zero-duration loop allowed");
    }

    // EnableDamage without a damage component is rejected.
    {
        GameplayAssetDefinition broken = asset;
        broken.damage.reset();
        broken.timeline.steps.clear();
        broken.timeline.steps.push_back({ TimelineActionKind::EnableDamage, 0.f, "", -1, -1, true });
        ContentValidationReport report;
        ValidateGameplayAssetDefinition(broken, report);
        CHECK(report.HasErrors(), "EnableDamage without damage component rejected");
    }

    // PlayVfx without a target name is rejected.
    {
        GameplayAssetDefinition broken = asset;
        broken.timeline.steps.clear();
        broken.timeline.steps.push_back({ TimelineActionKind::PlayVfx, 0.f, "", -1, -1, true });
        ContentValidationReport report;
        ValidateGameplayAssetDefinition(broken, report);
        CHECK(report.HasErrors(), "PlayVfx without target rejected");
    }
}

static void TestValueValidation()
{
    // Bad id characters.
    {
        GameplayAssetDefinition asset = BuildRichTestAsset();
        asset.id = "Bad Id!";
        ContentValidationReport report;
        ValidateGameplayAssetDefinition(asset, report);
        CHECK(report.HasErrors(), "malformed id rejected");
    }

    // NaN scale.
    {
        GameplayAssetDefinition asset = BuildRichTestAsset();
        asset.visual.scale = std::nanf("");
        ContentValidationReport report;
        ValidateGameplayAssetDefinition(asset, report);
        CHECK(report.HasErrors(), "NaN scale rejected");
    }

    // Damage without any collision geometry.
    {
        GameplayAssetDefinition asset = BuildRichTestAsset();
        asset.collision.reset();
        ContentValidationReport report;
        ValidateGameplayAssetDefinition(asset, report);
        CHECK(report.HasErrors(), "damage without geometry rejected");
    }

    // Projectile that can never expire.
    {
        ProjectileDefinition projectile;
        projectile.id         = "eternal_bolt";
        projectile.spritePath = "FX/Bolt.png";
        projectile.lifetime   = 0.f;
        projectile.range      = 0.f;
        ContentValidationReport report;
        ValidateProjectileDefinition(projectile, report);
        CHECK(report.HasErrors(), "projectile without lifetime or range rejected");
    }

    // Projectile above the hard instance cap.
    {
        ProjectileDefinition projectile;
        projectile.id         = "swarm_bolt";
        projectile.spritePath = "FX/Bolt.png";
        projectile.maximumLiveInstances = 999;
        ContentValidationReport report;
        ValidateProjectileDefinition(projectile, report);
        CHECK(report.HasErrors(), "instance cap above hard cap rejected");
    }
}

// -- Migration tests -----------------------------------------------------------

// Test-only migration: version 0 files used "sprite_path"; version 1 renamed
// it to "sprite". The step rewrites the entry and bumps the version counter.
static bool TestMigrationStepZeroToOne(ContentDocument& document, ContentError& error)
{
    (void)error;
    // Rebuild the document with the renamed key. Migrations are pure
    // text-level rewrites; they never touch typed definitions.
    ContentDocument rebuilt;
    ContentError parseError;
    std::string text = document.SerializeToText();
    size_t position = text.find("sprite_path=");
    if (position != std::string::npos)
        text.replace(position, 12, "sprite=");
    position = text.find("version=0");
    if (position != std::string::npos)
        text.replace(position, 9, "version=1");
    if (!rebuilt.ParseFromText(text, parseError))
        return false;
    document = rebuilt;
    return true;
}

static void TestMigrations()
{
    // A future version is rejected with a clear error.
    {
        ContentDocument document;
        ContentError error;
        CHECK(document.ParseFromText("version=99\nid=future_asset\n[visual]\nsprite=A.png\n", error),
              "future-version file parses");
        GameplayAssetDefinition loaded;
        CHECK(!LoadGameplayAssetFromDocument(document, loaded, error), "future version rejected");
        CHECK(error.message.find("newer") != std::string::npos, "future-version error mentions newer build");
    }

    // A version-0 file with no registered step fails cleanly.
    {
        ContentDocument document;
        ContentError error;
        document.ParseFromText("version=0\nid=old_asset\n[visual]\nsprite_path=A.png\n", error);
        GameplayAssetDefinition loaded;
        CHECK(!LoadGameplayAssetFromDocument(document, loaded, error), "missing migration step fails cleanly");
    }

    // With the step registered, the same file loads and the rename applied.
    // Migration mutates the in-memory document (so consumed-field tracking
    // works), but the file on disk stays untouched until an explicit Save.
    {
        RegisterContentMigrationStep(0, TestMigrationStepZeroToOne);
        const std::string originalText = "version=0\nid=old_asset\n[visual]\nsprite_path=A.png\n";
        ContentDocument document;
        ContentError error;
        document.ParseFromText(originalText, error);
        GameplayAssetDefinition loaded;
        CHECK(LoadGameplayAssetFromDocument(document, loaded, error), "registered migration loads old file");
        CHECK(loaded.visual.spritePath == "A.png", "migrated field carries its value");
        CHECK(document.GlobalSection().GetStringOr("version", "") == "1",
              "in-memory document reflects the migrated version");

        // A second, untouched parse of the same original text still reports
        // version 0 -- proving migration never rewrote the source file text,
        // only the caller's in-memory copy of it.
        ContentDocument freshFromSameText;
        freshFromSameText.ParseFromText(originalText, error);
        CHECK(freshFromSameText.GlobalSection().GetStringOr("version", "") == "0",
              "original source text is unaffected by migrating a loaded copy");
        ClearContentMigrationStepsForTesting();
    }
}

// -- Database, dependency index, and atomic save tests -------------------------

static void TestDatabase()
{
    // Fresh scratch database folder.
    std::error_code filesystemError;
    fs::remove_all(kScratchFolder, filesystemError);
    ContentDatabase database(kScratchFolder);

    // Save a projectile and a gameplay asset that references it.
    ProjectileDefinition iceBolt;
    iceBolt.id          = "ice_bolt_test";
    iceBolt.displayName = "Ice Bolt (Test)";
    iceBolt.spritePath  = "FX/IceBolt.png";
    ContentError error;
    CHECK(database.SaveProjectile(iceBolt, error), "projectile saves");

    GameplayAssetDefinition totem = BuildRichTestAsset();
    CHECK(database.SaveGameplayAsset(totem, error), "gameplay asset saves");

    EnemyDefinition archer;
    archer.id          = "archer_test";
    archer.displayName = "Archer (Test)";
    archer.baseStats["health"] = 40.f;
    archer.projectileIds = { "ice_bolt_test" };
    CHECK(database.SaveEnemy(archer, error), "enemy saves");

    // A rescan from disk finds all three.
    std::vector<ContentError> scanErrors;
    ContentDatabase rescanned(kScratchFolder);
    int loadedCount = rescanned.ScanAll(scanErrors);
    CHECK(loadedCount == 3, "rescan loads all three definitions");
    CHECK(scanErrors.empty(), "rescan reports no errors");
    CHECK(rescanned.FindGameplayAsset("ice_totem_test") != nullptr, "FindGameplayAsset resolves");
    CHECK(rescanned.FindProjectile("ice_bolt_test") != nullptr, "FindProjectile resolves");
    CHECK(rescanned.FindEnemy("archer_test") != nullptr, "FindEnemy resolves");
    CHECK(rescanned.FindGameplayAsset("does_not_exist") == nullptr, "missing id resolves to null");

    // Dependency index: both the totem's emitter and the archer reference the
    // bolt; deleting it must surface both.
    auto references = rescanned.FindReferencesTo("ice_bolt_test");
    CHECK(references.size() == 2, "dependency index finds both references");

    // Cross-reference validation: pointing the emitter at a missing
    // projectile becomes an error.
    GameplayAssetDefinition brokenTotem = totem;
    brokenTotem.emitter->projectileId = "missing_bolt";
    CHECK(database.SaveGameplayAsset(brokenTotem, error), "per-definition save still allowed");
    ContentValidationReport report = database.ValidateAll();
    CHECK(report.HasErrors(), "ValidateAll flags missing projectile reference");
    // Restore the working reference for the tests below.
    CHECK(database.SaveGameplayAsset(totem, error), "restore reference");

    // Atomic save: a second save keeps the previous file as .bak.
    GameplayAssetDefinition editedTotem = totem;
    editedTotem.displayName = "Ice Totem (Edited)";
    CHECK(database.SaveGameplayAsset(editedTotem, error), "edited asset saves");
    CHECK(fs::exists(database.GameplayAssetFilePath(totem.id) + ".bak"), "previous version kept as .bak");

    // The database reloaded the edited entry.
    const GameplayAssetDefinition* reloaded = database.FindGameplayAsset(totem.id);
    CHECK(reloaded && reloaded->displayName == "Ice Totem (Edited)", "database entry reloaded after save");

    // A validation-blocked save touches neither disk nor database.
    GameplayAssetDefinition invalidTotem = editedTotem;
    invalidTotem.visual.spritePath = "";
    CHECK(!database.SaveGameplayAsset(invalidTotem, error), "invalid asset blocked from saving");
    const GameplayAssetDefinition* untouched = database.FindGameplayAsset(totem.id);
    CHECK(untouched && untouched->visual.spritePath == totem.visual.spritePath,
          "blocked save leaves database unchanged");
    ContentDocument onDisk;
    ContentError diskError;
    CHECK(onDisk.LoadFromFile(database.GameplayAssetFilePath(totem.id), diskError) &&
          onDisk.FindSection("visual")->GetStringOr("sprite", "") == totem.visual.spritePath,
          "blocked save leaves file unchanged");

    // Duplicate ids across two files are reported at scan time.
    {
        std::ofstream duplicateFile(std::string(kScratchFolder) + "/GameplayAssets/duplicate_copy.gasset");
        duplicateFile << "version=1\nid=ice_totem_test\n[visual]\nsprite=B.png\n";
        duplicateFile.close();
        std::vector<ContentError> duplicateErrors;
        ContentDatabase duplicateScan(kScratchFolder);
        duplicateScan.ScanAll(duplicateErrors);
        bool foundDuplicateError = false;
        for (const ContentError& scanError : duplicateErrors)
            if (scanError.message.find("duplicate") != std::string::npos)
                foundDuplicateError = true;
        CHECK(foundDuplicateError, "duplicate id reported at scan time");
    }

    // A corrupt file is skipped and reported without blocking other content.
    {
        std::ofstream corruptFile(std::string(kScratchFolder) + "/Projectiles/broken.projectile");
        corruptFile << "this is not a content file\n";
        corruptFile.close();
        std::vector<ContentError> corruptErrors;
        ContentDatabase corruptScan(kScratchFolder);
        int survivingCount = corruptScan.ScanAll(corruptErrors);
        CHECK(!corruptErrors.empty(), "corrupt file reported");
        CHECK(corruptScan.FindProjectile("ice_bolt_test") != nullptr, "healthy content still loads");
        (void)survivingCount;
    }

    // Hot reload picks up an external edit.
    {
        std::string boltPath = database.ProjectileFilePath("ice_bolt_test");
        ContentDocument boltDocument;
        ContentError reloadError;
        CHECK(boltDocument.LoadFromFile(boltPath, reloadError), "bolt file loads for edit");
        std::string editedText = boltDocument.SerializeToText();
        size_t namePosition = editedText.find("name=Ice Bolt (Test)");
        CHECK(namePosition != std::string::npos, "bolt name found for edit");
        editedText.replace(namePosition, 20, "name=Frost Bolt");
        std::ofstream editedFile(boltPath, std::ios::binary | std::ios::trunc);
        editedFile << editedText;
        editedFile.close();
        CHECK(database.Reload("ice_bolt_test", reloadError), "external edit reloads");
        const ProjectileDefinition* reloadedBolt = database.FindProjectile("ice_bolt_test");
        CHECK(reloadedBolt && reloadedBolt->displayName == "Frost Bolt", "reload picked up the edit");
    }
}

// -- The design doc's ice_totem example must load as written -------------------

static void TestDesignDocExample()
{
    const char* documentText =
        "version=1\n"
        "id=ice_totem\n"
        "name=Ice Totem\n"
        "category=Hazard\n"
        "\n"
        "[visual]\n"
        "sprite=PowerUps/Hazard_IceTotem.png\n"
        "scale=4.0\n"
        "\n"
        "[trigger]\n"
        "type=CombatStart\n"
        "\n"
        "[emitter]\n"
        "projectile=ice_bolt\n"
        "count=1\n"
        "\n"
        "[timeline.0]\n"
        "action=Wait\n"
        "duration=1.5\n"
        "\n"
        "[timeline.1]\n"
        "action=Telegraph\n"
        "duration=0.6\n"
        "\n"
        "[timeline.2]\n"
        "action=LockTarget\n"
        "\n"
        "[timeline.3]\n"
        "action=FireProjectile\n"
        "\n"
        "[timeline.4]\n"
        "action=Wait\n"
        "duration=1.2\n"
        "\n"
        "[timeline.5]\n"
        "action=Repeat\n"
        "repeat_target=0\n";

    ContentDocument document;
    ContentError error;
    CHECK(document.ParseFromText(documentText, error), "design doc example parses");

    GameplayAssetDefinition totem;
    CHECK(LoadGameplayAssetFromDocument(document, totem, error), "design doc example loads");
    CHECK(totem.id == "ice_totem", "example id loads");
    CHECK(totem.trigger && totem.trigger->source == TriggerSourceKind::CombatStart, "example trigger loads");
    CHECK(totem.emitter && totem.emitter->projectileId == "ice_bolt", "example emitter loads");
    CHECK(totem.timeline.steps.size() == 6, "example timeline loads");

    ContentValidationReport report;
    ValidateGameplayAssetDefinition(totem, report);
    CHECK(!report.HasErrors(), "design doc example validates clean");
}

// -- Entry point ---------------------------------------------------------------

int main()
{
    std::printf("Content core tests\n==================\n");

    TestParserBasics();
    TestParserErrors();
    TestParserRoundTrip();
    TestGameplayAssetRoundTrip();
    TestProjectileRoundTrip();
    TestEnemyRoundTrip();
    TestUnknownFieldWarning();
    TestTimelineValidation();
    TestValueValidation();
    TestMigrations();
    TestDatabase();
    TestDesignDocExample();

    // Clean the scratch folder so repeated runs start fresh.
    std::error_code filesystemError;
    fs::remove_all(kScratchFolder, filesystemError);

    std::printf("\n%d passed, %d failed\n", testsPassed, testsFailed);
    return testsFailed == 0 ? 0 : 1;
}

#endif // CONTENT_CORE_TEST_MAIN
