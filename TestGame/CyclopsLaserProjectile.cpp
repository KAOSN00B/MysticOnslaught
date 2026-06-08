#include "CyclopsLaserProjectile.h"
#include "VirtualCanvas.h"
#include "raymath.h"
#include "VirtualCanvas.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr float kSweepLifetime = 0.75f;
    constexpr float kScatterLifetime = 0.28f;
    constexpr float kScatterArcDegrees = 70.f;
    constexpr int   kScatterBeams = 5;
    constexpr float kSweepGrowDuration = 1.44f;
}

void CyclopsLaserProjectile::InitSweep(Vector2 spawnPos, Vector2 direction, int damage)
{
    _worldPos  = spawnPos;
    _direction = (Vector2Length(direction) > 0.001f) ? Vector2Normalize(direction) : Vector2{ 1.f, 0.f };
    _beamDirections[0] = _direction;
    _damage    = damage;
    _lifeTimer = kSweepLifetime;
    _playerHitCooldown = 0.f;
    _beamLength = 1800.f;
    _beamWidth = 22.f;
    _drawLengthScale = 0.f;
    _beamCount = 1;
    _mode = Mode::Sweep;
    _isActive = true;
}

void CyclopsLaserProjectile::InitScatter(Vector2 spawnPos, Vector2 direction, int damage)
{
    _worldPos  = spawnPos;
    _direction = (Vector2Length(direction) > 0.001f) ? Vector2Normalize(direction) : Vector2{ 1.f, 0.f };
    _damage    = damage;
    _lifeTimer = kScatterLifetime;
    _playerHitCooldown = 0.f;
    _beamLength = 460.f;
    _beamWidth = 12.f;
    _drawLengthScale = 1.f;
    _beamCount = kScatterBeams;
    _mode = Mode::Scatter;
    _isActive = true;
    ConfigureScatterDirections(_direction);
}

void CyclopsLaserProjectile::Update(float dt)
{
    if (!_isActive)
        return;

    if (_mode == Mode::Sweep)
        _drawLengthScale = std::min(1.f, _drawLengthScale + dt / kSweepGrowDuration);

    _lifeTimer -= dt;
    _playerHitCooldown = std::max(0.f, _playerHitCooldown - dt);
    if (_lifeTimer <= 0.f)
    {
        Destroy();
        return;
    }
}

void CyclopsLaserProjectile::Draw(Vector2 worldOffset, const Vector2* clippedEnds, int clippedEndCount) const
{
    if (!_isActive || clippedEnds == nullptr || clippedEndCount <= 0)
        return;

    const Vector2 screenOrigin{
        _worldPos.x + worldOffset.x + kVirtualWidth * 0.5f,
        _worldPos.y + worldOffset.y + kVirtualHeight * 0.5f
    };

    for (int i = 0; i < clippedEndCount; ++i)
    {
        const Vector2 screenEnd{
            clippedEnds[i].x + worldOffset.x + kVirtualWidth * 0.5f,
            clippedEnds[i].y + worldOffset.y + kVirtualHeight * 0.5f
        };

        const float outerWidth = (_mode == Mode::Sweep) ? _beamWidth * 1.9f : _beamWidth * 2.1f;
        const float coreWidth  = (_mode == Mode::Sweep) ? _beamWidth : _beamWidth * 0.9f;
        const float innerWidth = (_mode == Mode::Sweep) ? _beamWidth * 0.34f : _beamWidth * 0.28f;

        const Color glowColor  = (_mode == Mode::Sweep)
            ? Color{ 255, 70, 70, 70 }
            : Color{ 255, 100, 80, 88 };
        const Color beamColor  = (_mode == Mode::Sweep)
            ? Color{ 255, 95, 95, 215 }
            : Color{ 255, 135, 95, 225 };

        DrawLineEx(screenOrigin, screenEnd, outerWidth, glowColor);
        DrawLineEx(screenOrigin, screenEnd, coreWidth, beamColor);
        DrawLineEx(screenOrigin, screenEnd, innerWidth, Color{ 255, 245, 235, 210 });
        DrawCircleV(screenOrigin, coreWidth * 0.44f, Fade(beamColor, 0.65f));
    }
}

void CyclopsLaserProjectile::Destroy()
{
    _isActive = false;
}

Vector2 CyclopsLaserProjectile::GetBeamDirection(int index) const
{
    if (index < 0 || index >= _beamCount)
        return Vector2Zero();
    return _beamDirections[index];
}

void CyclopsLaserProjectile::ConfigureScatterDirections(Vector2 centerDirection)
{
    const float baseAngle = atan2f(centerDirection.y, centerDirection.x);
    for (int i = 0; i < kScatterBeams; ++i)
    {
        float t = (kScatterBeams == 1) ? 0.5f : (float)i / (float)(kScatterBeams - 1);
        float offset = Lerp(-kScatterArcDegrees * 0.5f, kScatterArcDegrees * 0.5f, t) * DEG2RAD;
        float angle = baseAngle + offset;
        _beamDirections[i] = Vector2Normalize(Vector2{ cosf(angle), sinf(angle) });
    }
}
