#pragma once
#include "raylib.h"

class Game
{
public:
    Game();
    void Run();

private:
    void Update(float dt);
    void Draw();

private:
    bool _isRunning;
};