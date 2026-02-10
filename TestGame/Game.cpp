#include "Game.h"

Game::Game()
{
    InitWindow(800, 450, "Tiny Raylib Game");
    SetTargetFPS(60);

    _isRunning = true;
}

void Game::Run()
{
    while (_isRunning && !WindowShouldClose())
    {
        float dt = GetFrameTime();

        Update(dt);

        BeginDrawing();
        ClearBackground(RAYWHITE);
        Draw();
        EndDrawing();
    }

    CloseWindow();
}

void Game::Update(float dt)
{
    
}

void Game::Draw()
{
    DrawText("Game loop is working", 500, 500, 20, DARKGRAY);
}
