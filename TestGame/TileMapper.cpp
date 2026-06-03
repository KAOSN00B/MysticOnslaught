#include "TileMapper.h"
#include "TileDefs.h"
#include "raymath.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

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

    if (_sheet.id != 0)
    {
        UnloadTexture(_sheet);
        _sheet = {};
    }

    ScanFolder(folderPath);
}

void TileMapper::Unload()
{
    if (_sheet.id != 0)
    {
        UnloadTexture(_sheet);
        _sheet = {};
    }
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

    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    Vector2 mouse = GetMousePosition();

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

    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();

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
        bool hov = CheckCollisionPointRec(GetMousePosition(), { bx, by, 260.f, 52.f });
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
    _decorDefs.clear();
    _propScrollY  = 0.f;
    _decorScrollY = 0.f;
    _panelTab     = PanelTab::Tiles;
    _hasSelection  = false;
    _isDragging    = false;
    _hoveredTypeIdx = -1;

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

    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    _panelX = sw * (1.f - kPanelFrac);

    float availW = _panelX - 20.f;
    float availH = sh - 30.f;
    _scale = std::min(availW / (float)_sheet.width,
                      availH / (float)_sheet.height);

    float drawnW = _sheet.width  * _scale;
    float drawnH = _sheet.height * _scale;
    _offX = 10.f + (availW - drawnW) * 0.5f;
    _offY = 10.f + (availH - drawnH) * 0.5f;
}

void TileMapper::UpdateMapping()
{
    if (IsKeyPressed(KEY_ESCAPE))
    {
        // Return to file select — unload sheet, keep assignments saved.
        if (_sheet.id != 0) { UnloadTexture(_sheet); _sheet = {}; }
        _assignments.clear();
        _hasSelection  = false;
        _openFileIdx   = -1;
        _screen        = Screen::FileSelect;
        // Re-scan so hasSave badges refresh.
        return;
    }
    if (IsKeyPressed(KEY_S))
        ExportAndSave();

    HandleMouseMapping();
}

