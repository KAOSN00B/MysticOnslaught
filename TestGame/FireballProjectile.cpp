#include "FireballProjectile.h"

#include "AnimationUtils.h"
#include "raymath.h"

Texture2D FireballProjectile::_sharedTexture{};
bool FireballProjectile::_textureLoaded = false;

void FireballProjectile::Init(Vector2 spawnPos, Vector2 direction)
{
    EnsureTextureLoaded();
    _worldPos = spawnPos;
    _direction = Vector2Normalize(direction);
    _lifeTimer = 5.f;
    _runningTime = 0.f;
    _frame = 0;
    _isActive = true;
}

void FireballProjectile::Update(float dt)
{
    if (!_isActive)
        return;

    _lifeTimer -= dt;
    if (_lifeTimer <= 0.f)
    {
        Destroy();
        return;
    }

    _runningTime += dt;
    if (_runningTime >= _updateTime)
    {
        _runningTime = 0.f;
        _frame = (_frame + 1) % _frameCount;
    }

    _worldPos = Vector2Add(_worldPos, Vector2Scale(_direction, _speed * dt));
}

void FireballProjectile::Draw(Vector2 worldOffset) const
{
    if (!_isActive)
        return;

    Vector2 screenPos = Vector2Add(_worldPos, worldOffset);
    screenPos.x += GetScreenWidth() / 2.f;
    screenPos.y += GetScreenHeight() / 2.f;

    float rotation = atan2f(_direction.y, _direction.x) * RAD2DEG;
    Rectangle source = GetAnimationFrameRect(_sharedTexture, _frameWidth, _frameHeight, _frame);
    float scale = 7.2f;
    Rectangle dest{
        screenPos.x,
        screenPos.y,
        _frameWidth * scale,
        _frameHeight * scale
    };

    DrawTexturePro(_sharedTexture, source, dest, Vector2{ dest.width * 0.5f, dest.height * 0.5f }, rotation, WHITE);
}

void FireballProjectile::Destroy()
{
    _isActive = false;
}

void FireballProjectile::UnloadSharedResources()
{
    if (!_textureLoaded)
        return;

    UnloadTexture(_sharedTexture);
    _sharedTexture = Texture2D{};
    _textureLoaded = false;
}

bool FireballProjectile::IsActive() const
{
    return _isActive;
}

Rectangle FireballProjectile::GetCollisionRec() const
{
    return Rectangle{
        _worldPos.x - _radius,
        _worldPos.y - _radius,
        _radius * 2.f,
        _radius * 2.f
    };
}

Vector2 FireballProjectile::GetWorldPos() const
{
    return _worldPos;
}

Vector2 FireballProjectile::GetDirection() const
{
    return _direction;
}

void FireballProjectile::EnsureTextureLoaded()
{
    if (_textureLoaded)
        return;

    _sharedTexture = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\PowerUps\\Fireball.png");
    _textureLoaded = true;
}
