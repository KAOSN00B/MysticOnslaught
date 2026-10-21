#include "MainMenu.h"
#include "DevTools.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "VirtualCanvas.h"
#include "NineSlice.h"
#include "VirtualCanvas.h"
#include "Credits.h"
#include <algorithm>
#include <cmath>

// -- 9-slice corner sizes ------------------------------------------------------
static constexpr NineSliceCuts kMainBorderCuts{ 41, 42, 40, 42 };  // source pixels
static constexpr int kMainBorderScale = 5;
static constexpr int   kTitleBannerScale = 2;   // the banner is drawn whole, not sliced
static constexpr float BTN_SRC_CORNER    = 8.f;
static constexpr float BTN_DST_CORNER    = 16.f;

// The banner keeps its art pixels: its width is the art's width times a whole number.
static float TitleBannerWidth(const Texture2D& tex, float sw)
{
    return tex.id != 0 ? (float)(tex.width * kTitleBannerScale) : sw * 0.42f;
}

MainMenu::~MainMenu()
{
    if (_borderTex.id  != 0) UnloadTexture(_borderTex);
    if (_bannerTex.id  != 0) UnloadTexture(_bannerTex);
    if (_playBtnTex.id != 0) UnloadTexture(_playBtnTex);
    if (_continueBtnTex.id != 0) UnloadTexture(_continueBtnTex);
}

void MainMenu::Init()
{
    _buttons.clear();

    float sw = (float)kVirtualWidth;
    float sh = (float)kVirtualHeight;

    float buttonWidth  = sw * 0.20f;
    float buttonHeight = sh * 0.083f;
    float gap          = sh * 0.018f;

    float startX = sw / 2.f - buttonWidth / 2.f;

    // Y stored as offset from group top � draw adds _btnEdFirstY at runtime.
    // Rows stack from a running Y so hiding "Continue" (itch.io demo, which
    // never saves) closes the gap instead of leaving a hole.
    // A saved profile puts Continue first; a fresh profile shows only New Game
    // (Continue would have nothing to continue). Panel buttons always come
    // first in _buttons so gamepad indices map straight onto them.
    float rowY = 0.f;
    auto addPanelButton = [&](const char* text, float extraBelow = 0.f)
    {
        _buttons.push_back({ text, { startX, rowY, buttonWidth, buttonHeight } });
        rowY += buttonHeight + gap + extraBelow;
    };
#if !MO_ITCH_DEMO
    if (_hasProfile)
    {
        // Five buttons: a tighter column (the border grows to match, see
        // PanelBorderRect), plus room under Continue for the run caption.
        buttonHeight = sh * 0.062f;
        gap          = sh * 0.020f;
        addPanelButton("Continue", _runCaption.empty() ? 0.f : sh * 0.026f);
        addPanelButton("New Game");
    }
    else
#endif
        addPanelButton("New Game");
    addPanelButton("Settings");
    addPanelButton("Credits");
    addPanelButton("Quit");
    _buttons.push_back({ "Debug Mode",  { sw * 0.71f, sh * 0.54f, buttonWidth * 0.90f, sh * 0.083f * 0.95f } });

    // Corner buttons (smaller)
    float lbW = sw * 0.14f;
    float lbH = sh * 0.055f;
    float cornerPad = sw * 0.018f;
    float lbGap = sh * 0.008f;

    // Bottom-left dev tools � always visible so you can test without finishing the demo
    _buttons.push_back({ "Dungeon Run",   { cornerPad, sh - lbH * 4.f - lbGap * 3.f - sh * 0.018f, lbW, lbH } });
    _buttons.push_back({ "Tile Editor",   { cornerPad, sh - lbH * 3.f - lbGap * 2.f - sh * 0.018f, lbW, lbH } });
    _buttons.push_back({ "9-Slice Editor",{ cornerPad, sh - lbH * 2.f - lbGap       - sh * 0.018f, lbW, lbH } });
    _buttons.push_back({ "Char Animator", { cornerPad, sh - lbH                     - sh * 0.018f, lbW, lbH } });

    _startPressed          = false;
    _quitPressed           = false;
    _continuePressed       = false;
    _debugPressed          = false;
    _dungeonRunPressed     = false;
    _tileMapperPressed     = false;
    _nineSliceEditorPressed = false;
    _charAnimatorPressed    = false;
    _settingsPressed        = false;

    // Initialise editor rect from the computed border values.
    {
        float borderW = sw * 0.3703f;
        float borderH = PanelBorderRect().height;
        _edRect       = { sw / 2.f - borderW / 2.f, sh * 0.2731f, borderW, borderH };
        _bannerEdY    = sh * 0.0315f;
        _btnEdFirstY  = sh * 0.4014f;
    }

    if (_borderTex.id == 0)
        _borderTex  = LoadTexture(AssetPath("UI/MainMenuBorder.png").c_str());
    if (_bannerTex.id == 0)
        _bannerTex  = LoadTexture(AssetPath("UI/TitleBanner.png").c_str());
    if (_playBtnTex.id == 0)
        _playBtnTex = LoadTexture(AssetPath("UI/PlayButton.png").c_str());
    if (_continueBtnTex.id == 0)
        _continueBtnTex = LoadTexture(AssetPath("UI/HowToPlayButton.png").c_str());
}

