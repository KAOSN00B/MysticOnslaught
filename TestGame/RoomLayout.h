#pragma once
#include "TileDefs.h"
#include "GameTypes.h"
#include <string>
#include <vector>

// A placed prop or decoration instance inside a room.
struct SpritePlacement
{
    int defIdx;   // index into TileDefSet::props or TileDefSet::decors
    int col, row; // tile-grid position within the room
    std::string sourceTileset; // empty = active biome definitions
};

struct RoomTilePlacement
{
    std::string sourceTileset;
    TileType type = TileType::Floor;
    bool ground = false;
    Rectangle src{};
    int col = 0;
    int row = 0;
};

struct RoomSourceDefinitions
{
    std::string stem;
    TileDefSet definitions;
};

struct RoomDoorZone
{
    bool enabled = false;
    Rectangle tiles{}; // tile-space rectangle, independent from draw scale
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

    int visualVariant = 0; // index into the active biome's editable visual palette
    bool handcrafted = false;
    std::string sourceRoomId;
    float wallTopDepth = 1.0f;
    float wallBottomDepth = 1.0f;
    float wallLeftDepth = 1.0f;
    float wallRightDepth = 1.0f;

    TileType tiles[kRows][kCols]{};
    bool fall[kRows][kCols]{};
    bool solid[kRows][kCols]{};
    RoomDoorZone doorZones[4]{}; // top, bottom, left, right
    bool doorZoneOpen[4]{};
    bool roomCleared = false;
    std::vector<RoomTilePlacement> visualTiles;
    std::vector<RoomSourceDefinitions> assetSources;
    std::vector<SpritePlacement> props;       // solid objects with collision
    std::vector<SpritePlacement> animProps;   // animated solid objects with collision
    std::vector<SpritePlacement> decors;      // static floor decorations, no collision
    std::vector<SpritePlacement> animDecors;  // animated decorations (torches/fire), no collision

    // Auto-generate a room with walls around the border and floor inside.
    // Door openings are 3 tiles wide (N/S walls) or 2 tiles tall (E/W walls).
    // defs supplies the biome's prop/decor definitions — placement reads each
    // prop's REAL sprite + collision size so footprints never overlap, always
    // keep a walkable clearance gap (no more wedged enemies), and stay clear
    // of the door lanes. nullptr places nothing.
    static RoomLayout Generate(bool hasNorth, bool hasSouth,
                               bool hasEast,  bool hasWest,
                               RoomType type              = RoomType::Standard,
                               const TileDefSet* defs     = nullptr,
                               int      propDensityBonus  = 0);
};

inline const TileDefSet* ResolveRoomDefinitions(const RoomLayout& room,
                                                const SpritePlacement& placement,
                                                const TileDefSet& fallback)
{
    if (placement.sourceTileset.empty()) return &fallback;
    for (const RoomSourceDefinitions& source : room.assetSources)
        if (source.stem == placement.sourceTileset) return &source.definitions;
    return nullptr;
}
