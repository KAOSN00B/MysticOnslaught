#pragma once
#include "raylib.h"
#include <vector>

// A single sprite entry in the props or decorations array.
// collision: rect in source pixels relative to sprite top-left. Default = full sprite.
struct SpriteDef {
    Rectangle src;
    Rectangle collision;
};

// An animated sprite (e.g. torch/fire). Frames are laid out horizontally in the sheet.
struct AnimSpriteDef {
    Rectangle firstFrame;  // source rect of frame 0; subsequent frames at +firstFrame.width each
    int       frameCount = 1;
    float     fps        = 8.f;
};

// An animated prop — each frame is a separate source rectangle you selected in the TileMapper.
// Frames do not need to be equal-sized or laid out in a row.
struct AnimPropDef {
    std::vector<Rectangle> frames;    // one entry per frame, in playback order
    Rectangle              collision; // hitbox in source-pixel space of frame[0]
    float                  fps = 8.f;
};

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
    bool      fromGround[(int)TileType::Count]{};  // true = rect is from Ground TIles.png, not the biome sheet

    // Returns the source rect for a tile type.
    // Falls back to Floor if the type wasn't assigned in the save file.
    Rectangle Get(TileType t) const;

    std::vector<SpriteDef>     props;       // collision obstacles placed in rooms
    std::vector<AnimPropDef>   animProps;   // animated collision obstacles placed in rooms
    std::vector<SpriteDef>     decors;      // static floor decorations, no collision
    std::vector<AnimSpriteDef> animDecors;  // animated floor decorations (torches, fire, etc.)

    // Load assignments from a tilemapper_<stem>.txt save file.
    // Returns true if the file was found and read.
    bool LoadFromFile(const char* path);

    bool IsAssigned(TileType t) const
    {
        int i = (int)t;
        return i >= 0 && i < (int)TileType::Count && assigned[i];
    }
};
