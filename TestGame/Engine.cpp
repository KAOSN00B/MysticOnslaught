#include "Engine.h"

#include "AnimationUtils.h"
#include "raymath.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <queue>

namespace
{
    int ClampInt(int value, int minValue, int maxValue)
    {
        if (value < minValue)
            return minValue;
        if (value > maxValue)
            return maxValue;
        return value;
    }
}

Engine::Engine()
{
    Init();
}

Engine::~Engine()
{
    Enemy::UnloadSharedResources();
    FireBallPickup::UnloadSharedResources();
    SwordBeamPickup::UnloadSharedResources();
    FreezePickup::UnloadSharedResources();
    HealPickup::UnloadSharedResources();
    FireballProjectile::UnloadSharedResources();
    SwordBeamProjectile::UnloadSharedResources();
    FreezeProjectile::UnloadSharedResources();
    UnloadSound(_pickupSound);
    UnloadSound(_fireballCastSound);
    UnloadSound(_explosionSound);
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

    _pickupSound      = LoadSound("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Sounds\\PickupSound.mp3");
    _fireballCastSound= LoadSound("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Sounds\\GS1_Spell_Fire.mp3");
    _explosionSound   = LoadSound("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Sounds\\GS1_Spell_Explode.mp3");
    _bladeBeamSound   = LoadSound("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Sounds\\GS1_BladeBeam.mp3");
    _buttonPressSound = LoadSound("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Sounds\\ButtonPress.mp3");

    SetSoundPitch (_buttonPressSound, 1.25f);
    SetSoundVolume(_buttonPressSound, 0.35f);

    SetSoundPitch (_pickupSound, 1.35f);
    SetSoundVolume(_pickupSound, 0.45f);

    _pauseUI.Init();

    _menu.Init();

    _map = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\TileSet\\Map.png");
    _pillarTex = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\TileSet\\Pillar.png");
    _fireballCastTex = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\PowerUps\\Fireball_Cast.png");
    _fireballHitTex = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\PowerUps\\Fireball_Hit.png");
    _swordBeamCastTex = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\PowerUps\\BladeBeam_Cast.png");
    _swordBeamHitTex = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\PowerUps\\Hit03.png");
    _freezeCastTex = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\PowerUps\\Ice_Shard_Cast.png");
    _freezeHitTex = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\PowerUps\\Ice_Shard_Hit.png");
    _healEffectTex = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\PowerUps\\Health_Up.png");

    _props.clear();
    _pickups.clear();
    _fireballProjectiles.clear();
    _swordBeamProjectiles.clear();
    _freezeProjectiles.clear();
    _effects.clear();
    _enemies.clear();

    int propCount = GetRandomValue(7, 10);

    for (int i = 0; i < propCount; i++)
    {
        Vector2 pos = GetRandomPropPosition();
        _props.push_back(Prop{ pos, _pillarTex });
    }

    BuildNavigationGrid();

    _pickupSpawnTimer = 60.f;

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
    int enemyCount = std::min(_wave * 2, _maxActiveEnemies);

    float mapW = _map.width * _mapScale;
    float mapH = _map.height * _mapScale;

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
        enemy->SetWaveScale(_wave);
        enemy->SetTarget(&_player);
        _enemies.push_back(std::move(enemy));
    }
}

