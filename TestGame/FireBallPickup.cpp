#include "FireBallPickup.h"
#include "raymath.h"

FireBallPickup::FireBallPickup()
{
    _damage = 2.f;
    _isActive = true;
}

FireBallPickup::~FireBallPickup()
{
    UnloadTexture(_texture);
}

void FireBallPickup::Init(Vector2 spawnPos)
{
    _worldPos = spawnPos;

    _texture = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\PowerUps\\FireBallPickup.png");
}

void FireBallPickup::Draw(Vector2 worldOffset)
{
    if (!_isActive) return;

    Vector2 screenPos = Vector2Add(_worldPos, worldOffset); // world → screen

    //DrawTexture(_texture, (int)screenPos.x, (int)screenPos.y, WHITE);
    DrawCircleV(Vector2Add(_worldPos, worldOffset), 20, ORANGE);
}

Vector2 FireBallPickup::GetPosition() const
{
    return _worldPos;
}

bool FireBallPickup::IsActive() const
{
    return _isActive;
}

void FireBallPickup::Destroy()
{
    _isActive = false;
 
}

Rectangle FireBallPickup::GetCollisionRec()
{
    float size = 40.f;

    return Rectangle{ _worldPos.x - size / 2,_worldPos.y - size / 2, size, size };
}