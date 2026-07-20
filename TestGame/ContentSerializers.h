#pragma once

// ContentSerializers -- moves typed definitions to and from the shared
// ContentDocument format. All parsing goes through ContentDocument; nothing
// here runs its own ad hoc text scanning (authoring engine design doc, "File
// Format And Safe Persistence").
//
// Every loader first runs the migration chain, so callers always receive a
// definition at kContentSchemaVersion or a clear error.

#include "ContentDocument.h"
#include "ContentTypes.h"

// -- Migrations ----------------------------------------------------------------

// Steps a parsed document from its stored version up to
// kContentSchemaVersion, one registered migration at a time. Returns false
// (with an error) for versions newer than this build understands. The source
// file is never rewritten here; only the in-memory document changes.
bool MigrateContentDocument(ContentDocument& document, ContentError& error);

// A migration step rewrites a document from exactly `fromVersion` to
// `fromVersion + 1`. Registered steps apply to every content type; a step can
// inspect the document to decide whether it needs to change anything.
using ContentMigrationStep = bool (*)(ContentDocument& document, ContentError& error);
void RegisterContentMigrationStep(int fromVersion, ContentMigrationStep step);
void ClearContentMigrationStepsForTesting();

// -- Gameplay assets (.gasset) -------------------------------------------------

// Migrates `document` to the current schema version in place, then reads it.
// The document is passed by mutable reference (not the source file) so
// callers can inspect which fields the loader actually consumed afterward
// (see ReportUnconsumedEntries) -- migration steps rewrite the in-memory
// object, never the file on disk.
bool LoadGameplayAssetFromDocument(ContentDocument& document,
                                   GameplayAssetDefinition& outDefinition,
                                   ContentError& error);
void SaveGameplayAssetToDocument(const GameplayAssetDefinition& definition,
                                 ContentDocument& outDocument);

// -- Projectiles (.projectile) -------------------------------------------------

bool LoadProjectileFromDocument(ContentDocument& document,
                                ProjectileDefinition& outDefinition,
                                ContentError& error);
void SaveProjectileToDocument(const ProjectileDefinition& definition,
                              ContentDocument& outDocument);

// -- Enemies (.enemy) ----------------------------------------------------------

bool LoadEnemyFromDocument(ContentDocument& document,
                           EnemyDefinition& outDefinition,
                           ContentError& error);
void SaveEnemyToDocument(const EnemyDefinition& definition,
                         ContentDocument& outDocument);
