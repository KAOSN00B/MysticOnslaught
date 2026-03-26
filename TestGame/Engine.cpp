#include "Engine.h"

#include "AnimationUtils.h"
#include "AssetPaths.h"
#include "raymath.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <queue>

namespace
{
    constexpr float kDefaultTimedPickupInterval = 60.f;

    // Boss fights need a much denser combat-pickup cadence right now because
    // the boss only takes light chip damage from specials in the current demo
    // tuning pass.
    constexpr float kBossTimedPickupInterval = 2.f;

    // Spawn protection gives the player time to reposition if a wave enters on
    // top of them or very near the player after pathing/spawn validation.
    constexpr float kWaveSpawnProtectionDuration = 2.f;

    // Boss support adds keep the fight active, but they should respawn far
    // enough away that the player gets a real reposition window before the
    // next Ogre/Cyclops pressure cycle starts.
    constexpr float kBossSupportRespawnDelay = 20.f;
    constexpr float kBossSupportMinPlayerDistance = 520.f;

    int ClampInt(int value, int minValue, int maxValue)
    {
        if (value < minValue)
            return minValue;
        if (value > maxValue)
            return maxValue;
        return value;
    }

    int GetNavigationIndexForGrid(int col, int row, int cols)
    {
        return row * cols + col;
    }

    bool IsNavigationCellBlockedForGrid(const std::vector<bool>& blocked, int cols, int rows, int col, int row)
    {
        if (col < 0 || col >= cols || row < 0 || row >= rows)
            return true;

        return blocked[GetNavigationIndexForGrid(col, row, cols)];
    }

    bool FindNearestOpenCellForGrid(const std::vector<bool>& blocked, int cols, int rows, int& col, int& row)
    {
        if (!IsNavigationCellBlockedForGrid(blocked, cols, rows, col, row))
            return true;

        for (int radius = 1; radius < std::max(cols, rows); ++radius)
        {
            for (int dc = -radius; dc <= radius; ++dc)
            {
                for (int dr = -radius; dr <= radius; ++dr)
                {
                    if (std::abs(dc) != radius && std::abs(dr) != radius)
                        continue;

                    int nextCol = col + dc;
                    int nextRow = row + dr;
                    if (!IsNavigationCellBlockedForGrid(blocked, cols, rows, nextCol, nextRow))
                    {
                        col = nextCol;
                        row = nextRow;
                        return true;
                    }
                }
            }
        }

        return false;
    }

    int GetClosestOpenNavigationIndexForGrid(const std::vector<bool>& blocked, int cols, int rows, int col, int row)
    {
        int nextCol = ClampInt(col, 0, std::max(0, cols - 1));
        int nextRow = ClampInt(row, 0, std::max(0, rows - 1));

        if (!FindNearestOpenCellForGrid(blocked, cols, rows, nextCol, nextRow))
            return -1;

        return GetNavigationIndexForGrid(nextCol, nextRow, cols);
    }

    Engine::NavigationRefreshResult BuildNavigationRefreshResult(
        const std::vector<bool>& blocked,
        int cols,
        int rows,
        float cellSize,
        Vector2 playerFeet)
    {
        Engine::NavigationRefreshResult result{};

        if (cols <= 0 || rows <= 0)
            return result;

        result.distanceField.assign(cols * rows, std::numeric_limits<int>::max());

        int playerCol = ClampInt((int)(playerFeet.x / cellSize), 0, cols - 1);
        int playerRow = ClampInt((int)(playerFeet.y / cellSize), 0, rows - 1);
        result.playerNavIndex = GetClosestOpenNavigationIndexForGrid(blocked, cols, rows, playerCol, playerRow);

        if (result.playerNavIndex < 0)
            return result;

        using QueueItem = std::pair<int, int>;
        std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> frontier;
        result.distanceField[result.playerNavIndex] = 0;
        frontier.push({ 0, result.playerNavIndex });

        struct SearchOffset { int col, row, cost; };
        static const std::array<SearchOffset, 8> navOffsets{{
            {1,0,10},{-1,0,10},{0,1,10},{0,-1,10},
            {1,1,14},{1,-1,14},{-1,1,14},{-1,-1,14}
        }};

        while (!frontier.empty())
        {
            int cost = frontier.top().first;
            int currentIndex = frontier.top().second;
            frontier.pop();

            if (cost > result.distanceField[currentIndex])
                continue;

            int currentCol = currentIndex % cols;
            int currentRow = currentIndex / cols;

            for (const SearchOffset& off : navOffsets)
            {
                int nextCol = currentCol + off.col;
                int nextRow = currentRow + off.row;

                if (IsNavigationCellBlockedForGrid(blocked, cols, rows, nextCol, nextRow))
                    continue;

                if (off.col != 0 && off.row != 0)
                {
                    if (IsNavigationCellBlockedForGrid(blocked, cols, rows, currentCol + off.col, currentRow) ||
                        IsNavigationCellBlockedForGrid(blocked, cols, rows, currentCol, currentRow + off.row))
                    {
                        continue;
                    }
                }

                int nextIndex = GetNavigationIndexForGrid(nextCol, nextRow, cols);
                int nextCost = cost + off.cost;

                if (nextCost >= result.distanceField[nextIndex])
                    continue;

                result.distanceField[nextIndex] = nextCost;
                frontier.push({ nextCost, nextIndex });
            }
        }

        return result;
    }
}

Engine::Engine()
{
    Init();
}

Engine::~Engine()
{
    if (_navRefreshJob.valid())
        _navRefreshJob.wait();

    Enemy::UnloadSharedResources();
    Cyclops::UnloadSharedResources();
    Ogre::UnloadSharedResources();
    Molarbeast::UnloadSharedResources();
    // [SHELVED] FireBallPickup/SwordBeamPickup/FreezePickup unload calls removed
    HealPickup::UnloadSharedResources();
    ManaGemPickup::UnloadSharedResources();
    SpreadProjectile::UnloadSharedResources();
    SwordBeamProjectile::UnloadSharedResources();
    FreezeProjectile::UnloadSharedResources();
    LavaBallProjectile::UnloadSharedResources();
    UnloadSound(_pickupSound);
    UnloadSound(_fireballCastSound);
    UnloadSound(_explosionSound);
    UnloadSound(_lavaBallImpactSound);
    UnloadSound(_bladeBeamSound);
    UnloadSound(_buttonPressSound);
    UnloadTexture(_map);
    UnloadTexture(_pillarTex);
    UnloadTexture(_torchTex);
    UnloadTexture(_pillarTorchTex);
    UnloadTexture(_fireballCastTex);
    UnloadTexture(_fireballHitTex);
    UnloadTexture(_swordBeamCastTex);
    UnloadTexture(_swordBeamHitTex);
    UnloadTexture(_freezeCastTex);
    UnloadTexture(_freezeHitTex);
    UnloadTexture(_lightningCastTex);
    UnloadTexture(_healEffectTex);
    UnloadTexture(_abilityIconFireTex);
    UnloadTexture(_abilityIconIceTex);
    UnloadTexture(_abilityIconElectricTex);
    UnloadTexture(_upgradeAttackPowerTex);
    UnloadTexture(_upgradeAttackRangeTex);
    UnloadTexture(_upgradeHealthTex);
    UnloadTexture(_upgradeMagicTex);
    UnloadTexture(_upgradeDefenseTex);
    UnloadTexture(_upgradeMoveSpeedTex);
    _pauseUI.Unload();
    CloseWindow();
}

void Engine::Init()
{
    InitWindow(_windowWidth, _windowHeight, "Mystic Onslaught");
    SetTargetFPS(60);
    InitAudioDevice();

    _leaderboard.Load("leaderboard.txt");

    SetExitKey(KEY_NULL);

    _pickupSound      = LoadSound(AssetPath("Sounds/PickupSound.mp3").c_str());
    _fireballCastSound= LoadSound(AssetPath("Sounds/GS1_Spell_Fire.mp3").c_str());
    _explosionSound   = LoadSound(AssetPath("Sounds/GS1_Spell_Explode.mp3").c_str());
    {
        // Lavaball impact should only play for the first second of the clip.
        // Cropping the wave at load time keeps playback simple and lets each
        // collision just trigger a normal PlaySound call.
        Wave lavaImpactWave = LoadWave(AssetPath("Sounds/Explosion.wav").c_str());
        int oneSecondFrame = lavaImpactWave.sampleRate;
        if (oneSecondFrame > 0 && (int)lavaImpactWave.frameCount > oneSecondFrame)
            WaveCrop(&lavaImpactWave, 0, oneSecondFrame);
        _lavaBallImpactSound = LoadSoundFromWave(lavaImpactWave);
        UnloadWave(lavaImpactWave);
    }
    _bladeBeamSound   = LoadSound(AssetPath("Sounds/GS1_BladeBeam.mp3").c_str());
    _buttonPressSound = LoadSound(AssetPath("Sounds/ButtonPress.mp3").c_str());

    SetSoundPitch (_buttonPressSound, 1.25f);
    SetSoundVolume(_buttonPressSound, 0.35f);

    SetSoundPitch (_pickupSound, 1.35f);
    SetSoundVolume(_pickupSound, 0.45f);
    SetSoundVolume(_lavaBallImpactSound, 0.45f);

    _pauseUI.Init();
    LoadKeybindings();

    _menu.Init();

    _map = LoadTexture(AssetPath("TileSet/Map.png").c_str());
    _pillarTex      = LoadTexture(AssetPath("TileSet/Pillar.png").c_str());
    _torchTex       = LoadTexture(AssetPath("TileSet/Torch.png").c_str());
    _pillarTorchTex = LoadTexture(AssetPath("TileSet/PillarTorch.png").c_str());
    _fireballCastTex = LoadTexture(AssetPath("PowerUps/Fireball_Cast.png").c_str());
    _fireballHitTex = LoadTexture(AssetPath("PowerUps/Fireball_Hit.png").c_str());
    _swordBeamCastTex = LoadTexture(AssetPath("PowerUps/BladeBeam_Cast.png").c_str());
    _swordBeamHitTex = LoadTexture(AssetPath("PowerUps/Hit03.png").c_str());
    _freezeCastTex = LoadTexture(AssetPath("PowerUps/Ice_Shard_Cast.png").c_str());
    _freezeHitTex = LoadTexture(AssetPath("PowerUps/Ice_Shard_Hit.png").c_str());
    _lightningCastTex = LoadTexture(AssetPath("PowerUps/LightningCast.png").c_str());
    _healEffectTex = LoadTexture(AssetPath("PowerUps/Health_Up.png").c_str());
    _abilityIconFireTex     = LoadTexture(AssetPath("PowerUps/FireBallPickup.png").c_str());
    _abilityIconIceTex      = LoadTexture(AssetPath("PowerUps/IceSpellPickup.png").c_str());
    _abilityIconElectricTex = LoadTexture(AssetPath("PowerUps/LightningPickup.png").c_str());

    _upgradeAttackPowerTex = LoadTexture(AssetPath("UI/Upgrades/AttackPower.png").c_str());
    _upgradeAttackRangeTex = LoadTexture(AssetPath("UI/Upgrades/AttackRange.png").c_str());
    _upgradeHealthTex      = LoadTexture(AssetPath("UI/Upgrades/HealthUpgrade.png").c_str());
    _upgradeMagicTex       = LoadTexture(AssetPath("UI/Upgrades/Magic.png").c_str());
    _upgradeDefenseTex     = LoadTexture(AssetPath("UI/Upgrades/Defense.png").c_str());
    _upgradeMoveSpeedTex   = LoadTexture(AssetPath("UI/Upgrades/MoveSpeed.png").c_str());

    _props.clear();
    _pickups.clear();
    _spreadProjectiles.clear();
    _ultimateBlasts.clear();
    _swordBeamProjectiles.clear();
    _freezeProjectiles.clear();
    _lavaBalls.clear();
    _effects.clear();
    _enemies.clear();

    // Total prop slots: 7-10. Always reserve 3 for torch decorations
    // (2 wall torches + 1 pillar torch); the rest are solid stone pillars.
    // Frame sizes: Torch = 8 frames of 32x29, PillarTorch = 5 frames of 53x48.
    // Adjust frame counts here if the sprite sheets differ on disk.
    int propCount  = GetRandomValue(7, 10);
    int pillarCount = propCount - 3;

    for (int i = 0; i < pillarCount; i++)
    {
        Vector2 pos = GetRandomPropPosition();
        _props.push_back(Prop{ pos, _pillarTex });
    }
    for (int i = 0; i < 2; i++)
    {
        Vector2 pos = GetRandomPropPosition();
        _props.push_back(Prop{ pos, _torchTex, 8, 32, 29, 2.5f });
    }
    {
        Vector2 pos = GetRandomPropPosition();
        // PillarTorch.png: 290x52, 8 frames at stride 32 starting at col 17.
        // The 21px leading/trailing padding means frames aren't at col 0,
        // so frameXOffset=17 slides each source rect to capture the content.
        _props.push_back(Prop{ pos, _pillarTorchTex, 8, 32, 52, 3.f, 1.f / 10.f, 17 });
    }

    BuildNavigationGrid();

    _pickupSpawnTimer = kDefaultTimedPickupInterval;

    ResetRunState();
}

void Engine::SpawnWave()
{
    _wave++;
    _message = "Wave " + std::to_string(_wave);
    _waveStarting = true;
    _waveIntroTimer = _waveIntroDuration;
}

void Engine::SpawnEnemies()
{
    float mapW = _map.width * _mapScale;
    float mapH = _map.height * _mapScale;

    // Proper wave flow: every 5th wave is a solo boss fight for now. That
    // keeps the boss readable until the later "boss plus adds" rules are added.
    if (_wave > 0 && _wave % 5 == 0)
    {
        Vector2 pos{ mapW * 0.5f, mapH * 0.28f };
        SpawnMolarbeast(pos);
        SpawnBossSupportAdds();
        return;
    }

    // Regular enemies and cyclops share the same active budget so the HUD,
    // wave-complete checks, and performance cap all measure total pressure.
    const int cyclopsCount = GetCyclopsSpawnCountForWave(_wave);
    const int ogreCount = GetOgreSpawnCountForWave(_wave);
    const int enemyCount = std::max(0, std::min(_wave * 2, _maxActiveEnemies) - cyclopsCount - ogreCount);

    for (int i = 0; i < enemyCount; i++)
    {
        Vector2 pos{};
        int attempts = 0;

        do
        {
            pos.x = (float)GetRandomValue(300, (int)mapW - 300);
            pos.y = (float)GetRandomValue(300, (int)mapH - 300);
            attempts++;
        } while (!IsSpawnPositionValid(pos) && attempts < 40);

        if (TryGetPooledEnemySpawn(pos))
            continue;

        auto enemy = std::make_unique<Enemy>(pos);
        enemy->Init();
        ConfigureSpawnedEnemy(*enemy);
        _enemies.push_back(std::move(enemy));
    }

    // Cyclops use the same shared enemy pool, but only start appearing once
    // the player has had a few waves to learn melee and pickup usage.
    for (int i = 0; i < cyclopsCount; i++)
    {
        Vector2 pos{};
        int attempts = 0;
        do
        {
            pos.x = (float)GetRandomValue(300, (int)mapW - 300);
            pos.y = (float)GetRandomValue(300, (int)mapH - 300);
            attempts++;
        } while (!IsSpawnPositionValid(pos) && attempts < 40);

        SpawnCyclops(pos);
    }

    for (int i = 0; i < ogreCount; i++)
    {
        Vector2 pos{};
        int attempts = 0;

        do
        {
            pos.x = (float)GetRandomValue(300, (int)mapW - 300);
            pos.y = (float)GetRandomValue(300, (int)mapH - 300);
            attempts++;
        } while (!IsSpawnPositionValid(pos) && attempts < 40);

        SpawnOgre(pos);
    }
}

void Engine::Run()
{
    while (!WindowShouldClose() && !_shouldClose)
        RunFrame();
}

void Engine::RunFrame()
{
    float dt = GetFrameTime();

    Update(dt);

    BeginDrawing();
    ClearBackground(WHITE);
    Draw();
    EndDrawing();
}

void Engine::Update(float dt)
{
    switch (_gameState)
    {
    case GameState::Menu:
    {
        _menu.Update();

        if (_menu.StartPressed())
        {
            ResetRunState();
            _fadeInTimer = 2.0f;
            GenerateStartingAbilityOptions();
            _awaitingStartingAbility = true;
            _levelUpReturnState = GameState::Play;
            _levelUpOpenTimer = 0.8f;
            _gameState = GameState::LevelUpChoice;
        }
        if (_menu.QuitPressed())
            _shouldClose = true;
        if (_menu.HowToPressed())
        {
            _howToPlayFrom = GameState::Menu;
            _gameState = GameState::HowToPlay;
        }
        if (_menu.LeaderboardPressed())
            _gameState = GameState::Leaderboard;

        break;
    }

    case GameState::HowToPlay:
    {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE))
        {
            if (_howToPlayFrom == GameState::Menu)
                _menu.Init();
            _gameState = _howToPlayFrom;
        }
        break;
    }

    case GameState::Keybindings:
        // Navigation handled in the Draw case (Back button)
        break;

    case GameState::Play:
    {
        UpdateGamePlay(dt);
        break;
    }

    case GameState::Pause:
    {
        if (IsKeyPressed(KEY_ESCAPE))
            _gameState = GameState::Play;

        break;
    }

    case GameState::GameOver:
        break;

    case GameState::LevelUpChoice:
        if (_levelUpOpenTimer > 0.f)
            _levelUpOpenTimer -= dt;
        break;
    }
}

