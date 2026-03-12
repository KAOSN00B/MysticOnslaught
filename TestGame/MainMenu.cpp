#include "MainMenu.h"

void MainMenu::Init()
{
    _buttons.clear();

    float buttonWidth = 350.f;
    float buttonHeight = 90.f;

    int centerX = GetScreenWidth() / 2 - buttonWidth / 2;

    _buttons.push_back({ "Start Game", { (float)centerX, 350.f, buttonWidth, buttonHeight } });
    _buttons.push_back({ "Quit", { (float)centerX, 470.f, buttonWidth, buttonHeight } });

    _startPressed = false;
    _quitPressed = false;
}

void MainMenu::Update()
{
    Vector2 mouse = GetMousePosition();

    for (auto& button : _buttons)
    {
        button.hovered = CheckCollisionPointRec(mouse, button.bounds);

        if (button.hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            if (button.text == "Start Game")
                _startPressed = true;

            if (button.text == "Quit")
                _quitPressed = true;
        }
    }
}

void MainMenu::Draw()
{
    // background
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), DARKBROWN);

    // title banner
    DrawBanner("Dungeon Colosseum", 120, 80, BLACK, WHITE);

    for (auto& button : _buttons)
    {
        Color color;

        if (button.text == "Start Game")
            color = GREEN;
        else
            color = RED;

        // hover effect
        if (button.hovered)
            color = Fade(color, 0.85f);

        // filled button
        DrawRectangleRec(button.bounds, color);

        // button outline
        DrawRectangleLinesEx(button.bounds, 3, BLACK);

        int fontSize = 40;
        int textWidth = MeasureText(button.text.c_str(), fontSize);

        DrawText( button.text.c_str(), button.bounds.x + button.bounds.width / 2 - textWidth / 2,
            button.bounds.y + button.bounds.height / 2 - fontSize / 2,  fontSize, WHITE);
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