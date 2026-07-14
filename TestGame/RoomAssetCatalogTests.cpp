#include "RoomAssetCatalog.h"

#include <cassert>
#include <filesystem>
#include <fstream>

int main()
{
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "mystic_room_asset_catalog";
    fs::remove_all(root);
    fs::create_directories(root / "tiles");
    fs::create_directories(root / "meta");

    unsigned char pngHeader[24] =
        { 137,80,78,71,13,10,26,10, 0,0,0,13, 'I','H','D','R',
          0,0,0,32, 0,0,0,16 };
    std::ofstream png(root / "tiles" / "Forest.png", std::ios::binary);
    png.write((const char*)pngHeader, sizeof(pngHeader));
    png.close();
    std::ofstream metadata(root / "meta" / "tilemapper_Forest.txt");
    metadata << "BIOME Forest\n";
    metadata.close();

    RoomAssetCatalog catalog;
    assert(catalog.Refresh(root / "tiles", root / "meta"));
    const RoomAssetSource* source = catalog.Find("Forest");
    assert(source != nullptr);
    assert(source->tileColumns == 2);
    assert(source->tileRows == 1);
    assert(!source->metadataPath.empty());
    assert(catalog.Find("Missing") == nullptr);

    fs::remove_all(root);
    return 0;
}
