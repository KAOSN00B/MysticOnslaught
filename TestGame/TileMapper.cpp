#include "TileMapper.h"
#include "VirtualCanvas.h"
#include "TileDefs.h"
#include "VirtualCanvas.h"
#include "raymath.h"
#include "VirtualCanvas.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

constexpr float kListRowH = 52.f;

// ── Static colour table ───────────────────────────────────────────────────────

const Color TileMapper::kTypeColors[TileMapper::kTypeCount] = {
    Color{ 60, 180,  60, 160 },   // Floor
    Color{ 40, 140,  40, 160 },   // Floor Variant
    Color{120,  80,  40, 160 },   // Wall Body
    Color{160, 110,  60, 160 },   // Wall Top Face
    Color{140,  95,  50, 160 },   // Wall Corner TL
    Color{140,  95,  50, 160 },   // Wall Corner TR
    Color{120,  75,  35, 160 },   // Wall Inner Corner L
    Color{120,  75,  35, 160 },   // Wall Inner Corner R
    Color{ 20,  20,  20, 160 },   // Void
    Color{ 60, 200, 200, 160 },   // Door Open
    Color{200,  60,  60, 160 },   // Door Locked
    Color{255, 215,   0, 160 },   // Boss Key
    Color{150, 100,  50, 160 },   // Chest Closed
    Color{200, 150,  80, 160 },   // Chest Open
    Color{100,  60,  30, 160 },   // Wall Left
    Color{100,  60,  30, 160 },   // Wall Right
    Color{ 80,  50,  25, 160 },   // Wall Bottom
    Color{130,  85,  45, 160 },   // Wall Corner BL
    Color{130,  85,  45, 160 },   // Wall Corner BR
    Color{110,  70,  30, 160 },   // Wall Inner Corner BL
    Color{110,  70,  30, 160 },   // Wall Inner Corner BR
};

// ── Init / Unload ─────────────────────────────────────────────────────────────

void TileMapper::Init(const char* folderPath)
{
    _wantsToExit      = false;
    _screen           = Screen::FileSelect;
    _selectedFileIdx  = -1;
    _openFileIdx      = -1;
    _fileListScrollY  = 0.f;
    _assignments.clear();

    if (_sheet.id != 0) { UnloadTexture(_sheet); _sheet = {}; }

    // Ground TIles.png is always loaded — shown alongside every biome sheet.
    if (_groundSheet.id != 0) { UnloadTexture(_groundSheet); _groundSheet = {}; }
    std::string groundPath = std::string(folderPath) + "/Ground TIles.png";
    _groundSheet = LoadTexture(groundPath.c_str());
    if (_groundSheet.id != 0)
    {
        _groundCols = _groundSheet.width  / kTileSize;
        _groundRows = _groundSheet.height / kTileSize;
    }

    ScanFolder(folderPath);
}

void TileMapper::Unload()
{
    if (_sheet.id != 0)       { UnloadTexture(_sheet);       _sheet       = {}; }
    if (_groundSheet.id != 0) { UnloadTexture(_groundSheet); _groundSheet = {}; }
}

void TileMapper::ScanFolder(const char* folderPath)
{
    _files.clear();

    try
    {
        for (const auto& entry : fs::directory_iterator(folderPath))
        {
            if (!entry.is_regular_file())
                continue;
            if (entry.path().extension() != ".png" &&
                entry.path().extension() != ".PNG")
                continue;

            TilesetFile f;
            f.fullPath = entry.path().string();
            f.stem     = entry.path().stem().string();
            if (f.stem == "Ground TIles") continue;  // always shown separately, not a biome
            f.biomeIdx = 0;

            // Check whether a save file exists for this tileset.
            std::string savePath = "tilemapper_" + f.stem + ".txt";
            FILE* test = nullptr;
            fopen_s(&test, savePath.c_str(), "r");
            if (test)
            {
                f.hasSave = true;
                // Peek at the biome name stored in the save.
                char biomeName[64]{};
                char tag[16]{};
                if (fscanf_s(test, "%15s %63s", tag, (unsigned)sizeof(tag),
                             biomeName, (unsigned)sizeof(biomeName)) == 2)
                {
                    for (int i = 0; i < kBiomeCount; i++)
                    {
                        if (strcmp(biomeName, kBiomeNames[i]) == 0)
                        {
                            f.biomeIdx = i;
                            break;
                        }
                    }
                }
                fclose(test);
            }

            _files.push_back(std::move(f));
        }
    }
    catch (...) {}

    // Sort alphabetically by stem so the list is stable.
    std::sort(_files.begin(), _files.end(),
        [](const TilesetFile& a, const TilesetFile& b) {
            return a.stem < b.stem;
        });
}

// ── Per-frame ─────────────────────────────────────────────────────────────────

void TileMapper::Update()
{
    if (_screen == Screen::FileSelect)
        UpdateFileSelect();
    else
        UpdateMapping();
}

void TileMapper::Draw() const
{
    if (_screen == Screen::FileSelect)
        DrawFileSelect();
    else
        DrawMapping();
}

// ── Screen 1 — File Select ────────────────────────────────────────────────────

void TileMapper::UpdateFileSelect()
{
    if (IsKeyPressed(KEY_ESCAPE))
    {
        _wantsToExit = true;
        return;
    }

    float sw = (float)kVirtualWidth;
    float sh = (float)kVirtualHeight;
    Vector2 mouse = GetVirtualMousePos();

    // Row layout
    float listX  = sw * 0.05f;
    float listY  = 110.f;
    float rowH   = 54.f;
    float rowGap = 8.f;
    float listW  = sw * 0.55f;

    // Scroll with mouse wheel
    float wheel = GetMouseWheelMove();
    _fileListScrollY -= wheel * (rowH + rowGap) * 2.f;
    _fileListScrollY  = std::max(_fileListScrollY, 0.f);

    for (int i = 0; i < (int)_files.size(); i++)
    {
        float ry = listY + i * (rowH + rowGap) - _fileListScrollY;
        if (ry + rowH < listY || ry > sh - 60.f)
            continue;

        Rectangle row{ listX, ry, listW, rowH };

        // Click anywhere on the row to select it.
        if (CheckCollisionPointRec(mouse, row) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            _selectedFileIdx = i;
        }

        // Biome cycle buttons on the right side of each row.
        float btnW = 120.f;
        float btnH = 30.f;
        Rectangle prevBtn{ listX + listW + 12.f,        ry + (rowH - btnH) * 0.5f, 30.f,  btnH };
        Rectangle nextBtn{ listX + listW + 12.f + 30.f + btnW, ry + (rowH - btnH) * 0.5f, 30.f, btnH };

        if (CheckCollisionPointRec(mouse, prevBtn) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            _files[i].biomeIdx = (_files[i].biomeIdx - 1 + kBiomeCount) % kBiomeCount;
            if (i == _selectedFileIdx) {}  // selection stays
        }
        if (CheckCollisionPointRec(mouse, nextBtn) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            _files[i].biomeIdx = (_files[i].biomeIdx + 1) % kBiomeCount;
        }
    }

    // "Open" button at the bottom
    if (_selectedFileIdx >= 0 && _selectedFileIdx < (int)_files.size())
    {
        Rectangle openBtn{ sw * 0.5f - 130.f, sh - 72.f, 260.f, 52.f };
        if (CheckCollisionPointRec(mouse, openBtn) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            OpenSelectedFile();
        }
    }
}

void TileMapper::DrawFileSelect() const
{
    ClearBackground(Color{ 14, 14, 20, 255 });

    float sw = (float)kVirtualWidth;
    float sh = (float)kVirtualHeight;

    DrawText("TILE MAPPER  —  Select a Tileset", 40, 18, 28, GOLD);
    DrawText("[ESC] Back to Menu", 40, 54, 16, Fade(WHITE, 0.45f));

    if (_files.empty())
    {
        DrawText("No PNG files found in the MapTilesets folder.", 40, 120, 22, RED);
        return;
    }

    // Column headers
    float listX = sw * 0.05f;
    float listW = sw * 0.55f;
    DrawText("File",  (int)(listX + 10.f), 86, 16, Fade(WHITE, 0.5f));
    DrawText("Biome", (int)(listX + listW + 42.f), 86, 16, Fade(WHITE, 0.5f));

    float listY  = 110.f;
    float rowH   = 54.f;
    float rowGap = 8.f;

    for (int i = 0; i < (int)_files.size(); i++)
    {
        float ry = listY + i * (rowH + rowGap) - _fileListScrollY;
        if (ry + rowH < listY || ry > sh - 60.f)
            continue;

        bool selected = (i == _selectedFileIdx);
        Rectangle row{ listX, ry, listW, rowH };

        Color bg  = selected ? Color{ 35, 90, 120, 220 } : Color{ 22, 22, 30, 200 };
        Color rim = selected ? Color{100, 200, 255, 255 } : Fade(WHITE, 0.12f);
        DrawRectangleRounded(row, 0.18f, 6, bg);
        DrawRectangleRoundedLines(row, 0.18f, 6, rim);

        // File name
        int fs = 20;
        DrawText(_files[i].stem.c_str(),
            (int)(listX + 14.f), (int)(ry + rowH * 0.5f - fs * 0.5f), fs, RAYWHITE);

        // "Already mapped" badge
        if (_files[i].hasSave)
        {
            const char* badge = "mapped";
            int bw = MeasureText(badge, 13);
            DrawRectangleRounded(
                { listX + listW - bw - 24.f, ry + 8.f, (float)(bw + 16), 22.f },
                0.4f, 4, Color{ 40, 140, 70, 200 });
            DrawText(badge, (int)(listX + listW - bw - 16.f), (int)(ry + 12.f), 13, WHITE);
        }

        // Biome selector:  <  [BiomeName]  >
        float btnW   = 120.f;
        float btnH   = 30.f;
        float selectorX = listX + listW + 12.f;
        float selectorY = ry + (rowH - btnH) * 0.5f;

        Rectangle prevBtn{ selectorX,                   selectorY, 30.f,  btnH };
        Rectangle biomeBox{ selectorX + 30.f,           selectorY, btnW,  btnH };
        Rectangle nextBtn{ selectorX + 30.f + btnW,     selectorY, 30.f,  btnH };

        DrawRectangleRounded(prevBtn,  0.3f, 4, Fade(WHITE, 0.12f));
        DrawRectangleRounded(biomeBox, 0.3f, 4, Fade(WHITE, 0.08f));
        DrawRectangleRounded(nextBtn,  0.3f, 4, Fade(WHITE, 0.12f));
        DrawRectangleLinesEx(biomeBox, 1.f, Fade(WHITE, 0.20f));

        DrawText("<", (int)(prevBtn.x + prevBtn.width * 0.5f - 5.f),
                 (int)(selectorY + btnH * 0.5f - 9.f), 18, RAYWHITE);
        DrawText(">", (int)(nextBtn.x + nextBtn.width * 0.5f - 5.f),
                 (int)(selectorY + btnH * 0.5f - 9.f), 18, RAYWHITE);

        const char* biomeName = kBiomeNames[_files[i].biomeIdx];
        int bnw = MeasureText(biomeName, 15);
        DrawText(biomeName,
            (int)(biomeBox.x + biomeBox.width * 0.5f - bnw * 0.5f),
            (int)(selectorY + btnH * 0.5f - 7.f),
            15, GOLD);
    }

    // Open button
    if (_selectedFileIdx >= 0 && _selectedFileIdx < (int)_files.size())
    {
        float bx = sw * 0.5f - 130.f;
        float by = sh - 72.f;
        bool hov = CheckCollisionPointRec(GetVirtualMousePos(), { bx, by, 260.f, 52.f });
        DrawRectangleRounded({ bx, by, 260.f, 52.f }, 0.3f, 8,
            hov ? Color{ 40, 160, 40, 240 } : Color{ 25, 100, 25, 210 });
        DrawRectangleRoundedLines({ bx, by, 260.f, 52.f }, 0.3f, 8,
            hov ? Color{120, 255, 120, 255} : Fade(WHITE, 0.25f));

        const char* label = TextFormat("Open  \"%s\"",
            _files[_selectedFileIdx].stem.c_str());
        int lw = MeasureText(label, 20);
        DrawText(label, (int)(bx + 130.f - lw * 0.5f), (int)(by + 16.f), 20, RAYWHITE);
    }
    else
    {
        DrawText("Click a file above to select it.",
            (int)(sw * 0.5f - MeasureText("Click a file above to select it.", 18) * 0.5f),
            (int)(sh - 54.f), 18, Fade(WHITE, 0.35f));
    }
}