void Engine::UpdateGamePlay(float dt)
{
    if (IsKeyPressed(KEY_ESCAPE))
    {
        _gameState = GameState::Pause;
        return;
    }

    if (_fadeInTimer > 0.f)
        _fadeInTimer -= dt;
    if (_bossWarningTimer > 0.f)
        _bossWarningTimer -= dt;

    // Block attacks and abilities during wave intro and ultimate cinematic
    _player.SetCombatLocked(_waveStarting || _ultimatePhase != UltimatePhase::None);
    _player.Update(dt);

    // During the cinematic ultimate sequence everything except the player
    // animation is frozen. Drain any pending cast request so it doesn't fire
    // twice when play resumes.
    if (_ultimatePhase != UltimatePhase::None)
    {
        _player.ConsumeCastRequest();
        UpdateUltimateSequence(dt);
        return;
    }

    Character::CastType castType = _player.ConsumeCastRequest();

    if (castType == Character::CastType::FireSpread ||
        castType == Character::CastType::IceSpread  ||
        castType == Character::CastType::ElectricSpread)
    {
        AbilityType element =
            (castType == Character::CastType::FireSpread)     ? AbilityType::FireSpread :
            (castType == Character::CastType::IceSpread)      ? AbilityType::IceSpread  :
                                                                 AbilityType::ElectricSpread;
        SpawnCastEffect(castType);
        StopSound(_fireballCastSound);
        PlaySound(_fireballCastSound);
        SpawnSpreadBurst(element);
    }
    else if (castType == Character::CastType::FireUltimate  ||
             castType == Character::CastType::IceUltimate   ||
             castType == Character::CastType::ElectricUltimate)
    {
        AbilityType element =
            (castType == Character::CastType::FireUltimate)     ? AbilityType::FireUltimate  :
            (castType == Character::CastType::IceUltimate)      ? AbilityType::IceUltimate   :
                                                                   AbilityType::ElectricUltimate;
        TriggerUltimateSequence(element);
    }
    else if (castType == Character::CastType::FireBolt  ||
             castType == Character::CastType::IceBolt   ||
             castType == Character::CastType::ElectricBolt)
    {
        AbilityType element =
            (castType == Character::CastType::FireBolt)     ? AbilityType::FireBolt  :
            (castType == Character::CastType::IceBolt)      ? AbilityType::IceBolt   :
                                                               AbilityType::ElectricBolt;
        // Reuse the matching spread cast effect for visual feedback
        Character::CastType effectType =
            (element == AbilityType::FireBolt)     ? Character::CastType::FireSpread :
            (element == AbilityType::IceBolt)      ? Character::CastType::IceSpread  :
                                                     Character::CastType::ElectricSpread;
        SpawnCastEffect(effectType);
        StopSound(_fireballCastSound);
        PlaySound(_fireballCastSound);
        SpawnBolt(element);
    }
    // [SHELVED] — SwordBeam and FreezeWave dispatch. These branches are never
    // reached because no UpgradeType in GenerateLevelUpOptions can teach either
    // ability. The projectile vectors (_swordBeamProjectiles, _freezeProjectiles)
    // are always empty. Code preserved in case these are reintegrated later.
    else if (castType == Character::CastType::SwordBeam)
    {
        SpawnCastEffect(castType);
        StopSound(_bladeBeamSound);
        PlaySound(_bladeBeamSound);
        SpawnSwordBeam();
    }
    else if (castType == Character::CastType::Freeze)
    {
        SpawnCastEffect(castType);
        SpawnFreezeWave();
    }

    if (_player.GetHealthValue() <= 0.f && !_playerDying)
    {
        _playerDying = true;
        _gameOverTimer = _gameOverDelay;
    }

    if (_playerDying)
    {
        _gameOverTimer -= dt;

        if (_gameOverTimer <= 0.f)
        {
            _awaitingNameEntry = true;
            _pauseUI.ResetNameEntry();
            _gameState = GameState::GameOver;
        }

        return;
    }

    if (_waveStarting)
    {
        _waveIntroTimer -= dt;

        if (_waveIntroTimer <= 0.f)
        {
            _waveStarting = false;
            SpawnEnemies();
            _player.GrantInvulnerability(kWaveSpawnProtectionDuration);
        }
    }
    else
    {
        _gameTimer += dt;
        ApplyCompletedNavigationRefresh();

        // Testing cadence: during the boss fight, keep combat pickups flowing
        // constantly so the fight can be tuned without long dry periods.
        _pickupSpawnTimer -= dt;
        if (_pickupSpawnTimer <= 0.f)
        {
            SpawnTimedPickup();
            _pickupSpawnTimer = IsBossFightActive() ? kBossTimedPickupInterval : kDefaultTimedPickupInterval;
        }

        if (GetActiveEnemyCount() == 0)
        {
            // After wave 1 clears, guarantee a level-up before wave 2 begins.
            // Top up the player's exp to the next threshold so the level-up
            // screen fires naturally, keeping the level/exp counters in sync.
            if (_wave == 1 && !_wave1LevelUpDone)
            {
                _wave1LevelUpDone = true;
                int prevLevel  = _player.GetLevel();
                int remaining  = _player.GetExpToNext() - _player.GetExp();
                if (remaining > 0)
                    _player.AddExp(remaining);
                if (_player.GetLevel() > prevLevel)
                {
                    GenerateLevelUpOptions();
                    _levelUpReturnState = GameState::Play;
                    _levelUpOpenTimer   = 0.25f;
                    _gameState          = GameState::LevelUpChoice;
                    return; // wave 2 spawns when we come back to Play
                }
            }
            SpawnWave();
        }

        _navRefreshTimer -= dt;
        Vector2 playerFeet = _player.GetFeetWorldPos();
        int playerCol = ClampInt((int)(playerFeet.x / _navCellSize), 0, std::max(0, _navCols - 1));
        int playerRow = ClampInt((int)(playerFeet.y / _navCellSize), 0, std::max(0, _navRows - 1));
        int playerNavIndex = (_navCols > 0 && _navRows > 0) ? GetClosestOpenNavigationIndex(playerCol, playerRow) : -1;
        if (!_navRefreshInFlight && (_navRefreshTimer <= 0.f || playerNavIndex != _lastPlayerNavIndex))
        {
            RefreshNavigationField();
            _navRefreshTimer = _navRefreshInterval;
        }

        std::vector<Vector2> propCenters;
        propCenters.reserve(_props.size());
        for (auto& prop : _props)
            propCenters.push_back(prop.GetWorldPos());

        for (auto& enemy : _enemies)
        {
            if (!enemy->IsActive())
                continue;

            Vector2 navigationTarget = playerFeet;
            bool hasNavigationTarget = false;

            if (enemy->IsBoss())
            {
                // Boss pathing always comes from the A* helper so large enemies
                // can route around pillars consistently instead of only
                // pathing when line of sight is already broken.
                navigationTarget = GetAStarTarget(enemy->GetWorldPos(), playerFeet);
                hasNavigationTarget = !Vector2Equals(navigationTarget, playerFeet);
            }
            else if (!enemy->UsesDirectPursuit() &&
                !HasLineOfSight(enemy->GetWorldPos(), playerFeet))
            {
                navigationTarget = GetNavigationTarget(enemy->GetWorldPos(), playerFeet);
                hasNavigationTarget = !Vector2Equals(navigationTarget, playerFeet);
            }

            enemy->Update(dt, playerFeet, navigationTarget, hasNavigationTarget, _enemies, propCenters);

            // Cyclops shares the enemy pool, but still emits a separate laser
            // projectile when its charge completes. The engine owns the laser
            // list so projectile collision stays centralized.
            if (Cyclops* cyclops = enemy->AsCyclops())
            {
                if (cyclops->WantsToFire())
                {
                    CyclopsLaserProjectile laser;
                    laser.Init(cyclops->GetWorldPos(), cyclops->GetFireDirection(), cyclops->GetAttackPower());
                    _cyclopsLasers.push_back(laser);
                    cyclops->OnFired();
                    cyclops->PlayAttackSound();
                }
            }

            // Ogre rushes request impact shake when they end on a player, prop,
            // wall, or arena boundary. The engine owns the camera system, so
            // it consumes that one-shot request here after each enemy update.
            if (Ogre* ogre = enemy->AsOgre())
            {
                if (ogre->ConsumeImpactShakeRequest())
                    TriggerScreenShake(8.f, 0.14f);
            }

            // Molarbeast uses an engine-owned lavaball list for the same reason
            // Cyclops uses engine-owned lasers: collision with the player and
            // arena boundaries stays centralized in one place.
            if (Molarbeast* molarbeast = enemy->AsMolarbeast())
            {
                if (molarbeast->WantsToFireLavaBall())
                {
                    LavaBallProjectile projectile;
                    Vector2 toTarget = Vector2Subtract(molarbeast->GetQueuedLavaBallTarget(), molarbeast->GetLavaBallSpawnPos());
                    projectile.Init(molarbeast->GetLavaBallSpawnPos(), toTarget);
                    _lavaBalls.push_back(projectile);
                    molarbeast->OnLavaBallSpawned();
                }

                if (molarbeast->ConsumeImpactShakeRequest())
                    TriggerScreenShake(8.f, 0.14f);
            }
        }

        HandlePlayerMeleeDamage();
        UpdateSpreadProjectiles(dt);
        UpdateSwordBeamProjectiles(dt);
        UpdateFreezeProjectiles(dt);
        UpdateLavaBallProjectiles(dt);
        UpdateEffects(dt);
        UpdateEnemyCount(dt);
        UpdateBossSupportRespawns(dt);
        UpdateCyclopsLasers(dt);
    }

    for (auto& pickup : _pickups)
    {
        if (!pickup->IsActive())
            continue;

        if (CheckCollisionRecs(_player.GetCollisionRec(), pickup->GetCollisionRec()))
        {
            StopSound(_pickupSound);
            PlaySound(_pickupSound);
            pickup->OnCollect(_player);
        }
    }

    _pickups.erase(
        std::remove_if(_pickups.begin(), _pickups.end(),
            [](const std::unique_ptr<Pickup>& pickup) { return !pickup->IsActive(); }),
        _pickups.end());

    int healEffectsToSpawn = _player.ConsumeHealEffectRequests();
    for (int i = 0; i < healEffectsToSpawn; ++i)
        SpawnHealEffect();

    HandleCollisions();

    {
        float mapW = _map.width  * _mapScale;
        float mapH = _map.height * _mapScale;

        // _cameraPos = world position shown at screen centre
        float camMinX = _windowWidth  / 2.f;
        float camMaxX = mapW - _windowWidth  / 2.f;
        float camMinY = _windowHeight / 2.f;
        float camMaxY = mapH - _windowHeight / 2.f;

        _cameraPos.x = _player.GetWorldPos().x;
        _cameraPos.y = _player.GetWorldPos().y;
        if (_cameraPos.x < camMinX) _cameraPos.x = camMinX;
        if (_cameraPos.x > camMaxX) _cameraPos.x = camMaxX;
        if (_cameraPos.y < camMinY) _cameraPos.y = camMinY;
        if (_cameraPos.y > camMaxY) _cameraPos.y = camMaxY;

        // _mapPos: top-left of map on screen, aligned with the object rendering convention
        _mapPos.x = _windowWidth  / 2.f - _cameraPos.x;
        _mapPos.y = _windowHeight / 2.f - _cameraPos.y;
    }

    if (_shakeTimer > 0.f)
    {
        _shakeTimer -= dt;

        float x = GetRandomValue(-100, 100) / 50.f * _shakeStrength;
        float y = GetRandomValue(-100, 100) / 50.f * _shakeStrength;
        _shakeOffset = Vector2{ x, y };
    }
    else
    {
        _shakeOffset = Vector2Zero();
    }
}

void Engine::Draw()
{
    switch (_gameState)
    {
    case GameState::Menu:
    {
        _menu.Draw();
        break;
    }

    case GameState::Play:
    {
        DrawWorld();
        DrawUltimateSequence();
        DrawHUD();
        DrawWaveIntro();

        if (_fadeInTimer > 0.f)
        {
            float alpha = _fadeInTimer / 2.0f;   // 1.0 → 0.0 over two seconds
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, alpha));
        }
        break;
    }

    case GameState::Pause:
    {
        Vector2 shakenMapPos = Vector2Add(_mapPos, _shakeOffset);
        Vector2 cameraRef    = Vector2Subtract(_cameraPos, _shakeOffset);
        Vector2 worldOffset  = { -_cameraPos.x + _shakeOffset.x, -_cameraPos.y + _shakeOffset.y };

        DrawTextureEx(_map, shakenMapPos, 0.0f, _mapScale, WHITE);

        for (auto& prop : _props)
            prop.Render(cameraRef);

        for (auto& pickup : _pickups)
            pickup->Draw(worldOffset);

        for (const auto& projectile : _spreadProjectiles)
            projectile.Draw(worldOffset);

        for (const auto& projectile : _swordBeamProjectiles)
            projectile.Draw(worldOffset);

        for (const auto& projectile : _freezeProjectiles)
            projectile.Draw(worldOffset);

        for (const auto& projectile : _lavaBalls)
            projectile.Draw(worldOffset);

        for (const auto& laser : _cyclopsLasers)
            laser.Draw(worldOffset);

        DrawEffects(worldOffset);

        for (auto& enemy : _enemies)
        {
            if (!enemy->IsActive())
                continue;
            enemy->DrawEnemy(cameraRef);
        }

        _player.DrawPlayer(cameraRef);

        int pauseResult = _pauseUI.DrawPause();
        if (pauseResult != 0) { StopSound(_buttonPressSound); PlaySound(_buttonPressSound); }
        if (pauseResult == 1)
            _gameState = GameState::Play;
        else if (pauseResult == 2)
        {
            _howToPlayFrom = GameState::Pause;
            _gameState = GameState::HowToPlay;
        }
        else if (pauseResult == 3)
            _shouldClose = true;
        else if (pauseResult == 4)
        {
            _keybindingsEdit = _player.GetBindings();
            _gameState = GameState::Keybindings;
        }

        break;
    }

    case GameState::GameOver:
    {
        if (_awaitingNameEntry)
        {
            std::string confirmed = _pauseUI.DrawNameEntry(_wave, _gameTimer, _enemiesKilled);
            if (!confirmed.empty())
            {
                _leaderboard.AddEntry(_wave, _gameTimer, _enemiesKilled, confirmed);
                _leaderboard.Save("leaderboard.txt");
                _awaitingNameEntry = false;
            }
        }
        else
        {
            int goResult = _pauseUI.DrawGameOver(_wave, _gameTimer, _enemiesKilled, _leaderboard.GetEntries());
            if (goResult != 0) { StopSound(_buttonPressSound); PlaySound(_buttonPressSound); }
            if (goResult == 1) { ResetRunState(); _fadeInTimer = 2.0f; GenerateStartingAbilityOptions(); _awaitingStartingAbility = true; _levelUpReturnState = GameState::Play; _levelUpOpenTimer = 0.8f; _gameState = GameState::LevelUpChoice; }
            else if (goResult == 2) { ResetRunState(); _menu.Init(); _gameState = GameState::Menu; }
            else if (goResult == 3) _shouldClose = true;
        }
        break;
    }

    case GameState::HowToPlay:
    {
        DrawHowToPlay();
        break;
    }

    case GameState::Keybindings:
    {
        if (_pauseUI.DrawKeybindings(_keybindingsEdit))
        {
            _player.SetBindings(_keybindingsEdit);
            SaveKeybindings();
            _gameState = GameState::Pause;
        }
        break;
    }

    case GameState::Leaderboard:
    {
        if (_pauseUI.DrawLeaderboardScreen(_leaderboard.GetEntries()))
        {
            _menu.Init();
            _gameState = GameState::Menu;
        }
        break;
    }

    case GameState::LevelUpChoice:
    {
        DrawWorld();
        DrawHUD();
        DrawLevelUpChoice();
        break;
    }

    }
}

void Engine::HandleCollisions()
{
    float mapW = _map.width  * _mapScale;
    float mapH = _map.height * _mapScale;

    // Per-side player boundaries
    const float marginLeft   = 76.f;
    const float marginRight  = 96.f;
    const float marginTop    = 42.f;   // let the player go a little higher
    const float marginBottom = 320.f;  // two character heights — wall is drawn above the map edge

    Vector2 pos = _player.GetWorldPos();
    if (pos.x < marginLeft  || pos.x > mapW - marginRight
     || pos.y < marginTop   || pos.y > mapH - marginBottom)
    {
        if (_player.IsBeingForcedPushed())
            _player.OnForcedPushCollision();

        // Always clamp — stops forced pushes and normal movement at the boundary
        pos.x = std::max(marginLeft,        std::min(pos.x, mapW - marginRight));
        pos.y = std::max(marginTop,         std::min(pos.y, mapH - marginBottom));
        _player.SetWorldPos(pos);
    }

    for (auto& prop : _props)
    {
        if (CheckCollisionRecs(prop.GetCollisionRec(), _player.GetCollisionRec()))
        {
            if (_player.IsBeingForcedPushed())
                _player.OnForcedPushCollision();

            // Always eject the player fully outside the prop via MTV
            Rectangle pr    = _player.GetCollisionRec();
            Rectangle propr = prop.GetCollisionRec();
            float overlapX = std::min(pr.x + pr.width,  propr.x + propr.width)  - std::max(pr.x, propr.x);
            float overlapY = std::min(pr.y + pr.height, propr.y + propr.height) - std::max(pr.y, propr.y);
            Vector2 ppos = _player.GetWorldPos();
            if (overlapX < overlapY)
            {
                float dir = (pr.x + pr.width * 0.5f < propr.x + propr.width * 0.5f) ? -1.f : 1.f;
                _player.SetWorldPos({ ppos.x + dir * overlapX, ppos.y });
            }
            else
            {
                float dir = (pr.y + pr.height * 0.5f < propr.y + propr.height * 0.5f) ? -1.f : 1.f;
                _player.SetWorldPos({ ppos.x, ppos.y + dir * overlapY });
            }
        }

        for (auto& enemy : _enemies)
        {
            if (!enemy->IsActive())
                continue;
            if (enemy->IgnoresPropCollisions())
                continue;
            if (CheckCollisionCircleRec(prop.GetEnemyCollisionCenter(), prop.GetEnemyCollisionRadius(), enemy->GetCollisionRec()))
            {
                if (Ogre* ogre = enemy->AsOgre())
                {
                    if (ogre->IsRushing())
                        ogre->OnRushBlocked();
                }
                else if (Molarbeast* molarbeast = enemy->AsMolarbeast())
                {
                    if (molarbeast->IsDashing())
                        molarbeast->OnDashBlocked();
                }

                // Always eject the enemy fully outside the prop via MTV
                Rectangle er    = enemy->GetCollisionRec();
                Rectangle propr = prop.GetCollisionRec();
                if (CheckCollisionRecs(er, propr))
                {
                    float overlapX = std::min(er.x + er.width,  propr.x + propr.width)  - std::max(er.x, propr.x);
                    float overlapY = std::min(er.y + er.height, propr.y + propr.height) - std::max(er.y, propr.y);
                    Vector2 epos = enemy->GetWorldPos();
                    if (overlapX < overlapY)
                    {
                        float dir = (er.x + er.width * 0.5f < propr.x + propr.width * 0.5f) ? -1.f : 1.f;
                        enemy->Teleport({ epos.x + dir * overlapX, epos.y });
                    }
                    else
                    {
                        float dir = (er.y + er.height * 0.5f < propr.y + propr.height * 0.5f) ? -1.f : 1.f;
                        enemy->Teleport({ epos.x, epos.y + dir * overlapY });
                    }
                }
                else
                {
                    enemy->UndoMovement();
                }
            }
        }

    }

      // Enemies should obey the same arena rectangle as the player. Using the
      // shared per-side margins keeps all actors inside the intended playable
      // space instead of letting special enemies, like the ogre, rush beyond
      // the lower boundary.
      for (auto& enemy : _enemies)
      {
          if (!enemy->IsActive())
              continue;
          if (enemy->IsDying()) continue;
          Vector2 pos = enemy->GetWorldPos();
          if (pos.x < marginLeft  || pos.x > mapW - marginRight ||
              pos.y < marginTop   || pos.y > mapH - marginBottom)
          {
              if (Ogre* ogre = enemy->AsOgre())
              {
                  if (ogre->IsRushing())
                      ogre->OnRushBlocked();
                  else
                      enemy->UndoMovement();
              }
              else if (Molarbeast* molarbeast = enemy->AsMolarbeast())
              {
                  if (molarbeast->IsDashing())
                      molarbeast->OnDashBlocked();
                  else
                      enemy->UndoMovement();
              }
              else
              {
                  enemy->UndoMovement();
              }
        }
    }


    // Player vs enemy solid collision — dash passes straight through.
    // After the dash ends, eject the player if they landed inside an enemy.
    if (!_player.IsDashing())
    {
        for (auto& enemy : _enemies)
        {
            if (!enemy->IsActive() || !enemy->IsAlive())
                continue;

            Rectangle pr = _player.GetCollisionRec();
            Rectangle er = enemy->GetCollisionRec();
            if (!CheckCollisionRecs(pr, er))
                continue;

            // If the player is being force-pushed by an Ogre charge, hitting an
            // enemy should stop the push (same behaviour as hitting a prop/wall).
            // Using UndoMovement here causes the player to jitter inside the enemy
            // because the forced-push velocity overrides the revert every frame.
            // While the player is being force-pushed they slide through enemies;
            // only walls and props stop the push.
            if (_player.IsBeingForcedPushed())
                continue;

            // Normal case: revert the movement that caused the overlap
            _player.UndoMovement();

            // If still overlapping (e.g. stuck inside after a dash), eject via MTV
            pr = _player.GetCollisionRec();
            if (CheckCollisionRecs(pr, er))
            {
                float overlapX = std::min(pr.x + pr.width,  er.x + er.width)  - std::max(pr.x, er.x);
                float overlapY = std::min(pr.y + pr.height, er.y + er.height) - std::max(pr.y, er.y);
                Vector2 pos    = _player.GetWorldPos();
                if (overlapX < overlapY)
                {
                    float dir = (pr.x + pr.width * 0.5f < er.x + er.width * 0.5f) ? -1.f : 1.f;
                    _player.SetWorldPos({ pos.x + dir * overlapX, pos.y });
                }
                else
                {
                    float dir = (pr.y + pr.height * 0.5f < er.y + er.height * 0.5f) ? -1.f : 1.f;
                    _player.SetWorldPos({ pos.x, pos.y + dir * overlapY });
                }
            }
        }
    }
}

