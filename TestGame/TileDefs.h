#pragma once
#include "raylib.h"
#include <string>
#include <string_view>
#include <vector>

enum class RoomAssetKind : unsigned char;

enum class AnimPlaybackMode : unsigned char
{
    Loop = 0,
    PingPong = 1,
    PlayOnce = 2,
};

// A single sprite entry in the props or decorations array.
// collision: rect in source pixels relative to sprite top-left. Default = full sprite.
struct SpriteDef {
    Rectangle src;
    Rectangle collision;
    std::string id;
    std::string name;
    // Empty means the owning biome sheet; otherwise this is a MapTilesets stem.
    std::string sourceSheet;
};

// An animated sprite (e.g. torch/fire). Each frame is a separate source rectangle.
struct AnimSpriteDef {
    std::vector<Rectangle> frames;  // one entry per frame, in playback order
    float                  fps = 8.f;
    std::string            id;
    std::string            name;
    AnimPlaybackMode       playback = AnimPlaybackMode::Loop;
    std::string            sourceSheet;
};

// An animated prop — each frame is a separate source rectangle you selected in the TileMapper.
// Frames do not need to be equal-sized or laid out in a row.
struct AnimPropDef {
    std::vector<Rectangle> frames;    // one entry per frame, in playback order
    Rectangle              collision; // hitbox in source-pixel space of frame[0]
    float                  fps = 8.f;
    std::string            id;
    std::string            name;
    AnimPlaybackMode       playback = AnimPlaybackMode::Loop;
    std::string            sourceSheet;
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

    // Resolve a room's stable authored asset id once when building RoomLayout.
    // Returns -1 when the referenced definition no longer exists.
    int FindAssetIndex(RoomAssetKind kind, std::string_view id) const;

    bool IsAssigned(TileType t) const
    {
        int i = (int)t;
        return i >= 0 && i < (int)TileType::Count && assigned[i];
    }
};

// Identifies which texture owns a resolved tile source rectangle. Most tiles
// come from the active biome (or Ground Tiles); treasure chests may fall back
// to the shared Caverns reward art when a biome has no authored chest tile.
enum class TileRenderSheet
{
    Biome,
    Ground,
    SharedReward,
};

struct TileRenderSource
{
    Rectangle src{};
    TileRenderSheet sheet = TileRenderSheet::Biome;
};

TileRenderSource ResolveTileRenderSource(const TileDefSet& defs, TileType type);
