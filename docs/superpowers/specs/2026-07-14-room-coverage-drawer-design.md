# Room Coverage Drawer Design

## Purpose

Add a collapsible Room Coverage drawer to the Room Library screen so authored room progress is calculated from room data instead of filenames. The drawer reports which of the 15 non-empty N/S/E/W door combinations exist for the editor's current biome and tileset.

## Interface

- A compact top-right control always shows `Room Coverage: X/15`.
- Activating it expands a corner drawer without replacing the existing room list.
- Combinations are grouped as one-way, two-way, three-way, and four-way rooms.
- Present combinations use green styling and display a variant count when greater than one.
- Missing combinations use red styling and the word `Missing`.
- The drawer remembers its expanded state for the current editor session.

## Behavior

- Coverage uses `RoomBlueprint::DoorMask()` and never infers anything from a room's name.
- Only rooms matching the active editor biome and tileset count.
- Completion is strict: all 15 non-empty door masks must exist.
- Clicking a missing combination creates a fresh room with that exact door mask, gives it a readable default name, closes the library, and leaves it unsaved for editing.
- Clicking an existing combination does not modify or delete rooms.
- Forest currently reports 14/15 because NSW is missing; its two NSEW rooms display as `x2`.

## Implementation Boundaries

- Keep coverage calculation as a small pure helper that returns counts indexed by door mask.
- Reuse the existing Room Library and New Room flows.
- Do not change runtime room selection, four-door fallback behavior, room serialization, dungeon generation, or authored room files.

## Verification

- Unit-test coverage counting, biome/tileset filtering, duplicate counts, and all 15 masks.
- Unit-test creation of a room from a missing mask.
- Build `Debug|x64` and verify the drawer fits without covering the room edit/copy/delete controls.
