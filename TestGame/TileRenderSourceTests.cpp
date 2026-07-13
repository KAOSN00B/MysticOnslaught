#include "TileDefs.h"

#include <cassert>
#include <cstdio>

namespace
{
    void TestMissingChestUsesSharedRewardSheet()
    {
        TileDefSet defs{};
        defs.assigned[(int)TileType::Floor] = true;
        defs.rects[(int)TileType::Floor] = { 16.f, 16.f, 16.f, 16.f };
        TileRenderSource source = ResolveTileRenderSource(defs, TileType::ChestClosed);
        assert(source.sheet == TileRenderSheet::SharedReward);
        assert(source.src.x == 23.f * 16.f);
        assert(source.src.y == 18.f * 16.f);
        assert(source.src.width == 16.f && source.src.height == 16.f);
    }

    void TestAuthoredChestKeepsBiomeSheet()
    {
        TileDefSet defs{};
        defs.assigned[(int)TileType::ChestOpen] = true;
        defs.rects[(int)TileType::ChestOpen] = { 80.f, 96.f, 32.f, 16.f };
        TileRenderSource source = ResolveTileRenderSource(defs, TileType::ChestOpen);
        assert(source.sheet == TileRenderSheet::Biome);
        assert(source.src.x == 80.f && source.src.y == 96.f);
        assert(source.src.width == 32.f && source.src.height == 16.f);
    }

    void TestOrdinaryMissingTileKeepsExistingFallback()
    {
        TileDefSet defs{};
        defs.assigned[(int)TileType::Floor] = true;
        defs.rects[(int)TileType::Floor] = { 32.f, 48.f, 16.f, 16.f };
        TileRenderSource source = ResolveTileRenderSource(defs, TileType::BossKey);
        assert(source.sheet == TileRenderSheet::Biome);
        assert(source.src.x == 32.f && source.src.y == 48.f);
    }
}

int main()
{
    TestMissingChestUsesSharedRewardSheet();
    TestAuthoredChestKeepsBiomeSheet();
    TestOrdinaryMissingTileKeepsExistingFallback();
    std::puts("Tile render source tests passed");
    return 0;
}
