#include "NineSliceEditor.h"
#include "NineSlice.h"
#include "raymath.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

// ── Init / Unload ─────────────────────────────────────────────────────────────

void NineSliceEditor::Init(const char* assetsRoot)
{
    _assetsRoot   = assetsRoot ? assetsRoot : "";
    _screen       = Screen::FileSelect;
    _wantsToExit  = false;
    _selectedIdx  = -1;
    _openIdx      = -1;
    _listScrollY  = 0.f;
    _filterBuf[0] = '\0';

    if (_tex.id != 0) { UnloadTexture(_tex); _tex = {}; }

    ScanFolder(assetsRoot);
}

void NineSliceEditor::Unload()
{
    if (_tex.id != 0) { UnloadTexture(_tex); _tex = {}; }
}

// ── File scanning ─────────────────────────────────────────────────────────────

void NineSliceEditor::ScanFolder(const char* root)
{
    _files.clear();
    if (!root || root[0] == '\0') return;

    try
    {
        for (const auto& entry : fs::directory_iterator(root))
        {
            if (!entry.is_regular_file()) continue;

            auto ext = entry.path().extension().string();
            for (char& c : ext) c = (char)tolower((unsigned char)c);
            if (ext != ".png") continue;

            PngFile f;
            f.fullPath = entry.path().string();
            f.stem     = entry.path().stem().string();
            f.relName  = f.stem + ".png";

            TryLoad(f);
            _files.push_back(std::move(f));
        }
    }
    catch (...) {}

    std::sort(_files.begin(), _files.end(),
        [](const PngFile& a, const PngFile& b) { return a.relName < b.relName; });
}

// ── Per-frame routing ─────────────────────────────────────────────────────────

void NineSliceEditor::Update()
{
    if (_screen == Screen::FileSelect) UpdateFileSelect();
    else                               UpdateEditor();
}

void NineSliceEditor::Draw() const
{
    if (_screen == Screen::FileSelect) DrawFileSelect();
    else                               DrawEditor();
}

// ── Screen 1 — File Select ────────────────────────────────────────────────────

