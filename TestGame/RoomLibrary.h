#pragma once

#include "RoomBlueprint.h"

#include <filesystem>
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
    const RoomBlueprint* Choose(const RoomRequest& request,
                                std::string_view avoidId = {}) const;

    bool SaveRoom(const RoomBlueprint& room, bool overwrite, std::string& error);
    bool DeleteRoom(std::string_view id, std::string& error);
    bool NameExists(std::string_view name, std::string_view exceptId = {}) const;

    static std::string Slugify(std::string_view name);

private:
    static std::string BiomeFolderName(Biome biome);
    std::filesystem::path PathFor(const RoomBlueprint& room) const;
    bool IsInsideRoot(const std::filesystem::path& path) const;

    std::filesystem::path _root;
    std::vector<RoomBlueprint> _rooms;
    std::vector<std::filesystem::path> _paths;
};
