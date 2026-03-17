
#pragma once

#include "raylib.h"
#include "Character.h"
#include "Prop.h"
#include "Enemy.h"
#include "Cyclops.h"
#include "Ogre.h"
#include "Molarbeast.h"
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
#include "CyclopsLaserProjectile.h"
#include "LavaBallProjectile.h"

#include <vector>
#include <string>
#include <memory>
#include <future>
#include <chrono>

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
    struct NavigationRefreshResult
    {
        // Fully computed distance field for the current player nav cell.
        std::vector<int> distanceField;

        // Open-cell index the worker used as the player's navigation target.
        // The main thread applies this so it knows whether the player's cell changed.
        int playerNavIndex = -1;
    };

    Engine();
    ~Engine();

    void Run();
    void RunFrame();

private:

    void Init();

    void Update(float dt);
    void Draw();

    void UpdateGamePlay(float dt);
    void SpawnWave();
    void SpawnEnemies();
    void HandleCollisions();
    void UpdateEnemyCount(float dt);
    Enemy* SpawnCyclops(Vector2 pos);
    Enemy* SpawnOgre(Vector2 pos);
    void SpawnMolarbeast(Vector2 pos);
    void UpdateCyclopsLasers(float dt);
    void UpdateLavaBallProjectiles(float dt);
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
    Vector2 GetAStarTarget(Vector2 startWorldPos, Vector2 targetWorldPos) const;
    bool FindNearestOpenCell(int& col, int& row) const;
    bool HasLineOfSight(Vector2 start, Vector2 end) const;
    void RefreshNavigationField();
    void ApplyCompletedNavigationRefresh();
    int GetClosestOpenNavigationIndex(int col, int row) const;
    void SpawnEnemyDrop(Vector2 worldPos);
    void SpawnTimedPickup();
    void SpawnBossSupportAdds();
    void UpdateBossSupportRespawns(float dt);
    void ClearBossSupportAdds();
    void DrawMiniMap();
    void ResetRunState();
    int GetActiveEnemyCount() const;
    bool IsBossFightActive() const;
    bool TryGetPooledEnemySpawn(Vector2 pos);
    bool TryGetPooledCyclopsSpawn(Vector2 pos);
    bool TryGetPooledOgreSpawn(Vector2 pos);
    bool TryGetPooledMolarbeastSpawn(Vector2 pos);
    int GetCyclopsSpawnCountForWave(int wave) const;
    int GetOgreSpawnCountForWave(int wave) const;
    int GetEnemyPowerLevelForWave(int wave) const;
    void ConfigureSpawnedEnemy(Enemy& enemy);

    Vector2 GetRandomPropPosition();
    bool IsSpawnPositionValid(Vector2 pos);
    void RespawnOutOfBoundsEnemies();
    bool TryGetFarSpawnPosition(Vector2& pos, float minPlayerDistance);

private:
    struct BossSupportState
    {
        Enemy* enemy = nullptr;
        float respawnTimer = 0.f;
    };

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
    float _bossWarningTimer = 0.f;

    Sound _pickupSound{};
    Sound _fireballCastSound{};
    Sound _explosionSound{};
    Sound _lavaBallImpactSound{};
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
    bool _navRefreshInFlight = false;

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
    std::future<NavigationRefreshResult> _navRefreshJob;
    std::vector<std::unique_ptr<Enemy>>   _enemies;
    std::vector<CyclopsLaserProjectile>   _cyclopsLasers;
    std::vector<LavaBallProjectile>       _lavaBalls;
    BossSupportState _bossCyclopsSupport;
    BossSupportState _bossOgreSupport;

    std::string _message = "Objective: Survive";
};
