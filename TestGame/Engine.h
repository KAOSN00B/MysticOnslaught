
#pragma once

#include "raylib.h"
#include "KeyBindings.h"
#include "Character.h"
#include "Prop.h"
#include "Enemy.h"
#include "Cyclops.h"
#include "Ogre.h"
#include "Molarbeast.h"
#include "MainMenu.h"
#include "PauseAndGameOver.h"
#include "Pickup.h"
// [SHELVED] FireBallPickup.h / SwordBeamPickup.h / FreezePickup.h
//   These ammo-pickup classes are no longer spawned. Engine used to hand out
//   ability ammo via pickups; that system was replaced by the mana economy.
//   The .cpp files are still in the project so they compile, but Engine has no
//   dependency on them. Remove from .vcxproj when confirmed safe to delete.
#include "HealPickup.h"
#include "ManaGemPickup.h"
#include "SpreadProjectile.h"
#include "CyclopsLaserProjectile.h"
#include "LavaBallProjectile.h"
#include "Leaderboard.h"

#include <vector>
#include <string>
#include <memory>
#include <future>
#include <chrono>

enum class Biome { Dungeon, Forest };

enum class GameState
{
    Menu,
    Play,
    GameOver,
    Pause,
    HowToPlay,
    Leaderboard,
    Keybindings,
    LevelUpChoice,
    AbilityChoice
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
    void SpawnSpreadBurst(AbilityType element);
    void SpawnBolt(AbilityType element);
    void SpawnUltimateBurst(AbilityType element);
    void UpdateUltimateBlasts(float dt);
    void DrawUltimateBlasts(Vector2 worldOffset);

    // Cinematic ultimate sequence
    void TriggerUltimateSequence(AbilityType element);
    void UpdateUltimateSequence(float dt);
    void DrawUltimateSequence();
    void ApplyUltimateImpact();
    void GenerateStartingAbilityOptions();
    void UpdateSpreadProjectiles(float dt);
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
    void SpawnFloatingText(Vector2 worldPos, int value, Color color);
    void SpawnEnemyDrop(Vector2 worldPos);
    void SpawnTimedPickup();
    void SpawnBossSupportAdds();
    void UpdateBossSupportRespawns(float dt);
    void ClearBossSupportAdds();
    Biome GetBiomeForWave(int wave) const;
    const char* GetBiomeName(Biome biome) const;
    void ApplyBiome(Biome biome);
    void PopulatePropsForBiome(Biome biome);
    void UpdateBiomeTransition(float dt);
    float GetBiomeTransitionAlpha() const;
    void DrawMiniMap();
    void DrawLevelUpChoice();
    void GenerateLevelUpOptions();
    void DrawAbilityChoice();
    void GenerateAbilityChoiceOptions();
    void ResetRunState();
    void SaveKeybindings();
    void LoadKeybindings();
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

    struct FloatingText
    {
        Vector2 worldPos{};
        int     value     = 0;
        Color   color     = WHITE;
        float   spawnTime = 0.f;
        static constexpr float kLifetime = 0.75f;
    };

    GameState _gameState = GameState::Menu;

    bool _shouldExit = false;
    GameState _howToPlayFrom = GameState::Menu;
    bool _waveStarting = true;
    bool _wave1LevelUpDone = false; // ensures forced level-up after wave 1 only fires once
    bool _playerDying = false;
    bool _shouldClose = false;
    bool _awaitingNameEntry = false;
    bool _awaitingStartingAbility = false;

    int _wave        = 0;
    int _enemiesKilled = 0;

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

    float _gameOverTimer  = 0.f;
    float _gameOverDelay  = 2.f;
    float _fadeInTimer   = 0.f;
    float _bossWarningTimer = 0.f;
    float _levelUpOpenTimer = 0.f;  // blocks card clicks briefly after panel opens

    // Level-up choice state
    UpgradeType _levelUpOptions[3] = { UpgradeType::AttackPower, UpgradeType::AttackRange, UpgradeType::MaxHealth };
    UpgradeType _levelUpUltimateOptions[3] = { UpgradeType::LearnFireUltimate, UpgradeType::LearnIceUltimate, UpgradeType::LearnElectricUltimate };
    GameState   _levelUpReturnState  = GameState::Play;
    bool        _showUltimateRow     = false;
    bool        _ultimateRowPicked   = false;
    bool        _regularRowPicked    = false;
    // Ability choice state (shown after every 5th-wave boss clear)
    UpgradeType _abilityChoiceOptions[3] = { UpgradeType::LearnFireSpread, UpgradeType::LearnFireSpread, UpgradeType::LearnFireSpread };
    int         _abilityChoiceOptionCount = 0;
    bool        _abilityChoiceSwapPending = false;
    UpgradeType _abilityChoiceSwapTarget  = UpgradeType::AttackPower;
    int         _lastAbilityChoiceWave    = -1;
    float       _abilityChoiceOpenTimer   = 0.f;

