#include "TileRenderer.h"

void TileRenderer::Init(const char* tilesheetPath, const TileDefSet& defs)
{
    if (_sheet.id != 0) { UnloadTexture(_sheet); _sheet = {}; }
    _sheet = LoadTexture(tilesheetPath);
    _defs  = defs;
}

void TileRenderer::Unload()
{
    if (_sheet.id != 0) { UnloadTexture(_sheet); _sheet = {}; }
}

void TileRenderer::DrawRoom(const RoomLayout& layout, float scaleX, float scaleY,
                            Vector2 screenOffset) const
{
    float cellW = 16.f * scaleX;
    float cellH = 16.f * scaleY;

    // ── Pass 1: floor base ────────────────────────────────────────────────────
    for (int r = 0; r < RoomLayout::kRows; r++)
    {
        for (int c = 0; c < RoomLayout::kCols; c++)
        {
            TileType type = layout.tiles[r][c];
            float sx = screenOffset.x + c * cellW;
            float sy = screenOffset.y + r * cellH;

            if (type == TileType::Void || type == TileType::None)
                DrawRectangle((int)sx, (int)sy, (int)cellW, (int)cellH, Color{ 8, 6, 10, 255 });
            else if (type == TileType::DoorOpen)
                DrawRectangle((int)sx, (int)sy, (int)cellW, (int)cellH, Color{ 12, 10, 14, 255 });
            else if (type == TileType::DoorLocked)
                DrawRectangle((int)sx, (int)sy, (int)cellW, (int)cellH, Color{ 60, 10, 10, 255 });
            else
            {
                TileType base = (type == TileType::FloorVariant)
                    ? TileType::FloorVariant : TileType::Floor;
                DrawTile(base, sx, sy, scaleX, scaleY);
            }
        }
    }

    // ── Pass 1.5: decorations (half-size, centred in cell, no collision) ─────
    for (const SpritePlacement& d : layout.decors)
    {
        if (d.defIdx < 0 || d.defIdx >= (int)_defs.decors.size()) continue;
        const Rectangle& src = _defs.decors[d.defIdx].src;
        float dScaleX = scaleX * 0.5f;
        float dScaleY = scaleY * 0.5f;
        // Centre the smaller sprite within the full cell.
        float sx = screenOffset.x + d.col * cellW + (cellW - src.width  * dScaleX) * 0.5f;
        float sy = screenOffset.y + d.row * cellH + (cellH - src.height * dScaleY) * 0.5f;
        DrawSpriteScaled(src, sx, sy, dScaleX, dScaleY);
    }

    // ── Pass 2: walls on top ──────────────────────────────────────────────────
    for (int r = 0; r < RoomLayout::kRows; r++)
    {
        for (int c = 0; c < RoomLayout::kCols; c++)
        {
            TileType type = layout.tiles[r][c];
            if (type == TileType::Floor     || type == TileType::FloorVariant ||
                type == TileType::Void      || type == TileType::None         ||
                type == TileType::DoorOpen  || type == TileType::DoorLocked)
                continue;

            float sx = screenOffset.x + c * cellW;
            float sy = screenOffset.y + r * cellH;
            DrawTile(type, sx, sy, scaleX, scaleY);
        }
    }

    // ── Pass 3: props (solid objects, drawn last so they sit on top) ──────────
    for (const SpritePlacement& p : layout.props)
    {
        if (p.defIdx < 0 || p.defIdx >= (int)_defs.props.size()) continue;
        float sx = screenOffset.x + p.col * cellW;
        float sy = screenOffset.y + p.row * cellH;
        DrawSpriteScaled(_defs.props[p.defIdx].src, sx, sy, scaleX, scaleY);
    }
}

void TileRenderer::DrawTile(TileType type, float screenX, float screenY,
                            float scaleX, float scaleY) const
{
    if (_sheet.id == 0) return;
    DrawSpriteScaled(_defs.Get(type), screenX, screenY, scaleX, scaleY);
}

void TileRenderer::DrawSpriteScaled(Rectangle src, float screenX, float screenY,
                                    float scaleX, float scaleY) const
{
    if (_sheet.id == 0) return;
    Rectangle dst{ screenX, screenY, src.width * scaleX, src.height * scaleY };
    DrawTexturePro(_sheet, src, dst, {}, 0.f, WHITE);
}