void MainMenu::Update()
{
    // Reset all per-frame press flags so returning to menu without Init() doesn't re-fire
    _startPressed           = false;
    _quitPressed            = false;
    _continuePressed        = false;
    _debugPressed           = false;
    _dungeonRunPressed      = false;
    _tileMapperPressed      = false;
    _nineSliceEditorPressed = false;
    _charAnimatorPressed    = false;
    _settingsPressed        = false;

    float sw = (float)kVirtualWidth;
    float sh = (float)kVirtualHeight;

    if (_confirmNewGame)
    {
        UpdateNewGameConfirm();
        return;
    }
    if (_showCredits)
    {
        UpdateCredits();
        return;
    }

    Vector2 mouse = GetVirtualMousePos();

#if MO_DEV_TOOLS || MO_ITCH_DEMO
    // Backslash reveals the developer button column (Dungeon Run / editors).
    // Also honoured in the itch.io demo build as a hidden dev backdoor, matching
    // the \ / F12 debug unlock in Engine::Update.
    if (IsKeyPressed(KEY_BACKSLASH)) _devToolsVisible = !_devToolsVisible;
#endif

    for (auto& button : _buttons)
    {
        if (button.text == "Debug Mode" && !_debugUnlocked)
            continue;
        if ((button.text == "Dungeon Run" || button.text == "Tile Editor" || button.text == "9-Slice Editor" || button.text == "Char Animator")
            && !_devToolsVisible)
            continue;

        bool isPanelBtn = IsPanelLabel(button.text);
        Rectangle checkBounds = button.bounds;
        if (isPanelBtn)
            checkBounds.y = _btnEdFirstY + button.bounds.y;
        button.hovered = CheckCollisionPointRec(mouse, checkBounds);

        button.selected = false; // reset gamepad-selection flag each frame

        if (button.hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            if (button.text == "New Game")
            {
                if (_hasProfile) { _confirmNewGame = true; _confirmCursor = 1; }
                else             _startPressed = true;
            }
            if (button.text == "Quit")         _quitPressed       = true;
            if (button.text == "Continue")     _continuePressed   = true;
            if (button.text == "Debug Mode")   _debugPressed      = true;
            if (button.text == "Dungeon Run")    _dungeonRunPressed      = true;
            if (button.text == "Tile Editor")   _tileMapperPressed      = true;
            if (button.text == "9-Slice Editor") _nineSliceEditorPressed = true;
            if (button.text == "Char Animator")  _charAnimatorPressed    = true;
            if (button.text == "Settings")       _settingsPressed        = true;
            if (button.text == "Credits")        OpenCredits();
        }
    }

    // -- Gamepad navigation for panel buttons (New Game / Continue / Settings / Quit) --
    {
        // Mouse movement hands control back to the cursor
        Vector2 mouseDelta = GetMouseDelta();
        if (mouseDelta.x * mouseDelta.x + mouseDelta.y * mouseDelta.y > 4.f)
            _gpSelected = -1;
    }

    const int kPanelCount = PanelCount();
    if (IsGamepadAvailable(0))
    {
        bool moveDown = IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN);
        bool moveUp   = IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_UP);

        float stickY = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
        _gpStickCooldown -= GetFrameTime();
        if (_gpStickCooldown <= 0.f)
        {
            if      (stickY >  0.5f) { moveDown = true; _gpStickCooldown = 0.22f; }
            else if (stickY < -0.5f) { moveUp   = true; _gpStickCooldown = 0.22f; }
            else                       _gpStickCooldown = 0.f;
        }

        if (moveDown || moveUp)
        {
            if (_gpSelected < 0)
                _gpSelected = moveDown ? 0 : kPanelCount - 1;
            else
            {
                _gpSelected += moveDown ? 1 : -1;
                if (_gpSelected < 0)            _gpSelected = kPanelCount - 1;
                if (_gpSelected >= kPanelCount) _gpSelected = 0;
            }
        }

        // A / Cross confirms the highlighted button. If nothing is highlighted yet,
        // treat A as selecting the top option so controller users never get a dead press.
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN))
        {
            if (_gpSelected < 0)
                _gpSelected = 0;

            if (_gpSelected < kPanelCount)
            {
                const std::string& label = _buttons[_gpSelected].text;
                if (label == "New Game")
                {
                    if (_hasProfile) { _confirmNewGame = true; _confirmCursor = 1; }
                    else             _startPressed = true;
                }
                else if (label == "Continue") _continuePressed = true;
                else if (label == "Settings") _settingsPressed = true;
                else if (label == "Credits")  OpenCredits();
                else if (label == "Quit")     _quitPressed     = true;
            }
        }

        // Stamp the selected flag on the active panel button
        if (_gpSelected >= 0)
            for (int i = 0; i < kPanelCount && i < (int)_buttons.size(); i++)
                _buttons[i].selected = (_gpSelected == i);
    }

    if (!_editorActive) return;

    // -- Editor input � unified click priority ---------------------------------
    // Priority: edge handles > banner > button group > border interior
    const float hs = 10.f;
    auto handlePos = [&](int i) -> Vector2 {
        switch (i) {
            case 0: return { _edRect.x,                     _edRect.y                      };
            case 1: return { _edRect.x + _edRect.width,     _edRect.y                      };
            case 2: return { _edRect.x,                     _edRect.y + _edRect.height     };
            case 3: return { _edRect.x + _edRect.width,     _edRect.y + _edRect.height     };
            case 4: return { _edRect.x + _edRect.width/2.f, _edRect.y                      };
            case 5: return { _edRect.x + _edRect.width/2.f, _edRect.y + _edRect.height     };
            case 6: return { _edRect.x,                     _edRect.y + _edRect.height/2.f };
            default:return { _edRect.x + _edRect.width,     _edRect.y + _edRect.height/2.f };
        }
    };

    float bannerW = TitleBannerWidth(_bannerTex, sw);
    float bannerH = (_bannerTex.id != 0)
        ? bannerW * ((float)_bannerTex.height / (float)_bannerTex.width) : sh * 0.12f;
    Rectangle bannerRect{ sw / 2.f - bannerW / 2.f, _bannerEdY, bannerW, bannerH };

    const float btnW   = sw * 0.20f;
    const float btnH   = sh * 0.083f;
    const float btnGap = sh * 0.018f;
    Rectangle btnGroupRect{ sw / 2.f - btnW / 2.f - 8.f, _btnEdFirstY - 8.f,
                             btnW + 16.f, 4.f * btnH + 3.f * btnGap + 16.f };

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        _edHandle = -1; _bannerDragging = false; _btnDragging = false;

        // 1. Edge handles
        for (int i = 0; i < 8; i++)
        {
            Vector2 h = handlePos(i);
            if (CheckCollisionPointRec(mouse, { h.x - hs, h.y - hs, hs * 2.f, hs * 2.f }))
            { _edHandle = i; break; }
        }

        if (_edHandle == -1)
        {
            // 2. Banner
            if (CheckCollisionPointRec(mouse, bannerRect))
            { _bannerDragging = true; _bannerDragStartMY = mouse.y; _bannerDragStartY = _bannerEdY; }
            // 3. Button group (inside border � must be before interior check)
            else if (CheckCollisionPointRec(mouse, btnGroupRect))
            { _btnDragging = true; _btnDragStartMY = mouse.y; _btnDragStartFY = _btnEdFirstY; }
            // 4. Border interior (anywhere else inside the border rect)
            else if (CheckCollisionPointRec(mouse, _edRect))
            { _edHandle = -2; }
        }

        if (_edHandle != -1) { _edDragStart = mouse; _edRectStart = _edRect; }
    }

    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
    { _edHandle = -1; _bannerDragging = false; _btnDragging = false; }

    // Apply drags
    if (_edHandle != -1)
    {
        float dx = mouse.x - _edDragStart.x;
        float dy = mouse.y - _edDragStart.y;
        Rectangle r = _edRectStart;
        switch (_edHandle) {
            case -2: r.y += dy; break;
            case  0: r.x += dx; r.width -= dx; r.y += dy; r.height -= dy; break;
            case  1: r.width += dx;             r.y += dy; r.height -= dy; break;
            case  2: r.x += dx; r.width -= dx;             r.height += dy; break;
            case  3: r.width += dx;                        r.height += dy; break;
            case  4:                            r.y += dy; r.height -= dy; break;
            case  5:                                       r.height += dy; break;
            case  6: r.x += dx; r.width -= dx;                            break;
            case  7: r.width += dx;                                        break;
        }
        r.width  = std::max(r.width,  60.f);
        r.height = std::max(r.height, 40.f);
        _edRect = r;
    }
    if (_bannerDragging)
        _bannerEdY = _bannerDragStartY + (mouse.y - _bannerDragStartMY);
    if (_btnDragging)
        _btnEdFirstY = _btnDragStartFY + (mouse.y - _btnDragStartMY);
}

