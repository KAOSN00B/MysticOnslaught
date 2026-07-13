# Handcrafted Room Editor Design

**Date:** 2026-07-13  
**Project:** Mystic Onslaught  
**Status:** Approved design, awaiting implementation plan

## Purpose

Mystic Onslaught currently creates every dungeon room from the same fixed rectangular pattern and then scatters props and decor procedurally. This produces functional rooms, but it prevents a designer from intentionally making ponds, bridges, blocked corners, narrow paths, pits, lava fields, or carefully composed combat spaces.

This feature adds a `Draw Map` tab to the existing Tile Editor. The designer will paint complete `28 x 16` room blueprints for a particular region and tileset variant, place already-authored props and decor, paint fall zones, choose active doors, save the room, and test it through Dungeon Run. The current `New Game` and `Continue` flows remain unchanged until the handcrafted system has enough content and has been explicitly approved as their replacement.

## Goals

- Author intentional, fixed-size dungeon rooms without replacing the existing tileset-mapping workflow.
- Reuse the existing tile, prop, animated prop, decor, animated decor, and collider definitions.
- Let the designer paint or drag terrain and place/move props and decor directly on the room canvas.
- Treat water and lava as ordinary region-specific animated props, not as a new animation subsystem.
- Paint fall behavior separately on a room-level, tile-snapped gameplay layer.
- Save rooms into a region and tileset-variant library that Dungeon Run can select by room type and exact door layout.
- Preserve the old procedural generator as a safe fallback and leave `New Game` and `Continue` on that generator for this phase.
- Keep saved room references stable when assets are renamed, edited, added, or deleted.
- Fail safely when a room file or asset reference is missing or malformed.

## Non-Goals

- Replacing the procedural generator in `New Game`, `Continue`, the prologue, village, or normal campaign runs.
- Reworking enemy pathfinding or enemy spawn selection in this phase. Room data will expose walkable and fall cells so that work can follow cleanly.
- Adding variable room dimensions, room rotation, room mirroring, diagonal doors, or free-positioned doors.
- Building a separate water/lava animator.
- Randomly adding props or decor to handcrafted rooms.
- Automatically generating handcrafted rooms from procedural output.
- Changing combat balance, enemy behavior, village systems, progression, shops, or unrelated UI.

## Existing System Boundaries

- `RoomLayout` already owns a `28 x 16` `TileType` grid and four placement vectors: props, animated props, decor, and animated decor.
- `RoomLayout::Generate` creates the border, doors, floor variation, and random prop/decor placement.
- `DungeonGen` creates the room graph and records room type plus north, south, east, and west connectivity.
- `Engine::EnterDungeonRoom` and room-scroll preparation currently call `RoomLayout::Generate` directly.
- `TileMapper` already maps terrain, static props, animated props, decor, animated decor, colliders, and spawn-zone constraints for each tileset PNG.
- `TileDefSet` loads runtime definitions from `tilemapper_<stem>.txt`.
- Dungeon visual variants already identify a mapper stem and sheet stem. A handcrafted room must therefore target both a region and a specific mapper/tileset stem.
- Dungeon Run is entered independently from `New Game` and `Continue`, which provides the requested isolated integration point.

## Architecture

### Room Blueprint

Add a serializable `RoomBlueprint` data type that represents authored data without owning textures or game objects. It contains:

- File format version.
- Stable room identifier and editable display name.
- Region/biome identifier used internally.
- Tileset mapper stem and visual-variant stem.
- `RoomType` compatibility.
- Four enabled door flags.
- Exactly `28 x 16` terrain cells.
- Ordered placed-asset records for static props, animated props, static decor, and animated decor.
- Exactly `28 x 16` fall-zone flags.
- Optional editor-only metadata such as the last active layer and palette scroll is not written into runtime room data.

`RoomBlueprint` converts into a normal `RoomLayout` for rendering and existing collision systems. The converted layout contains only authored placements. It never invokes procedural prop, decor, floor-patch, or terrain placement.

### Stable Asset Identity

Room files cannot safely store only vector indices because deleting `Prop #2` would shift all later indices. Extend authored prop/decor definitions with a stable asset identifier and editable display name. Runtime definitions retain their compact vectors, but also expose a lookup from stable identifier to the current vector index.

Legacy `tilemapper_*.txt` files remain loadable. Definitions without an identifier receive a deterministic legacy identifier derived from their kind and original order. The next Tile Editor save writes the extended definition format. Renaming changes only the display name; editing frames, FPS, playback, source rectangle, or collider does not change the stable identifier.