void Engine::Run()
{
    while (!WindowShouldClose() && !_shouldClose)
    {
        float dt = GetFrameTime();

        Update(dt);

        BeginDrawing();
        ClearBackground(WHITE);
        Draw();
        EndDrawing();
    }
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

    if (_player.GetHealth() <= 0 && !_playerDying)
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
        }
    }
    else
    {
        _gameTimer += dt;

        // Timed pickup spawn — every 60 seconds
        _pickupSpawnTimer -= dt;
        if (_pickupSpawnTimer <= 0.f)
        {
            SpawnTimedPickup();
            _pickupSpawnTimer = 60.f;
        }

        if (GetActiveEnemyCount() == 0)
            SpawnWave();

        _navRefreshTimer -= dt;
        int playerCol = ClampInt((int)(_player.GetWorldPos().x / _navCellSize), 0, std::max(0, _navCols - 1));
        int playerRow = ClampInt((int)(_player.GetWorldPos().y / _navCellSize), 0, std::max(0, _navRows - 1));
        int playerNavIndex = (_navCols > 0 && _navRows > 0) ? GetClosestOpenNavigationIndex(playerCol, playerRow) : -1;
        if (_navRefreshTimer <= 0.f || playerNavIndex != _lastPlayerNavIndex)
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

            Vector2 navigationTarget = _player.GetWorldPos();
            bool hasNavigationTarget = false;

            if (!HasLineOfSight(enemy->GetWorldPos(), _player.GetWorldPos()))
            {
                navigationTarget = GetNavigationTarget(enemy->GetWorldPos(), _player.GetWorldPos());
                hasNavigationTarget = !Vector2Equals(navigationTarget, _player.GetWorldPos());
            }

            enemy->Update(dt, _player.GetWorldPos(), navigationTarget, hasNavigationTarget, _enemies, propCenters);
        }

        HandlePlayerMeleeDamage();
        UpdateFireballProjectiles(dt);
        UpdateSwordBeamProjectiles(dt);
        UpdateFreezeProjectiles(dt);
        UpdateEffects(dt);
        UpdateEnemyCount(dt);
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
        _player.UndoMovement();
    }

    const float enemyMargin = 80.f;

    for (auto& prop : _props)
    {
        if (CheckCollisionRecs(prop.GetCollisionRec(), _player.GetCollisionRec()))
            _player.UndoMovement();

        for (auto& enemy : _enemies)
        {
            if (!enemy->IsActive())
                continue;
            if (CheckCollisionRecs(prop.GetCollisionRec(), enemy->GetCollisionRec()))
                enemy->UndoMovement();
        }
    }

    // Push enemies back inside map bounds
    for (auto& enemy : _enemies)
    {
        if (!enemy->IsActive())
            continue;
        if (enemy->IsDying()) continue;
        Vector2 pos = enemy->GetWorldPos();
        if (pos.x < enemyMargin || pos.y < enemyMargin ||
            pos.x > mapW - enemyMargin || pos.y > mapH - enemyMargin)
        {
            enemy->UndoMovement();
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
    DrawText(("Health: " + std::to_string(_player.GetHealth())).c_str(), GetScreenWidth() - 200, 20, 30, RAYWHITE);

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

        // Freezes first enemy hit then is destroyed (no piercing)
        for (auto& enemy : _enemies)
        {
            if (!enemy->IsActive())
                continue;
            if (!enemy->IsAlive())
                continue;

            if (CheckCollisionRecs(projectile.GetCollisionRec(), enemy->GetCollisionRec()))
            {
                float duration = GetRandomValue(3, 5) * 1.f;
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
            if (!enemy->IsActive())
                continue;
            if (!enemy->IsAlive())
                continue;

            if (CheckCollisionRecs(projectile.GetCollisionRec(), enemy->GetCollisionRec()))
            {
                enemy->TakeDamage(1, _player.GetWorldPos());
                enemy->ApplyBurn(1.f, 1, _player.GetWorldPos());
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
            if (!enemy->IsActive())
                continue;
            if (!enemy->IsAlive() || projectile.HasHitEnemy(enemy.get()))
                continue;

            if (CheckCollisionRecs(projectile.GetCollisionRec(), enemy->GetCollisionRec()))
            {
                enemy->TakeDamage(1, _player.GetWorldPos());
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
    const int dropChancePercent = 20;

    if (GetRandomValue(1, 100) > dropChancePercent)
        return;

    // Weighted pool: FireBall=40, SwordBeam=40, Freeze=12, Heal=8  (total=100)
    int roll = GetRandomValue(1, 100);
    std::unique_ptr<Pickup> pickup;

    if (roll <= 40)
    {
        auto p = std::make_unique<FireBallPickup>();
        p->Init(worldPos, 1);
        pickup = std::move(p);
    }
    else if (roll <= 80)
    {
        auto p = std::make_unique<SwordBeamPickup>();
        p->Init(worldPos, 1);
        pickup = std::move(p);
    }
    else if (roll <= 92)
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

    // Timed pickups are either Freeze or Heal (50/50)
    std::unique_ptr<Pickup> pickup;

    if (GetRandomValue(0, 1) == 0)
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
        DrawCircleV(dot, 3.f, Fade(RED, 0.9f));
    }

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

    float minX = mapW * 0.05f;
    float maxX = mapW * 0.95f;
    float minY = mapH * 0.05f;
    float maxY = mapH * 0.85f;

    float minSpacing = 308.f;
    int attempts = 0;
    const int maxAttempts = 50;

    while (attempts < maxAttempts)
    {
        Vector2 pos{};
        pos.x = (float)GetRandomValue((int)minX, (int)maxX);
        pos.y = (float)GetRandomValue((int)minY, (int)maxY);

        bool tooClose = false;

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

void Engine::RefreshNavigationField()
{
    if (_navCols <= 0 || _navRows <= 0)
        return;

    _navDistance.assign(_navCols * _navRows, std::numeric_limits<int>::max());

    int playerCol = ClampInt((int)(_player.GetWorldPos().x / _navCellSize), 0, _navCols - 1);
    int playerRow = ClampInt((int)(_player.GetWorldPos().y / _navCellSize), 0, _navRows - 1);
    int targetIndex = GetClosestOpenNavigationIndex(playerCol, playerRow);
    _lastPlayerNavIndex = targetIndex;

    if (targetIndex < 0)
        return;

    using QueueItem = std::pair<int, int>;
    std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> frontier;
    _navDistance[targetIndex] = 0;
    frontier.push({ 0, targetIndex });

    struct SearchOffset { int col, row, cost; };
    static const std::array<SearchOffset, 8> navOffsets{{
        {1,0,10},{-1,0,10},{0,1,10},{0,-1,10},
        {1,1,14},{1,-1,14},{-1,1,14},{-1,-1,14}
    }};

    while (!frontier.empty())
    {
        int cost         = frontier.top().first;
        int currentIndex = frontier.top().second;
        frontier.pop();

        if (cost > _navDistance[currentIndex])
            continue;

        int currentCol = currentIndex % _navCols;
        int currentRow = currentIndex / _navCols;

        for (const SearchOffset& off : navOffsets)
        {
            int nc = currentCol + off.col;
            int nr = currentRow + off.row;

            if (IsNavigationCellBlocked(nc, nr))
                continue;

            if (off.col != 0 && off.row != 0)
            {
                if (IsNavigationCellBlocked(currentCol + off.col, currentRow) ||
                    IsNavigationCellBlocked(currentCol, currentRow + off.row))
                    continue;
            }

            int nextIndex = GetNavigationIndex(nc, nr);
            int nextCost  = cost + off.cost;

            if (nextCost >= _navDistance[nextIndex])
                continue;

            _navDistance[nextIndex] = nextCost;
            frontier.push({ nextCost, nextIndex });
        }
    }
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

    return true;
}

void Engine::ResetRunState()
{
    _wave = 0;
    _navRefreshTimer = 0.f;
    _lastPlayerNavIndex = -1;
    _gameTimer = 0.f;
    _playerDying = false;
    _waveStarting = true;
    _fireballProjectiles.clear();
    _swordBeamProjectiles.clear();
    _freezeProjectiles.clear();
    _pickups.clear();
    _effects.clear();
    _player.Init();
    _player.SetWorldPos(Vector2{ _map.width * _mapScale * 0.5f, _map.height * _mapScale * 0.5f });

    for (auto& enemy : _enemies)
    {
        enemy->SetActive(false);
        enemy->Teleport(Vector2{ -5000.f, -5000.f });
    }

    _pickupSpawnTimer = 60.f;

    RefreshNavigationField();
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

bool Engine::TryGetPooledEnemySpawn(Vector2 pos)
{
    for (auto& enemy : _enemies)
    {
        if (enemy->IsActive())
            continue;

        enemy->ResetForSpawn(pos);
        enemy->SetTarget(&_player);
        return true;
    }

    return false;
}

