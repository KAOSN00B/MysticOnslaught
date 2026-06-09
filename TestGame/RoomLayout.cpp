#include "RoomLayout.h"
#include "raylib.h"

RoomLayout RoomLayout::Generate(bool hasNorth, bool hasSouth,
                                bool hasEast,  bool hasWest,
                                RoomType type,
                                int propCount, int decorCount, int animDecorCount,
                                int animPropCount)
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

    // ── Treasure chest centred in the room ───────────────────────────────────
    if (type == RoomType::Treasure)
        layout.tiles[rows / 2][cols / 2] = TileType::ChestClosed;

    // ── Props (solid objects) ─────────────────────────────────────────────────
    if (propCount > 0 && type != RoomType::Boss && type != RoomType::Rest && type != RoomType::Store)
    {
        int numProps = 0;
        switch (type)
        {
        case RoomType::Standard: numProps = GetRandomValue(1, 3); break;
        case RoomType::Elite:    numProps = GetRandomValue(3, 5); break;
        case RoomType::Treasure: numProps = GetRandomValue(0, 2); break;
        default:                 numProps = GetRandomValue(0, 2); break;
        }

        for (int p = 0; p < numProps; p++)
        {
            for (int attempt = 0; attempt < 25; attempt++)
            {
                int pr = GetRandomValue(2, rows - 3);
                int pc = GetRandomValue(2, cols - 3);

                // Stay clear of door paths.
                if (pc >= doorStartC - 1 && pc <= doorStartC + 3) continue;
                if (pr >= doorStartR - 1 && pr <= doorStartR + 2) continue;

                TileType t = layout.tiles[pr][pc];
                if (t != TileType::Floor && t != TileType::FloorVariant) continue;

                layout.props.push_back({ GetRandomValue(0, propCount - 1), pc, pr });
                break;
            }
        }
    }

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
                layout.decors.push_back({ GetRandomValue(0, decorCount - 1), dc, dr });
                break;
            }
        }
    }

    // ── Animated props — scattered like static props, drawn on top with collision ─
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
            for (int attempt = 0; attempt < 25; attempt++)
            {
                int pr = GetRandomValue(2, rows - 3);
                int pc = GetRandomValue(2, cols - 3);
                if (pc >= doorStartC - 1 && pc <= doorStartC + 3) continue;
                if (pr >= doorStartR - 1 && pr <= doorStartR + 2) continue;
                TileType t = layout.tiles[pr][pc];
                if (t != TileType::Floor && t != TileType::FloorVariant) continue;
                bool occupied = false;
                for (const auto& sp : layout.props)
                    if (sp.col == pc && sp.row == pr) { occupied = true; break; }
                if (occupied) continue;
                layout.animProps.push_back({ GetRandomValue(0, animPropCount - 1), pc, pr });
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

                // Don't stack on another decor or prop.
                bool occupied = false;
                for (const auto& d : layout.animDecors)
                    if (d.col == pc && d.row == pr) { occupied = true; break; }
                if (!occupied)
                    for (const auto& p : layout.props)
                        if (p.col == pc && p.row == pr) { occupied = true; break; }
                if (occupied) continue;

                layout.animDecors.push_back({ GetRandomValue(0, animDecorCount - 1), pc, pr });
                break;
            }
        }
    }

    return layout;
}
