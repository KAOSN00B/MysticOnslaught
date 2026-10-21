#include "RoomBlueprint.h"
#include "RoomAssetCatalog.h"
#include "RoomCollision.h"
#include "TileRenderer.h"
#include "RoomLibrary.h"
#include "DungeonGen.h"
#include <cassert>
#include <iostream>

bool renderPreviews = false;

bool SameRect(Rectangle a, Rectangle b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

void Check(const std::string& name, bool pool = false)
{
    std::string error;
    auto source = RoomBlueprint::Load("TestGame/Rooms/Forest/forest/" + name + ".mroom", error);
    assert(source && error.empty());
    const std::string targetRoot = pool ? "docs/biome-content-audit/jungle-pool/Rooms/" : "docs/biome-content-audit/proof/";
    auto target = RoomBlueprint::Load(targetRoot + "forest_" + name + "_jungle.mroom", error);
    assert(target && error.empty());
    RoomAssetCatalog catalog;
    assert(catalog.Refresh("MapTilesets", "TestGame"));
    auto forest = catalog.Find("Forest");
    RoomAssetCatalog targetCatalog;
    assert(targetCatalog.Refresh(pool ? "docs/biome-content-audit/jungle-pool/MapTilesets" : "MapTilesets",
                                pool ? "docs/biome-content-audit/jungle-pool" : "TestGame"));
    auto jungle = targetCatalog.Find("Jungle");
    assert(forest && jungle);
    auto before = BuildRoomLayout(*source, forest->definitions, error, &catalog);
    assert(before && error.empty());
    auto after = BuildRoomLayout(*target, jungle->definitions, error, &targetCatalog);
    if (!error.empty()) std::cerr << name << ": " << error << '\n';
    assert(after && error.empty());
    assert(source->DoorMask() == target->DoorMask());
    assert(source->roomType == target->roomType);
    assert(source->combatCapacityOverride == target->combatCapacityOverride);
    assert(source->placements.size() == target->placements.size());
    assert(before->colliders.size() == after->colliders.size());
    assert(before->fallRects.size() == after->fallRects.size());
    for (size_t i=0; i<before->colliders.size(); ++i) assert(SameRect(before->colliders[i], after->colliders[i]));
    for (size_t i=0; i<before->fallRects.size(); ++i) assert(SameRect(before->fallRects[i], after->fallRects[i]));
    std::vector<Rectangle> beforeProps, afterProps;
    for (size_t i=0; i<source->placements.size(); ++i) {
        const auto& a=source->placements[i]; const auto& b=target->placements[i];
        assert(a.kind==b.kind && a.col==b.col && a.row==b.row && a.band==b.band);
        const auto* ad=catalog.Find(a.sourceTileset.empty()?source->tilesetStem:a.sourceTileset);
        const auto* bd=targetCatalog.Find(b.sourceTileset.empty()?target->tilesetStem:b.sourceTileset);
        assert(ad && bd);
        const int ai=ad->definitions.FindAssetIndex(a.kind,a.assetId);
        const int bi=bd->definitions.FindAssetIndex(b.kind,b.assetId);
        assert(ai>=0 && bi>=0);
        if (a.kind==RoomAssetKind::Prop) {
            const auto& ap=ad->definitions.props[ai]; const auto& bp=bd->definitions.props[bi];
            assert(SameRect(ap.collision,bp.collision));
            assert(ap.src.width==bp.src.width && ap.src.height==bp.src.height);
            beforeProps.push_back({a.col*16.f+ap.collision.x,a.row*16.f+ap.collision.y,ap.collision.width,ap.collision.height});
            afterProps.push_back({b.col*16.f+bp.collision.x,b.row*16.f+bp.collision.y,bp.collision.width,bp.collision.height});
        }
        // No Forest source has animated solid props; fail if that changes.
        assert(a.kind!=RoomAssetKind::AnimProp);
    }
    assert(before->visualTiles.size() == after->visualTiles.size());
    for (int r = 0; r < RoomLayout::kRows; ++r)
        for (int c = 0; c < RoomLayout::kCols; ++c) {
            assert(before->tiles[r][c] == after->tiles[r][c]);
            assert(before->solid[r][c] == after->solid[r][c]);
            assert(before->fall[r][c] == after->fall[r][c]);
        }
    for (size_t i = 0; i < after->visualTiles.size(); ++i) {
        const auto& a = before->visualTiles[i];
        const auto& b = after->visualTiles[i];
        assert(b.sourceTileset == "Jungle" || (pool && b.sourceTileset == "Jungle_ForestVariants"));
        assert(a.col == b.col && a.row == b.row);
        assert(a.ground == b.ground && a.door == b.door);
        assert(a.anchorOffset.x == b.anchorOffset.x && a.anchorOffset.y == b.anchorOffset.y);
        assert(b.src.x >= 0 && b.src.y >= 0 && b.src.width > 0 && b.src.height > 0);
        const auto* sheet=targetCatalog.Find(b.sourceTileset); assert(sheet);
        assert(b.src.x + b.src.width <= sheet->tileColumns * 16);
        assert(b.src.y + b.src.height <= sheet->tileRows * 16);
    }
    for (unsigned char mask : { (unsigned char)0, source->DoorMask() }) {
        ApplyActiveRoomDoorMask(*before, mask);
        ApplyActiveRoomDoorMask(*after, mask);
        for (int y = 0; y < 256; y += 4)
            for (int x = 0; x < 448; x += 4) {
                Rectangle body{ (float)x, (float)y, 8, 8 };
                assert(IsRoomSpawnAreaValid(*before, body, 16, 16, beforeProps) ==
                       IsRoomSpawnAreaValid(*after, body, 16, 16, afterProps));
                assert(IsRoomFallPoint(*before, {(float)x, (float)y}, 16, 16) ==
                       IsRoomFallPoint(*after, {(float)x, (float)y}, 16, 16));
            }
        for (size_t i = 0; i < after->visualTiles.size(); ++i)
            assert(RoomVisualClearedByOpenDoor(before->visualTiles[i], *before) ==
                   RoomVisualClearedByOpenDoor(after->visualTiles[i], *after));
    }
    std::cout << "PASS " << name << ": native loaders, " << after->visualTiles.size() << " Jungle sprites, identical grids and prop hitboxes, 14336 sampled body/fall positions and door clearing.\n";
    if (pool && renderPreviews) {
        TileRenderer originalRenderer, targetRenderer;
        originalRenderer.Init("MapTilesets/Forest.png", "MapTilesets/Ground TIles.png", "MapTilesets/Caverns.png", forest->definitions);
        targetRenderer.Init("MapTilesets/Jungle.png", "MapTilesets/Ground TIles.png", "MapTilesets/Caverns.png", jungle->definitions);
        originalRenderer.LoadRoomAssetCatalog(catalog);
        targetRenderer.LoadRoomAssetCatalog(targetCatalog);
        auto canvas=LoadRenderTexture(928,304);
        BeginTextureMode(canvas); ClearBackground(Color{20,20,24,255});
        DrawText((name+" - Forest").c_str(), 8, 4, 16, WHITE);
        DrawText((name+" - Jungle").c_str(), 472, 4, 16, WHITE);
        originalRenderer.DrawRoom(*before,1,1,{8,32});
        targetRenderer.DrawRoom(*after,1,1,{472,32});
        EndTextureMode();
        Image preview=LoadImageFromTexture(canvas.texture); ImageFlipVertical(&preview);
        const std::string output="docs/biome-content-audit/jungle-pool/"+name+"-native.png";
        assert(ExportImage(preview,output.c_str()));
        UnloadImage(preview); UnloadRenderTexture(canvas);
        originalRenderer.Unload(); targetRenderer.Unload();
    }
}

int main(int argc, char**) {
    renderPreviews=argc>1;
    if (renderPreviews) { SetConfigFlags(FLAG_WINDOW_HIDDEN); InitWindow(928,304,"Room validation"); }
    RoomAssetCatalog catalog;
    assert(catalog.Refresh("MapTilesets", "TestGame"));
    int loaded = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator("TestGame/Rooms")) {
        if (entry.path().extension() != ".mroom") continue;
        std::string error;
        auto blueprint = RoomBlueprint::Load(entry.path(), error);
        if (!blueprint) { std::cerr << entry.path() << ": " << error << '\n'; return 1; }
        auto asset = catalog.Find(blueprint->tilesetStem);
        assert(asset);
        auto layout = BuildRoomLayout(*blueprint, asset->definitions, error, &catalog);
        if (!layout || !error.empty()) { std::cerr << entry.path() << ": " << error << '\n'; return 1; }
        ++loaded;
    }
    std::cout << "PASS baseline native load: " << loaded << " rooms, no missing asset warnings.\n";
    Check("s"); Check("w"); Check("nwe");
    int variants=0;
    for (const auto& entry : std::filesystem::directory_iterator("TestGame/Rooms/Forest/forest")) {
        if (entry.path().extension() != ".mroom") continue;
        Check(entry.path().stem().string(), true); ++variants;
    }
    assert(variants==23);
    std::cout << "PASS staged Jungle pool: " << variants << " authored layouts.\n";
    RoomLibrary library;
    library.Refresh("docs/biome-content-audit/jungle-pool/Rooms");
    int worstAttempts=0;
    for (unsigned seed=0; seed<1000; ++seed) {
        SetRandomSeed(seed); bool complete=false;
        for (int attempt=1; attempt<=600; ++attempt) { // Engine's existing authored-graph limit
            DungeonGen graph; graph.Generate(); complete=true;
            for (const auto& room : graph.GetRooms()) {
                auto candidates=library.PlaytestCandidates(Biome::Jungle,room.type,
                    RoomDoorMask(room.hasNorth,room.hasSouth,room.hasEast,room.hasWest));
                if (candidates.empty()) { complete=false; break; }
                GetRandomValue(0,(int)candidates.size()-1); // same RNG consumption as Engine
            }
            if (complete) { worstAttempts=std::max(worstAttempts,attempt); break; }
        }
        if (!complete) { std::cerr << "No complete Jungle dungeon for seed " << seed << '\n'; return 1; }
    }
    std::cout << "PASS 1000 seeded authored Jungle dungeons; worst attempts " << worstAttempts << "/600.\n";
    if (renderPreviews) CloseWindow();
}
