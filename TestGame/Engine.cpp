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

	_hero.Init();      // IMPORTANT

	_map = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\TileSet\\Map.png");
	_pillarTex = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\TileSet\\Pillar.png");

	_props[0] = Prop{ Vector2{500.f, 500.f}, _pillarTex };
	_props[1] = Prop{ Vector2{1300.f, 700.f}, _pillarTex };

	SpawnWave();

}

void Engine::SpawnWave()
{
	int enemyCount = 4 * _enemyCountMultiplyer;

	if (_enemies.empty())
	{
		for (int i = 0; i < enemyCount; i++)
		{
			float x = GetRandomValue(200, 1800);
			float y = GetRandomValue(200, 1800);

			auto enemy = std::make_unique<Enemy>(Vector2{ x, y });
			enemy->Init();
			enemy->SetTarget(&_hero);

			_enemies.push_back(std::move(enemy));
		}

		_enemyCountMultiplyer *= 2;
		_wave++;
	}
}

void Engine::Run()
{
	while (!WindowShouldClose())
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
	_hero.Update(dt);

	if (_enemies.empty())
		SpawnWave();

	UpdateEnemyCount(dt);

	for (auto& enemy : _enemies)
	{
		enemy->Update(dt, _hero.GetWorldPos());
		_hero.DealDamage(*enemy);
	}

	HandleCollisions();

	_mapPos = Vector2Scale(_hero.GetWorldPos(), -1.f);
}

void Engine::Draw()
{
	// 1. Draw map
	DrawTextureEx(_map, _mapPos, 0.0f, _mapScale, WHITE);

	// 2. Draw props
	for (auto& prop : _props)
		prop.Render(_hero.GetWorldPos());

	// 3. Draw enemy
	for (auto& enemy : _enemies)
	{
		enemy->DrawEnemy(_hero.GetWorldPos());
	}

	// 4. Draw player 
	_hero.DrawPlayer();

	// string message for start of wave

		int fontSize = 30;

		int textWidth = MeasureText(_message.c_str(), fontSize);

		DrawRectangle(0, 0, GetScreenWidth(), (GetScreenHeight() / 8), Fade(BLACK, 0.6f));

		DrawText(_message.c_str(),GetScreenWidth() / 2 - textWidth / 2, 10, fontSize, YELLOW);
		DrawText(TextFormat("Time: %.1f", GetTime()), 85 + GetScreenWidth() / 2 - textWidth / 2, 60, fontSize, RAYWHITE);

		DrawText(("Wave: " + std::to_string(_wave)).c_str(),
			20,10,30,RAYWHITE);

		DrawText(("Enemies Left: " + std::to_string(_enemies.size())).c_str(),
			20,60,30,RAYWHITE);

		DrawText(("Health: " + std::to_string(_hero.GetHealth())).c_str(), GetScreenWidth() - 200, 20, 30, RAYWHITE);
}

void Engine::HandleCollisions()
{
	if (_hero.GetWorldPos().x < 0.0f || _hero.GetWorldPos().y < 0.0f
		|| _hero.GetWorldPos().x + _windowWidth > _map.width * _mapScale
		|| _hero.GetWorldPos().y + _windowHeight > _map.height * _mapScale)
	{
		_hero.UndoMovement();
		for (auto& enemy : _enemies)
		{
			enemy->UndoMovement();
		}

	}

	for (auto& prop : _props)
	{
		if (CheckCollisionRecs(prop.GetCollisionRec(), _hero.GetCollisionRec()))
			_hero.UndoMovement();

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
		{
			_enemies.erase(_enemies.begin() + i);
		}
	}
}