#pragma once

#include "Enemy.h"
#include "raymath.h"
#include <vector>
#include <memory>

// =============================================================================
// AbyssSlime — boss of the deep biomes (Demons Insides, Wastelands, Dream Realm).
// A massive gel horror that NEVER walks — all movement is bouncing:
//   - Locomotion: rhythmic short hops toward the player (grounded pauses are
//     the windows to hit it).
//   - Acid trail: hop landings leave corrosive puddles that damage the player
//     standing in them — the arena slowly fills with no-go zones.
//   - Crushing Leap: telegraphed jump onto the player's position with a landing
//     shockwave (dodge the landing ring) that always leaves a big puddle.
//   - Abyss Call: at 66% and 33% HP it stops to cast, summoning small slimes.
//   - Below 33% HP it enrages: faster hops, shorter pauses.
// Stays inside the shared enemy pool like Molarbeast so wave-clear checks,
// minimap, and death handling keep working unchanged.
// =============================================================================
class AbyssSlime : public Enemy
{
public:
    AbyssSlime(Vector2 pos);
    ~AbyssSlime() override;

    void Init();
    void ResetForSpawn(Vector2 pos) override;
    static void UnloadSharedResources();

    void Update(float dt, Vector2 heroWorldPos, Vector2 navigationTarget, bool hasNavigationTarget,
        const std::vector<std::unique_ptr<Enemy>>& enemies,
        const std::vector<Vector2>& propCenters) override;
    void SetWaveScale(int wave) override;
    void DrawEnemy(Vector2 cameraRef) override;
    void TakeDamage(int damage, Vector2 attackerPos) override;
    void ApplyFreeze(float duration) override;
    Rectangle GetCollisionRec() const override;
    Capsule2D GetCapsule()      const override;
    void DrawHealthBar(Vector2 screenPos, float w, float h) override;

    AbyssSlime* AsAbyssSlime() override { return this; }
    bool IsBoss() const override { return true; }
    bool UsesDirectPursuit() const override { return true; }

    // Summon interface (read by CombatDirector after Update):
    // returns how many small slimes to spawn this frame (0 = none).
    int  ConsumeSummonRequest();
    bool ConsumeImpactShakeRequest();

private:
    enum class State
    {
        Chasing,
        MeleeAttacking,
        JumpCharging,   // crouch telegraph
        Airborne,       // travelling toward the landing spot
        Landing,        // shockwave frame
        Summoning,      // Abyss Call cast
        Recovery
    };

    static void EnsureSharedResourcesLoaded();

    void SetAnimation(const Texture2D& sheet, float frameTime, bool resetFrame);
    void HandleChasing(float dt, Vector2 heroWorldPos);
    void HandleMelee();
    void HandleJumpCharge(float dt);
    void HandleAirborne(float dt);
    void HandleLanding(float dt);
    void HandleSummoning(float dt);
    void HandleRecovery(float dt);
    void HandleAnimation(float dt);
    void TryDealContactDamage();
    Rectangle GetBodyContactRec() const;
    Vector2 GetPushDirectionToPlayer() const;
    void BeginJump();
    bool IsEnraged() const { return _health <= _maxHealth * 0.33f; }

    // Corrosive puddle left behind by hop/leap landings.
    struct SlimePuddle
    {
        Vector2 pos{};
        float   timer  = 0.f;   // counts down to expiry
        float   radius = 95.f;
    };

    void UpdatePuddles(float dt);
    void SpawnPuddle(Vector2 pos, float radius);
    void DrawPuddles(Vector2 cameraRef) const;

    State _state = State::Chasing;
    float _stateTimer       = 0.f;
    float _meleeCooldown    = 0.f;
    float _contactCooldown  = 0.f;
    float _jumpCooldown     = 0.f;
    Vector2 _jumpStartPos{};
    Vector2 _jumpTargetPos{};
    float _airborneTimer    = 0.f;
    float _airborneDuration = 0.7f;
    bool  _landingDamageApplied = false;
    bool  _summonedAt66     = false;
    bool  _summonedAt33     = false;
    int   _pendingSummonCount = 0;
    bool  _impactShakeRequested = false;
    float _stableFrameW = 0.f;
    float _stableFrameH = 0.f;

    // Hop locomotion (replaces walking entirely)
    bool    _isHopping      = false;
    float   _hopTimer       = 0.f;
    float   _hopPauseTimer  = 0.f;
    Vector2 _hopDirection{ 1.f, 0.f };
    int     _hopsSincePuddle = 0;

    std::vector<SlimePuddle> _puddles;
    float _puddleDamageCooldown = 0.f;

    static constexpr int   _sheetFrameCount   = 6;
    static constexpr float _bossScale         = 5.6f;
    static constexpr float _moveSpeed         = 215.f;   // legacy base; hops use _hopSpeed
    static constexpr float _hopSpeed          = 520.f;   // travel speed while mid-hop
    static constexpr float _hopDuration       = 0.42f;
    static constexpr float _hopPauseBase      = 0.55f;   // grounded window between hops
    static constexpr float _puddleLifetime    = 6.5f;
    static constexpr float _puddleTickDamage  = 0.25f;
    static constexpr float _puddleTickInterval = 0.7f;
    static constexpr int   _maxPuddles        = 14;
    static constexpr float _meleeRange        = 190.f;
    static constexpr float _meleeCooldownBase = 1.9f;
    static constexpr float _contactCooldownBase = 0.75f;
    static constexpr float _bossDamagePerHit  = 0.5f;
    static constexpr float _bossPushSpeed     = 1350.f;
    static constexpr float _jumpChargeDuration = 0.85f;
    static constexpr float _jumpCooldownBase  = 4.6f;
    static constexpr float _landingRadius     = 230.f;
    static constexpr float _recoveryDuration  = 0.5f;
    static constexpr float _summonDuration    = 1.4f;
    static constexpr int   _bossBaseExpValue  = 15;

    static Texture2D _sharedIdleAnim;
    static Texture2D _sharedWalkAnim;
    static Texture2D _sharedMeleeAnim;
    static Texture2D _sharedMagicAnim;
    static Texture2D _sharedJumpAnim;
    static Texture2D _sharedFallAnim;
    static Texture2D _sharedHurtAnim;
    static Texture2D _sharedDeathAnim;
    static Sound     _sharedAttackSound;
    static Sound     _sharedHurtSound;
    static Sound     _sharedDeathSound;
    static Sound     _sharedLandSound;
    static bool      _sharedResourcesLoaded;
};