void MainMenu::Draw()
{
    float sw = (float)kVirtualWidth;
    float sh = (float)kVirtualHeight;

    // -- Animated checkerboard background ------------------------------------
    {
        const int   cell   = 80;
        const Color dark   = Color{ 104, 70, 46, 255 };
        const Color light  = Color{ 124, 88, 60, 255 };
        const int   period = cell * 2;   // full colour cycle = 2 cells wide

        float t      = (float)GetTime();
        int   offX   = (int)fmodf(t * 22.f, (float)period);
        int   offY   = (int)fmodf(t * 12.f, (float)period);
        int   phaseX = offX / cell;       // 0 or 1 � which colour phase
        int   phaseY = offY / cell;
        int   pixX   = offX % cell;       // sub-cell pixel offset
        int   pixY   = offY % cell;

        for (int gy = -1; gy <= (int)(sh / cell) + 1; gy++)
        {
            for (int gx = -1; gx <= (int)(sw / cell) + 1; gx++)
            {
                bool isDark = (((gx + phaseX) + (gy + phaseY)) % 2 + 2) % 2 == 0;
                DrawRectangle(gx * cell - pixX, gy * cell - pixY,
                    cell, cell, isDark ? dark : light);
            }
        }
    }

    // -- Panel border around the button area ----------------------------------
    Rectangle borderRect = _editorActive ? _edRect
        : PanelBorderRect();

    if (_borderTex.id != 0)
        DrawNineSliceScaled(_borderTex, kMainBorderCuts, kMainBorderScale, borderRect, WHITE);

    // -- Border editor overlay -------------------------------------------------
    if (_editorActive)
    {
        const float hs = 10.f;
        auto handlePos = [&](int i) -> Vector2 {
            switch (i) {
                case 0: return { _edRect.x,                     _edRect.y                      };
                case 1: return { _edRect.x + _edRect.width,     _edRect.y                      };
                case 2: return { _edRect.x,                     _edRect.y + _edRect.height     };
                case 3: return { _edRect.x + _edRect.width,     _edRect.y + _edRect.height     };
                case 4: return { _edRect.x + _edRect.width/2.f, _edRect.y                      };
                case 5: return { _edRect.x + _edRect.width/2.f, _edRect.y + _edRect.height     };
                case 6: return { _edRect.x,                     _edRect.y + _edRect.height/2.f };
                default:return { _edRect.x + _edRect.width,     _edRect.y + _edRect.height/2.f };
            }
        };

        DrawRectangleLinesEx(_edRect, 1.5f, Color{ 255, 220, 0, 180 });
        for (int i = 0; i < 8; i++)
        {
            Vector2 h = handlePos(i);
            Color hcol = (_edHandle == i) ? Color{ 255, 255, 80, 255 } : Color{ 255, 200, 0, 220 };
            DrawRectangle((int)(h.x - hs), (int)(h.y - hs), (int)(hs * 2.f), (int)(hs * 2.f), hcol);
        }

        const char* hint = "[F1] close  [S] export";
        DrawText(hint, (int)(_edRect.x + 4), (int)(_edRect.y - 18), 14, Color{ 255, 220, 0, 200 });

        if (IsKeyPressed(KEY_S))
        {
            TraceLog(LOG_INFO, "=== Menu Border Export ===");
            TraceLog(LOG_INFO, "borderW = sw * %.4ff;", _edRect.width  / sw);
            TraceLog(LOG_INFO, "borderH = sh * %.4ff;", _edRect.height / sh);
            TraceLog(LOG_INFO, "borderX = sw / 2.f - borderW / 2.f  (centred) | raw: sw * %.4ff", _edRect.x / sw);
            TraceLog(LOG_INFO, "borderY = sh * %.4ff;", _edRect.y / sh);
            TraceLog(LOG_INFO, "bannerY  = sh * %.4ff;", _bannerEdY    / sh);
            TraceLog(LOG_INFO, "firstY   = sh * %.4ff;", _btnEdFirstY  / sh);
            TraceLog(LOG_INFO, "===========================");
        }
    }

    // -- Title banner ---------------------------------------------------------
    float bannerW = TitleBannerWidth(_bannerTex, sw);
    float bannerH = (_bannerTex.id != 0)
        ? bannerW * ((float)_bannerTex.height / (float)_bannerTex.width)
        : sh * 0.12f;
    float bannerX = sw / 2.f - bannerW / 2.f;
    float bannerY = _editorActive ? _bannerEdY : sh * 0.0315f;

    if (_bannerTex.id != 0)
        DrawTexturePro(_bannerTex,
            { 0.f, 0.f, (float)_bannerTex.width, (float)_bannerTex.height },
            { bannerX, bannerY, bannerW, bannerH },
            {}, 0.f, WHITE);

    // Title text � centred in the flat body of the banner, with black outline
    int         titleSz = (int)(sh * 0.058f);
    const char* title   = "Mystic Onslaught";
    int         titleW  = MeasureText(title, titleSz);
    int         textX   = (int)(sw / 2.f - titleW / 2.f);
    int         textY   = (int)(bannerY + bannerH * 0.28f);

    for (int ox = -3; ox <= 3; ox += 3)
        for (int oy = -3; oy <= 3; oy += 3)
            if (ox != 0 || oy != 0)
                DrawText(title, textX + ox, textY + oy, titleSz, BLACK);

    DrawText(title, textX, textY, titleSz, GOLD);

    // Editor overlays for banner and button group
    if (_editorActive)
    {
        // Banner outline
        DrawRectangleLinesEx({ bannerX, bannerY, bannerW, bannerH }, 1.f,
            _bannerDragging ? Color{ 100, 220, 255, 220 } : Color{ 100, 200, 255, 120 });
        DrawText("drag", (int)(bannerX + 4), (int)(bannerY + 2), 12, Color{ 100, 200, 255, 160 });

        // Button group outline
        const float btnW2   = sw * 0.20f;
        const float btnH2   = sh * 0.083f;
        const float btnGap2 = sh * 0.018f;
        float groupH = 4.f * btnH2 + 3.f * btnGap2;
        Rectangle groupRect{ sw / 2.f - btnW2 / 2.f - 8.f, _btnEdFirstY - 8.f,
                              btnW2 + 16.f, groupH + 16.f };
        DrawRectangleLinesEx(groupRect, 1.f,
            _btnDragging ? Color{ 100, 255, 160, 220 } : Color{ 80, 220, 120, 120 });
        DrawText("drag", (int)(groupRect.x + 4), (int)(groupRect.y + 2), 12,
            Color{ 80, 220, 120, 160 });
    }

    // -- Buttons --------------------------------------------------------------
    for (auto& button : _buttons)
    {
        if (button.text == "Debug Mode" && !_debugUnlocked)
            continue;
        if ((button.text == "Dungeon Run" || button.text == "Tile Editor" || button.text == "9-Slice Editor" || button.text == "Char Animator")
            && !_devToolsVisible)
            continue;

        if (button.text == "Debug Mode")
        {
            Rectangle drawBounds = button.bounds;
            if (button.hovered)
            {
                float pulse  = 0.5f + 0.5f * sinf((float)GetTime() * 5.f);
                float expand = drawBounds.width * 0.035f;
                drawBounds = { drawBounds.x - expand, drawBounds.y - expand * 0.7f,
                               drawBounds.width + expand * 2.f, drawBounds.height + expand * 1.4f };
                Color glowCol{ 255, 205, 135, (unsigned char)(100 + (int)(80.f * pulse)) };
                DrawRectangleRoundedLines({ drawBounds.x - 3.f, drawBounds.y - 3.f,
                                            drawBounds.width + 6.f, drawBounds.height + 6.f },
                                           0.24f, 8, glowCol);
            }
            Color fill = button.hovered ? Color{ 180, 105, 55, 245 } : Color{ 126, 74, 38, 225 };
            Color edge = button.hovered ? Color{ 255, 205, 135, 255 } : Color{ 220, 168, 105, 215 };
            DrawRectangleRounded(drawBounds, 0.24f, 8, fill);
            DrawRectangleRoundedLines(drawBounds, 0.24f, 8, edge);
            int fs = (int)(sh * 0.032f);
            int tw = MeasureText(button.text.c_str(), fs);
            DrawText(button.text.c_str(),
                (int)(drawBounds.x + drawBounds.width  / 2 - tw / 2),
                (int)(drawBounds.y + drawBounds.height / 2 - fs / 2),
                fs, Color{ 255, 244, 220, 255 });
            continue;
        }

        // Dungeon Run button � teal
        if (button.text == "Dungeon Run")
        {
            Rectangle drawBounds = button.bounds;
            if (button.hovered)
            {
                float pulse  = 0.5f + 0.5f * sinf((float)GetTime() * 5.f);
                float expand = drawBounds.width * 0.035f;
                drawBounds = { drawBounds.x - expand, drawBounds.y - expand * 0.7f,
                               drawBounds.width + expand * 2.f, drawBounds.height + expand * 1.4f };
                Color glowCol{ 100, 230, 230, (unsigned char)(100 + (int)(80.f * pulse)) };
                DrawRectangleRoundedLines({ drawBounds.x - 3.f, drawBounds.y - 3.f,
                                            drawBounds.width + 6.f, drawBounds.height + 6.f },
                                           0.24f, 8, glowCol);
            }
            Color fill = button.hovered ? Color{ 40, 160, 160, 245 } : Color{ 25, 100, 100, 220 };
            Color edge = button.hovered ? Color{ 100, 230, 230, 255 } : Color{ 60, 170, 170, 200 };
            DrawRectangleRounded(drawBounds, 0.24f, 8, fill);
            DrawRectangleRoundedLines(drawBounds, 0.24f, 8, edge);
            int fs = (int)(sh * 0.026f);
            int tw = MeasureText(button.text.c_str(), fs);
            DrawText(button.text.c_str(),
                (int)(drawBounds.x + drawBounds.width  / 2 - tw / 2),
                (int)(drawBounds.y + drawBounds.height / 2 - fs / 2),
                fs, RAYWHITE);
            continue;
        }

        // Tile Editor / 9-Slice Editor � orange
        if (button.text == "Tile Editor" || button.text == "Tile Mapper" || button.text == "9-Slice Editor")
        {
            Rectangle drawBounds = button.bounds;
            if (button.hovered)
            {
                float pulse  = 0.5f + 0.5f * sinf((float)GetTime() * 5.f);
                float expand = drawBounds.width * 0.035f;
                drawBounds = { drawBounds.x - expand, drawBounds.y - expand * 0.7f,
                               drawBounds.width + expand * 2.f, drawBounds.height + expand * 1.4f };
                Color glowCol{ 255, 175, 80, (unsigned char)(100 + (int)(80.f * pulse)) };
                DrawRectangleRoundedLines({ drawBounds.x - 3.f, drawBounds.y - 3.f,
                                            drawBounds.width + 6.f, drawBounds.height + 6.f },
                                           0.24f, 8, glowCol);
            }
            Color fill = button.hovered ? Color{ 180, 100, 30, 245 } : Color{ 120, 65, 18, 220 };
            Color edge = button.hovered ? Color{ 255, 175, 80, 255 } : Color{ 200, 140, 60, 200 };
            DrawRectangleRounded(drawBounds, 0.24f, 8, fill);
            DrawRectangleRoundedLines(drawBounds, 0.24f, 8, edge);
            int fs = (int)(sh * 0.026f);
            int tw = MeasureText(button.text.c_str(), fs);
            DrawText(button.text.c_str(),
                (int)(drawBounds.x + drawBounds.width  / 2 - tw / 2),
                (int)(drawBounds.y + drawBounds.height / 2 - fs / 2),
                fs, RAYWHITE);
            continue;
        }

        Texture2D* tex  = &_playBtnTex;
        Color      tint = WHITE;

        if (button.text == "Continue")
            tex = &_continueBtnTex;
        else if (button.text == "Quit")
            tint = Color{ 230, 80, 80, 255 };

        bool isPanelBtn = IsPanelLabel(button.text);
        Rectangle drawBounds = button.bounds;
        if (isPanelBtn)
            drawBounds.y = _btnEdFirstY + button.bounds.y;

        bool highlighted = button.hovered || button.selected;
        if (highlighted)
        {
            // "Bloom bigger" � expand the button rect and draw a pulsing glow ring behind it
            float pulse  = 0.5f + 0.5f * sinf((float)GetTime() * 5.f);
            float expand = drawBounds.width * 0.04f;
            drawBounds = { drawBounds.x - expand, drawBounds.y - expand * 0.7f,
                           drawBounds.width + expand * 2.f, drawBounds.height + expand * 1.4f };

            // Gold glow for gamepad cursor, white glow for mouse hover
            Color glowCol = button.selected
                ? Color{ 255, (unsigned char)(200 + (int)(45.f * pulse)), 40,
                         (unsigned char)(160 + (int)(80.f * pulse)) }
                : Color{ 255, 255, 255, (unsigned char)(90 + (int)(70.f * pulse)) };
            Rectangle glowRect{ drawBounds.x - 4.f, drawBounds.y - 4.f,
                                 drawBounds.width + 8.f, drawBounds.height + 8.f };
            DrawRectangleRoundedLines(glowRect, 0.25f, 8, glowCol);
        }
        else
        {
            tint = Fade(tint, 0.78f); // dim slightly when not highlighted
        }

        if (tex->id != 0)
            DrawNineSlice(*tex, BTN_SRC_CORNER, BTN_DST_CORNER, drawBounds, tint);
        else
            DrawRectangleRec(drawBounds, Fade(GRAY, 0.7f));

        int fontSize  = (int)(sh * 0.036f);
        int textWidth = MeasureText(button.text.c_str(), fontSize);
        DrawText(button.text.c_str(),
            (int)(drawBounds.x + drawBounds.width  / 2 - textWidth / 2),
            (int)(drawBounds.y + drawBounds.height / 2 - fontSize  / 2),
            fontSize, BLACK);
        if (button.text == "Continue" && !_runCaption.empty())
        {
            const int captionFs = (int)(sh * 0.02f);
            const int captionW = MeasureText(_runCaption.c_str(), captionFs);
            const int cx = (int)(drawBounds.x + drawBounds.width / 2 - captionW / 2);
            const int cy = (int)(drawBounds.y + drawBounds.height + 5.f);
            DrawRectangleRounded(Rectangle{ (float)cx - 12.f, (float)cy - 4.f, (float)captionW + 24.f, (float)captionFs + 8.f },
                                 0.5f, 8, Fade(Color{ 30, 20, 14, 255 }, 0.85f));
            DrawText(_runCaption.c_str(), cx, cy, captionFs, GOLD);
        }
    }

    if (_confirmNewGame) DrawNewGameConfirm();
    if (_showCredits)    DrawCredits();
}

