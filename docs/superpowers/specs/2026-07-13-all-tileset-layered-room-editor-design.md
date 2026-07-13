# All-Tileset Layered Room Editor Design

## Objective

Expand Draw Map into a layered handcrafted-room editor that can browse and place terrain, props, decor, and animations from every tileset without changing the room's primary biome. Forest rooms may use tree walls in one room and stone or ruin walls in another. The room must preserve its original ground beneath all wall artwork, use collision authored independently from artwork, and visibly show adjustable door clear zones.

Water and lava must be available from every source tab. `FD_Animated_Water.png` provides shared water frames, while `RA_Hell_Animations.png` provides shared Hell lava frames. Assets created from either sheet retain their source-sheet identity and render from that same sheet in the editor and game.

## Editor Model

Draw Map keeps the room's primary biome and tileset fixed after the room is created. A source-tab bar changes only the library being browsed. Tabs list every discovered biome tileset and use stable tileset stems, rather than a hardcoded enum order, so newly added tilesets appear automatically when their mapping file and PNG are present.

Each source tab exposes these sections:

- Ground tiles assigned by that tileset.
- Wall and other non-ground terrain tiles assigned by that tileset.
- Every static prop and animated prop authored for that tileset.
- Every static decor and animated decor asset authored for that tileset.
- Shared Ground Tiles, Animated Water, and Hell Lava sheets beneath the selected biome content.

Changing tabs never clears the current selection or modifies already placed content. A placed item records the source tileset or shared sheet that owns its pixels. Missing source files produce a visible missing-asset placeholder and a warning; they do not crash or silently substitute art from the room's primary biome.

## Room Layers

The room is authored through four independent layers:

1. **Ground:** Permanent walkable artwork. Ground remains when doors open and cannot be removed by room-clear rules.
2. **Visuals:** Wall tiles, props, decor, water, lava, and animations placed over ground. Each item retains its complete source asset, source sheet, position, and visual layer.
3. **Collision:** A solid-cell mask painted independently from artwork. Any visual can represent a wall when solid collision is painted beneath it. This allows trees, cliffs, rocks, castle masonry, or other suitable assets to form room boundaries without changing movement code.
4. **Fall:** The existing fall mask remains independent. Water, lava, pits, or other artwork becomes a fall hazard only where the author paints fall cells.

The editor renders layers in this order: ground, lower decor, wall/prop visuals, actors during playtest, and upper/canopy visuals. This permits tree canopies and tall structures to overlap the player while their trunks and bases retain correctly positioned collision.

## Cross-Tileset References

Every placed visual stores a stable source reference instead of assuming the active biome texture:

```text
sourceTileset "Forest"
sourceSheet Biome
kind AnimProp
assetId "forest_waterfall_01"
position 8 4
layer Wall
```

Shared source sheets use stable identifiers: `Ground Tiles`, `FD_Animated_Water`, and `RA_Hell_Animations`. An animated asset has one source-sheet identifier shared by all of its frames. Frames remain source rectangles within that sheet, which avoids copying image data or modifying the original asset packs.

Runtime room loading collects the unique source sheets referenced by the room, loads each texture once through the existing texture ownership path, and resolves stable asset IDs against the matching tileset definition. Textures are reused between rooms and unloaded through the normal biome/room transition lifecycle.

## Door Clear Zones

Each enabled north, south, east, or west exit owns one adjustable rectangular door zone. The editor draws the zone over the room with a clear label, translucent fill, outline, and resize/move handles. The author can change its offset, width, depth, and collider rectangle independently for every door.

Draw Map provides two preview states:

- **Locked:** The door-zone collider is active and all non-ground artwork remains visible.
- **Cleared:** The door-zone collider is disabled and non-ground visual tiles or whole props overlapping that zone are hidden, revealing the permanent ground below.

When gameplay marks the room cleared, the same rule is applied at runtime. Any non-ground visual placement whose occupied rectangle overlaps the door zone is removed as one complete placement. The system never clips half of a sprite. The author will keep permanent neighboring assets outside the zone. Ground is never cleared, even if it occupies every cell beneath the door.

