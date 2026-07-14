#include "RoomBlueprint.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{
    std::filesystem::path TestRoot()
    {
        return std::filesystem::temp_directory_path() / "mystic_onslaught_room_blueprint_tests";
    }

    void WriteText(const std::filesystem::path& path, const std::string& text)
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << text;
    }
}

int main()
{
    const std::filesystem::path root = TestRoot();
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root, ec);
    assert(!ec);

    RoomBlueprint source = RoomBlueprint::CreateDefault();
    source.id = "room-test-1";
    source.name = "Narrow Crossing";
    source.biome = Biome::Caverns;
    source.tilesetStem = "Caverns";
    source.roomType = RoomType::Standard;
    source.hasNorth = true;
    source.hasSouth = true;
    source.wallTopDepth = 1.75f;
    source.wallBottomDepth = 0.50f;
    source.wallLeftDepth = 1.25f;
    source.wallRightDepth = 2.00f;
    source.tiles[5][7] = TileType::FloorVariant;
    source.fall[6][8] = true;
    source.solid[4][5] = true;
    source.visualTiles.push_back({ "Forest", TileType::WallBody, false,
        { 16.f, 32.f, 16.f, 16.f }, 5, 4 });
    source.visualTiles.push_back({ "Ground TIles", TileType::FloorVariant, true,
        { 0.f, 0.f, 16.f, 16.f }, 5, 4 });
    source.doorZones[(int)RoomWallSide::Top] = { true, { 11.f, 0.f, 6.f, 1.5f } };
    source.placements.push_back(
        { RoomAssetKind::AnimProp, "water_center_01", 8, 5 });

    std::string error;
    const std::filesystem::path roomPath = root / "narrow_crossing.mroom";
    assert(source.Validate(error));
    assert(source.Save(roomPath, error));
    assert(error.empty());

    std::optional<RoomBlueprint> loaded = RoomBlueprint::Load(roomPath, error);
    assert(loaded.has_value());
    assert(loaded->id == source.id);
    assert(loaded->name == source.name);
    assert(loaded->biome == Biome::Caverns);
    assert(loaded->tilesetStem == "Caverns");
    assert(loaded->roomType == RoomType::Standard);
    assert(loaded->DoorMask() == RoomDoorMask(true, true, false, false));
    assert(loaded->wallTopDepth == 1.75f);
    assert(loaded->wallBottomDepth == 0.50f);
    assert(loaded->wallLeftDepth == 1.25f);
    assert(loaded->wallRightDepth == 2.00f);
    assert(loaded->tiles[5][7] == TileType::FloorVariant);
    assert(loaded->fall[6][8]);
    assert(loaded->solid[4][5]);
    assert(loaded->visualTiles.size() == 2);
    assert(loaded->visualTiles[0].sourceTileset == "Forest");
    assert(!loaded->visualTiles[0].ground);
    assert(loaded->visualTiles[1].ground);
    assert(loaded->doorZones[(int)RoomWallSide::Top].tiles.width ==
           PredeterminedDoorZone(RoomWallSide::Top).width);
    assert(loaded->placements.size() == 1);
    assert(loaded->placements[0].kind == RoomAssetKind::AnimProp);
    assert(loaded->placements[0].assetId == "water_center_01");
    assert(loaded->placements[0].col == 8);
    assert(loaded->placements[0].row == 5);

    TileDefSet definitions{};
    definitions.props.push_back({ { 0.f, 0.f, 16.f, 16.f },
                                  { 1.f, 2.f, 14.f, 12.f },
                                  "rock_01", "Rock" });
    AnimPropDef water;
    water.id = "water_center_01";
    water.name = "Water Centre";
    water.frames.push_back({ 32.f, 0.f, 16.f, 16.f });
    water.collision = { 0.f, 0.f, 16.f, 16.f };
    definitions.animProps.push_back(water);

    RoomBlueprint convertible = *loaded;
    convertible.placements.push_back(
        { RoomAssetKind::Prop, "rock_01", 3, 4 });
    convertible.placements.push_back(
        { RoomAssetKind::Decor, "missing_decor", 10, 10 });
    std::optional<RoomLayout> layout = BuildRoomLayout(convertible, definitions, error);
    assert(layout.has_value());
    assert(layout->handcrafted);
    assert(layout->wallTopDepth == 1.75f);
    assert(layout->wallBottomDepth == 0.50f);
    assert(layout->wallLeftDepth == 1.25f);
    assert(layout->wallRightDepth == 2.00f);
    assert(layout->tiles[5][7] == TileType::FloorVariant);
    assert(layout->fall[6][8]);
    assert(layout->solid[4][5]);
    assert(layout->visualTiles.size() == 2);
    assert(layout->doorZones[(int)RoomWallSide::Top].tiles.height ==
           PredeterminedDoorZone(RoomWallSide::Top).height);
    assert(layout->props.size() == 1);
    assert(layout->props[0].defIdx == 0);
    assert(layout->props[0].col == 3 && layout->props[0].row == 4);
    assert(layout->animProps.size() == 1);
    assert(layout->animProps[0].defIdx == 0);
    assert(layout->decors.empty());

    ApplyActiveRoomDoorMask(*layout, RoomDoorMask(false, true, true, false));
    assert(!layout->doorZones[(int)RoomWallSide::Top].enabled);
    assert(layout->doorZones[(int)RoomWallSide::Bottom].enabled);
    assert(!layout->doorZones[(int)RoomWallSide::Left].enabled);
    assert(layout->doorZones[(int)RoomWallSide::Right].enabled);

    RoomBlueprint noDoor = RoomBlueprint::CreateDefault();
    noDoor.id = "invalid-no-door";
    noDoor.name = "No Door";
    noDoor.tilesetStem = "Caverns";
    assert(!noDoor.Validate(error));
    assert(error.find("door") != std::string::npos);

    const std::filesystem::path truncatedPath = root / "truncated.mroom";
    WriteText(truncatedPath,
        "MROOM 1\n"
        "ID \"broken\"\n"
        "NAME \"Broken\"\n"
        "BIOME 1\n"
        "TILESET \"Caverns\"\n"
        "ROOMTYPE 0\n"
        "DOORS 1 0 0 0\n"
        "TILES_BEGIN\n"
        "0 0 0\n");
    assert(!RoomBlueprint::Load(truncatedPath, error).has_value());
    assert(!error.empty());

    const std::filesystem::path wrongVersionPath = root / "future.mroom";
    WriteText(wrongVersionPath, "MROOM 999\n");
    assert(!RoomBlueprint::Load(wrongVersionPath, error).has_value());
    assert(error.find("version") != std::string::npos);

    // A complete version-1 room (walls/doors as tile art, no SOLID/DOORZONE
    // sections) must migrate: collision derives from the tiles and Door Zones
    // enable from the door flags, so legacy rooms keep loading and working.
    {
        const int doorCol = RoomLayout::kCols / 2;
        std::string v1 =
            "MROOM 1\n"
            "ID \"legacy-1\"\n"
            "NAME \"Legacy Room\"\n"
            "BIOME 1\n"
            "TILESET \"Caverns\"\n"
            "ROOMTYPE 0\n"
            "DOORS 1 0 0 0\n"
            "TILES_BEGIN\n";
        for (int r = 0; r < RoomLayout::kRows; ++r)
        {
            for (int c = 0; c < RoomLayout::kCols; ++c)
            {
                const bool border = r == 0 || c == 0 ||
                    r == RoomLayout::kRows - 1 || c == RoomLayout::kCols - 1;
                int tile = border ? 2 /*WallBody*/ : 0 /*Floor*/;
                if (r == 0 && c == doorCol) tile = 9; /*DoorOpen (north opening)*/
                v1 += std::to_string(tile);
                v1 += (c + 1 < RoomLayout::kCols) ? ' ' : '\n';
            }
        }
        v1 += "TILES_END\nPLACEMENTS_BEGIN\nPLACEMENTS_END\nFALL_BEGIN\n";
        for (int r = 0; r < RoomLayout::kRows; ++r)
            v1 += std::string(RoomLayout::kCols, '0') + "\n";
        v1 += "FALL_END\n";

        const std::filesystem::path v1Path = root / "legacy.mroom";
        WriteText(v1Path, v1);
        std::optional<RoomBlueprint> legacy = RoomBlueprint::Load(v1Path, error);
        assert(legacy.has_value());
        assert(legacy->hasNorth && !legacy->hasSouth);
        assert(legacy->solid[0][0]);                                    // WallBody border → solid
        assert(!legacy->solid[0][doorCol]);                             // DoorOpen cell → not solid
        assert(legacy->doorZones[(int)RoomWallSide::Top].enabled);      // zone enabled from door flag
        assert(!legacy->doorZones[(int)RoomWallSide::Bottom].enabled);
    }

    std::filesystem::remove_all(root, ec);
    return 0;
}
