#include "PauseAndGameOver.h"

// ── Shared helper: draw a rounded button and return true if clicked ───────────
static bool DrawButton(const char* label, Rectangle btn, Color col)
{
    Vector2 mouse = GetMousePosition();
    bool hovered  = CheckCollisionPointRec(mouse, btn);
    Color c       = hovered ? Fade(col, 0.75f) : Fade(col, 0.92f);

    DrawRectangleRounded(btn, 0.22f, 6, c);
    DrawRectangleRoundedLines(btn, 0.22f, 6, Fade(WHITE, 0.35f));

    int fontSize  = 30;
    int textW     = MeasureText(label, fontSize);
    DrawText(label,
        (int)(btn.x + btn.width  / 2.f - textW    / 2.f),
        (int)(btn.y + btn.height / 2.f - fontSize / 2.f),
        fontSize, WHITE);

    return hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
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
    const float btnW   = sw * 0.22f;    // ~422 px at 1920
    const float btnH   = sh * 0.074f;   // ~80 px at 1080
    const float btnGap = sh * 0.018f;
    const float padV   = sh * 0.045f;
    const float titleH = sh * 0.075f;
    const float panelW = btnW + sw * 0.06f;
    const float panelH = padV + titleH + 3.f * (btnH + btnGap) + padV;

    float panelX = sw / 2.f - panelW / 2.f;
    float panelY = sh / 2.f - panelH / 2.f;

    DrawRectangleRounded({ panelX, panelY, panelW, panelH }, 0.08f, 8, Fade(BLACK, 0.92f));
    DrawRectangleRoundedLines({ panelX, panelY, panelW, panelH }, 0.08f, 8, Fade(WHITE, 0.35f));

    // Title
    const char* title = "PAUSED";
    int titleSz = (int)(sh * 0.055f);
    int titleW  = MeasureText(title, titleSz);
    DrawText(title, (int)(sw / 2.f - titleW / 2.f), (int)(panelY + padV), titleSz, WHITE);

    // Buttons
    float btnX = sw / 2.f - btnW / 2.f;
    float btnY = panelY + padV + titleH;

    struct { const char* label; Color col; } btns[3] = {
        { "Resume",      GREEN },
        { "How To Play", BLUE  },
        { "Quit Game",   RED   },
    };

    int result = 0;
    for (int i = 0; i < 3; i++)
    {
        if (DrawButton(btns[i].label, { btnX, btnY, btnW, btnH }, btns[i].col))
            result = i + 1;
        btnY += btnH + btnGap;
    }

    return result;
}

// ── Game Over ─────────────────────────────────────────────────────────────────
bool PauseAndGameOver::DrawGameOver(int wave, float gameTimer)
{
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();

    ClearBackground(BLACK);

    // "GAME OVER" title — centered horizontally, upper quarter
    const char* title  = "GAME OVER";
    int titleSz        = (int)(sw * 0.07f);   // scales with width
    int titleW         = MeasureText(title, titleSz);
    DrawText(title, (int)(sw / 2.f - titleW / 2.f), (int)(sh * 0.22f), titleSz, RED);

    // Stats block — centered
    int statSz = (int)(sh * 0.032f);
    const char* waveTxt = TextFormat("Wave Reached: %d", wave);
    const char* timeTxt = TextFormat("Time Survived: %.1f s", gameTimer);
    int waveW = MeasureText(waveTxt, statSz);
    int timeW = MeasureText(timeTxt, statSz);

    float statsY = sh * 0.50f;
    DrawText(waveTxt, (int)(sw / 2.f - waveW / 2.f), (int)statsY, statSz, YELLOW);
    DrawText(timeTxt, (int)(sw / 2.f - timeW / 2.f), (int)(statsY + statSz + 12.f), statSz, YELLOW);

    // Play-again prompt — centered, lower quarter
    int subSz      = (int)(sh * 0.026f);
    const char* sub = "Press ENTER to play again";
    int subW       = MeasureText(sub, subSz);
    DrawText(sub, (int)(sw / 2.f - subW / 2.f), (int)(sh * 0.75f), subSz, RAYWHITE);

    // Quit button — bottom-right corner
    float btnW = sw * 0.115f;
    float btnH = sh * 0.055f;
    float btnX = sw - btnW - sw * 0.016f;
    float btnY = sh - btnH - sh * 0.028f;

    if (DrawButton("Quit Game", { btnX, btnY, btnW, btnH }, RED))
        return true;

    return false;
}
