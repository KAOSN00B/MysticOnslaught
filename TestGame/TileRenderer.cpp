#include "TileRenderer.h"
#include "RoomAssetCatalog.h"
#include "RoomCollision.h"

#include <algorithm>
#include <cmath>

namespace
{
    // Snap a sprite's destination to integer pixels so adjacent tiles share an
    // EXACT edge (tile A's right = tile B's left). Without this, fractional draw
    // scale leaves a 1px seam that shows whatever is underneath (floor/fall/bg).
    Rectangle SnapTileDest(float screenX, float screenY, float w, float h)
    {
        const float x0 = std::floor(screenX);
        const float y0 = std::floor(screenY);
        return { x0, y0, std::floor(screenX + w) - x0, std::floor(screenY + h) - y0 };
    }
}

namespace
{
    int PlaybackFrame(int frameCount, float fps, AnimPlaybackMode mode, double elapsed)
    {
        if (frameCount <= 1 || fps <= 0.f) return 0;
        const int tick = std::max(0, (int)(elapsed * fps));
        if (mode == AnimPlaybackMode::PlayOnce)
            return std::min(tick, frameCount - 1);
        if (mode == AnimPlaybackMode::PingPong)
        {
            const int cycle = frameCount * 2 - 2;
            const int phase = tick % cycle;
            return phase < frameCount ? phase : cycle - phase;
        }
        return tick % frameCount;
    }
}

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
    for (LoadedRoomSource& source : _roomSources)
        if (source.texture.id != 0) UnloadTexture(source.texture);
    _roomSources.clear();
}

void TileRenderer::LoadRoomAssetCatalog(const RoomAssetCatalog& catalog)
{
    for (LoadedRoomSource& source : _roomSources)
        if (source.texture.id != 0) UnloadTexture(source.texture);
    _roomSources.clear();
    for (const RoomAssetSource& source : catalog.Sources())
    {
        Texture2D texture = LoadTexture(source.imagePath.string().c_str());
        if (texture.id != 0) _roomSources.push_back({ source.stem, texture });
    }
}

