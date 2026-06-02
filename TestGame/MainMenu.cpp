#include "MainMenu.h"
#include "AssetPaths.h"
#include "NineSlice.h"
#include <cmath>

// ── 9-slice corner sizes ──────────────────────────────────────────────────────
// srcCorner: how many pixels from each edge of the texture form a corner.
// dstCorner: how large that corner appears on screen.
// Tune these to match your art (open the PNG and measure the corner detail).
static constexpr float BORDER_SRC_CORNER = 16.f;   // px in source texture
static constexpr float BORDER_DST_CORNER = 32.f;   // px on screen
static constexpr float BTN_SRC_CORNER    = 8.f;
static constexpr float BTN_DST_CORNER    = 12.f;

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

    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();

    float buttonWidth  = sw * 0.20f;
    float buttonHeight = sh * 0.083f;
    float gap          = sh * 0.018f;

    float startX = sw / 2.f - buttonWidth / 2.f;
    float firstY = sh * 0.47f;

    _buttons.push_back({ "Start Game",  { startX, firstY,                       buttonWidth, buttonHeight } });
    _buttons.push_back({ "How To Play", { startX, firstY + (buttonHeight + gap), buttonWidth, buttonHeight } });
    _buttons.push_back({ "Quit",        { startX, firstY + (buttonHeight + gap)*2, buttonWidth, buttonHeight } });
    _buttons.push_back({ "Debug Mode",  { sw * 0.71f, sh * 0.54f, buttonWidth * 0.90f, buttonHeight * 0.95f } });

    // Corner buttons (smaller)
    float lbW = sw * 0.14f;
    float lbH = sh * 0.055f;
    float cornerPad = sw * 0.018f;

    _startPressed       = false;
    _quitPressed        = false;
    _howToPressed       = false;
    _debugPressed       = false;

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
    Vector2 mouse = GetMousePosition();

    for (auto& button : _buttons)
    {
        if (button.text == "Debug Mode" && !_debugUnlocked)
            continue;

        button.hovered = CheckCollisionPointRec(mouse, button.bounds);

        if (button.hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            if (button.text == "Start Game")   _startPressed       = true;
            if (button.text == "Quit")         _quitPressed        = true;
            if (button.text == "How To Play")  _howToPressed       = true;
            if (button.text == "Debug Mode")   _debugPressed       = true;
        }
    }
}

void MainMenu::Draw()
{
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();

    // ── Animated checkerboard background ────────────────────────────────────
    {
        const int   cell   = 80;
        const Color dark   = Color{ 52, 38, 26, 255 };
        const Color light  = Color{ 72, 54, 36, 255 };
        const int   period = cell * 2;   // full colour cycle = 2 cells wide

        float t      = (float)GetTime();
        int   offX   = (int)fmodf(t * 22.f, (float)period);
        int   offY   = (int)fmodf(t * 12.f, (float)period);
        int   phaseX = offX / cell;       // 0 or 1 — which colour phase
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

    // ── Panel border around the button area ──────────────────────────────────
    float borderW = sw * 0.40f;
    float borderH = sh * 0.53f;
    float borderX = sw / 2.f - borderW / 2.f;
    float borderY = sh * 0.35f;

    if (_borderTex.id != 0)
        DrawNineSlice(_borderTex, BORDER_SRC_CORNER, BORDER_DST_CORNER,
            { borderX, borderY, borderW, borderH }, WHITE);

    // ── Title banner ─────────────────────────────────────────────────────────
    float bannerW = sw * 0.42f;
    float bannerH = (_bannerTex.id != 0)
        ? bannerW * ((float)_bannerTex.height / (float)_bannerTex.width)
        : sh * 0.12f;
    float bannerX = sw / 2.f - bannerW / 2.f;
    float bannerY = sh * 0.10f;

    if (_bannerTex.id != 0)
        DrawTexturePro(_bannerTex,
            { 0.f, 0.f, (float)_bannerTex.width, (float)_bannerTex.height },
            { bannerX, bannerY, bannerW, bannerH },
            {}, 0.f, WHITE);

    // Title text — centred in the flat body of the banner, with black outline
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

    // ── Buttons ──────────────────────────────────────────────────────────────
    for (auto& button : _buttons)
    {
        if (button.text == "Debug Mode" && !_debugUnlocked)
            continue;

        if (button.text == "Debug Mode")
        {
            Color fill = button.hovered ? Color{ 180, 105, 55, 245 } : Color{ 126, 74, 38, 225 };
            Color edge = button.hovered ? Color{ 255, 205, 135, 255 } : Color{ 220, 168, 105, 215 };
            DrawRectangleRounded(button.bounds, 0.24f, 8, fill);
            DrawRectangleRoundedLines(button.bounds, 0.24f, 8, edge);
            int fs = (int)(sh * 0.032f);
            int tw = MeasureText(button.text.c_str(), fs);
            DrawText(button.text.c_str(),
                (int)(button.bounds.x + button.bounds.width  / 2 - tw / 2),
                (int)(button.bounds.y + button.bounds.height / 2 - fs / 2),
                fs, Color{ 255, 244, 220, 255 });
            continue;
        }

        Texture2D* tex  = &_playBtnTex;
        Color      tint = WHITE;

        if (button.text == "How To Play")
            tex = &_htpBtnTex;
        else if (button.text == "Quit")
            tint = Color{ 230, 80, 80, 255 };

        if (button.hovered)
            tint = Fade(tint, 0.78f);

        if (tex->id != 0)
            DrawNineSlice(*tex, BTN_SRC_CORNER, BTN_DST_CORNER,
                button.bounds, tint);
        else
            DrawRectangleRec(button.bounds, Fade(GRAY, 0.7f));

        int fontSize  = (int)(sh * 0.036f);
        int textWidth = MeasureText(button.text.c_str(), fontSize);
        DrawText(button.text.c_str(),
            (int)(button.bounds.x + button.bounds.width  / 2 - textWidth / 2),
            (int)(button.bounds.y + button.bounds.height / 2 - fontSize  / 2),
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
