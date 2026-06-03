#pragma once
#include "TileDefs.h"
#include "GameTypes.h"
#include <vector>

// A placed prop or decoration instance inside a room.
struct SpritePlacement
{
    int defIdx;   // index into TileDefSet::props or TileDefSet::decors
    int col, row; // tile-grid position within the room
};

// ── RoomLayout ────────────────────────────────────────────────────────────────
// A 2D grid of TileType values representing one room, plus lists of prop and
// decoration instances placed in that room.
//
// Room dimensions at 3x draw scale (16px source → 48px on screen):
//   28 tiles wide × 16 tiles tall = 1344 × 768 px
// ─────────────────────────────────────────────────────────────────────────────
struct RoomLayout
{
    static constexpr int kCols = 28;
    static constexpr int kRows = 16;

    TileType tiles[kRows][kCols]{};
    std::vector<SpritePlacement> props;    // solid objects with collision
    std::vector<SpritePlacement> decors;   // floor decorations, no collision

    // Auto-generate a room with walls around the border and floor inside.
    // Door openings are 3 tiles wide (N/S walls) or 2 tiles tall (E/W walls).
    // propCount / decorCount: how many definitions are available to pick from.
    static RoomLayout Generate(bool hasNorth, bool hasSouth,
                               bool hasEast,  bool hasWest,
                               RoomType type      = RoomType::Standard,
                               int      propCount  = 0,
                               int      decorCount = 0);
};