Rectangle MainMenu::PanelBorderRect() const
{
    const float sw = (float)kVirtualWidth, sh = (float)kVirtualHeight;
    const float height = sh * (PanelCount() >= 5 ? 0.6916f : 0.6466f);   // five buttons need a taller frame
    return { sw / 2.f - sw * 0.3703f / 2.f, sh * 0.2731f, sw * 0.3703f, height };
}

void MainMenu::SetRunCaption(const std::string& caption)
{
    const bool relayout = caption.empty() != _runCaption.empty();   // room under Continue appears/goes
    _runCaption = caption;
    if (relayout) Init();
}

bool MainMenu::IsPanelLabel(const std::string& text)
{
    return text == "New Game" || text == "Continue" || text == "Settings" || text == "Credits" || text == "Quit";
}

// -- Credits -------------------------------------------------------------------
namespace
{
    void CreditsRects(Rectangle& panel, Rectangle& view, Rectangle& backBtn)
    {
        const float sw = (float)kVirtualWidth, sh = (float)kVirtualHeight;
        panel   = { sw * 0.5f - 560.f, sh * 0.5f - 470.f, 1120.f, 940.f };
        view    = { panel.x + 40.f, panel.y + 104.f, panel.width - 80.f, panel.height - 104.f - 120.f };
        backBtn = { sw * 0.5f - 140.f, panel.y + panel.height - 96.f, 280.f, 64.f };
    }

