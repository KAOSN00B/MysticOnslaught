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
    source.treasureChestCol = 10;
    source.treasureChestRow = 9;
    source.hasNorth = true;
    source.hasSouth = true;
    source.fallSurface = FallSurface::Water;
    source.wallTopDepth = 1.75f;
    source.wallBottomDepth = 0.50f;
    source.wallLeftDepth = 1.25f;
    source.wallRightDepth = 2.00f;
    source.tiles[5][7] = TileType::FloorVariant;
    source.fall[6][8] = true;
    source.fallRects.push_back({ 8.25f, 6.50f, 2.50f, 1.25f });
    source.solid[4][5] = true;
    source.visualTiles.push_back({ "Forest", TileType::WallBody, false,
        { 16.f, 32.f, 16.f, 16.f }, 5, 4 });
    source.visualTiles.back().anchorOffset = { 0.f, 2.f };
    source.visualTiles.push_back({ "Ground TIles", TileType::FloorVariant, true,
        { 0.f, 0.f, 16.f, 16.f }, 5, 4 });
    RoomTilePlacement northDoor{ "Forest", TileType::WallBody, false,
        { 32.f, 32.f, 16.f, 16.f }, 13, 0 };
    northDoor.door = true;
    source.visualTiles.push_back(northDoor);
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
    assert(loaded->treasureChestCol == 10);
    assert(loaded->treasureChestRow == 9);
    assert(loaded->fallSurface == FallSurface::Water);
    assert(loaded->DoorMask() == RoomDoorMask(true, true, false, false));
    assert(loaded->wallTopDepth == 1.75f);
    assert(loaded->wallBottomDepth == 0.50f);
    assert(loaded->wallLeftDepth == 1.25f);
    assert(loaded->wallRightDepth == 2.00f);
    assert(loaded->tiles[5][7] == TileType::FloorVariant);
    assert(loaded->fall[6][8]);
    assert(loaded->fallRects.size() == 1);
    assert(loaded->fallRects[0].x == 8.25f);
    assert(loaded->fallRects[0].width == 2.50f);
    assert(loaded->solid[4][5]);
    assert(loaded->visualTiles.size() == 3);
    assert(loaded->visualTiles[0].sourceTileset == "Forest");
    assert(!loaded->visualTiles[0].ground);
    assert(loaded->visualTiles[0].anchorOffset.x == 0.f);
    assert(loaded->visualTiles[0].anchorOffset.y == 2.f);
    assert(loaded->visualTiles[1].ground);
    assert(loaded->visualTiles[2].door);
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
    assert(layout->treasureChestCol == 10);
    assert(layout->treasureChestRow == 9);
    assert(layout->fallSurface == FallSurface::Water);
    assert(layout->wallTopDepth == 1.75f);
    assert(layout->wallBottomDepth == 0.50f);
    assert(layout->wallLeftDepth == 1.25f);
    assert(layout->wallRightDepth == 2.00f);
    assert(layout->tiles[5][7] == TileType::FloorVariant);
    assert(layout->fall[6][8]);
    assert(layout->fallRects.size() == 1);
    assert(layout->fallRects[0].height == 1.25f);
    assert(layout->solid[4][5]);
    assert(layout->visualTiles.size() == 3);
    assert(layout->visualTiles[0].anchorOffset.y == 2.f);
    assert(layout->visualTiles[2].door);
    assert(layout->doorZones[(int)RoomWallSide::Top].tiles.height ==
           PredeterminedDoorZone(RoomWallSide::Top).height);
    assert(layout->props.size() == 1);
    assert(layout->props[0].defIdx == 0);
    assert(layout->props[0].col == 3 && layout->props[0].row == 4);
    assert(layout->animProps.size() == 1);
    assert(layout->animProps[0].defIdx == 0);
    assert(layout->decors.empty());

    // A source rectangle extended two pixels above its logical tile remains
    // bottom-aligned with that tile in both editor and runtime coordinates.
    const Rectangle anchoredDestination = RoomTileDestination(
        layout->visualTiles[0], 100.f, 200.f, 3.f, 3.f);
    assert(anchoredDestination.x == 100.f);
    assert(anchoredDestination.y == 194.f);
    assert(anchoredDestination.width == 48.f);
    assert(anchoredDestination.height == 48.f);

    Rectangle selection{ 16.f, 32.f, 16.f, 16.f };
    Vector2 anchor{};
    assert(AdjustTileSelectionOverhang(
        selection, anchor, RoomWallSide::Top, 2, 512.f, 640.f));
    assert(selection.x == 16.f && selection.y == 30.f);
    assert(selection.width == 16.f && selection.height == 18.f);
    assert(anchor.x == 0.f && anchor.y == 2.f);
    RoomTilePlacement extended{ "Forest", TileType::WallBottom, false,
        selection, 7, 8 };
    extended.anchorOffset = anchor;
    const Rectangle extendedDestination = RoomTileDestination(
        extended, 350.f, 400.f, 2.f, 2.f);
    assert(extendedDestination.x == 350.f);
    assert(extendedDestination.y == 396.f);
    assert(extendedDestination.width == 32.f);
    assert(extendedDestination.height == 36.f);
    assert(AdjustTileSelectionOverhang(
        selection, anchor, RoomWallSide::Top, -1, 512.f, 640.f));
    assert(selection.y == 31.f && selection.height == 17.f && anchor.y == 1.f);

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

    RoomBlueprint invalidChest = source;
    invalidChest.treasureChestCol = RoomLayout::kCols;
    invalidChest.treasureChestRow = 5;
    assert(!invalidChest.Validate(error));
    assert(error.find("chest") != std::string::npos);

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

    const std::filesystem::path badSurfacePath = root / "bad_surface.mroom";
    {
        std::ifstream saved(roomPath, std::ios::binary);
        std::string badSurface((std::istreambuf_iterator<char>(saved)),
                               std::istreambuf_iterator<char>());
        const std::size_t surface = badSurface.find("FALLSURFACE 1");
        assert(surface != std::string::npos);
        badSurface.replace(surface, 13, "FALLSURFACE 9");
        WriteText(badSurfacePath, badSurface);
    }
    assert(!RoomBlueprint::Load(badSurfacePath, error).has_value());
    assert(error.find("fall surface") != std::string::npos);

    // Version-2 rooms had no explicit Door layer. Non-ground art overlapping an
    // enabled fixed door zone migrates to Door art so existing rooms keep opening.
    {
        const std::filesystem::path legacyV2Path = root / "legacy_v2.mroom";
        std::ifstream saved(roomPath, std::ios::binary);
        std::string v2((std::istreambuf_iterator<char>(saved)),
                       std::istreambuf_iterator<char>());
        const std::size_t header = v2.find("MROOM 7");
        assert(header != std::string::npos);
        v2.replace(header, 7, "MROOM 2");
        const std::size_t surfaceTag = v2.find("FALLSURFACE ");
        assert(surfaceTag != std::string::npos);
        v2.erase(surfaceTag, v2.find('\n', surfaceTag) - surfaceTag + 1);
        const std::size_t doorTag = v2.find("DOOR \"Forest\"");
        assert(doorTag != std::string::npos);
        v2.replace(doorTag, 4, "VISUAL");
        WriteText(legacyV2Path, v2);

        std::optional<RoomBlueprint> migrated = RoomBlueprint::Load(legacyV2Path, error);
        assert(migrated.has_value());
        assert(migrated->fallSurface == FallSurface::Void);
        assert(migrated->visualTiles[2].door);
        assert(!migrated->visualTiles[0].door);
    }

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
        assert(legacy->fallSurface == FallSurface::Void);
        assert(legacy->treasureChestCol == -1);
        assert(legacy->treasureChestRow == -1);
        assert(legacy->hasNorth && !legacy->hasSouth);
        assert(legacy->solid[0][0]);                                    // WallBody border → solid
        assert(!legacy->solid[0][doorCol]);                             // DoorOpen cell → not solid
        assert(legacy->doorZones[(int)RoomWallSide::Top].enabled);      // zone enabled from door flag
        assert(!legacy->doorZones[(int)RoomWallSide::Bottom].enabled);
    }

    std::filesystem::remove_all(root, ec);
    return 0;
}
