#pragma once

#include "Enemy.h"
#include "raymath.h"
#include <vector>
#include <memory>

// =============================================================================
// AncientBear — boss of the Dream Realm. A colossal rune-marked guardian and
// the hardest-hitting boss in the game:
//   - Dream Pull: a roar that DRAGS the player toward him over a second
//     (fight the pull by dashing away), always followed by…
//   - Crushing Slam: huge overhead swipe with a wide damage arc.
//   - Slow, deliberate walker with heavy paw swipes — highest HP after the
//     Titan Guard, and every hit hurts.
//   - Below 35% HP his runes ignite: pulls come faster and reach further.
// =============================================================================
class AncientBear : public Enemy
{
public:
    AncientBear(Vector2 pos);
    CreatureFamily GetCreatureFamily() const override { return CreatureFamily::Beast; }
    ~AncientBear() override;

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
    Capsule2D GetCapsule() const override;

    AncientBear* AsAncientBear() override { return this; }
    bool IsBoss() const override { return true; }
    bool UsesDirectPursuit() const override { return true; }

    bool ConsumeImpactShakeRequest();
    // CombatDirector reads these so the slam's impact ART matches the warned
    // shape (bursts around the ring / along the wedge, not just at his feet).
    int   GetSlamVariant() const { return _slamVariant; }
    float GetSlamWedgeAngle() const { return _slamWedgeAngle; }
    float GetSlamRingMidRadius() const { return (_slamRingInnerRadius + _slamRingOuterRadius) * 0.5f; }
    float GetSlamWedgeReach() const { return _slamWedgeRadius; }

    // ── Hybrid encounter pattern: Dream Collapse ─────────────────────────────
    // Every slam draws its danger shape during the windup, and the SHAPE is
    // what escalates: discs (safe outside), then a ring (safe in close OR far
    // out), then three 120-degree wedges rotating in a fixed, learnable order.
    // The pull changes your movement problem; the shapes are the answer key.
    void DrawEliteTelegraph() const override;
    void DebugForceEliteSignature() override;
    void DebugForceElitePhaseTwo() override;
    const char* GetEliteSignatureStateName() const override;

    // Character Animator support
    const char* GetTuningName() const override { return "AncientBear"; }
    int         GetEditorAnimCount() const override { return 6; }
    const char* GetEditorAnimName(int index) const override;
    void        PlayEditorAnim(int index) override;
    int         GetCurrentAnimSlot() const override;

private:
    enum class State
    {
        Lumbering,
        MeleeAttacking,
        Roaring,        // Dream Pull — drags the player closer
        SlamWindup,
        Slamming,       // wide-arc crushing blow
        Recovery
    };

    static void EnsureSharedResourcesLoaded();
    void SetAnimation(const Texture2D& sheet, float frameTime, bool resetFrame);
    void TryDealContactDamage();
    Rectangle GetBodyContactRec() const;
    Vector2 GetPushDirectionToPlayer() const;
    void HandleAnimation(float dt);
    bool IsRuneIgnited() const { return IsEnraged(); }  // latched phase (see _enrageThreshold)

    State _state = State::Lumbering;
    float _stateTimer      = 0.f;
    float _meleeCooldown   = 0.f;
    float _contactCooldown = 0.f;
    float _roarCooldown    = 0.f;
    float _pullTickTimer   = 0.f;
    bool  _slamDamageApplied = false;
    int   _slamChainRemaining = 0;    // phase 1 = double slam, phase 2 = triple
    bool  _pendingPhaseRoar   = false;// phase change opens with a Dream Pull -> slam
    bool  _impactShakeRequested = false;

    // Slam variants (Dream Collapse): 0 = disc, 1 = ring, 2 = wedge. The chain
    // sets each slam's variant; wedges rotate 120 degrees per slam.
    int   _slamVariant = 0;
    int   _slamIndexInChain = 0;
    float _slamWedgeAngle = 0.f;      // radians, wedge centre
    static constexpr float _slamRingInnerRadius = 200.f;
    static constexpr float _slamRingOuterRadius = 420.f;
    static constexpr float _slamWedgeRadius     = 360.f;
    static constexpr float _slamWedgeHalfAngle  = 1.047f;   // 60 degrees each side
    float _stableFrameW = 0.f;
    float _stableFrameH = 0.f;

    static constexpr int   _sheetFrameCount     = 6;
    static constexpr float _bossScale           = 5.0f;
    static constexpr float _moveSpeed           = 155.f;
    static constexpr float _meleeRange          = 235.f;
    static constexpr float _meleeCooldownBase   = 1.9f;
    static constexpr float _contactCooldownBase = 0.75f;
    static constexpr float _bossDamagePerHit    = 1.0f;    // heaviest paws in the game
    static constexpr float _bossPushSpeed       = 1500.f;
    static constexpr float _roarDuration        = 1.2f;
    static constexpr float _roarCooldownBase    = 6.5f;
    static constexpr float _pullRange           = 620.f;
    static constexpr float _pullStrength        = 300.f;   // pushed toward the bear per tick
    static constexpr float _slamWindupDuration  = 0.55f;
    static constexpr float _slamRadius          = 280.f;
    static constexpr float _recoveryDuration    = 0.65f;
    static constexpr int   _bossBaseExpValue    = 15;

    static Texture2D _sharedIdleAnim;
    static Texture2D _sharedWalkAnim;
    static Texture2D _sharedMeleeAnim;
    static Texture2D _sharedRoarAnim;
    static Texture2D _sharedHurtAnim;
    static Texture2D _sharedDeathAnim;
    static Sound     _sharedAttackSound;
    static Sound     _sharedHurtSound;
    static Sound     _sharedDeathSound;
    static Sound     _sharedRoarSound;
    static bool      _sharedResourcesLoaded;
};
