#include "PauseAndGameOver.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "VirtualCanvas.h"
#include "NineSlice.h"
#include "VirtualCanvas.h"

#include <cmath>

// ── 9-slice corner sizes ──────────────────────────────────────────────────────
// Border values tuned via 9-Slice Editor (MainMenuBorder.png).
static constexpr float BORDER_SRC_CORNER = 1.f;
static constexpr float BORDER_DST_CORNER = 16.f;
static constexpr float BTN_SRC_CORNER    = 8.f;
static constexpr float BTN_DST_CORNER    = 16.f;

static void DrawScrollingCheckerboard(float sw, float sh, Color dark, Color light, float speedX, float speedY, int cell = 80)
{
    const int period = cell * 2;
    float t    = (float)GetTime();
    int   offX = (int)fmodf(t * speedX, (float)period);
    int   offY = (int)fmodf(t * speedY, (float)period);
    int   phaseX = offX / cell;
    int   phaseY = offY / cell;
    int   pixX   = offX % cell;
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

// ── Shared helper: draw a textured button, return true if clicked ─────────────
// selected=true means the gamepad cursor is on this button (gold bloom + expansion)
static bool DrawButton(Texture2D& tex, const char* label, Rectangle btn, Color tint, bool selected = false)
{
    Vector2 mouse      = GetVirtualMousePos();
    bool    hovered    = CheckCollisionPointRec(mouse, btn);
    bool    highlighted = hovered || selected;

    if (highlighted)
    {
        // Expand the button rect ("bloom bigger")
        float pulse  = 0.5f + 0.5f * sinf((float)GetTime() * 5.f);
        float expand = btn.width * 0.04f;
        btn = { btn.x - expand,           btn.y - expand * 0.7f,
                btn.width + expand * 2.f, btn.height + expand * 1.4f };

        // Gold glow for gamepad selection, white glow for mouse hover
        Color glowCol = selected
            ? Color{ 255, (unsigned char)(200 + (int)(45.f * pulse)), 40,
                     (unsigned char)(160 + (int)(80.f * pulse)) }
            : Color{ 255, 255, 255, (unsigned char)(90 + (int)(70.f * pulse)) };
        DrawRectangleRoundedLines({ btn.x - 4.f, btn.y - 4.f, btn.width + 8.f, btn.height + 8.f },
                                   0.25f, 8, glowCol);
    }

    Color drawTint = highlighted ? tint : Fade(tint, 0.78f);

    if (tex.id != 0)
        DrawNineSlice(tex, BTN_SRC_CORNER, BTN_DST_CORNER, btn, drawTint);
    else
    {
        DrawRectangleRounded(btn, 0.22f, 6, drawTint);
        DrawRectangleRoundedLines(btn, 0.22f, 6, Fade(WHITE, 0.35f));
    }

    int fontSize = 30;
    int textW    = MeasureText(label, fontSize);
    DrawText(label,
        (int)(btn.x + btn.width  / 2.f - textW    / 2.f),
        (int)(btn.y + btn.height / 2.f - fontSize / 2.f),
        fontSize, BLACK);

    return hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

PauseAndGameOver::~PauseAndGameOver()
{
    Unload();
}

void PauseAndGameOver::Init()
{
    if (_borderTex.id == 0)
        _borderTex  = LoadTexture(AssetPath("UI/PauseBoarder.png").c_str());
    if (_btnTex.id == 0)
        _btnTex     = LoadTexture(AssetPath("UI/PlayButton.png").c_str());
    if (_htpBtnTex.id == 0)
        _htpBtnTex  = LoadTexture(AssetPath("UI/HowToPlayButton.png").c_str());
}

void PauseAndGameOver::Unload()
{
    if (_borderTex.id  != 0) { UnloadTexture(_borderTex);  _borderTex  = {}; }
    if (_btnTex.id     != 0) { UnloadTexture(_btnTex);     _btnTex     = {}; }
    if (_htpBtnTex.id  != 0) { UnloadTexture(_htpBtnTex);  _htpBtnTex  = {}; }
}

// ── Pause ─────────────────────────────────────────────────────────────────────
// Returns: 0=nothing  1=resume  2=howtoplay  3=quit
int PauseAndGameOver::DrawPause(InputPromptMode promptMode)
{
    float sw = (float)kVirtualWidth;
    float sh = (float)kVirtualHeight;

    // Dim overlay
    DrawRectangle(0, 0, (int)sw, (int)sh, Fade(BLACK, 0.65f));

    // Panel dimensions
    const float btnW   = sw * 0.22f;
    const float btnH   = sh * 0.074f;
    const float btnGap = sh * 0.018f;
    const float padV   = sh * 0.045f;
    const float titleH = sh * 0.075f;
    const float panelW = btnW + sw * 0.06f;
    const float panelH = padV + titleH + 5.f * (btnH + btnGap) + padV;

    float panelX    = sw / 2.f - panelW / 2.f;
    float borderPad = sw * 0.03f;

    Rectangle naturalBorderRect{ sw * 0.3300f, sh * 0.0768f, sw * 0.3400f, sh * 0.8391f };
    float naturalPanelY = naturalBorderRect.y + borderPad;

    // On first editor activation, seed the editor rect from the natural layout.
    if (_editorActive && !_editorInited)
    { _edRect = naturalBorderRect; _editorInited = true; }

    Rectangle borderRect = _editorActive ? _edRect : naturalBorderRect;
    // Draw border texture as the panel background (slightly oversized for visual frame)
    float panelY = _editorActive ? (_edRect.y + borderPad) : naturalPanelY;

    if (_borderTex.id != 0)
        DrawNineSlice(_borderTex, BORDER_SRC_CORNER, BORDER_DST_CORNER, borderRect, WHITE);
    else
    {
        DrawRectangleRounded({ panelX, panelY, panelW, panelH }, 0.08f, 8, Fade(BLACK, 0.92f));
        DrawRectangleRoundedLines({ panelX, panelY, panelW, panelH }, 0.08f, 8, Fade(WHITE, 0.35f));
    }

    // ── Border editor overlay ─────────────────────────────────────────────────
    if (_editorActive)
    {
        Vector2 mouse = GetVirtualMousePos();
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

        // Mouse input — priority: edge handles > button group > border interior
        float groupH2 = 5.f * (btnH + btnGap);
        Rectangle btnGroupRect{ sw / 2.f - btnW / 2.f - 8.f, _btnEdY - 8.f,
                                 btnW + 16.f, groupH2 + 16.f };

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            _edHandle = -1; _btnDragging = false;

            // 1. Edge handles
            for (int i = 0; i < 8; i++)
            {
                Vector2 h = handlePos(i);
                if (CheckCollisionPointRec(mouse, { h.x - hs, h.y - hs, hs * 2.f, hs * 2.f }))
                { _edHandle = i; break; }
            }

            if (_edHandle == -1)
            {
                // 2. Button group (inside border — checked before interior)
                if (CheckCollisionPointRec(mouse, btnGroupRect))
                { _btnDragging = true; _btnDragStartMY = mouse.y; _btnDragStartY = _btnEdY; }
                // 3. Border interior
                else if (CheckCollisionPointRec(mouse, _edRect))
                { _edHandle = -2; }
            }

            if (_edHandle != -1) { _edDragStart = mouse; _edRectStart = _edRect; }
        }
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) { _edHandle = -1; _btnDragging = false; }

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

        // Draw handles
        DrawRectangleLinesEx(_edRect, 1.5f, Color{ 255, 220, 0, 180 });
        for (int i = 0; i < 8; i++)
        {
            Vector2 h = handlePos(i);
            Color hcol = (_edHandle == i) ? Color{ 255, 255, 80, 255 } : Color{ 255, 200, 0, 220 };
            DrawRectangle((int)(h.x - hs), (int)(h.y - hs), (int)(hs * 2.f), (int)(hs * 2.f), hcol);
        }
        DrawText("[F1] close  [S] export", (int)(_edRect.x + 4), (int)(_edRect.y - 18), 14,
            Color{ 255, 220, 0, 200 });

        if (IsKeyPressed(KEY_S))
        {
            TraceLog(LOG_INFO, "=== Pause Border Export ===");
            TraceLog(LOG_INFO, "borderRect.x = sw * %.4ff;", _edRect.x / sw);
            TraceLog(LOG_INFO, "borderRect.y = sh * %.4ff;", _edRect.y / sh);
            TraceLog(LOG_INFO, "borderRect.w = sw * %.4ff;", _edRect.width  / sw);
            TraceLog(LOG_INFO, "borderRect.h = sh * %.4ff;", _edRect.height / sh);
            TraceLog(LOG_INFO, "btnY         = sh * %.4ff;", _btnEdY / sh);
            TraceLog(LOG_INFO, "===========================");
        }
    }

    // Seed the button group Y on first editor activation
    float naturalBtnY = panelY + padV + titleH;
    if (_editorActive && !_btnInited) { _btnEdY = naturalBtnY; _btnInited = true; }
    float activeBtnY = _editorActive ? _btnEdY : naturalBtnY;

    // Button group drag
    if (_editorActive)
    {
        Vector2 mouse = GetVirtualMousePos();
        float groupH = 5.f * (btnH + btnGap);
        Rectangle groupRect{ sw / 2.f - btnW / 2.f - 8.f, activeBtnY - 8.f,
                              btnW + 16.f, groupH + 16.f };

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && _edHandle == -1)
        {
            if (CheckCollisionPointRec(mouse, groupRect))
            { _btnDragging = true; _btnDragStartMY = mouse.y; _btnDragStartY = _btnEdY; }
        }
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) _btnDragging = false;
        if (_btnDragging)
            _btnEdY = _btnDragStartY + (mouse.y - _btnDragStartMY);
        activeBtnY = _btnEdY;

        // Button group outline
        groupRect.y = activeBtnY - 8.f;
        DrawRectangleLinesEx(groupRect, 1.f,
            _btnDragging ? Color{ 100, 255, 160, 220 } : Color{ 80, 220, 120, 120 });
        DrawText("drag", (int)(groupRect.x + 4), (int)(groupRect.y + 2), 12,
            Color{ 80, 220, 120, 160 });
    }

    // Title sits above buttons
    const char* title = "PAUSED";
    int titleSz  = (int)(sh * 0.055f);
    int titleWpx = MeasureText(title, titleSz);
    DrawText(title, (int)(sw / 2.f - titleWpx / 2.f),
        (int)(activeBtnY - titleH), titleSz, BLACK);

    // ── Gamepad D-pad / left-stick navigation ─────────────────────────────────
    if (IsGamepadAvailable(0))
    {
        bool moveDown = IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN);
        bool moveUp   = IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_UP);

        _gpPauseStickCooldown -= GetFrameTime();
        float stickY = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
        if (_gpPauseStickCooldown <= 0.f)
        {
            if      (stickY >  0.5f) { moveDown = true; _gpPauseStickCooldown = 0.22f; }
            else if (stickY < -0.5f) { moveUp   = true; _gpPauseStickCooldown = 0.22f; }
            else                       _gpPauseStickCooldown = 0.f;
        }

        if (moveDown) _gpPauseSelected = (_gpPauseSelected + 1) % 6;
        if (moveUp)   _gpPauseSelected = (_gpPauseSelected - 1 + 6) % 6;
    }

    float btnX = sw / 2.f - btnW / 2.f;
    float btnY = activeBtnY;
    int   result = 0;

    // Gamepad A = confirm selection, B / Circle = Resume shortcut
    bool gpActive = IsGamepadAvailable(0);
    if (gpActive)
    {
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN))
        {
            int map[6] = {1, 2, 4, 6, 5, 3};
            result = map[_gpPauseSelected];
        }
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT))
            result = 1;
    }

    const char* navHint = (promptMode == InputPromptMode::Gamepad)
        ? "Left Stick / D-Pad: Choose   A: Select   B: Resume"
        : (promptMode == InputPromptMode::Touch)
            ? "Tap: Select"
            : "Click: Select   Esc: Resume";
    int navFs = 20;
    int navW = MeasureText(navHint, navFs);
    DrawText(navHint, (int)(sw / 2.f - navW / 2.f), (int)(activeBtnY + 5.f * (btnH + btnGap) + 16.f), navFs, Fade(BLACK, 0.70f));

    // Resume — green
    if (DrawButton(_btnTex, "Resume", { btnX, btnY, btnW, btnH },
            Color{ 100, 210, 120, 255 }, gpActive && _gpPauseSelected == 0))
        result = 1;
    btnY += btnH + btnGap;

    // How To Play
    if (DrawButton(_htpBtnTex, "How To Play", { btnX, btnY, btnW, btnH },
            WHITE, gpActive && _gpPauseSelected == 1))
        result = 2;
    btnY += btnH + btnGap;

    // Button Mapping — blue
    if (DrawButton(_btnTex, "Button Mapping", { btnX, btnY, btnW, btnH },
            Color{ 80, 150, 230, 255 }, gpActive && _gpPauseSelected == 2))
        result = 4;
    btnY += btnH + btnGap;

    // Settings — teal
    if (DrawButton(_btnTex, "Settings", { btnX, btnY, btnW, btnH },
            Color{ 50, 170, 200, 255 }, gpActive && _gpPauseSelected == 3))
        result = 6;
    btnY += btnH + btnGap;

    // Main Menu — orange
    if (DrawButton(_htpBtnTex, "Main Menu", { btnX, btnY, btnW, btnH },
            Color{ 230, 160, 50, 255 }, gpActive && _gpPauseSelected == 4))
        result = 5;
    btnY += btnH + btnGap;

    // Quit — red
    if (DrawButton(_btnTex, "Quit Game", { btnX, btnY, btnW, btnH },
            Color{ 230, 80, 80, 255 }, gpActive && _gpPauseSelected == 5))
        result = 3;

    return result;
}

