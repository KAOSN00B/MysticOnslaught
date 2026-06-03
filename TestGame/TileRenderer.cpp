#include "TileRenderer.h"

void TileRenderer::Init(const char* tilesheetPath, const TileDefSet& defs)
{
    if (_sheet.id != 0)
    {
        UnloadTexture(_sheet);
        _sheet = {};
    }
    _sheet = LoadTexture(tilesheetPath);
    _defs  = defs;
}

void TileRenderer::Unload()
{
    if (_sheet.id != 0)
    {
        UnloadTexture(_sheet);
        _sheet = {};
    }
}

void TileRenderer::DrawRoom(const RoomLayout& layout, float drawScale,
                            Vector2 screenOffset) const
{
    float cellPx = 16.f * drawScale;

    // ── Pass 1: floor base ────────────────────────────────────────────────────
    // Draw floor under every cell first so transparent wall pixels reveal
    // floor instead of the dark background.
    for (int r = 0; r < RoomLayout::kRows; r++)
    {
        for (int c = 0; c < RoomLayout::kCols; c++)
        {
            TileType type = layout.tiles[r][c];
            float sx = screenOffset.x + c * cellPx;
            float sy = screenOffset.y + r * cellPx;

            if (type == TileType::Void || type == TileType::None)
            {
                DrawRectangle((int)sx, (int)sy, (int)cellPx, (int)cellPx,
                    Color{ 8, 6, 10, 255 });
            }
            else if (type == TileType::DoorOpen)
            {
                DrawRectangle((int)sx, (int)sy, (int)cellPx, (int)cellPx,
                    Color{ 12, 10, 14, 255 });
            }
            else if (type == TileType::DoorLocked)
            {
                DrawRectangle((int)sx, (int)sy, (int)cellPx, (int)cellPx,
                    Color{ 60, 10, 10, 255 });
            }
            else
            {
                // FloorVariant cells use their own sprite; everything else
                // (walls, corners) gets plain floor underneath.
                TileType base = (type == TileType::FloorVariant)
                    ? TileType::FloorVariant : TileType::Floor;
                DrawTile(base, sx, sy, drawScale);
            }
        }
    }

    // ── Pass 2: walls on top ──────────────────────────────────────────────────
    // Skip floor, void, and doors — already handled in pass 1.
    for (int r = 0; r < RoomLayout::kRows; r++)
    {
        for (int c = 0; c < RoomLayout::kCols; c++)
        {
            TileType type = layout.tiles[r][c];
            if (type == TileType::Floor        ||
                type == TileType::FloorVariant ||
                type == TileType::Void         ||
                type == TileType::None         ||
                type == TileType::DoorOpen     ||
                type == TileType::DoorLocked)
                continue;

            float sx = screenOffset.x + c * cellPx;
            float sy = screenOffset.y + r * cellPx;
            DrawTile(type, sx, sy, drawScale);
        }
    }
}

void TileRenderer::DrawTile(TileType type, float screenX, float screenY,
                            float drawScale) const
{
    if (_sheet.id == 0)
        return;

    Rectangle src = _defs.Get(type);

    // Draw at the tile's actual source dimensions so multi-tile sprites
    // (e.g. Wall Bottom at 16×32) render at full height and extend naturally
    // beyond the cell boundary — giving walls proper visible thickness.
    Rectangle dst{
        screenX,
        screenY,
        src.width  * drawScale,
        src.height * drawScale
    };

    DrawTexturePro(_sheet, src, dst, {}, 0.f, WHITE);
}
