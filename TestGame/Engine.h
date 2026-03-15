
#pragma once

#include "raylib.h"
#include "Character.h"
#include "Prop.h"
#include "Enemy.h"
#include "MainMenu.h"
#include "PauseAndGameOver.h"
#include "Pickup.h"
#include "FireBallPickup.h"
#include "SwordBeamPickup.h"
#include "FreezePickup.h"
#include "HealPickup.h"
#include "FireballProjectile.h"
#include "SwordBeamProjectile.h"
#include "FreezeProjectile.h"

#include <vector>
#include <string>
#include <memory>

enum class GameState
{
    Menu,
    Play,
    GameOver,
    Pause,
    HowToPlay
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
    void DrawHowToPlay();
    void DrawAbilityBar();   // unified 1-2-3-4 slot HUD
    void DrawWaveIntro();
    void HandlePlayerMeleeDamage();
    void SpawnFireballBurst();
    void SpawnSwordBeam();
    void SpawnFreezeWave();
    void UpdateFireballProjectiles(float dt);
    void UpdateSwordBeamProjectiles(float dt);
    void UpdateFreezeProjectiles(float dt);
    void UpdateEffects(float dt);
    void DrawEffects(Vector2 worldOffset);
    void SpawnCastEffect(Character::CastType castType);
    void SpawnHitEffect(Character::CastType castType, Vector2 worldPos, Vector2 direction);
    void SpawnHealEffect();
    void BuildNavigationGrid();
    bool IsNavigationCellBlocked(int col, int row) const;
    int GetNavigationIndex(int col, int row) const;
    Vector2 GetNavigationTarget(Vector2 startWorldPos, Vector2 targetWorldPos) const;
    bool FindNearestOpenCell(int& col, int& row) const;
    bool HasLineOfSight(Vector2 start, Vector2 end) const;
    void RefreshNavigationField();
    int GetClosestOpenNavigationIndex(int col, int row) const;
    void SpawnEnemyDrop(Vector2 worldPos);
    void SpawnTimedPickup();
    void DrawMiniMap();
    void ResetRunState();
    int GetActiveEnemyCount() const;
    bool TryGetPooledEnemySpawn(Vector2 pos);

    Vector2 GetRandomPropPosition();
    bool IsSpawnPositionValid(Vector2 pos);
    void RespawnOutOfBoundsEnemies();

private:

    struct AnimatedEffect
    {
        Texture2D* texture = nullptr;
        Vector2 worldPos{};
        Vector2 offset{};
        Vector2 direction{ 1.f, 0.f };
        Color tint = WHITE;
        float scale = 4.f;
        float frameTime = 1.f / 18.f;
        float runningTime = 0.f;
        int frameWidth = 32;
        int frameHeight = 32;
        int frameCount = 1;
        int frame = 0;
        bool followPlayer = false;
        bool followPlayerCenter = false;
        bool active = false;
    };

    GameState _gameState = GameState::Menu;

    bool _shouldExit = false;
    GameState _howToPlayFrom = GameState::Menu;
    bool _waveStarting = true;
    bool _playerDying = false;
    bool _shouldClose = false;

    int _wave = 0;

    float _shakeTimer = 0.f;
    float _shakeStrength = 0.f;
    float _gameTimer = 0.f;
    float _pickupSpawnTimer = 0.f;

    Vector2 _shakeOffset = { 0.f, 0.f };
    Vector2 _cameraPos   = { 0.f, 0.f };

    const int _windowWidth = 1920;
    const int _windowHeight = 1080;

    float _waveIntroTimer = 0.f;
    float _waveIntroDuration = 2.5f;
    float _navRefreshTimer = 0.f;
    float _navRefreshInterval = 0.2f;

    float _gameOverTimer = 0.f;
    float _gameOverDelay = 2.f;
    float _fadeInTimer   = 0.f;

    Sound _pickupSound{};
    Sound _fireballCastSound{};
    Sound _explosionSound{};
    Sound _bladeBeamSound{};
    Sound _buttonPressSound{};

    Texture2D _map{};
    Vector2 _mapPos{};
    float _mapScale = 3.f;
    float _navCellSize = 72.f;
    int _navCols = 0;
    int _navRows = 0;
    int _maxActiveEnemies = 16;
    int _lastPlayerNavIndex = -1;

    Texture2D _pillarTex{};
    Texture2D _fireballCastTex{};
    Texture2D _fireballHitTex{};
    Texture2D _swordBeamCastTex{};
    Texture2D _swordBeamHitTex{};
    Texture2D _freezeCastTex{};
    Texture2D _freezeHitTex{};
    Texture2D _healEffectTex{};

    Character _player;
    MainMenu _menu;
    PauseAndGameOver _pauseUI;

    std::vector<Prop> _props;
    std::vector<std::unique_ptr<Pickup>> _pickups;
    std::vector<FireballProjectile>  _fireballProjectiles;
    std::vector<SwordBeamProjectile> _swordBeamProjectiles;
    std::vector<FreezeProjectile>    _freezeProjectiles;
    std::vector<AnimatedEffect> _effects;
    std::vector<bool> _navBlocked;
    std::vector<int>  _navDistance;
    std::vector<std::unique_ptr<Enemy>> _enemies;

    std::string _message = "Objective: Survive";
};