void TileMapper::OpenSelectedFile()
{
    if (_selectedFileIdx < 0 || _selectedFileIdx >= (int)_files.size())
        return;

    _openFileIdx = _selectedFileIdx;
    _assignments.clear();
    _propDefs.clear();
    _animPropDefs.clear();
    _pendingAnimPropFrames.clear();
    _decorDefs.clear();
    _animDecorDefs.clear();
    _pendingAnimDecorFrames.clear();
    _propScrollY        = 0.f;
    _decorScrollY       = 0.f;
    _panelTab           = PanelTab::Tiles;
    _editingPropIdx     = -1;
    _editingAnimPropIdx = -1;
    _collDragging   = false;
    _collHandle     = CollHandle::None;
    _hasSelection    = false;
    _selFromGround   = false;
    _dragFromGround  = false;
    _isDragging      = false;
    _hoveredTypeIdx  = -1;
    _middleDragging  = false;

    LoadSheet(_files[_openFileIdx].fullPath);
    TryLoadSave();   // restore previous assignments if any

    _screen = Screen::Mapping;
}

// ── Screen 2 — Mapping ────────────────────────────────────────────────────────

void TileMapper::LoadSheet(const std::string& path)
{
    if (_sheet.id != 0)
    {
        UnloadTexture(_sheet);
        _sheet = {};
    }

    _sheet = LoadTexture(path.c_str());
    if (_sheet.id == 0)
    {
        TraceLog(LOG_WARNING, "TileMapper: could not load '%s'", path.c_str());
        return;
    }

    _sheetCols = _sheet.width  / kTileSize;
    _sheetRows = _sheet.height / kTileSize;

    float sw = (float)kVirtualWidth;
    float sh = (float)kVirtualHeight;
    _panelX = sw * (1.f - kPanelFrac);

    float availW = _panelX - 20.f;
    float availH = sh - 30.f;

    // Fit both sheets (main + ground) stacked with a gap.
    int   maxCols   = std::max(_sheetCols,  _groundCols);
    int   totalRows = _sheetRows + (_groundSheet.id != 0 ? kGroundGap / kTileSize + _groundRows : 0);
    float fitScaleW = availW / (float)(maxCols  * kTileSize);
    float fitScaleH = availH / (float)(totalRows * kTileSize);
    _minScale = std::max(std::min(fitScaleW, fitScaleH), 0.25f);
    _scale    = _minScale;

    float combinedW = maxCols   * kTileSize * _scale;
    float combinedH = totalRows * kTileSize * _scale;
    _offX = 10.f + (availW - combinedW) * 0.5f;
    _offY = 10.f + (availH - combinedH) * 0.5f;
}

void TileMapper::UpdateMapping()
{
    if (IsKeyPressed(KEY_ESCAPE))
    {
        // If the collision editor is open, ESC just closes it.
        if (_editingPropIdx >= 0 || _editingAnimPropIdx >= 0)
        {
            _editingPropIdx     = -1;
            _editingAnimPropIdx = -1;
            _collDragging   = false;
            _collHandle     = CollHandle::None;
            return;
        }
        // Return to file select — unload sheet, keep assignments saved.
        if (_sheet.id != 0) { UnloadTexture(_sheet); _sheet = {}; }
        _assignments.clear();
        _propDefs.clear();
        _animPropDefs.clear();
        _pendingAnimPropFrames.clear();
        _decorDefs.clear();
        _animDecorDefs.clear();
        _pendingAnimDecorFrames.clear();
        _hasSelection  = false;
        _openFileIdx   = -1;
        _screen        = Screen::FileSelect;
        return;
    }
    if (IsKeyPressed(KEY_S))
        ExportAndSave();

    if ((_editingPropIdx >= 0 || _editingAnimPropIdx >= 0) && _panelTab == PanelTab::Props)
        HandleCollisionEditorMouse();
    else
        HandleMouseMapping();
}

