#include "Engine.h"

#include "AnimationUtils.h"
#include "raymath.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <queue>

Engine::Engine()
{
    Init();
}

Engine::~Engine()
{
    FireBallPickup::UnloadSharedResources();
    SwordBeamPickup::UnloadSharedResources();
    FreezePickup::UnloadSharedResources();
    HealPickup::UnloadSharedResources();
    FireballProjectile::UnloadSharedResources();
    SwordBeamProjectile::UnloadSharedResources();
    FreezeProjectile::UnloadSharedResources();
    UnloadTexture(_map);
    UnloadTexture(_pillarTex);
    UnloadTexture(_fireballCastTex);
    UnloadTexture(_fireballHitTex);
    UnloadTexture(_swordBeamCastTex);
    UnloadTexture(_swordBeamHitTex);
    UnloadTexture(_freezeCastTex);
    UnloadTexture(_freezeHitTex);
    CloseWindow();
}

void Engine::Init()
{
    InitWindow(_windowWidth, _windowHeight, "Top Down Game");
    SetTargetFPS(60);
    InitAudioDevice();

    SetExitKey(KEY_NULL);

    _menu.Init();
    _player.Init();

    _map = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\TileSet\\Map.png");
    _pillarTex = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\TileSet\\Pillar.png");
    _fireballCastTex = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\PowerUps\\Fireball_Cast.png");
    _fireballHitTex = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\PowerUps\\Fireball_Hit.png");
    _swordBeamCastTex = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\PowerUps\\BladeBeam_Cast.png");
    _swordBeamHitTex = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\PowerUps\\Hit03.png");
    _freezeCastTex = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\PowerUps\\Ice_Shard_Cast.png");
    _freezeHitTex = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\PowerUps\\Ice_Shard_Hit.png");

    _props.clear();
    _pickups.clear();
    _fireballProjectiles.clear();
    _swordBeamProjectiles.clear();
    _freezeProjectiles.clear();
    _effects.clear();
    _enemies.clear();

    int propCount = GetRandomValue(20, 30);

    for (int i = 0; i < propCount; i++)
    {
        Vector2 pos = GetRandomPropPosition();
        _props.push_back(Prop{ pos, _pillarTex });
    }

    BuildNavigationGrid();

    auto starterPickup = std::make_unique<FireBallPickup>();
    starterPickup->Init(Vector2Add(_player.GetWorldPos(), Vector2{ 1000.f, 1000.f }), 1);
    _pickups.push_back(std::move(starterPickup));

    _pickupSpawnTimer = 60.f;

    _wave = 0;
    SpawnWave();
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
    int enemyCount = _wave * 2;

    float mapW = _map.width * _mapScale;
    float mapH = _map.height * _mapScale;

    for (int i = 0; i < enemyCount; i++)
    {
        Vector2 pos{};
        int attempts = 0;

        do
        {
            pos.x = (float)GetRandomValue(200, (int)mapW - 200);
            pos.y = (float)GetRandomValue(200, (int)mapH - 200);
            attempts++;
        } while (!IsSpawnPositionValid(pos) && attempts < 40);

        auto enemy = std::make_unique<Enemy>(pos);
        enemy->Init();
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
            _gameState = GameState::Play;

        if (_menu.QuitPressed())
            _shouldClose = true;

        if (_menu.HowToPressed())
            _gameState = GameState::HowToPlay;

        break;
    }

    case GameState::HowToPlay:
    {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE))
        {
            _menu.Init();
            _gameState = GameState::Menu;
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
    {
        if (IsKeyPressed(KEY_ENTER))
        {
            _wave = 0;
            _gameTimer = 0.f;
            _playerDying = false;
            _waveStarting = true;
            _fireballProjectiles.clear();
            _swordBeamProjectiles.clear();
            _freezeProjectiles.clear();
            _pickups.clear();
            _enemies.clear();
            _player.Init();

            auto starterPickup = std::make_unique<FireBallPickup>();
            starterPickup->Init(Vector2Add(_player.GetWorldPos(), Vector2{ 1000.f, 1000.f }), 1);
            _pickups.push_back(std::move(starterPickup));
            _pickupSpawnTimer = 60.f;

            SpawnWave();
            _gameState = GameState::Menu;
        }
        break;
    }
    }
}