Permanent collision outside the door zone remains active after room clear. Inside the cleared door rectangle, the temporary door collider and solid-mask contribution are ignored so the player can continue. Re-entering an already cleared room immediately uses the cleared state and does not briefly redraw or reactivate its barrier.

## Editor Interaction

The source-tab bar remains available while editing any layer. The asset palette preserves the current selection when hidden with Tab and remembers the last selection separately for each source tab and layer. Mouse-wheel scrolling covers the complete asset library.

Selecting an item shows the full-size snapped ghost before placement. The ghost uses the real source texture and aspect ratio, includes authored collision when applicable, and turns red when outside the room or otherwise invalid. One click places one complete asset. Right-click removes the topmost matching placement. Visible Undo and Redo buttons, plus `Ctrl+Z` and `Ctrl+Y`, cover placement, removal, collision painting, fall painting, door-zone adjustment, and layer changes that mutate room data. Source-tab and palette selections are editor UI state and do not consume history entries.

Door editing uses a dedicated mode rather than overloading ordinary asset selection. Clicking a door zone selects it; dragging moves it, handles resize it, and numeric controls allow exact values. A locked/cleared preview toggle immediately demonstrates which visuals will disappear and whether the opening is traversable.

## Save Compatibility

The `.mroom` format advances to version 2 because tile-source identity, solid masks, visual layers, and adjustable door zones change the room data model. Version 1 rooms remain loadable. On load, their existing border tiles become visual wall placements from the room's original tileset, existing floor tiles become permanent ground, existing wall solidity becomes the initial collision mask, and existing doors receive default zones matching their current openings.

Version 1 files are not rewritten until the user explicitly saves them. The migration occurs in memory and preserves existing placements, fall cells, door flags, and per-side wall depths. Saving produces version 2 data atomically using the existing temporary-file replacement path.

## Runtime Rules

The runtime renderer resolves every ground cell and visual placement through its saved source reference. Collision reads the independent solid mask plus the currently locked door zones; it no longer infers every handcrafted wall solely from the visual `TileType`. Procedural rooms and New Game/Continue remain unchanged until explicitly migrated later.

Dungeon Run continues to be the handcrafted-room test path. Room-clear state comes from the existing room progression state. While enemies remain, door zones are locked. On the first clear event, door-zone visuals disappear and their collision is disabled. Returning to that room restores the cleared version from room state.

Enemy spawning and pathfinding must treat solid cells, active door zones, and fall cells as unavailable. Door-zone visuals themselves do not need special pathfinding rules because their active collider already marks the blocked region.

## Error Handling

Rooms with a missing tileset mapping, texture, or asset ID remain editable. Missing content is displayed as a magenta checkerboard bearing the missing source name and asset ID. Save validation reports missing references but allows the user to save after confirming, preventing one renamed asset from making the entire room inaccessible.

Door rectangles are clamped to the room bounds and require positive width and height. A warning appears when an enabled door has no permanent ground beneath its zone or when its cleared collider still leaves no walkable path to the room edge. There is no per-asset doorway tag: every non-ground placement overlapping the zone follows the same automatic clear rule.

## Testing

Automated tests cover:

- Version 2 round-trip serialization of cross-tileset tile, prop, decor, and animation references.
- Version 1 in-memory migration without rewriting the source file.
- Discovery of every tileset mapping and shared water/lava sheet.
- Stable source-tab selection and full-asset placement history.
- Locked door collision and cleared door traversal.
- Removal of overlapping non-ground visual placements while preserving ground.
- Persistence of cleared-door state when re-entering a room.
- Graceful missing-source and missing-asset behavior.
- Runtime resolution of multiple texture sources without duplicate texture ownership.

The final verification target is the focused room/editor test suite followed by a `Debug|x64` solution build. An interactive Draw Map playtest must verify source tabs, water/lava animation previews, locked/cleared door previews, collider handles, save/reload, and gameplay room clearing.

## Scope Boundaries

This phase does not migrate procedural New Game/Continue rooms, redesign enemy AI, create new artwork, edit source PNG files, or add automatic biome-mixing rules. It gives the author explicit access to all existing assets and reliable tools for deciding what belongs in each handcrafted room.
