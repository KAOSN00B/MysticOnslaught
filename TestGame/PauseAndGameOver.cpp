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

// ── Shared helper: draw a textured button ─────────────────────────────────────
// selected=true means the gamepad cursor is on this button (gold bloom + expansion)
// Drawing only: clicks are read by ButtonClicked from UpdatePause.
static void DrawButton(Texture2D tex, const char* label, Rectangle btn, Color tint, bool selected = false)
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
}

// Hit test against the un-expanded rect, matching what DrawButton highlights.
static bool ButtonClicked(Rectangle btn)
{
    return CheckCollisionPointRec(GetVirtualMousePos(), btn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

// ── Pause menu buttons, top to bottom ─────────────────────────────────────────
// The gamepad cursor walks the same order, so one table maps index -> action.
static constexpr int kPauseButtonCount = 8;
static constexpr int kCharacterSheetAction = 8;
struct PauseButtonDef
{
    int         action;            // UpdatePause result code
    const char* label;
    const char* runLabel;          // label during a resumable run (nullptr = same)
    bool        howToPlayTexture;  // HowToPlayButton.png instead of PlayButton.png
    Color       tint;
};
static const PauseButtonDef kPauseButtons[kPauseButtonCount] = {
    { 1, "Resume",         nullptr,            false, Color{ 100, 210, 120, 255 } },  // green
    { kCharacterSheetAction, "Character", nullptr, false, Color{ 225, 190, 90, 255 } },  // gold
    { 2, "How To Play",    nullptr,            true,  WHITE                       },
    { 7, "Grimoire",       nullptr,            false, Color{ 170, 130, 230, 255 } },  // violet
    { 4, "Button Mapping", nullptr,            false, Color{ 80, 150, 230, 255 }  },  // blue
    { 6, "Settings",       nullptr,            false, Color{ 50, 170, 200, 255 }  },  // teal
    { 5, "Main Menu",      "Save & Main Menu", true,  Color{ 230, 160, 50, 255 }  },  // orange
    { 3, "Quit Game",      "Save & Quit Game", false, Color{ 230, 80, 80, 255 }   },  // red
};

// Border editor handle size and positions (corners, then edge midpoints).
static constexpr float kEditorHandleHalf = 10.f;
static Vector2 EditorHandlePos(const Rectangle& r, int i)
{
    switch (i) {
        case 0: return { r.x,               r.y                };
        case 1: return { r.x + r.width,     r.y                };
        case 2: return { r.x,               r.y + r.height     };
        case 3: return { r.x + r.width,     r.y + r.height     };
        case 4: return { r.x + r.width/2.f, r.y                };
        case 5: return { r.x + r.width/2.f, r.y + r.height     };
        case 6: return { r.x,               r.y + r.height/2.f };
        default:return { r.x + r.width,     r.y + r.height/2.f };
    }
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
PauseAndGameOver::PauseLayout PauseAndGameOver::ComputePauseLayout() const
{
    PauseLayout l{};
    l.sw = (float)kVirtualWidth;
    l.sh = (float)kVirtualHeight;

    // Panel dimensions
    l.btnW   = l.sw * 0.22f;
    l.btnH   = l.sh * 0.058f;   // eight buttons fit the frame
    l.btnGap = l.sh * 0.016f;
    l.padV   = l.sh * 0.045f;
    l.titleH = l.sh * 0.075f;
    l.panelW = l.btnW + l.sw * 0.06f;
    l.panelH = l.padV + l.titleH + (float)kPauseButtonCount * (l.btnH + l.btnGap) + l.padV;
    l.panelX = l.sw / 2.f - l.panelW / 2.f;

    const float borderPad = l.sw * 0.03f;
    const Rectangle naturalBorderRect{ l.sw * 0.3300f, l.sh * 0.0768f, l.sw * 0.3400f, l.sh * 0.8391f };
    const bool edited = _editorActive && _editorInited;
    // Border texture is the panel background (slightly oversized for visual frame).
    l.borderRect  = edited ? _edRect : naturalBorderRect;
    l.panelY      = edited ? (_edRect.y + borderPad) : (naturalBorderRect.y + borderPad);
    l.naturalBtnY = l.panelY + l.padV + l.titleH;
    return l;
}

float PauseAndGameOver::ActivePauseButtonY(const PauseLayout& layout) const
{
    return (_editorActive && _btnInited) ? _btnEdY : layout.naturalBtnY;
}

Rectangle PauseAndGameOver::PauseButtonRect(const PauseLayout& layout, float activeBtnY, int index)
{
    // Accumulate like the original stacked layout so positions stay bit-identical.
    float y = activeBtnY;
    for (int i = 0; i < index; i++) y += layout.btnH + layout.btnGap;
    return { layout.sw / 2.f - layout.btnW / 2.f, y, layout.btnW, layout.btnH };
}

int PauseAndGameOver::UpdatePause()
{
    // On first editor activation, seed the editor rect from the natural layout.
    if (_editorActive && !_editorInited)
    { _edRect = ComputePauseLayout().borderRect; _editorInited = true; }

    const PauseLayout layout = ComputePauseLayout();
    const float sw = layout.sw;
    const float sh = layout.sh;
    const float btnW = layout.btnW, btnH = layout.btnH, btnGap = layout.btnGap;

    // ── Border editor input ───────────────────────────────────────────────────
    if (_editorActive)
    {
        Vector2 mouse = GetVirtualMousePos();
        const float hs = kEditorHandleHalf;

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
                Vector2 h = EditorHandlePos(_edRect, i);
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
    if (_editorActive && !_btnInited) { _btnEdY = layout.naturalBtnY; _btnInited = true; }

    // Button group drag
    if (_editorActive)
    {
        Vector2 mouse = GetVirtualMousePos();
        float groupH = 5.f * (btnH + btnGap);
        Rectangle groupRect{ sw / 2.f - btnW / 2.f - 8.f, _btnEdY - 8.f,
                              btnW + 16.f, groupH + 16.f };

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && _edHandle == -1)
        {
            if (CheckCollisionPointRec(mouse, groupRect))
            { _btnDragging = true; _btnDragStartMY = mouse.y; _btnDragStartY = _btnEdY; }
        }
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) _btnDragging = false;
        if (_btnDragging)
            _btnEdY = _btnDragStartY + (mouse.y - _btnDragStartMY);
    }
    const float activeBtnY = ActivePauseButtonY(layout);

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

        if (moveDown) _gpPauseSelected = (_gpPauseSelected + 1) % kPauseButtonCount;
        if (moveUp)   _gpPauseSelected = (_gpPauseSelected - 1 + kPauseButtonCount) % kPauseButtonCount;
    }

    int result = 0;

    // Gamepad A = confirm selection, B / Circle = Resume shortcut
    if (IsGamepadAvailable(0))
    {
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN))
            result = kPauseButtons[_gpPauseSelected].action;
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT))
            result = 1;
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_TRIGGER_1) ||
            IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_TRIGGER_1))
            result = kCharacterSheetAction;
    }

    // Q / E / Tab switch to the Character page, like changing tabs.
    if (IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_TAB))
        result = kCharacterSheetAction;

    // Mouse / touch clicks (a click wins over a same-frame gamepad press, as before).
    for (int i = 0; i < kPauseButtonCount; i++)
        if (ButtonClicked(PauseButtonRect(layout, activeBtnY, i)))
            result = kPauseButtons[i].action;

    return result;
}

