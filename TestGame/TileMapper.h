#pragma once
#include "raylib.h"
#include <string>
#include <vector>

// ── TileMapper ────────────────────────────────────────────────────────────────
// Two-screen debug tool for building per-tileset tile definitions.
//
// Screen 1 — File Select
//   Scans the MapTilesets folder for PNG files and lists them.
//   Click a file to open it.  Use the biome buttons to assign which
//   in-game biome that tileset belongs to.  Click "Open Mapper" to proceed.
//
// Screen 2 — Tile Assignment
//   Displays the tilesheet with a 16×16 pixel grid overlay.
//   Click or drag to select a region, then click a tile type in the right
//   panel to assign it.  Press [S] to export + save.  [ESC] returns to
//   the file list.
//
// Save files live next to the game exe: tilemapper_<stem>.txt
// Each file stores the biome name on line 1, then one tile assignment per line.
// ─────────────────────────────────────────────────────────────────────────────
class TileMapper
{
public:
    void Init(const char* folderPath);
    void Unload();
    void Update();
    void Draw() const;

    bool WantsToExit() const { return _wantsToExit; }

    // ── Tile types ────────────────────────────────────────────────────────────
    static constexpr const char* kTypeNames[] = {
        "Floor",               // 0
        "Floor Variant",       // 1
        "Wall Body",           // 2
        "Wall Top Face",       // 3
        "Wall Corner TL",      // 4
        "Wall Corner TR",      // 5
        "Wall Inner Corner L", // 6
        "Wall Inner Corner R", // 7
        "Void",                // 8
        "Door Open",           // 9
        "Door Locked",         // 10
        "Boss Key",            // 11
        "Chest Closed",        // 12
        "Chest Open",          // 13
        "Wall Left",           // 14
        "Wall Right",          // 15
        "Wall Bottom",         // 16
        "Wall Corner BL",      // 17
        "Wall Corner BR",      // 18
        "Wall Inner Corner BL",// 19
        "Wall Inner Corner BR",// 20
    };
    static constexpr int kTypeCount = 21;
    static constexpr int kTileSize  = 16;

    // ── Biome names (match the game's Biome enum order) ───────────────────────
    static constexpr const char* kBiomeNames[] = {
        "Ancient Castle", "Caverns", "Demons Insides", "Dream Realm",
        "Forest", "Graveyard", "Jungle", "Lost City", "The Sanctuary", "Wastelands"
    };
    static constexpr int kBiomeCount = 10;

    // Gap between main tileset and Ground TIles in the sheet view (in source pixels).
    static constexpr int kGroundGap = 24;

    static const Color kTypeColors[kTypeCount];

private:
    // ── Internal state ────────────────────────────────────────────────────────
    enum class Screen    { FileSelect, Mapping };
    enum class PanelTab  { Tiles, Props, Decors };

    struct Assignment
    {
        int col = 0, row = 0, spanCols = 1, spanRows = 1;
        int typeIdx  = -1;
        bool fromGround = false;  // true = selection came from Ground TIles.png
    };

    // A prop sprite + its collision box (source-pixel coords, relative to src top-left).
    struct PropDef { Rectangle src; Rectangle collision; };

    // An animated prop — each frame is a separate drag-selected source rectangle.
    struct AnimPropDef { std::vector<Rectangle> frames; Rectangle collision; float fps; };

    // An animated decoration — each frame is a separately drag-selected source rectangle.
    struct AnimDecorDef { std::vector<Rectangle> frames; float fps; };

    // Eight handles used by the collision-box editor.
    enum class CollHandle { None, TL, TC, TR, ML, MR, BL, BC, BR, Body };

    struct TilesetFile
    {
        std::string fullPath;   // absolute path to the PNG
        std::string stem;       // filename without extension (used for save file)
        int         biomeIdx = 0;  // which biome this tileset belongs to
        bool        hasSave  = false;
    };


    // ── File select ───────────────────────────────────────────────────────────
    void ScanFolder(const char* folderPath);
    void UpdateFileSelect();
    void DrawFileSelect() const;
    void OpenSelectedFile();

