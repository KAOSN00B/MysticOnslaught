#pragma once

#include "raylib.h"
#include "Character.h"
#include "Prop.h"
#include "Enemy.h"
#include "MenuSystem.h"

#include <vector>
#include <string>
#include <memory>

enum class GameState
{
    Menu,
    Play,
    GameOver,
    Pause
};

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

    void UpdateGamePlay(float dt);
    void SpawnWave();
    void HandleCollisions();
    void UpdateEnemyCount(float dt);
    void TriggerScreenShake(float strength, float duration);

private:

    GameState _gameState = GameState::Menu;

    bool _shouldExit = false;

    int _wave = 0;

    float _shakeTimer = 0.f;
    float _shakeStrength = 0.f;

    Vector2 _shakeOffset = { 0.f, 0.f };

    const int _windowWidth = 1200;
    const int _windowHeight = 800;

    Texture2D _map{};
    Vector2 _mapPos{};
    float _mapScale = 5.5f;

    Texture2D _pillarTex{};

    Character _player;
    MenuSystem _menu;

    Prop _props[2];

    std::vector<Rectangle> _collisionRects;
    std::vector<std::unique_ptr<Enemy>> _enemies;

    std::string _message = "Objective: Survive";
};