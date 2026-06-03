#include "TileDefs.h"
#include <cstdio>
#include <cstring>

static constexpr int kTileSize = 16;

Rectangle TileDefSet::Get(TileType t) const
{
    int i = (int)t;
    if (i >= 0 && i < (int)TileType::Count && assigned[i])
        return rects[i];

    // Fallback chain: prefer Floor, then first assigned tile, then top-left of sheet.
    if (assigned[(int)TileType::Floor])
        return rects[(int)TileType::Floor];
    for (int j = 0; j < (int)TileType::Count; j++)
        if (assigned[j]) return rects[j];

    return { 0.f, 0.f, (float)kTileSize, (float)kTileSize };
}

bool TileDefSet::LoadFromFile(const char* path)
{
    FILE* f = nullptr;
    fopen_s(&f, path, "r");
    if (!f)
        return false;

    // First line is "BIOME <name>" — skip it.
    char tag[32]{}, biome[64]{};
    fscanf_s(f, "%31s %63s", tag, (unsigned)sizeof(tag),
                              biome, (unsigned)sizeof(biome));

    // Remaining lines: col row spanCols spanRows typeIdx (all in 16px grid units).
    int col, row, sc, sr, typeIdx;
    while (fscanf_s(f, "%d %d %d %d %d", &col, &row, &sc, &sr, &typeIdx) == 5)
    {
        if (typeIdx < 0 || typeIdx >= (int)TileType::Count)
            continue;

        rects[typeIdx] = {
            (float)(col * kTileSize),
            (float)(row * kTileSize),
            (float)(sc  * kTileSize),
            (float)(sr  * kTileSize)
        };
        assigned[typeIdx] = true;
    }

    fclose(f);
    return true;
}
