#include "RoomLayout.h"
#include "raylib.h"

RoomLayout RoomLayout::Generate(bool hasNorth, bool hasSouth,
                                bool hasEast,  bool hasWest)
{
    RoomLayout layout;

    const int cols = kCols;
    const int rows = kRows;

    // ── Fill interior with floor ──────────────────────────────────────────────
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
            layout.tiles[r][c] = TileType::Floor;

    // ── Top wall ──────────────────────────────────────────────────────────────
    layout.tiles[0][0]        = TileType::WallCornerTL;
    layout.tiles[0][cols - 1] = TileType::WallCornerTR;
    for (int c = 1; c < cols - 1; c++)
        layout.tiles[0][c] = TileType::WallTopFace;

    // ── Bottom wall ───────────────────────────────────────────────────────────
    layout.tiles[rows - 1][0]        = TileType::WallCornerBL;
    layout.tiles[rows - 1][cols - 1] = TileType::WallCornerBR;
    for (int c = 1; c < cols - 1; c++)
        layout.tiles[rows - 1][c] = TileType::WallBottom;

    // ── Left and right walls ──────────────────────────────────────────────────
    for (int r = 1; r < rows - 1; r++)
    {
        layout.tiles[r][0]        = TileType::WallLeft;
        layout.tiles[r][cols - 1] = TileType::WallRight;
    }

    // ── Door openings ─────────────────────────────────────────────────────────
    // North / South doors: 3 tiles wide, centred horizontally.
    int doorCentreC = cols / 2;
    int doorStartC  = doorCentreC - 1;   // tiles doorStartC, doorStartC+1, doorStartC+2

    // East / West doors: 2 tiles tall, centred vertically.
    int doorCentreR = rows / 2;
    int doorStartR  = doorCentreR - 1;   // tiles doorStartR, doorStartR+1

    if (hasNorth)
    {
        for (int dc = 0; dc < 3; dc++)
            layout.tiles[0][doorStartC + dc] = TileType::DoorOpen;
    }
    if (hasSouth)
    {
        for (int dc = 0; dc < 3; dc++)
            layout.tiles[rows - 1][doorStartC + dc] = TileType::DoorOpen;
    }
    if (hasWest)
    {
        for (int dr = 0; dr < 2; dr++)
            layout.tiles[doorStartR + dr][0] = TileType::DoorOpen;
    }
    if (hasEast)
    {
        for (int dr = 0; dr < 2; dr++)
            layout.tiles[doorStartR + dr][cols - 1] = TileType::DoorOpen;
    }

    // ── Random floor variant scatter (≈15% of floor tiles) ───────────────────
    for (int r = 1; r < rows - 1; r++)
        for (int c = 1; c < cols - 1; c++)
            if (layout.tiles[r][c] == TileType::Floor && GetRandomValue(0, 99) < 15)
                layout.tiles[r][c] = TileType::FloorVariant;

    return layout;
}
