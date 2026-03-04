#pragma once

#include "raylib.h"
#include "Character.h"
#include "Prop.h"
#include "Enemy.h"
#include <vector>

class Engine
{
public:
    Engine();
    ~Engine();
    void Run();

private:

    void Init();
    void Update(float dt);
    void Draw();
    void HandleCollisions();

    const int _windowWidth = 1200;
    const int _windowHeight = 800;

    Texture2D _map;
    Vector2 _mapPos{};
    float _mapScale = 5.5f;

    Character _hero;

    Texture2D _pillarTex;
    Prop _props[2];

    Enemy _goblin;
    std::vector<Rectangle> _collisionRects;
};