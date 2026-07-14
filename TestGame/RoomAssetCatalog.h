#pragma once

#include "TileDefs.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

struct RoomAssetSource
{
    std::string stem;
    std::filesystem::path imagePath;
    std::filesystem::path metadataPath;
    TileDefSet definitions;
    int tileColumns = 0;
    int tileRows = 0;
};

// Discovers the sheets available to the room editor without owning textures.
// Keeping file discovery separate lets editor and runtime resolve identical IDs.
class RoomAssetCatalog
{
public:
    bool Refresh(const std::filesystem::path& tilesetRoot,
                 const std::filesystem::path& metadataRoot = {});

    const std::vector<RoomAssetSource>& Sources() const { return _sources; }
    const RoomAssetSource* Find(std::string_view stem) const;
    const std::vector<std::string>& Warnings() const { return _warnings; }

private:
    std::vector<RoomAssetSource> _sources;
    std::vector<std::string> _warnings;
};
