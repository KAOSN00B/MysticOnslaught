#include "SwordBeamProjectile.h"
#include "AssetPaths.h"

#include "AnimationUtils.h"
#include "raymath.h"

#include <algorithm>

Texture2D SwordBeamProjectile::_sharedTexture{};
bool SwordBeamProjectile::_textureLoaded = false;

void SwordBeamProjectile::Init(Vector2 spawnPos, Vector2 direction)
{
    EnsureTextureLoaded();
    _worldPos = spawnPos;
    _direction = Vector2Normalize(direction);
    _lifeTimer = 5.f;
    _runningTime = 0.f;
    _frame = 0;
    _isActive = true;
    _hitEnemies.clear();
}

void SwordBeamProjectile::Update(float dt)
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

void SwordBeamProjectile::Draw(Vector2 worldOffset) const
{
    if (!_isActive)
        return;

    Vector2 screenPos = Vector2Add(_worldPos, worldOffset);
    screenPos.x += GetScreenWidth() / 2.f;
    screenPos.y += GetScreenHeight() / 2.f;

    float rotation = atan2f(_direction.y, _direction.x) * RAD2DEG + 90.f;
    Rectangle source = GetAnimationFrameRect(_sharedTexture, _frameWidth, _frameHeight, _frame);
    float scale = 8.f;
    Rectangle dest{
        screenPos.x,
        screenPos.y,
        _frameWidth * scale,
        _frameHeight * scale
    };

    DrawTexturePro(_sharedTexture, source, dest, Vector2{ dest.width * 0.5f, dest.height * 0.5f }, rotation, WHITE);
}

void SwordBeamProjectile::Destroy()
{
    _isActive = false;
}

void SwordBeamProjectile::UnloadSharedResources()
{
    if (!_textureLoaded)
        return;

    UnloadTexture(_sharedTexture);
    _sharedTexture = Texture2D{};
    _textureLoaded = false;
}

bool SwordBeamProjectile::IsActive() const
{
    return _isActive;
}

Rectangle SwordBeamProjectile::GetCollisionRec() const
{
    return Rectangle{
        _worldPos.x - _width * 0.5f,
        _worldPos.y - _height * 0.5f,
        _width,
        _height
    };
}

Vector2 SwordBeamProjectile::GetWorldPos() const
{
    return _worldPos;
}

Vector2 SwordBeamProjectile::GetDirection() const
{
    return _direction;
}

bool SwordBeamProjectile::HasHitEnemy(const Enemy* enemy) const
{
    return std::find(_hitEnemies.begin(), _hitEnemies.end(), enemy) != _hitEnemies.end();
}

void SwordBeamProjectile::RegisterHitEnemy(const Enemy* enemy)
{
    _hitEnemies.push_back(enemy);
}

void SwordBeamProjectile::EnsureTextureLoaded()
{
    if (_textureLoaded)
        return;

    _sharedTexture = LoadTexture(AssetPath("PowerUps/BladeBeam_AProjectile.png").c_str());
    _textureLoaded = true;
}
