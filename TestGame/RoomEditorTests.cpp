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

    RoomEditor editor;
    editor.BindForTesting("Caverns", Biome::Caverns, defs, root);
    editor.SetDoors(true, true, false, false);
    assert(editor.SetTerrain(4, 5, TileType::FloorVariant));
    assert(editor.SetFall(8, 6, true));
    assert(editor.PlaceAsset({ RoomAssetKind::Prop, "rock", 3, 4 }));
    assert(editor.Blueprint().tiles[5][4] == TileType::FloorVariant);
    assert(editor.Blueprint().fall[6][8]);
    assert(editor.Blueprint().placements.size() == 1);

    editor.Undo();
    assert(editor.Blueprint().placements.empty());
    editor.Redo();
    assert(editor.Blueprint().placements.size() == 1);

    assert(!editor.SetFall(14, 1, true)); // protected north entry lane
    assert(!editor.PlaceAsset({ RoomAssetKind::Prop, "rock", -1, 4 }));
    assert(!editor.PlaceAsset({ RoomAssetKind::Prop, "missing", 3, 4 }));

    std::filesystem::remove_all(root, ec);
    return 0;
}
