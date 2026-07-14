#include "RoomBlueprint.h"
#include "TileDefs.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{
    void WriteText(const std::filesystem::path& path, const std::string& text)
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << text;
    }
}

int main()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "mystic_onslaught_tile_asset_tests";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root, ec);
    assert(!ec);

    const std::filesystem::path legacyPath = root / "legacy.txt";
    WriteText(legacyPath,
        "BIOME Caverns\n"
        "PROP 0 0 32 32 2 3 28 26\n"
        "ANIMPROP 0 0 16 16 8 2 32 0 16 16 48 0 16 16\n"
        "DECOR 64 0 16 16\n"
        "ANIMDECOR 6 2 80 0 16 16 96 0 16 16\n");

    TileDefSet legacy{};
    assert(legacy.LoadFromFile(legacyPath.string().c_str()));
    assert(legacy.props.size() == 1);
    assert(legacy.animProps.size() == 1);
    assert(legacy.decors.size() == 1);
    assert(legacy.animDecors.size() == 1);
    assert(legacy.props[0].id == "prop_0");
    assert(legacy.props[0].name == "Prop 1");
    assert(legacy.animProps[0].id == "anim_prop_0");
    assert(legacy.animProps[0].playback == AnimPlaybackMode::Loop);
    assert(legacy.FindAssetIndex(RoomAssetKind::Prop, "prop_0") == 0);
    assert(legacy.FindAssetIndex(RoomAssetKind::AnimProp, "missing") == -1);

    const std::filesystem::path extendedPath = root / "extended.txt";
    WriteText(extendedPath,
        "BIOME Caverns\n"
        "PROPV2 \"rock-pillar-id\" \"Rock Pillar\" 0 0 32 48 3 12 26 34\n"
        "ANIMPROPV2 \"water-center-id\" \"Water Centre\" 1 10 0 0 16 16 3 "
        "32 0 16 16 48 0 16 16 64 0 16 16\n"
        "DECORV2 \"bones-id\" \"Small Bones\" 80 0 16 16\n"
        "ANIMDECORV2 \"torch-id\" \"Wall Torch\" 2 7 2 "
        "96 0 16 16 112 0 16 16\n");

    TileDefSet extended{};
    assert(extended.LoadFromFile(extendedPath.string().c_str()));
    assert(extended.props[0].id == "rock-pillar-id");
    assert(extended.props[0].name == "Rock Pillar");
    assert(extended.animProps[0].id == "water-center-id");
    assert(extended.animProps[0].name == "Water Centre");
    assert(extended.animProps[0].playback == AnimPlaybackMode::PingPong);
    assert(extended.animProps[0].frames.size() == 3);
    assert(extended.animDecors[0].playback == AnimPlaybackMode::PlayOnce);
    assert(extended.FindAssetIndex(RoomAssetKind::AnimDecor, "torch-id") == 0);
    assert(extended.props[0].sourceSheet.empty());
    assert(extended.animProps[0].sourceSheet.empty());

    const std::filesystem::path sharedPath = root / "shared-sheets.txt";
    WriteText(sharedPath,
        "BIOME Forest\n"
        "PROPV3 \"water-edge\" \"Water Edge\" \"FD_Animated_Water\" 0 0 16 16 0 0 16 16\n"
        "ANIMPROPV3 \"lava-flow\" \"Lava Flow\" \"RA_Hell_Animations\" 0 8 0 0 16 16 2 "
        "0 0 16 16 16 0 16 16\n"
        "ANIMDECORV3 \"water-ripple\" \"Water Ripple\" \"FD_Animated_Water\" 1 6 2 "
        "0 16 16 16 16 16 16 16\n");
    TileDefSet shared{};
    assert(shared.LoadFromFile(sharedPath.string().c_str()));
    assert(shared.props.size() == 1);
    assert(shared.props[0].sourceSheet == "FD_Animated_Water");
    assert(shared.animProps.size() == 1);
    assert(shared.animProps[0].sourceSheet == "RA_Hell_Animations");
    assert(shared.animDecors.size() == 1);
    assert(shared.animDecors[0].sourceSheet == "FD_Animated_Water");

    std::filesystem::remove_all(root, ec);
    return 0;
}