void PauseAndGameOver::DrawPause(InputPromptMode promptMode, bool runInProgress) const
{
    const PauseLayout layout = ComputePauseLayout();
    const float sw = layout.sw;
    const float sh = layout.sh;
    const float btnW = layout.btnW, btnH = layout.btnH, btnGap = layout.btnGap;

    // Dim overlay
    DrawRectangle(0, 0, (int)sw, (int)sh, Fade(BLACK, 0.65f));

    if (_borderTex.id != 0)
        DrawNineSlice(_borderTex, BORDER_SRC_CORNER, BORDER_DST_CORNER, layout.borderRect, WHITE);
    else
    {
        const Rectangle panel{ layout.panelX, layout.panelY, layout.panelW, layout.panelH };
        DrawRectangleRounded(panel, 0.08f, 8, Fade(BLACK, 0.92f));
        DrawRectangleRoundedLines(panel, 0.08f, 8, Fade(WHITE, 0.35f));
    }

    // ── Border editor overlay ─────────────────────────────────────────────────
    if (_editorActive)
    {
        const float hs = kEditorHandleHalf;
        DrawRectangleLinesEx(_edRect, 1.5f, Color{ 255, 220, 0, 180 });
        for (int i = 0; i < 8; i++)
        {
            Vector2 h = EditorHandlePos(_edRect, i);
            Color hcol = (_edHandle == i) ? Color{ 255, 255, 80, 255 } : Color{ 255, 200, 0, 220 };
            DrawRectangle((int)(h.x - hs), (int)(h.y - hs), (int)(hs * 2.f), (int)(hs * 2.f), hcol);
        }
        DrawText("[F1] close  [S] export", (int)(_edRect.x + 4), (int)(_edRect.y - 18), 14,
            Color{ 255, 220, 0, 200 });
    }

    const float activeBtnY = ActivePauseButtonY(layout);

    // Button group outline
    if (_editorActive)
    {
        float groupH = 5.f * (btnH + btnGap);
        Rectangle groupRect{ sw / 2.f - btnW / 2.f - 8.f, activeBtnY - 8.f,
                              btnW + 16.f, groupH + 16.f };
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
        (int)(activeBtnY - layout.titleH), titleSz, BLACK);

    const char* navHint = (promptMode == InputPromptMode::Gamepad)
        ? "A: Select   B: Resume   LB / RB: Character"
        : (promptMode == InputPromptMode::Touch)
            ? "Tap: Select"
            : "Esc: Resume   Q / E: Character";
    int navFs = 20;
    int navW = MeasureText(navHint, navFs);
    DrawText(navHint, (int)(sw / 2.f - navW / 2.f),
        (int)(activeBtnY + (float)kPauseButtonCount * (btnH + btnGap) + 4.f), navFs, Fade(BLACK, 0.70f));

    const bool gpActive = IsGamepadAvailable(0);
    for (int i = 0; i < kPauseButtonCount; i++)
    {
        const PauseButtonDef& button = kPauseButtons[i];
        const char* label = (runInProgress && button.runLabel) ? button.runLabel : button.label;
        DrawButton(button.howToPlayTexture ? _htpBtnTex : _btnTex, label,
                   PauseButtonRect(layout, activeBtnY, i), button.tint,
                   gpActive && _gpPauseSelected == i);
    }
}
