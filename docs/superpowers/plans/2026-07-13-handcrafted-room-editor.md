# Handcrafted Room Editor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a `Draw Map` tab that authors fixed-size handcrafted rooms from existing tiles, props, animated props, and decor, then safely playtests those rooms through Dungeon Run with procedural fallback and room-authored fall zones.

**Architecture:** Introduce pure-data `RoomBlueprint` and `RoomLibrary` modules, plus a focused `RoomEditor` component hosted by `TileMapper`. Convert selected blueprints into the existing `RoomLayout`, so rendering and authored prop colliders stay authoritative. Gate runtime selection behind a Dungeon Run-only policy; normal New Game, Continue, and prologue paths keep calling the procedural generator.

**Tech Stack:** C++17, raylib 5.5, `std::filesystem`, Visual Studio/MSBuild `Debug|x64`, standalone assertion tests compiled with `cl.exe`.

## Global Constraints

- Preserve every existing user, Claude, and Codex change in the dirty worktree. Never reset, revert, or broadly reformat unrelated files.
- Keep room dimensions exactly `28 x 16`.
- Keep center-fixed north, south, east, and west door sockets.
- Handcrafted rooms never receive procedural terrain patches, props, animated props, decor, or animated decor.
- Water and lava are ordinary named animated props authored through the existing prop pipeline.
- Fall behavior is a room-level tile mask, not an asset-name rule or separate animator.
- New Game, Continue, prologue, village, and main-run setup remain on the current room generator.
- Only the main-menu Dungeon Run entry enables handcrafted room selection.
- Missing, incompatible, or malformed room data falls back to `RoomLayout::Generate`.
- Enemy pathfinding and enemy/pickup spawn avoidance are deferred, but the fall mask must be exposed for that phase.
- Build `Debug|x64` and run focused tests after every runtime-facing task.

---

## File Structure

- Create `TestGame/RoomBlueprint.h/.cpp`: room data model, versioned text serialization, validation, conversion helpers.
- Create `TestGame/RoomLibrary.h/.cpp`: directory scanning, indexing, exact compatibility filtering, non-repeating selection, save/delete operations.
- Create `TestGame/RoomEditor.h/.cpp`: editor state, layer tools, placement model, command history, library view, and raylib drawing/input.
- Create `TestGame/RoomBlueprintTests.cpp`: serialization, validation, compatibility, and library assertions.
- Modify `TestGame/TileDefs.h/.cpp`: stable asset IDs, names, playback modes, lookup helpers, and backward-compatible parsing.
- Modify `TestGame/TileMapper.h/.cpp`: host the Draw Map tab, feed the active tileset to RoomEditor, and enhance finalized animation editing.
- Modify `TestGame/RoomLayout.h/.cpp`: carry fall masks and identify authored layouts without changing procedural generation.
- Modify `TestGame/Engine.h/.cpp`: Dungeon Run-only room policy, room-scroll parity, interior tile collision, and fall recovery.
- Modify `TestGame/TestGame.vcxproj` and `.filters`: compile the new production modules.

---

### Task 1: Room Blueprint Data And Versioned Serialization

**Files:**
- Create: `TestGame/RoomBlueprint.h`
- Create: `TestGame/RoomBlueprint.cpp`
- Create: `TestGame/RoomBlueprintTests.cpp`

**Interfaces:**
- Produces `enum class RoomAssetKind { Prop, AnimProp, Decor, AnimDecor };`.
- Produces `struct RoomAssetPlacement { RoomAssetKind kind; std::string assetId; int col; int row; };`.
- Produces `struct RoomBlueprint` with fixed terrain/fall arrays, metadata, `Validate`, `Save`, and `Load`.
- Produces `unsigned char RoomDoorMask(bool north, bool south, bool east, bool west)`.

- [ ] **Step 1: Write the failing round-trip and malformed-file tests**

Create assertions equivalent to:

```cpp
RoomBlueprint source = RoomBlueprint::CreateDefault();
source.id = "room-test-1";
source.name = "Narrow Crossing";
source.biome = Biome::Caverns;
source.tilesetStem = "Caverns";
source.roomType = RoomType::Standard;
source.hasNorth = source.hasSouth = true;
source.tiles[5][7] = TileType::FloorVariant;
source.fall[6][8] = true;
source.placements.push_back({ RoomAssetKind::AnimProp, "water_center_01", 8, 5 });
assert(source.Save(tempPath, error));
auto loaded = RoomBlueprint::Load(tempPath, error);
assert(loaded.has_value());
assert(loaded->fall[6][8]);
assert(loaded->placements[0].assetId == "water_center_01");
assert(!RoomBlueprint::Load(truncatedPath, error).has_value());
```