void TileRenderer::DrawRoom(const RoomLayout& layout, float scaleX, float scaleY,
                            Vector2 screenOffset, bool includeProps) const
{
    const std::string animationId = layout.handcrafted ? layout.sourceRoomId : std::string{};
    if (animationId != _activeRoomAnimationId)
    {
        _activeRoomAnimationId = animationId;
        _roomAnimationStart = GetTime();
    }
    const double animationElapsed = GetTime() - _roomAnimationStart;
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

    // Draw one decor / anim-decor placement at FULL tile scale, anchored at the
    // cell origin — a 16px source exactly fills one tile (water/lava no longer
    // shrink). Used for every band so the look is identical wherever it's painted.
    auto drawDecor = [&](const SpritePlacement& d, bool anim)
    {
        const TileDefSet* definitions = ResolveRoomDefinitions(layout, d, _defs);
        if (definitions == nullptr || d.defIdx < 0) return;
        Rectangle src{};
        std::string stem = d.sourceTileset;
        if (anim)
        {
            if (d.defIdx >= (int)definitions->animDecors.size()) return;
            const AnimSpriteDef& a = definitions->animDecors[d.defIdx];
            if (a.frames.empty()) return;
            int frame = 0;
            if ((int)a.frames.size() > 1 && a.fps > 0.f)
                frame = PlaybackFrame((int)a.frames.size(), a.fps, a.playback, animationElapsed);
            src = a.frames[frame];
            if (!a.sourceSheet.empty()) stem = a.sourceSheet;
        }
        else
        {
            if (d.defIdx >= (int)definitions->decors.size()) return;
            const SpriteDef& def = definitions->decors[d.defIdx];
            src = def.src;
            if (!def.sourceSheet.empty()) stem = def.sourceSheet;
        }
        DrawSpriteScaled(src, screenOffset.x + d.col * cellW,
                         screenOffset.y + d.row * cellH, scaleX, scaleY,
                         FindRoomSourceTexture(stem));
    };

    // ── Ground band: authored floor overlays + decors painted "as ground" ─────
    // (animated water/lava live here, just above the floor, below every wall).
    for (const RoomTilePlacement& visual : layout.visualTiles)
    {
        if (!visual.ground) continue;
        const Texture2D* texture = FindRoomSourceTexture(visual.sourceTileset);
        if (texture == nullptr) continue;
        DrawSpriteScaled(visual.src,
            screenOffset.x + visual.col * cellW,
            screenOffset.y + visual.row * cellH,
            scaleX, scaleY, texture);
    }
    for (const SpritePlacement& d : layout.decors)
        if (d.band == RoomDrawBand::Ground) drawDecor(d, false);
    for (const SpritePlacement& d : layout.animDecors)
        if (d.band == RoomDrawBand::Ground) drawDecor(d, true);

    // ── Visual band: terrain walls + authored visual wall tiles + decor-as-visual
    for (int r = 0; r < RoomLayout::kRows; r++)
    {
        for (int c = 0; c < RoomLayout::kCols; c++)
        {
            TileType type = layout.tiles[r][c];
            if (type == TileType::Floor     || type == TileType::FloorVariant ||
                type == TileType::Void      || type == TileType::None         ||
                type == TileType::DoorOpen  || type == TileType::DoorLocked)
                continue;

            // A terrain wall tile inside an open Door Zone is hidden so the floor
            // base drawn in Pass 1 shows through the opening — same rule the
            // authored visual wall layer follows below.
            if (RoomPlacementClearsAtDoor({ (float)c, (float)r, 1.f, 1.f }, layout))
                continue;

            float sx = screenOffset.x + c * cellW;
            float sy = screenOffset.y + r * cellH;
            DrawTile(type, sx, sy, scaleX, scaleY);
        }
    }

    // Non-ground art is the destructible visual wall layer. Door clear zones
    // remove only this pass, never the floor underneath it.
    for (const RoomTilePlacement& visual : layout.visualTiles)
    {
        if (visual.ground) continue;
        if (RoomVisualClearedByOpenDoor(visual, layout)) continue;
        const Texture2D* texture = FindRoomSourceTexture(visual.sourceTileset);
        if (texture == nullptr) continue;
        DrawSpriteScaled(visual.src,
            screenOffset.x + visual.col * cellW,
            screenOffset.y + visual.row * cellH,
            scaleX, scaleY, texture);
    }
    for (const SpritePlacement& d : layout.decors)
        if (d.band == RoomDrawBand::Visual) drawDecor(d, false);
    for (const SpritePlacement& d : layout.animDecors)
        if (d.band == RoomDrawBand::Visual) drawDecor(d, true);

    // ── Decor band: normal floor decor, above walls, y-sorted so a lower sprite
    // draws over a higher one (typical top-down overlap).
    {
        struct DecorItem { const SpritePlacement* placement; bool anim; float bottomY; };
        std::vector<DecorItem> items;
        auto queue = [&](const SpritePlacement& d, bool anim, float srcHeight)
        {
            if (d.band != RoomDrawBand::Decor) return;
            items.push_back({ &d, anim,
                              screenOffset.y + d.row * cellH + srcHeight * scaleY });
        };
        for (const SpritePlacement& d : layout.decors)
        {
            const TileDefSet* defs = ResolveRoomDefinitions(layout, d, _defs);
            float h = 16.f;
            if (defs != nullptr && d.defIdx >= 0 && d.defIdx < (int)defs->decors.size())
                h = defs->decors[d.defIdx].src.height;
            queue(d, false, h);
        }
        for (const SpritePlacement& d : layout.animDecors)
        {
            const TileDefSet* defs = ResolveRoomDefinitions(layout, d, _defs);
            float h = 16.f;
            if (defs != nullptr && d.defIdx >= 0 && d.defIdx < (int)defs->animDecors.size() &&
                !defs->animDecors[d.defIdx].frames.empty())
                h = defs->animDecors[d.defIdx].frames[0].height;
            queue(d, true, h);
        }
        std::sort(items.begin(), items.end(),
                  [](const DecorItem& a, const DecorItem& b) { return a.bottomY < b.bottomY; });
        for (const DecorItem& it : items) drawDecor(*it.placement, it.anim);
    }

    // ── Props band — the room's top layer. Gameplay passes includeProps=false
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
        const TileDefSet* definitions = ResolveRoomDefinitions(layout, p, _defs);
        if (definitions == nullptr || p.defIdx < 0 || p.defIdx >= (int)definitions->props.size()) continue;
        const Rectangle& src = definitions->props[p.defIdx].src;
        if (!inRequestedHalf(p.row, src.height)) continue;
        float sx = screenOffset.x + p.col * cellW;
        float sy = screenOffset.y + p.row * cellH;
        DrawSpriteScaled(src, sx, sy, scaleX, scaleY,
            FindRoomSourceTexture(definitions->props[p.defIdx].sourceSheet.empty()
                ? p.sourceTileset : definitions->props[p.defIdx].sourceSheet));
    }

    for (const SpritePlacement& p : layout.animProps)
    {
        const TileDefSet* definitions = ResolveRoomDefinitions(layout, p, _defs);
        if (definitions == nullptr || p.defIdx < 0 || p.defIdx >= (int)definitions->animProps.size()) continue;
        const AnimPropDef& anim = definitions->animProps[p.defIdx];
        if (anim.frames.empty()) continue;
        if (!inRequestedHalf(p.row, anim.frames[0].height)) continue;

        int fc    = (int)anim.frames.size();
        int frame = PlaybackFrame(fc, anim.fps, anim.playback,
                                  GetTime() - _roomAnimationStart);
        float sx = screenOffset.x + p.col * cellW;
        float sy = screenOffset.y + p.row * cellH;
        DrawSpriteScaled(anim.frames[frame], sx, sy, scaleX, scaleY,
                         FindRoomSourceTexture(anim.sourceSheet.empty()
                            ? p.sourceTileset : anim.sourceSheet));
    }
}