    // Row height and font size per kind of credit line.
    float CreditRowHeight(CreditLine::Kind kind)
    {
        switch (kind)
        {
        case CreditLine::Kind::Title:   return 76.f;
        case CreditLine::Kind::Heading: return 56.f;
        case CreditLine::Kind::Name:    return 46.f;
        default:                        return 36.f;
        }
    }

    int CreditFontSize(CreditLine::Kind kind)
    {
        switch (kind)
        {
        case CreditLine::Kind::Title:   return 56;
        case CreditLine::Kind::Heading: return 30;
        case CreditLine::Kind::Name:    return 38;
        default:                        return 26;
        }
    }

    // Extra space above line i: a second publisher under one heading gets air.
    float CreditLeading(int i)
    {
        return (i > 0 && kCreditLines[i].kind == CreditLine::Kind::Name &&
                kCreditLines[i - 1].kind == CreditLine::Kind::Detail) ? 16.f : 0.f;
    }

    float CreditsContentHeight()
    {
        float h = 0.f;
        for (int i = 0; i < kCreditLineCount; ++i) h += CreditLeading(i) + CreditRowHeight(kCreditLines[i].kind);
        return h;
    }

    // Lines fade out over this many px at the top and bottom of the scroll area;
    // the list starts (and ends) one band in, so nothing is faded at rest.
    constexpr float kCreditsFade = 56.f;
}

