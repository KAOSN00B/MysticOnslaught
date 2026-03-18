#pragma once

#include "raylib.h"
#include <vector>
#include <string>

class MainMenu
{
public:

    ~MainMenu();

    void Init();
    void Update();
    void Draw();

    bool StartPressed()       const;
    bool QuitPressed()        const;
    bool HowToPressed()       const;
    bool LeaderboardPressed() const;

private:

    struct Button
    {
        std::string text;
        Rectangle bounds;
        bool hovered = false;
    };
    std::vector<Button> _buttons;

    Texture2D _borderTex{};
    Texture2D _bannerTex{};
    Texture2D _playBtnTex{};
    Texture2D _htpBtnTex{};

    bool _startPressed       = false;
    bool _quitPressed        = false;
    bool _howToPressed       = false;
    bool _leaderboardPressed = false;
};