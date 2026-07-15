#pragma once

#include "RoomBlueprint.h"
#include "RoomAssetCatalog.h"

#include <filesystem>
#include <array>
#include <string>
#include <string_view>
#include <vector>

struct RoomRequest
{
    Biome biome = Biome::Caverns;
    std::string tilesetStem;
    RoomType roomType = RoomType::Standard;
    unsigned char doorMask = 0;
};

class RoomLibrary
{
public:
    void Refresh(const std::filesystem::path& root);
    const std::vector<RoomBlueprint>& Rooms() const { return _rooms; }
    const RoomBlueprint* FindById(std::string_view id) const;
    const RoomBlueprint* Choose(const RoomRequest& request,
                                std::string_view avoidId = {}) const;
    // Editor playtests preserve gameplay identity as well as geometry. Prefer an
    // exact door mask; a four-door blueprint of the same biome and room type is
    // the only fallback and has its unused exits sealed by the runtime graph.
    std::vector<const RoomBlueprint*> PlaytestCandidates(
        Biome biome, RoomType roomType, unsigned char requiredDoorMask) const;
    std::array<int, 16> DoorMaskCounts(Biome biome,
                                       std::string_view tilesetStem) const;
    std::optional<RoomLayout> Resolve(const RoomRequest& request,
                                      const TileDefSet& definitions,
                                      const RoomAssetCatalog* catalog,
                                      std::string_view avoidId,
                                      std::string& selectedId,
                                      std::string& warning) const;

    bool SaveRoom(const RoomBlueprint& room, bool overwrite, std::string& error);
    bool DeleteRoom(std::string_view id, std::string& error);
    bool NameExists(std::string_view name, std::string_view exceptId = {}) const;

    static std::string Slugify(std::string_view name);
    static std::string BiomeFolderName(Biome biome);

private:
    std::filesystem::path PathFor(const RoomBlueprint& room) const;
    bool IsInsideRoot(const std::filesystem::path& path) const;

    std::filesystem::path _root;
    std::vector<RoomBlueprint> _rooms;
    std::vector<std::filesystem::path> _paths;
};
