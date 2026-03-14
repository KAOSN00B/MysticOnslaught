#pragma once

#include "raylib.h"
#include <vector>
#include <string>

class MainMenu
{
public:

    void Init();
    void Update();
    void Draw();
    void DrawBanner(const char* text, int y, int fontSize,
        Color bannerColor, Color textColor);

    bool StartPressed()   const;
    bool QuitPressed()    const;
    bool HowToPressed()   const;



private:

    struct Button
    {
        std::string text;
        Rectangle bounds;
        bool hovered = false;
    };
    std::vector<Button> _buttons;

    bool _startPressed  = false;
    bool _quitPressed   = false;
    bool _howToPressed  = false;
};