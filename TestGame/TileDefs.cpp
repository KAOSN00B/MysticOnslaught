#include "TileDefs.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

static constexpr int kTileSize = 16;

Rectangle TileDefSet::Get(TileType t) const
{
    int i = (int)t;
    if (i >= 0 && i < (int)TileType::Count && assigned[i])
        return rects[i];

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
    if (!f) return false;

    // First line: BIOME <name> — skip it.
    char tag[32]{}, biome[64]{};
    fscanf_s(f, "%31s %63s", tag, (unsigned)sizeof(tag),
                              biome, (unsigned)sizeof(biome));

    // Remaining lines: tag-prefixed or legacy (5 bare integers).
    while (fscanf_s(f, "%31s", tag, (unsigned)sizeof(tag)) == 1)
    {
        if (strcmp(tag, "TILE") == 0)
        {
            int col, row, sc, sr, typeIdx;
            if (fscanf_s(f, "%d %d %d %d %d", &col, &row, &sc, &sr, &typeIdx) != 5) continue;
            if (typeIdx < 0 || typeIdx >= (int)TileType::Count) continue;
            rects[typeIdx]    = { (float)(col * kTileSize), (float)(row * kTileSize),
                                  (float)(sc  * kTileSize), (float)(sr  * kTileSize) };
            assigned[typeIdx] = true;
        }
        else if (strcmp(tag, "PROP") == 0)
        {
            float x, y, w, h;
            if (fscanf_s(f, "%f %f %f %f", &x, &y, &w, &h) != 4) continue;
            props.push_back({ { x, y, w, h } });
        }
        else if (strcmp(tag, "DECOR") == 0)
        {
            float x, y, w, h;
            if (fscanf_s(f, "%f %f %f %f", &x, &y, &w, &h) != 4) continue;
            decors.push_back({ { x, y, w, h } });
        }
        else
        {
            // Legacy format: tag is actually the first integer (col).
            int col = atoi(tag);
            int row, sc, sr, typeIdx;
            if (fscanf_s(f, "%d %d %d %d", &row, &sc, &sr, &typeIdx) != 4) continue;
            if (typeIdx < 0 || typeIdx >= (int)TileType::Count) continue;
            rects[typeIdx]    = { (float)(col * kTileSize), (float)(row * kTileSize),
                                  (float)(sc  * kTileSize), (float)(sr  * kTileSize) };
            assigned[typeIdx] = true;
        }
    }

    fclose(f);
    return true;
}