void TileMapper::HandleMouseMapping()
{
    if (_sheet.id == 0) return;

    Vector2 mouse = GetVirtualMousePos();
    float sw      = (float)kVirtualWidth;
    float sh      = (float)kVirtualHeight;
    float panelW  = sw - _panelX;

    // ── Zoom (scroll wheel in sheet area) ─────────────────────────────────────
    float wheel = GetMouseWheelMove();
    if (wheel != 0.f && mouse.x < _panelX)
    {
        float factor   = (wheel > 0.f) ? 1.15f : (1.f / 1.15f);
        float newScale = std::clamp(_scale * factor, _minScale, _minScale * 20.f);
        if (newScale != _scale)
        {
            _offX  = mouse.x - (mouse.x - _offX) * (newScale / _scale);
            _offY  = mouse.y - (mouse.y - _offY) * (newScale / _scale);
            _scale = newScale;
        }
    }

    // ── Pan (middle-mouse drag) ────────────────────────────────────────────────
    if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE) && mouse.x < _panelX)
    {
        _middleDragging        = true;
        _middleDragStart       = mouse;
        _middleDragOffsetStart = { _offX, _offY };
    }
    if (_middleDragging)
    {
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
        {
            _offX = _middleDragOffsetStart.x + (mouse.x - _middleDragStart.x);
            _offY = _middleDragOffsetStart.y + (mouse.y - _middleDragStart.y);
        }
        else _middleDragging = false;
    }

    // ── Sheet drag-selection (left-mouse, both sheets) ────────────────────────
    float cellPx      = kTileSize * _scale;
    float mainRight   = _offX + _sheetCols * cellPx;
    float mainBottom  = _offY + _sheetRows * cellPx;
    float groundTop   = GroundSheetScreenY();
    float groundRight = _offX + _groundCols * cellPx;
    float groundBot   = groundTop + _groundRows * cellPx;

    bool inMain   = (mouse.x >= _offX && mouse.x < mainRight &&
                     mouse.y >= _offY && mouse.y < mainBottom);
    bool inGround = (_groundSheet.id != 0 &&
                     mouse.x >= _offX && mouse.x < groundRight &&
                     mouse.y >= groundTop && mouse.y < groundBot);

    if (inMain || inGround)
    {
        int gc, gr;
        if (inMain)
        {
            Vector2 g = ScreenToGrid(mouse);
            gc = std::max(0, std::min((int)g.x, _sheetCols - 1));
            gr = std::max(0, std::min((int)g.y, _sheetRows - 1));
        }
        else
        {
            gc = std::max(0, std::min((int)((mouse.x - _offX)    / cellPx), _groundCols - 1));
            gr = std::max(0, std::min((int)((mouse.y - groundTop) / cellPx), _groundRows - 1));
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            _isDragging     = true;
            _dragFromGround = inGround;
            _dragC0 = _dragC1 = gc;
            _dragR0 = _dragR1 = gr;
        }
        if (_isDragging && IsMouseButtonDown(MOUSE_LEFT_BUTTON) && (_dragFromGround == inGround))
            { _dragC1 = gc; _dragR1 = gr; }
        if (_isDragging && IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
        {
            _isDragging    = false;
            _hasSelection  = true;
            _selFromGround = _dragFromGround;
            _selC0 = std::min(_dragC0, _dragC1); _selR0 = std::min(_dragR0, _dragR1);
            _selC1 = std::max(_dragC0, _dragC1); _selR1 = std::max(_dragR0, _dragR1);
        }
    }
    else if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && _isDragging)
    {
        _isDragging = false;
    }

    // Biome cycle
    float bx = _panelX + 10.f;
    float by = 40.f;
    if (_openFileIdx >= 0 && _openFileIdx < (int)_files.size())
    {
        if (CheckCollisionPointRec(mouse, { bx, by, 24.f, 22.f }) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            _files[_openFileIdx].biomeIdx =
                (_files[_openFileIdx].biomeIdx - 1 + kBiomeCount) % kBiomeCount;
        if (CheckCollisionPointRec(mouse, { bx + 24.f + 130.f, by, 24.f, 22.f }) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            _files[_openFileIdx].biomeIdx =
                (_files[_openFileIdx].biomeIdx + 1) % kBiomeCount;
    }

    // ── Layout constants — must match DrawPanel exactly ───────────────────────
    static constexpr float kTabsY    = 96.f;
    static constexpr float kTabH     = 26.f;
    static constexpr float kContentY = kTabsY + kTabH + 4.f;   // = 126
    static constexpr float kBtnStart = kContentY + 18.f;        // tile buttons start below hint text
    static constexpr float kBtnH     = 30.f;
    static constexpr float kBtnGap   = 3.f;
    static constexpr float kListRowH = 52.f;
    static constexpr float kAddBtnH  = 28.f;
    static constexpr float kListY    = kContentY + kAddBtnH + 8.f;

    // Tab row
    float tabW = (panelW - 20.f) / 3.f;
    for (int t = 0; t < 3; t++)
    {
        Rectangle tab{ _panelX + 10.f + t * tabW, kTabsY, tabW, kTabH };
        if (CheckCollisionPointRec(mouse, tab) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            _panelTab = (PanelTab)t;
    }

    // ── Tiles tab ─────────────────────────────────────────────────────────────
    if (_panelTab == PanelTab::Tiles)
    {
        _hoveredTypeIdx = -1;
        int visibleIdx = 0;
        for (int i = 0; i < kTypeCount; i++)
        {
            if (i == (int)TileType::WallInnerCornerL  ||
                i == (int)TileType::WallInnerCornerR  ||
                i == (int)TileType::WallInnerCornerBL ||
                i == (int)TileType::WallInnerCornerBR)
                continue;

            Rectangle btn{ _panelX + 10.f, kBtnStart + visibleIdx * (kBtnH + kBtnGap),
                           panelW - 20.f, kBtnH };
            if (CheckCollisionPointRec(mouse, btn))
            {
                _hoveredTypeIdx = i;
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && _hasSelection)
                    ConfirmSelection(i);
            }
            visibleIdx++;
        }
        float clearY = kBtnStart + visibleIdx * (kBtnH + kBtnGap) + 8.f;

        if (CheckCollisionPointRec(mouse, { _panelX + 10.f, clearY, panelW - 20.f, 26.f }) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            ClearSelection();

        if (CheckCollisionPointRec(mouse, { _panelX + 10.f, clearY + 32.f, panelW - 20.f, 26.f }) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        { _assignments.clear(); _hasSelection = false; }
    }

    // ── Props tab ─────────────────────────────────────────────────────────────
    if (_panelTab == PanelTab::Props)
    {
        if (mouse.x > _panelX)
            _propScrollY = std::max(0.f, _propScrollY - GetMouseWheelMove() * 50.f);

        Rectangle sel{
            (float)(_selC0 * kTileSize), (float)(_selR0 * kTileSize),
            (float)((_selC1 - _selC0 + 1) * kTileSize),
            (float)((_selR1 - _selR0 + 1) * kTileSize) };

        // ── Static prop add ───────────────────────────────────────────────────
        if (CheckCollisionPointRec(mouse, { _panelX+10.f, kContentY, panelW-20.f, 24.f }) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && _hasSelection)
        {
            _propDefs.push_back({ sel, { 0.f, 0.f, sel.width, sel.height } });
            _hasSelection = false;
        }

        // ── Anim prop builder ─────────────────────────────────────────────────
        float abY = kContentY + 44.f;   // "Add Frame" button Y — must match DrawPanel

        // Add Frame button — appends current selection to pending list
        if (CheckCollisionPointRec(mouse, { _panelX+10.f, abY, panelW-20.f, 24.f }) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && _hasSelection)
        {
            _pendingAnimPropFrames.push_back(sel);
            _hasSelection = false;
        }

        // FPS stepper
        float fpsY = kContentY + 72.f;
        if (CheckCollisionPointRec(mouse, { _panelX+52.f, fpsY, 18.f, 18.f }) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            _animPropFps = std::max(1.f, _animPropFps - 1.f);
        if (CheckCollisionPointRec(mouse, { _panelX+88.f, fpsY, 18.f, 18.f }) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            _animPropFps = std::min(30.f, _animPropFps + 1.f);

        // Per-frame thumbnail X buttons
        float thumbY = kContentY + 96.f;
        float thumbSlot = 36.f;
        for (int i = 0; i < (int)_pendingAnimPropFrames.size(); i++)
        {
            float tx = _panelX + 10.f + i * thumbSlot;
            if (tx + 32.f > _panelX + panelW - 10.f) break;
            Rectangle xBtn{ tx + 22.f, thumbY + 10.f, 10.f, 10.f };
            if (CheckCollisionPointRec(mouse, xBtn) &&
                IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                _pendingAnimPropFrames.erase(_pendingAnimPropFrames.begin() + i);
                break;
            }
        }

        // Finalize button — commits pending frames as a new AnimPropDef
        float finalY = kContentY + 134.f;
        bool hasPending = !_pendingAnimPropFrames.empty();
        if (CheckCollisionPointRec(mouse, { _panelX+10.f, finalY, panelW-20.f, 24.f }) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && hasPending)
        {
            AnimPropDef def;
            def.frames    = _pendingAnimPropFrames;
            def.fps       = _animPropFps;
            def.collision = { 0.f, 0.f, def.frames[0].width, def.frames[0].height };
            _animPropDefs.push_back(std::move(def));
            _pendingAnimPropFrames.clear();
            // Auto-open collision editor for the new entry
            _editingAnimPropIdx = (int)_animPropDefs.size() - 1;
            _editingPropIdx     = -1;
            _collDragging = false;
            _collHandle   = CollHandle::None;
        }

        float propListY = kContentY + 166.f;

        // Static props list
        for (int i = 0; i < (int)_propDefs.size(); i++)
        {
            float ry = propListY + i * kListRowH - _propScrollY;
            if (ry + kListRowH < kContentY || ry > sh) continue;

            Rectangle removeBtn{ _panelX + panelW - 36.f, ry + 13.f, 26.f, 24.f };
            if (CheckCollisionPointRec(mouse, removeBtn) &&
                IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                if (_editingPropIdx == i) _editingPropIdx = -1;
                else if (_editingPropIdx > i) _editingPropIdx--;
                _propDefs.erase(_propDefs.begin() + i);
                break;
            }

            Rectangle rowRect{ _panelX + 6.f, ry, panelW - 46.f, kListRowH };
            if (CheckCollisionPointRec(mouse, rowRect) &&
                IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                _editingPropIdx     = (_editingPropIdx == i) ? -1 : i;
                _editingAnimPropIdx = -1;
                _collDragging = false;
                _collHandle   = CollHandle::None;
            }
        }

        // Animated props list
        int animRowOffset = (int)_propDefs.size();
        for (int i = 0; i < (int)_animPropDefs.size(); i++)
        {
            float ry = propListY + (animRowOffset + i) * kListRowH - _propScrollY;
            if (ry + kListRowH < kContentY || ry > sh) continue;

            Rectangle removeBtn{ _panelX + panelW - 36.f, ry + 13.f, 26.f, 24.f };
            if (CheckCollisionPointRec(mouse, removeBtn) &&
                IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                if (_editingAnimPropIdx == i) _editingAnimPropIdx = -1;
                else if (_editingAnimPropIdx > i) _editingAnimPropIdx--;
                _animPropDefs.erase(_animPropDefs.begin() + i);
                break;
            }

            Rectangle rowRect{ _panelX + 6.f, ry, panelW - 46.f, kListRowH };
            if (CheckCollisionPointRec(mouse, rowRect) &&
                IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                _editingAnimPropIdx = (_editingAnimPropIdx == i) ? -1 : i;
                _editingPropIdx     = -1;
                _collDragging = false;
                _collHandle   = CollHandle::None;
            }
        }
    }

    // ── Decors tab ────────────────────────────────────────────────────────────
    if (_panelTab == PanelTab::Decors)
    {
        if (mouse.x > _panelX)
            _decorScrollY = std::max(0.f, _decorScrollY - GetMouseWheelMove() * 50.f);

        Rectangle src{
            (float)(_selC0 * kTileSize), (float)(_selR0 * kTileSize),
            (float)((_selC1 - _selC0 + 1) * kTileSize),
            (float)((_selR1 - _selR0 + 1) * kTileSize) };

        // Static add button
        if (CheckCollisionPointRec(mouse, { _panelX + 10.f, kContentY, panelW - 20.f, 24.f }) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && _hasSelection)
        { _decorDefs.push_back(src); _hasSelection = false; }

        // ── Anim decor builder (mirrors anim prop builder in Props tab) ─────────
        float abY = kContentY + 32.f;
        if (CheckCollisionPointRec(mouse, { _panelX + 10.f, abY, panelW - 20.f, 24.f }) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && _hasSelection)
        {
            _pendingAnimDecorFrames.push_back(src);
            _hasSelection = false;
        }

        // FPS stepper
        float fpsY = kContentY + 60.f;
        if (CheckCollisionPointRec(mouse, { _panelX + 50.f, fpsY, 20.f, 20.f }) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            _animDecorFps = std::max(1.f, _animDecorFps - 1.f);
        if (CheckCollisionPointRec(mouse, { _panelX + 90.f, fpsY, 20.f, 20.f }) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            _animDecorFps = std::min(30.f, _animDecorFps + 1.f);

        // Per-frame X buttons on the pending thumbnail strip
        float thumbY    = kContentY + 84.f;
        float thumbSlot = 36.f;
        for (int i = 0; i < (int)_pendingAnimDecorFrames.size(); i++)
        {
            float tx = _panelX + 10.f + i * thumbSlot;
            if (tx + 32.f > _panelX + panelW - 10.f) break;
            Rectangle xBtn{ tx + 22.f, thumbY + 10.f, 10.f, 10.f };
            if (CheckCollisionPointRec(mouse, xBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                _pendingAnimDecorFrames.erase(_pendingAnimDecorFrames.begin() + i);
                break;
            }
        }

        // Finalize button — commits pending frames as a new AnimDecorDef
        float finalY = kContentY + 122.f;
        bool hasPendingDecor = !_pendingAnimDecorFrames.empty();
        if (CheckCollisionPointRec(mouse, { _panelX + 10.f, finalY, panelW - 20.f, 24.f }) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && hasPendingDecor)
        {
            AnimDecorDef def;
            def.frames = _pendingAnimDecorFrames;
            def.fps    = _animDecorFps;
            _animDecorDefs.push_back(std::move(def));
            _pendingAnimDecorFrames.clear();
        }

        // List area: static decors then animated decors
        float listY = kContentY + 150.f;
        int totalEntries = (int)_decorDefs.size() + (int)_animDecorDefs.size();
        for (int i = 0; i < (int)_decorDefs.size(); i++)
        {
            float ry = listY + i * kListRowH - _decorScrollY;
            if (ry + kListRowH < kContentY || ry > sh) continue;
            Rectangle removeBtn{ _panelX + panelW - 36.f, ry + 13.f, 26.f, 24.f };
            if (CheckCollisionPointRec(mouse, removeBtn) &&
                IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            { _decorDefs.erase(_decorDefs.begin() + i); break; }
        }
        for (int i = 0; i < (int)_animDecorDefs.size(); i++)
        {
            float ry = listY + ((int)_decorDefs.size() + i) * kListRowH - _decorScrollY;
            if (ry + kListRowH < kContentY || ry > sh) continue;
            Rectangle removeBtn{ _panelX + panelW - 36.f, ry + 13.f, 26.f, 24.f };
            if (CheckCollisionPointRec(mouse, removeBtn) &&
                IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            { _animDecorDefs.erase(_animDecorDefs.begin() + i); break; }
        }
        (void)totalEntries;
    }
}

void TileMapper::ConfirmSelection(int typeIdx)
{
    if (!_hasSelection || typeIdx < 0 || typeIdx >= kTypeCount) return;
    _assignments.erase(
        std::remove_if(_assignments.begin(), _assignments.end(),
            [typeIdx](const Assignment& a){ return a.typeIdx == typeIdx; }),
        _assignments.end());
    Assignment a;
    a.col        = _selC0;
    a.row        = _selR0;
    a.spanCols   = _selC1 - _selC0 + 1;
    a.spanRows   = _selR1 - _selR0 + 1;
    a.typeIdx    = typeIdx;
    a.fromGround = _selFromGround;
    _assignments.push_back(a);
    _hasSelection = false;
}

void TileMapper::ClearSelection()
{
    if (!_hasSelection) return;
    int c0 = _selC0, r0 = _selR0;
    _assignments.erase(
        std::remove_if(_assignments.begin(), _assignments.end(),
            [c0, r0](const Assignment& a){ return a.col == c0 && a.row == r0; }),
        _assignments.end());
    _hasSelection = false;
}

// ── Export / Save / Load ──────────────────────────────────────────────────────

std::string TileMapper::SavePath() const
{
    std::string stem = (_openFileIdx >= 0 && _openFileIdx < (int)_files.size())
        ? _files[_openFileIdx].stem : "unknown";
    return "tilemapper_" + stem + ".txt";
}

void TileMapper::ExportAndSave() const
{
    int biomeIdx = (_openFileIdx >= 0 && _openFileIdx < (int)_files.size())
        ? _files[_openFileIdx].biomeIdx : 0;
    const char* biomeName = kBiomeNames[biomeIdx];

    TraceLog(LOG_INFO, "=== TileMapper Export: %s (%s) ===", SavePath().c_str(), biomeName);
    for (const Assignment& a : _assignments)
    {
        if (a.typeIdx < 0 || a.typeIdx >= kTypeCount) continue;
        TraceLog(LOG_INFO, "%-24s = { %3d, %3d, %3d, %3d };",
            kTypeNames[a.typeIdx],
            a.col * kTileSize, a.row * kTileSize,
            a.spanCols * kTileSize, a.spanRows * kTileSize);
    }
    for (int i = 0; i < (int)_propDefs.size(); i++)
        TraceLog(LOG_INFO, "Prop[%d]          = { %3d, %3d, %3d, %3d }  coll{ %3d,%3d,%3d,%3d };", i,
            (int)_propDefs[i].src.x,       (int)_propDefs[i].src.y,
            (int)_propDefs[i].src.width,   (int)_propDefs[i].src.height,
            (int)_propDefs[i].collision.x, (int)_propDefs[i].collision.y,
            (int)_propDefs[i].collision.width, (int)_propDefs[i].collision.height);
    for (int i = 0; i < (int)_decorDefs.size(); i++)
        TraceLog(LOG_INFO, "Decor[%d]         = { %3d, %3d, %3d, %3d };", i,
            (int)_decorDefs[i].x, (int)_decorDefs[i].y,
            (int)_decorDefs[i].width, (int)_decorDefs[i].height);
    for (int i = 0; i < (int)_animPropDefs.size(); i++)
    {
        TraceLog(LOG_INFO, "AnimProp[%d]      = %dfr  %.0ffps  coll{ %.0f, %.0f, %.0f, %.0f };", i,
            (int)_animPropDefs[i].frames.size(), _animPropDefs[i].fps,
            _animPropDefs[i].collision.x, _animPropDefs[i].collision.y,
            _animPropDefs[i].collision.width, _animPropDefs[i].collision.height);
        for (int f = 0; f < (int)_animPropDefs[i].frames.size(); f++)
            TraceLog(LOG_INFO, "  frame[%d]        = { %.0f, %.0f, %.0f, %.0f };", f,
                _animPropDefs[i].frames[f].x, _animPropDefs[i].frames[f].y,
                _animPropDefs[i].frames[f].width, _animPropDefs[i].frames[f].height);
    }
    for (int i = 0; i < (int)_animDecorDefs.size(); i++)
    {
        TraceLog(LOG_INFO, "AnimDecor[%d]     = %dfr  %.0ffps;", i,
            (int)_animDecorDefs[i].frames.size(), _animDecorDefs[i].fps);
        for (int f = 0; f < (int)_animDecorDefs[i].frames.size(); f++)
            TraceLog(LOG_INFO, "  frame[%d]        = { %.0f, %.0f, %.0f, %.0f };", f,
                _animDecorDefs[i].frames[f].x, _animDecorDefs[i].frames[f].y,
                _animDecorDefs[i].frames[f].width, _animDecorDefs[i].frames[f].height);
    }
    TraceLog(LOG_INFO, "==========================================");

    std::string path = SavePath();
    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "w");
    if (f)
    {
        fprintf(f, "BIOME %s\n", biomeName);
        for (const Assignment& a : _assignments)
        {
            if (a.typeIdx < 0 || a.typeIdx >= kTypeCount) continue;
            fprintf(f, "%s %d %d %d %d %d\n",
                a.fromGround ? "GTILE" : "TILE",
                a.col, a.row, a.spanCols, a.spanRows, a.typeIdx);
        }
        for (const PropDef& p : _propDefs)
            fprintf(f, "PROP %.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f\n",
                p.src.x, p.src.y, p.src.width, p.src.height,
                p.collision.x, p.collision.y, p.collision.width, p.collision.height);
        for (const AnimPropDef& a : _animPropDefs)
        {
            // Format: cx cy cw ch fps frameCount  x0 y0 w0 h0  x1 y1 w1 h1 ...
            fprintf(f, "ANIMPROP %.0f %.0f %.0f %.0f %.1f %d",
                a.collision.x, a.collision.y, a.collision.width, a.collision.height,
                a.fps, (int)a.frames.size());
            for (const Rectangle& r : a.frames)
                fprintf(f, " %.0f %.0f %.0f %.0f", r.x, r.y, r.width, r.height);
            fprintf(f, "\n");
        }
        for (const Rectangle& r : _decorDefs)
            fprintf(f, "DECOR %.0f %.0f %.0f %.0f\n", r.x, r.y, r.width, r.height);
        for (const AnimDecorDef& a : _animDecorDefs)
        {
            fprintf(f, "ANIMDECOR %.1f %d", a.fps, (int)a.frames.size());
            for (const Rectangle& r : a.frames)
                fprintf(f, " %.0f %.0f %.0f %.0f", r.x, r.y, r.width, r.height);
            fprintf(f, "\n");
        }
        fclose(f);

        if (_openFileIdx >= 0 && _openFileIdx < (int)_files.size())
            const_cast<TileMapper*>(this)->_files[_openFileIdx].hasSave = true;
    }
}

void TileMapper::TryLoadSave()
{
    if (_openFileIdx < 0 || _openFileIdx >= (int)_files.size()) return;

    std::string path = SavePath();
    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "r");
    if (!f) return;

    // First line: BIOME <name>
    char biomeTag[16]{}, biomeName[64]{};
    if (fscanf_s(f, "%15s %63s", biomeTag, (unsigned)sizeof(biomeTag),
                 biomeName, (unsigned)sizeof(biomeName)) == 2)
    {
        for (int i = 0; i < kBiomeCount; i++)
        {
            if (strcmp(biomeName, kBiomeNames[i]) == 0)
            {
                _files[_openFileIdx].biomeIdx = i;
                break;
            }
        }
    }

    char tag[32]{};
    while (fscanf_s(f, "%31s", tag, (unsigned)sizeof(tag)) == 1)
    {
        if (strcmp(tag, "TILE") == 0 || strcmp(tag, "GTILE") == 0)
        {
            bool isGround = (strcmp(tag, "GTILE") == 0);
            int col, row, sc, sr, ti;
            if (fscanf_s(f, "%d %d %d %d %d", &col, &row, &sc, &sr, &ti) != 5) continue;
            if (ti < 0 || ti >= kTypeCount) continue;
            Assignment a;
            a.col = col; a.row = row; a.spanCols = sc; a.spanRows = sr;
            a.typeIdx = ti; a.fromGround = isGround;
            _assignments.push_back(a);
        }
        else if (strcmp(tag, "PROP") == 0)
        {
            float x, y, w, h;
            if (fscanf_s(f, "%f %f %f %f", &x, &y, &w, &h) != 4) continue;
            float cx = 0.f, cy = 0.f, cw = w, ch = h;
            long savedPos = ftell(f);
            float tmp[4]{};
            if (fscanf_s(f, "%f %f %f %f", &tmp[0], &tmp[1], &tmp[2], &tmp[3]) == 4)
                { cx = tmp[0]; cy = tmp[1]; cw = tmp[2]; ch = tmp[3]; }
            else
                fseek(f, savedPos, SEEK_SET);
            _propDefs.push_back({ {x, y, w, h}, {cx, cy, cw, ch} });
        }
        else if (strcmp(tag, "DECOR") == 0)
        {
            float x, y, w, h;
            if (fscanf_s(f, "%f %f %f %f", &x, &y, &w, &h) != 4) continue;
            _decorDefs.push_back({ x, y, w, h });
        }
        else if (strcmp(tag, "ANIMDECOR") == 0)
        {
            float fps;
            int   fc;
            if (fscanf_s(f, "%f %d", &fps, &fc) != 2) continue;
            AnimDecorDef def;
            def.fps = fps;
            for (int i = 0; i < fc; i++)
            {
                float fx, fy, fw, fh;
                if (fscanf_s(f, "%f %f %f %f", &fx, &fy, &fw, &fh) != 4) break;
                def.frames.push_back({ fx, fy, fw, fh });
            }
            if (!def.frames.empty())
                _animDecorDefs.push_back(std::move(def));
        }
        else if (strcmp(tag, "ANIMPROP") == 0)
        {
            float cx, cy, cw, ch, fps;
            int   fc;
            if (fscanf_s(f, "%f %f %f %f %f %d", &cx, &cy, &cw, &ch, &fps, &fc) != 6) continue;
            AnimPropDef def;
            def.collision = { cx, cy, cw, ch };
            def.fps       = fps;
            for (int i = 0; i < fc; i++)
            {
                float fx, fy, fw, fh;
                if (fscanf_s(f, "%f %f %f %f", &fx, &fy, &fw, &fh) != 4) break;
                def.frames.push_back({ fx, fy, fw, fh });
            }
            if (!def.frames.empty())
                _animPropDefs.push_back(std::move(def));
        }
        else
        {
            // Legacy format: tag is the col integer.
            int col = atoi(tag), row, sc, sr, ti;
            if (fscanf_s(f, "%d %d %d %d", &row, &sc, &sr, &ti) != 4) continue;
            if (ti < 0 || ti >= kTypeCount) continue;
            Assignment a; a.col = col; a.row = row; a.spanCols = sc; a.spanRows = sr; a.typeIdx = ti;
            _assignments.push_back(a);
        }
    }
    fclose(f);
}

// ── Drawing helpers ───────────────────────────────────────────────────────────

void TileMapper::DrawMapping() const
{
    if ((_editingPropIdx >= 0 || _editingAnimPropIdx >= 0) && _panelTab == PanelTab::Props)
    {
        DrawCollisionEditor();
    }
    else
    {
        ClearBackground(Color{ 18, 18, 24, 255 });
        DrawSheet();
        DrawGrid();
        DrawAssignments();
        DrawSelection();
        const char* hint = "[Click] Select  [Drag] Multi-tile  [S] Save & Export  [ESC] Back";
        DrawText(hint, 10, kVirtualHeight - 20, 14, Fade(WHITE, 0.4f));
    }
    DrawPanel();
}

void TileMapper::DrawSheet() const
{
    if (_sheet.id == 0)
    {
        DrawText("Sheet failed to load.", 20, 20, 22, RED);
        return;
    }
    DrawTexturePro(_sheet,
        { 0, 0, (float)_sheet.width, (float)_sheet.height },
        { _offX, _offY, _sheet.width * _scale, _sheet.height * _scale },
        {}, 0.f, WHITE);

    // Ground TIles sheet — always shown below the main sheet.
    if (_groundSheet.id != 0)
    {
        float gy = GroundSheetScreenY();
        DrawText("Ground Tiles  (always available)",
            (int)_offX, (int)(gy - 18.f * std::max(_scale / _minScale, 1.f)), 14, Fade(GOLD, 0.8f));
        DrawTexturePro(_groundSheet,
            { 0, 0, (float)_groundSheet.width, (float)_groundSheet.height },
            { _offX, gy, _groundSheet.width * _scale, _groundSheet.height * _scale },
            {}, 0.f, WHITE);
    }
}

void TileMapper::DrawGrid() const
{
    if (_sheet.id == 0) return;
    float cellPx  = kTileSize * _scale;
    float groundY = GroundSheetScreenY();

    // Main sheet grid
    for (int c = 0; c <= _sheetCols; c++)
        DrawLineV({ _offX + c * cellPx, _offY },
                  { _offX + c * cellPx, _offY + _sheetRows * cellPx }, Fade(WHITE, 0.14f));
    for (int r = 0; r <= _sheetRows; r++)
        DrawLineV({ _offX, _offY + r * cellPx },
                  { _offX + _sheetCols * cellPx, _offY + r * cellPx }, Fade(WHITE, 0.14f));

    // Ground sheet grid
    if (_groundSheet.id != 0)
    {
        for (int c = 0; c <= _groundCols; c++)
            DrawLineV({ _offX + c * cellPx, groundY },
                      { _offX + c * cellPx, groundY + _groundRows * cellPx }, Fade(GOLD, 0.14f));
        for (int r = 0; r <= _groundRows; r++)
            DrawLineV({ _offX, groundY + r * cellPx },
                      { _offX + _groundCols * cellPx, groundY + r * cellPx }, Fade(GOLD, 0.14f));
    }

    // Hover highlight (main or ground sheet)
    Vector2 mouse = GetVirtualMousePos();
    bool inMain   = (mouse.x >= _offX && mouse.x < _offX + _sheetCols  * cellPx &&
                     mouse.y >= _offY  && mouse.y < _offY  + _sheetRows  * cellPx);
    bool inGround = (_groundSheet.id != 0 &&
                     mouse.x >= _offX && mouse.x < _offX + _groundCols * cellPx &&
                     mouse.y >= groundY && mouse.y < groundY + _groundRows * cellPx);

    if (inMain)
    {
        Vector2 g  = ScreenToGrid(mouse);
        int gc = std::max(0, std::min((int)g.x, _sheetCols - 1));
        int gr = std::max(0, std::min((int)g.y, _sheetRows - 1));
        DrawRectangleLinesEx(GridToScreen(gc, gr, 1, 1), 1.5f, Fade(YELLOW, 0.85f));
        DrawText(TextFormat("(%d, %d)", gc * kTileSize, gr * kTileSize),
            (int)(_offX + gc * cellPx + 2.f), (int)(_offY + gr * cellPx - 14.f), 11, YELLOW);
    }
    else if (inGround)
    {
        int gc = std::max(0, std::min((int)((mouse.x - _offX)   / cellPx), _groundCols - 1));
        int gr = std::max(0, std::min((int)((mouse.y - groundY) / cellPx), _groundRows - 1));
        DrawRectangleLinesEx(GridToGroundScreen(gc, gr, 1, 1), 1.5f, Fade(GOLD, 0.85f));
        DrawText(TextFormat("GT (%d, %d)", gc * kTileSize, gr * kTileSize),
            (int)(_offX + gc * cellPx + 2.f), (int)(groundY + gr * cellPx - 14.f), 11, GOLD);
    }
}

void TileMapper::DrawAssignments() const
{
    for (const Assignment& a : _assignments)
    {
        Rectangle r = a.fromGround
            ? GridToGroundScreen(a.col, a.row, a.spanCols, a.spanRows)
            : GridToScreen(a.col, a.row, a.spanCols, a.spanRows);
        Color c = (a.typeIdx >= 0 && a.typeIdx < kTypeCount)
            ? kTypeColors[a.typeIdx] : Fade(WHITE, 0.15f);
        DrawRectangleRec(r, c);
        DrawRectangleLinesEx(r, 1.5f, a.fromGround ? GOLD : Fade(WHITE, 0.5f));
        if (a.typeIdx >= 0 && a.typeIdx < kTypeCount)
        {
            int fs = 9;
            int tw = MeasureText(kTypeNames[a.typeIdx], fs);
            if (tw < (int)r.width - 2)
                DrawText(kTypeNames[a.typeIdx],
                    (int)(r.x + r.width * 0.5f - tw * 0.5f),
                    (int)(r.y + r.height * 0.5f - fs * 0.5f),
                    fs, WHITE);
        }
    }
}

void TileMapper::DrawSelection() const
{
    if (!_hasSelection && !_isDragging) return;
    int c0, r0, c1, r1;
    bool fromGround;
    if (_isDragging)
    {
        c0 = std::min(_dragC0, _dragC1); r0 = std::min(_dragR0, _dragR1);
        c1 = std::max(_dragC0, _dragC1); r1 = std::max(_dragR0, _dragR1);
        fromGround = _dragFromGround;
    }
    else
    {
        c0 = _selC0; r0 = _selR0; c1 = _selC1; r1 = _selR1;
        fromGround = _selFromGround;
    }

    Rectangle r = fromGround
        ? GridToGroundScreen(c0, r0, c1 - c0 + 1, r1 - r0 + 1)
        : GridToScreen(c0, r0, c1 - c0 + 1, r1 - r0 + 1);
    Color col = fromGround ? GOLD : SKYBLUE;
    DrawRectangleRec(r, Fade(col, 0.28f));
    DrawRectangleLinesEx(r, 2.f, col);
    DrawText(TextFormat("%s{ %d, %d, %d, %d }",
        fromGround ? "GT " : "",
        c0 * kTileSize, r0 * kTileSize,
        (c1 - c0 + 1) * kTileSize, (r1 - r0 + 1) * kTileSize),
        (int)(r.x + 2.f), (int)(r.y - 16.f), 12, col);
}

void TileMapper::DrawPanel() const
{
    float sw = (float)kVirtualWidth;
    float sh = (float)kVirtualHeight;
    float panelW = sw - _panelX;

    DrawRectangle((int)_panelX, 0, (int)panelW, (int)sh, Color{ 16, 16, 22, 248 });
    DrawLineV({ _panelX, 0.f }, { _panelX, sh }, Fade(WHITE, 0.18f));

    // File name
    std::string stemLabel = (_openFileIdx >= 0 && _openFileIdx < (int)_files.size())
        ? _files[_openFileIdx].stem : "?";
    int nameFs = 17;
    int nameW  = MeasureText(stemLabel.c_str(), nameFs);
    DrawText(stemLabel.c_str(),
        (int)std::min(_panelX + 12.f, _panelX + panelW * 0.5f - nameW * 0.5f),
        14, nameFs, GOLD);

    // Biome row
    int biomeIdx = (_openFileIdx >= 0 && _openFileIdx < (int)_files.size())
        ? _files[_openFileIdx].biomeIdx : 0;
    float bx = _panelX + 10.f;
    float by = 40.f;
    DrawRectangleRounded({ bx, by, 24.f, 22.f }, 0.3f, 4, Fade(WHITE, 0.12f));
    DrawText("<", (int)(bx + 7.f), (int)(by + 3.f), 16, RAYWHITE);
    DrawText(kBiomeNames[biomeIdx], (int)(bx + 30.f), (int)(by + 4.f), 14, GOLD);
    DrawRectangleRounded({ bx + 30.f + 130.f, by, 24.f, 22.f }, 0.3f, 4, Fade(WHITE, 0.12f));
    DrawText(">", (int)(bx + 30.f + 130.f + 7.f), (int)(by + 3.f), 16, RAYWHITE);

    // Selection status
    const char* selInfo = _hasSelection
        ? TextFormat("Sel: (%dx%d) px %d,%d",
              (_selC1 - _selC0 + 1) * kTileSize, (_selR1 - _selR0 + 1) * kTileSize,
              _selC0 * kTileSize, _selR0 * kTileSize)
        : "Click or drag a region";
    DrawText(selInfo, (int)(_panelX + 10.f), 78, 12,
        _hasSelection ? SKYBLUE : Fade(WHITE, 0.4f));

    // ── Tab row ───────────────────────────────────────────────────────────────
    static constexpr float kTabsY   = 96.f;
    static constexpr float kTabH    = 26.f;
    static constexpr float kContentY = kTabsY + kTabH + 4.f;
    const char* tabNames[] = { "Tiles", "Props", "Decors" };
    float tabW = (panelW - 20.f) / 3.f;
    for (int t = 0; t < 3; t++)
    {
        Rectangle tab{ _panelX + 10.f + t * tabW, kTabsY, tabW, kTabH };
        bool active = ((int)_panelTab == t);
        DrawRectangleRec(tab, active ? Color{ 40, 80, 140, 220 } : Color{ 20, 20, 30, 180 });
        DrawRectangleLinesEx(tab, 1.f, active ? Color{120,180,255,255} : Fade(WHITE,0.18f));
        int tw = MeasureText(tabNames[t], 13);
        DrawText(tabNames[t],
            (int)(tab.x + tab.width * 0.5f - tw * 0.5f),
            (int)(tab.y + tab.height * 0.5f - 6.f),
            13, active ? WHITE : Fade(WHITE, 0.5f));
    }

    // ── Tiles tab ─────────────────────────────────────────────────────────────
    if (_panelTab == PanelTab::Tiles)
    {
        DrawText(_hasSelection ? "Then click a type:" : "(drag for multi-tile)",
            (int)(_panelX + 10.f), (int)(kContentY + 2.f), 12, Fade(WHITE, 0.45f));

        float btnH   = 30.f;
        float btnGap = 3.f;
        float btnStartY = kContentY + 18.f;
        int visibleIdx  = 0;

        for (int i = 0; i < kTypeCount; i++)
        {
            if (i == (int)TileType::WallInnerCornerL  ||
                i == (int)TileType::WallInnerCornerR  ||
                i == (int)TileType::WallInnerCornerBL ||
                i == (int)TileType::WallInnerCornerBR)
                continue;

            Rectangle btn{ _panelX + 10.f, btnStartY + visibleIdx * (btnH + btnGap), panelW - 20.f, btnH };
            bool hov  = (i == _hoveredTypeIdx && _hasSelection);
            DrawRectangleRec(btn, hov ? Fade(kTypeColors[i], 0.95f) : Fade(kTypeColors[i], 0.40f));
            DrawRectangleLinesEx(btn, 1.f, hov ? WHITE : Fade(WHITE, 0.22f));
            int fs = 13;
            int tw2 = MeasureText(kTypeNames[i], fs);
            DrawText(kTypeNames[i],
                (int)(btn.x + btn.width * 0.5f - tw2 * 0.5f),
                (int)(btn.y + btn.height * 0.5f - fs * 0.5f),
                fs, WHITE);
            visibleIdx++;
        }

        float clearY = btnStartY + visibleIdx * (btnH + btnGap) + 8.f;
        bool clearHov    = CheckCollisionPointRec(GetVirtualMousePos(),
                               { _panelX + 10.f, clearY, panelW - 20.f, 26.f }) && _hasSelection;
        bool clearAllHov = CheckCollisionPointRec(GetVirtualMousePos(),
                               { _panelX + 10.f, clearY + 32.f, panelW - 20.f, 26.f });
        DrawRectangleRec({ _panelX + 10.f, clearY,        panelW - 20.f, 26.f },
            clearHov    ? Fade(RED, 0.75f) : Fade(RED, 0.30f));
        DrawRectangleLinesEx({ _panelX + 10.f, clearY, panelW - 20.f, 26.f }, 1.f,
            clearHov ? WHITE : Fade(WHITE, 0.2f));
        DrawText("Clear Selection",
            (int)(_panelX + 10.f + (panelW - 20.f) * 0.5f - MeasureText("Clear Selection", 13) * 0.5f),
            (int)(clearY + 6.f), 13, WHITE);

        DrawRectangleRec({ _panelX + 10.f, clearY + 32.f, panelW - 20.f, 26.f },
            clearAllHov ? Color{220,30,30,230} : Color{160,20,20,180});
        DrawRectangleLinesEx({ _panelX + 10.f, clearY + 32.f, panelW - 20.f, 26.f }, 1.f,
            clearAllHov ? WHITE : Fade(WHITE, 0.25f));
        DrawText("Clear ALL",
            (int)(_panelX + 10.f + (panelW - 20.f) * 0.5f - MeasureText("Clear ALL", 13) * 0.5f),
            (int)(clearY + 38.f), 13, WHITE);

        int ew = MeasureText("[S]  Save & Export", 15);
        DrawText("[S]  Save & Export",
            (int)(_panelX + panelW * 0.5f - ew * 0.5f),
            (int)(clearY + 68.f), 15, Fade(GOLD, 0.85f));
    }

    // ── Props tab ─────────────────────────────────────────────────────────────
    auto drawSpriteList = [&](const std::vector<Rectangle>& defs, float scrollY,
                               const char* addLabel, const char* emptyLabel)
    {
        // Add button
        bool addHov = CheckCollisionPointRec(GetVirtualMousePos(),
            { _panelX + 10.f, kContentY, panelW - 20.f, 28.f }) && _hasSelection;
        DrawRectangleRec({ _panelX + 10.f, kContentY, panelW - 20.f, 28.f },
            addHov ? Color{40,160,40,230} : Color{25,100,25,180});
        DrawRectangleLinesEx({ _panelX + 10.f, kContentY, panelW - 20.f, 28.f }, 1.f,
            addHov ? WHITE : Fade(WHITE, 0.25f));
        DrawText(addLabel,
            (int)(_panelX + 10.f + (panelW - 20.f) * 0.5f - MeasureText(addLabel, 13) * 0.5f),
            (int)(kContentY + 7.f), 13, WHITE);

        if (!_hasSelection)
            DrawText("(select a region first)", (int)(_panelX + 10.f),
                (int)(kContentY + 32.f), 11, Fade(WHITE, 0.35f));

        float listY = kContentY + 36.f;

        // Clip region for the scrollable list
        BeginScissorMode((int)_panelX, (int)listY, (int)panelW, (int)(sh - listY));

        if (defs.empty())
        {
            DrawText(emptyLabel, (int)(_panelX + 14.f), (int)(listY + 10.f), 13, Fade(WHITE, 0.35f));
        }
        else
        {
            for (int i = 0; i < (int)defs.size(); i++)
            {
                float ry = listY + i * 52.f - scrollY;
                if (ry + 52.f < listY || ry > sh) continue;

                Rectangle row{ _panelX + 6.f, ry, panelW - 12.f, 50.f };
                DrawRectangleRec(row, Color{ 22, 22, 32, 200 });
                DrawRectangleLinesEx(row, 1.f, Fade(WHITE, 0.15f));

                // Sprite thumbnail
                if (_sheet.id != 0)
                {
                    Rectangle dst{ _panelX + 12.f, ry + 7.f, 36.f, 36.f };
                    DrawTexturePro(_sheet, defs[i], dst, {}, 0.f, WHITE);
                    DrawRectangleLinesEx(dst, 1.f, Fade(WHITE, 0.3f));
                }

                // Label
                DrawText(TextFormat("#%d  (%dx%d)", i,
                             (int)defs[i].width, (int)defs[i].height),
                    (int)(_panelX + 54.f), (int)(ry + 17.f), 12, RAYWHITE);

                // Remove button
                Rectangle removeBtn{ _panelX + panelW - 36.f, ry + 13.f, 26.f, 24.f };
                bool remHov = CheckCollisionPointRec(GetVirtualMousePos(), removeBtn);
                DrawRectangleRec(removeBtn, remHov ? Fade(RED, 0.85f) : Fade(RED, 0.45f));
                DrawRectangleLinesEx(removeBtn, 1.f, remHov ? WHITE : Fade(WHITE, 0.2f));
                DrawText("X",
                    (int)(removeBtn.x + removeBtn.width * 0.5f - MeasureText("X", 13) * 0.5f),
                    (int)(removeBtn.y + 5.f), 13, WHITE);
            }
        }

        EndScissorMode();

        // Scroll indicator
        if ((int)defs.size() > 6)
            DrawText(TextFormat("[Scroll]  %d items", (int)defs.size()),
                (int)(_panelX + 10.f), (int)(sh - 22.f), 11, Fade(WHITE, 0.35f));
    };

    // ── Props tab ─────────────────────────────────────────────────────────────
    if (_panelTab == PanelTab::Props)
    {
        Vector2 mouse2 = GetVirtualMousePos();

        // ── Static add button ─────────────────────────────────────────────────
        bool addHov = CheckCollisionPointRec(mouse2,
            { _panelX+10.f, kContentY, panelW-20.f, 24.f }) && _hasSelection;
        DrawRectangleRec({ _panelX+10.f, kContentY, panelW-20.f, 24.f },
            addHov ? Color{40,160,40,230} : Color{25,100,25,180});
        DrawRectangleLinesEx({ _panelX+10.f, kContentY, panelW-20.f, 24.f }, 1.f,
            addHov ? WHITE : Fade(WHITE, 0.25f));
        const char* addLbl = "+ Add Static Prop";
        DrawText(addLbl,
            (int)(_panelX+10.f+(panelW-20.f)*0.5f-MeasureText(addLbl,13)*0.5f),
            (int)(kContentY+5.f), 13, WHITE);

        // ── Anim prop builder ─────────────────────────────────────────────────
        DrawLineEx({ _panelX+10.f, kContentY+28.f }, { _panelX+panelW-10.f, kContentY+28.f },
            1.f, Fade(ORANGE, 0.3f));
        DrawText("Anim Prop Builder", (int)(_panelX+12.f), (int)(kContentY+30.f),
            10, Fade(ORANGE, 0.7f));

        float abY = kContentY + 44.f;
        bool frameHov = CheckCollisionPointRec(mouse2,
            { _panelX+10.f, abY, panelW-20.f, 24.f }) && _hasSelection;
        DrawRectangleRec({ _panelX+10.f, abY, panelW-20.f, 24.f },
            frameHov ? Color{160,80,40,230} : Color{100,50,20,180});
        DrawRectangleLinesEx({ _panelX+10.f, abY, panelW-20.f, 24.f }, 1.f,
            frameHov ? WHITE : Fade(WHITE, 0.25f));
        const char* frameLbl = "+ Add Frame";
        DrawText(frameLbl,
            (int)(_panelX+10.f+(panelW-20.f)*0.5f-MeasureText(frameLbl,13)*0.5f),
            (int)(abY+5.f), 13, _hasSelection ? ORANGE : Fade(ORANGE, 0.4f));

        // FPS stepper
        float fpsY = kContentY + 72.f;
        DrawText("FPS:", (int)(_panelX+12.f), (int)(fpsY+2.f), 11, Fade(WHITE, 0.7f));
        DrawRectangleRounded({ _panelX+52.f, fpsY, 18.f, 18.f }, 0.3f, 4, Fade(WHITE,0.12f));
        DrawText("<", (int)(_panelX+57.f), (int)(fpsY+2.f), 12, WHITE);
        DrawText(TextFormat("%.0f", _animPropFps), (int)(_panelX+74.f), (int)(fpsY+2.f), 12, GOLD);
        DrawRectangleRounded({ _panelX+88.f, fpsY, 18.f, 18.f }, 0.3f, 4, Fade(WHITE,0.12f));
        DrawText(">", (int)(_panelX+93.f), (int)(fpsY+2.f), 12, WHITE);

        // Pending frames thumbnail strip with per-frame X buttons
        float thumbY  = kContentY + 96.f;
        float thumbSz = 32.f;
        float thumbSlot = 36.f;
        int   nPending = (int)_pendingAnimPropFrames.size();
        if (nPending == 0)
        {
            DrawText("(select regions, click Add Frame)",
                (int)(_panelX+12.f), (int)(thumbY+8.f), 10, Fade(WHITE, 0.3f));
        }
        else
        {
            DrawText(TextFormat("%d frame%s:", nPending, nPending==1?"":"s"),
                (int)(_panelX+12.f), (int)(thumbY-2.f), 10, Fade(ORANGE, 0.8f));
            for (int i = 0; i < nPending; i++)
            {
                float tx = _panelX + 10.f + i * thumbSlot;
                if (tx + thumbSz > _panelX + panelW - 10.f)
                {
                    DrawText(TextFormat("+%d", nPending - i),
                        (int)(tx+2.f), (int)(thumbY+10.f), 11, Fade(ORANGE, 0.7f));
                    break;
                }
                if (_sheet.id != 0)
                    DrawTexturePro(_sheet, _pendingAnimPropFrames[i],
                        { tx, thumbY+10.f, thumbSz, thumbSz }, {}, 0.f, WHITE);
                DrawRectangleLinesEx({ tx, thumbY+10.f, thumbSz, thumbSz }, 1.f,
                    Fade(ORANGE, 0.5f));
                // X button (top-right corner of thumbnail)
                Rectangle xBtn{ tx+22.f, thumbY+10.f, 10.f, 10.f };
                bool xHov = CheckCollisionPointRec(mouse2, xBtn);
                DrawRectangleRec(xBtn, xHov ? Fade(RED,0.9f) : Fade(RED,0.55f));
                DrawText("x", (int)(xBtn.x+2.f), (int)(xBtn.y+1.f), 9, WHITE);
            }
        }

        // Finalize button
        float finalY  = kContentY + 134.f;
        bool hasPend  = nPending > 0;
        bool finalHov = hasPend && CheckCollisionPointRec(mouse2,
            { _panelX+10.f, finalY, panelW-20.f, 24.f });
        DrawRectangleRec({ _panelX+10.f, finalY, panelW-20.f, 24.f },
            finalHov ? Color{60,180,60,230}
            : hasPend ? Color{35,110,35,200} : Color{20,40,20,160});
        DrawRectangleLinesEx({ _panelX+10.f, finalY, panelW-20.f, 24.f }, 1.f,
            finalHov ? WHITE : hasPend ? Fade(WHITE,0.3f) : Fade(WHITE,0.12f));
        const char* fnLbl = hasPend ? "Finalize Anim Prop" : "Finalize  (add frames first)";
        DrawText(fnLbl,
            (int)(_panelX+10.f+(panelW-20.f)*0.5f-MeasureText(fnLbl,12)*0.5f),
            (int)(finalY+5.f), 12, hasPend ? WHITE : Fade(WHITE, 0.30f));

        DrawLineEx({ _panelX+10.f, kContentY+162.f },
            { _panelX+panelW-10.f, kContentY+162.f }, 1.f, Fade(WHITE,0.15f));

        // ── Prop list ─────────────────────────────────────────────────────────
        float propListY = kContentY + 166.f;
        BeginScissorMode((int)_panelX, (int)propListY, (int)panelW, (int)(sh - propListY));

        int totalPropRows = (int)_propDefs.size() + (int)_animPropDefs.size();
        if (totalPropRows == 0)
            DrawText("No props yet", (int)(_panelX+14.f), (int)(propListY+10.f),
                13, Fade(WHITE, 0.35f));

        for (int i = 0; i < (int)_propDefs.size(); i++)
        {
            float ry = propListY + i * kListRowH - _propScrollY;
            if (ry + kListRowH < propListY || ry > sh) continue;

            bool sel = (i == _editingPropIdx);
            Rectangle row{ _panelX+6.f, ry, panelW-12.f, kListRowH-2.f };
            DrawRectangleRec(row, sel ? Color{30,60,120,220} : Color{22,22,32,200});
            DrawRectangleLinesEx(row, 1.f, sel ? Color{80,160,255,255} : Fade(WHITE,0.15f));
            if (_sheet.id != 0)
            {
                Rectangle dst{ _panelX+12.f, ry+7.f, 36.f, 36.f };
                DrawTexturePro(_sheet, _propDefs[i].src, dst, {}, 0.f, WHITE);
                DrawRectangleLinesEx(dst, 1.f, Fade(WHITE, 0.3f));
            }
            DrawText(TextFormat("#%d  %dx%d", i,
                (int)_propDefs[i].src.width, (int)_propDefs[i].src.height),
                (int)(_panelX+54.f), (int)(ry+9.f), 12, RAYWHITE);
            DrawText(sel ? "[click to close]" : "[click to edit hitbox]",
                (int)(_panelX+54.f), (int)(ry+27.f), 10,
                sel ? Color{80,200,255,255} : Fade(WHITE,0.40f));
            Rectangle rb{ _panelX+panelW-36.f, ry+13.f, 26.f, 24.f };
            bool rh = CheckCollisionPointRec(mouse2, rb);
            DrawRectangleRec(rb, rh ? Fade(RED,0.85f) : Fade(RED,0.45f));
            DrawRectangleLinesEx(rb, 1.f, rh ? WHITE : Fade(WHITE,0.2f));
            DrawText("X", (int)(rb.x+rb.width*0.5f-MeasureText("X",13)*0.5f),
                (int)(rb.y+5.f), 13, WHITE);
        }

        int animOff = (int)_propDefs.size();
        for (int i = 0; i < (int)_animPropDefs.size(); i++)
        {
            float ry = propListY + (animOff + i) * kListRowH - _propScrollY;
            if (ry + kListRowH < propListY || ry > sh) continue;

            bool sel = (i == _editingAnimPropIdx);
            Rectangle row{ _panelX+6.f, ry, panelW-12.f, kListRowH-2.f };
            DrawRectangleRec(row, sel ? Color{70,35,10,220} : Color{28,18,10,200});
            DrawRectangleLinesEx(row, 1.f,
                sel ? Color{255,140,40,255} : Color{180,90,40,120});

            // Thumbnail — cycle live through defined frames
            if (_sheet.id != 0 && !_animPropDefs[i].frames.empty())
            {
                int fc    = (int)_animPropDefs[i].frames.size();
                int frame = (fc > 1 && _animPropDefs[i].fps > 0.f)
                    ? (int)(GetTime() * _animPropDefs[i].fps) % fc : 0;
                Rectangle dst{ _panelX+12.f, ry+7.f, 36.f, 36.f };
                DrawTexturePro(_sheet, _animPropDefs[i].frames[frame], dst, {}, 0.f, WHITE);
                DrawRectangleLinesEx(dst, 1.f, Fade(ORANGE, 0.5f));
            }
            int fc = (int)_animPropDefs[i].frames.size();
            DrawText(TextFormat("Anim #%d  %dfr  %.0ffps", i, fc, _animPropDefs[i].fps),
                (int)(_panelX+54.f), (int)(ry+9.f), 11, ORANGE);
            DrawText(sel ? "[click to close]" : "[click to edit hitbox]",
                (int)(_panelX+54.f), (int)(ry+27.f), 10,
                sel ? Color{255,160,60,255} : Fade(ORANGE,0.55f));
            Rectangle rb{ _panelX+panelW-36.f, ry+13.f, 26.f, 24.f };
            bool rh = CheckCollisionPointRec(mouse2, rb);
            DrawRectangleRec(rb, rh ? Fade(RED,0.85f) : Fade(RED,0.45f));
            DrawRectangleLinesEx(rb, 1.f, rh ? WHITE : Fade(WHITE,0.2f));
            DrawText("X", (int)(rb.x+rb.width*0.5f-MeasureText("X",13)*0.5f),
                (int)(rb.y+5.f), 13, WHITE);
        }

        EndScissorMode();
        if (totalPropRows > 4)
            DrawText(TextFormat("[Scroll]  %d props", totalPropRows),
                (int)(_panelX+10.f), (int)(sh-22.f), 11, Fade(WHITE,0.35f));
    }

    if (_panelTab == PanelTab::Decors)
    {
        // Static add button
        bool addHov = CheckCollisionPointRec(GetVirtualMousePos(),
            { _panelX + 10.f, kContentY, panelW - 20.f, 24.f }) && _hasSelection;
        DrawRectangleRec({ _panelX + 10.f, kContentY, panelW - 20.f, 24.f },
            addHov ? Color{40,160,40,230} : Color{25,100,25,180});
        DrawRectangleLinesEx({ _panelX + 10.f, kContentY, panelW - 20.f, 24.f }, 1.f,
            addHov ? WHITE : Fade(WHITE, 0.25f));
        const char* addLbl = "+ Add Static Decor";
        DrawText(addLbl,
            (int)(_panelX + 10.f + (panelW-20.f)*0.5f - MeasureText(addLbl,12)*0.5f),
            (int)(kContentY + 5.f), 12, WHITE);

        // ── Anim decor builder header ─────────────────────────────────────────
        DrawLineEx({ _panelX+10.f, kContentY+28.f }, { _panelX+panelW-10.f, kContentY+28.f },
            1.f, Fade(ORANGE, 0.3f));
        DrawText("Anim Decor Builder", (int)(_panelX+12.f), (int)(kContentY+30.f),
            10, Fade(ORANGE, 0.7f));

        float abY = kContentY + 44.f;
        bool frameHov = CheckCollisionPointRec(GetVirtualMousePos(),
            { _panelX+10.f, abY, panelW-20.f, 24.f }) && _hasSelection;
        DrawRectangleRec({ _panelX+10.f, abY, panelW-20.f, 24.f },
            frameHov ? Color{160,80,40,230} : Color{100,50,20,180});
        DrawRectangleLinesEx({ _panelX+10.f, abY, panelW-20.f, 24.f }, 1.f,
            frameHov ? WHITE : Fade(WHITE, 0.25f));
        const char* frameLbl = "+ Add Frame";
        DrawText(frameLbl,
            (int)(_panelX+10.f+(panelW-20.f)*0.5f-MeasureText(frameLbl,13)*0.5f),
            (int)(abY+5.f), 13, _hasSelection ? ORANGE : Fade(ORANGE, 0.4f));

        // FPS stepper
        float fpsY = kContentY + 72.f;
        DrawText("FPS:", (int)(_panelX+12.f), (int)(fpsY+2.f), 11, Fade(WHITE, 0.7f));
        DrawRectangleRounded({ _panelX+52.f, fpsY, 18.f, 18.f }, 0.3f, 4, Fade(WHITE,0.12f));
        DrawText("<", (int)(_panelX+57.f), (int)(fpsY+2.f), 12, WHITE);
        DrawText(TextFormat("%.0f", _animDecorFps), (int)(_panelX+74.f), (int)(fpsY+2.f), 12, GOLD);
        DrawRectangleRounded({ _panelX+88.f, fpsY, 18.f, 18.f }, 0.3f, 4, Fade(WHITE,0.12f));
        DrawText(">", (int)(_panelX+93.f), (int)(fpsY+2.f), 12, WHITE);

        // Pending frame thumbnail strip with per-frame X buttons
        float thumbY    = kContentY + 96.f;
        float thumbSz   = 32.f;
        float thumbSlot = 36.f;
        int   nPendD    = (int)_pendingAnimDecorFrames.size();
        if (nPendD == 0)
        {
            DrawText("(select regions, click Add Frame)",
                (int)(_panelX+12.f), (int)(thumbY+8.f), 10, Fade(WHITE, 0.3f));
        }
        else
        {
            DrawText(TextFormat("%d frame%s:", nPendD, nPendD==1?"":"s"),
                (int)(_panelX+12.f), (int)(thumbY-2.f), 10, Fade(ORANGE, 0.8f));
            for (int i = 0; i < nPendD; i++)
            {
                float tx = _panelX + 10.f + i * thumbSlot;
                if (tx + thumbSz > _panelX + panelW - 10.f)
                {
                    DrawText(TextFormat("+%d", nPendD - i),
                        (int)(tx+2.f), (int)(thumbY+10.f), 11, Fade(ORANGE, 0.7f));
                    break;
                }
                if (_sheet.id != 0)
                    DrawTexturePro(_sheet, _pendingAnimDecorFrames[i],
                        { tx, thumbY+10.f, thumbSz, thumbSz }, {}, 0.f, WHITE);
                DrawRectangleLinesEx({ tx, thumbY+10.f, thumbSz, thumbSz }, 1.f,
                    Fade(ORANGE, 0.5f));
                Rectangle xBtn{ tx+22.f, thumbY+10.f, 10.f, 10.f };
                bool xHov = CheckCollisionPointRec(GetVirtualMousePos(), xBtn);
                DrawRectangleRec(xBtn, xHov ? Fade(RED,0.9f) : Fade(RED,0.55f));
                DrawText("x", (int)(xBtn.x+2.f), (int)(xBtn.y+1.f), 9, WHITE);
            }
        }

        // Finalize button
        float finalY  = kContentY + 134.f;
        bool hasPendD = nPendD > 0;
        bool finalHov = hasPendD && CheckCollisionPointRec(GetVirtualMousePos(),
            { _panelX+10.f, finalY, panelW-20.f, 24.f });
        DrawRectangleRec({ _panelX+10.f, finalY, panelW-20.f, 24.f },
            finalHov ? Color{60,180,60,230}
            : hasPendD ? Color{35,110,35,200} : Color{20,40,20,160});
        DrawRectangleLinesEx({ _panelX+10.f, finalY, panelW-20.f, 24.f }, 1.f,
            finalHov ? WHITE : hasPendD ? Fade(WHITE,0.3f) : Fade(WHITE,0.12f));
        const char* fnLbl = hasPendD ? "Finalize Anim Decor" : "Finalize  (add frames first)";
        DrawText(fnLbl,
            (int)(_panelX+10.f+(panelW-20.f)*0.5f-MeasureText(fnLbl,12)*0.5f),
            (int)(finalY+5.f), 12, hasPendD ? WHITE : Fade(WHITE, 0.30f));

        DrawLineEx({ _panelX+10.f, kContentY+162.f },
            { _panelX+panelW-10.f, kContentY+162.f }, 1.f, Fade(WHITE,0.15f));

        // Unified scrollable list: static decors then animated decors
        float listY = kContentY + 166.f;
        BeginScissorMode((int)_panelX, (int)listY, (int)panelW, (int)(sh - listY));

        int totalRows = (int)_decorDefs.size() + (int)_animDecorDefs.size();
        if (totalRows == 0)
            DrawText("No decors yet", (int)(_panelX + 14.f), (int)(listY + 8.f),
                13, Fade(WHITE, 0.35f));

        for (int i = 0; i < (int)_decorDefs.size(); i++)
        {
            float ry = listY + i * kListRowH - _decorScrollY;
            if (ry + kListRowH < listY || ry > sh) continue;
            DrawRectangleRec({ _panelX+6.f, ry, panelW-12.f, kListRowH-2.f },
                Color{22,22,32,200});
            DrawRectangleLinesEx({ _panelX+6.f, ry, panelW-12.f, kListRowH-2.f },
                1.f, Fade(WHITE, 0.15f));
            if (_sheet.id != 0)
                DrawTexturePro(_sheet, _decorDefs[i],
                    { _panelX+12.f, ry+7.f, 36.f, 36.f }, {}, 0.f, WHITE);
            DrawText(TextFormat("Static #%d", i),
                (int)(_panelX+54.f), (int)(ry+15.f), 11, RAYWHITE);
            Rectangle rb{ _panelX+panelW-36.f, ry+13.f, 26.f, 24.f };
            DrawRectangleRec(rb, Fade(RED, 0.45f));
            DrawText("X", (int)(rb.x+rb.width*0.5f-MeasureText("X",13)*0.5f),
                (int)(rb.y+5.f), 13, WHITE);
        }

        for (int i = 0; i < (int)_animDecorDefs.size(); i++)
        {
            int row = (int)_decorDefs.size() + i;
            float ry = listY + row * kListRowH - _decorScrollY;
            if (ry + kListRowH < listY || ry > sh) continue;
            DrawRectangleRec({ _panelX+6.f, ry, panelW-12.f, kListRowH-2.f },
                Color{32,22,16,200});
            DrawRectangleLinesEx({ _panelX+6.f, ry, panelW-12.f, kListRowH-2.f },
                1.f, Color{180,90,40,120});
            if (_sheet.id != 0 && !_animDecorDefs[i].frames.empty())
                DrawTexturePro(_sheet, _animDecorDefs[i].frames[0],
                    { _panelX+12.f, ry+7.f, 36.f, 36.f }, {}, 0.f, WHITE);
            DrawText(TextFormat("Anim #%d  %dfr %.0ffps", i,
                    (int)_animDecorDefs[i].frames.size(), _animDecorDefs[i].fps),
                (int)(_panelX+54.f), (int)(ry+15.f), 11, ORANGE);
            Rectangle rb{ _panelX+panelW-36.f, ry+13.f, 26.f, 24.f };
            DrawRectangleRec(rb, Fade(RED, 0.45f));
            DrawText("X", (int)(rb.x+rb.width*0.5f-MeasureText("X",13)*0.5f),
                (int)(rb.y+5.f), 13, WHITE);
        }

        EndScissorMode();

        if (totalRows > 4)
            DrawText(TextFormat("[Scroll]  %d decors", totalRows),
                (int)(_panelX + 10.f), (int)(sh - 22.f), 11, Fade(WHITE, 0.35f));
    }

    // Save hint (always visible)
    int ew2 = MeasureText("[S]  Save & Export", 15);
    if (_panelTab != PanelTab::Tiles)
        DrawText("[S]  Save & Export",
            (int)(_panelX + panelW * 0.5f - ew2 * 0.5f),
            (int)(sh - 38.f), 15, Fade(GOLD, 0.85f));
}

// ── Collision-box editor ──────────────────────────────────────────────────────

void TileMapper::GetCollEditorLayout(float& offX, float& offY, float& zoom) const
{
    const Rectangle* src = nullptr;
    if (_editingAnimPropIdx >= 0 && _editingAnimPropIdx < (int)_animPropDefs.size()
        && !_animPropDefs[_editingAnimPropIdx].frames.empty())
        src = &_animPropDefs[_editingAnimPropIdx].frames[0];
    else if (_editingPropIdx >= 0 && _editingPropIdx < (int)_propDefs.size())
        src = &_propDefs[_editingPropIdx].src;
    else
        { offX = offY = zoom = 0.f; return; }

    float availW = _panelX - 40.f;
    float availH = (float)kVirtualHeight - 120.f;
    zoom = std::min({ availW / src->width, availH / src->height, 24.f });
    zoom = std::max(zoom, 4.f);
    float dw = src->width  * zoom;
    float dh = src->height * zoom;
    offX = 20.f + (availW - dw) * 0.5f;
    offY = 60.f + (availH - dh) * 0.5f;
}

void TileMapper::HandleCollisionEditorMouse()
{
    Rectangle* src  = nullptr;
    Rectangle* coll = nullptr;
    if (_editingAnimPropIdx >= 0 && _editingAnimPropIdx < (int)_animPropDefs.size()
        && !_animPropDefs[_editingAnimPropIdx].frames.empty())
    {
        src  = &_animPropDefs[_editingAnimPropIdx].frames[0];
        coll = &_animPropDefs[_editingAnimPropIdx].collision;
    }
    else if (_editingPropIdx >= 0 && _editingPropIdx < (int)_propDefs.size())
    {
        src  = &_propDefs[_editingPropIdx].src;
        coll = &_propDefs[_editingPropIdx].collision;
    }
    else return;

    const Rectangle& srcRef  = *src;
    Rectangle&       collRef = *coll;

    float offX, offY, zoom;
    GetCollEditorLayout(offX, offY, zoom);

    float cx = offX + collRef.x * zoom;
    float cy = offY + collRef.y * zoom;
    float cw = collRef.width  * zoom;
    float ch = collRef.height * zoom;

    Vector2 mouse = GetVirtualMousePos();

    // Handle proximity — checked only when not already dragging
    if (!_collDragging)
    {
        struct { CollHandle h; float px, py; } pts[] = {
            { CollHandle::TL, cx,        cy        },
            { CollHandle::TC, cx+cw*0.5f,cy        },
            { CollHandle::TR, cx+cw,     cy        },
            { CollHandle::ML, cx,        cy+ch*0.5f},
            { CollHandle::MR, cx+cw,     cy+ch*0.5f},
            { CollHandle::BL, cx,        cy+ch     },
            { CollHandle::BC, cx+cw*0.5f,cy+ch     },
            { CollHandle::BR, cx+cw,     cy+ch     },
        };
        _collHandle = CollHandle::None;
        for (auto& p : pts)
            if (CheckCollisionPointRec(mouse, {p.px-8.f, p.py-8.f, 16.f, 16.f}))
                { _collHandle = p.h; break; }
        if (_collHandle == CollHandle::None &&
            CheckCollisionPointRec(mouse, {cx, cy, cw, ch}))
            _collHandle = CollHandle::Body;

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && _collHandle != CollHandle::None)
        {
            _collDragging  = true;
            _collDragStart = mouse;
            _collDragOrig  = collRef;
        }
    }

    if (_collDragging && IsMouseButtonDown(MOUSE_LEFT_BUTTON))
    {
        float dx = (mouse.x - _collDragStart.x) / zoom;
        float dy = (mouse.y - _collDragStart.y) / zoom;
        Rectangle c = _collDragOrig;
        switch (_collHandle)
        {
        case CollHandle::TL: c.x+=dx; c.y+=dy; c.width-=dx; c.height-=dy; break;
        case CollHandle::TC:           c.y+=dy;               c.height-=dy; break;
        case CollHandle::TR:           c.y+=dy; c.width+=dx;  c.height-=dy; break;
        case CollHandle::ML: c.x+=dx;           c.width-=dx;               break;
        case CollHandle::MR:                     c.width+=dx;               break;
        case CollHandle::BL: c.x+=dx;           c.width-=dx; c.height+=dy; break;
        case CollHandle::BC:                                  c.height+=dy; break;
        case CollHandle::BR:                     c.width+=dx; c.height+=dy; break;
        case CollHandle::Body: c.x+=dx; c.y+=dy;                           break;
        default: break;
        }
        c.width  = std::clamp(c.width,  1.f, srcRef.width);
        c.height = std::clamp(c.height, 1.f, srcRef.height);
        c.x = std::clamp(c.x, 0.f, srcRef.width  - c.width);
        c.y = std::clamp(c.y, 0.f, srcRef.height - c.height);
        collRef = c;
    }

    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
        _collDragging = false;
}

void TileMapper::DrawCollisionEditor() const
{
    const Rectangle* srcPtr  = nullptr;
    const Rectangle* collPtr = nullptr;
    bool             isAnim  = false;
    int              frameCount = 1;
    float            fps        = 8.f;
    int              editIdx    = -1;

    if (_editingAnimPropIdx >= 0 && _editingAnimPropIdx < (int)_animPropDefs.size()
        && !_animPropDefs[_editingAnimPropIdx].frames.empty())
    {
        srcPtr     = &_animPropDefs[_editingAnimPropIdx].frames[0];
        collPtr    = &_animPropDefs[_editingAnimPropIdx].collision;
        isAnim     = true;
        frameCount = (int)_animPropDefs[_editingAnimPropIdx].frames.size();
        fps        = _animPropDefs[_editingAnimPropIdx].fps;
        editIdx    = _editingAnimPropIdx;
    }
    else if (_editingPropIdx >= 0 && _editingPropIdx < (int)_propDefs.size())
    {
        srcPtr  = &_propDefs[_editingPropIdx].src;
        collPtr = &_propDefs[_editingPropIdx].collision;
        editIdx = _editingPropIdx;
    }
    else return;

    const Rectangle& srcRect  = *srcPtr;
    const Rectangle& coll     = *collPtr;

    float offX, offY, zoom;
    GetCollEditorLayout(offX, offY, zoom);

    float spriteW = srcRect.width  * zoom;
    float spriteH = srcRect.height * zoom;

    ClearBackground(Color{ 10, 10, 14, 255 });

    if (isAnim)
        DrawText(TextFormat("Collision Editor  —  Anim Prop #%d  (%dx%d px  %dfr  %.0ffps)",
            editIdx, (int)srcRect.width, (int)srcRect.height, frameCount, fps),
            20, 12, 16, ORANGE);
    else
        DrawText(TextFormat("Collision Editor  —  Prop #%d  (%dx%d px source)",
            editIdx, (int)srcRect.width, (int)srcRect.height),
            20, 12, 16, GOLD);
    DrawText("[ESC] Done  •  [S] Save", 20, 34, 13, Fade(WHITE, 0.45f));

    // For animated props, cycle the live frame so you can see every frame
    // while adjusting the hitbox. The hitbox applies to all frames.
    Rectangle drawSrc = srcRect;
    if (isAnim && frameCount > 1 && fps > 0.f)
    {
        int frame = (int)(GetTime() * fps) % frameCount;
        drawSrc = _animPropDefs[_editingAnimPropIdx].frames[frame];
    }

    // Checkerboard background behind sprite
    static constexpr int kCheck = 8;
    for (int ry = 0; ry < (int)spriteH; ry += kCheck)
        for (int rx = 0; rx < (int)spriteW; rx += kCheck)
        {
            bool light = ((rx / kCheck + ry / kCheck) % 2 == 0);
            DrawRectangle((int)(offX + rx), (int)(offY + ry),
                std::min(kCheck, (int)(spriteW - rx)),
                std::min(kCheck, (int)(spriteH - ry)),
                light ? Color{55,55,55,255} : Color{35,35,35,255});
        }

    // Sprite (animated props cycle live; static props show the fixed frame)
    if (_sheet.id != 0)
        DrawTexturePro(_sheet, drawSrc,
            { offX, offY, spriteW, spriteH }, {}, 0.f, WHITE);

    // Collision rect overlay
    float cx = offX + coll.x * zoom;
    float cy = offY + coll.y * zoom;
    float cw = coll.width  * zoom;
    float ch = coll.height * zoom;
    DrawRectangle((int)cx, (int)cy, (int)cw, (int)ch, Color{ 0, 120, 255, 55 });
    DrawRectangleLinesEx({ cx, cy, cw, ch }, 2.f, Color{ 60, 180, 255, 255 });

    // Resize handles
    struct { float px, py; CollHandle h; } handles[] = {
        { cx,        cy,         CollHandle::TL },
        { cx+cw*0.5f,cy,         CollHandle::TC },
        { cx+cw,     cy,         CollHandle::TR },
        { cx,        cy+ch*0.5f, CollHandle::ML },
        { cx+cw,     cy+ch*0.5f, CollHandle::MR },
        { cx,        cy+ch,      CollHandle::BL },
        { cx+cw*0.5f,cy+ch,      CollHandle::BC },
        { cx+cw,     cy+ch,      CollHandle::BR },
    };
    for (auto& h : handles)
    {
        bool hot = (h.h == _collHandle);
        DrawRectangle((int)(h.px-5.f), (int)(h.py-5.f), 10, 10, hot ? YELLOW : WHITE);
        DrawRectangleLinesEx({ h.px-5.f, h.py-5.f, 10.f, 10.f }, 1.f,
            hot ? ORANGE : Fade(BLACK, 0.5f));
    }

    // Info text below sprite
    float infoY = offY + spriteH + 14.f;
    DrawText(TextFormat("Collision:  x=%.0f  y=%.0f  w=%.0f  h=%.0f",
        coll.x, coll.y, coll.width, coll.height),
        20, (int)infoY, 14, SKYBLUE);
    DrawText("Drag handles to resize  •  Drag inside rect to move",
        20, (int)(infoY + 22.f), 13, Fade(WHITE, 0.45f));
    if (isAnim)
        DrawText("(animation cycling live — hitbox applies to all frames)",
            20, (int)(infoY + 44.f), 12, Fade(ORANGE, 0.75f));
}

// ── Coordinate helpers ────────────────────────────────────────────────────────

Vector2 TileMapper::ScreenToGrid(Vector2 screen) const
{
    float cellPx = kTileSize * _scale;
    return { (screen.x - _offX) / cellPx, (screen.y - _offY) / cellPx };
}

Rectangle TileMapper::GridToScreen(int col, int row, int spanCols, int spanRows) const
{
    float cellPx = kTileSize * _scale;
    return { _offX + col * cellPx, _offY + row * cellPx,
             spanCols * cellPx, spanRows * cellPx };
}

float TileMapper::GroundSheetScreenY() const
{
    return _offY + (_sheetRows * kTileSize + kGroundGap) * _scale;
}

Rectangle TileMapper::GridToGroundScreen(int col, int row, int spanCols, int spanRows) const
{
    float cellPx = kTileSize * _scale;
    float gy     = GroundSheetScreenY();
    return { _offX + col * cellPx, gy + row * cellPx,
             spanCols * cellPx, spanRows * cellPx };
}
