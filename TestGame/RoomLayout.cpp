#include "RoomLayout.h"
#include "raylib.h"

#include <cmath>
#include <cstdlib>
#include <vector>

namespace
{
    // A placed object's footprint in tile units (col/row/width/height).
    struct TileFootprint { int col, row, w, h; };

    // Tiles a sprite covers, from its source-pixel size (16px tiles, round up).
    int TilesFor(float sourcePixels)
    {
        int tiles = (int)std::ceil(sourcePixels / 16.f);
        return (tiles < 1) ? 1 : tiles;
    }

    bool RectsOverlap(int aCol, int aRow, int aW, int aH,
                      int bCol, int bRow, int bW, int bH)
    {
        return aCol < bCol + bW && bCol < aCol + aW &&
               aRow < bRow + bH && bRow < aRow + aH;
    }
}

RoomLayout RoomLayout::Generate(bool hasNorth, bool hasSouth,
                                bool hasEast,  bool hasWest,
                                RoomType type,
                                const TileDefSet* defs,
                                int propDensityBonus)
{
    RoomLayout layout;

    const int cols = kCols;
    const int rows = kRows;

    // ── Fill with floor ───────────────────────────────────────────────────────
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
            layout.tiles[r][c] = TileType::Floor;

    // ── Border walls ──────────────────────────────────────────────────────────
    layout.tiles[0][0]        = TileType::WallCornerTL;
    layout.tiles[0][cols - 1] = TileType::WallCornerTR;
    for (int c = 1; c < cols - 1; c++)
        layout.tiles[0][c] = TileType::WallTopFace;

    layout.tiles[rows - 1][0]        = TileType::WallCornerBL;
    layout.tiles[rows - 1][cols - 1] = TileType::WallCornerBR;
    for (int c = 1; c < cols - 1; c++)
        layout.tiles[rows - 1][c] = TileType::WallBottom;

    for (int r = 1; r < rows - 1; r++)
    {
        layout.tiles[r][0]        = TileType::WallLeft;
        layout.tiles[r][cols - 1] = TileType::WallRight;
    }

    // ── Door openings ─────────────────────────────────────────────────────────
    int doorCentreC = cols / 2;
    int doorStartC  = doorCentreC - 1;
    int doorCentreR = rows / 2;
    int doorStartR  = doorCentreR - 1;

    if (hasNorth)
        for (int dc = 0; dc < 3; dc++)
            layout.tiles[0][doorStartC + dc] = TileType::DoorOpen;
    if (hasSouth)
        for (int dc = 0; dc < 3; dc++)
            layout.tiles[rows - 1][doorStartC + dc] = TileType::DoorOpen;
    if (hasWest)
        for (int dr = 0; dr < 2; dr++)
            layout.tiles[doorStartR + dr][0] = TileType::DoorOpen;
    if (hasEast)
        for (int dr = 0; dr < 2; dr++)
            layout.tiles[doorStartR + dr][cols - 1] = TileType::DoorOpen;

    // ── Floor variant scatter (≈15%) ──────────────────────────────────────────
    for (int r = 1; r < rows - 1; r++)
        for (int c = 1; c < cols - 1; c++)
            if (layout.tiles[r][c] == TileType::Floor && GetRandomValue(0, 99) < 15)
                layout.tiles[r][c] = TileType::FloorVariant;

    if (defs == nullptr)
        return layout;

    // ── Footprint-aware placement ─────────────────────────────────────────────
    // Every solid object is placed by its REAL sprite size (in tiles), with:
    //   * kWallMargin tiles kept clear of every wall,
    //   * the door cross-lanes kept fully clear (guaranteed walkable paths),
    //   * kClearance tiles of open floor between any two footprints, so the
    //     player AND enemies always fit between props (no more wedging),
    //   * no footprint ever overlapping another (no more stacked props).
    const int kWallMargin = 2;   // tiles between a footprint and the border walls
    const int kClearance  = 2;   // open tiles required between any two footprints

    std::vector<TileFootprint> placedFootprints;

    // The door lanes: a horizontal and a vertical band through the room centre
    // (matching the door positions) that placement must never block.
    const int laneC0 = doorStartC - 1, laneC1 = doorStartC + 3;   // vertical lane cols
    const int laneR0 = doorStartR - 1, laneR1 = doorStartR + 2;   // horizontal lane rows

    auto canPlaceFootprint = [&](int col, int row, int w, int h) -> bool
    {
        // Fully inside the walkable interior, with the wall margin.
        if (col < kWallMargin || row < kWallMargin) return false;
        if (col + w > cols - kWallMargin || row + h > rows - kWallMargin) return false;

        // Off the door lanes.
        if (RectsOverlap(col, row, w, h, laneC0, 0, laneC1 - laneC0 + 1, rows)) return false;
        if (RectsOverlap(col, row, w, h, 0, laneR0, cols, laneR1 - laneR0 + 1)) return false;

        // Clearance gap against everything already placed (expand by kClearance).
        for (const TileFootprint& other : placedFootprints)
            if (RectsOverlap(col - kClearance, row - kClearance,
                             w + kClearance * 2, h + kClearance * 2,
                             other.col, other.row, other.w, other.h))
                return false;

        return true;
    };

    // Tries to place one object of the given tile size; records the footprint
    // and returns the anchor via out params. False = no space found (skip it —
    // fewer props always beats overlapping props).
    auto tryPlace = [&](int w, int h, int& outCol, int& outRow) -> bool
    {
        for (int attempt = 0; attempt < 60; attempt++)
        {
            int col = GetRandomValue(kWallMargin, cols - kWallMargin - w);
            int row = GetRandomValue(kWallMargin, rows - kWallMargin - h);
            if (!canPlaceFootprint(col, row, w, h)) continue;
            placedFootprints.push_back({ col, row, w, h });
            outCol = col; outRow = row;
            return true;
        }
        return false;
    };

    const int propCount      = (int)defs->props.size();
    const int animPropCount  = (int)defs->animProps.size();
    const int decorCount     = (int)defs->decors.size();
    const int animDecorCount = (int)defs->animDecors.size();

    // ── Props (solid objects) ─────────────────────────────────────────────────
    if (propCount > 0 && type != RoomType::Boss && type != RoomType::Rest && type != RoomType::Store)
    {
        // Base counts raised now that footprint placement guarantees clean
        // spacing — dense biomes (Forest/Jungle) add propDensityBonus on top.
        int numProps = 0;
        switch (type)
        {
        case RoomType::Standard: numProps = GetRandomValue(3, 6 + propDensityBonus); break;
        case RoomType::Elite:    numProps = GetRandomValue(4, 7 + propDensityBonus); break;
        case RoomType::Treasure: numProps = GetRandomValue(1, 4 + propDensityBonus); break;
        default:                 numProps = GetRandomValue(1, 4 + propDensityBonus); break;
        }

        for (int p = 0; p < numProps; p++)
        {
            int defIdx = GetRandomValue(0, propCount - 1);
            const Rectangle& src = defs->props[defIdx].src;
            int col, row;
            if (tryPlace(TilesFor(src.width), TilesFor(src.height), col, row))
                layout.props.push_back({ defIdx, col, row });
        }
    }

    // ── Animated props — same footprint rules, sharing the placed list ───────
    if (animPropCount > 0 && type != RoomType::Boss && type != RoomType::Rest && type != RoomType::Store)
    {
        int numAnimProps = 0;
        switch (type)
        {
        case RoomType::Standard: numAnimProps = GetRandomValue(0, 1); break;
        case RoomType::Elite:    numAnimProps = GetRandomValue(0, 2); break;
        case RoomType::Treasure: numAnimProps = GetRandomValue(0, 1); break;
        default:                 numAnimProps = 0; break;
        }

        for (int p = 0; p < numAnimProps; p++)
        {
            int defIdx = GetRandomValue(0, animPropCount - 1);
            if (defs->animProps[defIdx].frames.empty()) continue;
            const Rectangle& frame = defs->animProps[defIdx].frames[0];
            int col, row;
            if (tryPlace(TilesFor(frame.width), TilesFor(frame.height), col, row))
                layout.animProps.push_back({ defIdx, col, row });
        }
    }

    // Decors have no collision, so they skip clearance — but they must never sit
    // under a prop's footprint (that's the "stacked on top of each other" look).
    auto cellUnderFootprint = [&](int col, int row) -> bool
    {
        for (const TileFootprint& fp : placedFootprints)
            if (RectsOverlap(col, row, 1, 1, fp.col, fp.row, fp.w, fp.h))
                return true;
        return false;
    };

    // ── Decorations (floor-level, no collision) — 1–7 randomly placed ───────
    if (decorCount > 0)
    {
        int numDecors = GetRandomValue(1, 7);
        for (int d = 0; d < numDecors; d++)
        {
            for (int attempt = 0; attempt < 20; attempt++)
            {
                int dr = GetRandomValue(1, rows - 2);
                int dc = GetRandomValue(1, cols - 2);
                TileType t = layout.tiles[dr][dc];
                if (t != TileType::Floor && t != TileType::FloorVariant) continue;
                if (cellUnderFootprint(dc, dr)) continue;

                // Not on another decor's cell either.
                bool taken = false;
                for (const auto& existing : layout.decors)
                    if (existing.col == dc && existing.row == dr) { taken = true; break; }
                if (taken) continue;

                layout.decors.push_back({ GetRandomValue(0, decorCount - 1), dc, dr });
                break;
            }
        }
    }

    // ── Animated decorations (torches/fire) — placed next to walls ───────────
    // Torches sit on the floor tile immediately inside a wall, giving a
    // wall-mounted appearance when the animated flame renders above them.
    if (animDecorCount > 0)
    {
        int numTorches = GetRandomValue(2, 5);
        for (int t = 0; t < numTorches; t++)
        {
            for (int attempt = 0; attempt < 30; attempt++)
            {
                // Pick a cell adjacent to a border wall (row 1 or rows-2, col 1 or cols-2).
                int pr, pc;
                int side = GetRandomValue(0, 3);
                switch (side)
                {
                case 0: pr = 1;        pc = GetRandomValue(2, cols - 3); break; // near top wall
                case 1: pr = rows - 2; pc = GetRandomValue(2, cols - 3); break; // near bottom wall
                case 2: pr = GetRandomValue(2, rows - 3); pc = 1;        break; // near left wall
                default:pr = GetRandomValue(2, rows - 3); pc = cols - 2; break; // near right wall
                }

                TileType t2 = layout.tiles[pr][pc];
                if (t2 != TileType::Floor && t2 != TileType::FloorVariant) continue;
                if (cellUnderFootprint(pc, pr)) continue;

                // Don't stack on another torch.
                bool occupied = false;
                for (const auto& d : layout.animDecors)
                    if (d.col == pc && d.row == pr) { occupied = true; break; }
                if (occupied) continue;

                layout.animDecors.push_back({ GetRandomValue(0, animDecorCount - 1), pc, pr });
                break;
            }
        }
    }

    return layout;
}