void Engine::UpdateEnemyCount(float dt)
{
    for (auto& enemy : _enemies)
    {
        if (!enemy->IsActive())
            continue;

        Vector2 dropPos = enemy->GetWorldPos();
        if (enemy->UpdateDeath(dt))
        {
            // Boss support adds respawn on a timer while the Molarbeast is
            // alive. Track their death moment before deactivating the pooled
            // object so the engine can bring back the same pressure slot later.
            if (enemy.get() == _bossCyclopsSupport.enemy && IsBossFightActive())
                _bossCyclopsSupport.respawnTimer = kBossSupportRespawnDelay;
            if (enemy.get() == _bossOgreSupport.enemy && IsBossFightActive())
                _bossOgreSupport.respawnTimer = kBossSupportRespawnDelay;

            // Exp rules:
            //  Boss kill     → flat bonus of 10 × wave (grows each boss fight)
            //  Support add   → no exp while boss is alive (block easy farming)
            //  Normal enemy  → standard per-enemy exp
            bool isBoss = (dynamic_cast<Molarbeast*>(enemy.get()) != nullptr);
            int prevLevel = _player.GetLevel();

            if (isBoss)
                _player.AddExp(10 * _wave);
            else if (!IsBossFightActive())
                _player.AddExp(enemy->GetExpValue());
            // else: support add during active boss fight — no exp

            if (_player.GetLevel() > prevLevel)
            {
                GenerateLevelUpOptions();
                _levelUpReturnState = GameState::Play;
                _levelUpOpenTimer = 0.25f;
                _gameState = GameState::LevelUpChoice;
            }
            _enemiesKilled++;
            SpawnEnemyDrop(dropPos);
            enemy->SetActive(false);
            enemy->Teleport(Vector2{ -5000.f, -5000.f });
        }
    }
}

void Engine::TriggerScreenShake(float strength, float duration)
{
    _shakeStrength = strength;
    _shakeTimer = duration;
}

void Engine::DrawWorld()
{
    // Map: top-left is _mapPos + shake
    Vector2 shakenMapPos = Vector2Add(_mapPos, _shakeOffset);

    // Props / enemies / player: worldPos - cameraRef + screenCenter
    Vector2 cameraRef = Vector2Subtract(_cameraPos, _shakeOffset);

    // Pickups / projectiles / effects: worldPos + worldOffset + screenCenter
    Vector2 worldOffset = { -_cameraPos.x + _shakeOffset.x, -_cameraPos.y + _shakeOffset.y };

    DrawTextureEx(_map, shakenMapPos, 0.0f, _mapScale, WHITE);

    for (auto& prop : _props)
        prop.Render(cameraRef);

    for (auto& pickup : _pickups)
        pickup->Draw(worldOffset);

    for (const auto& projectile : _spreadProjectiles)
        projectile.Draw(worldOffset);

    DrawUltimateBlasts(worldOffset);

    for (const auto& projectile : _swordBeamProjectiles)
        projectile.Draw(worldOffset);

    for (const auto& projectile : _freezeProjectiles)
        projectile.Draw(worldOffset);

    for (const auto& projectile : _lavaBalls)
        projectile.Draw(worldOffset);

    for (const auto& laser : _cyclopsLasers)
        laser.Draw(worldOffset);

    DrawEffects(worldOffset);

    for (auto& enemy : _enemies)
    {
        if (!enemy->IsActive())
            continue;
        enemy->DrawEnemy(cameraRef);
    }

    _player.DrawPlayer(cameraRef);
}

void Engine::DrawHUD()
{
    // ── Shared bottom-bar layout constants (mirrored in DrawAbilityBar) ───────
    static constexpr float kBarW   = 400.f;
    static constexpr float kBarH   = 28.f;
    static constexpr float kBarGap = 8.f;
    static constexpr float kBotPad = 12.f;

    const float expBarY  = (float)GetScreenHeight() - kBotPad - kBarH;
    const float manaBarY = expBarY - kBarGap - kBarH;
    const float hpBarY   = manaBarY - kBarGap - kBarH;
    const float barX     = (float)GetScreenWidth() / 2.f - kBarW / 2.f;

    // ── Top banner ────────────────────────────────────────────────────────────
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight() / 8, Fade(BLACK, 0.6f));

    DrawText(TextFormat("Time: %.1f", _gameTimer), 85 + GetScreenWidth() / 2 - 150, 60, 30, RAYWHITE);
    DrawText(("Enemies Defeated: " + std::to_string(_enemiesKilled)).c_str(), 20, 10, 30, RAYWHITE);
    DrawText(("Enemies Left: " + std::to_string(GetActiveEnemyCount())).c_str(), 20, 60, 30, RAYWHITE);

    // ── Wave display — top right ──────────────────────────────────────────────
    {
        bool isBoss = (_wave > 0 && _wave % 5 == 0);
        const char* waveLabel = isBoss
            ? TextFormat("Wave %d  - BOSS", _wave)
            : TextFormat("Wave %d", _wave);
        int waveLabelW = MeasureText(waveLabel, 32);
        DrawText(waveLabel, GetScreenWidth() - waveLabelW - 20, 20, 32,
            isBoss ? ORANGE : RAYWHITE);
    }

    // ── HP bar (bottom, above EXP) ────────────────────────────────────────────
    {
        float maxHp = _player.GetMaxHealthValue();
        float curHp = _player.GetHealthValue();
        float hpPct = (maxHp > 0.f) ? (curHp / maxHp) : 0.f;

        Color fillColor = (hpPct > 0.5f) ? GREEN :
                          (hpPct > 0.25f) ? YELLOW : RED;

        DrawRectangleRounded({ barX, hpBarY, kBarW, kBarH }, 0.3f, 6, Fade(BLACK, 0.75f));
        DrawRectangleRounded({ barX, hpBarY, kBarW * hpPct, kBarH }, 0.3f, 6, fillColor);
        DrawRectangleRoundedLines({ barX, hpBarY, kBarW, kBarH }, 0.3f, 6, Fade(WHITE, 0.25f));

        const char* hpLabel = TextFormat("HP  %.0f / %.0f", curHp, maxHp);
        int labelW = MeasureText(hpLabel, 18);
        DrawText(hpLabel,
            (int)(barX + kBarW / 2.f - labelW / 2.f),
            (int)(hpBarY + kBarH / 2.f - 9.f),
            18, RAYWHITE);
    }

    // ── Mana bar (middle) ─────────────────────────────────────────────────────
    {
        int   curMana  = _player.GetMana();
        int   maxMana  = _player.GetMaxMana();
        float manaPct  = (maxMana > 0) ? (float)curMana / (float)maxMana : 0.f;

        static const Color kManaFill = { 60, 120, 255, 230 };

        DrawRectangleRounded({ barX, manaBarY, kBarW, kBarH }, 0.3f, 6, Fade(BLACK, 0.75f));
        DrawRectangleRounded({ barX, manaBarY, kBarW * manaPct, kBarH }, 0.3f, 6, kManaFill);
        DrawRectangleRoundedLines({ barX, manaBarY, kBarW, kBarH }, 0.3f, 6, Fade(WHITE, 0.25f));

        const char* manaLabel = TextFormat("MP  %d / %d", curMana, maxMana);
        int manaLabelW = MeasureText(manaLabel, 18);
        DrawText(manaLabel,
            (int)(barX + kBarW / 2.f - manaLabelW / 2.f),
            (int)(manaBarY + kBarH / 2.f - 9.f),
            18, RAYWHITE);
    }

    // ── EXP bar (very bottom) ─────────────────────────────────────────────────
    {
        int   level    = _player.GetLevel();
        int   exp      = _player.GetExp();
        int   expToNext= _player.GetExpToNext();
        float expPct   = (level < 10 && expToNext > 0) ? (float)exp / (float)expToNext : 1.f;

        static const Color kExpFill = { 255, 210, 0, 230 };

        DrawRectangleRounded({ barX, expBarY, kBarW, kBarH }, 0.3f, 6, Fade(BLACK, 0.75f));
        if (level < 10)
            DrawRectangleRounded({ barX, expBarY, kBarW * expPct, kBarH }, 0.3f, 6, kExpFill);
        DrawRectangleRoundedLines({ barX, expBarY, kBarW, kBarH }, 0.3f, 6, Fade(WHITE, 0.25f));

        const char* levelText = (level < 10)
            ? TextFormat("Lv.%d  %d/%d EXP", level, exp, expToNext)
            : "Lv.MAX";
        int textW = MeasureText(levelText, 18);
        DrawText(levelText,
            (int)(barX + kBarW / 2.f - textW / 2.f),
            (int)(expBarY + kBarH / 2.f - 9.f),
            18, kExpFill);
    }

    DrawAbilityBar();
    DrawMiniMap();

    if (_bossWarningTimer > 0.f)
    {
        const char* warning = "DON'T GET TOO CLOSE";
        int warningSize = 34;
        int warningWidth = MeasureText(warning, warningSize);
        DrawText(warning,
            GetScreenWidth() / 2 - warningWidth / 2,
            96,
            warningSize,
            ORANGE);
    }
}

void Engine::DrawAbilityBar()
{
    // Only render slots the player has unlocked (picked up at least once)
    const int   learnedCount = _player.GetLearnedCount();
    if (learnedCount == 0)
        return;

    const float slotSize = 90.f;
    const float slotGap  = 10.f;

    // Compute slotY — must match DrawHUD() constants
    static constexpr float kBarH   = 28.f;
    static constexpr float kBarGap = 8.f;
    static constexpr float kBotPad = 12.f;
    const float expBarY  = (float)GetScreenHeight() - kBotPad - kBarH;
    const float manaBarY = expBarY - kBarGap - kBarH;
    const float hpBarY   = manaBarY - kBarGap - kBarH;
    const float slotY    = hpBarY - 12.f - slotSize;

    const float totalW = learnedCount * slotSize + (learnedCount - 1) * slotGap;
    const float startX = GetScreenWidth() / 2.f - totalW / 2.f;

    Vector2 mouse = GetMousePosition();

    for (int i = 0; i < learnedCount; i++)
    {
        AbilityType ability = _player.GetLearnedAbility(i);
        bool canCast = _player.GetMana() >= GetAbilityManaCost(ability);
        float x = startX + i * (slotSize + slotGap);
        Rectangle slot{ x, slotY, slotSize, slotSize };
        bool hovered = CheckCollisionPointRec(mouse, slot);

        // Background + border
        DrawRectangleRounded(slot, 0.18f, 6, hovered ? Fade(BLACK, 0.80f) : Fade(BLACK, 0.55f));
        DrawRectangleRoundedLines(slot, 0.18f, 6,
            hovered ? Fade(GOLD, 0.70f) :
            canCast  ? Fade(LIGHTGRAY, 0.35f) : Fade(RED, 0.40f));

        // Key label
        DrawText(GetKeyName(_player.GetAbilityKey(i)),
            (int)(x + 6.f), (int)(slotY + 6.f), 16, Fade(WHITE, 0.6f));

        // Click to cast
        if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            _player.TriggerAbilityCast(i);

        // Icon — per-element pickup sprite, dimmed when out of mana
        const Texture2D* iconTex = &_abilityIconFireTex;
        if      (ability == AbilityType::IceSpread      ||
                 ability == AbilityType::IceBolt        ||
                 ability == AbilityType::IceUltimate)    iconTex = &_abilityIconIceTex;
        else if (ability == AbilityType::ElectricSpread ||
                 ability == AbilityType::ElectricBolt   ||
                 ability == AbilityType::ElectricUltimate) iconTex = &_abilityIconElectricTex;

        Color iconTint = canCast ? WHITE : Fade(WHITE, 0.35f);
        float maxIconSize = slotSize * 0.60f;
        float iconScale   = std::min(maxIconSize / (float)iconTex->width,
                                     maxIconSize / (float)iconTex->height);
        float iw = iconTex->width  * iconScale;
        float ih = iconTex->height * iconScale;
        float cx = x + slotSize * 0.5f;
        float cy = slotY + slotSize * 0.44f;
        DrawTextureEx(*iconTex, { cx - iw * 0.5f, cy - ih * 0.5f }, 0.f, iconScale, iconTint);

        // Ability name below icon
        const char* abilityName = GetAbilityName(ability);
        int nameW = MeasureText(abilityName, 14);
        DrawText(abilityName,
            (int)(x + slotSize / 2.f - nameW / 2.f),
            (int)(slotY + slotSize - 22.f),
            14, canCast ? RAYWHITE : Fade(GRAY, 0.6f));
    }
}

void Engine::DrawWaveIntro()
{
    if (!_waveStarting)
        return;

    DrawRectangle(0, GetScreenHeight() / 2 - 80, GetScreenWidth(), 160, Fade(BLACK, 0.7f));

    int fontSize = 60;

    if (_wave == 1)
    {
        const char* line1 = "Objective:";
        const char* line2 = "Survive";

        int w1 = MeasureText(line1, fontSize);
        int w2 = MeasureText(line2, fontSize);

        DrawText(line1, GetScreenWidth() / 2 - w1 / 2, GetScreenHeight() / 2 - 60, fontSize, YELLOW);
        DrawText(line2, GetScreenWidth() / 2 - w2 / 2, GetScreenHeight() / 2 + 10, fontSize, YELLOW);
    }
    else
    {
        bool isBossWave = (_wave % 5 == 0);

        std::string waveText = "Wave " + std::to_string(_wave);
        int textWidth = MeasureText(waveText.c_str(), fontSize);

        int waveY = isBossWave ? GetScreenHeight() / 2 - 55 : GetScreenHeight() / 2 - 30;
        DrawText(waveText.c_str(), GetScreenWidth() / 2 - textWidth / 2, waveY, fontSize, YELLOW);

        if (isBossWave)
        {
            const char* bossLine = "- Boss Incoming -";
            int bossLineW = MeasureText(bossLine, fontSize);
            DrawText(bossLine, GetScreenWidth() / 2 - bossLineW / 2, waveY + fontSize + 10, fontSize, RED);
        }
    }
}

void Engine::GenerateStartingAbilityOptions()
{
    // Pool of all 6 abilities (3 spread + 3 bolt). Shuffle and show 3 so the
    // player always sees a varied starting choice.
    UpgradeType pool[6] = {
        UpgradeType::LearnFireSpread,
        UpgradeType::LearnIceSpread,
        UpgradeType::LearnElectricSpread,
        UpgradeType::LearnFireBolt,
        UpgradeType::LearnIceBolt,
        UpgradeType::LearnElectricBolt,
    };
    for (int i = 0; i < 6; i++)
    {
        int j = GetRandomValue(i, 5);
        UpgradeType tmp = pool[i]; pool[i] = pool[j]; pool[j] = tmp;
    }
    _levelUpOptions[0] = pool[0];
    _levelUpOptions[1] = pool[1];
    _levelUpOptions[2] = pool[2];
}