void MainMenu::UpdateCredits()
{
    const float dt = GetFrameTime();
    Rectangle panel, view, backBtn;
    CreditsRects(panel, view, backBtn);
    const float maxScroll = std::max(0.f, CreditsContentHeight() - view.height + kCreditsFade * 2.f);
    constexpr float kScrollSpeed = 420.f;

    float manual = -GetMouseWheelMove() * 60.f;
    if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) manual += kScrollSpeed * dt;
    if (IsKeyDown(KEY_UP)   || IsKeyDown(KEY_W)) manual -= kScrollSpeed * dt;
    bool close = IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_ENTER);
    if (IsGamepadAvailable(0))
    {
        const float stick = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
        if (fabsf(stick) > 0.25f) manual += stick * kScrollSpeed * dt;
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) manual += kScrollSpeed * dt;
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_UP))   manual -= kScrollSpeed * dt;
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT) ||
            IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN))
            close = true;
    }
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetVirtualMousePos(), backBtn))
        close = true;
    if (close) { _showCredits = false; return; }

    if (manual != 0.f) { _creditsScroll += manual; _creditsIdle = 0.f; }
    else
    {
        _creditsIdle += dt;
        if (_creditsIdle > 1.2f) _creditsScroll += 40.f * dt;   // gentle roll once left alone
    }
    _creditsScroll = std::clamp(_creditsScroll, 0.f, maxScroll);
}