- [ ] **Step 2: Compile to verify the test fails because the API is absent**

Run from an x64 Visual Studio developer prompt:

```bat
cl /nologo /std:c++17 /EHsc /I TestGame /I C:\CLibraries\raylib-5.5_win64_msvc16\include TestGame\RoomBlueprint.cpp TestGame\RoomBlueprintTests.cpp /Fe:%TEMP%\RoomBlueprintTests.exe
```

Expected: compile failure naming missing `RoomBlueprint` APIs.

- [ ] **Step 3: Implement the fixed data model and line-oriented parser**

Use these public declarations:

```cpp
struct RoomBlueprint {
    static constexpr int kVersion = 1;
    std::string id, name, tilesetStem;
    Biome biome = Biome::Caverns;
    RoomType roomType = RoomType::Standard;
    bool hasNorth = false, hasSouth = false, hasEast = false, hasWest = false;
    TileType tiles[RoomLayout::kRows][RoomLayout::kCols]{};
    bool fall[RoomLayout::kRows][RoomLayout::kCols]{};
    std::vector<RoomAssetPlacement> placements;

    static RoomBlueprint CreateDefault();
    unsigned char DoorMask() const;
    bool Validate(std::string& error) const;
    bool Save(const std::filesystem::path& path, std::string& error) const;
    static std::optional<RoomBlueprint> Load(const std::filesystem::path& path,
                                              std::string& error);
};
```

Parse with `std::getline`, `std::istringstream`, `std::quoted`, bounded placement counts, exactly 16 terrain rows of 28 integers, and exactly 16 fall rows of 28 `0/1` characters. Save to `<name>.tmp`, close successfully, then replace the destination.

- [ ] **Step 4: Run the focused test**

Expected: executable exits `0`; malformed files return `std::nullopt` and a non-empty error.

- [ ] **Step 5: Commit only Task 1 files**

```bat
git add TestGame\RoomBlueprint.h TestGame\RoomBlueprint.cpp TestGame\RoomBlueprintTests.cpp
git commit -m "feat: add handcrafted room blueprint format"
```

---

### Task 2: Stable Named Tile Assets And Animation Playback

**Files:**
- Modify: `TestGame/TileDefs.h`
- Modify: `TestGame/TileDefs.cpp`
- Modify: `TestGame/TileMapper.h`
- Modify: `TestGame/TileMapper.cpp`
- Modify: `TestGame/TileRenderSourceTests.cpp`

**Interfaces:**
- Produces `enum class AnimPlaybackMode { Loop, PingPong, PlayOnce };`.
- Adds `id` and `name` to `SpriteDef`, `AnimSpriteDef`, and `AnimPropDef`.
- Adds `playback` to animated definitions.
- Produces `int TileDefSet::FindAssetIndex(RoomAssetKind kind, std::string_view id) const`.

- [ ] **Step 1: Add failing backward-compatibility and stable-lookup assertions**

Test a legacy mapper file, an extended mapper file, renaming without ID changes, and playback parsing:

```cpp
assert(defs.LoadFromFile(legacyPath));
assert(!defs.animProps[0].id.empty());
assert(defs.animProps[0].playback == AnimPlaybackMode::Loop);
assert(defs.FindAssetIndex(RoomAssetKind::AnimProp, defs.animProps[0].id) == 0);
```

- [ ] **Step 2: Run the focused test and observe failure**

Compile `TileDefs.cpp`, `RoomBlueprint.cpp`, and the focused test against raylib headers. Expected failure: metadata and lookup members do not exist.

- [ ] **Step 3: Extend definitions without breaking legacy files**

Extended save records use quoted strings and explicit tags:

```text
PROPV2 "stable-id" "Display Name" x y w h cx cy cw ch
ANIMPROPV2 "stable-id" "Display Name" playback fps cx cy cw ch frameCount ...
DECORV2 "stable-id" "Display Name" x y w h
ANIMDECORV2 "stable-id" "Display Name" playback fps frameCount ...
```

