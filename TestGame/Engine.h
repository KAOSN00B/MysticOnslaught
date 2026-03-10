#pragma once

#include "raylib.h"
#include "Character.h"
#include "Prop.h"
#include "Enemy.h"
#include <vector>
#include <string>

class Engine
{
public:
    Engine();
    ~Engine();
    void Run();

private:

    void Init();
    void SpawnWave();
    void Update(float dt);
    void Draw();
    void HandleCollisions();
    void UpdateEnemyCount(float dt);
    

    int _enemyCountMultiplyer = 1;
    int _wave = 0;

    const int _windowWidth = 1200;
    const int _windowHeight = 800;

    Texture2D _map;
    Vector2 _mapPos{};
    float _mapScale = 5.5f;

    Character _hero;

    Texture2D _pillarTex;
    Prop _props[2];

    std::vector<Rectangle> _collisionRects;
    std::vector<Enemy> _enemies;
    //std::vector<Prop> _props;
    std::string _message = "Objective: Surivive";

};