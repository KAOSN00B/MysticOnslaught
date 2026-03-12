#include "MenuSystem.h"

void MenuSystem::Init()
{
    _buttons.clear();

    int centerX = GetScreenWidth() / 2 - 100;

    _buttons.push_back({ "Start Game", { (float)centerX, 320.f, 200.f, 60.f } });
    _buttons.push_back({ "Quit", { (float)centerX, 400.f, 200.f, 60.f } });

    _startPressed = false;
    _quitPressed = false;
}

void MenuSystem::Update()
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

void MenuSystem::Draw()
{
    // draw background
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), DARKBLUE);

    const char* title = "TOP DOWN SURVIVAL";

    int titleSize = 60;
    int titleWidth = MeasureText(title, titleSize);

    DrawText(
        title,
        GetScreenWidth() / 2 - titleWidth / 2,
        150,
        titleSize,
        WHITE
    );

    for (auto& button : _buttons)
    {
        Color color = button.hovered ? YELLOW : WHITE;

        DrawRectangleLinesEx(button.bounds, 3, color);

        int fontSize = 30;
        int textWidth = MeasureText(button.text.c_str(), fontSize);

        DrawText(
            button.text.c_str(),
            button.bounds.x + button.bounds.width / 2 - textWidth / 2,
            button.bounds.y + 18,
            fontSize,
            color
        );
    }
}

bool MenuSystem::StartPressed() const
{
    return _startPressed;
}

bool MenuSystem::QuitPressed() const
{
    return _quitPressed;
}