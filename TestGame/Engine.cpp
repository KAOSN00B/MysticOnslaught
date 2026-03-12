#include "Engine.h"
#include "raymath.h"

Engine::Engine()
{
    Init();
}

Engine::~Engine()
{
    UnloadTexture(_map);
    UnloadTexture(_pillarTex);
    CloseWindow();
}

void Engine::Init()
{
    InitWindow(_windowWidth, _windowHeight, "Top Down Game");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    InitAudioDevice();

    _menu.Init();
    _player.Init();

    _map = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\TileSet\\Map.png");
    _pillarTex = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\TileSet\\Pillar.png");

    _props[0] = Prop{ Vector2{500.f, 500.f}, _pillarTex };
    _props[1] = Prop{ Vector2{1300.f, 700.f}, _pillarTex };
}

void Engine::SpawnWave()
{
    int enemyCount = 4 + (_wave * 2);

    if (_enemies.empty())
    {
        for (int i = 0; i < enemyCount; i++)
        {
            float x = GetRandomValue(200, 1800);
            float y = GetRandomValue(200, 1800);

            auto enemy = std::make_unique<Enemy>(Vector2{ x, y });
            enemy->Init();
            enemy->SetTarget(&_player);

            _enemies.push_back(std::move(enemy));
        }

        _wave++;
    }
}

void Engine::Run()
{
    while (!WindowShouldClose() && !_shouldExit)
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
            _wave = 0;
            _enemies.clear();
            _player.Init();
            SpawnWave();

            _gameState = GameState::Play;
        }

        if (_menu.QuitPressed())
            _shouldExit = true;

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

    if (_player.GetHealth() <= 0)
    {
        _gameState = GameState::GameOver;
        return;
    }

    _player.Update(dt);

    if (_enemies.empty())
        SpawnWave();

    UpdateEnemyCount(dt);

    for (auto& enemy : _enemies)
    {
        enemy->Update(dt, _player.GetWorldPos());

        int prevHealth = enemy->GetHealth();

        _player.DealDamage(*enemy);

        if (enemy->GetHealth() < prevHealth)
            TriggerScreenShake(6.f, 0.07f);
    }

    HandleCollisions();

    _mapPos = Vector2Scale(_player.GetWorldPos(), -1.f);

    if (_shakeTimer > 0.f)
    {
        _shakeTimer -= dt;

        float x = GetRandomValue(-100, 100) / 100.f * _shakeStrength;
        float y = GetRandomValue(-100, 100) / 100.f * _shakeStrength;

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
        Vector2 shakenMapPos = Vector2Add(_mapPos, _shakeOffset);

        DrawTextureEx(_map, shakenMapPos, 0.0f, _mapScale, WHITE);

        for (auto& prop : _props)
            prop.Render(Vector2Subtract(_player.GetWorldPos(), _shakeOffset));

        for (auto& enemy : _enemies)
            enemy->DrawEnemy(Vector2Subtract(_player.GetWorldPos(), _shakeOffset));

        _player.DrawPlayer();

        int fontSize = 30;
        int textWidth = MeasureText(_message.c_str(), fontSize);

        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight() / 8, Fade(BLACK, 0.6f));

        DrawText(_message.c_str(), GetScreenWidth() / 2 - textWidth / 2, 10, fontSize, YELLOW);

        DrawText(TextFormat("Time: %.1f", GetTime()),
            85 + GetScreenWidth() / 2 - textWidth / 2, 60, fontSize, RAYWHITE);

        DrawText(("Wave: " + std::to_string(_wave)).c_str(), 20, 10, 30, RAYWHITE);

        DrawText(("Enemies Left: " + std::to_string(_enemies.size())).c_str(), 20, 60, 30, RAYWHITE);

        DrawText(("Health: " + std::to_string(_player.GetHealth())).c_str(),
            GetScreenWidth() - 200, 20, 30, RAYWHITE);

        break;
    }

    case GameState::Pause:
    {
        DrawText("PAUSED", GetScreenWidth() / 2 - 120, GetScreenHeight() / 2, 60, YELLOW);
        DrawText("Press ESC to resume", GetScreenWidth() / 2 - 160, GetScreenHeight() / 2 + 70, 30, WHITE);
        break;
    }

    case GameState::GameOver:
    {
        DrawText("GAME OVER", GetScreenWidth() / 2 - 150, GetScreenHeight() / 2, 60, RED);
        DrawText("Press ENTER to return to menu",
            GetScreenWidth() / 2 - 210, GetScreenHeight() / 2 + 80, 30, WHITE);
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

        for (auto& enemy : _enemies)
            enemy->UndoMovement();
    }

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
}

void Engine::UpdateEnemyCount(float dt)
{
    for (int i = _enemies.size() - 1; i >= 0; i--)
    {
        if (_enemies[i]->UpdateDeath(dt))
            _enemies.erase(_enemies.begin() + i);
    }
}

void Engine::TriggerScreenShake(float strength, float duration)
{
    _shakeStrength = strength;
    _shakeTimer = duration;
}