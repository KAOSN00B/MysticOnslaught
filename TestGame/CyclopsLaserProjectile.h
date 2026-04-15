#pragma once
#include "raylib.h"

// A fast energy bolt fired by the Cyclops after its charge animation.
// Drawn procedurally as a glowing beam — no texture needed.
class CyclopsLaserProjectile
{
public:
    enum class Mode
    {
        Sweep,
        Scatter
    };

    void InitSweep(Vector2 spawnPos, Vector2 direction, int damage);
    void InitScatter(Vector2 spawnPos, Vector2 direction, int damage);
    void Update(float dt);
    void Draw(Vector2 worldOffset, const Vector2* clippedEnds, int clippedEndCount) const;
    void Destroy();

    bool      IsActive()       const { return _isActive; }
    Vector2   GetWorldPos()    const { return _worldPos; }
    Vector2   GetDirection()   const { return _direction; }
    int       GetDamage()      const { return _damage; }
    Mode      GetMode()        const { return _mode; }
    int       GetBeamCount()   const { return _beamCount; }
    Vector2   GetBeamDirection(int index) const;
    float     GetBeamLength()  const { return _beamLength; }
    float     GetCurrentBeamLength() const { return _beamLength * _drawLengthScale; }
    float     GetBeamWidth()   const { return _beamWidth; }
    bool      CanHitPlayer()   const { return _playerHitCooldown <= 0.f; }
    void      OnHitPlayer()          { _playerHitCooldown = 0.45f; }

private:
    void ConfigureScatterDirections(Vector2 centerDirection);

    Vector2 _worldPos{};
    Vector2 _direction{};
    Vector2 _beamDirections[5]{};
    int     _damage    = 2;
    float   _lifeTimer = 0.f;
    float   _playerHitCooldown = 0.f;
    float   _beamLength = 1800.f;
    float   _beamWidth  = 22.f;
    float   _drawLengthScale = 1.f;
    int     _beamCount  = 1;
    Mode    _mode       = Mode::Sweep;
    bool    _isActive  = false;
};
