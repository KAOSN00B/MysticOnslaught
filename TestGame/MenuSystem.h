#pragma once

#include "raylib.h"
#include <vector>
#include <string>

class MenuSystem
{
public:

    void Init();
    void Update();
    void Draw();

    bool StartPressed() const;
    bool QuitPressed() const;

private:

    struct Button
    {
        std::string text;
        Rectangle bounds;
        bool hovered = false;
    };

    std::vector<Button> _buttons;

    bool _startPressed = false;
    bool _quitPressed = false;
};