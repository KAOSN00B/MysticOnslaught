#include "Engine.h"

#include "AnimationUtils.h"
#include "AssetPaths.h"
#include "raymath.h"

#include <algorithm>
#include <array>
#include <cmath>
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
    FireBallPickup::UnloadSharedResources();
    SwordBeamPickup::UnloadSharedResources();
    FreezePickup::UnloadSharedResources();
    HealPickup::UnloadSharedResources();
    FireballProjectile::UnloadSharedResources();
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
    UnloadTexture(_fireballCastTex);
    UnloadTexture(_fireballHitTex);
    UnloadTexture(_swordBeamCastTex);
    UnloadTexture(_swordBeamHitTex);
    UnloadTexture(_freezeCastTex);
    UnloadTexture(_freezeHitTex);
    UnloadTexture(_healEffectTex);
    _pauseUI.Unload();
    CloseWindow();
}

void Engine::Init()
{
    InitWindow(_windowWidth, _windowHeight, "Top Down Game");
    SetTargetFPS(60);
    InitAudioDevice();

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

    _menu.Init();

    _map = LoadTexture(AssetPath("TileSet/Map.png").c_str());
    _pillarTex = LoadTexture(AssetPath("TileSet/Pillar.png").c_str());
    _fireballCastTex = LoadTexture(AssetPath("PowerUps/Fireball_Cast.png").c_str());
    _fireballHitTex = LoadTexture(AssetPath("PowerUps/Fireball_Hit.png").c_str());
    _swordBeamCastTex = LoadTexture(AssetPath("PowerUps/BladeBeam_Cast.png").c_str());
    _swordBeamHitTex = LoadTexture(AssetPath("PowerUps/Hit03.png").c_str());
    _freezeCastTex = LoadTexture(AssetPath("PowerUps/Ice_Shard_Cast.png").c_str());
    _freezeHitTex = LoadTexture(AssetPath("PowerUps/Ice_Shard_Hit.png").c_str());
    _healEffectTex = LoadTexture(AssetPath("PowerUps/Health_Up.png").c_str());

    _props.clear();
    _pickups.clear();
    _fireballProjectiles.clear();
    _swordBeamProjectiles.clear();
    _freezeProjectiles.clear();
    _lavaBalls.clear();
    _effects.clear();
    _enemies.clear();

    int propCount = GetRandomValue(7, 10);

    for (int i = 0; i < propCount; i++)
    {
        Vector2 pos = GetRandomPropPosition();
        _props.push_back(Prop{ pos, _pillarTex });
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
            _fadeInTimer = 2.0f;
            _gameState = GameState::Play;
        }
        if (_menu.QuitPressed())
            _shouldClose = true;
        if (_menu.HowToPressed())
        {
            _howToPlayFrom = GameState::Menu;
            _gameState = GameState::HowToPlay;
        }

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

    _player.Update(dt);

    Character::CastType castType = _player.ConsumeCastRequest();

    if (castType == Character::CastType::Fireball)
    {
        SpawnCastEffect(castType);
        StopSound(_fireballCastSound);
        PlaySound(_fireballCastSound);
        SpawnFireballBurst();
    }
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
            _gameState = GameState::GameOver;

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
            SpawnWave();

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
        UpdateFireballProjectiles(dt);
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

        for (const auto& projectile : _fireballProjectiles)
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

        break;
    }

    case GameState::GameOver:
    {
        int goResult = _pauseUI.DrawGameOver(_wave, _gameTimer);
        if (goResult != 0) { StopSound(_buttonPressSound); PlaySound(_buttonPressSound); }
        if (goResult == 1) { ResetRunState(); _fadeInTimer = 2.0f; _gameState = GameState::Play; }
        else if (goResult == 2) { ResetRunState(); _menu.Init(); _gameState = GameState::Menu; }
        else if (goResult == 3) _shouldClose = true;
        break;
    }

    case GameState::HowToPlay:
    {
        DrawHowToPlay();
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
        else
            _player.UndoMovement();
    }

    for (auto& prop : _props)
    {
        if (CheckCollisionRecs(prop.GetCollisionRec(), _player.GetCollisionRec()))
        {
            if (_player.IsBeingForcedPushed())
                _player.OnForcedPushCollision();
            else
                _player.UndoMovement();
        }

        for (auto& enemy : _enemies)
        {
            if (!enemy->IsActive())
                continue;
            if (enemy->IgnoresPropCollisions())
                continue;
            if (CheckCollisionRecs(prop.GetCollisionRec(), enemy->GetCollisionRec()))
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

            _player.AddExp(enemy->GetExpValue());
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

    for (const auto& projectile : _fireballProjectiles)
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
}

void Engine::DrawHUD()
{
    int fontSize = 30;

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight() / 8, Fade(BLACK, 0.6f));

    DrawText(TextFormat("Time: %.1f", _gameTimer), 85 + GetScreenWidth() / 2 - 150, 60, fontSize, RAYWHITE);
    DrawText(("Wave: " + std::to_string(_wave)).c_str(), 20, 10, 30, RAYWHITE);
    DrawText(("Enemies Left: " + std::to_string(GetActiveEnemyCount())).c_str(), 20, 60, 30, RAYWHITE);
    DrawText(TextFormat("Health: %.1f", _player.GetHealthValue()), GetScreenWidth() - 200, 20, 30, RAYWHITE);

    // EXP bar and level display
    int level = _player.GetLevel();
    int exp = _player.GetExp();
    int expToNext = _player.GetExpToNext();

    const float barW = 200.f;
    const float barH = 14.f;
    const float barX = GetScreenWidth() / 2.f - barW / 2.f;
    const float barY = 12.f;

    DrawRectangleRounded(Rectangle{ barX - 2.f, barY - 2.f, barW + 4.f, barH + 4.f }, 0.5f, 4, Fade(BLACK, 0.8f));
    DrawRectangleRounded(Rectangle{ barX, barY, barW, barH }, 0.5f, 4, Fade(DARKGRAY, 0.9f));

    if (level < 10)
    {
        float expPercent = (float)exp / (float)expToNext;
        DrawRectangleRounded(Rectangle{ barX, barY, barW * expPercent, barH }, 0.5f, 4, Fade(YELLOW, 0.9f));
    }

    const char* levelText = level < 10
        ? TextFormat("Lv.%d  %d/%d EXP", level, exp, expToNext)
        : "Lv.MAX";

    int textW = MeasureText(levelText, 18);
    DrawText(levelText, (int)(barX + barW / 2.f - textW / 2.f), (int)(barY + barH + 4.f), 18, YELLOW);

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
    // 3 slots: 0=Fireball, 1=SwordBeam, 2=Freeze
    const int   slotCount  = 3;
    const float slotSize   = 90.f;
    const float slotGap    = 10.f;
    const float totalW     = slotCount * slotSize + (slotCount - 1) * slotGap;
    const float startX     = GetScreenWidth() / 2.f - totalW / 2.f;
    const float slotY      = GetScreenHeight() - slotSize - 14.f;
    const int   selected   = _player.GetSelectedAbility();

    int ammo[3] = {
        _player.GetFireballAmmo(),
        _player.GetSwordBeamAmmo(),
        _player.GetFreezeAmmo()
    };

    for (int i = 0; i < slotCount; i++)
    {
        float x = startX + i * (slotSize + slotGap);
        Rectangle slot{ x, slotY, slotSize, slotSize };

        // Background
        DrawRectangleRounded(slot, 0.18f, 6,
            i == selected ? Fade(BLACK, 0.88f) : Fade(BLACK, 0.55f));

        // Gold border on selected slot
        Color borderCol = (i == selected) ? GOLD : Fade(LIGHTGRAY, 0.35f);
        DrawRectangleRoundedLines(slot, 0.18f, 6, borderCol);

        // Slot number
        DrawText(TextFormat("%d", i + 1), (int)(x + 6.f), (int)(slotY + 6.f), 16,
            i == selected ? GOLD : Fade(WHITE, 0.5f));

        // Icon
        float cx = x + slotSize * 0.42f;
        float cy = slotY + slotSize * 0.46f;

        if (i == 0) // Fireball
        {
            Texture2D tex = FireBallPickup::GetSharedTexture();
            float sc = 2.8f;
            DrawTextureEx(tex,
                Vector2{ cx - tex.width * sc * 0.5f, cy - tex.height * sc * 0.5f },
                0.f, sc, ammo[i] > 0 ? WHITE : Fade(WHITE, 0.3f));
        }
        else if (i == 1) // Sword Beam
        {
            Texture2D tex = SwordBeamPickup::GetSharedTexture();
            float sc = 2.9f;
            DrawTextureEx(tex,
                Vector2{ cx - tex.width * sc * 0.5f, cy - tex.height * sc * 0.5f },
                0.f, sc, ammo[i] > 0 ? WHITE : Fade(WHITE, 0.3f));
        }
        else if (i == 2) // Freeze
        {
            Texture2D tex = FreezePickup::GetSharedTexture();
            float sc = 2.9f;
            DrawTextureEx(tex,
                Vector2{ cx - tex.width * sc * 0.5f, cy - tex.height * sc * 0.5f },
                0.f, sc, ammo[i] > 0 ? WHITE : Fade(WHITE, 0.3f));
        }

        // Ammo count
        DrawText(TextFormat("x%d", ammo[i]),
            (int)(x + slotSize - MeasureText(TextFormat("x%d", ammo[i]), 18) - 6.f),
            (int)(slotY + slotSize - 22.f), 18,
            ammo[i] > 0 ? RAYWHITE : Fade(GRAY, 0.6f));
    }

    // "RMB to use" hint below bar
    const char* hint = "RMB to use";
    int hw = MeasureText(hint, 14);
    DrawText(hint, (int)(GetScreenWidth() / 2.f - hw / 2.f),
        (int)(slotY + slotSize + 2.f), 14, Fade(WHITE, 0.45f));

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
        std::string waveText = "Wave " + std::to_string(_wave);
        int textWidth = MeasureText(waveText.c_str(), fontSize);

        DrawText(waveText.c_str(), GetScreenWidth() / 2 - textWidth / 2, GetScreenHeight() / 2 - 30, fontSize, YELLOW);
    }
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

        if (CheckCollisionRecs(attackRec, enemy->GetCollisionRec()))
        {
            enemy->TakeDamage(2, _player.GetWorldPos());
            SpawnHitEffect(Character::CastType::None, enemy->GetWorldPos(), _player.GetFacingDirection());
            hitAny = true;
        }
    }

    _player.ConsumeMeleeDamageFrame();

    if (hitAny)
        TriggerScreenShake(6.f, 0.07f);
}

