
#pragma once

#include "raylib.h"
#include "Character.h"
#include "Prop.h"
#include "Enemy.h"
#include "MainMenu.h"
#include "PauseAndGameOver.h"

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
    void SpawnEnemies();
    void HandleCollisions();
    void UpdateEnemyCount(float dt);
    void TriggerScreenShake(float strength, float duration);
    void DrawWorld();
    void DrawHUD();
    void DrawWaveIntro();
    
    Vector2 GetRandomPropPosition();
    bool IsSpawnPositionValid(Vector2 pos);

private:

    GameState _gameState = GameState::Menu;

    bool _shouldExit = false;
    bool _waveStarting = true;
    bool _playerDying = false;
    bool _shouldClose = false;

    int _wave = 0;

    float _shakeTimer = 0.f;
    float _shakeStrength = 0.f;
    float _gameTimer = 0.f;

    Vector2 _shakeOffset = { 0.f, 0.f };

    const int _windowWidth = 1200;
    const int _windowHeight = 800;

    float _waveIntroTimer = 0.f;
    float _waveIntroDuration = 2.5f;

    float _gameOverTimer = 0.f;
    float _gameOverDelay = 2.f;

    Texture2D _map{};
    Vector2 _mapPos{};
    float _mapScale = 5.5f;

    Texture2D _pillarTex{};

    Character _player;
    MainMenu _menu;
    PauseAndGameOver _pauseUI;

    std::vector<Prop> _props;

    std::vector<Rectangle> _collisionRects;
    std::vector<std::unique_ptr<Enemy>> _enemies;

    std::string _message = "Objective: Survive";
};