Keep parsing `PROP`, `ANIMPROP`, `DECOR`, and `ANIMDECOR`. Generate deterministic legacy IDs from kind plus original order. Existing animation playback defaults to Loop.

- [ ] **Step 4: Enhance finalized animation controls**

In the existing Props/Decors panel, selecting a finalized animated definition exposes Edit Frames, Loop/Ping-Pong/Play Once, FPS, Rename, Duplicate, Collider (props), and Delete. Playback buttons are mutually exclusive. Editing retains the stable ID; Duplicate generates a new ID.

- [ ] **Step 5: Run tests and build Debug x64**

```bat
MSBuild.exe TestGame.sln /m /p:Configuration=Debug /p:Platform=x64
```

Expected: zero errors and no behavior change for existing mapper files.

- [ ] **Step 6: Commit Task 2 files**

```bat
git add TestGame\TileDefs.h TestGame\TileDefs.cpp TestGame\TileMapper.h TestGame\TileMapper.cpp TestGame\TileRenderSourceTests.cpp
git commit -m "feat: add stable named room assets"
```

---

### Task 3: Region And Tileset Room Library

**Files:**
- Create: `TestGame/RoomLibrary.h`
- Create: `TestGame/RoomLibrary.cpp`
- Modify: `TestGame/RoomBlueprintTests.cpp`

**Interfaces:**
- Produces `void RoomLibrary::Refresh(const std::filesystem::path& root)`.
- Produces `const std::vector<RoomBlueprint>& RoomLibrary::Rooms() const`.
- Produces `const RoomBlueprint* RoomLibrary::Choose(const RoomRequest&, int avoidIndex = -1) const`.
- Produces `bool SaveRoom`, `bool DeleteRoom`, and `bool NameExists`.

- [ ] **Step 1: Add failing exact-compatibility tests**

```cpp
RoomRequest request{ Biome::Caverns, "Caverns", RoomType::Standard,
                     RoomDoorMask(true, true, false, false) };
const RoomBlueprint* selected = library.Choose(request);
assert(selected != nullptr);
assert(selected->tilesetStem == "Caverns");
assert(selected->DoorMask() == request.doorMask);
assert(library.Choose({ Biome::Forest, "Forest", RoomType::Boss, 0x0f }) == nullptr);
```

- [ ] **Step 2: Verify the test fails, then implement scanning and indexing**

Scan only `.mroom` files below the configured root. Record file path alongside each parsed blueprint. Invalid files produce one `TraceLog(LOG_WARNING, ...)` and are excluded. `Choose` filters exact biome, stem, type, and mask before random selection.

- [ ] **Step 3: Implement atomic save, rename/duplicate support, and deletion**

Sanitize slugs to lowercase ASCII letters, digits, and underscores. Never delete outside the configured room root; canonicalize both paths and verify the destination begins with the canonical root.

- [ ] **Step 4: Run tests and commit**

Expected: all temporary room-library tests exit `0`.

---

### Task 4: Room Editor Model, Layers, And Undo History

**Files:**
- Create: `TestGame/RoomEditor.h`
- Create: `TestGame/RoomEditor.cpp`
- Modify: `TestGame/RoomBlueprintTests.cpp`

**Interfaces:**
- Produces `RoomEditor::BindTileset(...)`, `NewRoom`, `OpenRoom`, `Update`, `Draw`, and `HasUnsavedChanges`.
- Produces layers `Terrain`, `Props`, `Decor`, `FallZones`, and `Markers`.
- Produces command-batched paint/placement undo and redo.

- [ ] **Step 1: Test pure editing operations before raylib UI**

Expose narrow model methods used by input and tests:

```cpp
editor.SetTerrain(4, 5, TileType::FloorVariant);
editor.SetFall(8, 6, true);
editor.PlaceAsset({ RoomAssetKind::AnimProp, "water_center_01", 7, 5 });
editor.EndStroke();
editor.Undo();
assert(!editor.Blueprint().fall[6][8]);
editor.Redo();
assert(editor.Blueprint().fall[6][8]);
```

- [ ] **Step 2: Implement bounded editing and command history**

Use one command per drag stroke, cap history at 128 commands, reject out-of-bounds anchors and protected door-lane cells, and clear redo after a new edit.

