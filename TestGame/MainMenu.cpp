#include "MainMenu.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "VirtualCanvas.h"
#include "NineSlice.h"
#include "VirtualCanvas.h"
#include <cmath>

// -- 9-slice corner sizes ------------------------------------------------------
static constexpr float BORDER_SRC_CORNER = 1.0f;
static constexpr float BORDER_DST_CORNER = 20.0f; // screen px � raise/lower if corners look off
static constexpr float BTN_SRC_CORNER    = 8.f;
static constexpr float BTN_DST_CORNER    = 16.f;

MainMenu::~MainMenu()
{
    if (_borderTex.id  != 0) UnloadTexture(_borderTex);
    if (_bannerTex.id  != 0) UnloadTexture(_bannerTex);
    if (_playBtnTex.id != 0) UnloadTexture(_playBtnTex);
    if (_htpBtnTex.id  != 0) UnloadTexture(_htpBtnTex);
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

    // Y stored as offset from group top � draw adds _btnEdFirstY at runtime
    _buttons.push_back({ "Start Game",  { startX, 0.f,                           buttonWidth, buttonHeight } });
    _buttons.push_back({ "How To Play", { startX, (buttonHeight + gap),           buttonWidth, buttonHeight } });
    _buttons.push_back({ "Settings",    { startX, (buttonHeight + gap) * 2.f,     buttonWidth, buttonHeight } });
    _buttons.push_back({ "Quit",        { startX, (buttonHeight + gap) * 3.f,     buttonWidth, buttonHeight } });
    _buttons.push_back({ "Debug Mode",  { sw * 0.71f, sh * 0.54f, buttonWidth * 0.90f, buttonHeight * 0.95f } });

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
    _howToPressed          = false;
    _debugPressed          = false;
    _dungeonRunPressed     = false;
    _tileMapperPressed     = false;
    _nineSliceEditorPressed = false;
    _charAnimatorPressed    = false;
    _settingsPressed        = false;

    // Initialise editor rect from the computed border values.
    {
        float borderW = sw * 0.3703f;
        float borderH = sh * 0.6466f;
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
    if (_htpBtnTex.id == 0)
        _htpBtnTex  = LoadTexture(AssetPath("UI/HowToPlayButton.png").c_str());
}

void MainMenu::Update()
{
    // Reset all per-frame press flags so returning to menu without Init() doesn't re-fire
    _startPressed           = false;
    _quitPressed            = false;
    _howToPressed           = false;
    _debugPressed           = false;
    _dungeonRunPressed      = false;
    _tileMapperPressed      = false;
    _nineSliceEditorPressed = false;
    _charAnimatorPressed    = false;
    _settingsPressed        = false;

    float sw = (float)kVirtualWidth;
    float sh = (float)kVirtualHeight;

    Vector2 mouse = GetVirtualMousePos();

    if (IsKeyPressed(KEY_BACKSLASH)) _devToolsVisible = !_devToolsVisible;

    for (auto& button : _buttons)
    {
        if (button.text == "Debug Mode" && !_debugUnlocked)
            continue;
        if ((button.text == "Dungeon Run" || button.text == "Tile Editor" || button.text == "9-Slice Editor" || button.text == "Char Animator")
            && !_devToolsVisible)
            continue;

        bool isPanelBtn = (button.text == "Start Game" || button.text == "How To Play" ||
                           button.text == "Settings"   || button.text == "Quit");
        Rectangle checkBounds = button.bounds;
        if (isPanelBtn)
            checkBounds.y = _btnEdFirstY + button.bounds.y;
        button.hovered = CheckCollisionPointRec(mouse, checkBounds);

        button.selected = false; // reset gamepad-selection flag each frame

        if (button.hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            if (button.text == "Start Game")   _startPressed      = true;
            if (button.text == "Quit")         _quitPressed       = true;
            if (button.text == "How To Play")  _howToPressed      = true;
            if (button.text == "Debug Mode")   _debugPressed      = true;
            if (button.text == "Dungeon Run")    _dungeonRunPressed      = true;
            if (button.text == "Tile Editor")   _tileMapperPressed      = true;
            if (button.text == "9-Slice Editor") _nineSliceEditorPressed = true;
            if (button.text == "Char Animator")  _charAnimatorPressed    = true;
            if (button.text == "Settings")       _settingsPressed        = true;
        }
    }

    // -- Gamepad navigation for panel buttons (Start Game / HTP / Settings / Quit) --
    {
        // Mouse movement hands control back to the cursor
        Vector2 mouseDelta = GetMouseDelta();
        if (mouseDelta.x * mouseDelta.x + mouseDelta.y * mouseDelta.y > 4.f)
            _gpSelected = -1;
    }

    const int kPanelCount = 4;
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

            switch (_gpSelected)
            {
            case 0: _startPressed    = true; break;
            case 1: _howToPressed    = true; break;
            case 2: _settingsPressed = true; break;
            case 3: _quitPressed     = true; break;
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

    float bannerW = sw * 0.42f;
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
        const Color dark   = Color{ 52, 38, 26, 255 };
        const Color light  = Color{ 72, 54, 36, 255 };
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
        : Rectangle{ sw / 2.f - sw * 0.3703f / 2.f, sh * 0.2731f, sw * 0.3703f, sh * 0.6466f };

    if (_borderTex.id != 0)
        DrawNineSlice(_borderTex, BORDER_SRC_CORNER, BORDER_DST_CORNER, borderRect, WHITE);

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
    float bannerW = sw * 0.42f;
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

        if (button.text == "How To Play")
            tex = &_htpBtnTex;
        else if (button.text == "Quit")
            tint = Color{ 230, 80, 80, 255 };

        bool isPanelBtn = (button.text == "Start Game" || button.text == "How To Play" ||
                           button.text == "Settings"   || button.text == "Quit");
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
    }
}

bool MainMenu::StartPressed()       const { return _startPressed;       }
bool MainMenu::QuitPressed()        const { return _quitPressed;        }
bool MainMenu::HowToPressed()       const { return _howToPressed;       }
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