void MainMenu::DrawCredits() const
{
    const float sw = (float)kVirtualWidth, sh = (float)kVirtualHeight;
    DrawRectangle(0, 0, (int)sw, (int)sh, Fade(BLACK, 0.7f));
    Rectangle panel, view, backBtn;
    CreditsRects(panel, view, backBtn);
    DrawRectangleRounded(panel, 0.04f, 10, Color{ 34, 24, 18, 250 });
    DrawRectangleRoundedLinesEx(panel, 0.04f, 10, 3.f, Color{ 220, 168, 105, 255 });

    const char* title = "CREDITS";
    DrawText(title, (int)(sw * 0.5f - MeasureText(title, 52) * 0.5f), (int)(panel.y + 28.f), 52, GOLD);
    DrawLineEx({ panel.x + 40.f, view.y - 14.f }, { panel.x + panel.width - 40.f, view.y - 14.f }, 2.f,
               Fade(Color{ 220, 168, 105, 255 }, 0.45f));

    float y = view.y + kCreditsFade - _creditsScroll;
    for (int i = 0; i < kCreditLineCount; ++i)
    {
        const CreditLine& line = kCreditLines[i];
        y += CreditLeading(i);
        const float rowH = CreditRowHeight(line.kind);
        const int fs = CreditFontSize(line.kind);
        if (line.kind != CreditLine::Kind::Gap && y + fs > view.y && y < view.y + view.height)
        {
            const float edge = std::min(y - view.y, view.y + view.height - (y + fs));
            const float alpha = std::clamp(edge / kCreditsFade, 0.f, 1.f);
            Color color = line.kind == CreditLine::Kind::Title   ? GOLD
                        : line.kind == CreditLine::Kind::Heading ? Color{ 220, 168, 105, 255 }
                        : line.kind == CreditLine::Kind::Name    ? Color{ 245, 235, 215, 255 }
                                                                 : Color{ 200, 190, 175, 255 };
            if (alpha > 0.f)
                DrawText(line.text, (int)(sw * 0.5f - MeasureText(line.text, fs) * 0.5f), (int)y, fs, Fade(color, alpha));
        }
        y += rowH;
    }

    const bool backHover = CheckCollisionPointRec(GetVirtualMousePos(), backBtn);
    DrawRectangleRounded(backBtn, 0.25f, 8, backHover ? Color{ 70, 40, 26, 255 } : Color{ 46, 30, 22, 255 });
    DrawRectangleRoundedLinesEx(backBtn, 0.25f, 8, backHover ? 4.f : 2.f,
                                backHover ? Color{ 255, 205, 135, 255 } : Color{ 160, 124, 88, 255 });
    const char* back = "BACK";
    DrawText(back, (int)(backBtn.x + backBtn.width * 0.5f - MeasureText(back, 32) * 0.5f),
             (int)(backBtn.y + backBtn.height * 0.5f - 16.f), 32, Color{ 245, 235, 215, 255 });
    const Color hintColor{ 150, 135, 118, 255 };
    const int hintY = (int)(backBtn.y + backBtn.height * 0.5f - 10.f);
    const char* scrollHint = "Scroll: wheel / arrows / stick";
    const char* closeHint  = "Close: Esc / B";
    DrawText(scrollHint, (int)(panel.x + 40.f), hintY, 20, hintColor);
    DrawText(closeHint, (int)(panel.x + panel.width - 40.f - MeasureText(closeHint, 20)), hintY, 20, hintColor);
}

int MainMenu::PanelCount() const
{
    int n = 0;
    while (n < (int)_buttons.size() && IsPanelLabel(_buttons[n].text)) ++n;
    return n;
}

void MainMenu::SetHasProfile(bool hasProfile)
{
#if MO_ITCH_DEMO
    hasProfile = false;   // the public demo never saves, so it never continues
#endif
    if (hasProfile == _hasProfile) return;
    _hasProfile = hasProfile;
    _gpSelected = -1;
    Init();   // rebuild the panel in the new order
}

