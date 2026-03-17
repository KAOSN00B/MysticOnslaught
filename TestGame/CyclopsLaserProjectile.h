#pragma once
#include "raylib.h"

// A fast energy bolt fired by the Cyclops after its charge animation.
// Drawn procedurally as a glowing beam — no texture needed.
class CyclopsLaserProjectile
{
public:
    void Init(Vector2 spawnPos, Vector2 direction, int damage);
    void Update(float dt);
    void Draw(Vector2 worldOffset) const;
    void Destroy();

    bool      IsActive()       const { return _isActive; }
    Vector2   GetWorldPos()    const { return _worldPos; }
    Vector2   GetDirection()   const { return _direction; }
    int       GetDamage()      const { return _damage; }
    Rectangle GetCollisionRec()const;

private:
    Vector2 _worldPos{};
    Vector2 _direction{};
    int     _damage    = 2;
    float   _speed     = 820.f;
    float   _lifeTimer = 2.f;
    bool    _isActive  = false;

    static constexpr float _hitRadius  = 20.f;
    static constexpr float _beamLength = 90.f;
    static constexpr float _beamWidth  = 12.f;
};
