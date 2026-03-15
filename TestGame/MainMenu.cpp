#include "MainMenu.h"

void MainMenu::Init()
{
    _buttons.clear();

    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();

    float buttonWidth  = sw * 0.20f;     // ~384 px at 1920
    float buttonHeight = sh * 0.083f;    // ~90 px at 1080
    float gap          = sh * 0.018f;    // ~20 px at 1080

    float startX = sw / 2.f - buttonWidth / 2.f;
    float firstY = sh * 0.42f;           // first button at 42% down

    _buttons.push_back({ "Start Game",   { startX, firstY,                         buttonWidth, buttonHeight } });
    _buttons.push_back({ "How To Play",  { startX, firstY + (buttonHeight + gap),   buttonWidth, buttonHeight } });
    _buttons.push_back({ "Quit",         { startX, firstY + (buttonHeight + gap)*2, buttonWidth, buttonHeight } });

    _startPressed  = false;
    _quitPressed   = false;
    _howToPressed  = false;
}

void MainMenu::Update()
{
    Vector2 mouse = GetMousePosition();

    for (auto& button : _buttons)
    {
        button.hovered = CheckCollisionPointRec(mouse, button.bounds);

        if (button.hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            if (button.text == "Start Game")  _startPressed = true;
            if (button.text == "Quit")        _quitPressed  = true;
            if (button.text == "How To Play") _howToPressed = true;
        }
    }
}

void MainMenu::Draw()
{
    // background
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), DARKBROWN);

    // title banner
    int titleSz = GetScreenHeight() / 12;   // ~90 px at 1080
    DrawBanner("Dungeon Colosseum", GetScreenHeight() / 6, titleSz, BLACK, WHITE);

    for (auto& button : _buttons)
    {
        Color color;

        if (button.text == "Start Game")
            color = GREEN;
        else if (button.text == "How To Play")
            color = BLUE;
        else
            color = RED;

        if (button.hovered)
            color = Fade(color, 0.85f);

        DrawRectangleRec(button.bounds, color);
        DrawRectangleLinesEx(button.bounds, 3, BLACK);

        int fontSize = 40;
        int textWidth = MeasureText(button.text.c_str(), fontSize);

        DrawText(button.text.c_str(),
            button.bounds.x + button.bounds.width  / 2 - textWidth / 2,
            button.bounds.y + button.bounds.height / 2 - fontSize  / 2,
            fontSize, WHITE);
    }
}

void MainMenu::DrawBanner(const char* text, int y, int fontSize, Color bannerColor, Color textColor)
{
    int screenWidth = GetScreenWidth();

    int textWidth = MeasureText(text, fontSize);

    int padding = 30;
    int bannerHeight = fontSize + padding;

    // shadow
    DrawRectangle(0, y + 5, screenWidth,
        bannerHeight, Fade(BLACK, 0.4f) );

    // banner background
    DrawRectangle( 0, y,
        screenWidth, bannerHeight, bannerColor);

    // text
    DrawText( text,screenWidth / 2 - textWidth / 2,
        y + padding / 2, fontSize,textColor );
}


bool MainMenu::StartPressed() const
{
    return _startPressed;
}

bool MainMenu::QuitPressed() const
{
    return _quitPressed;
}

bool MainMenu::HowToPressed() const
{
    return _howToPressed;
}