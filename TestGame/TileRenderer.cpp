#include "TileRenderer.h"

void TileRenderer::Init(const char* tilesheetPath, const char* groundSheetPath,
                        const char* sharedRewardSheetPath, const TileDefSet& defs)
{
    if (_sheet.id != 0)              { UnloadTexture(_sheet);              _sheet = {}; }
    if (_groundSheet.id != 0)        { UnloadTexture(_groundSheet);        _groundSheet = {}; }
    if (_sharedRewardSheet.id != 0)  { UnloadTexture(_sharedRewardSheet);  _sharedRewardSheet = {}; }
    _sheet             = LoadTexture(tilesheetPath);
    _groundSheet       = LoadTexture(groundSheetPath);
    _sharedRewardSheet = LoadTexture(sharedRewardSheetPath);
    _defs = defs;
}

void TileRenderer::Unload()
{
    if (_sheet.id != 0)       { UnloadTexture(_sheet);       _sheet       = {}; }
    if (_groundSheet.id != 0) { UnloadTexture(_groundSheet); _groundSheet = {}; }
    if (_sharedRewardSheet.id != 0)
        { UnloadTexture(_sharedRewardSheet); _sharedRewardSheet = {}; }
}

void TileRenderer::DrawRoom(const RoomLayout& layout, float scaleX, float scaleY,
                            Vector2 screenOffset, bool includeProps) const
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
                DrawTile(TileType::Floor, sx, sy, scaleX, scaleY);
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

    // ── Pass 1.5a: static decorations (half-size, centred in cell) ───────────
    for (const SpritePlacement& d : layout.decors)
    {
        if (d.defIdx < 0 || d.defIdx >= (int)_defs.decors.size()) continue;
        const Rectangle& src = _defs.decors[d.defIdx].src;
        float dScaleX = scaleX * 0.75f;
        float dScaleY = scaleY * 0.75f;
        // Centre the smaller sprite within the full cell.
        float sx = screenOffset.x + d.col * cellW + (cellW - src.width  * dScaleX) * 0.5f;
        float sy = screenOffset.y + d.row * cellH + (cellH - src.height * dScaleY) * 0.5f;
        DrawSpriteScaled(src, sx, sy, dScaleX, dScaleY);
    }

    // ── Pass 1.5b: animated decorations (torches, fire) ──────────────────────
    // Frame is computed from elapsed time so every instance animates automatically.
    for (const SpritePlacement& d : layout.animDecors)
    {
        if (d.defIdx < 0 || d.defIdx >= (int)_defs.animDecors.size()) continue;
        const AnimSpriteDef& anim = _defs.animDecors[d.defIdx];

        if (anim.frames.empty()) continue;
        int frame = 0;
        if ((int)anim.frames.size() > 1 && anim.fps > 0.f)
            frame = (int)(GetTime() * anim.fps) % (int)anim.frames.size();

        Rectangle src = anim.frames[frame];

        // Draw at half scale like static decors, centred in the cell.
        float dScaleX = scaleX * 0.75f;
        float dScaleY = scaleY * 0.75f;
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

    // ── Pass 3: props — the room's top layer. Gameplay passes includeProps=false
    // and calls DrawRoomProps AFTER the player/enemies so they walk behind trees.
    if (includeProps)
        DrawRoomProps(layout, scaleX, scaleY, screenOffset);
}

void TileRenderer::DrawRoomPropsSplit(const RoomLayout& layout, float scaleX, float scaleY,
                                      Vector2 screenOffset, float splitScreenY, bool frontHalf) const
{
    float cellW = 16.f * scaleX;
    float cellH = 16.f * scaleY;

    // A prop belongs to the FRONT half when its bottom edge sits below the
    // split line (the player's feet) — those draw after the player.
    auto inRequestedHalf = [&](int row, float spriteSourceH) -> bool
    {
        float bottomY = screenOffset.y + row * cellH + spriteSourceH * scaleY;
        return (bottomY > splitScreenY) == frontHalf;
    };

    for (const SpritePlacement& p : layout.props)
    {
        if (p.defIdx < 0 || p.defIdx >= (int)_defs.props.size()) continue;
        const Rectangle& src = _defs.props[p.defIdx].src;
        if (!inRequestedHalf(p.row, src.height)) continue;
        float sx = screenOffset.x + p.col * cellW;
        float sy = screenOffset.y + p.row * cellH;
        DrawSpriteScaled(src, sx, sy, scaleX, scaleY);
    }

    for (const SpritePlacement& p : layout.animProps)
    {
        if (p.defIdx < 0 || p.defIdx >= (int)_defs.animProps.size()) continue;
        const AnimPropDef& anim = _defs.animProps[p.defIdx];
        if (anim.frames.empty()) continue;
        if (!inRequestedHalf(p.row, anim.frames[0].height)) continue;

        int fc    = (int)anim.frames.size();
        int frame = (fc > 1 && anim.fps > 0.f)
            ? (int)(GetTime() * anim.fps) % fc : 0;
        float sx = screenOffset.x + p.col * cellW;
        float sy = screenOffset.y + p.row * cellH;
        DrawSpriteScaled(anim.frames[frame], sx, sy, scaleX, scaleY);
    }
}

void TileRenderer::DrawRoomProps(const RoomLayout& layout, float scaleX, float scaleY,
                                 Vector2 screenOffset) const
{
    float cellW = 16.f * scaleX;
    float cellH = 16.f * scaleY;

    // ── Static props ──────────────────────────────────────────────────────────
    for (const SpritePlacement& p : layout.props)
    {
        if (p.defIdx < 0 || p.defIdx >= (int)_defs.props.size()) continue;
        float sx = screenOffset.x + p.col * cellW;
        float sy = screenOffset.y + p.row * cellH;
        DrawSpriteScaled(_defs.props[p.defIdx].src, sx, sy, scaleX, scaleY);
    }

    // ── Animated props (same layer, each frame is a direct rect) ─────────────
    for (const SpritePlacement& p : layout.animProps)
    {
        if (p.defIdx < 0 || p.defIdx >= (int)_defs.animProps.size()) continue;
        const AnimPropDef& anim = _defs.animProps[p.defIdx];
        if (anim.frames.empty()) continue;

        int fc    = (int)anim.frames.size();
        int frame = (fc > 1 && anim.fps > 0.f)
            ? (int)(GetTime() * anim.fps) % fc : 0;

        float sx = screenOffset.x + p.col * cellW;
        float sy = screenOffset.y + p.row * cellH;
        DrawSpriteScaled(anim.frames[frame], sx, sy, scaleX, scaleY);
    }
}

void TileRenderer::DrawTile(TileType type, float screenX, float screenY,
                            float scaleX, float scaleY) const
{
    TileRenderSource source = ResolveTileRenderSource(_defs, type);
    const Texture2D& tex = source.sheet == TileRenderSheet::Ground ? _groundSheet
                         : source.sheet == TileRenderSheet::SharedReward ? _sharedRewardSheet
                         : _sheet;
    if (tex.id == 0) return;
    Rectangle src = source.src;
    Rectangle dst{ screenX, screenY, src.width * scaleX, src.height * scaleY };
    DrawTexturePro(tex, src, dst, {}, 0.f, WHITE);
}

void TileRenderer::DrawSpriteScaled(Rectangle src, float screenX, float screenY,
                                    float scaleX, float scaleY) const
{
    if (_sheet.id == 0) return;
    Rectangle dst{ screenX, screenY, src.width * scaleX, src.height * scaleY };
    DrawTexturePro(_sheet, src, dst, {}, 0.f, WHITE);
}
