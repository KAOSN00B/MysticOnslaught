#pragma once

#include "raylib.h"
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// EnemyProjectile — generic straight-flying enemy shot.
// Used by the Skeleton Archer (arrows) and the Flame Wisp (fire bolts).
// The enemy owns the attack timing; the Engine owns the projectile list so
// collision stays centralized with lavaballs and cyclops lasers.
// ─────────────────────────────────────────────────────────────────────────────
enum class EnemyProjectileKind
{
    Arrow,      // fast, rotated static sprite
    FireBolt,   // slower, animated fireball sheet
    Spit,       // toxic glob drawn with primitives (ChompBug / Toxic Vermin)
};

class EnemyProjectile
{
public:
    EnemyProjectile() = default;
    ~EnemyProjectile() = default;

    void Init(Vector2 spawnPos, Vector2 direction, EnemyProjectileKind kind, int damage,
              const char* tuningKey = nullptr);
    void Update(float dt);
    void Draw(Vector2 worldOffset) const;
    void Destroy() { _isActive = false; }

    bool      IsActive()        const { return _isActive; }
    int       GetDamage()       const { return _damage; }
    Vector2   GetWorldPos()     const { return _worldPos; }
    Rectangle GetCollisionRec() const;
    EnemyProjectileKind GetKind() const { return _kind; }

    static void UnloadSharedResources();

private:
    static void EnsureTexturesLoaded();

    Vector2 _worldPos{};
    Vector2 _direction{ 1.f, 0.f };
    std::string _tuningKey;
    EnemyProjectileKind _kind = EnemyProjectileKind::Arrow;
    int   _damage      = 1;
    float _speed       = 700.f;
    float _lifeTimer   = 0.f;
    float _runningTime = 0.f;
    int   _frame       = 0;
    bool  _isActive    = false;

    static constexpr float _arrowSpeed        = 780.f;
    static constexpr float _fireBoltSpeed     = 460.f;
    static constexpr float _spitSpeed         = 540.f;
    static constexpr float _maxLife           = 5.f;
    static constexpr float _arrowScale        = 3.4f;
    static constexpr float _fireBoltScale     = 4.6f;
    static constexpr int   _fireBoltFrameW    = 32;
    static constexpr int   _fireBoltFrameH    = 32;
    static constexpr int   _fireBoltFrames    = 8;
    static constexpr float _fireBoltFrameTime = 1.f / 16.f;

    static Texture2D _sharedArrowTex;
    static Texture2D _sharedFireBoltTex;
    static bool      _sharedTexturesLoaded;
};