- [ ] **Step 3: Implement palette and canvas input**

Left click/drag paints, right click erases the active layer, selection-drag moves one placement, `Delete` removes it, middle drag pans, wheel scrolls the hovered palette, `Tab` toggles palette visibility without clearing selection, `S` saves, and `Ctrl+Shift+S` saves as.

- [ ] **Step 4: Implement rendering and room-library screen**

Draw actual mapped tiles and asset frames on the canvas. Overlay the grid, enabled door sockets, selected placement, protected entry lanes, and translucent red fall cells. The library supports New, Edit, Duplicate, Rename, Playtest, and Delete confirmation.

- [ ] **Step 5: Run model tests and build**

Expected: model tests exit `0`; Debug x64 compiles with the component not yet entered at runtime.

---

### Task 5: Host Draw Map Inside Tile Editor

**Files:**
- Modify: `TestGame/TileMapper.h`
- Modify: `TestGame/TileMapper.cpp`
- Modify: `TestGame/TestGame.vcxproj`
- Modify: `TestGame/TestGame.vcxproj.filters`

**Interfaces:**
- Extends `TileMapper::Screen` with `DrawMap` while preserving FileSelect and Mapping.
- Passes the open tileset stem, biome, sheet texture, ground texture, and current definitions to `RoomEditor`.

- [ ] **Step 1: Add project entries for new production files**

Add `RoomBlueprint.cpp`, `RoomLibrary.cpp`, and `RoomEditor.cpp` to ClCompile and their headers to ClInclude. Do not add standalone test mains to the game target.

- [ ] **Step 2: Add the Draw Map tab and lifecycle**

The tab is available only after opening a tileset. Entering binds/reloads editor assets. Returning to Mapping keeps mapper selection and sheet position. `TileMapper::Unload` releases RoomEditor-owned state without unloading textures owned by TileMapper.

- [ ] **Step 3: Keep save responsibilities separate**

Mapper `S` writes `tilemapper_<stem>.txt`; Draw Map `S` writes the active `.mroom`. Refresh RoomEditor asset lookup after mapper export so newly finalized water/lava props appear without restarting the tool.

- [ ] **Step 4: Build and manually exercise the authoring loop**

Verify: open tileset, enter Draw Map, hide/show palette, paint terrain, place an animated prop, place decor, paint/erase fall, save, reopen, duplicate, rename, and delete.

- [ ] **Step 5: Commit editor integration**

Commit only TileMapper, RoomEditor, project, and filter changes from Tasks 4-5.

---

### Task 6: Convert Blueprints To Runtime RoomLayout

**Files:**
- Modify: `TestGame/RoomLayout.h`
- Modify: `TestGame/RoomLayout.cpp`
- Modify: `TestGame/RoomBlueprint.h/.cpp`
- Modify: `TestGame/RoomBlueprintTests.cpp`

**Interfaces:**
- Adds `bool handcrafted` and `bool fall[kRows][kCols]` to `RoomLayout`.
- Produces `std::optional<RoomLayout> BuildRoomLayout(const RoomBlueprint&, const TileDefSet&, std::string& error)`.

- [ ] **Step 1: Add failing conversion tests**

Assert exact terrain, resolved vector indices, skipped missing asset IDs, and exact fall masks. Confirm procedural `RoomLayout::Generate` returns `handcrafted == false` and an empty fall mask.

- [ ] **Step 2: Implement one-time asset resolution**

Resolve each stable placement ID through `TileDefSet::FindAssetIndex`, append to the corresponding existing placement vector, and set `handcrafted = true`. Missing IDs warn and skip only that placement.

- [ ] **Step 3: Run tests and build**

Expected: focused tests pass and existing procedural room rendering remains unchanged.

---

### Task 7: Dungeon Run-Only Handcrafted Selection

**Files:**
- Modify: `TestGame/Engine.h`
- Modify: `TestGame/Engine.cpp`
- Modify: `TestGame/RoomLibrary.h/.cpp`

**Interfaces:**
- Adds `bool _useHandcraftedDungeonRooms = false`.
- Adds `RoomLayout Engine::BuildDungeonRoomLayout(int roomIdx, const TileDefSet&, int visualVariant)`.
- Stores the room-library root and refreshes it when Dungeon Run begins.

- [ ] **Step 1: Centralize room construction**

