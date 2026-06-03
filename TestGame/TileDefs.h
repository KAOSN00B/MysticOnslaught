#pragma once
#include "raylib.h"

// ── TileType ──────────────────────────────────────────────────────────────────
// Matches TileMapper::kTypeNames order exactly.
// Do NOT reorder — tilemapper save files store these as integer indices.
enum class TileType : int
{
    Floor = 0,
    FloorVariant,
    WallBody,
    WallTopFace,
    WallCornerTL,
    WallCornerTR,
    WallInnerCornerL,
    WallInnerCornerR,
    Void,
    DoorOpen,
    DoorLocked,
    BossKey,
    ChestClosed,
    ChestOpen,
    WallLeft,
    WallRight,
    WallBottom,
    WallCornerBL,
    WallCornerBR,
    WallInnerCornerBL,
    WallInnerCornerBR,
    Count,
    None = -1,
};

// ── TileDefSet ────────────────────────────────────────────────────────────────
// One source rectangle per tile type, loaded from a tilemapper save file.
// Unassigned types return a fallback rect so the renderer never crashes.
struct TileDefSet
{
    Rectangle rects[(int)TileType::Count]{};
    bool      assigned[(int)TileType::Count]{};

    // Returns the source rect for a tile type.
    // Falls back to Floor if the type wasn't assigned in the save file.
    Rectangle Get(TileType t) const;

    // Load assignments from a tilemapper_<stem>.txt save file.
    // Returns true if the file was found and read.
    bool LoadFromFile(const char* path);

    bool IsAssigned(TileType t) const
    {
        int i = (int)t;
        return i >= 0 && i < (int)TileType::Count && assigned[i];
    }
};