    // ── Mapping ───────────────────────────────────────────────────────────────
    void LoadSheet(const std::string& path);
    void UpdateMapping();
    void DrawMapping() const;

    void HandleMouseMapping();
    void DrawSheet()       const;
    void DrawGrid()        const;
    void DrawAssignments() const;
    void DrawSelection()   const;
    void DrawPanel()       const;

    // Collision-box editor for a selected prop.
    void GetCollEditorLayout(float& offX, float& offY, float& zoom) const;
    void HandleCollisionEditorMouse();
    void DrawCollisionEditor() const;

    void ConfirmSelection(int typeIdx);
    void ClearSelection();
    void ExportAndSave()   const;
    void TryLoadSave(TilesetFile& file);
    void TryLoadSave();

    std::string SavePath() const;

    Vector2   ScreenToGrid(Vector2 screen) const;
    Rectangle GridToScreen(int col, int row, int spanCols, int spanRows) const;
    float     GroundSheetScreenY() const;   // top-left Y of the ground sheet in screen space
    Rectangle GridToGroundScreen(int col, int row, int spanCols, int spanRows) const;

    // ── State ─────────────────────────────────────────────────────────────────
    Screen   _screen    = Screen::FileSelect;
    PanelTab _panelTab  = PanelTab::Tiles;
    bool     _wantsToExit = false;

    // File select state
    std::vector<TilesetFile> _files;
    int   _selectedFileIdx  = -1;   // highlighted in the list
    float _fileListScrollY  = 0.f;

    // Mapping state
    Texture2D   _sheet{};
    Texture2D   _groundSheet{};   // Ground TIles.png — always loaded, shown below main sheet
    float _scale    = 1.f;
    float _minScale = 1.f;        // fit-to-view scale; zoom cannot go below this
    float _offX     = 0.f;
    float _offY     = 0.f;
    int   _sheetCols  = 0;
    int   _sheetRows  = 0;
    int   _groundCols = 0;
    int   _groundRows = 0;
    float _panelX     = 0.f;

    // Middle-mouse pan
    bool    _middleDragging        = false;
    Vector2 _middleDragStart       = {};
    Vector2 _middleDragOffsetStart = {};

    bool _isDragging   = false;
    int  _dragC0 = 0, _dragR0 = 0;
    int  _dragC1 = 0, _dragR1 = 0;

    bool _hasSelection   = false;
    bool _selFromGround  = false;   // selection was made on Ground TIles sheet
    bool _dragFromGround = false;   // current drag is on Ground TIles sheet
    int  _selC0 = 0, _selR0 = 0;
    int  _selC1 = 0, _selR1 = 0;

    int  _hoveredTypeIdx = -1;

    std::vector<Assignment> _assignments;

    // Props and decorations defined for this tileset.
    std::vector<PropDef>      _propDefs;
    std::vector<AnimPropDef>  _animPropDefs;
    std::vector<Rectangle>    _decorDefs;
    std::vector<AnimDecorDef> _animDecorDefs;
    float _propScrollY      = 0.f;
    float _decorScrollY     = 0.f;
    float _animDecorFps     = 8.f;  // fps for the anim decor being built
    float _animPropFps      = 8.f;  // fps for the anim prop being built

    // Frames accumulated during anim prop/decor building; cleared after Finalize or Clear.
    std::vector<Rectangle> _pendingAnimPropFrames;
    std::vector<Rectangle> _pendingAnimDecorFrames;

    // Collision-box editor state.
    // Exactly one of _editingPropIdx or _editingAnimPropIdx is >= 0 at a time.
    int        _editingPropIdx     = -1;
    int        _editingAnimPropIdx = -1;
    CollHandle _collHandle     = CollHandle::None;
    bool       _collDragging   = false;
    Vector2    _collDragStart  = {};
    Rectangle  _collDragOrig   = {};

    // Index into _files for the sheet currently open
    int _openFileIdx = -1;

    static constexpr float kPanelFrac = 0.30f;
};
