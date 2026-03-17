#include "CyclopsLaserProjectile.h"
#include "raymath.h"
#include <cmath>

void CyclopsLaserProjectile::Init(Vector2 spawnPos, Vector2 direction, int damage)
{
    _worldPos  = spawnPos;
    _direction = Vector2Normalize(direction);
    _damage    = damage;
    _lifeTimer = 2.f;
    _isActive  = true;
}

void CyclopsLaserProjectile::Update(float dt)
{
    if (!_isActive) return;

    _lifeTimer -= dt;
    if (_lifeTimer <= 0.f)
    {
        Destroy();
        return;
    }

    _worldPos = Vector2Add(_worldPos, Vector2Scale(_direction, _speed * dt));
}

void CyclopsLaserProjectile::Draw(Vector2 worldOffset) const
{
    if (!_isActive) return;

    Vector2 screenPos;
    screenPos.x = _worldPos.x + worldOffset.x + GetScreenWidth()  * 0.5f;
    screenPos.y = _worldPos.y + worldOffset.y + GetScreenHeight() * 0.5f;

    float angle = atan2f(_direction.y, _direction.x) * RAD2DEG;

    // Outer glow
    Rectangle outer{ screenPos.x, screenPos.y, _beamLength * 1.3f, _beamWidth * 2.2f };
    DrawRectanglePro(outer,
        Vector2{ outer.width * 0.5f, outer.height * 0.5f },
        angle,
        Color{ 255, 200, 50, 80 });

    // Core beam
    Rectangle beam{ screenPos.x, screenPos.y, _beamLength, _beamWidth };
    DrawRectanglePro(beam,
        Vector2{ beam.width * 0.5f, beam.height * 0.5f },
        angle,
        Color{ 255, 240, 100, 230 });

    // White-hot centre line
    Rectangle core{ screenPos.x, screenPos.y, _beamLength, _beamWidth * 0.35f };
    DrawRectanglePro(core,
        Vector2{ core.width * 0.5f, core.height * 0.5f },
        angle,
        Color{ 255, 255, 255, 200 });

    // Bright head circle
    Vector2 head{
        screenPos.x + cosf(angle * DEG2RAD) * _beamLength * 0.5f,
        screenPos.y + sinf(angle * DEG2RAD) * _beamLength * 0.5f
    };
    DrawCircleV(head, _beamWidth * 1.1f, Color{ 255, 255, 180, 220 });
}

void CyclopsLaserProjectile::Destroy()
{
    _isActive = false;
}

Rectangle CyclopsLaserProjectile::GetCollisionRec() const
{
    return Rectangle{
        _worldPos.x - _hitRadius,
        _worldPos.y - _hitRadius,
        _hitRadius * 2.f,
        _hitRadius * 2.f
    };
}
