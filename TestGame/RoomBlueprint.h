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
};

unsigned char RoomDoorMask(bool north, bool south, bool east, bool west);

struct RoomBlueprint
{
    static constexpr int kVersion = 1;
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
    TileType tiles[RoomLayout::kRows][RoomLayout::kCols]{};
    bool fall[RoomLayout::kRows][RoomLayout::kCols]{};
    std::vector<RoomAssetPlacement> placements;

    static RoomBlueprint CreateDefault();
    unsigned char DoorMask() const;
    bool Validate(std::string& error) const;
    bool Save(const std::filesystem::path& path, std::string& error) const;
    static std::optional<RoomBlueprint> Load(const std::filesystem::path& path,
                                              std::string& error);
};

std::optional<RoomLayout> BuildRoomLayout(const RoomBlueprint& blueprint,
                                          const TileDefSet& definitions,
                                          std::string& warning);
