# All-Tileset Layered Room Editor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give Draw Map access to every tileset and authored asset, shared water/lava animation sheets, independent ground/visual/collision/fall layers, and adjustable door clear zones that remove overlapping non-ground artwork after room clear.

**Architecture:** Extend saved tile/asset definitions with source-sheet identity, then add a data-only `RoomAssetCatalog` shared by the editor and runtime. Version 2 room blueprints store source-qualified visual placements, a solid mask, and four door zones. `TileRenderer` resolves handcrafted visual placements through registered source textures while procedural layouts keep the existing fast path.

**Tech Stack:** C++17, raylib 5.5, `std::filesystem`, assertion-based standalone tests, Visual Studio/MSBuild `Debug|x64`.

## Global Constraints

- Preserve all existing user and Claude changes and avoid unrelated village, combat, and procedural-room edits.
- New Game and Continue keep their existing procedural room path; Dungeon Run remains the handcrafted-room test path.
- Existing version 1 `.mroom` and legacy `tilemapper_*.txt` files must load without manual conversion.
- Ground never disappears when a door clears; every overlapping non-ground placement clears as a complete object.
- Shared sheet IDs are exactly `Ground TIles`, `FD_Animated_Water`, and `RA_Hell_Animations`, matching existing filenames.
- Missing source data must warn and render a placeholder instead of crashing.

---

### Task 1: Source-Aware Tile and Animation Definitions

**Files:**
- Modify: `TestGame/TileDefs.h`
- Modify: `TestGame/TileDefs.cpp`
- Modify: `TestGame/TileMapper.h`
- Modify: `TestGame/TileMapper.cpp`
- Modify: `TestGame/TileAssetMetadataTests.cpp`

**Interfaces:**
- Add `std::string sourceSheet` to `SpriteDef`, `AnimPropDef`, and `AnimSpriteDef`; an empty value means the owning biome sheet.
- Replace the mapper's ground-only selection boolean with `TileSourceSheet { Biome, Ground, Water, Lava }`.
- Save source-aware records as `PROPV3`, `ANIMPROPV3`, `DECORV3`, and `ANIMDECORV3`; legacy/V2 records default to an empty source.

- [ ] Write failing metadata tests that parse and round-trip source-aware animated water and lava definitions while retaining V2 compatibility.
- [ ] Compile the test and confirm missing `sourceSheet`/V3 parsing failures.
- [ ] Implement V3 parsing and source fields with legacy defaults.
- [ ] Load `FD_Animated_Water.png` and `RA_Hell_Animations.png` beside Ground Tiles in TileMapper; include them in fit, pan, grid, selection, preview, and unload paths.
- [ ] Ensure all frames added to one animation come from one source sheet and display a clear warning when they do not.
- [ ] Save V3 records and rerun `TileAssetMetadataTests` to green.

### Task 2: Version 2 Room Blueprint and Door-Clear Rules

**Files:**
- Modify: `TestGame/RoomBlueprint.h`
- Modify: `TestGame/RoomBlueprint.cpp`
- Modify: `TestGame/RoomLayout.h`
- Modify: `TestGame/RoomBlueprintTests.cpp`
- Modify: `TestGame/RoomCollision.h`
- Modify: `TestGame/RoomCollision.cpp`
- Modify: `TestGame/RoomCollisionTests.cpp`

**Interfaces:**
- Add `RoomTilePlacement { std::string sourceTileset; TileType type; bool ground; int col; int row; }`.
- Extend `RoomAssetPlacement` with `std::string sourceTileset` while treating an empty value as the room's primary tileset.
- Add `RoomDoorZone { bool enabled; Rectangle tiles; }` indexed by `RoomWallSide`.
- Add `bool solid[kRows][kCols]`, `std::vector<RoomTilePlacement> visualTiles`, and door zones to `RoomBlueprint` and `RoomLayout`.
- Add `bool RoomPlacementClearsAtDoor(Rectangle occupiedTiles, const RoomLayout&, bool roomCleared)` and make handcrafted collision read `solid` plus active door zones.

- [ ] Write failing tests for V2 source references, solid-mask persistence, adjustable zones, legacy V1 loading, and clear overlap semantics.
- [ ] Confirm tests fail because V2 fields and rules are absent.
- [ ] Implement V2 serialization with explicit `SOLID_BEGIN`, `VISUALS_BEGIN`, and `DOOR_ZONES` sections.
- [ ] Keep the V1 parser and migrate its inferred solidity/default door openings in memory.
- [ ] Copy V2 data into `RoomLayout` and implement locked/cleared collision behavior.
- [ ] Rerun blueprint and collision tests to green.

### Task 3: Discoverable All-Tileset Asset Catalog