void Engine::GenerateLevelUpOptions()
{
    // Build pool of stat boosts + unlearned abilities
    UpgradeType pool[15];
    int poolSize = 0;

    // Always include all 6 stat upgrades
    pool[poolSize++] = UpgradeType::AttackPower;
    pool[poolSize++] = UpgradeType::AttackRange;
    pool[poolSize++] = UpgradeType::MaxHealth;
    pool[poolSize++] = UpgradeType::MaxMana;
    pool[poolSize++] = UpgradeType::Defense;
    pool[poolSize++] = UpgradeType::MoveSpeed;

    // Add ability unlocks the player hasn't learned yet (and has a free slot for)
    if (!_player.HasLearnedAbility(AbilityType::FireSpread) &&
        _player.GetLearnedCount() < _player.GetMaxAbilitySlots())
        pool[poolSize++] = UpgradeType::LearnFireSpread;

    if (!_player.HasLearnedAbility(AbilityType::IceSpread) &&
        _player.GetLearnedCount() < _player.GetMaxAbilitySlots())
        pool[poolSize++] = UpgradeType::LearnIceSpread;

    if (!_player.HasLearnedAbility(AbilityType::ElectricSpread) &&
        _player.GetLearnedCount() < _player.GetMaxAbilitySlots())
        pool[poolSize++] = UpgradeType::LearnElectricSpread;

    if (!_player.HasLearnedAbility(AbilityType::FireBolt) &&
        _player.GetLearnedCount() < _player.GetMaxAbilitySlots())
        pool[poolSize++] = UpgradeType::LearnFireBolt;

    if (!_player.HasLearnedAbility(AbilityType::IceBolt) &&
        _player.GetLearnedCount() < _player.GetMaxAbilitySlots())
        pool[poolSize++] = UpgradeType::LearnIceBolt;

    if (!_player.HasLearnedAbility(AbilityType::ElectricBolt) &&
        _player.GetLearnedCount() < _player.GetMaxAbilitySlots())
        pool[poolSize++] = UpgradeType::LearnElectricBolt;

    // Ultimates have their own dedicated row at level 3 — keep them out of here.

    // Fisher-Yates shuffle first 3
    for (int i = 0; i < 3 && i < poolSize; i++)
    {
        int j = GetRandomValue(i, poolSize - 1);
        UpgradeType tmp = pool[i]; pool[i] = pool[j]; pool[j] = tmp;
    }

    _levelUpOptions[0] = pool[0];
    _levelUpOptions[1] = (poolSize > 1) ? pool[1] : pool[0];
    _levelUpOptions[2] = (poolSize > 2) ? pool[2] : pool[0];

    // Show the ultimate row only at level 3
    _showUltimateRow   = (_player.GetLevel() == 3);
    _ultimateRowPicked = false;
    _regularRowPicked  = false;
}

void Engine::DrawLevelUpChoice()
{
    const float sw = (float)GetScreenWidth();
    const float sh = (float)GetScreenHeight();

    // Dim overlay
    DrawRectangle(0, 0, (int)sw, (int)sh, Fade(BLACK, 0.65f));

    // Determine which rows are still visible
    bool showUlt = _showUltimateRow && !_ultimateRowPicked && !_awaitingStartingAbility;
    bool showReg = !_regularRowPicked;

    // Title
    const char* title;
    if (_awaitingStartingAbility)
        title = "Choose your starting ability:";
    else if (showUlt && showReg)
        title = "LEVEL UP!  Choose an Ultimate AND an Upgrade:";
    else if (showUlt)
        title = "Now choose your Ultimate:";
    else if (showReg)
        title = "Now choose an Upgrade:";
    else
        title = "LEVEL UP!";

    int titleSz = 44;
    int titleW  = MeasureText(title, titleSz);
    DrawText(title, (int)(sw / 2.f - titleW / 2.f), (int)(sh * 0.06f), titleSz, GOLD);

    // Card layout
    const float cardW   = 280.f;
    const float cardH   = 360.f;
    const float cardGap = 40.f;
    const float totalW  = 3.f * cardW + 2.f * cardGap;
    const float startX  = sw / 2.f - totalW / 2.f;

    // Row Y positions — stack both rows when both visible, otherwise center the lone row
    float ultRowY, regRowY;
    if (showUlt && showReg)
    {
        float totalH = cardH * 2.f + 50.f;
        float topY   = sh / 2.f - totalH / 2.f;
        ultRowY = topY;
        regRowY = topY + cardH + 50.f;
    }
    else
    {
        ultRowY = sh / 2.f - cardH / 2.f;
        regRowY = sh / 2.f - cardH / 2.f;
    }

    // Use cardY to keep the rest of the code below working for the regular row
    const float cardY = regRowY;

    // Map UpgradeType to display info
    auto getUpgradeInfo = [&](UpgradeType type, const char*& name, const char*& desc, Texture2D*& icon)
    {
        switch (type)
        {
        case UpgradeType::AttackPower:
            name = "Attack Power";
            desc = "+8% melee\ndamage";
            icon = &_upgradeAttackPowerTex;
            break;
        case UpgradeType::AttackRange:
            name = "Attack Range";
            desc = "+10% melee\nreach";
            icon = &_upgradeAttackRangeTex;
            break;
        case UpgradeType::MaxHealth:
            name = "Max Health";
            desc = "+12% max HP\n(heals too)";
            icon = &_upgradeHealthTex;
            break;
        case UpgradeType::MaxMana:
            name = "Max Mana";
            desc = "+20 max MP";
            icon = &_upgradeMagicTex;
            break;
        case UpgradeType::Defense:
            name = "Defense";
            desc = "+5% damage\nreduction";
            icon = &_upgradeDefenseTex;
            break;
        case UpgradeType::MoveSpeed:
            name = "Move Speed";
            desc = "+8% movement\nspeed";
            icon = &_upgradeMoveSpeedTex;
            break;
        case UpgradeType::LearnFireSpread:
            name = "Fire Spread";
            desc = "8 fireballs\nburn on hit";
            icon = &_abilityIconFireTex;
            break;
        case UpgradeType::LearnIceSpread:
            name = "Ice Spread";
            desc = "8 ice shards\nfreeze on hit";
            icon = &_abilityIconIceTex;
            break;
        case UpgradeType::LearnElectricSpread:
            name = "Electric Spread";
            desc = "8 bolts\nstun randomly";
            icon = &_abilityIconElectricTex;
            break;
        case UpgradeType::LearnFireBolt:
            name = "Fire Bolt";
            desc = "Aimed fireball\nhigh damage + burn";
            icon = &_abilityIconFireTex;
            break;
        case UpgradeType::LearnIceBolt:
            name = "Ice Bolt";
            desc = "Aimed shard\nhigh damage + freeze";
            icon = &_abilityIconIceTex;
            break;
        case UpgradeType::LearnElectricBolt:
            name = "Electric Bolt";
            desc = "Aimed bolt\nhigh damage + stun";
            icon = &_abilityIconElectricTex;
            break;
        case UpgradeType::LearnFireUltimate:
            name = "Fire Ultimate";
            desc = "Blasts everywhere\n4 dmg + burn, all MP";
            icon = &_abilityIconFireTex;
            break;
        case UpgradeType::LearnIceUltimate:
            name = "Ice Ultimate";
            desc = "Blasts everywhere\n4 dmg + freeze, all MP";
            icon = &_abilityIconIceTex;
            break;
        case UpgradeType::LearnElectricUltimate:
            name = "Elec. Ultimate";
            desc = "Blasts everywhere\n4 dmg + stun, all MP";
            icon = &_abilityIconElectricTex;
            break;
        default:
            name = "???";
            desc = "";
            icon = &_upgradeAttackPowerTex;
            break;
        }
    };

    Vector2 mouse = GetMousePosition();
    bool ready = (_levelUpOpenTimer <= 0.f);

    // ── Ultimate row (level 3 only, top) ─────────────────────────────────────
    if (showUlt)
    {
        // Row label
        int lblSz = 22;
        const char* lbl = "- Elemental Ultimate -";
        DrawText(lbl, (int)(sw / 2.f - MeasureText(lbl, lblSz) / 2.f),
                 (int)(ultRowY - lblSz - 8.f), lblSz, Color{255,200,80,200});

        for (int i = 0; i < 3; i++)
        {
            float x = startX + i * (cardW + cardGap);
            Rectangle card{ x, ultRowY, cardW, cardH };
            bool hovered = ready && CheckCollisionPointRec(mouse, card);

            // Gold-tinted card background to distinguish from regular row
            Color bgColor = hovered ? Color{55, 40, 10, 230} : Color{35, 25, 5, 215};
            DrawRectangleRounded(card, 0.12f, 8, bgColor);
            DrawRectangleRoundedLines(card, 0.12f, 8,
                hovered ? Color{255,180,0,255} : Color{200,140,30,160});

            const char* name = ""; const char* desc = ""; Texture2D* icon = nullptr;
            getUpgradeInfo(_levelUpUltimateOptions[i], name, desc, icon);

            // Icon / pattern area
            {
                const float iconAreaSize = cardW * 0.52f;
                const float previewCX    = x + cardW / 2.f;
                const float previewCY    = ultRowY + cardH * 0.10f + iconAreaSize / 2.f;

                UpgradeType opt = _levelUpUltimateOptions[i];
                Color ec =
                    (opt == UpgradeType::LearnFireUltimate)     ? Color{255,110, 20,255} :
                    (opt == UpgradeType::LearnIceUltimate)      ? Color{100,210,255,255} :
                                                                  Color{255,220, 30,255};
                Color ecDim = {ec.r, ec.g, ec.b, 80};
                const float outerR = iconAreaSize * 0.44f;
                DrawCircleLinesV({previewCX, previewCY}, outerR, ecDim);
                const float dotAngles[10] = {0.f,36.f,72.f,108.f,144.f,180.f,216.f,252.f,288.f,324.f};
                const float dotDists[10]  = {0.55f,0.80f,0.65f,0.90f,0.50f,0.75f,0.85f,0.60f,0.95f,0.70f};
                for (int d = 0; d < 10; d++)
                {
                    float rad  = dotAngles[d] * DEG2RAD;
                    float dist = outerR * dotDists[d];
                    Vector2 dp = {previewCX + cosf(rad)*dist, previewCY + sinf(rad)*dist};
                    DrawCircleV(dp, 5.f, ecDim);
                    DrawCircleV(dp, 3.f, ec);
                }
                if (icon && icon->id != 0)
                {
                    float iconTarget = (opt == UpgradeType::LearnFireUltimate) ? 44.f : 72.f;
                    float iconScale  = std::min(iconTarget/(float)icon->width, iconTarget/(float)icon->height);
                    float iw = icon->width * iconScale; float ih = icon->height * iconScale;
                    DrawTextureEx(*icon, {previewCX - iw*0.5f, previewCY - ih*0.5f}, 0.f, iconScale, WHITE);
                }
            }

            // Name
            int nameSz = 26;
            int nameW  = MeasureText(name, nameSz);
            DrawText(name, (int)(x + cardW/2.f - nameW/2.f),
                     (int)(ultRowY + cardH*0.58f), nameSz, hovered ? GOLD : RAYWHITE);

            // Description
            std::string descStr = desc;
            int nlPos = (int)descStr.find('\n');
            int descSz = 20;
            if (nlPos != (int)std::string::npos)
            {
                std::string line1 = descStr.substr(0, nlPos);
                std::string line2 = descStr.substr(nlPos + 1);
                DrawText(line1.c_str(), (int)(x + cardW/2.f - MeasureText(line1.c_str(),descSz)/2.f),
                         (int)(ultRowY + cardH*0.72f), descSz, LIGHTGRAY);
                DrawText(line2.c_str(), (int)(x + cardW/2.f - MeasureText(line2.c_str(),descSz)/2.f),
                         (int)(ultRowY + cardH*0.72f + descSz + 4), descSz, LIGHTGRAY);
            }
            else
            {
                DrawText(desc, (int)(x + cardW/2.f - MeasureText(desc,descSz)/2.f),
                         (int)(ultRowY + cardH*0.72f), descSz, LIGHTGRAY);
            }

            if (ready && hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                _player.ApplyUpgrade(_levelUpUltimateOptions[i]);
                _ultimateRowPicked = true;
                if (_regularRowPicked)
                    _gameState = _levelUpReturnState;
            }
        }
    }

    // ── Regular row ──────────────────────────────────────────────────────────
    if (showReg)
    {
        // Row label when both rows are visible
        if (showUlt)
        {
            int lblSz = 22;
            const char* lbl = "- Upgrade -";
            DrawText(lbl, (int)(sw / 2.f - MeasureText(lbl, lblSz) / 2.f),
                     (int)(regRowY - lblSz - 8.f), lblSz, Color{200,200,200,180});
        }

    for (int i = 0; i < 3; i++)
    {
        float x = startX + i * (cardW + cardGap);
        Rectangle card{ x, cardY, cardW, cardH };

        bool hovered = ready && CheckCollisionPointRec(mouse, card);

        // Card background
        Color bgColor = hovered ? Color{ 50, 45, 25, 230 } : Color{ 25, 22, 12, 210 };
        DrawRectangleRounded(card, 0.12f, 8, bgColor);
        DrawRectangleRoundedLines(card, 0.12f, 8,
            hovered ? Color{ 255, 200, 0, 220 } : Color{ 180, 160, 80, 120 });

        const char* name = "";
        const char* desc = "";
        Texture2D* icon  = nullptr;
        getUpgradeInfo(_levelUpOptions[i], name, desc, icon);

        // Icon area — ability unlocks get a drawn attack-pattern preview so the
        // player can immediately see "burst vs single shot". Stat upgrades keep
        // their regular texture icon.
        {
            const float iconAreaSize = cardW * 0.52f;
            const float previewCX    = x + cardW / 2.f;
            const float previewCY    = cardY + cardH * 0.10f + iconAreaSize / 2.f;

            UpgradeType opt = _levelUpOptions[i];
            bool isSpreadAbility   = (opt == UpgradeType::LearnFireSpread  ||
                                      opt == UpgradeType::LearnIceSpread   ||
                                      opt == UpgradeType::LearnElectricSpread);
            bool isBoltAbility     = (opt == UpgradeType::LearnFireBolt    ||
                                      opt == UpgradeType::LearnIceBolt     ||
                                      opt == UpgradeType::LearnElectricBolt);
            bool isUltimateAbility = (opt == UpgradeType::LearnFireUltimate    ||
                                      opt == UpgradeType::LearnIceUltimate     ||
                                      opt == UpgradeType::LearnElectricUltimate);

            if (isSpreadAbility || isBoltAbility || isUltimateAbility)
            {
                // Pick element colour
                Color ec =
                    (opt == UpgradeType::LearnFireSpread    || opt == UpgradeType::LearnFireBolt    || opt == UpgradeType::LearnFireUltimate)     ? Color{255, 110,  20, 255} :
                    (opt == UpgradeType::LearnIceSpread     || opt == UpgradeType::LearnIceBolt     || opt == UpgradeType::LearnIceUltimate)      ? Color{100, 210, 255, 255} :
                                                                                                                                                    Color{255, 220,  30, 255};
                Color ecDim = { ec.r, ec.g, ec.b, 80 };

                if (isSpreadAbility)
                {
                    // 8 radiating arrows from a centre point
                    const float armLen  = iconAreaSize * 0.40f;
                    const float headLen = 10.f;
                    const float headW   = 6.f;
                    const float angles[8] = { 0.f, 45.f, 90.f, 135.f, 180.f, 225.f, 270.f, 315.f };

                    for (float deg : angles)
                    {
                        float rad  = deg * DEG2RAD;
                        float cx2  = cosf(rad);
                        float cy2  = sinf(rad);
                        Vector2 shaft0 = { previewCX + cx2 * 22.f, previewCY + cy2 * 22.f };
                        Vector2 tip    = { previewCX + cx2 * armLen, previewCY + cy2 * armLen };

                        DrawLineEx(shaft0, tip, 2.2f, ec);

                        Vector2 perpL = { -cy2, cx2 };
                        Vector2 hb1 = { tip.x - cx2 * headLen + perpL.x * headW,
                                        tip.y - cy2 * headLen + perpL.y * headW };
                        Vector2 hb2 = { tip.x - cx2 * headLen - perpL.x * headW,
                                        tip.y - cy2 * headLen - perpL.y * headW };
                        DrawTriangle(tip, hb1, hb2, ec);
                    }
                }
                else if (isBoltAbility)
                {
                    // Single large arrow aimed to the right
                    const float armLen  = iconAreaSize * 0.38f;
                    const float headLen = 18.f;
                    const float headW   = 11.f;

                    Vector2 shaftStart = { previewCX - armLen * 0.55f, previewCY };
                    Vector2 tip        = { previewCX + armLen,          previewCY };
                    DrawLineEx(shaftStart, tip, 4.5f, ec);

                    Vector2 hb1 = { tip.x - headLen, tip.y - headW };
                    Vector2 hb2 = { tip.x - headLen, tip.y + headW };
                    DrawTriangle(tip, hb1, hb2, ec);
                }
                else // ultimate
                {
                    // Scattered burst: outer ring + 10 dots at irregular distances
                    // to suggest "explosions everywhere". Positions are deterministic
                    // so they don't flicker each frame.
                    const float outerR = iconAreaSize * 0.44f;
                    DrawCircleLinesV({ previewCX, previewCY }, outerR, ecDim);

                    const float dotAngles[10] = { 0.f, 36.f, 72.f, 108.f, 144.f, 180.f, 216.f, 252.f, 288.f, 324.f };
                    const float dotDists[10]  = { 0.55f, 0.80f, 0.65f, 0.90f, 0.50f, 0.75f, 0.85f, 0.60f, 0.95f, 0.70f };

                    for (int d = 0; d < 10; d++)
                    {
                        float rad  = dotAngles[d] * DEG2RAD;
                        float dist = outerR * dotDists[d];
                        Vector2 dp = { previewCX + cosf(rad) * dist, previewCY + sinf(rad) * dist };
                        DrawCircleV(dp, 5.f, ecDim);
                        DrawCircleV(dp, 3.f, ec);
                    }
                }

                // Icon centred on top of the arrow pattern
                if (icon && icon->id != 0)
                {
                    // Fire icon reads clearly at a smaller size; ice and electric
                    // sprites are visually smaller so give them more room.
                    float iconTarget =
                        (opt == UpgradeType::LearnIceSpread      || opt == UpgradeType::LearnIceBolt      || opt == UpgradeType::LearnIceUltimate      ||
                         opt == UpgradeType::LearnElectricSpread || opt == UpgradeType::LearnElectricBolt || opt == UpgradeType::LearnElectricUltimate)
                        ? 72.f : 44.f;
                    float iconScale = std::min(iconTarget / (float)icon->width,
                                               iconTarget / (float)icon->height);
                    float iw = icon->width  * iconScale;
                    float ih = icon->height * iconScale;
                    DrawTextureEx(*icon, { previewCX - iw * 0.5f, previewCY - ih * 0.5f },
                                  0.f, iconScale, WHITE);
                }
            }
            else if (icon && icon->id != 0)
            {
                // Stat upgrade — draw the regular texture icon
                float iconScale = std::min(iconAreaSize / (float)icon->width,
                                           iconAreaSize / (float)icon->height);
                float iconW = icon->width  * iconScale;
                float iconH = icon->height * iconScale;
                float iconX = previewCX - iconW / 2.f;
                float iconY = cardY + cardH * 0.10f + (iconAreaSize - iconH) / 2.f;
                DrawTextureEx(*icon, { iconX, iconY }, 0.f, iconScale, WHITE);
            }
        }

        // Name
        int nameSz = 26;
        int nameW  = MeasureText(name, nameSz);
        DrawText(name,
            (int)(x + cardW / 2.f - nameW / 2.f),
            (int)(cardY + cardH * 0.58f),
            nameSz, hovered ? GOLD : RAYWHITE);

        // Description — manual newline split
        std::string descStr = desc;
        int nlPos = (int)descStr.find('\n');
        int descSz = 20;
        if (nlPos != (int)std::string::npos)
        {
            std::string line1 = descStr.substr(0, nlPos);
            std::string line2 = descStr.substr(nlPos + 1);
            int l1W = MeasureText(line1.c_str(), descSz);
            int l2W = MeasureText(line2.c_str(), descSz);
            DrawText(line1.c_str(),
                (int)(x + cardW / 2.f - l1W / 2.f),
                (int)(cardY + cardH * 0.72f), descSz, LIGHTGRAY);
            DrawText(line2.c_str(),
                (int)(x + cardW / 2.f - l2W / 2.f),
                (int)(cardY + cardH * 0.72f + descSz + 4), descSz, LIGHTGRAY);
        }
        else
        {
            int dW = MeasureText(desc, descSz);
            DrawText(desc,
                (int)(x + cardW / 2.f - dW / 2.f),
                (int)(cardY + cardH * 0.72f), descSz, LIGHTGRAY);
        }

        // Click to select — blocked until open timer expires
        if (ready && hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            _player.ApplyUpgrade(_levelUpOptions[i]);
            _awaitingStartingAbility = false;
            _regularRowPicked = true;
            if (!_showUltimateRow || _ultimateRowPicked)
                _gameState = _levelUpReturnState;
        }
    }
    } // end showReg

    // Hint / loading cue
    const char* hint;
    if (_levelUpOpenTimer > 0.f)
        hint = "Loading...";
    else if (showUlt && showReg)
        hint = "Choose one from each row";
    else
        hint = "Click a card to choose";

    Color hintColor = (_levelUpOpenTimer > 0.f) ? Fade(GRAY, 0.6f) : Fade(LIGHTGRAY, 0.7f);
    float hintRefY  = showReg ? (regRowY + cardH + 28.f) : (ultRowY + cardH + 28.f);
    int hintW = MeasureText(hint, 22);
    DrawText(hint, (int)(sw / 2.f - hintW / 2.f), (int)hintRefY, 22, hintColor);
}