When loading a room, an unresolved asset identifier produces a warning and skips only that placement. It does not crash or reject the rest of the room.

### Room Library

Add a `RoomLibrary` responsible for scanning, loading, validating, indexing, selecting, and saving blueprints. Store files under:

```text
Rooms/<Region>/<TilesetStem>/<room-slug>.mroom
```

The runtime index groups rooms by region, tileset stem, room type, and four-bit door mask. Selection receives those exact requirements and returns a random compatible blueprint. A recently used room should be avoided when multiple candidates exist, but strict no-repeat history is not required in the first version.

The library owns file and compatibility logic; `Engine` and `TileMapper` consume its public API instead of parsing files themselves.

### Room Editor Component

Keep `TileMapper` as the entry point, but place room-authoring state and behavior in a focused `RoomEditor` component. `TileMapper` supplies the currently open tileset stem, region, texture, and loaded asset definitions. This prevents the already-large `TileMapper.cpp` from absorbing the room model, command history, serialization, and library management.

From the user's perspective, `RoomEditor` appears as the `Draw Map` tab within Tile Editor. Internally it owns the working blueprint, canvas interaction, palette state, placement selection, undo/redo history, validation messages, and room-library screen.

## Editor Workflow

### Entering Draw Map

The designer first opens a tileset as today. A top-level `Draw Map` tab becomes available beside the existing mapping workspace. Entering it binds the room to that tileset's region and mapper stem. The room canvas uses the same tile definitions and textures that runtime will use.

The toolbar contains:

- Region and tileset identity, displayed but derived from the open file.
- Editable room name.
- Room type selector.
- Toggle buttons for north, south, west, and east doors.
- New Room, Room Library, Save, Save As, Playtest, Undo, and Redo.

### Fixed Canvas And Doors

The canvas is always `28 x 16`. It uses the current dungeon tile scale and fixed center door sockets. Enabling a door paints or restores the correct opening at that socket; disabling it restores the appropriate wall tiles. Door cells and a short safe entry lane are protected from accidental prop placement and fall painting.

The room does not rotate or mirror at runtime. Its door mask must exactly match the `DungeonRoom` that requests it. This preserves intentional composition and directional artwork.

### Layers

The editor exposes these mutually exclusive layers:

1. `Terrain`: paint `TileType` cells from the currently mapped tile definitions.
2. `Props`: place static or animated solid assets. Water and lava live here when authored as animated props.
3. `Decor`: place static or animated non-solid decorative assets.
4. `Fall Zones`: paint tile-snapped gameplay cells over any visual layer.
5. `Markers`: reserved for the later spawn/pathfinding phase and not required for initial room validity.

Changing layers does not alter content on other layers. The eraser affects only the active layer.

### Painting And Selection

- Left click paints one terrain/fall cell or places the selected asset.
- Holding and dragging paints terrain and fall cells continuously.
- Holding and dragging an asset brush places another instance only after the cursor enters a new valid anchor cell; overlap rejection prevents accidental piles.
- The selection tool picks a placed prop or decor. A selected instance can be dragged to a new tile anchor or removed with `Delete`.
- Right click erases from the active layer.
- `Ctrl+Z` and `Ctrl+Y` undo and redo editor commands.
- Middle-mouse drag pans the canvas when necessary.
- Mouse wheel scrolls whichever palette/list is under the pointer.
- `Tab` hides or restores the palette without clearing the selected brush, layer, or palette scroll.
- `S` saves. `Ctrl+Shift+S` performs Save As.
- `Escape` returns to mapping or the library. Unsaved changes require confirmation.

### Palette

The palette is filtered to definitions from the open tileset and grouped into Terrain, Props, and Decor. Water and lava are not special runtime types; they appear as named static or animated props. The palette shows a thumbnail, display name, animation indicator, footprint, and collider indicator.

If a tileset has no asset whose display name/category identifies water or lava, Draw Map shows a non-blocking warning. It does not borrow mismatched art from another region automatically. The designer can return to the prop tools, author the missing animation from that region's sheet, save it, and then use it immediately in Draw Map.

### Room Library Management

The Room Library lists rooms for the open region and tileset. Each row shows thumbnail or miniature grid preview, name, room type, and door mask. Actions are:

- Edit.
- Duplicate.
- Rename.
- Playtest.
- Delete with confirmation.

Deleting removes the room file and refreshes the in-memory index. It never deletes tileset art or asset definitions. Duplicate creates a new stable room identifier and requires a unique name/slug.

## Prop Animation Enhancements

Animated water, lava, and other props continue using the existing animated-prop authoring flow. The designer selects source rectangles individually in playback order and presses `Make Animation`/finalize. The result becomes a named reusable animated prop with its existing collider editor.

