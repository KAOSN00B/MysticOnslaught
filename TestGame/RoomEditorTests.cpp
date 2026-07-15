#include "RoomEditor.h"

#include <cassert>
#include <filesystem>
#include <fstream>

int main()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "mystic_onslaught_room_editor_tests";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);

    TileDefSet defs{};
    defs.assigned[(int)TileType::Floor] = true;
    defs.assigned[(int)TileType::FloorVariant] = true;
    defs.props.push_back({ { 0.f, 0.f, 16.f, 16.f },
                           { 0.f, 0.f, 16.f, 16.f }, "rock", "Rock" });
    defs.decors.push_back({ { 16.f, 0.f, 16.f, 16.f }, {}, "flower", "Flower" });

    RoomEditor editor;
    editor.BindForTesting("Caverns", Biome::Caverns, defs, root);
    editor.SetDoors(true, true, false, false);
    assert(editor.SetWallDepth(RoomWallSide::Top, 1.75f));
    assert(editor.Blueprint().wallTopDepth == 1.75f);
    assert(editor.Blueprint().wallBottomDepth == 1.00f);
    assert(editor.Undo());
    assert(editor.Blueprint().wallTopDepth == 1.00f);
    assert(editor.Redo());
    assert(editor.Blueprint().wallTopDepth == 1.75f);
    assert(editor.SetTerrain(4, 5, TileType::FloorVariant));
    assert(editor.SetVisual(5, 5, false, "Forest", { 16.f, 32.f, 16.f, 16.f }));
    assert(!editor.SetVisual(5, 5, false, "Forest", { 16.f, 32.f, 16.f, 16.f }));
    assert(editor.SetSolid(5, 5, true));
    assert(editor.SetFall(8, 6, true));
    assert(editor.Blueprint().fallSurface == FallSurface::Void);
    assert(editor.SetFallSurface(FallSurface::Water));
    assert(editor.Blueprint().fallSurface == FallSurface::Water);
    assert(!editor.SetFallSurface(FallSurface::Water));
    assert(editor.SetFallSurface(FallSurface::Lava));
    assert(editor.Blueprint().fallSurface == FallSurface::Lava);
    assert(editor.Undo());
    assert(editor.Blueprint().fallSurface == FallSurface::Water);
    assert(editor.Redo());
    assert(editor.Blueprint().fallSurface == FallSurface::Lava);
    assert(editor.PlaceAsset({ RoomAssetKind::Prop, "rock", 3, 4 }));
    assert(editor.Blueprint().tiles[5][4] == TileType::FloorVariant);
    assert(editor.Blueprint().fall[6][8]);
    assert(editor.Blueprint().solid[5][5]);
    assert(editor.Blueprint().visualTiles.size() == 1);
    assert(editor.Blueprint().placements.size() == 1);
    editor.Undo();
    assert(editor.Blueprint().placements.empty());
    editor.Redo();
    assert(editor.Blueprint().placements.size() == 1);

    // Treasure rooms author one chest marker. It participates in undo/redo and
    // rejects authored blockers instead of silently spawning inside terrain.
    editor.Blueprint().roomType = RoomType::Treasure;
    assert(editor.TreasureChestSpawnFits(10, 10));
    assert(editor.SetTreasureChestSpawn(10, 10));
    assert(editor.Blueprint().treasureChestCol == 10);
    assert(editor.Blueprint().treasureChestRow == 10);
    assert(!editor.SetTreasureChestSpawn(10, 10));
    assert(editor.Undo());
    assert(editor.Blueprint().treasureChestCol == -1);
    assert(editor.Redo());
    assert(editor.Blueprint().treasureChestCol == 10);
    assert(!editor.TreasureChestSpawnFits(5, 5));  // painted solid cell
    assert(!editor.TreasureChestSpawnFits(8, 6));  // painted fall cell
    assert(!editor.TreasureChestSpawnFits(3, 4));  // blocking rock prop
    assert(!editor.PlaceAsset({ RoomAssetKind::Prop, "rock", 10, 10 }));
    assert(editor.ClearTreasureChestSpawn());
    assert(editor.Blueprint().treasureChestCol == -1);
    editor.Blueprint().roomType = RoomType::Standard;

    // Ground random brushes choose authored variants once and store the result as
    // an ordinary visual tile. Other editor layers and the runtime room format do
    // not need to understand random brushes.
    editor.SelectLayer(RoomEditor::Layer::Ground);
    assert(editor.AddGroundBrushTile("Caverns", { 0.f, 0.f, 16.f, 16.f }, 8));
    assert(!editor.RandomGroundBrushEnabled());
    assert(editor.AddGroundBrushTile("Forest", { 16.f, 32.f, 16.f, 16.f }, 2));
    // Once a mix has two members it must become active immediately. Otherwise
    // PaintCell falls back to _selectedRawTile and the last selected tile appears
    // to ignore all weights.
    assert(editor.RandomGroundBrushEnabled());
    assert(!editor.AddGroundBrushTile("Forest", { 16.f, 32.f, 16.f, 16.f }, 2));
    assert(editor.GroundBrushTiles().size() == 2);
    assert(editor.SetGroundBrushWeight(1, 3));
    assert(editor.GroundBrushTiles()[1].weight == 3);
    assert(editor.ChooseGroundBrushIndex(0) == 0);
    assert(editor.ChooseGroundBrushIndex(7) == 0);
    assert(editor.ChooseGroundBrushIndex(8) == 1);
    assert(editor.ChooseGroundBrushIndex(10) == 1);
    assert(editor.PaintActiveCell(10, 10));
    bool storedRandomGround = false;
    for (const RoomTilePlacement& visual : editor.Blueprint().visualTiles)
        if (visual.col == 10 && visual.row == 10 && visual.ground)
            storedRandomGround = visual.sourceTileset == "Caverns" ||
                                 visual.sourceTileset == "Forest";
    assert(storedRandomGround);

    std::string presetError;
    assert(editor.SaveGroundBrushPreset("Mossy Floor", presetError));
    editor.ClearGroundBrush();
    assert(editor.GroundBrushTiles().empty());
    assert(editor.LoadGroundBrushPreset("Mossy Floor", presetError));
    assert(editor.GroundBrushTiles().size() == 2);
    assert(editor.GroundBrushTiles()[0].weight == 8);
    assert(editor.GroundBrushTiles()[1].weight == 3);

    // A malformed preset must fail safely and leave the current brush untouched.
    const std::filesystem::path badPreset = root / "_GroundBrushes" / "Caverns" /
        "caverns" / "broken.gbrush";
    std::filesystem::create_directories(badPreset.parent_path(), ec);
    {
        std::ofstream bad(badPreset);
        bad << "MYSTIC_GROUND_BRUSH 1\n"
               "name \"Broken\"\n"
               "tile \"Caverns\" 0 0 -16 16 1\n";
    }
    assert(!editor.LoadGroundBrushPreset("broken", presetError));
    assert(editor.GroundBrushTiles().size() == 2);
    assert(editor.RemoveGroundBrushTile(1));
    assert(editor.GroundBrushTiles().size() == 1);
    assert(!editor.RandomGroundBrushEnabled());

    // Selection order never overrides weights: the third/most recently added
    // tile remains rare, and the same ratio reaches the real paint path.
    RoomEditor orderedBrush;
    orderedBrush.BindForTesting("Caverns", Biome::Caverns, defs, root);
    orderedBrush.SelectLayer(RoomEditor::Layer::Ground);
    assert(orderedBrush.AddGroundBrushTile("Base", { 0.f, 0.f, 16.f, 16.f }, 8));
    assert(orderedBrush.AddGroundBrushTile("RareA", { 16.f, 0.f, 16.f, 16.f }, 1));
    assert(orderedBrush.AddGroundBrushTile("RareB", { 32.f, 0.f, 16.f, 16.f }, 1));
    int weightedPicks[3]{};
    for (std::uint32_t sample = 0; sample < 100; ++sample)
        ++weightedPicks[orderedBrush.ChooseGroundBrushIndex(sample)];
    assert(weightedPicks[0] == 80 && weightedPicks[1] == 10 && weightedPicks[2] == 10);

    for (int row = 0; row < RoomLayout::kRows; ++row)
        for (int col = 0; col < RoomLayout::kCols; ++col)
            assert(orderedBrush.PaintActiveCell(col, row));
    int paintedBase = 0, paintedRareA = 0, paintedRareB = 0;
    for (const RoomTilePlacement& tile : orderedBrush.Blueprint().visualTiles)
    {
        paintedBase += tile.sourceTileset == "Base";
        paintedRareA += tile.sourceTileset == "RareA";
        paintedRareB += tile.sourceTileset == "RareB";
    }
    assert(paintedRareA > 0 && paintedRareB > 0);
    assert(paintedBase > paintedRareA * 4);
    assert(paintedBase > paintedRareB * 4);

    // Painting through the door/wall lanes is now allowed — doors are authored
    // Door Zones, not protected cells, so a continuous wall can cross an opening.
    assert(editor.SetFall(14, 1, true));                 // was blocked as a "door lane"
    assert(editor.Blueprint().fall[1][14]);
    assert(editor.SetVisual(14, 0, false, "Forest", { 16.f, 32.f, 16.f, 16.f })); // wall art on the edge
    assert(editor.SetDoorVisual(13, 0, "Forest", { 32.f, 32.f, 16.f, 16.f }));
    assert(!editor.SetDoorVisual(5, 5, "Forest", { 32.f, 32.f, 16.f, 16.f }));
    assert(editor.Blueprint().visualTiles.back().door);
    assert(editor.PlaceAsset({ RoomAssetKind::Prop, "rock", 13, 1 })); // prop in the north lane
    assert(!editor.PlaceAsset({ RoomAssetKind::Prop, "rock", -1, 4 }));
    assert(!editor.PlaceAsset({ RoomAssetKind::Prop, "missing", 3, 4 }));

    // The explicit eraser affects only the active layer at one cell.
    assert(editor.SetVisual(25, 9, true, "Caverns", { 0.f, 0.f, 16.f, 16.f }));
    assert(editor.SetVisual(25, 9, false, "Caverns", { 16.f, 0.f, 16.f, 16.f }));
    assert(editor.SetSolid(25, 9, true));
    assert(editor.SetFall(25, 9, true));
    editor.SelectLayer(RoomEditor::Layer::Visual);
    assert(editor.EraseAt(25, 9));
    assert(editor.Blueprint().solid[9][25]);
    assert(editor.Blueprint().fall[9][25]);
    int groundAtCell = 0;
    int visualAtCell = 0;
    for (const RoomTilePlacement& visual : editor.Blueprint().visualTiles)
    {
        if (visual.col != 25 || visual.row != 9) continue;
        if (visual.ground) ++groundAtCell; else ++visualAtCell;
    }
    assert(groundAtCell == 1);
    assert(visualAtCell == 0);

    editor.SelectLayer(RoomEditor::Layer::Door);
    assert(editor.EraseAt(13, 0));
    for (const RoomTilePlacement& visual : editor.Blueprint().visualTiles)
        assert(!(visual.col == 13 && visual.row == 0 && visual.door));
    assert(editor.SetDoorVisual(13, 0, "Forest", { 32.f, 32.f, 16.f, 16.f }));

    assert(editor.PlaceAsset({ RoomAssetKind::Prop, "rock", 26, 9 }));
    assert(editor.PlaceAsset({ RoomAssetKind::Decor, "flower", 26, 9 }));
    editor.SelectLayer(RoomEditor::Layer::Props);
    assert(editor.EraseAt(26, 9));
    assert(editor.Blueprint().placements.size() >= 1);
    bool decorRemains = false;
    for (const RoomAssetPlacement& placement : editor.Blueprint().placements)
        decorRemains = decorRemains || (placement.assetId == "flower");
    assert(decorRemains);

    // Requirement 1: toggling an exit must not touch any authored layer, only the
    // connection flag and its Door Zone — and must never insert doorway art.
    const RoomBlueprint before = editor.Blueprint();
    editor.SetDoors(before.hasNorth, before.hasSouth, /*east*/true, before.hasWest);
    const RoomBlueprint& after = editor.Blueprint();
    assert(after.hasEast);
    assert(after.doorZones[(int)RoomWallSide::Right].enabled);
    bool tilesSame = true, fallSame = true, solidSame = true;
    for (int r = 0; r < RoomLayout::kRows; ++r)
        for (int c = 0; c < RoomLayout::kCols; ++c)
        {
            tilesSame = tilesSame && after.tiles[r][c] == before.tiles[r][c];
            fallSame  = fallSame  && after.fall[r][c]  == before.fall[r][c];
            solidSame = solidSame && after.solid[r][c] == before.solid[r][c];
        }
    assert(tilesSame && fallSame && solidSame);
    assert(after.visualTiles.size() == before.visualTiles.size());
    assert(after.placements.size() == before.placements.size());

    // ── New authoring ops: fill / flood / clear / eyedropper / duplicate ─────
    // Rectangle fill records ONE undo entry for the whole area.
    editor.SelectLayer(RoomEditor::Layer::Collision);
    assert(editor.FillRect(18, 10, 20, 12, true));
    for (int r = 10; r <= 12; ++r) for (int c = 18; c <= 20; ++c)
        assert(editor.Blueprint().solid[r][c]);
    assert(editor.Undo());
    for (int r = 10; r <= 12; ++r) for (int c = 18; c <= 20; ++c)
        assert(!editor.Blueprint().solid[r][c]);

    // Flood fill covers a contiguous region; one undo restores it.
    editor.SelectLayer(RoomEditor::Layer::FallZones);
    assert(editor.FillRect(22, 10, 24, 12, true));   // isolated 3x3 block of fall
    assert(editor.FloodFillFrom(23, 11, false));      // clear the contiguous block
    for (int r = 10; r <= 12; ++r) for (int c = 22; c <= 24; ++c)
        assert(!editor.Blueprint().fall[r][c]);
    assert(editor.Undo());
    for (int r = 10; r <= 12; ++r) for (int c = 22; c <= 24; ++c)
        assert(editor.Blueprint().fall[r][c]);

    // Clear layer wipes only the active layer, one undo.
    editor.Blueprint().fallRects.push_back({ 3.25f, 4.50f, 2.25f, 1.25f });
    assert(editor.ClearActiveLayer());
    for (int r = 0; r < RoomLayout::kRows; ++r) for (int c = 0; c < RoomLayout::kCols; ++c)
        assert(!editor.Blueprint().fall[r][c]);
    assert(editor.Blueprint().fallRects.empty());
    assert(editor.Undo());
    assert(editor.Blueprint().fall[10][22]);          // block restored
    assert(editor.Blueprint().fallRects.size() == 1);

    // Eyedropper picks a placed prop into the selection.
    editor.SelectLayer(RoomEditor::Layer::Props);
    assert(editor.PlaceAsset({ RoomAssetKind::Prop, "rock", 9, 9 }));
    editor.PickAt(9, 9);
    assert(editor.SelectedAssetId() == "rock");

    // Duplicate forks a room: fresh id, "<name> copy", identical layers.
    RoomBlueprint original = editor.Blueprint();
    original.name = "Original";
    RoomBlueprint dup = RoomEditor::Duplicate(original);
    assert(dup.id != original.id);
    assert(dup.name == "Original copy");
    assert(dup.visualTiles.size() == original.visualTiles.size());
    assert(dup.solid[10][22] == original.solid[10][22]);

    // A missing coverage chip prepares a fresh unsaved room with that exact mask.
    assert(!editor.CreateRoomForDoorMask(0));
    assert(!editor.CreateRoomForDoorMask(16));
    const unsigned char nsw = RoomDoorMask(true, true, false, true);
    assert(editor.CreateRoomForDoorMask(nsw));
    assert(editor.Blueprint().DoorMask() == nsw);
    assert(editor.Blueprint().hasNorth && editor.Blueprint().hasSouth);
    assert(!editor.Blueprint().hasEast && editor.Blueprint().hasWest);
    assert(editor.Blueprint().name == "NSW Room");
    assert(editor.Blueprint().visualTiles.empty());
    assert(editor.Blueprint().placements.empty());

    std::filesystem::remove_all(root, ec);
    return 0;
}