void Engine::HandlePlayerMeleeDamage()
{
    if (!_player.CanApplyMeleeDamage())
        return;

    bool hitAny = false;
    Rectangle attackRec = _player.GetAttackCollisionRec();

    for (auto& enemy : _enemies)
    {
        if (!enemy->IsActive())
            continue;
        if (!enemy->IsAlive())
            continue;

        if (CheckCollisionRecs(attackRec, enemy->GetHitCollisionRec()))
        {
            enemy->TakeDamage(_player.GetMeleeDamage(), _player.GetWorldPos());
            SpawnHitEffect(Character::CastType::None, enemy->GetWorldPos(), _player.GetFacingDirection());
            hitAny = true;
        }
    }

    _player.ConsumeMeleeDamageFrame();

    if (hitAny)
        TriggerScreenShake(6.f, 0.07f);
}

void Engine::SpawnSpreadBurst(AbilityType element)
{
    static const std::array<Vector2, 8> directions{
        Vector2{  1.f,  0.f },
        Vector2{ -1.f,  0.f },
        Vector2{  0.f,  1.f },
        Vector2{  0.f, -1.f },
        Vector2{  1.f,  1.f },
        Vector2{  1.f, -1.f },
        Vector2{ -1.f,  1.f },
        Vector2{ -1.f, -1.f }
    };

    Vector2 origin = _player.GetCastOrigin();

    for (const Vector2& dir : directions)
    {
        SpreadProjectile projectile;
        projectile.Init(origin, dir, element);
        _spreadProjectiles.push_back(projectile);
    }
}

void Engine::SpawnBolt(AbilityType element)
{
    SpreadProjectile projectile;
    projectile.Init(_player.GetCastOrigin(), _player.GetFacingDirection(), element);
    _spreadProjectiles.push_back(projectile);
}

void Engine::SpawnUltimateBurst(AbilityType element)
{
    // Fill the entire visible screen with blasts. Positions are computed in
    // screen space then converted to world space so they always cover the
    // full 1920x1080 view regardless of where the player is.
    const int   blastCount = 25;
    const float lifetime   = _ultCinematicDuration;
    const float margin     = 80.f;   // keep icons away from the very edge

    const float sw = (float)GetScreenWidth();
    const float sh = (float)GetScreenHeight();
    Vector2 playerPos = _player.GetWorldPos();

    for (int i = 0; i < blastCount; i++)
    {
        float sx = (float)GetRandomValue((int)margin, (int)(sw - margin));
        float sy = (float)GetRandomValue((int)margin, (int)(sh - margin));

        // Convert screen-space position to world-space so DrawUltimateBlasts
        // maps it back to the same screen pixel via the worldOffset calculation.
        Vector2 pos = {
            playerPos.x + sx - sw * 0.5f,
            playerPos.y + sy - sh * 0.5f
        };

        // Angle each blast outward from the screen center so they look like
        // they're radiating away from the player.
        float rotation = atan2f(sy - sh * 0.5f, sx - sw * 0.5f) * RAD2DEG;

        UltimateBlast blast;
        blast.worldPos  = pos;
        blast.element   = element;
        blast.lifetime  = lifetime;
        blast.timer     = lifetime;
        blast.rotation  = rotation;
        _ultimateBlasts.push_back(blast);
    }

    TriggerScreenShake(6.f, 0.3f);
}

void Engine::UpdateUltimateBlasts(float dt)
{
    // Purely visual during the cinematic phase — damage is applied all at once
    // by ApplyUltimateImpact at the Impact phase transition.
    for (auto& blast : _ultimateBlasts)
        blast.timer -= dt;

    _ultimateBlasts.erase(
        std::remove_if(_ultimateBlasts.begin(), _ultimateBlasts.end(),
            [](const UltimateBlast& b) { return b.timer <= 0.f; }),
        _ultimateBlasts.end());
}

void Engine::TriggerUltimateSequence(AbilityType element)
{
    _ultimatePhase       = UltimatePhase::WindUp;
    _ultimatePhaseTimer  = 0.f;
    _ultimateCircleAngle = 0.f;
    _ultimateElement     = element;
    _ultimateBlasts.clear();
    StopSound(_fireballCastSound);
    PlaySound(_fireballCastSound);
}

void Engine::UpdateUltimateSequence(float dt)
{
    _ultimatePhaseTimer  += dt;
    _ultimateCircleAngle += dt * 2.2f;

    switch (_ultimatePhase)
    {
    case UltimatePhase::WindUp:
        if (_ultimatePhaseTimer >= _ultWindUpDuration)
        {
            _ultimatePhase      = UltimatePhase::Cinematic;
            _ultimatePhaseTimer = 0.f;
            SpawnUltimateBurst(_ultimateElement);
        }
        break;

    case UltimatePhase::Cinematic:
        UpdateUltimateBlasts(dt);
        if (_ultimatePhaseTimer >= _ultCinematicDuration)
        {
            _ultimatePhase      = UltimatePhase::Impact;
            _ultimatePhaseTimer = 0.f;
            ApplyUltimateImpact();
            _ultimateBlasts.clear();
            TriggerScreenShake(14.f, 0.45f);
            StopSound(_explosionSound);
            PlaySound(_explosionSound);
        }
        break;

    case UltimatePhase::Impact:
        if (_ultimatePhaseTimer >= _ultImpactDuration)
        {
            _ultimatePhase      = UltimatePhase::Release;
            _ultimatePhaseTimer = 0.f;
        }
        break;

    case UltimatePhase::Release:
        if (_ultimatePhaseTimer >= _ultReleaseDuration)
        {
            _ultimatePhase      = UltimatePhase::None;
            _ultimatePhaseTimer = 0.f;
        }
        break;

    default:
        break;
    }
}

void Engine::ApplyUltimateImpact()
{
    // Hit every enemy currently visible on screen (with a small margin beyond edges).
    const float halfW = GetScreenWidth()  * 0.55f;
    const float halfH = GetScreenHeight() * 0.55f;
    Vector2 playerPos = _player.GetWorldPos();

    for (auto& enemy : _enemies)
    {
        if (!enemy->IsActive() || !enemy->IsAlive())
            continue;

        Vector2 delta = Vector2Subtract(enemy->GetWorldPos(), playerPos);
        if (fabsf(delta.x) > halfW || fabsf(delta.y) > halfH)
            continue;

        int dmg = enemy->AsMolarbeast() ? 2 : 4;
        enemy->TakeDamage(dmg, playerPos);

        if (_ultimateElement == AbilityType::FireUltimate)
            enemy->ApplyBurn(0.5f, _player.GetBoltBurnDamage(), playerPos);
        else if (_ultimateElement == AbilityType::IceUltimate)
            enemy->ApplyFreeze(3.f);
        else if (_ultimateElement == AbilityType::ElectricUltimate)
            enemy->ApplyElectricCharge();
    }
}

void Engine::DrawUltimateSequence()
{
    if (_ultimatePhase == UltimatePhase::None)
        return;

    const float sw = (float)GetScreenWidth();
    const float sh = (float)GetScreenHeight();
    const float cx = sw * 0.5f;
    const float cy = sh * 0.5f;

    Color ec =
        (_ultimateElement == AbilityType::FireUltimate)     ? Color{255, 110,  20, 255} :
        (_ultimateElement == AbilityType::IceUltimate)      ? Color{ 80, 200, 255, 255} :
                                                               Color{255, 220,   0, 255};

    // ── Dark overlay (ramps up during wind-up, holds, fades on release) ───────
    if (_ultimatePhase != UltimatePhase::Impact)
    {
        float alpha =
            (_ultimatePhase == UltimatePhase::WindUp)
                ? (_ultimatePhaseTimer / _ultWindUpDuration) * 0.55f :
            (_ultimatePhase == UltimatePhase::Release)
                ? 0.55f * (1.f - _ultimatePhaseTimer / _ultReleaseDuration)
                : 0.55f;
        DrawRectangle(0, 0, (int)sw, (int)sh, Fade(BLACK, alpha));
    }

    // ── Impact flash ─────────────────────────────────────────────────────────
    if (_ultimatePhase == UltimatePhase::Impact)
    {
        float t = 1.f - (_ultimatePhaseTimer / _ultImpactDuration);
        DrawRectangle(0, 0, (int)sw, (int)sh, Fade(WHITE, t * 0.85f));
        // Expanding ring in element colour
        Color ringCol = { ec.r, ec.g, ec.b, (unsigned char)(t * 210.f) };
        DrawCircleLinesV({ cx, cy }, (1.f - t) * sw * 0.9f, ringCol);
    }

    // ── Magic circle (wind-up + cinematic) ───────────────────────────────────
    if (_ultimatePhase == UltimatePhase::WindUp || _ultimatePhase == UltimatePhase::Cinematic)
    {
        float progress = (_ultimatePhase == UltimatePhase::WindUp)
            ? _ultimatePhaseTimer / _ultWindUpDuration : 1.f;

        const float radii[3]  = { 75.f, 120.f, 165.f };
        const float speeds[3] = { 1.0f, -0.65f, 0.45f };

        for (int r = 0; r < 3; r++)
        {
            float radius = radii[r] * progress;
            float angle  = _ultimateCircleAngle * speeds[r];
            unsigned char a = (unsigned char)(190.f * progress);
            Color ringCol = { ec.r, ec.g, ec.b, a };

            DrawCircleLinesV({ cx, cy }, radius, ringCol);

            // 4 glowing markers rotating around each ring
            for (int m = 0; m < 4; m++)
            {
                float ma = angle + m * (PI * 0.5f);
                Vector2 mp = { cx + cosf(ma) * radius, cy + sinf(ma) * radius };
                DrawCircleV(mp, 5.f * progress, ringCol);
            }
        }

        // Soft glow around the player's screen position
        DrawCircleV({ cx, cy }, 48.f * progress,
            { ec.r, ec.g, ec.b, (unsigned char)(85.f * progress) });
    }
}

void Engine::DrawUltimateBlasts(Vector2 worldOffset)
{
    for (const auto& blast : _ultimateBlasts)
    {
        if (blast.timer <= 0.f)
            continue;

        // Progress 0→1 as blast ages
        float progress = 1.f - (blast.timer / blast.lifetime);

        // Pulse envelope: pop in fast, hold, fade out
        float pulse;
        if      (progress < 0.3f)  pulse = progress / 0.3f;
        else if (progress < 0.75f) pulse = 1.f;
        else                       pulse = 1.f - (progress - 0.75f) / 0.25f;

        if (pulse <= 0.f) continue;

        // Animated sprite sheet — ultimate types use their own 64x64 sheets
        const Texture2D& tex = SpreadProjectile::GetAnimTexture(blast.element);

        const float fw        = (float)SpreadProjectile::GetFrameWFor(blast.element);
        const float fh        = (float)SpreadProjectile::GetFrameHFor(blast.element);
        const int   frameCount = SpreadProjectile::GetFrameCountFor(blast.element);

        float elapsed = blast.lifetime - blast.timer;
        int   frame   = (int)(elapsed / (1.f / 16.f)) % frameCount;

        Rectangle src = GetAnimationFrameRect(tex, (int)fw, (int)fh, frame);

        const float maxSize = 200.f;
        float size = maxSize * pulse;

        Vector2 screenPos = {
            blast.worldPos.x + worldOffset.x + GetScreenWidth()  * 0.5f,
            blast.worldPos.y + worldOffset.y + GetScreenHeight() * 0.5f
        };

        // Fixed facing direction per blast — no spinning
        float rotation = blast.rotation;

        Rectangle dest     = { screenPos.x, screenPos.y, size, size };
        Vector2   origin   = { size * 0.5f, size * 0.5f };
        unsigned char alpha = (unsigned char)(pulse * 255.f);

        // Soft glow behind — slightly larger, low alpha
        float     glowSize   = size * 1.65f;
        Rectangle glowDest   = { screenPos.x, screenPos.y, glowSize, glowSize };
        Vector2   glowOrigin = { glowSize * 0.5f, glowSize * 0.5f };
        DrawTexturePro(tex, src, glowDest, glowOrigin, rotation,
            { 255, 255, 255, (unsigned char)(pulse * 55.f) });

        // Main animated frame
        DrawTexturePro(tex, src, dest, origin, rotation, { 255, 255, 255, alpha });
    }
}

void Engine::SpawnSwordBeam()
{
    SwordBeamProjectile projectile;
    projectile.Init(_player.GetCastOrigin(), _player.GetFacingDirection());
    _swordBeamProjectiles.push_back(projectile);
}

void Engine::SpawnFreezeWave()
{
    FreezeProjectile projectile;
    projectile.Init(_player.GetCastOrigin(), _player.GetFacingDirection());
    _freezeProjectiles.push_back(projectile);
}

void Engine::UpdateEffects(float dt)
{
    for (auto& effect : _effects)
    {
        if (!effect.active)
            continue;

        effect.runningTime += dt;
        if (effect.runningTime >= effect.frameTime)
        {
            effect.runningTime = 0.f;
            effect.frame++;

            if (effect.frame >= effect.frameCount)
                effect.active = false;
        }
    }

    _effects.erase(
        std::remove_if(_effects.begin(), _effects.end(),
            [](const AnimatedEffect& effect) { return !effect.active; }),
        _effects.end());
}

void Engine::DrawEffects(Vector2 worldOffset)
{
    for (const auto& effect : _effects)
    {
        if (!effect.active || effect.texture == nullptr || effect.texture->id == 0)
            continue;

        Vector2 worldPos = effect.followPlayer
            ? (effect.followPlayerCenter
                ? Vector2Add(_player.GetWorldPos(), Vector2{ effect.offset.x, effect.offset.y })
                : Vector2Add(_player.GetCastOrigin(), Vector2{ effect.offset.x, effect.offset.y }))
            : effect.worldPos;

        Vector2 screenPos = Vector2Add(worldPos, worldOffset);
        screenPos.x += GetScreenWidth() / 2.f;
        screenPos.y += GetScreenHeight() / 2.f;

        float rotation = atan2f(effect.direction.y, effect.direction.x) * RAD2DEG;
        Rectangle source = GetAnimationFrameRect(*effect.texture, effect.frameWidth, effect.frameHeight, effect.frame);
        Rectangle dest{
            screenPos.x,
            screenPos.y,
            effect.frameWidth * effect.scale,
            effect.frameHeight * effect.scale
        };

        DrawTexturePro(*effect.texture, source, dest,
            Vector2{ dest.width * 0.5f, dest.height * 0.5f }, rotation, effect.tint);
    }
}

void Engine::SpawnCastEffect(Character::CastType castType)
{
    AnimatedEffect effect{};
    effect.followPlayer = true;
    effect.worldPos = _player.GetCastOrigin();
    effect.direction = _player.GetFacingDirection();
    effect.active = true;

    switch (castType)
    {
    case Character::CastType::FireSpread:
        effect.texture = &_fireballCastTex;
        effect.frameCount = 8;
        effect.scale = 4.f;
        break;
    case Character::CastType::IceSpread:
        effect.texture = &_freezeCastTex;
        effect.frameCount = 8;
        effect.scale = 4.f;
        effect.tint = Color{ 100, 200, 255, 255 };
        break;
    case Character::CastType::ElectricSpread:
        effect.texture = &_lightningCastTex;
        effect.frameCount = 8;
        effect.scale = 4.f;
        break;

    case Character::CastType::SwordBeam:
        effect.texture = &_swordBeamCastTex;
        effect.frameCount = 10;
        effect.scale = 4.f;
        break;

    case Character::CastType::Freeze:
        effect.texture = &_freezeCastTex;
        effect.frameCount = 8;
        effect.scale = 4.f;
        break;

    default:
        effect.active = false;
        break;
    }

    if (effect.active)
        _effects.push_back(effect);
}

