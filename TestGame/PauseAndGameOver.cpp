#include "PauseAndGameOver.h"
#include "AssetPaths.h"
#include "NineSlice.h"

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
static bool DrawButton(Texture2D& tex, const char* label, Rectangle btn, Color tint)
{
    Vector2 mouse  = GetMousePosition();
    bool hovered   = CheckCollisionPointRec(mouse, btn);
    Color drawTint = hovered ? Fade(tint, 0.75f) : tint;

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
int PauseAndGameOver::DrawPause()
{
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();

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

    float panelX = sw / 2.f - panelW / 2.f;
    float panelY = sh / 2.f - panelH / 2.f;

    // Draw border texture as the panel background (slightly oversized for visual frame)
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
    const char* title = "PAUSED";
    int titleSz = (int)(sh * 0.055f);
    int titleWpx = MeasureText(title, titleSz);
    DrawText(title, (int)(sw / 2.f - titleWpx / 2.f), (int)(panelY + padV), titleSz, BLACK);

    // Buttons
    float btnX = sw / 2.f - btnW / 2.f;
    float btnY = panelY + padV + titleH;

    int result = 0;

    // Resume — green tint on play button
    if (DrawButton(_btnTex, "Resume", { btnX, btnY, btnW, btnH }, Color{ 100, 210, 120, 255 }))
        result = 1;
    btnY += btnH + btnGap;

    // How To Play — use the HTP button texture
    if (DrawButton(_htpBtnTex, "How To Play", { btnX, btnY, btnW, btnH }, WHITE))
        result = 2;
    btnY += btnH + btnGap;

    // Button Mapping — blue tint
    if (DrawButton(_btnTex, "Button Mapping", { btnX, btnY, btnW, btnH }, Color{ 80, 150, 230, 255 }))
        result = 4;
    btnY += btnH + btnGap;

    // Main Menu — orange tint
    if (DrawButton(_htpBtnTex, "Main Menu", { btnX, btnY, btnW, btnH }, Color{ 230, 160, 50, 255 }))
        result = 5;
    btnY += btnH + btnGap;

    // Quit — red tint
    if (DrawButton(_btnTex, "Quit Game", { btnX, btnY, btnW, btnH }, Color{ 230, 80, 80, 255 }))
        result = 3;

    return result;
}

// ── Button Mapping ────────────────────────────────────────────────────────────
// Returns true when Back is pressed. Modifies bindings in place.
// Panel is draggable (touch or mouse on the title bar) to reposition it.
bool PauseAndGameOver::DrawKeybindings(KeyBindings& bindings)
{
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();

    // ── Unified input (touch maps to mouse in raylib) ─────────────────────────
    const Vector2 inputPos   = GetMousePosition();
    const bool    inputDown  = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    const bool    inputPress = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    DrawScrollingCheckerboard(sw, sh,
        Color{ 20, 74, 54, 255 },
        Color{ 34, 108, 78, 255 },
        20.f, 11.f);

    // Slot table: name, pointer to the key, clearable (KEY_NULL allowed)
    struct SlotDef { const char* name; KeyboardKey* key; bool clearable; };
    SlotDef slots[10] = {
        { "Move Up",    &bindings.moveUp,      false },
        { "Move Down",  &bindings.moveDown,    false },
        { "Move Left",  &bindings.moveLeft,    false },
        { "Move Right", &bindings.moveRight,   false },
        { "Dash",       &bindings.dash,        false },
        { "Attack Key", &bindings.attack,      true  },
        { "Ability 1",  &bindings.ability[0],  true  },
        { "Ability 2",  &bindings.ability[1],  true  },
        { "Ability 3",  &bindings.ability[2],  true  },
        { "Ability 4",  &bindings.ability[3],  true  },
    };

    // Poll for a key press when rebinding
    if (_rebindingSlot >= 0 && _rebindingSlot < 10)
    {
        if (IsKeyPressed(KEY_BACKSPACE) && slots[_rebindingSlot].clearable)
        {
            *slots[_rebindingSlot].key = KEY_NULL;
            _rebindingSlot = -1;
        }
        else
        {
            int pressed = GetKeyPressed();
            if (pressed != 0)
            {
                KeyboardKey k = (KeyboardKey)pressed;
                if (k == KEY_ESCAPE)
                    _rebindingSlot = -1;
                else if (k != KEY_BACKSPACE)
                {
                    *slots[_rebindingSlot].key = k;
                    _rebindingSlot = -1;
                }
            }
        }
    }

    // Layout
    const float panelW      = sw * 0.42f;
    const float rowH        = sh * 0.058f;
    const float rowGap      = sh * 0.010f;
    const float padV        = sh * 0.036f;
    const float titleH      = sh * 0.065f;
    const float groupLabelH = sh * 0.030f;
    const float groupGap    = sh * 0.022f;
    const float backH       = sh * 0.062f;
    const float hintH       = sh * 0.022f;

    // 3 groups: 4 + 2 + 4 rows
    const float panelH = padV + titleH
        + groupLabelH + 4.f * (rowH + rowGap)
        + groupGap
        + groupLabelH + 2.f * (rowH + rowGap)
        + groupGap
        + groupLabelH + 4.f * (rowH + rowGap)
        + groupGap + hintH + rowGap + backH + padV;

    float panelX = sw / 2.f - panelW / 2.f + _keybindPanelOffset.x;
    float panelY = sh / 2.f - panelH / 2.f + _keybindPanelOffset.y;

    // ── Panel drag (title bar is the drag handle) ─────────────────────────────
    const Rectangle titleBarRect{ panelX, panelY, panelW, padV + titleH };
    if (_keybindDragging)
    {
        if (inputDown)
        {
            Vector2 delta = { inputPos.x - _keybindDragStart.x, inputPos.y - _keybindDragStart.y };
            _keybindPanelOffset = { _keybindPanelAtDrag.x + delta.x, _keybindPanelAtDrag.y + delta.y };
            panelX = sw / 2.f - panelW / 2.f + _keybindPanelOffset.x;
            panelY = sh / 2.f - panelH / 2.f + _keybindPanelOffset.y;
        }
        else
        {
            _keybindDragging = false;
        }
    }
    else if (inputPress && _rebindingSlot < 0 &&
             CheckCollisionPointRec(inputPos, titleBarRect))
    {
        _keybindDragging    = true;
        _keybindDragStart   = inputPos;
        _keybindPanelAtDrag = _keybindPanelOffset;
    }

    DrawRectangleRounded({ panelX, panelY, panelW, panelH }, 0.05f, 8, Fade(Color{14, 64, 48, 255}, 0.94f));
    DrawRectangleRoundedLines({ panelX, panelY, panelW, panelH }, 0.05f, 8, Fade(Color{140, 255, 208, 255}, 0.38f));

    // Drag hint strip on title bar when hovered
    if (!_keybindDragging && CheckCollisionPointRec(inputPos, titleBarRect))
        DrawRectangleRounded(titleBarRect, 0.05f, 4, Fade(WHITE, 0.06f));

    // Title
    const char* title   = "BUTTON MAPPING";
    int         titleSz = (int)(sh * 0.046f);
    DrawText(title,
        (int)(panelX + panelW / 2.f - MeasureText(title, titleSz) / 2.f),
        (int)(panelY + padV), titleSz, Color{255, 212, 94, 255});

    int nameSz   = (int)(sh * 0.026f);
    int keySz    = (int)(sh * 0.029f);
    int groupSz  = (int)(sh * 0.022f);

    struct GroupDef { const char* name; int start; int count; };
    GroupDef groups[3] = {
        { "MOVEMENT",  0, 4 },
        { "ACTIONS",   4, 2 },
        { "ABILITIES", 6, 4 },
    };

    float rowY = panelY + padV + titleH;

    for (int g = 0; g < 3; g++)
    {
        if (g > 0) rowY += groupGap;

        // Group label
        DrawText(groups[g].name,
            (int)(panelX + panelW * 0.05f),
            (int)(rowY), groupSz, Fade(Color{255, 212, 94, 255}, 0.82f));
        rowY += groupLabelH;

        for (int ri = 0; ri < groups[g].count; ri++)
        {
            int slotIdx = groups[g].start + ri;
            float rowX     = panelX + panelW * 0.05f;
            float rowRight = panelX + panelW * 0.95f;

            DrawRectangleRounded({ rowX, rowY, panelW * 0.90f, rowH }, 0.2f, 4, Fade(Color{170, 255, 216, 255}, 0.12f));

            DrawText(slots[slotIdx].name,
                (int)(rowX + 12.f),
                (int)(rowY + rowH * 0.5f - nameSz * 0.5f),
                nameSz, Color{219, 245, 232, 255});

            float badgeW = sw * 0.095f;
            float badgeH = rowH * 0.70f;
            float badgeX = rowRight - badgeW - 4.f;
            float badgeY = rowY + rowH * 0.5f - badgeH * 0.5f;
            Rectangle badgeRect{ badgeX, badgeY, badgeW, badgeH };

            bool rebinding = (_rebindingSlot == slotIdx);
            bool hovered   = CheckCollisionPointRec(inputPos, badgeRect) && !rebinding && !_keybindDragging;

            Color badgeCol = rebinding ? Fade(GOLD, 0.85f)
                           : hovered   ? Fade(Color{110, 255, 220, 255}, 0.82f)
                                       : Fade(Color{42, 126, 94, 255}, 0.88f);
            DrawRectangleRounded(badgeRect, 0.3f, 4, badgeCol);
            DrawRectangleRoundedLines(badgeRect, 0.3f, 4, Fade(Color{210, 255, 231, 255}, 0.52f));

            const char* keyLabel = rebinding ? "..." : GetKeyName(*slots[slotIdx].key);
            int labelW = MeasureText(keyLabel, keySz);
            DrawText(keyLabel,
                (int)(badgeX + badgeW / 2.f - labelW / 2.f),
                (int)(badgeY + badgeH / 2.f - keySz  / 2.f),
                keySz, rebinding ? Color{46, 40, 12, 255} : Color{243, 255, 249, 255});

            if (hovered && inputPress)
                _rebindingSlot = slotIdx;

            rowY += rowH + rowGap;
        }
    }

    // Hint text
    rowY += groupGap;
    const char* hintText = (_rebindingSlot >= 0)
        ? (slots[_rebindingSlot].clearable
            ? "Press a key  (ESC cancels  |  BKSP to clear)"
            : "Press a key  (ESC cancels)")
        : "Tap a badge to rebind  |  drag title to move panel";
    int hintSz = (int)(sh * 0.019f);
    DrawText(hintText,
        (int)(panelX + panelW / 2.f - MeasureText(hintText, hintSz) / 2.f),
        (int)(rowY), hintSz, Fade(Color{214, 250, 233, 255}, 0.72f));

    // Back button
    rowY += hintH + rowGap;
    float backW = panelW * 0.40f;
    float backX = panelX + panelW / 2.f - backW / 2.f;
    bool done   = DrawButton(_btnTex, "Back", { backX, rowY, backW, backH }, Color{ 118, 220, 176, 255 });

    return done;
}

// ── Game Over ─────────────────────────────────────────────────────────────────
// Returns: 0=nothing  1=retry  2=main menu  3=quit
int PauseAndGameOver::DrawGameOver()
{
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();

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

    // Buttons — all centred
    float btnX = sw / 2.f - btnW / 2.f;
    float btnY = panelY + padV + titleH;

    int result = 0;

    if (DrawButton(_btnTex,    "Retry",     { btnX, btnY, btnW, btnH }, Color{ 100, 210, 120, 255 }))
        result = 1;
    btnY += btnH + btnGap;

    if (DrawButton(_htpBtnTex, "Main Menu", { btnX, btnY, btnW, btnH }, Color{ 230, 160,  50, 255 }))
        result = 2;
    btnY += btnH + btnGap;

    if (DrawButton(_btnTex,    "Quit Game", { btnX, btnY, btnW, btnH }, Color{ 230,  80,  80, 255 }))
        result = 3;

    return result;
}
