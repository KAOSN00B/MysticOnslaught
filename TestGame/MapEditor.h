#pragma once

#include "VillageMap.h"
#include "raylib.h"
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// MapEditor — dev tool (opened from the main menu with the V key) for painting
// the hub village and building interiors by hand, in layers.
//
// Left panel  = the tilesheet palette. Click a tile to pick a 1×1 brush, or
//               DRAG a rectangle to pick a multi-tile stamp (how whole houses
//               are placed in one click). Number keys / tab buttons switch
//               between the loaded sheets (Ground / Village / Interior...).
// Right side  = the map canvas. Left-click paints the current stamp on the
//               active layer; right-click erases. On the COLLISION layer,
//               left-click marks cells solid and right-click clears them.
//
//   Q / W / E / R   active layer: Ground / Objects / Overhead / Collision
//   1..5            palette sheet tabs
//   Mouse wheel     canvas zoom (over canvas)  /  palette scroll (over palette)
//   Middle-drag / arrows   pan the canvas
//   F               flood-fill the hovered region (ground-style fills)
//   G               toggle grid       C   toggle collision overlay
//   S               save              N   new blank map (with confirm)
//   H               controls overlay  ESC exit (auto-prompts unsaved changes)
// ─────────────────────────────────────────────────────────────────────────────
class MapEditor
{
public:
    void Init();
    void Update();
    void Draw() const;
    void Unload();
    bool WantsToExit() const { return _wantsToExit; }

private:
    // A brush is a rectangle of tiles picked from one sheet (1×1 for a single
    // tile; larger for house/tree stamps).
    struct Brush
    {
        short sheet = 0;
        short col = 0, row = 0;   // top-left tile within the sheet
        short w = 1,   h = 1;     // size in tiles
    };

    void LoadSheets();
    void UpdatePalette(Rectangle panel);
    void UpdateCanvas(Rectangle canvas);
    void PaintAt(int cellCol, int cellRow);
    void EraseAt(int cellCol, int cellRow);
    void FloodFillAt(int cellCol, int cellRow);
    void DrawPalette(Rectangle panel) const;
    void DrawCanvas(Rectangle canvas) const;
    void DrawStatusBar() const;
    void DrawHelp() const;

    // Canvas coordinate helpers.
    float   TilePx() const { return VillageMap::kTileSize * _zoom; }
    Vector2 MapOrigin(Rectangle canvas) const
    { return Vector2{ canvas.x - _cameraPan.x, canvas.y - _cameraPan.y }; }

    // ── Sheets / palette ──
    std::vector<Texture2D>   _sheets;
    std::vector<std::string> _sheetNames;
    int     _activeSheet    = 0;
    Vector2 _paletteScroll{};          // scroll offset within the palette view
    bool    _paletteDragging = false;  // rubber-band stamp selection in progress
    Vector2 _paletteDragStartTile{};   // tile coords where the drag began

    // ── Editing state ──
    VillageMap        _map;
    std::string       _mapName = "village";
    VillageMap::Layer _activeLayer = VillageMap::Layer::Ground;
    bool              _collisionMode = false;   // R tab: painting solid flags
    Brush             _brush;
    bool              _dirty = false;           // unsaved changes

    // ── Canvas view ──
    float   _zoom = 2.f;               // screen pixels per source pixel
    Vector2 _cameraPan{};              // map-pixel offset of the view
    bool    _panning = false;
    Vector2 _panMouseStart{};
    Vector2 _panCamStart{};

    // ── UI state ──
    bool        _wantsToExit  = false;
    bool        _confirmingExit = false;   // unsaved-changes prompt active
    bool        _confirmingNew  = false;   // new-map prompt active
    bool        _showGrid      = true;
    bool        _showCollision = true;
    bool        _helpOpen      = false;
    std::string _status;                   // bottom-bar message ("Saved." etc.)
    float       _statusTimer = 0.f;
};