void Engine::SpawnHitEffect(Character::CastType castType, Vector2 worldPos, Vector2 direction)
{
    AnimatedEffect effect{};
    effect.followPlayer = false;
    effect.worldPos = worldPos;
    effect.direction = direction;
    effect.active = true;
    effect.frameTime = 1.f / 20.f;

    switch (castType)
    {
    case Character::CastType::None:
        effect.texture = &_swordBeamHitTex;
        effect.frameCount = 5;
        effect.scale = 3.5f;
        effect.tint = Color{ 255, 150, 150, 255 };
        break;

    case Character::CastType::FireSpread:
        effect.texture = &_fireballHitTex;
        effect.frameCount = 6;
        effect.scale = 4.f;
        effect.tint = WHITE;
        break;
    case Character::CastType::IceSpread:
        effect.texture = &_freezeHitTex;
        effect.frameCount = 5;
        effect.scale = 4.f;
        effect.tint = Color{ 100, 200, 255, 255 };
        break;
    case Character::CastType::ElectricSpread:
        effect.texture = &_swordBeamHitTex;  // Hit03.png — the lightning impact sprite
        effect.frameCount = 5;
        effect.scale = 4.f;
        effect.tint = WHITE;
        break;

    case Character::CastType::SwordBeam:
        effect.texture = &_swordBeamHitTex;
        effect.frameCount = 5;
        effect.scale = 3.5f;
        effect.tint = Color{ 150, 220, 255, 255 };
        break;

    case Character::CastType::Freeze:
        effect.texture = &_freezeHitTex;
        effect.frameCount = 5;
        effect.scale = 4.f;
        effect.tint = Color{ 70, 110, 210, 255 };
        break;

    default:
        effect.active = false;
        break;
    }

    if (effect.active)
        _effects.push_back(effect);
}

void Engine::SpawnHealEffect()
{
    AnimatedEffect effect{};
    effect.texture = &_healEffectTex;
    effect.followPlayer = true;
    effect.followPlayerCenter = true;
    effect.offset = Vector2{ 0.f, -20.f };
    effect.direction = Vector2{ 1.f, 0.f };
    effect.tint = WHITE;
    effect.frameCount = 13;
    effect.frameTime = 1.f / 16.f;
    effect.scale = 4.5f;
    effect.active = (_healEffectTex.id != 0);

    if (effect.active)
        _effects.push_back(effect);
}

void Engine::UpdateFreezeProjectiles(float dt)
{
    for (auto& projectile : _freezeProjectiles)
    {
        if (!projectile.IsActive())
            continue;

        projectile.Update(dt);

        if (!projectile.IsActive())
            continue;

        // Destroyed by walls / props
        for (auto& prop : _props)
        {
            if (CheckCollisionRecs(projectile.GetCollisionRec(), prop.GetCollisionRec()))
            {
                projectile.Destroy();
                break;
            }
        }

        if (!projectile.IsActive())
            continue;

        // Freeze still keeps its crowd-control role, but it also chips for a
        // light amount of direct damage so it scales with the player's special
        // damage multiplier instead of falling off completely.
        for (auto& enemy : _enemies)
        {
            if (!enemy->IsActive() || !enemy->IsAlive())
                continue;

            if (CheckCollisionRecs(projectile.GetCollisionRec(), enemy->GetHitCollisionRec()))
            {
                float duration = GetRandomValue(3, 5) * 1.f;
                enemy->TakeDamage(_player.GetFreezeDamage(), _player.GetWorldPos());
                enemy->ApplyFreeze(duration);
                SpawnHitEffect(Character::CastType::Freeze, projectile.GetWorldPos(), projectile.GetDirection());
                projectile.Destroy();
                TriggerScreenShake(3.f, 0.04f);
                break;
            }
        }

    }

    _freezeProjectiles.erase(
        std::remove_if(_freezeProjectiles.begin(), _freezeProjectiles.end(),
            [](const FreezeProjectile& p) { return !p.IsActive(); }),
        _freezeProjectiles.end());
}

void Engine::UpdateSpreadProjectiles(float dt)
{
    for (auto& projectile : _spreadProjectiles)
    {
        if (!projectile.IsActive())
            continue;

        projectile.Update(dt);

        if (!projectile.IsActive())
            continue;

        // Helper: map element -> CastType so hit effects use the right sprite
        auto elementToCastType = [](AbilityType el) -> Character::CastType
        {
            if (el == AbilityType::IceSpread     || el == AbilityType::IceBolt)      return Character::CastType::IceSpread;
            if (el == AbilityType::ElectricSpread|| el == AbilityType::ElectricBolt) return Character::CastType::ElectricSpread;
            return Character::CastType::FireSpread;
        };

        // Map-boundary wall check — same margins as player collision
        {
            float mapW = _map.width  * _mapScale;
            float mapH = _map.height * _mapScale;
            Vector2 p  = projectile.GetWorldPos();
            if (p.x < 76.f || p.x > mapW - 96.f ||
                p.y < 42.f || p.y > mapH - 320.f)
            {
                SpawnHitEffect(elementToCastType(projectile.GetElement()),
                               projectile.GetWorldPos(), projectile.GetDirection());
                projectile.Destroy();
                continue;
            }
        }

        for (auto& prop : _props)
        {
            if (CheckCollisionRecs(projectile.GetCollisionRec(), prop.GetCollisionRec()))
            {
                SpawnHitEffect(elementToCastType(projectile.GetElement()),
                               projectile.GetWorldPos(), projectile.GetDirection());
                projectile.Destroy();
                break;
            }
        }

        if (!projectile.IsActive())
            continue;

        for (auto& enemy : _enemies)
        {
            if (!enemy->IsActive() || !enemy->IsAlive())
                continue;

            if (!CheckCollisionRecs(projectile.GetCollisionRec(), enemy->GetHitCollisionRec()))
                continue;

            AbilityType element = projectile.GetElement();

            bool isBolt = (element == AbilityType::FireBolt  ||
                           element == AbilityType::IceBolt   ||
                           element == AbilityType::ElectricBolt);

            // Bolts deal more damage per shot; boss caps at 2 from bolts vs 1 from spread
            int hitDamage;
            if (enemy->AsMolarbeast() != nullptr)
                hitDamage = isBolt ? 2 : 1;
            else
                hitDamage = isBolt ? _player.GetBoltHitDamage() : _player.GetSpreadHitDamage();
            enemy->TakeDamage(hitDamage, _player.GetWorldPos());

            // Per-element on-hit effect — same for both spread and bolt of the same element
            Character::CastType hitEffectType = Character::CastType::FireSpread;
            if (element == AbilityType::FireSpread || element == AbilityType::FireBolt)
            {
                int burnDmg = isBolt ? _player.GetBoltBurnDamage() : _player.GetSpreadBurnDamage();
                enemy->ApplyBurn(1.f, burnDmg, _player.GetWorldPos());
                hitEffectType = Character::CastType::FireSpread;
            }
            else if (element == AbilityType::IceSpread || element == AbilityType::IceBolt)
            {
                enemy->ApplyFreeze(3.f);
                hitEffectType = Character::CastType::IceSpread;
            }
            else if (element == AbilityType::ElectricSpread || element == AbilityType::ElectricBolt)
            {
                enemy->ApplyElectricCharge();
                hitEffectType = Character::CastType::ElectricSpread;
            }

            SpawnHitEffect(hitEffectType, projectile.GetWorldPos(), projectile.GetDirection());
            projectile.Destroy();
            TriggerScreenShake(4.f, 0.05f);
            StopSound(_explosionSound);
            PlaySound(_explosionSound);
            break;
        }
    }

    _spreadProjectiles.erase(
        std::remove_if(_spreadProjectiles.begin(), _spreadProjectiles.end(),
            [](const SpreadProjectile& p) { return !p.IsActive(); }),
        _spreadProjectiles.end());
}

void Engine::UpdateSwordBeamProjectiles(float dt)
{
    for (auto& projectile : _swordBeamProjectiles)
    {
        if (!projectile.IsActive())
            continue;

        projectile.Update(dt);

        if (!projectile.IsActive())
            continue;

        // Disappears when it travels beyond visible range (approx screen diagonal)
        if (Vector2Distance(projectile.GetWorldPos(), _player.GetWorldPos()) > 900.f)
        {
            projectile.Destroy();
            continue;
        }

        for (auto& enemy : _enemies)
        {
            if (!enemy->IsActive() || !enemy->IsAlive() || projectile.HasHitEnemy(enemy.get()))
                continue;

            if (CheckCollisionRecs(projectile.GetCollisionRec(), enemy->GetHitCollisionRec()))
            {
                // Boss tuning: blade beam is the strongest special tool in the
                // fight, so it always lands for 2 direct damage on the
                // Molarbeast while regular enemies still use the player's
                // scaled sword-beam value.
                int swordBeamDamage = (enemy->AsMolarbeast() != nullptr)
                    ? 2
                    : _player.GetSwordBeamDamage();
                enemy->TakeDamage(swordBeamDamage, _player.GetWorldPos());
                projectile.RegisterHitEnemy(enemy.get());
                SpawnHitEffect(Character::CastType::SwordBeam, enemy->GetWorldPos(), projectile.GetDirection());
                TriggerScreenShake(5.f, 0.06f);
            }
        }

    }

    _swordBeamProjectiles.erase(
        std::remove_if(_swordBeamProjectiles.begin(), _swordBeamProjectiles.end(),
            [](const SwordBeamProjectile& projectile) { return !projectile.IsActive(); }),
        _swordBeamProjectiles.end());
}


void Engine::SpawnEnemyDrop(Vector2 worldPos)
{
    const int dropChancePercent = 22;
    if (GetRandomValue(1, 100) > dropChancePercent)
        return;

    // 50/50 heal or mana gem
    std::unique_ptr<Pickup> pickup;
    if (GetRandomValue(0, 1) == 0)
    {
        auto p = std::make_unique<HealPickup>();
        p->Init(worldPos);
        pickup = std::move(p);
    }
    else
    {
        auto p = std::make_unique<ManaGemPickup>();
        p->Init(worldPos);
        pickup = std::move(p);
    }

    // Move drop away from inside a prop if needed
    Vector2 dropPos = worldPos;
    if (!IsSpawnPositionValid(dropPos))
    {
        float mapW = _map.width * _mapScale;
        float mapH = _map.height * _mapScale;

        for (int attempt = 0; attempt < 20; ++attempt)
        {
            Vector2 candidate{
                worldPos.x + (float)GetRandomValue(-180, 180),
                worldPos.y + (float)GetRandomValue(-180, 180)
            };

            candidate.x = (float)ClampInt((int)candidate.x, 100, (int)mapW - 100);
            candidate.y = (float)ClampInt((int)candidate.y, 100, (int)mapH - 100);

            if (!IsSpawnPositionValid(candidate))
                continue;

            dropPos = candidate;
            break;
        }
    }

    if (pickup->GetType() == PickupType::Heal)
        static_cast<HealPickup*>(pickup.get())->Init(dropPos);
    else
        static_cast<ManaGemPickup*>(pickup.get())->Init(dropPos);

    _pickups.push_back(std::move(pickup));
}

void Engine::SpawnTimedPickup()
{
    float mapW = _map.width  * _mapScale;
    float mapH = _map.height * _mapScale;

    Vector2 pos{};
    int attempts = 0;
    do
    {
        pos = Vector2{
            (float)GetRandomValue(300, (int)mapW - 300),
            (float)GetRandomValue(300, (int)mapH - 300)
        };
        attempts++;
    } while (!IsSpawnPositionValid(pos) && attempts < 40);

    // Timed pickups are always health or mana gems
    std::unique_ptr<Pickup> pickup;

    if (GetRandomValue(0, 1) == 0)
    {
        auto p = std::make_unique<HealPickup>();
        p->Init(pos);
        p->SetTimerSpawned(true);
        pickup = std::move(p);
    }
    else
    {
        auto p = std::make_unique<ManaGemPickup>();
        p->Init(pos);
        p->SetTimerSpawned(true);
        pickup = std::move(p);
    }

    _pickups.push_back(std::move(pickup));
}

void Engine::DrawHowToPlay()
{
    const float sw = (float)GetScreenWidth();
    const float sh = (float)GetScreenHeight();

    // ── Font sizes (screen-relative) ────────────────────────────────────────
    const int titleSz  = (int)(sh * 0.074f);   // ~80
    const int headerSz = (int)(sh * 0.038f);   // ~41
    const int labelSz  = (int)(sh * 0.030f);   // ~32
    const int descSz   = (int)(sh * 0.024f);   // ~26

    // ── Background — slow-scrolling checkerboard ────────────────────────────
    {
        const int   cell  = 80;
        const Color dark  = Color{ 52, 38, 26, 255 };
        const Color light = Color{ 72, 54, 36, 255 };

        const int period = cell * 2;
        float t    = (float)GetTime();
        int   offX = (int)fmodf(t * 22.f, (float)period);
        int   offY = (int)fmodf(t * 12.f, (float)period);
        int   phaseX = offX / cell;
        int   phaseY = offY / cell;
        int   pixX   = offX % cell;
        int   pixY   = offY % cell;

        for (int gy = -1; gy <= (int)(sh / cell) + 1; gy++)
        {
            for (int gx = -1; gx <= (int)(sw / cell) + 1; gx++)
            {
                bool isDark = (((gx + phaseX) + (gy + phaseY)) % 2 + 2) % 2 == 0;
                DrawRectangle(gx * cell - pixX, gy * cell - pixY,
                    cell, cell, isDark ? dark : light);
            }
        }
    }

    // ── Title banner ────────────────────────────────────────────────────────
    float titleBannerY = sh * 0.02f;
    float titleBannerH = titleSz + sh * 0.03f;
    DrawRectangle(0, (int)titleBannerY, (int)sw, (int)titleBannerH, Fade(BLACK, 0.6f));
    const char* title = "How To Play";
    int titleW = MeasureText(title, titleSz);
    DrawText(title, (int)(sw / 2.f - titleW / 2.f), (int)(titleBannerY + sh * 0.012f), titleSz, YELLOW);

    // ── Layout anchors ───────────────────────────────────────────────────────
    const float contentY  = titleBannerY + titleBannerH + sh * 0.025f;
    const float dividerX  = sw * 0.50f;              // centre divider
    const float gap       = sw * 0.035f;             // gap either side of divider
    const float colL      = sw * 0.03f;              // left col start
    const float iconCX    = dividerX + gap * 0.6f;   // icon centre — well clear of the line
    const float colRText  = dividerX + gap + 28.f;   // right col text start
    const float rowGap    = sh * 0.090f;

    // ── Divider ─────────────────────────────────────────────────────────────
    DrawLineEx({ dividerX, contentY }, { dividerX, sh - sh * 0.09f }, 2.f, Fade(WHITE, 0.30f));

    // ── LEFT column — Controls ───────────────────────────────────────────────
    float rowY = contentY;
    DrawText("CONTROLS", (int)colL, (int)rowY, headerSz, ORANGE);
    rowY += headerSz + sh * 0.018f;

    struct CtrlEntry { const char* key; const char* desc; };
    CtrlEntry controls[] = {
        { "W A S D",     "Move"                        },
        { "SPACE",       "Dash  (brief invincibility)" },
        { "Left Click",  "Melee attack"                },
        { "Right Click", "Fireball burst (needs ammo)" },
        { "1 / 2 / 3",  "Select ability slot"         },
        { "Scroll",      "Cycle ability slot"          },
        { "ESC",         "Pause / unpause"             },
    };

    for (auto& c : controls)
    {
        int kw = MeasureText(c.key, labelSz);
        float badgeH = (float)labelSz + 10.f;
        DrawRectangleRounded({ colL, rowY - 4.f, (float)kw + 18.f, badgeH },
            0.3f, 4, Fade(BLACK, 0.7f));
        DrawRectangleRoundedLines({ colL, rowY - 4.f, (float)kw + 18.f, badgeH },
            0.3f, 4, Fade(WHITE, 0.5f));
        DrawText(c.key,  (int)colL + 9, (int)rowY, labelSz, WHITE);
        DrawText(c.desc, (int)(colL + kw + 30.f), (int)rowY, descSz, LIGHTGRAY);
        rowY += rowGap * 0.82f;
    }

    // EXP section below controls
    rowY += sh * 0.01f;
    DrawText("EXP & LEVELS", (int)colL, (int)rowY, headerSz, ORANGE);
    rowY += headerSz + sh * 0.012f;
    DrawText("Every enemy kill grants 1 EXP.",                  (int)colL, (int)rowY, descSz, LIGHTGRAY); rowY += descSz + 6;
    DrawText("Level up: +1 ATK, +1 Max HP, +1 HP restored.",    (int)colL, (int)rowY, descSz, LIGHTGRAY); rowY += descSz + 6;
    DrawText("EXP threshold doubles each level (10, 20, 40\xE2\x80\xA6)", (int)colL, (int)rowY, descSz, LIGHTGRAY); rowY += descSz + 6;
    DrawText("Max level: 10.",                                   (int)colL, (int)rowY, descSz, LIGHTGRAY);

    // ── RIGHT column — Pickups & Enemies ─────────────────────────────────────
    float rowR = contentY;
    DrawText("PICKUPS & ENEMIES", (int)colRText, (int)rowR, headerSz, ORANGE);
    rowR += headerSz + sh * 0.018f;

    struct PickupEntry { const char* name; const char* desc; int shape; };
    PickupEntry entries[] = {
        { "Fireball Ammo",   "Right-click — 8 fireballs outward.",  0 },
        { "Sword Beam Ammo", "Right-click — piercing beam forward.", 1 },
        { "Freeze Ammo",     "Right-click — freezes all enemies.",   2 },
        { "Heal",            "Restores 1 HP (rare drop).",           3 },
        { "Enemy",           "Chases you. Drops a pickup on death.", 4 },
    };

    for (auto& e : entries)
    {
        float cy = rowR + labelSz * 0.5f;

        // Icon — drawn at iconCX, well to the right of the divider
        if (e.shape == 0)
        {
            DrawCircleV({ iconCX, cy }, 20.f, Fade(ORANGE, 0.5f));
            DrawCircleV({ iconCX, cy }, 13.f, Fade(RED,    0.8f));
            DrawCircleV({ iconCX, cy },  5.f, Fade(YELLOW, 0.9f));
        }
        else if (e.shape == 1)
        {
            DrawCircleV({ iconCX + 6.f,  cy      }, 18.f, Fade(BLUE,  0.8f));
            DrawCircleV({ iconCX + 15.f, cy - 2.f}, 17.f, Fade(BLACK, 0.95f));
            DrawLineEx({ iconCX - 14.f, cy - 6.f }, { iconCX - 34.f, cy - 10.f }, 4.f, Fade(SKYBLUE, 0.7f));
            DrawLineEx({ iconCX - 12.f, cy + 7.f }, { iconCX - 30.f, cy + 13.f }, 3.f, Fade(SKYBLUE, 0.55f));
        }
        else if (e.shape == 2)
        {
            DrawCircleV({ iconCX, cy }, 20.f, Fade(BLUE,    0.35f));
            DrawCircleV({ iconCX, cy }, 14.f, Fade(SKYBLUE, 0.85f));
            DrawCircleV({ iconCX, cy },  6.f, Fade(WHITE,   0.90f));
            DrawLineEx({ iconCX - 12.f, cy }, { iconCX + 12.f, cy }, 2.f, WHITE);
            DrawLineEx({ iconCX, cy - 12.f }, { iconCX, cy + 12.f }, 2.f, WHITE);
        }
        else if (e.shape == 3)
        {
            DrawCircleV({ iconCX, cy }, 20.f, Fade(RED,  0.35f));
            DrawCircleV({ iconCX, cy }, 14.f, Fade(RED,  0.85f));
            DrawCircleV({ iconCX, cy },  6.f, Fade(PINK, 0.90f));
            DrawLineEx({ iconCX - 8.f, cy }, { iconCX + 8.f, cy }, 3.f, WHITE);
            DrawLineEx({ iconCX, cy - 8.f }, { iconCX, cy + 8.f }, 3.f, WHITE);
        }
        else
        {
            DrawCircleV({ iconCX, cy }, 20.f, Fade(MAROON, 0.7f));
            DrawCircleV({ iconCX, cy - 7.f }, 10.f, Fade(RED,    0.85f));
            DrawCircleV({ iconCX, cy + 9.f }, 13.f, Fade(MAROON, 0.9f));
        }

        // Name + desc to the right of the icon
        float textX = iconCX + 28.f;
        DrawText(e.name, (int)textX, (int)rowR, labelSz, WHITE);
        DrawText(e.desc, (int)textX, (int)(rowR + labelSz + 4), descSz, LIGHTGRAY);

        rowR += rowGap * 1.05f;
    }

    // ── Back button ──────────────────────────────────────────────────────────
    const float btnW = sw * 0.13f;
    const float btnH = sh * 0.055f;
    const float btnX = sw / 2.f - btnW / 2.f;
    const float btnY = sh - btnH - sh * 0.018f;

    Rectangle backBtn{ btnX, btnY, btnW, btnH };
    bool hovered = CheckCollisionPointRec(GetMousePosition(), backBtn);

    DrawRectangleRounded(backBtn, 0.3f, 6, hovered ? Fade(GRAY, 0.9f) : Fade(DARKGRAY, 0.85f));
    DrawRectangleRoundedLines(backBtn, 0.3f, 6, Fade(WHITE, 0.5f));

    const char* backLabel = (_howToPlayFrom == GameState::Pause) ? "Resume Game" : "< Back";
    int backLabelSz = (int)(sh * 0.030f);
    int backW = MeasureText(backLabel, backLabelSz);
    DrawText(backLabel,
        (int)(btnX + btnW / 2.f - backW / 2.f),
        (int)(btnY + btnH / 2.f - backLabelSz / 2.f),
        backLabelSz, WHITE);

    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        StopSound(_buttonPressSound); PlaySound(_buttonPressSound);
        if (_howToPlayFrom == GameState::Menu)
            _menu.Init();
        _gameState = _howToPlayFrom;
    }
}

