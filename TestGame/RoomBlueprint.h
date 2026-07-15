#pragma once

#include "GameTypes.h"
#include "RoomLayout.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

enum class RoomAssetKind : unsigned char
{
    Prop,
    AnimProp,
    Decor,
    AnimDecor,
};

struct RoomAssetPlacement
{
    RoomAssetKind kind = RoomAssetKind::Prop;
    std::string assetId;
    int col = 0;
    int row = 0;
    std::string sourceTileset; // empty = room.tilesetStem (version 1 compatibility)
    // Decor/AnimDecor only: which layer band it draws in. Lets a Decor asset
    // (e.g. animated water/lava) be painted as Ground or Visual. Ignored for props.
    RoomDrawBand band = RoomDrawBand::Decor;
};

enum class RoomWallSide : unsigned char
{
    Top,
    Bottom,
    Left,
    Right,
};

unsigned char RoomDoorMask(bool north, bool south, bool east, bool west);

// Expands or contracts an authored tile selection while keeping its original
// 16x16 cell as the placement anchor. Positive delta includes source pixels;
// negative delta removes previously included overhang pixels.
bool AdjustTileSelectionOverhang(Rectangle& source, Vector2& anchorOffset,
                                 RoomWallSide side, int delta,
                                 float sheetWidth, float sheetHeight);

// The fixed tile-space rectangle for a door on the given side. Predetermined and
// identical for every room so handcrafted door openings always line up with the
// procedural dungeon door lanes (no free placement, no misalignment).
Rectangle PredeterminedDoorZone(RoomWallSide side);
void ApplyActiveRoomDoorMask(RoomLayout& layout, unsigned char doorMask);

struct RoomBlueprint
{
    static constexpr int kVersion = 7;
    static constexpr std::size_t kMaxPlacements = 4096;

    std::string id;
    std::string name;
    std::string tilesetStem;
    Biome biome = Biome::Caverns;
    RoomType roomType = RoomType::Standard;
    bool hasNorth = false;
    bool hasSouth = false;
    bool hasEast = false;
    bool hasWest = false;
    // Collision depth measured inward from each room edge in tile units.
    // Keeping this independent from render scale makes editor and runtime agree.
    float wallTopDepth = 1.0f;
    float wallBottomDepth = 1.0f;
    float wallLeftDepth = 1.0f;
    float wallRightDepth = 1.0f;
    FallSurface fallSurface = FallSurface::Void;
    int treasureChestCol = -1;
    int treasureChestRow = -1;
    TileType tiles[RoomLayout::kRows][RoomLayout::kCols]{};
    bool fall[RoomLayout::kRows][RoomLayout::kCols]{};
    bool solid[RoomLayout::kRows][RoomLayout::kCols]{};
    RoomDoorZone doorZones[4]{};
    // Free-size collision rectangles in TILE space (may be smaller than a tile).
    // These sit alongside the full-tile `solid[][]` grid — brush paints the grid,
    // the Rectangle tool draws these adjustable boxes (fences, thin walls, etc.).
    std::vector<Rectangle> colliders;
    std::vector<Rectangle> fallRects; // precise fall triggers in tile space
    std::vector<RoomTilePlacement> visualTiles;
    std::vector<RoomAssetPlacement> placements;

    static RoomBlueprint CreateDefault();
    bool HasTreasureChestSpawn() const
    {
        return treasureChestCol >= 0 && treasureChestRow >= 0;
    }
    unsigned char DoorMask() const;
    bool Validate(std::string& error) const;
    bool Save(const std::filesystem::path& path, std::string& error) const;
    static std::optional<RoomBlueprint> Load(const std::filesystem::path& path,
                                              std::string& error);
};

class RoomAssetCatalog;

std::optional<RoomLayout> BuildRoomLayout(const RoomBlueprint& blueprint,
                                          const TileDefSet& definitions,
                                          std::string& warning,
                                          const RoomAssetCatalog* catalog = nullptr);