Replace direct calls in `EnterDungeonRoom` and next-room scroll preparation with `BuildDungeonRoomLayout`. The helper computes the exact door mask and active mapper stem, asks RoomLibrary only when `_useHandcraftedDungeonRooms` is true, then falls back to `RoomLayout::Generate`.

- [ ] **Step 2: Enable the policy only from the Dungeon Run menu button**

Set true in `_menu.DungeonRunPressed()`. Set false in `StartMainRun`, `StartPrologue`, Continue/village run setup, debug main-game starts, and any other non-Dungeon Run initialization before rooms are built.

- [ ] **Step 3: Preserve scroll parity and dynamic doors**

Prepare `_dungeonScrollNextLayout` through the same helper and correct next visual-variant definitions. Continue calling `ApplyDungeonRoomDoorState` after loading.

- [ ] **Step 4: Build and smoke-test fallback**

With an empty Rooms directory, Dungeon Run must behave exactly as before. New Game and Continue must never query the room library.

- [ ] **Step 5: Smoke-test authored selection**

Save one room matching a Dungeon Run room's stem/type/mask and verify it appears with no extra procedural props or decor before, during, and after room scrolling.

---

### Task 8: Interior Collision And Fall Recovery

**Files:**
- Modify: `TestGame/Engine.h`
- Modify: `TestGame/Engine.cpp`
- Modify: `TestGame/RoomBlueprintTests.cpp`

**Interfaces:**
- Stores `_dungeonFallRecoveryPos`, `_dungeonFallState`, `_dungeonFallTimer`, and `_dungeonFallInvulnerabilityTimer`.
- Adds `bool IsDungeonFallCell(Vector2 worldPoint) const`, `BeginDungeonFall()`, and `UpdateDungeonFall(float dt)`.

- [ ] **Step 1: Add pure coordinate assertions**

Extract/test world-point to room-cell conversion at room edges and the bottom-center player contact point. Out-of-range points are never fall cells.

- [ ] **Step 2: Store the recovery position on every room entry**

Use the actual `playerSpawnPos` accepted by `EnterDungeonRoom`; this already represents the safe point just inside the entry door. Update it only after a completed room transition, never while the player moves.

- [ ] **Step 3: Add handcrafted interior tile collision**

For handcrafted layouts, block mapped wall/void interior cells while leaving Floor, FloorVariant, DoorOpen, and authored fall cells traversable. Keep existing border clamps and procedural-room behavior unchanged.

- [ ] **Step 4: Make fall checks outrank prop collision**

After player movement and before prop resolution, sample the bottom-center of `_player.GetCollisionRec()`. If the cell is painted fall, begin the fall sequence and skip ordinary prop/interior collision for that frame.

- [ ] **Step 5: Implement recovery feedback and damage**

Lock movement/casts briefly, fade the player, apply exactly one damage through the normal player damage/death path, and if alive restore `_dungeonFallRecoveryPos`. Grant a short recovery invulnerability window. A lethal fall must use normal death handling.

- [ ] **Step 6: Build and manually verify all entry directions**

Enter the same fall room through north, south, east, and west doors. Falling must return to the latest entry position rather than a fixed room corner or previous room's entry.

---

### Task 9: Final Verification And Delivery

**Files:**
- Verify all changed files.
- Update the implementation plan checkboxes as tasks complete.

- [ ] **Step 1: Run every focused standalone test**

Run RoomBlueprint/RoomLibrary/editor-model tests plus existing TileDefs/room tests. Expected: all executables exit `0`.

- [ ] **Step 2: Build the game**

```bat
MSBuild.exe TestGame.sln /m /p:Configuration=Debug /p:Platform=x64
```

Expected: zero errors. Record existing warnings separately from new warnings.

- [ ] **Step 3: Launch-smoke the Debug x64 executable**

Verify main menu load, Tile Editor entry/exit, Dungeon Run entry, and clean return to menu without crash.

- [ ] **Step 4: Perform regression checks**

Verify New Game still enters the tutorial, Continue still enters the established village flow, prologue rooms remain procedural, and unrelated enemy behavior files are unchanged by this feature.

- [ ] **Step 5: Review the final diff**

Run `git diff --check`, inspect `git status --short`, and ensure commits contain only the room-editor feature plus the already-committed design/plan documents.

