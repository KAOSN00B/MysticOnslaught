# Complete authored-room coverage across biomes

Status: proposed implementation design for user review.

Priority update: the user accepted the broad biome direction, then prioritized game feel and dungeon transitions. Execute the game-flow milestone described in `2026-09-23-game-flow-and-arrival-design.md` first. Complete biome coverage remains required afterward.

## Agreed intent

Make Mystic Onslaught a finished, satisfying, replayable title. The user explicitly wants every existing authored layout available in every biome using that biome's corresponding assets. Additional biome-exclusive rooms are welcome, but must supplement complete coverage of the existing layouts. Preserve existing authored content and current uncommitted work.

This first milestone delivers the content foundation. It does not establish that the whole game is release-ready or that combat is fun; those require subsequent implementation and playtesting.

## Inspected project evidence

- C++/raylib project with RoomBlueprint, RoomLibrary, RoomAssetCatalog, TileDefs and an in-game room editor.
- 55 source .mroom files: Forest 23, Caverns 23, Graveyard 9. Treat every file as a source identity until explicit review establishes duplicates; filenames alone do not establish equivalence.
- Ten biome enum values and corresponding MapTilesets images: Ancient Castle, Caverns, Demons Insides, Dream Realm, Forest, Graveyard, Jungle, Lost City, The Sanctuary and Wastelands.
- Blueprints contain tile grids, exact sprite rectangles, anchors, source sheets, asset IDs, collision grids/rectangles, fall zones, door masks and room roles. Both sprite coordinates and asset IDs require explicit translation.
- Tile catalogs vary in completeness. Ancient Castle has 13 TILE records and no registered props/decorations; Lost City and The Sanctuary also have no registered decorations. This indicates metadata gaps, not proof that artwork is absent from their images.
- RoomLibrary selection currently matches biome, tileset, room type and door mask. Production and editor-playtest selection paths both need validation.
- Existing combat-polish work is uncommitted; its playtest document labels it a first-pass candidate.

## Approaches considered

1. Manually copy and repaint all rooms. Gives direct control but makes hundreds of variants expensive to maintain and easy to desynchronize.
2. Translate sprites on every runtime load. Avoids generated files but makes visual exceptions and editor inspection harder, with incomplete mappings surfacing during play.
3. Recommended: explicit biome mapping profiles plus an offline variant generator, producing editable, validated .mroom files. Preserve source provenance and use authored overrides for artwork that cannot be translated cleanly.

## First milestone design

### Source preservation and coverage

Inventory and hash all 55 original files before changes. Keep originals untouched. Assign each source ID a layout-family identity. Each family must be available in all ten biomes: 550 coverage entries, including the 55 original entries, rather than 550 newly invented levels. Maintain a manifest linking source ID/hash, target biome, output path, mapping revision and validation status. Existing originals satisfy their own biome entries.

Generated variants have stable unique IDs and live in a dedicated generated subtree under the room content root. The generator refuses to overwrite originals or manually edited outputs. Store overrides separately and detect source/mapping drift so regeneration cannot silently destroy edits. A source hash mismatch marks downstream variants stale until rebuilt and revalidated.

### Biome asset mapping

Inspect each tilesheet visually and complete metadata for the actual artwork. Map structural pieces by their visual role, orientation, footprint and anchor: floors, wall faces, corners, edges, door covers and fall boundaries. Map props/decorations through explicit source-to-target entries with target asset IDs, source rectangles or multi-piece replacements as needed. Do not assume equal coordinates, prop numbers or arbitrary nearest sizes imply equivalence.

Profiles specify permitted sheets for each biome. Shared water/lava animation sheets may be explicitly permitted when appropriate to that biome. No unexplained foreign-biome art is permitted in completed variants. If no suitable replacement exists, record a blocking mapping issue and author a replacement arrangement from target assets. Do not silently delete props or substitute generic floor tiles.

### Preserve gameplay geometry

Keep door masks/zones, room type, treasure marker, capacity override, wall depths, solid/fall grids and explicit collision/fall rectangles unchanged. Preserve the gameplay footprint of collision-bearing props as well; their collision may be derived from target definitions and therefore must be compared after layout construction, not merely by comparing blueprint fields. Use appropriately fitted artwork or variant-specific definitions without changing globally shared asset collision.

Retain intended draw bands, occlusion and anchors. A biome-specific fall surface may change presentation only; it must retain existing fall-zone behavior and coverage. New damage hazards or encounter mechanics belong to a later design pass.

### Integration and repetition

Use the existing .mroom load/render pipeline for generated output. Verify that normal runs, editor previews and packaged builds load the same library. Track the source layout-family identity so later room-selection work can avoid immediately repeating the same layout under a different skin. Selection must continue respecting required door masks and room roles; do not change progression merely to force a variant into a run.

New biome-exclusive rooms are a separate authored set and never replace the required existing-family coverage. Their number and designs will follow playtesting of this foundation.

## Delivery sequence

1. Inventory original rooms, asset references, geometry and catalogs; produce the complete coverage matrix and mapping-gap report.
2. Implement profiles and the generator with deterministic output, overwrite protection and useful diagnostics.
3. Prove the pipeline on representative Forest/Caverns/Graveyard rooms and Ancient Castle as a catalog-gap case. Include multi-piece art, custom colliders, fall zones, doors and special room roles.
4. Complete mappings and generate every remaining room-biome variant.
5. Inspect contact sheets of every variant, repair exceptions, and playtest representative rooms in all ten biomes.
6. Verify production selection and native/web packaging. Deliver a playable candidate with coverage and validation reports.

## Acceptance criteria

- All 55 original files retain their baseline hashes.
- Every source room has an original or validated target variant in each of the ten biomes; coverage totals 550 entries and contains no unresolved mappings or duplicate IDs.
- All variants load through RoomBlueprint and BuildRoomLayout with no missing-asset warnings; source rectangles and animation frames remain within image bounds.
- Every referenced sheet is permitted by its target biome profile.
- Door behavior, room roles, chest markers and effective collision/fall geometry match the source. Automated checks cover traversability between applicable doors and spawn/reward clearance; baseline source issues are reported separately and require deliberate repairs.
- Deterministic regeneration yields identical output and refuses to clobber edited content.
- Visual inspection covers every output; gameplay checks cover every biome and representative geometry/room-role cases. Visual completion is not inferred from passing parser tests.
- Native and web packages include the required room files, metadata and images, and successfully enter representative generated rooms.

## Following milestones toward the full title

1. Replayability: audit the existing relics, abilities, route choices, events and progression; improve build-changing decisions, encounter combinations and family-aware room selection. Favor unlocks that expand choices. Define run length and endgame scope with the user before restructuring progression.
2. Combat and pacing: playtest the existing polish candidate, tune readable attacks and feedback, reduce dead time, and give each biome distinctive encounter composition. Evaluate repeated full runs, not only isolated fights.
3. Content completion: add exclusive discovery rooms, complete bosses and progression payoff, onboarding and the run-ending experience.
4. Release hardening: define target storefront/platform requirements, validate save recovery and upgrades, input/settings flows, audio/assets, performance and clean-machine packages. Store publication requires separate explicit authorization.

## Review boundary

The user has approved complete biome coverage and the broader direction. This document makes the implementation approach concrete for review. Next, approve or revise this spec, then prepare the implementation plan. No product code or room content has been changed in this design pass.