void Engine::SpawnFireballBurst()
{
    static const std::array<Vector2, 8> directions{
        Vector2{ 1.f, 0.f },
        Vector2{ -1.f, 0.f },
        Vector2{ 0.f, 1.f },
        Vector2{ 0.f, -1.f },
        Vector2{ 1.f, 1.f },
        Vector2{ 1.f, -1.f },
        Vector2{ -1.f, 1.f },
        Vector2{ -1.f, -1.f }
    };

    Vector2 origin = _player.GetCastOrigin();

    for (const Vector2& direction : directions)
    {
        FireballProjectile projectile;
        projectile.Init(origin, direction);
        _fireballProjectiles.push_back(projectile);
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
    case Character::CastType::Fireball:
        effect.texture = &_fireballCastTex;
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

    case Character::CastType::Fireball:
        effect.texture = &_fireballHitTex;
        effect.frameCount = 6;
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

            if (CheckCollisionRecs(projectile.GetCollisionRec(), enemy->GetCollisionRec()))
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

void Engine::UpdateFireballProjectiles(float dt)
{
    for (auto& projectile : _fireballProjectiles)
    {
        if (!projectile.IsActive())
            continue;

        projectile.Update(dt);

        if (!projectile.IsActive())
            continue;

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

        for (auto& enemy : _enemies)
        {
            if (!enemy->IsActive() || !enemy->IsAlive())
                continue;

            if (CheckCollisionRecs(projectile.GetCollisionRec(), enemy->GetCollisionRec()))
            {
                // Boss tuning: fireball should always land for 1 direct damage
                // on the Molarbeast, while regular enemies still use the
                // player's scaled fireball damage.
                int fireballDamage = (enemy->AsMolarbeast() != nullptr)
                    ? 1
                    : _player.GetFireballHitDamage();
                enemy->TakeDamage(fireballDamage, _player.GetWorldPos());
                enemy->ApplyBurn(1.f, _player.GetFireballBurnDamage(), _player.GetWorldPos());
                SpawnHitEffect(Character::CastType::Fireball, projectile.GetWorldPos(), projectile.GetDirection());
                projectile.Destroy();
                TriggerScreenShake(4.f, 0.05f);
                StopSound(_explosionSound);
                PlaySound(_explosionSound);
                break;
            }
        }

    }

    _fireballProjectiles.erase(
        std::remove_if(_fireballProjectiles.begin(), _fireballProjectiles.end(),
            [](const FireballProjectile& projectile) { return !projectile.IsActive(); }),
        _fireballProjectiles.end());
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

            if (CheckCollisionRecs(projectile.GetCollisionRec(), enemy->GetCollisionRec()))
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
    // Normal-wave drops should smooth out run pacing a bit more than before.
    // Keeping the total chance conservative avoids flooding the arena, while
    // shifting more weight into heal and freeze makes non-boss waves less
    // feast-or-famine.
    const int dropChancePercent = 22;

    if (GetRandomValue(1, 100) > dropChancePercent)
        return;

    // Weighted pool:
    // FireBall  = 32
    // SwordBeam = 26
    // Freeze    = 16
    // Heal      = 26
    // Total     = 100
    int roll = GetRandomValue(1, 100);
    std::unique_ptr<Pickup> pickup;

    if (roll <= 32)
    {
        auto p = std::make_unique<FireBallPickup>();
        p->Init(worldPos, 1);
        pickup = std::move(p);
    }
    else if (roll <= 58)
    {
        auto p = std::make_unique<SwordBeamPickup>();
        p->Init(worldPos, 1);
        pickup = std::move(p);
    }
    else if (roll <= 74)
    {
        auto p = std::make_unique<FreezePickup>();
        p->Init(worldPos);
        pickup = std::move(p);
    }
    else
    {
        auto p = std::make_unique<HealPickup>();
        p->Init(worldPos);
        pickup = std::move(p);
    }

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

    if (pickup->GetType() == PickupType::FireBall)
        static_cast<FireBallPickup*>(pickup.get())->Init(dropPos, 1);
    else if (pickup->GetType() == PickupType::SwordBeam)
        static_cast<SwordBeamPickup*>(pickup.get())->Init(dropPos, 1);
    else if (pickup->GetType() == PickupType::Freeze)
        static_cast<FreezePickup*>(pickup.get())->Init(dropPos);
    else
        static_cast<HealPickup*>(pickup.get())->Init(dropPos);

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

    // Boss-fight testing pool: only combat pickups are spawned from the timed
    // system so every drop helps tune the actual fight loop.
    std::unique_ptr<Pickup> pickup;

    if (IsBossFightActive())
    {
        int roll = GetRandomValue(0, 2);

        if (roll == 0)
        {
            auto p = std::make_unique<FireBallPickup>();
            p->Init(pos);
            p->SetTimerSpawned(true);
            pickup = std::move(p);
        }
        else if (roll == 1)
        {
            auto p = std::make_unique<SwordBeamPickup>();
            p->Init(pos);
            p->SetTimerSpawned(true);
            pickup = std::move(p);
        }
        else
        {
            auto p = std::make_unique<FreezePickup>();
            p->Init(pos);
            p->SetTimerSpawned(true);
            pickup = std::move(p);
        }
    }
    else if (GetRandomValue(0, 1) == 0)
    {
        auto p = std::make_unique<FreezePickup>();
        p->Init(pos);
        p->SetTimerSpawned(true);
        pickup = std::move(p);
    }
    else
    {
        auto p = std::make_unique<HealPickup>();
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
    _wave = 0;
    _navRefreshTimer = 0.f;
    _lastPlayerNavIndex = -1;
    _gameTimer = 0.f;
    _playerDying = false;
    _waveStarting = true;
    _bossWarningTimer = 0.f;
    _fireballProjectiles.clear();
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
    if (wave < 4)
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