void TileRenderer::DrawRoomProps(const RoomLayout& layout, float scaleX, float scaleY,
                                 Vector2 screenOffset) const
{
    float cellW = 16.f * scaleX;
    float cellH = 16.f * scaleY;

    // Static + animated props share the top band and y-sort together: a prop
    // whose bottom edge is lower on screen draws over one that is higher.
    struct PropItem { const SpritePlacement* placement; bool anim; float bottomY; };
    std::vector<PropItem> items;
    for (const SpritePlacement& p : layout.props)
    {
        const TileDefSet* defs = ResolveRoomDefinitions(layout, p, _defs);
        if (defs == nullptr || p.defIdx < 0 || p.defIdx >= (int)defs->props.size()) continue;
        items.push_back({ &p, false,
            screenOffset.y + p.row * cellH + defs->props[p.defIdx].src.height * scaleY });
    }
    for (const SpritePlacement& p : layout.animProps)
    {
        const TileDefSet* defs = ResolveRoomDefinitions(layout, p, _defs);
        if (defs == nullptr || p.defIdx < 0 || p.defIdx >= (int)defs->animProps.size()) continue;
        if (defs->animProps[p.defIdx].frames.empty()) continue;
        items.push_back({ &p, true,
            screenOffset.y + p.row * cellH + defs->animProps[p.defIdx].frames[0].height * scaleY });
    }
    std::sort(items.begin(), items.end(),
              [](const PropItem& a, const PropItem& b) { return a.bottomY < b.bottomY; });

    for (const PropItem& it : items)
    {
        const SpritePlacement& p = *it.placement;
        const TileDefSet* defs = ResolveRoomDefinitions(layout, p, _defs);
        float sx = screenOffset.x + p.col * cellW;
        float sy = screenOffset.y + p.row * cellH;
        if (!it.anim)
        {
            const SpriteDef& def = defs->props[p.defIdx];
            DrawSpriteScaled(def.src, sx, sy, scaleX, scaleY,
                FindRoomSourceTexture(def.sourceSheet.empty() ? p.sourceTileset : def.sourceSheet));
        }
        else
        {
            const AnimPropDef& anim = defs->animProps[p.defIdx];
            int frame = PlaybackFrame((int)anim.frames.size(), anim.fps, anim.playback,
                                      GetTime() - _roomAnimationStart);
            DrawSpriteScaled(anim.frames[frame], sx, sy, scaleX, scaleY,
                FindRoomSourceTexture(anim.sourceSheet.empty() ? p.sourceTileset : anim.sourceSheet));
        }
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
    Rectangle dst = SnapTileDest(screenX, screenY, src.width * scaleX, src.height * scaleY);
    DrawTexturePro(tex, src, dst, {}, 0.f, WHITE);
}

void TileRenderer::DrawSpriteScaled(Rectangle src, float screenX, float screenY,
                                    float scaleX, float scaleY,
                                    const Texture2D* sourceTexture) const
{
    const Texture2D& texture = sourceTexture != nullptr ? *sourceTexture : _sheet;
    if (texture.id == 0) return;
    Rectangle dst = SnapTileDest(screenX, screenY, src.width * scaleX, src.height * scaleY);
    DrawTexturePro(texture, src, dst, {}, 0.f, WHITE);
}

const Texture2D* TileRenderer::FindRoomSourceTexture(const std::string& stem) const
{
    if (stem.empty()) return nullptr;
    for (const LoadedRoomSource& source : _roomSources)
        if (source.stem == stem) return &source.texture;
    return nullptr;
}