Selecting a finalized animation exposes:

- Edit Frames.
- Reorder or remove frames.
- Add additional selected frames.
- Playback mode buttons: `Loop`, `Ping-Pong`, and `Play Once` as a mutually exclusive segmented control.
- FPS adjustment with live preview.
- Rename.
- Duplicate.
- Collider editing.
- Delete with a warning when saved rooms reference the asset.

Playback settings are shared by every placed instance. `Play Once` starts on room entry and holds its last frame. Existing animated definitions default to `Loop` so legacy visuals do not change.

## Fall Zones

Fall behavior is authored on the room rather than hardcoded into water, lava, or pit asset names. The designer places the desired visuals first, selects `Fall Zones`, and paints over cells that should cause a fall.

Fall-zone cells take gameplay precedence over solid prop collision in the covered cell. This allows an animated prop to retain its authored collider while the room decides that selected portions are fallable. Unpainted parts of the same prop remain ordinary solid collision. A fall triggers when the player's ground-contact point at the bottom-center of the collision body enters a painted cell; merely brushing a fall cell with the upper body or weapon does not trigger it.

When the player's collision/body enters a fall cell:

1. Input and attacks are briefly locked.
2. A short fall/fade response plays.
3. The player takes one point of damage through the normal damage path, without bypassing death handling.
4. If still alive, the player returns to a safe point immediately inside the door used to enter the room.
5. The player receives brief recovery invulnerability to prevent immediate repeat falls.

The safe recovery point is derived from `_dungeonEntryDoorSide` and the existing door spawn coordinates. If a room was entered without a door side, use the initial spawn supplied to `EnterDungeonRoom`. Store that recovery point when room entry completes rather than recomputing it from the player's later position.

Fall zones are visually overlaid in the editor and hidden during normal gameplay. Their cells are exposed as non-walkable to future enemy navigation and invalid for future enemy/pickup spawning, but enemy pathfinding and spawning changes are intentionally deferred.

## File Format

Use a versioned, line-oriented text format so rooms remain inspectable and recoverable. Parsing uses complete lines and bounded counts rather than unrestricted token reads. A representative file is:

```text
MROOM 1
ID 8b6c...
NAME "Narrow Water Crossing"
BIOME 1
TILESET "Caverns"
ROOMTYPE 0
DOORS 1 1 0 0
TILES_BEGIN
0 0 0 0 ... 28 values
... 16 rows
TILES_END
PLACEMENTS_BEGIN
ANIMPROP "water_center_01" 8 5
PROP "rock_column_02" 4 7
DECOR "bones_small_01" 12 9
PLACEMENTS_END
FALL_BEGIN
0000000000000000000000000000
... 16 rows
FALL_END
```

Unknown future tags are ignored with a warning. Unsupported major versions, invalid dimensions, excessive placement counts, missing terminators, or invalid tile values reject that room only. Save writes to a temporary sibling file and replaces the destination only after the complete write succeeds.

## Dungeon Run Integration

Add an explicit handcrafted-room policy used only by the main-menu Dungeon Run entry. `New Game`, `Continue`, the prologue, and main-run setup keep the current procedural policy.

When Dungeon Run enters or prepares a room scroll:

1. Determine the current region, active visual-variant mapper stem, `RoomType`, and exact door mask.
2. Ask `RoomLibrary` for a compatible blueprint.
3. If found, convert it to `RoomLayout`, preserve its authored terrain and placements, and attach its fall mask.
4. If no compatible valid room exists, call `RoomLayout::Generate` exactly as today.
5. Apply dynamic door lock/open state after loading, as the current runtime already does.

Handcrafted rooms never pass through procedural floor-patch, prop, animated-prop, decor, or animated-decor placement. Graph generation, room specials, enemies, rewards, room transitions, and door locking continue to use existing systems.

The same selection path must be used when constructing `_dungeonScrollNextLayout`; otherwise a handcrafted room would appear only after the scroll completes or visually change during transition.

## Rendering And Collision

Handcrafted blueprints convert to the existing `RoomLayout` vectors so `TileRenderer` and existing prop/decor rendering remain authoritative. No second renderer is introduced.

Player collision continues using authored prop collider rectangles. Before resolving a prop collision, test the player's bottom-center ground-contact point against the fall mask. A fall-zone trigger suppresses ordinary solid resolution for that contact and begins recovery. This priority rule is centralized so static and animated props behave identically.

