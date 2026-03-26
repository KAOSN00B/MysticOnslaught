#pragma once
#include "raylib.h"

enum class PickupType { FireBall, SwordBeam, Freeze, Heal, Mana };

// Forward declare Character so subclasses can call player methods in OnCollect
class Character;

// ─────────────────────────────────────────────────────────────
// Base Pickup class — all pickup types inherit from this.
// To add a new pickup:
//   1. Add its type to PickupType enum above
//   2. Create a new .h/.cpp that inherits from Pickup
//   3. Implement Init(), Draw(), OnCollect(), GetType(), GetCollisionRec()
//   4. Add it to the drop pool weights in Engine::SpawnEnemyDrop()
// ─────────────────────────────────────────────────────────────
class Pickup
{
public:
    virtual ~Pickup() = default;

    virtual void        Init(Vector2 spawnPos)       = 0;
    virtual void        Draw(Vector2 worldOffset)    = 0;
    virtual void        OnCollect(Character& player) = 0;
    virtual PickupType  GetType()          const     = 0;
    virtual Rectangle   GetCollisionRec()  const     = 0;

    bool    IsActive()      const { return _isActive; }
    void    Destroy()             { _isActive = false; }
    Vector2 GetWorldPos()   const { return _worldPos; }

    // Marks pickups that were spawned by the world timer (shown on mini-map)
    bool IsTimerSpawned()   const { return _timerSpawned; }
    void SetTimerSpawned(bool v)  { _timerSpawned = v; }

protected:
    Vector2 _worldPos{};
    bool    _isActive     = false;
    bool    _timerSpawned = false;
};
