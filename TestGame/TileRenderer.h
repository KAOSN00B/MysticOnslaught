#pragma once
#include "TileDefs.h"
#include "RoomLayout.h"
#include "raylib.h"
#include <string>

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
    void Init(const char* tilesheetPath, const char* groundSheetPath,
              const char* sharedRewardSheetPath, const TileDefSet& defs);
    void Unload();

    // Draw every tile in the layout.
    // scaleX/scaleY: display pixels per source pixel on each axis.
    // Pass the same value for both if you want square tiles.
    // screenOffset: top-left screen position of the room.
    // includeProps=false skips the prop passes so gameplay can draw them AFTER
    // the player/enemies via DrawRoomProps (lets the player walk behind trees).
    void DrawRoom(const RoomLayout& layout, float scaleX, float scaleY,
                  Vector2 screenOffset, bool includeProps = true) const;

    // Just the prop + animated-prop passes (the room's top layer).
    void DrawRoomProps(const RoomLayout& layout, float scaleX, float scaleY,
                       Vector2 screenOffset) const;

    // Y-sorted split for gameplay: draws only the props whose BOTTOM edge is
    // above (frontHalf=false) or below (frontHalf=true) splitScreenY — the
    // player's feet. Back props draw before the player, front props after, so
    // standing behind a tree tucks the player under its canopy.
    void DrawRoomPropsSplit(const RoomLayout& layout, float scaleX, float scaleY,
                            Vector2 screenOffset, float splitScreenY, bool frontHalf) const;

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
    Texture2D  _sharedRewardSheet{};
    TileDefSet _defs{};
    mutable std::string _activeRoomAnimationId;
    mutable double _roomAnimationStart = 0.0;
};
