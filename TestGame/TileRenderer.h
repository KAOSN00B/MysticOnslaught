#pragma once
#include "TileDefs.h"
#include "RoomLayout.h"
#include "raylib.h"

// ── TileRenderer ──────────────────────────────────────────────────────────────
// Draws a RoomLayout to the screen using a tilesheet and a TileDefSet.
//
// Usage:
//   _tileRenderer.Init(tilesheetPath, defs);
//   _tileRenderer.DrawRoom(layout, scaleX, scaleY, offset);
// ─────────────────────────────────────────────────────────────────────────────
class TileRenderer
{
public:
    // groundSheetPath: path to Ground TIles.png, always drawn for tiles flagged fromGround.
    void Init(const char* tilesheetPath, const char* groundSheetPath, const TileDefSet& defs);
    void Unload();

    // Draw every tile in the layout.
    // scaleX/scaleY: display pixels per source pixel on each axis.
    // Pass the same value for both if you want square tiles.
    // screenOffset: top-left screen position of the room.
    void DrawRoom(const RoomLayout& layout, float scaleX, float scaleY,
                  Vector2 screenOffset) const;

    bool IsLoaded() const { return _sheet.id != 0; }

    // Pixel size of one room at the given draw scales.
    static constexpr float RoomPixelW(float scaleX)
    { return RoomLayout::kCols * 16.f * scaleX; }
    static constexpr float RoomPixelH(float scaleY)
    { return RoomLayout::kRows * 16.f * scaleY; }

private:
    void DrawTile(TileType type, float screenX, float screenY,
                  float scaleX, float scaleY) const;
    void DrawSpriteScaled(Rectangle src, float screenX, float screenY,
                          float scaleX, float scaleY) const;

    Texture2D  _sheet{};
    Texture2D  _groundSheet{};
    TileDefSet _defs{};
};
