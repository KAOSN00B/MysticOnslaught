#pragma once
#include "TileDefs.h"
#include "RoomLayout.h"
#include "raylib.h"

// ── TileRenderer ──────────────────────────────────────────────────────────────
// Draws a RoomLayout to the screen using a tilesheet and a TileDefSet.
//
// Usage:
//   _tileRenderer.Init(tilesheetPath, defs);
//   _tileRenderer.DrawRoom(layout, 3.f, offset);
// ─────────────────────────────────────────────────────────────────────────────
class TileRenderer
{
public:
    void Init(const char* tilesheetPath, const TileDefSet& defs);
    void Unload();

    // Draw every tile in the layout.
    // drawScale: display pixels per source pixel (e.g. 3 = 48px per 16px tile).
    // screenOffset: top-left screen position of the room.
    void DrawRoom(const RoomLayout& layout, float drawScale,
                  Vector2 screenOffset) const;

    bool IsLoaded() const { return _sheet.id != 0; }

    // Pixel size of one room at the given draw scale.
    static constexpr float RoomPixelW(float scale)
    { return RoomLayout::kCols * 16.f * scale; }
    static constexpr float RoomPixelH(float scale)
    { return RoomLayout::kRows * 16.f * scale; }

private:
    void DrawTile(TileType type, float screenX, float screenY,
                  float drawScale) const;

    Texture2D  _sheet{};
    TileDefSet _defs{};
};
