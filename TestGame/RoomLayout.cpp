#include "RoomLayout.h"
#include "raylib.h"

RoomLayout RoomLayout::Generate(bool hasNorth, bool hasSouth,
                                bool hasEast,  bool hasWest,
                                RoomType type,
                                int propCount, int decorCount)
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
    if (propCount > 0 && type != RoomType::Boss && type != RoomType::Rest)
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

    return layout;
}
