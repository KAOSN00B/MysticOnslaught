#pragma once
#include "raylib.h"
#include <string>
#include <vector>

// ── NineSliceEditor ───────────────────────────────────────────────────────────
// Debug tool for visually setting nine-slice values on any PNG in the UI/
// folder. Accessible from the main menu.
//
// Screen 1 — File list: scroll/filter list, click a row then "Open".
// Screen 2 — Editor:
//   Left panel  — source texture with four independently draggable yellow
//                 grid lines (top / bottom / left / right).
//   Right panel — three live previews at different sizes.
//   Bottom bar  — drag-float controls for srcTop/srcBot/srcLeft/srcRight/dst.
//   [Save]      — writes nineslice_<stem>.txt next to the PNG.
//
// ESC from either screen goes back one level.
// ─────────────────────────────────────────────────────────────────────────────
class NineSliceEditor
{
public:
    void Init(const char* assetsRoot);
    void Unload();
    ~NineSliceEditor() { Unload(); }
    void Update();
    void Draw() const;

    bool WantsToExit() const { return _wantsToExit; }

private:
    // ── File entry ────────────────────────────────────────────────────────────
    struct PngFile
    {
        std::string fullPath;
        std::string relName;
        std::string stem;
        float srcTop   = 16.f;
        float srcBot   = 16.f;
        float srcLeft  = 16.f;
        float srcRight = 16.f;
        float dstCorner = 16.f;
        bool  hasSave   = false;
    };

    enum class Screen { FileSelect, Editor };

    // ── File select ───────────────────────────────────────────────────────────
    void ScanFolder(const char* root);
    void UpdateFileSelect();
    void DrawFileSelect() const;
    void OpenSelected();

    // ── Editor ────────────────────────────────────────────────────────────────
    void UpdateEditor();
    void DrawEditor() const;
    void DrawTexPanel(Rectangle area) const;
    void DrawPreviews(Rectangle area) const;
    void DrawBottomBar(Rectangle area) const;

    void Save();
    void TryLoad(PngFile& f);
    std::string SavePath(const PngFile& f) const;

    bool DragFloat(float& val, float minV, float maxV,
                   float speed, Rectangle hitRect,
                   bool& dragging, float& dragStartX, float& dragStartVal);

    // ── State ─────────────────────────────────────────────────────────────────
    Screen _screen      = Screen::FileSelect;
    bool   _wantsToExit = false;
    std::string _assetsRoot;

    std::vector<PngFile> _files;
    int   _selectedIdx  = -1;
    float _listScrollY  = 0.f;
    char  _filterBuf[128] = {};

    // Editor state
    Texture2D _tex{};
    int       _openIdx  = -1;
    float _srcTop   = 16.f;
    float _srcBot   = 16.f;
    float _srcLeft  = 16.f;
    float _srcRight = 16.f;
    float _dstCorner = 16.f;

    // Texture view
    float   _texScale = 1.f;
    float   _texOffX  = 0.f;
    float   _texOffY  = 0.f;
    bool    _midDrag  = false;
    Vector2 _midDragStart{};
    Vector2 _midDragOff{};

    // Grid-line drag — which line: 0=top 1=bot 2=left 3=right -1=none
    int   _lineDragIdx     = -1;
    float _lineDragStartX  = 0.f;
    float _lineDragStartY  = 0.f;
    float _lineDragOrigVal = 0.f;

    // Bottom-bar drag-float state (one per src edge + dst)
    // Index: 0=top 1=bot 2=left 3=right 4=dst
    bool  _barDrag[5]     = {};
    float _barDragX[5]    = {};
    float _barDragVal[5]  = {};

    static constexpr float kTopBarH    = 52.f;
    static constexpr float kBotBarH    = 86.f;   // taller to fit 5 controls
    static constexpr float kTexFrac    = 0.44f;
    static constexpr float kListRowH   = 46.f;
    static constexpr float kListRowGap = 6.f;
};
