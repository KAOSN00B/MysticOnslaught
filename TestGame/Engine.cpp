#include "Engine.h"
#include "raymath.h"

Engine::Engine()
    : _goblin{ Vector2{1000.0f,500.0f} }
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
    _hero.Init();      // IMPORTANT
    _goblin.Init();    // IMPORTANT
    _map = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\TileSet\\Map.png");
    _pillarTex = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\TileSet\\Pillar.png");

    

    _props[0] = Prop{ Vector2{500.f, 500.f}, _pillarTex };
    _props[1] = Prop{ Vector2{1300.f, 700.f}, _pillarTex };

    _goblin.SetTarget(&_hero);
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
    _goblin.Update(dt, _hero.GetWorldPos());

    _hero.DealDamage(_goblin);

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
    _goblin.DrawEnemy(_hero.GetWorldPos());

    // 4. Draw player LAST
    _hero.DrawPlayer();
}

void Engine::HandleCollisions()
{
    if (_hero.GetWorldPos().x < 0.0f || _hero.GetWorldPos().y < 0.0f
        || _hero.GetWorldPos().x + _windowWidth > _map.width * _mapScale
        || _hero.GetWorldPos().y + _windowHeight > _map.height * _mapScale)
    {
        _hero.UndoMovement();
        _goblin.UndoMovement();
    }

    for (auto& prop : _props)
    {
        if (CheckCollisionRecs(prop.GetCollisionRec(), _hero.GetCollisionRec()))
            _hero.UndoMovement();

        if (CheckCollisionRecs(prop.GetCollisionRec(), _goblin.GetCollisionRec()))
            _goblin.UndoMovement();
    }
}