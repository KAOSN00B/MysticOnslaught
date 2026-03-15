//#pragma once

#include "raylib.h"

class PauseAndGameOver
{
public:

    int DrawPause();   // 0=nothing  1=resume  2=howtoplay  3=quit
    bool DrawGameOver(int wave, float gameTimer);

};