#include "PauseAndGameOver.h"

static const char* UI_BASE = "C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\UI\\";

// ── Shared helper: draw a textured button, return true if clicked ─────────────
static bool DrawButton(Texture2D& tex, const char* label, Rectangle btn, Color tint)
{
    Vector2 mouse  = GetMousePosition();
    bool hovered   = CheckCollisionPointRec(mouse, btn);
    Color drawTint = hovered ? Fade(tint, 0.75f) : tint;

    if (tex.id != 0)
        DrawTexturePro(tex,
            { 0.f, 0.f, (float)tex.width, (float)tex.height },
            btn, {}, 0.f, drawTint);
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
        _borderTex  = LoadTexture(TextFormat("%sPauseBoarder.png",     UI_BASE));
    if (_btnTex.id == 0)
        _btnTex     = LoadTexture(TextFormat("%sPlayButton.png",       UI_BASE));
    if (_htpBtnTex.id == 0)
        _htpBtnTex  = LoadTexture(TextFormat("%sHowToPlayButton.png",  UI_BASE));
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
    const float panelH = padV + titleH + 3.f * (btnH + btnGap) + padV;

    float panelX = sw / 2.f - panelW / 2.f;
    float panelY = sh / 2.f - panelH / 2.f;

    // Draw border texture as the panel background (slightly oversized for visual frame)
    float borderPad = sw * 0.012f;
    if (_borderTex.id != 0)
        DrawTexturePro(_borderTex,
            { 0.f, 0.f, (float)_borderTex.width, (float)_borderTex.height },
            { panelX - borderPad, panelY - borderPad,
              panelW + borderPad * 2.f, panelH + borderPad * 2.f },
            {}, 0.f, WHITE);
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

    // Quit — red tint on play button
    if (DrawButton(_btnTex, "Quit Game", { btnX, btnY, btnW, btnH }, Color{ 230, 80, 80, 255 }))
        result = 3;

    return result;
}

// ── Game Over ─────────────────────────────────────────────────────────────────
// Returns: 0=nothing  1=play again  2=main menu  3=quit
int PauseAndGameOver::DrawGameOver(int wave, float gameTimer)
{
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();

    ClearBackground(BLACK);

    // Title
    const char* title = "GAME OVER";
    int titleSz       = (int)(sw * 0.07f);
    int titleW        = MeasureText(title, titleSz);
    DrawText(title, (int)(sw / 2.f - titleW / 2.f), (int)(sh * 0.22f), titleSz, RED);

    // Stats
    int statSz = (int)(sh * 0.032f);
    const char* waveTxt = TextFormat("Wave Reached: %d", wave);
    const char* timeTxt = TextFormat("Time Survived: %.1f s", gameTimer);
    DrawText(waveTxt, (int)(sw / 2.f - MeasureText(waveTxt, statSz) / 2.f), (int)(sh * 0.50f),               statSz, YELLOW);
    DrawText(timeTxt, (int)(sw / 2.f - MeasureText(timeTxt, statSz) / 2.f), (int)(sh * 0.50f + statSz + 12), statSz, YELLOW);

    // Buttons — Play Again, Main Menu, Quit stacked centre-screen
    const float btnW   = sw * 0.22f;
    const float btnH   = sh * 0.074f;
    const float btnGap = sh * 0.020f;
    const float btnX   = sw / 2.f - btnW / 2.f;
    float       btnY   = sh * 0.64f;

    struct { const char* label; Texture2D* tex; Color tint; } btns[3] = {
        { "Play Again", &_btnTex,    Color{ 100, 210, 120, 255 } },
        { "Main Menu",  &_htpBtnTex, WHITE                       },
        { "Quit Game",  &_btnTex,    Color{ 230, 80,  80,  255 } },
    };

    int result = 0;
    for (int i = 0; i < 3; i++)
    {
        if (DrawButton(*btns[i].tex, btns[i].label, { btnX, btnY, btnW, btnH }, btns[i].tint))
            result = i + 1;
        btnY += btnH + btnGap;
    }

    return result;
}
