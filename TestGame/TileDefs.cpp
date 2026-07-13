#ifdef _MSC_VER
#pragma warning(disable: 4996) // suppress MSVC "unsafe function" warnings for fopen/fscanf
#endif

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

TileRenderSource ResolveTileRenderSource(const TileDefSet& defs, TileType type)
{
    const int idx = (int)type;
    const bool assigned = idx >= 0 && idx < (int)TileType::Count && defs.assigned[idx];

    // Caverns is the canonical chest-art source. This fallback is deliberately
    // limited to rewards so no biome floor, wall, border, prop, or door art can
    // leak in from Caverns.
    if (!assigned && (type == TileType::ChestClosed || type == TileType::ChestOpen))
    {
        const float row = type == TileType::ChestClosed ? 18.f : 19.f;
        return { { 23.f * kTileSize, row * kTileSize,
                   (float)kTileSize, (float)kTileSize },
                 TileRenderSheet::SharedReward };
    }

    TileRenderSheet sheet = TileRenderSheet::Biome;
    if (assigned && defs.fromGround[idx])
        sheet = TileRenderSheet::Ground;
    return { defs.Get(type), sheet };
}

bool TileDefSet::LoadFromFile(const char* path)
{
    FILE* f = fopen(path, "r");
    if (!f) return false;

    // First line: BIOME <name> — skip it.
    char tag[32]{}, biome[64]{};
    fscanf(f, "%31s %63s", tag, biome);

    // Remaining lines: tag-prefixed or legacy (5 bare integers).
    while (fscanf(f, "%31s", tag) == 1)
    {
        if (strcmp(tag, "TILE") == 0)
        {
            int col, row, sc, sr, typeIdx;
            if (fscanf(f, "%d %d %d %d %d", &col, &row, &sc, &sr, &typeIdx) != 5) continue;
            if (typeIdx < 0 || typeIdx >= (int)TileType::Count) continue;
            rects[typeIdx]    = { (float)(col * kTileSize), (float)(row * kTileSize),
                                  (float)(sc  * kTileSize), (float)(sr  * kTileSize) };
            assigned[typeIdx] = true;
        }
        else if (strcmp(tag, "PROP") == 0)
        {
            float x, y, w, h;
            if (fscanf(f, "%f %f %f %f", &x, &y, &w, &h) != 4) continue;
            // Try to read optional collision rect (cx cy cw ch).
            // Old files only have 4 values; default collision = full sprite.
            float cx = 0.f, cy = 0.f, cw = w, ch = h;
            long savedPos = ftell(f);
            float tmp[4]{};
            if (fscanf(f, "%f %f %f %f", &tmp[0], &tmp[1], &tmp[2], &tmp[3]) == 4)
                { cx = tmp[0]; cy = tmp[1]; cw = tmp[2]; ch = tmp[3]; }
            else
                fseek(f, savedPos, SEEK_SET);
            props.push_back({ {x, y, w, h}, {cx, cy, cw, ch} });
        }
        else if (strcmp(tag, "DECOR") == 0)
        {
            float x, y, w, h;
            if (fscanf(f, "%f %f %f %f", &x, &y, &w, &h) != 4) continue;
            decors.push_back({ { x, y, w, h } });
        }
        else if (strcmp(tag, "ANIMDECOR") == 0)
        {
            float fps;
            int   fc;
            if (fscanf(f, "%f %d", &fps, &fc) != 2) continue;
            AnimSpriteDef def;
            def.fps = fps;
            for (int i = 0; i < fc; i++)
            {
                float fx, fy, fw, fh;
                if (fscanf(f, "%f %f %f %f", &fx, &fy, &fw, &fh) != 4) break;
                def.frames.push_back({ fx, fy, fw, fh });
            }
            if (!def.frames.empty())
                animDecors.push_back(std::move(def));
        }
        else if (strcmp(tag, "GTILE") == 0)
        {
            int col, row, sc, sr, typeIdx;
            if (fscanf(f, "%d %d %d %d %d", &col, &row, &sc, &sr, &typeIdx) != 5) continue;
            if (typeIdx < 0 || typeIdx >= (int)TileType::Count) continue;
            rects[typeIdx]     = { (float)(col * kTileSize), (float)(row * kTileSize),
                                   (float)(sc  * kTileSize), (float)(sr  * kTileSize) };
            assigned[typeIdx]  = true;
            fromGround[typeIdx] = true;
        }
        else if (strcmp(tag, "ANIMPROP") == 0)
        {
            // Format: cx cy cw ch fps frameCount  x0 y0 w0 h0  x1 y1 w1 h1 ...
            float cx, cy, cw, ch, fps;
            int   fc;
            if (fscanf(f, "%f %f %f %f %f %d", &cx, &cy, &cw, &ch, &fps, &fc) != 6) continue;
            AnimPropDef def;
            def.collision = { cx, cy, cw, ch };
            def.fps       = fps;
            for (int i = 0; i < fc; i++)
            {
                float fx, fy, fw, fh;
                if (fscanf(f, "%f %f %f %f", &fx, &fy, &fw, &fh) != 4) break;
                def.frames.push_back({ fx, fy, fw, fh });
            }
            if (!def.frames.empty())
                animProps.push_back(std::move(def));
        }
        else
        {
            // Legacy format: tag is actually the first integer (col).
            int col = atoi(tag);
            int row, sc, sr, typeIdx;
            if (fscanf(f, "%d %d %d %d", &row, &sc, &sr, &typeIdx) != 4) continue;
            if (typeIdx < 0 || typeIdx >= (int)TileType::Count) continue;
            rects[typeIdx]    = { (float)(col * kTileSize), (float)(row * kTileSize),
                                  (float)(sc  * kTileSize), (float)(sr  * kTileSize) };
            assigned[typeIdx] = true;
        }
    }

    fclose(f);
    return true;
}
