#pragma once
#include "TileDefs.h"

// ── RoomLayout ────────────────────────────────────────────────────────────────
// A 2D grid of TileType values representing one room.
// Generated automatically from which sides have doors (N/S/E/W).
//
// Room dimensions at 3x draw scale (16px source → 48px on screen):
//   28 tiles wide × 16 tiles tall = 1344 × 768 px
// ─────────────────────────────────────────────────────────────────────────────
struct RoomLayout
{
    static constexpr int kCols = 28;
    static constexpr int kRows = 16;

    TileType tiles[kRows][kCols]{};

    // Auto-generate a room with walls around the border and floor inside.
    // Door openings are 3 tiles wide (N/S walls) or 2 tiles tall (E/W walls).
    static RoomLayout Generate(bool hasNorth, bool hasSouth,
                               bool hasEast,  bool hasWest);
};