void Engine::UpdateGamePlay(float dt)
{
    if (IsKeyPressed(KEY_ESCAPE))
    {
        _gameState = GameState::Pause;
        return;
    }

    _player.Update(dt);

    Character::CastType castType = _player.ConsumeCastRequest();

    if (castType == Character::CastType::Fireball)
    {
        SpawnCastEffect(castType);
        SpawnFireballBurst();
    }
    else if (castType == Character::CastType::SwordBeam)
    {
        SpawnCastEffect(castType);
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

        if (_enemies.empty())
            SpawnWave();

        for (auto& enemy : _enemies)
        {
            Vector2 navigationTarget = _player.GetWorldPos();
            bool hasNavigationTarget = false;

            if (!HasLineOfSight(enemy->GetWorldPos(), _player.GetWorldPos()))
            {
                navigationTarget = GetNavigationTarget(enemy->GetWorldPos(), _player.GetWorldPos());
                hasNavigationTarget = !Vector2Equals(navigationTarget, _player.GetWorldPos());
            }

            enemy->Update(dt, _player.GetWorldPos(), navigationTarget, hasNavigationTarget, _enemies);
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
            pickup->OnCollect(_player);
        }
    }

    _pickups.erase(
        std::remove_if(_pickups.begin(), _pickups.end(),
            [](const std::unique_ptr<Pickup>& pickup) { return !pickup->IsActive(); }),
        _pickups.end());

    HandleCollisions();

    _mapPos = Vector2Scale(_player.GetWorldPos(), -1.f);

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
        break;
    }

    case GameState::Pause:
    {
        Vector2 shakenMapPos = Vector2Add(_mapPos, _shakeOffset);

        DrawTextureEx(_map, shakenMapPos, 0.0f, _mapScale, WHITE);

        for (auto& prop : _props)
            prop.Render(Vector2Subtract(_player.GetWorldPos(), _shakeOffset));

        for (auto& pickup : _pickups)
            pickup->Draw(shakenMapPos);

        for (const auto& projectile : _fireballProjectiles)
            projectile.Draw(shakenMapPos);

        for (const auto& projectile : _swordBeamProjectiles)
            projectile.Draw(shakenMapPos);

        for (const auto& projectile : _freezeProjectiles)
            projectile.Draw(shakenMapPos);

        DrawEffects(shakenMapPos);

        for (auto& enemy : _enemies)
            enemy->DrawEnemy(Vector2Subtract(_player.GetWorldPos(), _shakeOffset));

        _player.DrawPlayer();
        DrawHowToPlay(true);

        break;
    }

    case GameState::GameOver:
    {
        if (_pauseUI.DrawGameOver(_wave, _gameTimer))
            _shouldClose = true;

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
    if (_player.GetWorldPos().x < 0.0f || _player.GetWorldPos().y < 0.0f
        || _player.GetWorldPos().x + _windowWidth > _map.width * _mapScale
        || _player.GetWorldPos().y + _windowHeight > _map.height * _mapScale)
    {
        _player.UndoMovement();
    }

    float mapW = _map.width  * _mapScale;
    float mapH = _map.height * _mapScale;
    const float enemyMargin = 80.f;

    for (auto& prop : _props)
    {
        if (CheckCollisionRecs(prop.GetCollisionRec(), _player.GetCollisionRec()))
            _player.UndoMovement();

        for (auto& enemy : _enemies)
        {
            if (CheckCollisionRecs(prop.GetCollisionRec(), enemy->GetCollisionRec()))
                enemy->UndoMovement();
        }
    }

    // Push enemies back inside map bounds
    for (auto& enemy : _enemies)
    {
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
    for (int i = static_cast<int>(_enemies.size()) - 1; i >= 0; i--)
    {
        // Save position BEFORE UpdateDeath calls Death() which teleports to {-1000,-1000}
        Vector2 dropPos = _enemies[i]->GetWorldPos();
        if (_enemies[i]->UpdateDeath(dt))
        {
            _player.AddExp(_enemies[i]->GetExpValue());
            SpawnEnemyDrop(dropPos);
            _enemies.erase(_enemies.begin() + i);
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
    Vector2 shakenMapPos = Vector2Add(_mapPos, _shakeOffset);

    DrawTextureEx(_map, shakenMapPos, 0.0f, _mapScale, WHITE);

    for (auto& prop : _props)
        prop.Render(Vector2Subtract(_player.GetWorldPos(), _shakeOffset));

    for (auto& pickup : _pickups)
        pickup->Draw(shakenMapPos);

    for (const auto& projectile : _fireballProjectiles)
        projectile.Draw(shakenMapPos);

    for (const auto& projectile : _swordBeamProjectiles)
        projectile.Draw(shakenMapPos);

    for (const auto& projectile : _freezeProjectiles)
        projectile.Draw(shakenMapPos);

    DrawEffects(shakenMapPos);

    for (auto& enemy : _enemies)
        enemy->DrawEnemy(Vector2Subtract(_player.GetWorldPos(), _shakeOffset));

    _player.DrawPlayer();
}

void Engine::DrawHUD()
{
    int fontSize = 30;

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight() / 8, Fade(BLACK, 0.6f));

    DrawText(TextFormat("Time: %.1f", _gameTimer), 85 + GetScreenWidth() / 2 - 150, 60, fontSize, RAYWHITE);
    DrawText(("Wave: " + std::to_string(_wave)).c_str(), 20, 10, 30, RAYWHITE);
    DrawText(("Enemies Left: " + std::to_string(_enemies.size())).c_str(), 20, 60, 30, RAYWHITE);
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
        if (!enemy->IsAlive())
            continue;

        if (CheckCollisionRecs(attackRec, enemy->GetCollisionRec()))
        {
            enemy->TakeDamage(2, _player.GetWorldPos());
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
            ? Vector2Add(_player.GetCastOrigin(), Vector2{ effect.offset.x, effect.offset.y })
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
            Vector2{ dest.width * 0.5f, dest.height * 0.5f }, rotation, WHITE);
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
    case Character::CastType::Fireball:
        effect.texture = &_fireballHitTex;
        effect.frameCount = 6;
        effect.scale = 4.f;
        break;

    case Character::CastType::SwordBeam:
        effect.texture = &_swordBeamHitTex;
        effect.frameCount = 5;
        effect.scale = 3.5f;
        break;

    case Character::CastType::Freeze:
        effect.texture = &_freezeHitTex;
        effect.frameCount = 5;
        effect.scale = 4.f;
        break;

    default:
        effect.active = false;
        break;
    }

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
                SpawnHitEffect(Character::CastType::Freeze, projectile.GetWorldPos(), projectile.GetDirection());
                projectile.Destroy();
                break;
            }
        }

        if (!projectile.IsActive())
            continue;

        // Freezes first enemy hit then is destroyed (no piercing)
        for (auto& enemy : _enemies)
        {
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
                SpawnHitEffect(Character::CastType::Fireball, projectile.GetWorldPos(), projectile.GetDirection());
                projectile.Destroy();
                break;
            }
        }

        if (!projectile.IsActive())
            continue;

        for (auto& enemy : _enemies)
        {
            if (!enemy->IsAlive())
                continue;

            if (CheckCollisionRecs(projectile.GetCollisionRec(), enemy->GetCollisionRec()))
            {
                enemy->TakeDamage(2, _player.GetWorldPos());
                enemy->ApplyBurn(1.f, 1, _player.GetWorldPos());
                SpawnHitEffect(Character::CastType::Fireball, projectile.GetWorldPos(), projectile.GetDirection());
                projectile.Destroy();
                TriggerScreenShake(4.f, 0.05f);
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

        for (auto& prop : _props)
        {
            if (CheckCollisionRecs(projectile.GetCollisionRec(), prop.GetCollisionRec()))
            {
                SpawnHitEffect(Character::CastType::SwordBeam, projectile.GetWorldPos(), projectile.GetDirection());
                projectile.Destroy();
                break;
            }
        }

        if (!projectile.IsActive())
            continue;

        for (auto& enemy : _enemies)
        {
            if (!enemy->IsAlive() || projectile.HasHitEnemy(enemy.get()))
                continue;

            if (CheckCollisionRecs(projectile.GetCollisionRec(), enemy->GetCollisionRec()))
            {
                enemy->TakeDamage(4, _player.GetWorldPos());
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

void Engine::BuildNavigationGrid()
{
    float mapW = _map.width * _mapScale;
    float mapH = _map.height * _mapScale;

    _navCols = std::max(1, (int)std::ceil(mapW / _navCellSize));
    _navRows = std::max(1, (int)std::ceil(mapH / _navCellSize));
    _navBlocked.assign(_navCols * _navRows, false);

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
    if (col < 0 || row < 0 || col >= _navCols || row >= _navRows)
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
        for (int rowOffset = -radius; rowOffset <= radius; ++rowOffset)
        {
            for (int colOffset = -radius; colOffset <= radius; ++colOffset)
            {
                int nextCol = col + colOffset;
                int nextRow = row + rowOffset;

                if (IsNavigationCellBlocked(nextCol, nextRow))
                    continue;

                col = nextCol;
                row = nextRow;
                return true;
            }
        }
    }

    return false;
}

bool Engine::HasLineOfSight(Vector2 start, Vector2 end) const
{
    Vector2 delta = Vector2Subtract(end, start);
    float length = Vector2Length(delta);

    if (length < 1.f)
        return true;

    Vector2 direction = Vector2Normalize(delta);
    float step = 24.f;

    for (float distance = 0.f; distance <= length; distance += step)
    {
        Vector2 sample = Vector2Add(start, Vector2Scale(direction, distance));
        Rectangle probe{ sample.x - 10.f, sample.y - 10.f, 20.f, 20.f };

        for (auto& prop : _props)
        {
            if (CheckCollisionRecs(probe, prop.GetCollisionRec()))
                return false;
        }
    }

    return true;
}

Vector2 Engine::GetNavigationTarget(Vector2 startWorldPos, Vector2 targetWorldPos) const
{
    if (_navCols <= 0 || _navRows <= 0)
        return targetWorldPos;

    auto clampToGrid = [](int value, int maxValue)
    {
        if (value < 0)
            return 0;
        if (value > maxValue)
            return maxValue;
        return value;
    };

    int startCol = clampToGrid((int)(startWorldPos.x / _navCellSize), _navCols - 1);
    int startRow = clampToGrid((int)(startWorldPos.y / _navCellSize), _navRows - 1);
    int targetCol = clampToGrid((int)(targetWorldPos.x / _navCellSize), _navCols - 1);
    int targetRow = clampToGrid((int)(targetWorldPos.y / _navCellSize), _navRows - 1);

    if (!FindNearestOpenCell(startCol, startRow) || !FindNearestOpenCell(targetCol, targetRow))
        return targetWorldPos;

    int startIndex = GetNavigationIndex(startCol, startRow);
    int targetIndex = GetNavigationIndex(targetCol, targetRow);

    if (startIndex == targetIndex)
        return targetWorldPos;

    std::vector<int> distance(_navCols * _navRows, std::numeric_limits<int>::max());
    std::vector<int> previous(_navCols * _navRows, -1);

    using QueueItem = std::pair<int, int>;
    std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> frontier;

    distance[startIndex] = 0;
    frontier.push({ 0, startIndex });

    struct SearchOffset
    {
        int col;
        int row;
        int cost;
    };

    static const std::array<SearchOffset, 8> offsets{
        SearchOffset{ 1, 0, 10 },
        SearchOffset{ -1, 0, 10 },
        SearchOffset{ 0, 1, 10 },
        SearchOffset{ 0, -1, 10 },
        SearchOffset{ 1, 1, 14 },
        SearchOffset{ 1, -1, 14 },
        SearchOffset{ -1, 1, 14 },
        SearchOffset{ -1, -1, 14 }
    };

    while (!frontier.empty())
    {
        QueueItem current = frontier.top();
        frontier.pop();

        int cost = current.first;
        int currentIndex = current.second;

        if (cost > distance[currentIndex])
            continue;

        if (currentIndex == targetIndex)
            break;

        int currentCol = currentIndex % _navCols;
        int currentRow = currentIndex / _navCols;

        for (const SearchOffset& offset : offsets)
        {
            int nextCol = currentCol + offset.col;
            int nextRow = currentRow + offset.row;

            if (IsNavigationCellBlocked(nextCol, nextRow))
                continue;

            if (offset.col != 0 && offset.row != 0)
            {
                if (IsNavigationCellBlocked(currentCol + offset.col, currentRow)
                    || IsNavigationCellBlocked(currentCol, currentRow + offset.row))
                {
                    continue;
                }
            }

            int nextIndex = GetNavigationIndex(nextCol, nextRow);
            int nextCost = cost + offset.cost;

            if (nextCost >= distance[nextIndex])
                continue;

            distance[nextIndex] = nextCost;
            previous[nextIndex] = currentIndex;
            frontier.push({ nextCost, nextIndex });
        }
    }

    if (previous[targetIndex] == -1)
        return targetWorldPos;

    int nextIndex = targetIndex;

    while (previous[nextIndex] != startIndex && previous[nextIndex] != -1)
        nextIndex = previous[nextIndex];

    if (previous[nextIndex] == -1)
        return targetWorldPos;

    int nextCol = nextIndex % _navCols;
    int nextRow = nextIndex / _navCols;

    return Vector2{
        nextCol * _navCellSize + _navCellSize * 0.5f,
        nextRow * _navCellSize + _navCellSize * 0.5f
    };
}

void Engine::SpawnEnemyDrop(Vector2 worldPos)
{
    // *** Testing: 100% drop rate. Change to 20 for final drop rate. ***
    const int dropChancePercent = 100;

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

    _pickups.push_back(std::move(pickup));
}

void Engine::SpawnTimedPickup()
{
    float mapW = _map.width  * _mapScale;
    float mapH = _map.height * _mapScale;

    Vector2 pos{
        (float)GetRandomValue(200, (int)mapW - 200),
        (float)GetRandomValue(200, (int)mapH - 200)
    };

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

void Engine::DrawHowToPlay(bool resumeMode)
{
    const int sw = GetScreenWidth();
    const int sh = GetScreenHeight();

    // ── Background ──────────────────────────────────────────────────────────
    DrawRectangle(0, 0, sw, sh, DARKBROWN);

    // ── Title ───────────────────────────────────────────────────────────────
    const char* title = "How To Play";
    int titleSize = 60;
    int titleW = MeasureText(title, titleSize);
    DrawRectangle(0, 20, sw, titleSize + 24, Fade(BLACK, 0.6f));
    DrawText(title, sw / 2 - titleW / 2, 32, titleSize, YELLOW);

    // ── Panel setup ─────────────────────────────────────────────────────────
    const float colL   = 60.f;          // left column X
    const float colR   = sw / 2.f + 30.f; // right column X
    float rowY         = 120.f;
    const float rowGap = 80.f;
    const int   labelSz  = 22;
    const int   descSz   = 18;
    const float iconX    = 36.f;        // icon centre offset from col start

    // Divider
    DrawLineEx({ sw / 2.f, 110.f }, { sw / 2.f, (float)sh - 80.f }, 2.f, Fade(WHITE, 0.25f));

    // ── Left column header: Controls ─────────────────────────────────────
    DrawText("CONTROLS", (int)colL, (int)rowY, 26, ORANGE);
    rowY += 36.f;

    struct CtrlEntry { const char* key; const char* desc; };
    CtrlEntry controls[] = {
        { "W A S D",    "Move"                        },
        { "SPACE",      "Dash  (brief invincibility)" },
        { "Left Click", "Melee attack"                },
        { "Right Click","Fireball burst (needs ammo)" },
        { "F",          "Sword Beam    (needs ammo)"  },
        { "ESC",        "Pause / unpause"             },
    };

    for (auto& c : controls)
    {
        // key badge
        int kw = MeasureText(c.key, labelSz);
        DrawRectangleRounded({ colL, rowY - 4.f, (float)kw + 16.f, (float)labelSz + 10.f },
            0.3f, 4, Fade(BLACK, 0.7f));
        DrawRectangleRoundedLines({ colL, rowY - 4.f, (float)kw + 16.f, (float)labelSz + 10.f },
            0.3f, 4, Fade(WHITE, 0.5f));
        DrawText(c.key, (int)colL + 8, (int)rowY, labelSz, WHITE);

        // description
        DrawText(c.desc, (int)(colL + kw + 28.f), (int)rowY, descSz, LIGHTGRAY);

        rowY += rowGap * 0.85f;
    }

    // ── Right column header: Pickups & Enemies ───────────────────────────
    float rowR = 120.f + 36.f;
    DrawText("PICKUPS & ENEMIES", (int)colR, 120, 26, ORANGE);

    struct PickupEntry {
        const char* name;
        const char* desc;
        Color       iconColor;
        int         shape; // 0=fireball circles, 1=swordbeam, 2=freeze, 3=heal, 4=enemy
    };

    PickupEntry entries[] = {
        { "Fireball Ammo",  "Right-click to fire 8 fireballs.\nBurns enemies over time.",
          ORANGE, 0 },
        { "Sword Beam Ammo","Press F to fire a piercing beam\nthat hits multiple enemies.",
          BLUE,   1 },
        { "Freeze  [Q]",    "Freezes ALL enemies for 3-5 sec.\nThey can still take damage.",
          SKYBLUE,2 },
        { "Heal",           "Restores 1 HP (rare drop).\nWon't exceed your max HP.",
          RED,    3 },
        { "Enemy",          "Chases and attacks you.\nDrops a pickup on death.",
          RED,    4 },
    };

    const float iconCX = colR - 30.f; // icon centre X

    for (auto& e : entries)
    {
        float cy = rowR + 20.f;

        // ── Draw icon ──────────────────────────────────────────────────
        if (e.shape == 0) // fireball
        {
            DrawCircleV({ iconCX, cy }, 18.f, Fade(ORANGE, 0.5f));
            DrawCircleV({ iconCX, cy }, 12.f, Fade(RED,    0.8f));
            DrawCircleV({ iconCX, cy },  5.f, Fade(YELLOW, 0.9f));
        }
        else if (e.shape == 1) // sword beam
        {
            DrawCircleV({ iconCX + 6.f,  cy      }, 17.f, Fade(BLUE,    0.8f));
            DrawCircleV({ iconCX + 14.f, cy - 2.f}, 16.f, Fade(BLACK,   0.95f));
            DrawLineEx({ iconCX - 14.f, cy - 6.f }, { iconCX - 32.f, cy - 10.f }, 4.f, Fade(SKYBLUE, 0.7f));
            DrawLineEx({ iconCX - 12.f, cy + 7.f }, { iconCX - 28.f, cy + 13.f }, 3.f, Fade(SKYBLUE, 0.55f));
        }
        else if (e.shape == 2) // freeze
        {
            DrawCircleV({ iconCX, cy }, 18.f, Fade(BLUE,    0.35f));
            DrawCircleV({ iconCX, cy }, 13.f, Fade(SKYBLUE, 0.85f));
            DrawCircleV({ iconCX, cy },  6.f, Fade(WHITE,   0.90f));
            DrawLineEx({ iconCX - 11.f, cy }, { iconCX + 11.f, cy }, 2.f, WHITE);
            DrawLineEx({ iconCX, cy - 11.f }, { iconCX, cy + 11.f }, 2.f, WHITE);
        }
        else if (e.shape == 3) // heal
        {
            DrawCircleV({ iconCX, cy }, 18.f, Fade(RED,   0.35f));
            DrawCircleV({ iconCX, cy }, 13.f, Fade(RED,   0.85f));
            DrawCircleV({ iconCX, cy },  6.f, Fade(PINK,  0.90f));
            DrawLineEx({ iconCX - 7.f, cy }, { iconCX + 7.f, cy }, 3.f, WHITE);
            DrawLineEx({ iconCX, cy - 7.f }, { iconCX, cy + 7.f }, 3.f, WHITE);
        }
        else // enemy — red silhouette circle
        {
            DrawCircleV({ iconCX, cy }, 18.f, Fade(MAROON, 0.7f));
            DrawCircleV({ iconCX, cy - 6.f }, 9.f, Fade(RED, 0.85f));   // "head"
            DrawCircleV({ iconCX, cy + 8.f }, 12.f, Fade(MAROON, 0.9f)); // "body"
        }

        // ── Name + description ─────────────────────────────────────────
        float textX = colR + 4.f;
        DrawText(e.name, (int)textX, (int)(rowR), labelSz, WHITE);

        // Split desc on \n
        std::string full(e.desc);
        size_t nl = full.find('\n');
        if (nl != std::string::npos)
        {
            DrawText(full.substr(0, nl).c_str(), (int)textX, (int)(rowR + labelSz + 4), descSz, LIGHTGRAY);
            DrawText(full.substr(nl + 1).c_str(), (int)textX, (int)(rowR + labelSz + 4 + descSz + 2), descSz, LIGHTGRAY);
        }
        else
        {
            DrawText(full.c_str(), (int)textX, (int)(rowR + labelSz + 4), descSz, LIGHTGRAY);
        }

        rowR += rowGap * 1.15f;
    }

    // ── EXP / Level note ────────────────────────────────────────────────────
    DrawText("EXP & Levels", (int)colL, (int)rowY + 4.f, 26, ORANGE);
    rowY += 36.f;
    DrawText("Every enemy kill grants 1 EXP.", (int)colL, (int)rowY, descSz, LIGHTGRAY);
    rowY += descSz + 6;
    DrawText("Level up: +1 ATK, +1 Max HP, +1 HP restored.", (int)colL, (int)rowY, descSz, LIGHTGRAY);
    rowY += descSz + 6;
    DrawText("Thresholds double each level (10, 20, 40...)", (int)colL, (int)rowY, descSz, LIGHTGRAY);
    rowY += descSz + 6;
    DrawText("Max level: 10.", (int)colL, (int)rowY, descSz, LIGHTGRAY);

    // ── Back button ──────────────────────────────────────────────────────────
    const float btnW = 220.f;
    const float btnH = 55.f;
    const float btnX = sw / 2.f - btnW / 2.f;
    const float btnY = sh - btnH - 16.f;

    Rectangle backBtn{ btnX, btnY, btnW, btnH };
    bool hovered = CheckCollisionPointRec(GetMousePosition(), backBtn);

    DrawRectangleRounded(backBtn, 0.3f, 6, hovered ? Fade(GRAY, 0.9f) : Fade(DARKGRAY, 0.85f));
    DrawRectangleRoundedLines(backBtn, 0.3f, 6, Fade(WHITE, 0.5f));

    const char* backLabel = resumeMode ? "Resume" : "< Back";
    int backW = MeasureText(backLabel, 30);
    DrawText(backLabel, (int)(btnX + btnW / 2.f - backW / 2.f), (int)(btnY + btnH / 2.f - 15.f), 30, WHITE);

    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        if (resumeMode)
            _gameState = GameState::Play;
        else
        {
            _menu.Init();
            _gameState = GameState::Menu;
        }
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
    float maxX = mapW * 0.72f;
    float minY = mapH * 0.05f;
    float maxY = mapH * 0.76f;

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

void Engine::RespawnOutOfBoundsEnemies()
{
    float mapW = _map.width  * _mapScale;
    float mapH = _map.height * _mapScale;
    const float hardMargin = 60.f;   // enemy is truly outside if beyond this

    for (auto& enemy : _enemies)
    {
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
            newPos.x = (float)GetRandomValue(200, (int)(mapW - 200));
            newPos.y = (float)GetRandomValue(200, (int)(mapH - 200));

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
        if (Vector2Distance(pos, enemy->GetWorldPos()) < safeDistance)
            return false;
    }

    return true;
}
