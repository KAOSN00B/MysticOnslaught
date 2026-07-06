#pragma once
#include "raylib.h"
#include "AbilityType.h"

// Generic 8-way spread projectile.
// The element determines visuals and on-hit status effect.
// Fire  → burn   (engine applies burn on hit)
// Ice   → freeze (engine applies freeze on hit)
// Electric → charged status (engine applies charge on hit)
class SpreadProjectile
{
public:
    SpreadProjectile() = default;

    void Init(Vector2 spawnPos, Vector2 direction, AbilityType element);
    // Lightweight class basic-attack shot: small, tinted, short-lived, low damage.
    void InitBasic(Vector2 spawnPos, Vector2 direction, AbilityType element, Color tint);
    bool IsBasic() const { return _basic; }
    void Update(float dt);
    void Draw(Vector2 worldOffset) const;
    void Destroy();

    static void UnloadSharedResources();

    // Returns the animated sprite sheet for a given element so the Engine can
    // draw the same frames during the ultimate cinematic.
    static const Texture2D& GetAnimTexture(AbilityType element);
    // Spread projectile constants (32x32, 8 frames)
    static constexpr int GetFrameW() { return _frameWidth; }
    static constexpr int GetFrameH() { return _frameHeight; }
    static constexpr int GetFrameCount() { return _frameCount; }
    // Per-element helpers — ultimates use 64x64 frames and different counts
    static int GetFrameWFor(AbilityType element);
    static int GetFrameHFor(AbilityType element);
    static int GetFrameCountFor(AbilityType element);

    bool        IsActive()       const;
    Rectangle   GetCollisionRec() const;
    Vector2     GetWorldPos()     const;
    Vector2     GetDirection()    const;
    AbilityType GetElement()      const;

private:
    static void EnsureTexturesLoaded();

    Vector2     _worldPos{};
    Vector2     _direction{};
    AbilityType _element = AbilityType::FireSpread;

    float _speed       = 650.f;
    float _radius      = 56.f;
    float _lifeTimer   = 5.f;
    float _runningTime = 0.f;
    float _updateTime  = 1.f / 16.f;
    int   _frame       = 0;
    bool  _isActive    = false;
    bool  _basic       = false;   // class basic attack (small, low damage)
    Color _tint        { 255, 255, 255, 255 };

    static constexpr int _frameWidth  = 32;
    static constexpr int _frameHeight = 32;
    static constexpr int _frameCount  = 8;

    static Texture2D _fireTex;
    static Texture2D _iceTex;
    static Texture2D _electricTex;
    // Ultimate-specific sprite sheets (64x64 frames)
    static Texture2D _fireUltTex;
    static Texture2D _iceUltTex;
    static Texture2D _electricUltTex;
    static bool      _texturesLoaded;
};
