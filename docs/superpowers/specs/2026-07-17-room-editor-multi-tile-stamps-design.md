# Room Editor Multi-Tile Stamps Design

## Scope

Ground and Visual sprite-sheet palettes support left-click drag selection of a
rectangular group of source tiles. A normal click remains a one-tile selection.
Door editing, prop metadata, room serialization, and runtime rendering formats
remain unchanged.

## Interaction

Pressing left mouse over a Ground or Visual sheet begins a selection. Dragging
updates the highlighted rectangle in real time; releasing keeps that rectangle
selected for repeated room placement. The rectangle is aligned to the existing
16 by 16 source grid and works through the current zoom and pan transform.

The existing room placement ghost previews the complete selection. Painting the
room stores the selection as the existing `RoomTilePlacement::src` rectangle,
anchored at the selected room cell. Single-click selection, weighted Ground
brushes, undo, save/load, and source switching keep their current behavior.

## Verification

A focused test verifies forward and reverse drag bounds and repeated placement
of the resulting source rectangle. `RoomEditorTests` and `Debug|x64` must pass.