    // Upgrade icon textures (loaded once, never reloaded)
    Texture2D _upgradeAttackPowerTex{};
    Texture2D _upgradeAttackRangeTex{};
    Texture2D _upgradeHealthTex{};
    Texture2D _upgradeMagicTex{};
    Texture2D _upgradeDefenseTex{};
    Texture2D _upgradeMoveSpeedTex{};

    Sound _pickupSound{};
    Sound _fireballCastSound{};
    Sound _explosionSound{};
    Sound _lavaBallImpactSound{};
    Sound _buttonPressSound{};

    Texture2D _map{};
    Texture2D _treeTex{};
    Texture2D _smallTreeTex{};
    Texture2D _rockTex{};
    Texture2D _bigRockTex{};
    Vector2 _mapPos{};
    float _mapScale = 3.f;
    float _navCellSize = 72.f;
    int _navCols = 0;
    int _navRows = 0;
    int _maxActiveEnemies = 16;
    int _lastPlayerNavIndex = -1;
    bool _navRefreshInFlight = false;

    Texture2D _pillarTex{};
    Texture2D _torchTex{};       // Torch.png — 256x29, 8 frames of 32x29
    Texture2D _pillarTorchTex{}; // PillarTorch.png — 290x52, 8 frames of 32x52 (content offset x=17)
    Texture2D _fireballCastTex{};
    Texture2D _fireballHitTex{};
    Texture2D _genericHitTex{};   // Hit03.png — melee hit splat + electric impact sprite
    Texture2D _iceHitTex{};       // Ice_Shard_Hit.png — ice ability impact
    Texture2D _lightningCastTex{};
    Texture2D _healEffectTex{};

    // Ability card icons for the level-up / starting ability panels
    Texture2D _abilityIconFireTex{};
    Texture2D _abilityIconIceTex{};
    Texture2D _abilityIconElectricTex{};

    Character _player;
    MainMenu _menu;
    PauseAndGameOver _pauseUI;
    KeyBindings _keybindingsEdit;

    std::vector<Prop> _props;
    std::vector<std::unique_ptr<Pickup>> _pickups;
    enum class UltimatePhase { None, WindUp, Cinematic, Impact, Release };

    struct UltimateBlast
    {
        Vector2     worldPos{};
        AbilityType element  = AbilityType::FireUltimate;
        float       timer    = 0.f;
        float       lifetime = 0.f;
        float       rotation = 0.f;   // initial rotation angle (degrees)
    };

    static constexpr float _ultWindUpDuration    = 0.75f;
    static constexpr float _ultCinematicDuration = 1.5f;
    static constexpr float _ultImpactDuration    = 0.35f;
    static constexpr float _ultReleaseDuration   = 0.5f;

    UltimatePhase _ultimatePhase       = UltimatePhase::None;
    float         _ultimatePhaseTimer  = 0.f;
    float         _ultimateCircleAngle = 0.f;
    AbilityType   _ultimateElement     = AbilityType::FireUltimate;

    std::vector<UltimateBlast>       _ultimateBlasts;
    std::vector<SpreadProjectile>    _spreadProjectiles;
    std::vector<AnimatedEffect> _effects;
    std::vector<FloatingText>   _floatingTexts;
    std::vector<bool> _navBlocked;
    std::vector<int>  _navDistance;
    std::future<NavigationRefreshResult> _navRefreshJob;
    std::vector<std::unique_ptr<Enemy>>   _enemies;
    std::vector<CyclopsLaserProjectile>   _cyclopsLasers;
    std::vector<LavaBallProjectile>       _lavaBalls;
    BossSupportState _bossCyclopsSupport;
    BossSupportState _bossOgreSupport;

    std::string _message = "Objective: Survive";
    Biome _currentBiome = Biome::Dungeon;
    Biome _pendingBiome = Biome::Dungeon;
    bool  _biomeTransitionActive = false;
    bool  _biomeTransitionSwapped = false;
    float _biomeTransitionTimer = 0.f;

    Leaderboard _leaderboard;
};