void Engine::DrawMiniMap()
{
    const float mapW = _map.width  * _mapScale;
    const float mapH = _map.height * _mapScale;

    const float miniW = 160.f;
    const float miniH = miniW * (mapH / mapW);
    const float padding = 12.f;
    const float originX = padding;
    const float originY = GetScreenHeight() - miniH - padding;

    // Background
    DrawRectangleRounded(Rectangle{ originX - 2.f, originY - 2.f, miniW + 4.f, miniH + 4.f },
        0.08f, 4, Fade(BLACK, 0.85f));
    DrawRectangle((int)originX, (int)originY, (int)miniW, (int)miniH, Fade(DARKGRAY, 0.5f));

    auto toMini = [&](Vector2 worldPos) -> Vector2 {
        return Vector2{
            originX + (worldPos.x / mapW) * miniW,
            originY + (worldPos.y / mapH) * miniH
        };
    };

    // Enemy dots — red
    for (const auto& enemy : _enemies)
    {
        if (!enemy->IsActive())
            continue;
        if (!enemy->IsAlive())
            continue;

        Vector2 dot = toMini(enemy->GetWorldPos());
        if (enemy->AsMolarbeast() != nullptr)
            DrawCircleV(dot, 6.f, Fade(MAROON, 0.95f));
        else if (enemy->AsCyclops() != nullptr)
            DrawCircleV(dot, 4.f, Fade(ORANGE, 0.9f));
        else if (enemy->AsOgre() != nullptr)
            DrawCircleV(dot, 4.f, Fade(BROWN, 0.9f));
        else
            DrawCircleV(dot, 3.f, Fade(RED, 0.9f));
    }

    // Cyclops dots — orange

    // Pickup dots — green for all active pickups
    for (const auto& pickup : _pickups)
    {
        if (!pickup->IsActive())
            continue;

        Vector2 dot = toMini(pickup->GetWorldPos());
        DrawCircleV(dot, 4.f, Fade(GREEN, 0.9f));
    }

    // Player dot — yellow
    Vector2 playerDot = toMini(_player.GetWorldPos());
    DrawCircleV(playerDot, 5.f, Fade(YELLOW, 1.0f));

    DrawRectangleLinesEx(Rectangle{ originX, originY, miniW, miniH }, 1.f, Fade(LIGHTGRAY, 0.6f));
}

Vector2 Engine::GetRandomPropPosition()
{
    float mapW = _map.width * _mapScale;
    float mapH = _map.height * _mapScale;
    Vector2 playerStart{ mapW * 0.5f, mapH * 0.5f };

    float minX = mapW * 0.05f;
    float maxX = mapW * 0.95f;
    float minY = mapH * 0.05f;
    float maxY = mapH * 0.85f;

    float minSpacing = 308.f;
    float playerSafeRadius = 320.f;
    int attempts = 0;
    const int maxAttempts = 50;

    while (attempts < maxAttempts)
    {
        Vector2 pos{};
        pos.x = (float)GetRandomValue((int)minX, (int)maxX);
        pos.y = (float)GetRandomValue((int)minY, (int)maxY);

        bool tooClose = false;

        if (Vector2Distance(pos, playerStart) < playerSafeRadius)
            tooClose = true;

        for (auto& prop : _props)
        {
            if (Vector2Distance(pos, prop.GetWorldPos()) < minSpacing)
            {
                tooClose = true;
                break;
            }
        }

        if (!tooClose)
            return pos;

        attempts++;
    }

    Vector2 fallback{};
    fallback.x = (float)GetRandomValue((int)minX, (int)maxX);
    fallback.y = (float)GetRandomValue((int)minY, (int)maxY);
    return fallback;
}

void Engine::BuildNavigationGrid()
{
    float mapW = _map.width * _mapScale;
    float mapH = _map.height * _mapScale;

    _navCols = std::max(1, (int)std::ceil(mapW / _navCellSize));
    _navRows = std::max(1, (int)std::ceil(mapH / _navCellSize));
    _navBlocked.assign(_navCols * _navRows, false);
    _navDistance.assign(_navCols * _navRows, std::numeric_limits<int>::max());

    for (int row = 0; row < _navRows; ++row)
    {
        for (int col = 0; col < _navCols; ++col)
        {
            Rectangle cellRect{
                col * _navCellSize,
                row * _navCellSize,
                _navCellSize,
                _navCellSize
            };

            for (auto& prop : _props)
            {
                if (CheckCollisionRecs(cellRect, prop.GetCollisionRec()))
                {
                    _navBlocked[GetNavigationIndex(col, row)] = true;
                    break;
                }
            }
        }
    }
}

bool Engine::IsNavigationCellBlocked(int col, int row) const
{
    if (col < 0 || col >= _navCols || row < 0 || row >= _navRows)
        return true;
    return _navBlocked[GetNavigationIndex(col, row)];
}

int Engine::GetNavigationIndex(int col, int row) const
{
    return row * _navCols + col;
}

bool Engine::FindNearestOpenCell(int& col, int& row) const
{
    if (!IsNavigationCellBlocked(col, row))
        return true;

    for (int radius = 1; radius < std::max(_navCols, _navRows); ++radius)
    {
        for (int dc = -radius; dc <= radius; ++dc)
        {
            for (int dr = -radius; dr <= radius; ++dr)
            {
                if (std::abs(dc) != radius && std::abs(dr) != radius)
                    continue;

                int nc = col + dc;
                int nr = row + dr;

                if (!IsNavigationCellBlocked(nc, nr))
                {
                    col = nc;
                    row = nr;
                    return true;
                }
            }
        }
    }

    return false;
}

int Engine::GetClosestOpenNavigationIndex(int col, int row) const
{
    int nextCol = ClampInt(col, 0, std::max(0, _navCols - 1));
    int nextRow = ClampInt(row, 0, std::max(0, _navRows - 1));

    if (!FindNearestOpenCell(nextCol, nextRow))
        return -1;

    return GetNavigationIndex(nextCol, nextRow);
}

bool Engine::HasLineOfSight(Vector2 start, Vector2 end) const
{
    Vector2 dir = Vector2Subtract(end, start);
    float dist = Vector2Length(dir);

    if (dist < 0.01f)
        return true;

    dir = Vector2Normalize(dir);
    float step = _navCellSize * 0.5f;

    for (float t = 0.f; t < dist; t += step)
    {
        Vector2 point = Vector2Add(start, Vector2Scale(dir, t));
        int col = (int)(point.x / _navCellSize);
        int row = (int)(point.y / _navCellSize);

        if (IsNavigationCellBlocked(col, row))
            return false;
    }

    return true;
}

Vector2 Engine::GetNavigationTarget(Vector2 startWorldPos, Vector2 targetWorldPos) const
{
    if (_navCols <= 0 || _navRows <= 0)
        return targetWorldPos;

    int startCol = ClampInt((int)(startWorldPos.x / _navCellSize), 0, _navCols - 1);
    int startRow = ClampInt((int)(startWorldPos.y / _navCellSize), 0, _navRows - 1);

    if (!FindNearestOpenCell(startCol, startRow))
        return targetWorldPos;

    static const std::array<std::pair<int,int>, 8> offsets{{
        {1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}
    }};

    int startIndex = GetNavigationIndex(startCol, startRow);
    int bestIndex  = startIndex;
    int bestCost   = _navDistance[startIndex];

    for (int i = 0; i < (int)offsets.size(); ++i)
    {
        int dc = offsets[i].first;
        int dr = offsets[i].second;
        int nc = startCol + dc;
        int nr = startRow + dr;

        if (IsNavigationCellBlocked(nc, nr))
            continue;

        if (dc != 0 && dr != 0)
        {
            if (IsNavigationCellBlocked(startCol + dc, startRow) ||
                IsNavigationCellBlocked(startCol, startRow + dr))
                continue;
        }

        int nextIndex = GetNavigationIndex(nc, nr);
        int nextCost  = _navDistance[nextIndex];

        if (nextCost < bestCost)
        {
            bestCost  = nextCost;
            bestIndex = nextIndex;
        }
    }

    if (bestIndex == startIndex || bestCost == std::numeric_limits<int>::max())
        return targetWorldPos;

    int nextCol = bestIndex % _navCols;
    int nextRow = bestIndex / _navCols;

    return Vector2{
        nextCol * _navCellSize + _navCellSize * 0.5f,
        nextRow * _navCellSize + _navCellSize * 0.5f
    };
}

Vector2 Engine::GetAStarTarget(Vector2 startWorldPos, Vector2 targetWorldPos) const
{
    if (_navCols <= 0 || _navRows <= 0)
        return targetWorldPos;

    int startCol = ClampInt((int)(startWorldPos.x / _navCellSize), 0, _navCols - 1);
    int startRow = ClampInt((int)(startWorldPos.y / _navCellSize), 0, _navRows - 1);
    int goalCol  = ClampInt((int)(targetWorldPos.x / _navCellSize), 0, _navCols - 1);
    int goalRow  = ClampInt((int)(targetWorldPos.y / _navCellSize), 0, _navRows - 1);

    // Snap start and goal to the nearest open cells in case they sit inside a blocked tile.
    if (!FindNearestOpenCell(startCol, startRow))
        return targetWorldPos;
    FindNearestOpenCell(goalCol, goalRow);

    int startIdx = GetNavigationIndex(startCol, startRow);
    int goalIdx  = GetNavigationIndex(goalCol,  goalRow);

    if (startIdx == goalIdx)
        return targetWorldPos;

    const int total = _navCols * _navRows;

    struct Node
    {
        int f, g, idx;
        bool operator>(const Node& o) const { return f > o.f; }
    };

    std::vector<int>  gScore(total, INT_MAX);
    std::vector<int>  parent(total, -1);
    std::vector<bool> closed(total, false);
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;

    // Octile heuristic (integer scaled x10) — admissible for 8-directional movement.
    auto heuristic = [&](int col, int row) -> int
    {
        int dc = std::abs(col - goalCol);
        int dr = std::abs(row - goalRow);
        return 10 * (dc + dr) + (14 - 20) * std::min(dc, dr);  // = 10*(dc+dr) - 6*min
    };

    gScore[startIdx] = 0;
    open.push({ heuristic(startCol, startRow), 0, startIdx });

    static constexpr int dc[] = { 1, -1,  0,  0,  1,  1, -1, -1 };
    static constexpr int dr[] = { 0,  0,  1, -1,  1, -1,  1, -1 };
    static constexpr int cost[]= {10, 10, 10, 10, 14, 14, 14, 14 };

    bool found = false;
    while (!open.empty())
    {
        Node cur = open.top();
        open.pop();

        if (cur.idx == goalIdx) { found = true; break; }
        if (closed[cur.idx]) continue;
        closed[cur.idx] = true;

        int col = cur.idx % _navCols;
        int row = cur.idx / _navCols;

        for (int i = 0; i < 8; ++i)
        {
            int nc = col + dc[i];
            int nr = row + dr[i];
            if (nc < 0 || nc >= _navCols || nr < 0 || nr >= _navRows) continue;

            int nIdx = GetNavigationIndex(nc, nr);
            if (_navBlocked[nIdx] || closed[nIdx]) continue;

            // For diagonals, require both shared cardinal neighbours to be open
            // so the path doesn't clip through prop corners.
            if (i >= 4 && (IsNavigationCellBlocked(col + dc[i], row) ||
                           IsNavigationCellBlocked(col, row + dr[i])))
                continue;

            int tentG = gScore[cur.idx] + cost[i];
            if (tentG < gScore[nIdx])
            {
                parent[nIdx] = cur.idx;
                gScore[nIdx] = tentG;
                open.push({ tentG + heuristic(nc, nr), tentG, nIdx });
            }
        }
    }

    if (!found)
        return targetWorldPos;

    // Walk the parent chain back to find the first step after the start cell.
    int step = goalIdx;
    while (parent[step] != -1 && parent[step] != startIdx)
        step = parent[step];

    if (parent[step] == -1)
        return targetWorldPos;

    int stepCol = step % _navCols;
    int stepRow = step / _navCols;

    return Vector2{
        stepCol * _navCellSize + _navCellSize * 0.5f,
        stepRow * _navCellSize + _navCellSize * 0.5f
    };
}

void Engine::RefreshNavigationField()
{
    if (_navCols <= 0 || _navRows <= 0 || _navRefreshInFlight)
        return;

    Vector2 playerFeet = _player.GetFeetWorldPos();
    std::vector<bool> blockedCopy = _navBlocked;
    int cols = _navCols;
    int rows = _navRows;
    float cellSize = _navCellSize;

#ifdef __EMSCRIPTEN__
    // The first web build keeps navigation single-threaded so it does not rely
    // on browser pthread support. The desktop build still uses the async job.
    NavigationRefreshResult result = BuildNavigationRefreshResult(blockedCopy, cols, rows, cellSize, playerFeet);
    _navDistance = std::move(result.distanceField);
    _lastPlayerNavIndex = result.playerNavIndex;
#else
    // Build the shared flow field off-thread using copies of the immutable
    // grid data for this refresh. Enemy movement still consumes the finished
    // field on the main thread, so gameplay mutation stays single-threaded.
    _navRefreshJob = std::async(std::launch::async,
        [blocked = std::move(blockedCopy), cols, rows, cellSize, playerFeet]() mutable
        {
            return BuildNavigationRefreshResult(blocked, cols, rows, cellSize, playerFeet);
        });
    _navRefreshInFlight = true;
#endif
}