static void NewGameConfirmRects(Rectangle& panel, Rectangle& eraseBtn, Rectangle& cancelBtn)
{
    const float sw = (float)kVirtualWidth, sh = (float)kVirtualHeight;
    panel     = { sw * 0.5f - 430.f, sh * 0.5f - 170.f, 860.f, 340.f };
    eraseBtn  = { panel.x + 70.f,                     panel.y + panel.height - 110.f, 330.f, 70.f };
    cancelBtn = { panel.x + panel.width - 70.f - 330.f, panel.y + panel.height - 110.f, 330.f, 70.f };
}

void MainMenu::UpdateNewGameConfirm()
{
    Rectangle panel, eraseBtn, cancelBtn;
    NewGameConfirmRects(panel, eraseBtn, cancelBtn);
    const Vector2 mouse = GetVirtualMousePos();
    if (CheckCollisionPointRec(mouse, eraseBtn))  _confirmCursor = 0;
    if (CheckCollisionPointRec(mouse, cancelBtn)) _confirmCursor = 1;
    if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A)) _confirmCursor = 0;
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) _confirmCursor = 1;
    bool confirm = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
    bool cancel  = IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_N) || IsKeyPressed(KEY_BACKSPACE);
    if (IsGamepadAvailable(0))
    {
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT))  _confirmCursor = 0;
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) _confirmCursor = 1;
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN))  confirm = true;
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) cancel  = true;
    }
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        if (CheckCollisionPointRec(mouse, eraseBtn))       confirm = true;
        else if (CheckCollisionPointRec(mouse, cancelBtn)) cancel  = true;
    }
    if (cancel || (confirm && _confirmCursor == 1)) { _confirmNewGame = false; return; }
    if (confirm && _confirmCursor == 0)
    {
        _confirmNewGame = false;
        _startPressed   = true;   // the engine erases the profile, then starts fresh
    }
}

void MainMenu::DrawNewGameConfirm() const
{
    const float sw = (float)kVirtualWidth, sh = (float)kVirtualHeight;
    DrawRectangle(0, 0, (int)sw, (int)sh, Fade(BLACK, 0.65f));
    Rectangle panel, eraseBtn, cancelBtn;
    NewGameConfirmRects(panel, eraseBtn, cancelBtn);
    DrawRectangleRounded(panel, 0.08f, 10, Color{ 34, 24, 18, 250 });
    DrawRectangleRoundedLinesEx(panel, 0.08f, 10, 3.f, Color{ 220, 168, 105, 255 });
    const char* title = "Start a new game?";
    DrawText(title, (int)(sw * 0.5f - MeasureText(title, 44) * 0.5f), (int)(panel.y + 34.f), 44, GOLD);
    const char* l1 = "This erases your Echoes, unlocks, classes, mastery and village.";
    const char* l2 = "Settings and key bindings are kept.";
    DrawText(l1, (int)(sw * 0.5f - MeasureText(l1, 24) * 0.5f), (int)(panel.y + 110.f), 24, Color{ 240, 228, 210, 255 });
    DrawText(l2, (int)(sw * 0.5f - MeasureText(l2, 24) * 0.5f), (int)(panel.y + 146.f), 24, Color{ 200, 190, 175, 255 });
    if (!_runCaption.empty())
    {
        const char* l3 = "Your run in progress will be lost.";
        DrawText(l3, (int)(sw * 0.5f - MeasureText(l3, 24) * 0.5f), (int)(panel.y + 182.f), 24, Color{ 240, 120, 100, 255 });
    }
    auto button = [&](Rectangle r, const char* label, bool selected, Color accent) {
        DrawRectangleRounded(r, 0.25f, 8, selected ? Color{ 70, 40, 26, 255 } : Color{ 46, 30, 22, 255 });
        DrawRectangleRoundedLinesEx(r, 0.25f, 8, selected ? 4.f : 2.f, selected ? accent : Color{ 120, 96, 72, 255 });
        DrawText(label, (int)(r.x + r.width * 0.5f - MeasureText(label, 28) * 0.5f), (int)(r.y + r.height * 0.5f - 14.f), 28,
                 selected ? accent : Color{ 210, 200, 185, 255 });
    };
    button(eraseBtn,  "Erase and start", _confirmCursor == 0, Color{ 240, 110, 90, 255 });
    button(cancelBtn, "Cancel",          _confirmCursor == 1, Color{ 140, 220, 140, 255 });
}

bool MainMenu::StartPressed()       const { return _startPressed;       }
bool MainMenu::QuitPressed()        const { return _quitPressed;        }
bool MainMenu::ContinuePressed()    const { return _continuePressed;    }
bool MainMenu::DebugPressed()       const { return _debugPressed;       }

void MainMenu::SetDebugUnlocked(bool unlocked)
{
    if (unlocked) _debugUnlocked = true;
}

bool MainMenu::DungeonRunPressed()       const { return _dungeonRunPressed;       }
bool MainMenu::TileMapperPressed()       const { return _tileMapperPressed;       }
bool MainMenu::NineSliceEditorPressed()  const { return _nineSliceEditorPressed;  }
bool MainMenu::CharacterAnimatorPressed() const { return _charAnimatorPressed;     }
bool MainMenu::SettingsPressed()         const { return _settingsPressed;          }
