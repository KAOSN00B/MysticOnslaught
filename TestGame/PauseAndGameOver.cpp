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
static bool DrawButton(Texture2D& tex, const char* label, Rectangle btn, Color tint)
{
    Vector2 mouse  = GetVirtualMousePos();
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

    // Settings — teal tint
    if (DrawButton(_btnTex, "Settings", { btnX, btnY, btnW, btnH }, Color{ 50, 170, 200, 255 }))
        result = 6;
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

// ── Game Over ─────────────────────────────────────────────────────────────────
// Returns: 0=nothing  1=retry  2=main menu  3=quit
int PauseAndGameOver::DrawGameOver()
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