void Engine::ApplyCompletedNavigationRefresh()
{
#ifdef __EMSCRIPTEN__
    return;
#else
    if (!_navRefreshInFlight || !_navRefreshJob.valid())
        return;

    if (_navRefreshJob.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;

    NavigationRefreshResult result = _navRefreshJob.get();
    _navDistance = std::move(result.distanceField);
    _lastPlayerNavIndex = result.playerNavIndex;
    _navRefreshInFlight = false;
#endif
}

void Engine::RespawnOutOfBoundsEnemies()
{
    float mapW = _map.width  * _mapScale;
    float mapH = _map.height * _mapScale;
    const float hardMargin = 60.f;   // enemy is truly outside if beyond this

    for (auto& enemy : _enemies)
    {
        if (!enemy->IsActive())
            continue;
        if (enemy->IsDying() || !enemy->IsAlive())
            continue;

        Vector2 pos = enemy->GetWorldPos();
        bool outOfBounds = pos.x < hardMargin || pos.y < hardMargin ||
                           pos.x > mapW - hardMargin || pos.y > mapH - hardMargin;

        if (!outOfBounds)
            continue;

        // Find a valid position away from props, other enemies, and the player
        Vector2 newPos{};
        bool found = false;

        for (int attempt = 0; attempt < 50; ++attempt)
        {
            newPos.x = (float)GetRandomValue(300, (int)(mapW - 300));
            newPos.y = (float)GetRandomValue(300, (int)(mapH - 300));

            if (!IsSpawnPositionValid(newPos))
                continue;

            // Also keep a safe distance from the player
            if (Vector2Distance(newPos, _player.GetWorldPos()) < 300.f)
                continue;

            found = true;
            break;
        }

        if (found)
            enemy->Teleport(newPos);
    }
}

bool Engine::IsSpawnPositionValid(Vector2 pos)
{
    const float safeDistance = 200.f;
    const float playerSafeDistance = 260.f;

    // Reject if the spawn cell or any neighbour is blocked by a prop on the nav grid
    int col = (int)(pos.x / _navCellSize);
    int row = (int)(pos.y / _navCellSize);
    for (int dc = -1; dc <= 1; dc++)
        for (int dr = -1; dr <= 1; dr++)
            if (IsNavigationCellBlocked(col + dc, row + dr))
                return false;

    for (auto& prop : _props)
    {
        if (Vector2Distance(pos, prop.GetWorldPos()) < safeDistance)
            return false;
    }

    for (auto& enemy : _enemies)
    {
        if (!enemy->IsActive())
            continue;
        if (Vector2Distance(pos, enemy->GetWorldPos()) < safeDistance)
            return false;
    }

    // Keep enemy, boss, and timed pickup spawns away from the player so
    // nothing materializes directly on top of them.
    if (Vector2Distance(pos, _player.GetWorldPos()) < playerSafeDistance)
        return false;

    return true;
}

void Engine::ResetRunState()
{
    if (_navRefreshJob.valid())
        _navRefreshJob.wait();

    _navRefreshInFlight = false;
    _wave          = 0;
    _enemiesKilled = 0;
    _navRefreshTimer = 0.f;
    _lastPlayerNavIndex = -1;
    _gameTimer = 0.f;
    _playerDying = false;
    _awaitingNameEntry = false;
    _awaitingStartingAbility = false;
    _waveStarting        = true;
    _wave1LevelUpDone    = false;
    _bossWarningTimer    = 0.f;
    _ultimatePhase       = UltimatePhase::None;
    _ultimatePhaseTimer  = 0.f;
    _ultimateCircleAngle = 0.f;
    _showUltimateRow     = false;
    _ultimateRowPicked   = false;
    _regularRowPicked    = false;
    _spreadProjectiles.clear();
    _ultimateBlasts.clear();
    _swordBeamProjectiles.clear();
    _freezeProjectiles.clear();
    _lavaBalls.clear();
    _cyclopsLasers.clear();
    _pickups.clear();
    _effects.clear();
    _player.Init();
    _player.SetWorldPos(Vector2{ _map.width * _mapScale * 0.5f, _map.height * _mapScale * 0.5f });
    _bossCyclopsSupport = {};
    _bossOgreSupport = {};

    for (auto& enemy : _enemies)
    {
        enemy->SetActive(false);
        enemy->Teleport(Vector2{ -5000.f, -5000.f });
    }

    _pickupSpawnTimer = kDefaultTimedPickupInterval;

    RefreshNavigationField();
    if (_navRefreshJob.valid())
        _navRefreshJob.wait();
    ApplyCompletedNavigationRefresh();
    SpawnWave();

}

int Engine::GetActiveEnemyCount() const
{
    int count = 0;
    for (const auto& enemy : _enemies)
    {
        if (enemy->IsActive())
            count++;
    }

    return count;
}

bool Engine::IsBossFightActive() const
{
    for (const auto& enemy : _enemies)
    {
        if (!enemy->IsActive())
            continue;
        if (!enemy->IsAlive())
            continue;
        if (enemy->AsMolarbeast() != nullptr)
            return true;
    }

    return false;
}

bool Engine::TryGetFarSpawnPosition(Vector2& pos, float minPlayerDistance)
{
    float mapW = _map.width * _mapScale;
    float mapH = _map.height * _mapScale;

    for (int attempt = 0; attempt < 60; ++attempt)
    {
        Vector2 candidate{
            (float)GetRandomValue(300, (int)mapW - 300),
            (float)GetRandomValue(300, (int)mapH - 300)
        };

        if (!IsSpawnPositionValid(candidate))
            continue;
        if (Vector2Distance(candidate, _player.GetWorldPos()) < minPlayerDistance)
            continue;

        pos = candidate;
        return true;
    }

    return false;
}

void Engine::SpawnBossSupportAdds()
{
    Vector2 cyclopsPos{};
    if (TryGetFarSpawnPosition(cyclopsPos, kBossSupportMinPlayerDistance))
        _bossCyclopsSupport.enemy = SpawnCyclops(cyclopsPos);

    Vector2 ogrePos{};
    if (TryGetFarSpawnPosition(ogrePos, kBossSupportMinPlayerDistance))
        _bossOgreSupport.enemy = SpawnOgre(ogrePos);

    _bossCyclopsSupport.respawnTimer = 0.f;
    _bossOgreSupport.respawnTimer = 0.f;
}

void Engine::ClearBossSupportAdds()
{
    if (_bossCyclopsSupport.enemy != nullptr)
    {
        _bossCyclopsSupport.enemy->SetActive(false);
        _bossCyclopsSupport.enemy->Teleport(Vector2{ -5000.f, -5000.f });
    }

    if (_bossOgreSupport.enemy != nullptr)
    {
        _bossOgreSupport.enemy->SetActive(false);
        _bossOgreSupport.enemy->Teleport(Vector2{ -5000.f, -5000.f });
    }

    _bossCyclopsSupport = {};
    _bossOgreSupport = {};
}

void Engine::UpdateBossSupportRespawns(float dt)
{
    if (!IsBossFightActive())
    {
        ClearBossSupportAdds();
        return;
    }

    if (_bossCyclopsSupport.enemy != nullptr &&
        !_bossCyclopsSupport.enemy->IsActive() &&
        _bossCyclopsSupport.respawnTimer > 0.f)
    {
        _bossCyclopsSupport.respawnTimer -= dt;
        if (_bossCyclopsSupport.respawnTimer <= 0.f)
        {
            Vector2 spawnPos{};
            if (TryGetFarSpawnPosition(spawnPos, kBossSupportMinPlayerDistance))
                _bossCyclopsSupport.enemy = SpawnCyclops(spawnPos);
            _bossCyclopsSupport.respawnTimer = 0.f;
        }
    }

    if (_bossOgreSupport.enemy != nullptr &&
        !_bossOgreSupport.enemy->IsActive() &&
        _bossOgreSupport.respawnTimer > 0.f)
    {
        _bossOgreSupport.respawnTimer -= dt;
        if (_bossOgreSupport.respawnTimer <= 0.f)
        {
            Vector2 spawnPos{};
            if (TryGetFarSpawnPosition(spawnPos, kBossSupportMinPlayerDistance))
                _bossOgreSupport.enemy = SpawnOgre(spawnPos);
            _bossOgreSupport.respawnTimer = 0.f;
        }
    }
}

bool Engine::TryGetPooledEnemySpawn(Vector2 pos)
{
    for (auto& enemy : _enemies)
    {
        if (enemy->IsActive())
            continue;
        if (enemy->AsCyclops() != nullptr)
            continue;
        if (enemy->AsOgre() != nullptr)
            continue;
        if (enemy->AsMolarbeast() != nullptr)
            continue;

        enemy->ResetForSpawn(pos);
        ConfigureSpawnedEnemy(*enemy);
        return true;
    }

    return false;
}

bool Engine::TryGetPooledCyclopsSpawn(Vector2 pos)
{
    for (auto& enemy : _enemies)
    {
        if (enemy->IsActive())
            continue;

        Cyclops* cyclops = enemy->AsCyclops();
        if (cyclops == nullptr)
            continue;

        cyclops->ResetForSpawn(pos);
        ConfigureSpawnedEnemy(*cyclops);
        return true;
    }

    return false;
}

int Engine::GetCyclopsSpawnCountForWave(int wave) const
{
    // Cyclops joins the main enemy pool instead of being a separate testing
    // stream. Early waves stay melee-focused, then ranged pressure ramps in.
    if (wave <= 2)
        return 0;
    if (wave <= 4)
        return 1;
    return std::min(2, _maxActiveEnemies);
}

bool Engine::TryGetPooledOgreSpawn(Vector2 pos)
{
    for (auto& enemy : _enemies)
    {
        if (enemy->IsActive())
            continue;

        Ogre* ogre = enemy->AsOgre();
        if (ogre == nullptr)
            continue;

        ogre->ResetForSpawn(pos);
        ConfigureSpawnedEnemy(*ogre);
        return true;
    }

    return false;
}

bool Engine::TryGetPooledMolarbeastSpawn(Vector2 pos)
{
    for (auto& enemy : _enemies)
    {
        if (enemy->IsActive())
            continue;

        Molarbeast* molarbeast = enemy->AsMolarbeast();
        if (molarbeast == nullptr)
            continue;

        molarbeast->ResetForSpawn(pos);
        ConfigureSpawnedEnemy(*molarbeast);
        return true;
    }

    return false;
}

int Engine::GetOgreSpawnCountForWave(int wave) const
{
    // Ogre starts as a single wave-4 threat, then gains a second slot later
    // so it remains readable and fair while the player is learning the charge.
    if (wave < 1)
        return 0;
    if (wave < 8)
        return 1;
    return std::min(2, _maxActiveEnemies);
}

int Engine::GetEnemyPowerLevelForWave(int wave) const
{
    // Enemy power level advances once per completed 5-wave block:
    // waves 1-5   = level 1
    // waves 6-10  = level 2
    // waves 11-15 = level 3
    // This keeps the long-run curve readable and lets per-wave composition do
    // most of the heavy lifting in the early and mid game.
    if (wave <= 0)
        return 1;

    return 1 + ((wave - 1) / 5);
}

void Engine::ConfigureSpawnedEnemy(Enemy& enemy)
{
    // All enemy types share the same spawn-tuning path:
    // 1. Apply their archetype stats for the current wave band.
    // 2. Apply the softer global enemy power level that advances every 5 waves.
    // 3. Restore the player target so pooled enemies rejoin the current run.
    enemy.SetWaveScale(_wave);
    enemy.ApplyEnemyPowerLevel(GetEnemyPowerLevelForWave(_wave));
    enemy.SetTarget(&_player);
}

Enemy* Engine::SpawnCyclops(Vector2 pos)
{
    for (auto& enemy : _enemies)
    {
        if (enemy->IsActive())
            continue;

        Cyclops* cyclops = enemy->AsCyclops();
        if (cyclops == nullptr)
            continue;

        cyclops->ResetForSpawn(pos);
        ConfigureSpawnedEnemy(*cyclops);
        return cyclops;
    }

    // No pooled instance - create a new one.
    auto cyclops = std::make_unique<Cyclops>(pos);
    cyclops->Init();
    ConfigureSpawnedEnemy(*cyclops);
    Enemy* cyclopsPtr = cyclops.get();
    _enemies.push_back(std::move(cyclops));
    return cyclopsPtr;
}

Enemy* Engine::SpawnOgre(Vector2 pos)
{
    for (auto& enemy : _enemies)
    {
        if (enemy->IsActive())
            continue;

        Ogre* ogre = enemy->AsOgre();
        if (ogre == nullptr)
            continue;

        ogre->ResetForSpawn(pos);
        ConfigureSpawnedEnemy(*ogre);
        return ogre;
    }

    auto ogre = std::make_unique<Ogre>(pos);
    ogre->Init();
    ConfigureSpawnedEnemy(*ogre);
    Enemy* ogrePtr = ogre.get();
    _enemies.push_back(std::move(ogre));
    return ogrePtr;
}

void Engine::SpawnMolarbeast(Vector2 pos)
{
    if (TryGetPooledMolarbeastSpawn(pos))
    {
        _bossWarningTimer = 4.f;
        _pickupSpawnTimer = std::min(_pickupSpawnTimer, kBossTimedPickupInterval);
        return;
    }

    auto molarbeast = std::make_unique<Molarbeast>(pos);
    molarbeast->Init();
    ConfigureSpawnedEnemy(*molarbeast);
    _enemies.push_back(std::move(molarbeast));
    _bossWarningTimer = 4.f;
    _pickupSpawnTimer = std::min(_pickupSpawnTimer, kBossTimedPickupInterval);
}

// =============================================================================
void Engine::UpdateCyclopsLasers(float dt)
{
    for (auto& laser : _cyclopsLasers)
    {
        if (!laser.IsActive()) continue;

        laser.Update(dt);
        if (!laser.IsActive()) continue;

        // Destroyed by props
        for (auto& prop : _props)
        {
            if (CheckCollisionRecs(laser.GetCollisionRec(), prop.GetCollisionRec()))
            {
                laser.Destroy();
                break;
            }
        }

        if (!laser.IsActive()) continue;

        // Damages player on hit
        if (_player.IsAlive() &&
            CheckCollisionRecs(laser.GetCollisionRec(), _player.GetCollisionRec()))
        {
            _player.TakeDamage(laser.GetDamage(), laser.GetWorldPos());
            laser.Destroy();

            // Cyclops laser already has a strong visual read from the charge
            // glow and beam draw, so keep the hit shake lighter than melee and
            // explosive ability impacts to avoid overdriving the camera.
            TriggerScreenShake(2.5f, 0.07f);
        }
    }

    _cyclopsLasers.erase(
        std::remove_if(_cyclopsLasers.begin(), _cyclopsLasers.end(),
            [](const CyclopsLaserProjectile& l) { return !l.IsActive(); }),
        _cyclopsLasers.end());
}

void Engine::UpdateLavaBallProjectiles(float dt)
{
    const float mapW = _map.width * _mapScale;
    const float mapH = _map.height * _mapScale;
    const float marginLeft = 76.f;
    const float marginRight = 96.f;
    const float marginTop = 42.f;
    const float marginBottom = 320.f;

    for (auto& projectile : _lavaBalls)
    {
        if (!projectile.IsActive())
            continue;

        projectile.Update(dt);
        if (!projectile.IsActive())
            continue;

        Rectangle collisionRec = projectile.GetCollisionRec();

        // Arena wall and prop checks only matter while the ball is travelling.
        if (projectile.IsFlying())
        {
            bool hitArenaWall =
                collisionRec.x < marginLeft ||
                collisionRec.x + collisionRec.width > mapW - marginRight ||
                collisionRec.y < marginTop ||
                collisionRec.y + collisionRec.height > mapH - marginBottom;

            if (hitArenaWall)
            {
                projectile.BeginHit();
                StopSound(_lavaBallImpactSound);
                PlaySound(_lavaBallImpactSound);
                continue;
            }

            bool hitProp = false;
            for (const auto& prop : _props)
            {
                if (CheckCollisionRecs(collisionRec, prop.GetCollisionRec()))
                {
                    hitProp = true;
                    break;
                }
            }
            if (hitProp)
            {
                projectile.BeginHit();
                StopSound(_lavaBallImpactSound);
                PlaySound(_lavaBallImpactSound);
                continue;
            }
        }

        // Player collision is active during both flying and the explosion so
        // a direct hit always registers. HasHitPlayer guards against applying
        // the burn twice from the same projectile.
        if (_player.IsAlive() &&
            !projectile.HasHitPlayer() &&
            CheckCollisionRecs(collisionRec, _player.GetCollisionRec()))
        {
            _player.ApplyBurnTicks(1.0f, 2, 0.2f, projectile.GetWorldPos());
            projectile.OnPlayerHit();
            if (projectile.IsFlying())
            {
                projectile.BeginHit();
                StopSound(_lavaBallImpactSound);
                PlaySound(_lavaBallImpactSound);
            }
        }
    }

    _lavaBalls.erase(
        std::remove_if(_lavaBalls.begin(), _lavaBalls.end(),
            [](const LavaBallProjectile& projectile) { return !projectile.IsActive(); }),
        _lavaBalls.end());
}

void Engine::SaveKeybindings()
{
    const KeyBindings& b = _player.GetBindings();
    FILE* f = nullptr;
    fopen_s(&f, "keybindings.cfg", "w");
    if (!f) return;
    std::fprintf(f, "moveUp %d\n",    (int)b.moveUp);
    std::fprintf(f, "moveDown %d\n",  (int)b.moveDown);
    std::fprintf(f, "moveLeft %d\n",  (int)b.moveLeft);
    std::fprintf(f, "moveRight %d\n", (int)b.moveRight);
    std::fprintf(f, "dash %d\n",      (int)b.dash);
    std::fprintf(f, "attack %d\n",    (int)b.attack);
    std::fprintf(f, "ability0 %d\n",  (int)b.ability[0]);
    std::fprintf(f, "ability1 %d\n",  (int)b.ability[1]);
    std::fprintf(f, "ability2 %d\n",  (int)b.ability[2]);
    std::fclose(f);
}

void Engine::LoadKeybindings()
{
    FILE* f = nullptr;
    fopen_s(&f, "keybindings.cfg", "r");
    if (!f) return;
    KeyBindings b;
    char key[32];
    int  value;
    while (fscanf_s(f, "%31s %d", key, (unsigned)sizeof(key), &value) == 2)
    {
        if      (std::strcmp(key, "moveUp")    == 0) b.moveUp     = (KeyboardKey)value;
        else if (std::strcmp(key, "moveDown")  == 0) b.moveDown   = (KeyboardKey)value;
        else if (std::strcmp(key, "moveLeft")  == 0) b.moveLeft   = (KeyboardKey)value;
        else if (std::strcmp(key, "moveRight") == 0) b.moveRight  = (KeyboardKey)value;
        else if (std::strcmp(key, "dash")      == 0) b.dash       = (KeyboardKey)value;
        else if (std::strcmp(key, "attack")    == 0) b.attack     = (KeyboardKey)value;
        else if (std::strcmp(key, "ability0")  == 0) b.ability[0] = (KeyboardKey)value;
        else if (std::strcmp(key, "ability1")  == 0) b.ability[1] = (KeyboardKey)value;
        else if (std::strcmp(key, "ability2")  == 0) b.ability[2] = (KeyboardKey)value;
    }
    std::fclose(f);
    _player.SetBindings(b);
}