void NineSliceEditor::UpdateFileSelect()
{
    if (IsKeyPressed(KEY_ESCAPE)) { _wantsToExit = true; return; }

    float sw  = (float)GetScreenWidth();
    float sh  = (float)GetScreenHeight();
    Vector2 m = GetMousePosition();

    if (IsKeyPressed(KEY_BACKSPACE) && _filterBuf[0] != '\0')
        _filterBuf[strlen(_filterBuf) - 1] = '\0';
    else
    {
        int ch  = GetCharPressed();
        int len = (int)strlen(_filterBuf);
        if (ch >= 32 && ch < 127 && len < (int)sizeof(_filterBuf) - 1)
        {
            _filterBuf[len]     = (char)ch;
            _filterBuf[len + 1] = '\0';
            _selectedIdx = -1;
            _listScrollY = 0.f;
        }
    }

    const float listX = sw * 0.04f;
    const float listY = 120.f;
    const float listW = sw * 0.92f;
    const float rowH  = kListRowH;
    const float rowG  = kListRowGap;

    _listScrollY -= GetMouseWheelMove() * (rowH + rowG) * 3.f;
    _listScrollY  = std::max(_listScrollY, 0.f);

    int visible = 0;
    for (int i = 0; i < (int)_files.size(); ++i)
    {
        if (_filterBuf[0] != '\0')
        {
            std::string lName = _files[i].relName;
            std::string lFilt = _filterBuf;
            for (char& c : lName) c = (char)tolower((unsigned char)c);
            for (char& c : lFilt) c = (char)tolower((unsigned char)c);
            if (lName.find(lFilt) == std::string::npos) continue;
        }

        float ry = listY + visible * (rowH + rowG) - _listScrollY;
        visible++;

        if (ry + rowH < listY || ry > sh - 80.f) continue;

        Rectangle row{ listX, ry, listW, rowH };
        if (CheckCollisionPointRec(m, row) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            _selectedIdx = i;
    }

    Rectangle openBtn{ sw * 0.5f - 130.f, sh - 66.f, 260.f, 48.f };
    if (_selectedIdx >= 0 && _selectedIdx < (int)_files.size())
    {
        if (CheckCollisionPointRec(m, openBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            OpenSelected();
    }
}

void NineSliceEditor::DrawFileSelect() const
{
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();

    ClearBackground(Color{ 12, 12, 18, 255 });
    DrawText("9-SLICE EDITOR  —  Select a PNG", 36, 16, 28, GOLD);
    DrawText("[ESC] Back to Menu", 36, 50, 16, Fade(WHITE, 0.45f));

    {
        const char* label = "Filter: ";
        int lw = MeasureText(label, 18);
        float bx = sw * 0.04f;
        float by = 80.f;
        DrawText(label, (int)bx, (int)by + 3, 18, Fade(WHITE, 0.6f));

        Rectangle box{ bx + lw, by, 320.f, 26.f };
        DrawRectangleRec(box, Color{ 30, 30, 40, 255 });
        DrawRectangleLinesEx(box, 1.f, Fade(WHITE, 0.3f));

        char display[130];
        snprintf(display, sizeof(display), "%s|", _filterBuf);
        DrawText(display, (int)box.x + 6, (int)box.y + 4, 16, WHITE);
    }

    if (_files.empty())
    {
        DrawText("No PNG files found.", 36, 130, 22, RED);
        return;
    }

    const float listX = sw * 0.04f;
    const float listY = 120.f;
    const float listW = sw * 0.92f;
    const float rowH  = kListRowH;
    const float rowG  = kListRowGap;

    int visible = 0;
    for (int i = 0; i < (int)_files.size(); ++i)
    {
        if (_filterBuf[0] != '\0')
        {
            std::string lName = _files[i].relName;
            std::string lFilt = _filterBuf;
            for (char& c : lName) c = (char)tolower((unsigned char)c);
            for (char& c : lFilt) c = (char)tolower((unsigned char)c);
            if (lName.find(lFilt) == std::string::npos) continue;
        }

        float ry = listY + visible * (rowH + rowG) - _listScrollY;
        visible++;

        if (ry + rowH < listY || ry > sh - 80.f) continue;

        bool  sel = (i == _selectedIdx);
        Color bg  = sel ? Color{ 60, 80, 120, 255 } : Color{ 24, 24, 36, 200 };
        Color bd  = sel ? GOLD : Fade(WHITE, 0.15f);

        DrawRectangle((int)listX, (int)ry, (int)listW, (int)rowH, bg);
        DrawRectangleLinesEx({ listX, ry, listW, rowH }, 1.f, bd);
        DrawText(_files[i].relName.c_str(), (int)listX + 12, (int)ry + 7, 18,
                 sel ? WHITE : Fade(WHITE, 0.75f));

        if (_files[i].hasSave)
        {
            const PngFile& f = _files[i];
            char info[80];
            snprintf(info, sizeof(info), "T%.0f B%.0f L%.0f R%.0f  dst%.0f",
                     f.srcTop, f.srcBot, f.srcLeft, f.srcRight, f.dstCorner);
            int iw = MeasureText(info, 15);
            DrawText(info, (int)(listX + listW - iw - 12), (int)ry + 9, 15, LIME);
        }
        else
        {
            DrawText("no save", (int)(listX + listW - 70), (int)ry + 9, 14, Fade(WHITE, 0.3f));
        }
    }

    if (_selectedIdx >= 0 && _selectedIdx < (int)_files.size())
    {
        Rectangle openBtn{ sw * 0.5f - 130.f, sh - 66.f, 260.f, 48.f };
        Vector2 m = GetMousePosition();
        bool hov  = CheckCollisionPointRec(m, openBtn);
        DrawRectangleRec(openBtn, hov ? Color{ 70, 130, 200, 255 } : Color{ 40, 90, 160, 255 });
        DrawRectangleLinesEx(openBtn, 1.5f, Fade(WHITE, 0.5f));
        const char* txt = "Open in Editor";
        int tw = MeasureText(txt, 22);
        DrawText(txt, (int)(openBtn.x + openBtn.width * 0.5f - tw * 0.5f),
                 (int)(openBtn.y + 12), 22, WHITE);
    }
}

void NineSliceEditor::OpenSelected()
{
    if (_selectedIdx < 0 || _selectedIdx >= (int)_files.size()) return;

    PngFile& f = _files[_selectedIdx];

    if (_tex.id != 0) { UnloadTexture(_tex); _tex = {}; }
    _tex = LoadTexture(f.fullPath.c_str());
    if (_tex.id == 0) return;

    _openIdx   = _selectedIdx;
    _srcTop    = f.srcTop;
    _srcBot    = f.srcBot;
    _srcLeft   = f.srcLeft;
    _srcRight  = f.srcRight;
    _dstCorner = f.dstCorner;

    float sw     = (float)GetScreenWidth();
    float sh     = (float)GetScreenHeight();
    float panelW = sw * kTexFrac - 20.f;
    float panelH = sh - kTopBarH - kBotBarH - 20.f;
    float scaleX = panelW / _tex.width;
    float scaleY = panelH / _tex.height;
    _texScale    = std::min(scaleX, scaleY);
    _texOffX     = 10.f + (panelW - _tex.width  * _texScale) * 0.5f;
    _texOffY     = kTopBarH + 10.f + (panelH - _tex.height * _texScale) * 0.5f;

    _midDrag     = false;
    _lineDragIdx = -1;
    for (int i = 0; i < 5; ++i) _barDrag[i] = false;

    _screen = Screen::Editor;
}

// ── Screen 2 — Editor ─────────────────────────────────────────────────────────

bool NineSliceEditor::DragFloat(float& val, float minV, float maxV,
                                float speed, Rectangle hitRect,
                                bool& dragging, float& dragStartX, float& dragStartVal)
{
    Vector2 m    = GetMousePosition();
    bool changed = false;

    if (!dragging && CheckCollisionPointRec(m, hitRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        dragging     = true;
        dragStartX   = m.x;
        dragStartVal = val;
    }
    if (dragging)
    {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            float newVal = dragStartVal + (m.x - dragStartX) * speed;
            newVal = std::max(minV, std::min(maxV, newVal));
            if (newVal != val) { val = newVal; changed = true; }
        }
        else { dragging = false; }
    }
    return changed;
}

void NineSliceEditor::UpdateEditor()
{
    if (IsKeyPressed(KEY_ESCAPE))
    {
        if (_tex.id != 0) { UnloadTexture(_tex); _tex = {}; }
        _screen = Screen::FileSelect;
        return;
    }

    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();

    Rectangle texArea{ 0.f, kTopBarH, sw * kTexFrac, sh - kTopBarH - kBotBarH };

    // ── Scroll-wheel zoom ─────────────────────────────────────────────────────
    float wheel = GetMouseWheelMove();
    if (wheel != 0.f && CheckCollisionPointRec(GetMousePosition(), texArea))
    {
        float zoomFactor = (wheel > 0.f) ? 1.12f : (1.f / 1.12f);
        float mx = GetMousePosition().x;
        float my = GetMousePosition().y;
        _texOffX = mx - (mx - _texOffX) * zoomFactor;
        _texOffY = my - (my - _texOffY) * zoomFactor;
        _texScale *= zoomFactor;
        _texScale = std::max(0.05f, std::min(64.f, _texScale));
    }

    // ── Middle-mouse pan ──────────────────────────────────────────────────────
    if (IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON) &&
        CheckCollisionPointRec(GetMousePosition(), texArea))
    {
        _midDrag      = true;
        _midDragStart = GetMousePosition();
        _midDragOff   = { _texOffX, _texOffY };
    }
    if (!IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) _midDrag = false;
    if (_midDrag)
    {
        Vector2 delta = Vector2Subtract(GetMousePosition(), _midDragStart);
        _texOffX = _midDragOff.x + delta.x;
        _texOffY = _midDragOff.y + delta.y;
    }

    // ── Grid-line drag (4 independent lines) ─────────────────────────────────
    if (_lineDragIdx < 0)
    {
        float texW = _tex.width  * _texScale;
        float texH = _tex.height * _texScale;

        float lineTop   = _texOffY + _srcTop   * _texScale;
        float lineBot   = _texOffY + texH - _srcBot   * _texScale;
        float lineLeft  = _texOffX + _srcLeft  * _texScale;
        float lineRight = _texOffX + texW - _srcRight * _texScale;

        Vector2 m = GetMousePosition();
        const float kSnap = 7.f;

        // Determine which line the mouse is closest to and within snap distance
        bool inTexX = m.x >= _texOffX && m.x <= _texOffX + texW;
        bool inTexY = m.y >= _texOffY && m.y <= _texOffY + texH;

        float dTop   = inTexX ? fabsf(m.y - lineTop)   : 9999.f;
        float dBot   = inTexX ? fabsf(m.y - lineBot)   : 9999.f;
        float dLeft  = inTexY ? fabsf(m.x - lineLeft)  : 9999.f;
        float dRight = inTexY ? fabsf(m.x - lineRight) : 9999.f;

        float best = std::min({ dTop, dBot, dLeft, dRight });
        int candidate = -1;
        if (best < kSnap)
        {
            if      (best == dTop)   candidate = 0;
            else if (best == dBot)   candidate = 1;
            else if (best == dLeft)  candidate = 2;
            else                     candidate = 3;
        }

        if (candidate >= 0 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)
            && CheckCollisionPointRec(m, texArea))
        {
            _lineDragIdx     = candidate;
            _lineDragStartX  = m.x;
            _lineDragStartY  = m.y;
            float* vals[4]   = { &_srcTop, &_srcBot, &_srcLeft, &_srcRight };
            _lineDragOrigVal = *vals[candidate];
        }
    }
    else
    {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            Vector2 m    = GetMousePosition();
            float maxT   = (float)_tex.height * 0.5f - 1.f;
            float maxL   = (float)_tex.width  * 0.5f - 1.f;

            switch (_lineDragIdx)
            {
            case 0: // top line — drag down = bigger
                _srcTop = std::max(1.f, std::min(maxT,
                    _lineDragOrigVal + (m.y - _lineDragStartY) / _texScale));
                break;
            case 1: // bottom line — drag up = bigger (negative delta)
                _srcBot = std::max(1.f, std::min(maxT,
                    _lineDragOrigVal - (m.y - _lineDragStartY) / _texScale));
                break;
            case 2: // left line — drag right = bigger
                _srcLeft = std::max(1.f, std::min(maxL,
                    _lineDragOrigVal + (m.x - _lineDragStartX) / _texScale));
                break;
            case 3: // right line — drag left = bigger (negative delta)
                _srcRight = std::max(1.f, std::min(maxL,
                    _lineDragOrigVal - (m.x - _lineDragStartX) / _texScale));
                break;
            }
        }
        else { _lineDragIdx = -1; }
    }

    // ── Bottom-bar drag-float controls ────────────────────────────────────────
    float barY   = sh - kBotBarH;
    float maxT   = (float)_tex.height * 0.5f - 1.f;
    float maxL   = (float)_tex.width  * 0.5f - 1.f;
    float boxW   = (sw - 40.f) / 5.f;   // 5 boxes spread across the bar

    // src top/bot/left/right + dst
    float* srcPtrs[5] = { &_srcTop, &_srcBot, &_srcLeft, &_srcRight, &_dstCorner };
    float  srcMin[5]  = { 1.f, 1.f, 1.f, 1.f, 1.f };
    float  srcMax[5]  = { maxT, maxT, maxL, maxL, 512.f };
    float  srcSpd[5]  = { 0.25f, 0.25f, 0.25f, 0.25f, 0.5f };

    for (int i = 0; i < 5; ++i)
    {
        Rectangle hit{ 20.f + i * boxW, barY + 24.f, boxW - 10.f, kBotBarH - 34.f };
        DragFloat(*srcPtrs[i], srcMin[i], srcMax[i], srcSpd[i], hit,
                  _barDrag[i], _barDragX[i], _barDragVal[i]);
    }

    // ── S key — print values to VS console in paste-ready format ─────────────
    if (IsKeyPressed(KEY_S) && _openIdx >= 0 && _openIdx < (int)_files.size())
    {
        const std::string& stem = _files[_openIdx].stem;
        printf("\n===== 9-Slice: %s =====\n", stem.c_str());
        if (_srcTop == _srcBot && _srcTop == _srcLeft && _srcTop == _srcRight)
        {
            printf("srcCorner = %.1ff   dstCorner = %.1ff\n", _srcTop, _dstCorner);
        }
        else
        {
            printf("srcTop=%.1ff  srcBot=%.1ff  srcLeft=%.1ff  srcRight=%.1ff  dstCorner=%.1ff\n",
                   _srcTop, _srcBot, _srcLeft, _srcRight, _dstCorner);
        }
        printf("=========================\n\n");
        fflush(stdout);
    }

    // ── Top-bar buttons ───────────────────────────────────────────────────────
    Vector2 m = GetMousePosition();

    Rectangle saveBtn{ sw - 130.f, 8.f, 110.f, kTopBarH - 16.f };
    if (CheckCollisionPointRec(m, saveBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        Save();

    Rectangle backBtn{ 8.f, 8.f, 90.f, kTopBarH - 16.f };
    if (CheckCollisionPointRec(m, backBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        if (_tex.id != 0) { UnloadTexture(_tex); _tex = {}; }
        _screen = Screen::FileSelect;
    }
}

void NineSliceEditor::DrawEditor() const
{
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();

    ClearBackground(Color{ 10, 10, 16, 255 });

    Rectangle topBar  { 0.f, 0.f,         sw, kTopBarH };
    Rectangle texArea { 0.f, kTopBarH,     sw * kTexFrac, sh - kTopBarH - kBotBarH };
    Rectangle prevArea{ sw * kTexFrac, kTopBarH, sw * (1.f - kTexFrac), sh - kTopBarH - kBotBarH };
    Rectangle botBar  { 0.f, sh - kBotBarH, sw, kBotBarH };

    DrawTexPanel(texArea);
    DrawPreviews(prevArea);
    DrawBottomBar(botBar);

    // ── Top bar ───────────────────────────────────────────────────────────────
    DrawRectangleRec(topBar, Color{ 20, 20, 30, 255 });
    DrawLine(0, (int)kTopBarH, (int)sw, (int)kTopBarH, Fade(WHITE, 0.2f));

    Rectangle backBtn{ 8.f, 8.f, 90.f, kTopBarH - 16.f };
    bool backHov = CheckCollisionPointRec(GetMousePosition(), backBtn);
    DrawRectangleRec(backBtn, backHov ? Color{ 60, 60, 80, 255 } : Color{ 40, 40, 55, 255 });
    DrawRectangleLinesEx(backBtn, 1.f, Fade(WHITE, 0.3f));
    DrawText("< Back", (int)backBtn.x + 8, (int)backBtn.y + 6, 16, WHITE);

    if (_openIdx >= 0 && _openIdx < (int)_files.size())
    {
        const char* name = _files[_openIdx].relName.c_str();
        int nw = MeasureText(name, 20);
        DrawText(name, (int)(sw * 0.5f - nw * 0.5f), (int)(kTopBarH * 0.5f - 10), 20,
                 Fade(WHITE, 0.85f));
    }

    Rectangle saveBtn{ sw - 130.f, 8.f, 110.f, kTopBarH - 16.f };
    bool saveHov = CheckCollisionPointRec(GetMousePosition(), saveBtn);
    DrawRectangleRec(saveBtn, saveHov ? Color{ 40, 160, 80, 255 } : Color{ 30, 120, 60, 255 });
    DrawRectangleLinesEx(saveBtn, 1.5f, Fade(WHITE, 0.4f));
    const char* saveTxt = "Save";
    int stw = MeasureText(saveTxt, 20);
    DrawText(saveTxt, (int)(saveBtn.x + saveBtn.width * 0.5f - stw * 0.5f),
             (int)(saveBtn.y + 7), 20, WHITE);
}

void NineSliceEditor::DrawTexPanel(Rectangle area) const
{
    DrawRectangleRec(area, Color{ 18, 18, 26, 255 });
    DrawRectangleLinesEx(area, 1.f, Fade(WHITE, 0.12f));

    if (_tex.id == 0) return;

    float texW = _tex.width  * _texScale;
    float texH = _tex.height * _texScale;

    // Checkerboard (shows transparency)
    {
        const int cell = 10;
        int cx = (int)(texW / cell) + 2;
        int cy = (int)(texH / cell) + 2;
        for (int gy = -1; gy <= cy; gy++)
        for (int gx = -1; gx <= cx; gx++)
        {
            float px = _texOffX + gx * cell;
            float py = _texOffY + gy * cell;
            float rx = std::max(px, area.x);
            float ry = std::max(py, area.y);
            float rw = std::min(px + cell, area.x + area.width)  - rx;
            float rh = std::min(py + cell, area.y + area.height) - ry;
            if (rw <= 0.f || rh <= 0.f) continue;
            Color c = ((gx + gy) % 2 == 0) ? Color{ 50, 50, 50, 255 } : Color{ 65, 65, 65, 255 };
            DrawRectangle((int)rx, (int)ry, (int)rw, (int)rh, c);
        }
    }

    BeginScissorMode((int)area.x, (int)area.y, (int)area.width, (int)area.height);
    {
        Rectangle src  = { 0.f, 0.f, (float)_tex.width, (float)_tex.height };
        Rectangle dest = { _texOffX, _texOffY, texW, texH };
        DrawTexturePro(_tex, src, dest, {}, 0.f, WHITE);

        // ── Four independent grid lines ───────────────────────────────────────
        float lineTop   = _texOffY + _srcTop   * _texScale;
        float lineBot   = _texOffY + texH - _srcBot   * _texScale;
        float lineLeft  = _texOffX + _srcLeft  * _texScale;
        float lineRight = _texOffX + texW - _srcRight * _texScale;

        // Corner region highlights (semi-transparent yellow over each corner)
        Color cornerFill = Color{ 255, 220, 60, 45 };
        DrawRectangle((int)_texOffX,   (int)_texOffY,   (int)(_srcLeft * _texScale), (int)(_srcTop * _texScale), cornerFill);
        DrawRectangle((int)lineRight,  (int)_texOffY,   (int)(_srcRight* _texScale), (int)(_srcTop * _texScale), cornerFill);
        DrawRectangle((int)_texOffX,   (int)lineBot,    (int)(_srcLeft * _texScale), (int)(_srcBot * _texScale), cornerFill);
        DrawRectangle((int)lineRight,  (int)lineBot,    (int)(_srcRight* _texScale), (int)(_srcBot * _texScale), cornerFill);

        // Lines — each a distinct colour so you know which one you grabbed
        //   top = YELLOW, bot = ORANGE, left = SKYBLUE, right = GREEN
        float lw = 1.5f;
        DrawLineEx({ _texOffX, lineTop  }, { _texOffX + texW, lineTop  }, lw, YELLOW);
        DrawLineEx({ _texOffX, lineBot  }, { _texOffX + texW, lineBot  }, lw, ORANGE);
        DrawLineEx({ lineLeft,  _texOffY }, { lineLeft,  _texOffY + texH }, lw, SKYBLUE);
        DrawLineEx({ lineRight, _texOffY }, { lineRight, _texOffY + texH }, lw, GREEN);

        DrawRectangleLinesEx(dest, 1.f, Fade(WHITE, 0.4f));

        // Per-line labels
        char buf[32];
        snprintf(buf, sizeof(buf), "T%.1f", _srcTop);
        DrawText(buf, (int)(_texOffX + 4), (int)lineTop + 3, 13, YELLOW);
        snprintf(buf, sizeof(buf), "B%.1f", _srcBot);
        DrawText(buf, (int)(_texOffX + 4), (int)lineBot + 3, 13, ORANGE);
        snprintf(buf, sizeof(buf), "L%.1f", _srcLeft);
        DrawText(buf, (int)lineLeft + 3, (int)(_texOffY + 4), 13, SKYBLUE);
        snprintf(buf, sizeof(buf), "R%.1f", _srcRight);
        DrawText(buf, (int)lineRight + 3, (int)(_texOffY + 4), 13, GREEN);
    }
    EndScissorMode();

    DrawText("Drag lines  |  Scroll=zoom  |  MMB=pan  |  [S] Print values  |  Yellow=Top  Orange=Bot  Blue=Left  Green=Right",
             (int)area.x + 8, (int)(area.y + area.height - 20), 13, Fade(WHITE, 0.4f));
}

void NineSliceEditor::DrawPreviews(Rectangle area) const
{
    DrawRectangleRec(area, Color{ 22, 22, 32, 255 });
    DrawRectangleLinesEx(area, 1.f, Fade(WHITE, 0.1f));

    if (_tex.id == 0) return;

    const float pad = 20.f;
    float px  = area.x + pad;
    float py  = area.y + pad;
    float aw  = area.width - pad * 2.f;

    DrawText("LIVE PREVIEWS", (int)px, (int)py, 18, Fade(GOLD, 0.75f));
    py += 28.f;

    auto drawPreview = [&](Rectangle r, const char* desc)
    {
        DrawRectangleRec({ r.x - 2.f, r.y - 2.f, r.width + 4.f, r.height + 4.f },
                         Color{ 8, 8, 12, 255 });
        DrawNineSliceEx(_tex, _srcTop, _srcBot, _srcLeft, _srcRight, _dstCorner, r, WHITE);
        DrawRectangleLinesEx(r, 1.f, Fade(YELLOW, 0.35f));

        char info[80];
        snprintf(info, sizeof(info), "%s  (%.0fx%.0f)", desc, r.width, r.height);
        DrawText(info, (int)r.x, (int)(r.y + r.height + 5), 13, Fade(WHITE, 0.5f));
    };

    Rectangle wide{ px, py, aw, 80.f };
    drawPreview(wide, "wide panel");
    py += 80.f + 28.f;

    float sqSide = std::min(aw * 0.6f, 200.f);
    Rectangle square{ px, py, sqSide, sqSide };
    drawPreview(square, "square box");
    py += sqSide + 28.f;

    float tallH = area.y + area.height - py - pad;
    if (tallH > 60.f)
    {
        float tallW = std::min(aw * 0.35f, 130.f);
        Rectangle tall{ px, py, tallW, tallH };
        drawPreview(tall, "tall / narrow");
    }

    // Value readout
    char vals[96];
    snprintf(vals, sizeof(vals), "T%.1f  B%.1f  L%.1f  R%.1f  dst%.1f",
             _srcTop, _srcBot, _srcLeft, _srcRight, _dstCorner);
    int vw = MeasureText(vals, 13);
    DrawText(vals, (int)(area.x + area.width - vw - 12), (int)(area.y + 10), 13, LIME);
}

void NineSliceEditor::DrawBottomBar(Rectangle area) const
{
    DrawRectangleRec(area, Color{ 20, 20, 30, 255 });
    DrawLine((int)area.x, (int)area.y, (int)(area.x + area.width), (int)area.y, Fade(WHITE, 0.2f));

    float sw   = (float)GetScreenWidth();
    float boxW = (sw - 40.f) / 5.f;

    const char* labels[5] = { "srcTop", "srcBot", "srcLeft", "srcRight", "dstCorner" };
    Color       cols[5]   = { YELLOW, ORANGE, SKYBLUE, GREEN, Fade(WHITE, 0.9f) };
    float vals[5] = { _srcTop, _srcBot, _srcLeft, _srcRight, _dstCorner };

    for (int i = 0; i < 5; ++i)
    {
        float bx = 20.f + i * boxW;
        Rectangle hit{ bx, area.y + 24.f, boxW - 10.f, area.height - 34.f };
        bool hov = CheckCollisionPointRec(GetMousePosition(), hit) || _barDrag[i];

        DrawRectangleRec(hit, hov ? Color{ 50, 50, 70, 255 } : Color{ 30, 30, 45, 255 });
        DrawRectangleLinesEx(hit, 1.f, Fade(WHITE, hov ? 0.5f : 0.2f));

        DrawText(labels[i], (int)bx, (int)(area.y + 6), 14, Fade(cols[i], 0.75f));

        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f", vals[i]);
        int bw = MeasureText(buf, 22);
        DrawText(buf, (int)(hit.x + hit.width * 0.5f - bw * 0.5f),
                 (int)(hit.y + hit.height * 0.5f - 11), 22, cols[i]);
    }
}

// ── Save / Load ───────────────────────────────────────────────────────────────

std::string NineSliceEditor::SavePath(const PngFile& f) const
{
    // Save next to the PNG so the file is always found regardless of working directory.
    fs::path pngPath(f.fullPath);
    return (pngPath.parent_path() / ("nineslice_" + f.stem + ".txt")).string();
}

void NineSliceEditor::Save()
{
    if (_openIdx < 0 || _openIdx >= (int)_files.size()) return;

    PngFile& f  = _files[_openIdx];
    f.srcTop    = _srcTop;
    f.srcBot    = _srcBot;
    f.srcLeft   = _srcLeft;
    f.srcRight  = _srcRight;
    f.dstCorner = _dstCorner;
    f.hasSave   = true;

    FILE* fp = nullptr;
    fopen_s(&fp, SavePath(f).c_str(), "w");
    if (!fp) return;

    fprintf(fp, "srcTop=%.4f\n",    _srcTop);
    fprintf(fp, "srcBot=%.4f\n",    _srcBot);
    fprintf(fp, "srcLeft=%.4f\n",   _srcLeft);
    fprintf(fp, "srcRight=%.4f\n",  _srcRight);
    fprintf(fp, "dstCorner=%.4f\n", _dstCorner);
    fclose(fp);
}

void NineSliceEditor::TryLoad(PngFile& f)
{
    FILE* fp = nullptr;
    fopen_s(&fp, SavePath(f).c_str(), "r");
    if (!fp) return;

    char  key[32];
    float val;
    while (fscanf_s(fp, "%31[^=]=%f\n", key, (unsigned)sizeof(key), &val) == 2)
    {
        if (strcmp(key, "srcTop")    == 0) f.srcTop    = val;
        if (strcmp(key, "srcBot")    == 0) f.srcBot    = val;
        if (strcmp(key, "srcLeft")   == 0) f.srcLeft   = val;
        if (strcmp(key, "srcRight")  == 0) f.srcRight  = val;
        if (strcmp(key, "dstCorner") == 0) f.dstCorner = val;
        // Legacy: a single "srcCorner" fills all four edges
        if (strcmp(key, "srcCorner") == 0)
        { f.srcTop = f.srcBot = f.srcLeft = f.srcRight = val; }
    }
    fclose(fp);
    f.hasSave = true;
}