// ── Game Over ─────────────────────────────────────────────────────────────────
// Returns: 0=nothing  1=retry  2=main menu  3=quit
int PauseAndGameOver::DrawGameOver(InputPromptMode promptMode)
{
    float sw = (float)kVirtualWidth;
    float sh = (float)kVirtualHeight;

    DrawScrollingCheckerboard(sw, sh,
        Color{ 60, 10, 10, 255 },
        Color{ 90, 20, 20, 255 },
        18.f, 12.f);

    // Panel dimensions — same proportions as pause
    const float btnW   = sw * 0.22f;
    const float btnH   = sh * 0.074f;
    const float btnGap = sh * 0.018f;
    const float padV   = sh * 0.045f;
    const float titleH = sh * 0.090f;
    const float panelW = btnW + sw * 0.06f;
    const float panelH = padV + titleH + 3.f * (btnH + btnGap) + padV;

    float panelX = sw / 2.f - panelW / 2.f;
    float panelY = sh / 2.f - panelH / 2.f;

    float borderPad = sw * 0.03f;
    if (_borderTex.id != 0)
        DrawNineSlice(_borderTex, BORDER_SRC_CORNER, BORDER_DST_CORNER,
            { panelX - borderPad, panelY - borderPad,
              panelW + borderPad * 2.f, panelH + borderPad * 2.f }, WHITE);
    else
    {
        DrawRectangleRounded({ panelX, panelY, panelW, panelH }, 0.08f, 8, Fade(BLACK, 0.92f));
        DrawRectangleRoundedLines({ panelX, panelY, panelW, panelH }, 0.08f, 8, Fade(WHITE, 0.35f));
    }

    // Title
    const char* title = "GAME OVER";
    int titleSz = (int)(sh * 0.060f);
    DrawText(title,
        (int)(sw / 2.f - MeasureText(title, titleSz) / 2.f),
        (int)(panelY + padV), titleSz, RED);

    // ── Gamepad D-pad / left-stick navigation ─────────────────────────────────
    if (IsGamepadAvailable(0))
    {
        bool moveDown = IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN);
        bool moveUp   = IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_UP);

        _gpGameOverStickCooldown -= GetFrameTime();
        float stickY = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
        if (_gpGameOverStickCooldown <= 0.f)
        {
            if      (stickY >  0.5f) { moveDown = true; _gpGameOverStickCooldown = 0.22f; }
            else if (stickY < -0.5f) { moveUp   = true; _gpGameOverStickCooldown = 0.22f; }
            else                       _gpGameOverStickCooldown = 0.f;
        }

        if (moveDown) _gpGameOverSelected = (_gpGameOverSelected + 1) % 3;
        if (moveUp)   _gpGameOverSelected = (_gpGameOverSelected - 1 + 3) % 3;
    }

    // Buttons — all centred
    float btnX = sw / 2.f - btnW / 2.f;
    float btnY = panelY + padV + titleH;
    int   result = 0;

    bool gpActive = IsGamepadAvailable(0);
    if (gpActive && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN))
    {
        int map[3] = {1, 2, 3};
        result = map[_gpGameOverSelected];
    }

    const char* goHint = (promptMode == InputPromptMode::Gamepad)
        ? "Left Stick / D-Pad: Choose   A: Select"
        : (promptMode == InputPromptMode::Touch)
            ? "Tap: Select"
            : "Click: Select";
    int goFs = 20;
    int goW = MeasureText(goHint, goFs);
    DrawText(goHint, (int)(sw / 2.f - goW / 2.f), (int)(btnY + 3.f * (btnH + btnGap) + 14.f), goFs, Fade(RAYWHITE, 0.70f));

    if (DrawButton(_btnTex,    "Retry",     { btnX, btnY, btnW, btnH },
            Color{ 100, 210, 120, 255 }, gpActive && _gpGameOverSelected == 0))
        result = 1;
    btnY += btnH + btnGap;

    if (DrawButton(_htpBtnTex, "Main Menu", { btnX, btnY, btnW, btnH },
            Color{ 230, 160,  50, 255 }, gpActive && _gpGameOverSelected == 1))
        result = 2;
    btnY += btnH + btnGap;

    if (DrawButton(_btnTex,    "Quit Game", { btnX, btnY, btnW, btnH },
            Color{ 230,  80,  80, 255 }, gpActive && _gpGameOverSelected == 2))
        result = 3;

    return result;
}
