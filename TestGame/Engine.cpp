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
	SetTargetFPS(60);
	InitAudioDevice();

	SetExitKey(KEY_NULL);

	_menu.Init();
	_player.Init();

	_map = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\TileSet\\Map.png");
	_pillarTex = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\TileSet\\Pillar.png");

	_props[0] = Prop{ Vector2{500.f, 500.f}, _pillarTex };
	_props[1] = Prop{ Vector2{1300.f, 700.f}, _pillarTex };

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
	int enemyCount = 0 + (_wave * 2);

	for (int i = 0; i < enemyCount; i++)
	{
		float x = GetRandomValue(200, 1800);
		float y = GetRandomValue(200, 1800);

		auto enemy = std::make_unique<Enemy>(Vector2{ x, y });
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

			_enemies.clear();
			_player.Init();

			SpawnWave();

			_gameState = GameState::Menu;
		}
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

	// Update player FIRST so animation runs
	_player.Update(dt);

	// If player died start the dying phase
	if (_player.GetHealth() <= 0 && !_playerDying)
	{
		_playerDying = true;
		_gameOverTimer = _gameOverDelay;
	}

	// Handle death delay
	if (_playerDying)
	{
		_gameOverTimer -= dt;

		if (_gameOverTimer <= 0.f)
		{
			_gameState = GameState::GameOver;
		}

		return; // stop gameplay while dying
	}

	// NORMAL GAMEPLAY BELOW HERE
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

		if (_enemies.empty())
			SpawnWave();

		UpdateEnemyCount(dt);

		for (auto& enemy : _enemies)
		{
			enemy->Update(dt, _player.GetWorldPos(), _enemies);

			int prevHealth = enemy->GetHealth();

			_player.DealDamage(*enemy);

			if (enemy->GetHealth() < prevHealth)
				TriggerScreenShake(6.f, 0.07f);
		}
	}

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

		for (auto& enemy : _enemies)
			enemy->DrawEnemy(Vector2Subtract(_player.GetWorldPos(), _shakeOffset));

		_player.DrawPlayer();

		if (_pauseUI.DrawPause())
			_shouldClose = true;

		break;
	}

	case GameState::GameOver:
	{
		if (_pauseUI.DrawGameOver(_wave, _gameTimer))
			_shouldClose = true;

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

void Engine::DrawWorld()
{
	Vector2 shakenMapPos = Vector2Add(_mapPos, _shakeOffset);

	DrawTextureEx(_map, shakenMapPos, 0.0f, _mapScale, WHITE);

	for (auto& prop : _props)
		prop.Render(Vector2Subtract(_player.GetWorldPos(), _shakeOffset));

	for (auto& enemy : _enemies)
		enemy->DrawEnemy(Vector2Subtract(_player.GetWorldPos(), _shakeOffset));

	_player.DrawPlayer();
}

void Engine::DrawHUD()
{
	int fontSize = 30;

	DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight() / 8, Fade(BLACK, 0.6f));

	DrawText(TextFormat("Time: %.1f", _gameTimer), 85 + 
		GetScreenWidth() / 2 - 150,	60, fontSize, RAYWHITE);

	DrawText(("Wave: " + std::to_string(_wave)).c_str(),
		20, 10, 30, RAYWHITE);

	DrawText(("Enemies Left: " + std::to_string(_enemies.size())).c_str(),
		20, 60, 30, RAYWHITE);

	DrawText(("Health: " + std::to_string(_player.GetHealth())).c_str(),
		GetScreenWidth() - 200, 20, 30, RAYWHITE);
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

		DrawText(line1, GetScreenWidth() / 2 - w1 / 2,
			GetScreenHeight() / 2 - 60, fontSize, YELLOW);

		DrawText(line2, GetScreenWidth() / 2 - w2 / 2,
			GetScreenHeight() / 2 + 10, fontSize, YELLOW);
	}
	else
	{
		std::string waveText = "Wave " + std::to_string(_wave);

		int textWidth = MeasureText(waveText.c_str(), fontSize);

		DrawText(waveText.c_str(), GetScreenWidth() / 2 - textWidth / 2,
			GetScreenHeight() / 2 - 30, fontSize, YELLOW);
	}
}