void TileMapper::HandleMouseMapping()
{
    if (_sheet.id == 0) return;

    Vector2 mouse = GetMousePosition();
    float sw      = (float)GetScreenWidth();
    float sh      = (float)GetScreenHeight();
    float panelW  = sw - _panelX;

    float sheetRight  = _offX + _sheet.width  * _scale;
    float sheetBottom = _offY + _sheet.height * _scale;
    bool inSheet = (mouse.x >= _offX && mouse.x < sheetRight &&
                    mouse.y >= _offY && mouse.y < sheetBottom);

    if (inSheet)
    {
        Vector2 g  = ScreenToGrid(mouse);
        int gc = std::max(0, std::min((int)g.x, _sheetCols - 1));
        int gr = std::max(0, std::min((int)g.y, _sheetRows - 1));

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            { _isDragging = true; _dragC0 = _dragC1 = gc; _dragR0 = _dragR1 = gr; }
        if (_isDragging && IsMouseButtonDown(MOUSE_LEFT_BUTTON))
            { _dragC1 = gc; _dragR1 = gr; }
        if (_isDragging && IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
        {
            _isDragging   = false;
            _hasSelection = true;
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

        Rectangle addBtn{ _panelX + 10.f, kContentY, panelW - 20.f, kAddBtnH };
        if (CheckCollisionPointRec(mouse, addBtn) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && _hasSelection)
        {
            _propDefs.push_back({
                (float)(_selC0 * kTileSize), (float)(_selR0 * kTileSize),
                (float)((_selC1 - _selC0 + 1) * kTileSize),
                (float)((_selR1 - _selR0 + 1) * kTileSize) });
            _hasSelection = false;
        }

        for (int i = 0; i < (int)_propDefs.size(); i++)
        {
            float ry = kListY + i * kListRowH - _propScrollY;
            if (ry + kListRowH < kContentY || ry > sh) continue;
            Rectangle removeBtn{ _panelX + panelW - 36.f, ry + 13.f, 26.f, 24.f };
            if (CheckCollisionPointRec(mouse, removeBtn) &&
                IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            { _propDefs.erase(_propDefs.begin() + i); break; }
        }
    }

    // ── Decors tab ────────────────────────────────────────────────────────────
    if (_panelTab == PanelTab::Decors)
    {
        if (mouse.x > _panelX)
            _decorScrollY = std::max(0.f, _decorScrollY - GetMouseWheelMove() * 50.f);

        Rectangle addBtn{ _panelX + 10.f, kContentY, panelW - 20.f, kAddBtnH };
        if (CheckCollisionPointRec(mouse, addBtn) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && _hasSelection)
        {
            _decorDefs.push_back({
                (float)(_selC0 * kTileSize), (float)(_selR0 * kTileSize),
                (float)((_selC1 - _selC0 + 1) * kTileSize),
                (float)((_selR1 - _selR0 + 1) * kTileSize) });
            _hasSelection = false;
        }

        for (int i = 0; i < (int)_decorDefs.size(); i++)
        {
            float ry = kListY + i * kListRowH - _decorScrollY;
            if (ry + kListRowH < kContentY || ry > sh) continue;
            Rectangle removeBtn{ _panelX + panelW - 36.f, ry + 13.f, 26.f, 24.f };
            if (CheckCollisionPointRec(mouse, removeBtn) &&
                IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            { _decorDefs.erase(_decorDefs.begin() + i); break; }
        }
    }
}

void TileMapper::ConfirmSelection(int typeIdx)
{
    if (!_hasSelection || typeIdx < 0 || typeIdx >= kTypeCount) return;
    _assignments.erase(
        std::remove_if(_assignments.begin(), _assignments.end(),
            [this](const Assignment& a){ return a.col == _selC0 && a.row == _selR0; }),
        _assignments.end());
    Assignment a;
    a.col = _selC0; a.row = _selR0;
    a.spanCols = _selC1 - _selC0 + 1;
    a.spanRows = _selR1 - _selR0 + 1;
    a.typeIdx  = typeIdx;
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
        TraceLog(LOG_INFO, "Prop[%d]          = { %3d, %3d, %3d, %3d };", i,
            (int)_propDefs[i].x, (int)_propDefs[i].y,
            (int)_propDefs[i].width, (int)_propDefs[i].height);
    for (int i = 0; i < (int)_decorDefs.size(); i++)
        TraceLog(LOG_INFO, "Decor[%d]         = { %3d, %3d, %3d, %3d };", i,
            (int)_decorDefs[i].x, (int)_decorDefs[i].y,
            (int)_decorDefs[i].width, (int)_decorDefs[i].height);
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
            fprintf(f, "TILE %d %d %d %d %d\n",
                a.col, a.row, a.spanCols, a.spanRows, a.typeIdx);
        }
        for (const Rectangle& r : _propDefs)
            fprintf(f, "PROP %.0f %.0f %.0f %.0f\n", r.x, r.y, r.width, r.height);
        for (const Rectangle& r : _decorDefs)
            fprintf(f, "DECOR %.0f %.0f %.0f %.0f\n", r.x, r.y, r.width, r.height);
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
        if (strcmp(tag, "TILE") == 0)
        {
            int col, row, sc, sr, ti;
            if (fscanf_s(f, "%d %d %d %d %d", &col, &row, &sc, &sr, &ti) != 5) continue;
            if (ti < 0 || ti >= kTypeCount) continue;
            Assignment a; a.col = col; a.row = row; a.spanCols = sc; a.spanRows = sr; a.typeIdx = ti;
            _assignments.push_back(a);
        }
        else if (strcmp(tag, "PROP") == 0)
        {
            float x, y, w, h;
            if (fscanf_s(f, "%f %f %f %f", &x, &y, &w, &h) != 4) continue;
            _propDefs.push_back({ x, y, w, h });
        }
        else if (strcmp(tag, "DECOR") == 0)
        {
            float x, y, w, h;
            if (fscanf_s(f, "%f %f %f %f", &x, &y, &w, &h) != 4) continue;
            _decorDefs.push_back({ x, y, w, h });
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
    ClearBackground(Color{ 18, 18, 24, 255 });
    DrawSheet();
    DrawGrid();
    DrawAssignments();
    DrawSelection();
    DrawPanel();

    const char* hint = "[Click] Select     [Drag] Multi-tile     [S] Save & Export     [ESC] Back to list";
    DrawText(hint, 10, GetScreenHeight() - 20, 14, Fade(WHITE, 0.4f));
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
}

void TileMapper::DrawGrid() const
{
    if (_sheet.id == 0) return;
    float cellPx = kTileSize * _scale;
    for (int c = 0; c <= _sheetCols; c++)
        DrawLineV({ _offX + c * cellPx, _offY },
                  { _offX + c * cellPx, _offY + _sheetRows * cellPx },
                  Fade(WHITE, 0.14f));
    for (int r = 0; r <= _sheetRows; r++)
        DrawLineV({ _offX, _offY + r * cellPx },
                  { _offX + _sheetCols * cellPx, _offY + r * cellPx },
                  Fade(WHITE, 0.14f));

    // Hovered cell highlight
    Vector2 mouse = GetMousePosition();
    if (mouse.x >= _offX && mouse.x < _offX + _sheet.width  * _scale &&
        mouse.y >= _offY && mouse.y < _offY + _sheet.height * _scale)
    {
        Vector2 g  = ScreenToGrid(mouse);
        int gc = std::max(0, std::min((int)g.x, _sheetCols - 1));
        int gr = std::max(0, std::min((int)g.y, _sheetRows - 1));
        DrawRectangleLinesEx(GridToScreen(gc, gr, 1, 1), 1.5f, Fade(YELLOW, 0.85f));
        DrawText(TextFormat("(%d, %d)", gc * kTileSize, gr * kTileSize),
            (int)(_offX + gc * cellPx + 2.f), (int)(_offY + gr * cellPx - 15.f),
            11, YELLOW);
    }
}

void TileMapper::DrawAssignments() const
{
    for (const Assignment& a : _assignments)
    {
        Rectangle r = GridToScreen(a.col, a.row, a.spanCols, a.spanRows);
        Color c = (a.typeIdx >= 0 && a.typeIdx < kTypeCount)
            ? kTypeColors[a.typeIdx] : Fade(WHITE, 0.15f);
        DrawRectangleRec(r, c);
        DrawRectangleLinesEx(r, 1.5f, Fade(WHITE, 0.5f));
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
    if (_isDragging)
    {
        c0 = std::min(_dragC0, _dragC1); r0 = std::min(_dragR0, _dragR1);
        c1 = std::max(_dragC0, _dragC1); r1 = std::max(_dragR0, _dragR1);
    }
    else { c0 = _selC0; r0 = _selR0; c1 = _selC1; r1 = _selR1; }

    Rectangle r = GridToScreen(c0, r0, c1 - c0 + 1, r1 - r0 + 1);
    DrawRectangleRec(r, Fade(SKYBLUE, 0.28f));
    DrawRectangleLinesEx(r, 2.f, SKYBLUE);
    DrawText(TextFormat("{ %d, %d, %d, %d }",
        c0 * kTileSize, r0 * kTileSize,
        (c1 - c0 + 1) * kTileSize, (r1 - r0 + 1) * kTileSize),
        (int)(r.x + 2.f), (int)(r.y - 16.f), 12, SKYBLUE);
}

void TileMapper::DrawPanel() const
{
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
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
        bool clearHov    = CheckCollisionPointRec(GetMousePosition(),
                               { _panelX + 10.f, clearY, panelW - 20.f, 26.f }) && _hasSelection;
        bool clearAllHov = CheckCollisionPointRec(GetMousePosition(),
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
        bool addHov = CheckCollisionPointRec(GetMousePosition(),
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
                bool remHov = CheckCollisionPointRec(GetMousePosition(), removeBtn);
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

    if (_panelTab == PanelTab::Props)
        drawSpriteList(_propDefs,  _propScrollY,  "+ Add Selected as Prop",  "No props yet");
    if (_panelTab == PanelTab::Decors)
        drawSpriteList(_decorDefs, _decorScrollY, "+ Add Selected as Decor", "No decors yet");

    // Save hint (always visible)
    int ew2 = MeasureText("[S]  Save & Export", 15);
    if (_panelTab != PanelTab::Tiles)
        DrawText("[S]  Save & Export",
            (int)(_panelX + panelW * 0.5f - ew2 * 0.5f),
            (int)(sh - 38.f), 15, Fade(GOLD, 0.85f));
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
