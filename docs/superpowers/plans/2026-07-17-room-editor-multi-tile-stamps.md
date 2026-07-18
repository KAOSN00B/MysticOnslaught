# Room Editor Multi-Tile Stamps Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let Ground and Visual palettes select rectangular tile groups and repeatedly stamp them into handcrafted rooms.

**Architecture:** Extend `RoomEditor::SheetPaletteView` with one source-rectangle helper and add transient drag state to `RoomEditor`. Existing `Rectangle _selectedRawTile`, placement preview, `RoomTilePlacement`, serialization, and runtime rendering remain the data path.

**Tech Stack:** C++17, Raylib, MSVC, existing assert-based RoomEditor tests.

## Global Constraints

- A normal click must still select exactly one 16 by 16 source tile.
- Multi-tile selection applies only to Ground and Visual palettes.
- The selected stamp remains active after every placement.
- Existing zoom, pan, Door editing, room files, and runtime rendering remain compatible.

---

### Task 1: Multi-Tile Palette Selection

**Files:**
- Modify: `TestGame/RoomEditor.h`
- Modify: `TestGame/RoomEditor.cpp`
- Test: `TestGame/RoomEditorTests.cpp`

**Interfaces:**
- Produces: `SheetPaletteView::SourceRectBetween(int, int, int, int)` returning a 16-pixel-aligned `Rectangle`.
- Consumes: existing `SheetPaletteView::TileAt`, `_selectedRawTile`, `SetVisual`, and `DrawPlacementPreview`.

- [ ] Add a failing test asserting forward and reverse drags produce the same source rectangle and that the rectangle can be stamped repeatedly.
- [ ] Compile the test and confirm it fails because `SourceRectBetween` is absent.
- [ ] Implement `SourceRectBetween` and Ground/Visual palette drag state; update `_selectedRawTile` live while dragging.
- [ ] Keep Door selection on its existing single-click path and reset transient drag state on source changes.
- [ ] Run `RoomEditorTests` and require exit code 0.
- [ ] Build `Debug|x64` and require exit code 0.
- [ ] Run `git diff --check` on the three touched source files.