**Files:**
- Create: `TestGame/RoomAssetCatalog.h`
- Create: `TestGame/RoomAssetCatalog.cpp`
- Create: `TestGame/RoomAssetCatalogTests.cpp`
- Modify: `TestGame/TestGame.vcxproj`
- Modify: `TestGame/TestGame.vcxproj.filters`

**Interfaces:**
- `RoomAssetCatalog::Refresh(mapTilesetRoot, metadataRoot)` discovers mapped biome PNGs plus the three shared sheets.
- `RoomAssetCatalog::Sources()` returns stable alphabetic source tabs with stem, image path, and `TileDefSet`.
- `FindSource(stem)` and `FindAsset(sourceStem, kind, assetId)` provide data-only resolution without loading textures.

- [ ] Write a failing filesystem test with two biome mapper files and the three shared PNG names.
- [ ] Confirm discovery/resolution APIs are missing.
- [ ] Implement deterministic discovery, special-sheet classification, and missing-file warnings.
- [ ] Add project entries and run the catalog test to green.

### Task 4: Layered Draw Map UI

**Files:**
- Modify: `TestGame/RoomEditor.h`
- Modify: `TestGame/RoomEditor.cpp`
- Modify: `TestGame/RoomEditorTests.cpp`
- Modify: `TestGame/TileMapper.cpp`

**Interfaces:**
- `RoomEditor::Bind` receives a `RoomAssetCatalog` and lazily loads source textures while Draw Map is active.
- Layers become `Ground`, `VisualTiles`, `Props`, `Decor`, `Collision`, `FallZones`, and `Doors`.
- Source tabs change palette data only; selections are remembered per source/layer.
- Door mode exposes locked/cleared preview, selectable zone rectangles, drag movement, resize handles, and exact quarter-tile +/- controls.

- [ ] Write failing editor tests for source-qualified placement, solid painting, door-zone undo/redo, and cleared-preview filtering.
- [ ] Confirm the new editor APIs are absent.
- [ ] Implement source tabs and scrollable asset sections, including shared water/lava tabs/content.
- [ ] Implement ground and non-ground tile placement, collision brush, door editing, full-size ghosts, and missing-asset placeholders.
- [ ] Preserve one-click whole-asset placement, visible Undo/Redo, Tab palette hiding, library edit/delete, and S save.
- [ ] Rerun editor tests to green and compile `RoomEditor.cpp` with UI enabled.

### Task 5: Runtime Rendering, Collision, and Room-Clear Integration

**Files:**
- Modify: `TestGame/TileRenderer.h`
- Modify: `TestGame/TileRenderer.cpp`
- Modify: `TestGame/Engine.h`
- Modify: `TestGame/Engine.cpp`
- Modify: `TestGame/RoomLibrary.h`
- Modify: `TestGame/RoomLibrary.cpp`
- Create: `TestGame/RoomDoorClearTests.cpp`

**Interfaces:**
- `TileRenderer::RegisterRoomSource(stem, texturePath, definitions)` owns one texture per referenced source and `ClearRoomSources()` releases them.
- Handcrafted runtime placements retain resolved source stem and asset index; procedural placements remain source-empty and use `_defs`.
- `RoomLayout::roomCleared` controls door-zone visual filtering and collision.
- `Engine::ConfigureHandcraftedRoomSources()` registers unique sources after building or entering a handcrafted room.

- [ ] Write failing tests for complete-object door clearing, ground preservation, and source collection without duplicates.
- [ ] Confirm runtime helper APIs are missing.
- [ ] Resolve source-qualified placements through `RoomAssetCatalog` in `RoomLibrary`/`BuildRoomLayout`.
- [ ] Extend renderer tile, decor, and prop passes to select the registered source texture/definition and skip non-ground overlaps when cleared.
- [ ] Set `roomCleared` whenever existing room state changes, rebuild navigation, and preserve state on re-entry.
- [ ] Make enemy spawning/navigation avoid `solid`, fall, and active door-zone cells.
- [ ] Run the new runtime test and all focused room tests to green.

### Task 6: Verification

**Files:**
- Review only all files above.

- [ ] Run `RoomBlueprintTests`, `RoomLibraryTests`, `RoomEditorTests`, `RoomCollisionTests`, `RoomAssetCatalogTests`, `TileAssetMetadataTests`, and `RoomDoorClearTests`.
- [ ] Compile the non-headless TileMapper, RoomEditor, TileRenderer, and Engine paths.
- [ ] Run `git diff --check` on only the touched files.
- [ ] Build `TestGame.sln` with `Debug|x64` and require exit code 0.
- [ ] Interactively verify biome tabs, full prop lists, water/lava sheets, collision painting, door resizing, locked/cleared preview, save/reload, and room-clear traversal; report if interactive verification cannot be performed.
