#pragma once

#include "raylib.h"
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
//  MapScaleMode
//
//  Controls how the game world is sized relative to the player's screen.
//  Swap the mode in WorldConfig to instantly change the feel for any resolution
//  — 720p, 1080p, 4K, phone portrait, phone landscape, etc.
// ─────────────────────────────────────────────────────────────────────────────
enum class MapScaleMode
{
    // Scale until the map FILLS the screen on both axes.
    // The longer map dimension will overflow the screen slightly, allowing a
    // small amount of camera scrolling. No black void is ever visible.
    // Recommended default — keeps action tight on every resolution.
    FillScreen,

    // Scale until the ENTIRE map fits inside the screen.
    // One axis will match the screen exactly; the other may have empty space
    // (centred automatically). Good for "overview" / top-down layouts.
    FitToScreen,

    // Scale so the map WIDTH exactly matches the screen width.
    // The map may be taller or shorter than the screen; scroll on Y if taller.
    FitWidth,

    // Scale so the map HEIGHT exactly matches the screen height.
    // The map may be wider or narrower than the screen; scroll on X if wider.
    FitHeight,

    // Ignore the screen size entirely and use a developer-supplied scale factor.
    // Set worldConfig.fixedScale before calling Recalculate().
    // Useful for testing a specific pixel density or locking a retro look.
    Fixed,
};


// ─────────────────────────────────────────────────────────────────────────────
//  WorldConfig
//
//  Single source of truth for how the game world scales to the screen.
//
//  HOW TO USE
//  ──────────
//  1. Set `mode` (and optionally `fixedScale`, `minScale`, `maxScale`).
//  2. Call Recalculate() after loading a map texture OR after a screen resize.
//  3. Sync Engine's _mapScale:  _mapScale = _worldConfig.GetScale();
//  4. Replace hand-written camera clamp with ClampCamera() and GetMapScreenPos().
//
//  MULTIPLE RESOLUTIONS
//  ────────────────────
//  Because Recalculate() reads the live screen size at call time, the game
//  automatically adapts to any window — just call it again on resize.
//  Works for 720p, 1080p, 4K, and phone portrait/landscape without any
//  extra conditional branches in your game code.
// ─────────────────────────────────────────────────────────────────────────────
class WorldConfig
{
public:

    // ── Settings — tweak these to control how the world is sized ─────────────

    // Which scaling strategy to apply. See MapScaleMode above.
    MapScaleMode mode = MapScaleMode::FillScreen;

    // Only used when mode == MapScaleMode::Fixed.
    // For reference: Map.png at 3.f gives a 2880×1920 world on 1080p.
    float fixedScale = 3.f;

    // Hard floor on the computed scale.
    // Prevents the world from becoming microscopic on very large screens.
    float minScale = 1.0f;

    // Hard ceiling on the computed scale.
    // Prevents aggressive pixelation on very small screens (e.g. low-DPI phones).
    float maxScale = 8.0f;


    // ── Core method ──────────────────────────────────────────────────────────

    // Call this every time the map texture changes (biome swap) or the
    // window is resized. All getters below are only valid after this call.
    void Recalculate(const Texture2D& mapTex, int screenW, int screenH);


    // ── Computed getters ─────────────────────────────────────────────────────

    // The computed scale factor — pass this directly to DrawTextureEx as its
    // scale argument, and use it anywhere you previously wrote _mapScale.
    float GetScale()  const { return _scale;  }

    // World width and height in pixels (mapTex.width * scale).
    float GetWorldW() const { return _worldW; }
    float GetWorldH() const { return _worldH; }

    // True when the world fits entirely inside the screen on each axis.
    // Used internally by ClampCamera to decide whether to scroll or centre.
    bool IsNarrowerThanScreen() const { return _worldW < (float)_screenW; }
    bool IsShorterThanScreen()  const { return _worldH < (float)_screenH; }


    // ── Camera helpers ───────────────────────────────────────────────────────

    // Returns the clamped camera world-position for this frame.
    //
    // Behaviour:
    //  • If the world is LARGER than the screen on an axis → clamp so the
    //    camera cannot scroll past the map edge (prevents black void).
    //  • If the world is SMALLER than the screen on an axis → centre the
    //    camera on the map so there is no visible void or offset.
    //
    // Pass the result straight into _cameraPos.
    Vector2 ClampCamera(Vector2 playerWorldPos, int screenW, int screenH) const;

    // Returns the screen-space top-left position of the map texture.
    // Pass the result straight into _mapPos.
    // Always call this AFTER ClampCamera with the clamped camera position.
    Vector2 GetMapScreenPos(Vector2 cameraPos, int screenW, int screenH) const;


private:

    float _scale  = 3.f;     // computed by Recalculate()
    float _worldW = 2880.f;  // _scale * mapTex.width
    float _worldH = 1920.f;  // _scale * mapTex.height
    int   _screenW = 1920;   // cached screen width  from last Recalculate()
    int   _screenH = 1080;   // cached screen height from last Recalculate()
};