The room terrain grid must also participate in player solidity for handcrafted interior walls and void cells. Border-only clamping remains for the outer room boundary and doors, while an interior tile collision pass blocks wall/void terrain that is not a fall zone. This is necessary for designer-painted obstacles to affect gameplay rather than only appearance.

## Validation And Error Handling

Saving is blocked when:

- The room name is empty or its slug conflicts with another room unless overwrite is confirmed.
- The dimensions are not exactly `28 x 16`.
- No door is enabled.
- A protected door opening or recovery cell is painted as a fall zone.
- An enabled door cannot reach at least one adjacent interior floor cell.
- A placement references an asset that no longer exists.
- Placement count or undo command limits are exceeded.

Saving warns but remains allowed when:

- No water asset is available for the tileset.
- No lava asset is available for the tileset.
- A room has no decor or props.
- A room type has very little open floor; enemy suitability is deferred.

Loading malformed files logs the exact filename and reason, excludes the room from selection, and continues loading the rest of the library. Runtime fallback guarantees Dungeon Run remains playable even with an empty or damaged room library.

## Performance And Memory

- Load room metadata and parsed blueprint data once when entering Dungeon Run or refreshing the editor library, not every frame.
- Store terrain and fall masks in fixed-size arrays.
- Keep texture ownership in existing tile renderers and TileMapper; room blueprints store identifiers and coordinates only.
- Cap undo/redo history by command count and clear redo history after a new edit.
- Batch drag painting into one undo command.
- Avoid allocating one animation controller per placed prop; use existing shared definition timing and room elapsed time.
- Resolve stable asset identifiers to runtime indices when the blueprint is converted, not during every draw or collision check.

## Testing

### Serialization Tests

- Round-trip a room containing every terrain and placement kind.
- Preserve exact tile grid, door mask, fall mask, names, and stable asset references.
- Reject truncated, oversized, unsupported-version, and invalid-dimension files without crashing.
- Skip one missing asset placement while preserving valid placements.
- Confirm atomic save leaves the previous room intact when writing fails.

### Library Tests

- Select only rooms matching region, tileset stem, room type, and exact door mask.
- Return no result when no compatible room exists.
- Refresh correctly after create, rename, duplicate, and delete.
- Avoid immediate repetition when at least two compatible rooms exist.

### Editor Tests

- Paint and drag terrain and fall cells.
- Place, select, move, and delete each placement kind.
- Confirm props retain colliders and decor remains non-solid.
- Confirm `Tab` preserves the active brush and palette scroll.
- Confirm undo/redo treats one drag stroke as one command.
- Confirm saved rooms reopen identically.
- Confirm animation frame editing and playback modes persist.

### Runtime Tests

- Dungeon Run uses a matching handcrafted room.
- A missing door-mask match falls back to the procedural room.
- New Game, Continue, and prologue room generation remain unchanged.
- Room scrolling renders the same handcrafted next room before and after transition.
- No random props or decor appear in handcrafted rooms.
- Static and animated prop colliders remain aligned.
- Interior painted walls block the player.
- Fall zones override covered solid collision, deal one damage, and recover at the room-entry safe point.
- A lethal fall enters normal death handling rather than respawning a dead player.
- Re-entry from a different door updates the recovery point.
- Build and run `Debug|x64`.

## Implementation Boundaries

Expected focused files include:

- New `RoomBlueprint.h/.cpp`.
- New `RoomLibrary.h/.cpp`.
- New `RoomEditor.h/.cpp`.
- `TileMapper.h/.cpp` for Draw Map integration and animation-authoring enhancements.
- `TileDefs.h/.cpp` for stable asset identity, names, and playback metadata with backward compatibility.
- `RoomLayout.h/.cpp` for authored blueprint conversion support without changing procedural generation behavior.
- `Engine.h/.cpp` for Dungeon Run policy, room selection, interior collision, and fall recovery.
- Visual Studio project/filter entries and focused tests.

Unrelated enemy, combat, progression, village, menu, and tutorial changes must be preserved and left untouched except where a compile dependency makes a minimal integration edit unavoidable.

## Delivery Sequence

1. Add data types, stable asset identity, serialization, and tests.
2. Add Room Library indexing and compatibility selection.
3. Add the isolated Room Editor component and Draw Map tab.
4. Enhance finalized animated-prop editing and playback persistence.
5. Add room-library create/edit/duplicate/delete/playtest workflows.
6. Integrate handcrafted selection into Dungeon Run only, including room-scroll preparation and procedural fallback.
7. Add interior tile collision and fall recovery.
8. Build, run focused tests, and smoke-test Dungeon Run plus unchanged New Game/Continue entry paths.
