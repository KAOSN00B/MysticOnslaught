#include "WorldConfig.h"

// ─────────────────────────────────────────────────────────────────────────────
//  WorldConfig::Recalculate
//
//  Computes _scale from the chosen MapScaleMode, then caches the resulting
//  world dimensions. Call this:
//    • After loading a new map texture (biome change).
//    • After a window resize (if you support resizable windows).
// ─────────────────────────────────────────────────────────────────────────────
void WorldConfig::Recalculate(const Texture2D& mapTex, int screenW, int screenH)
{
    _screenW = screenW;
    _screenH = screenH;

    const float texW = (float)mapTex.width;
    const float texH = (float)mapTex.height;

    // Scale ratios: how many times larger than one pixel the screen dimension is.
    const float scaleX = (float)screenW / texW;   // 1920 / 960 = 2.0 on 1080p
    const float scaleY = (float)screenH / texH;   // 1080 / 640 = 1.6875 on 1080p

    switch (mode)
    {
    // Fill: use the LARGER ratio so both axes cover the screen.
    // The map overflows slightly on one axis → slight camera scroll is possible.
    case MapScaleMode::FillScreen:
        _scale = std::max(scaleX, scaleY);
        break;

    // Fit: use the SMALLER ratio so the whole map fits inside the screen.
    // One axis fills the screen exactly; the other is centred with empty space.
    case MapScaleMode::FitToScreen:
        _scale = std::min(scaleX, scaleY);
        break;

    // FitWidth / FitHeight: pin one dimension exactly to the screen edge.
    case MapScaleMode::FitWidth:
        _scale = scaleX;
        break;

    case MapScaleMode::FitHeight:
        _scale = scaleY;
        break;

    // Fixed: use the developer-supplied constant (ignores screen size entirely).
    case MapScaleMode::Fixed:
        _scale = fixedScale;
        break;
    }

    // Guard rails — keep the world within a sensible range regardless of
    // the screen resolution or texture size.
    _scale = std::clamp(_scale, minScale, maxScale);

    // Cache the world dimensions so callers don't have to recompute them.
    _worldW = texW * _scale;
    _worldH = texH * _scale;
}


// ─────────────────────────────────────────────────────────────────────────────
//  WorldConfig::ClampCamera
//
//  Returns a safe camera world-position for this frame.
//
//  On each axis independently:
//    • World > screen  →  clamp so the viewport edge never leaves the map.
//    • World ≤ screen  →  centre the camera on the map (no scrolling needed).
//
//  This replaces the hand-written clamp block that used _windowWidth/Height
//  constants and didn't handle the "map smaller than screen" case.
// ─────────────────────────────────────────────────────────────────────────────
Vector2 WorldConfig::ClampCamera(Vector2 playerWorldPos, int screenW, int screenH) const
{
    const float halfW = (float)screenW * 0.5f;
    const float halfH = (float)screenH * 0.5f;

    Vector2 cam = playerWorldPos;

    // ── Horizontal ───────────────────────────────────────────────────────────
    if (_worldW <= (float)screenW)
    {
        // Map fits inside the screen — lock to centre so it never drifts.
        cam.x = _worldW * 0.5f;
    }
    else
    {
        // Map is wider than the screen — scroll, but clamp to map edges.
        cam.x = std::clamp(cam.x, halfW, _worldW - halfW);
    }

    // ── Vertical ─────────────────────────────────────────────────────────────
    if (_worldH <= (float)screenH)
    {
        // Map fits inside the screen — lock to centre.
        cam.y = _worldH * 0.5f;
    }
    else
    {
        // Map is taller than the screen — scroll, but clamp to map edges.
        cam.y = std::clamp(cam.y, halfH, _worldH - halfH);
    }

    return cam;
}


// ─────────────────────────────────────────────────────────────────────────────
//  WorldConfig::GetMapScreenPos
//
//  Converts the clamped camera world-position into the screen-space top-left
//  of the map texture. This is what goes into _mapPos and is used by every
//  DrawTextureEx(_map, ...) call.
//
//  Formula: mapPos = screenCentre - cameraPos
//  (The camera world-pos is the world point that maps to the screen centre.)
// ─────────────────────────────────────────────────────────────────────────────
Vector2 WorldConfig::GetMapScreenPos(Vector2 cameraPos, int screenW, int screenH) const
{
    return {
        (float)screenW * 0.5f - cameraPos.x,
        (float)screenH * 0.5f - cameraPos.y
    };
}
