#pragma once

#include "raylib.h"

class PauseAndGameOver
{
public:

    ~PauseAndGameOver();

    void Init();
    void Unload();

    int  DrawPause();      // 0=nothing  1=resume  2=howtoplay  3=quit
    int  DrawGameOver(int wave, float gameTimer);  // 0=nothing  1=play again  2=main menu  3=quit

private:

    Texture2D _borderTex{};
    Texture2D _btnTex{};
    Texture2D _htpBtnTex{};
};