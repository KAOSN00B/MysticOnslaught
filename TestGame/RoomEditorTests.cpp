#include "RoomEditor.h"

#include <cassert>
#include <filesystem>

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

    // Painting through the door/wall lanes is now allowed — doors are authored
    // Door Zones, not protected cells, so a continuous wall can cross an opening.
    assert(editor.SetFall(14, 1, true));                 // was blocked as a "door lane"
    assert(editor.Blueprint().fall[1][14]);
    assert(editor.SetVisual(14, 0, false, "Forest", { 16.f, 32.f, 16.f, 16.f })); // wall art on the edge
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
    assert(editor.ClearActiveLayer());
    for (int r = 0; r < RoomLayout::kRows; ++r) for (int c = 0; c < RoomLayout::kCols; ++c)
        assert(!editor.Blueprint().fall[r][c]);
    assert(editor.Undo());
    assert(editor.Blueprint().fall[10][22]);          // block restored

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

    std::filesystem::remove_all(root, ec);
    return 0;
}
