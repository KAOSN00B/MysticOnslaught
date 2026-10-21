#pragma once

#include "DamageSource.h"
#include "raylib.h"
#include <string>

// The boss owns the attack timing, but the engine owns the active projectile
// list so collision stays centralized with the other ability projectiles. This
// class handles the large animated lavaball sprite while it travels, then
// swaps to the large hit animation when it collides with the player or arena.
class LavaBallProjectile
{
public:
    LavaBallProjectile() = default;
    ~LavaBallProjectile() = default;

    void Init(Vector2 spawnPos, Vector2 direction, const char* tuningKey = nullptr);
    void Update(float dt);
    void Draw(Vector2 worldOffset) const;
    void BeginHit();
    void Destroy();

    bool IsActive() const { return _isActive; }
    // Arena Pressure themes the shared shot art per elite archetype (ice blue,
    // storm gold, toxic green); WHITE keeps the normal lava look.
    void SetTint(Color tint) { _tint = tint; }
    // Draw scale and hitbox (world px) for this shot; Arena Pressure volleys
    // use a fire bolt's size. Defaults are the boss lava ball's. A tuning key's
    // hitbox, when set, still wins.
    void SetSize(float drawScale, float hitSize) { _drawScale = drawScale; _hitSize = hitSize; }
    bool IsFlying() const { return _state == State::Flying; }
    bool HasHitPlayer() const { return _playerHit; }
    void OnPlayerHit() { _playerHit = true; }
    // Who fired it (the run log's cause of death when it lands).
    void SetSource(const DamageSource& source) { _source = source; }
    const DamageSource& GetSource() const { return _source; }
    Vector2 GetWorldPos() const { return _worldPos; }
    Rectangle GetCollisionRec() const;

    static void UnloadSharedResources();

private:
    DamageSource _source;
    enum class State
    {
        Flying,
        Exploding
    };

    static void EnsureSharedResourcesLoaded();
    void AdvanceAnimation(float dt, int frameCount, float frameTime, bool loop);
    void DrawFlyingSheet(Vector2 worldOffset, float rotation) const;
    void DrawHitSheet(Vector2 worldOffset) const;

    Vector2 _worldPos{};
    Vector2 _direction{ 1.f, 0.f };
    Color _tint = WHITE;
    std::string _tuningKey;
    float _lifeTimer = 0.f;
    float _runningTime = 0.f;
    int _frame = 0;
    State _state = State::Flying;
    bool _isActive = false;
    bool _playerHit = false;

    static constexpr float _speed = 520.f;
    static constexpr float _maxLife = 6.f;
    static constexpr float kDefaultDrawScale = 4.4f;
    static constexpr float kDefaultHitSize = 64.f;
    float _drawScale = kDefaultDrawScale;
    float _hitSize = kDefaultHitSize;
    static constexpr int _flyingFrameCount = 8;
    static constexpr int _hitFrameCount = 5;
    static constexpr float _frameTime = 1.f / 12.f;
    static constexpr float _hitFrameTime = 1.f / 16.f;
    static constexpr int _flyingColumns = 8;
    static constexpr int _flyingRows = 1;
    static constexpr int _hitColumns = 5;
    static constexpr int _hitRows = 1;

    static Texture2D _sharedLavaBallTex;
    static Texture2D _sharedLavaBallHitTex;
    static bool _sharedResourcesLoaded;
};
