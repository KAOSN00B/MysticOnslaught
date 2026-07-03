#include "EnemyProjectile.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "AnimationUtils.h"
#include "raymath.h"

Texture2D EnemyProjectile::_sharedArrowTex{};
Texture2D EnemyProjectile::_sharedFireBoltTex{};
bool      EnemyProjectile::_sharedTexturesLoaded = false;

void EnemyProjectile::Init(Vector2 spawnPos, Vector2 direction, EnemyProjectileKind kind, int damage)
{
    EnsureTexturesLoaded();

    _worldPos  = spawnPos;
    _kind      = kind;
    _damage    = damage;
    _speed     = (kind == EnemyProjectileKind::Arrow) ? _arrowSpeed : _fireBoltSpeed;
    _direction = (Vector2LengthSqr(direction) > 0.0001f)
        ? Vector2Normalize(direction)
        : Vector2{ 1.f, 0.f };
    _lifeTimer   = 0.f;
    _runningTime = 0.f;
    _frame       = 0;
    _isActive    = true;
}

void EnemyProjectile::Update(float dt)
{
    if (!_isActive)
        return;

    _lifeTimer += dt;
    if (_lifeTimer >= _maxLife)
    {
        _isActive = false;
        return;
    }

    if (_kind == EnemyProjectileKind::FireBolt)
    {
        _runningTime += dt;
        if (_runningTime >= _fireBoltFrameTime)
        {
            _runningTime = 0.f;
            _frame = (_frame + 1) % _fireBoltFrames;
        }
    }

    _worldPos = Vector2Add(_worldPos, Vector2Scale(_direction, _speed * dt));
}

void EnemyProjectile::Draw(Vector2 worldOffset) const
{
    if (!_isActive)
        return;

    Vector2 screenPos = Vector2Add(_worldPos, worldOffset);
    screenPos.x += kVirtualWidth  / 2.f;
    screenPos.y += kVirtualHeight / 2.f;

    float rotation = atan2f(_direction.y, _direction.x) * RAD2DEG;

    if (_kind == EnemyProjectileKind::Arrow)
    {
        // The arrow art points up-right (north-east), i.e. -45 degrees from
        // "pointing right", so add 45 to align the sprite with its flight path.
        Rectangle source{ 0.f, 0.f, (float)_sharedArrowTex.width, (float)_sharedArrowTex.height };
        Rectangle dest{
            screenPos.x,
            screenPos.y,
            _sharedArrowTex.width  * _arrowScale,
            _sharedArrowTex.height * _arrowScale
        };
        DrawTexturePro(_sharedArrowTex, source, dest,
            Vector2{ dest.width * 0.5f, dest.height * 0.5f }, rotation + 45.f, WHITE);
    }
    else
    {
        Rectangle source = GetAnimationFrameRect(_sharedFireBoltTex, _fireBoltFrameW, _fireBoltFrameH, _frame);
        Rectangle dest{
            screenPos.x,
            screenPos.y,
            _fireBoltFrameW * _fireBoltScale,
            _fireBoltFrameH * _fireBoltScale
        };
        DrawTexturePro(_sharedFireBoltTex, source, dest,
            Vector2{ dest.width * 0.5f, dest.height * 0.5f }, rotation, WHITE);
    }
}

Rectangle EnemyProjectile::GetCollisionRec() const
{
    // Small centred hit box regardless of visual size so dodging feels fair.
    const float halfSize = (_kind == EnemyProjectileKind::Arrow) ? 16.f : 26.f;
    return Rectangle{
        _worldPos.x - halfSize,
        _worldPos.y - halfSize,
        halfSize * 2.f,
        halfSize * 2.f
    };
}

void EnemyProjectile::UnloadSharedResources()
{
    if (_sharedTexturesLoaded)
    {
        UnloadTexture(_sharedArrowTex);
        UnloadTexture(_sharedFireBoltTex);
        _sharedArrowTex      = Texture2D{};
        _sharedFireBoltTex   = Texture2D{};
        _sharedTexturesLoaded = false;
    }
}

void EnemyProjectile::EnsureTexturesLoaded()
{
    if (_sharedTexturesLoaded)
        return;

    _sharedArrowTex    = LoadTexture(AssetPath("Enemy/Arrow.png").c_str());
    _sharedFireBoltTex = LoadTexture(AssetPath("PowerUps/Fireball.png").c_str());
    _sharedTexturesLoaded = true;
}
