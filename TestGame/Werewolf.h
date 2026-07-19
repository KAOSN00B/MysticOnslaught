#pragma once

#include "Enemy.h"
#include "raymath.h"
#include <vector>
#include <memory>

// =============================================================================
// Werewolf — boss of the Forest. Pure melee predator built on relentless speed:
//   - Fastest boss on foot; circles slightly instead of walking straight in.
//   - Savage Combo: two-swipe melee chain (second swipe steps forward).
//   - Pounce: telegraphed leap onto the player's position (Jump anim, landing
//     swipe with a generous arc).
//   - Blood Howl at 66% and 33% HP: stops to howl, then enters a frenzy —
//     faster everything for 6 seconds (red glow). Howl is a free damage window.
// No projectiles, no summons: the fight is pure spacing and stamina.
// =============================================================================
class Werewolf : public Enemy
{
public:
    Werewolf(Vector2 pos);
    CreatureFamily GetCreatureFamily() const override { return CreatureFamily::Beast; }
    ~Werewolf() override;

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

    Werewolf* AsWerewolf() override { return this; }
    bool IsBoss() const override { return true; }
    bool UsesDirectPursuit() const override { return true; }

    bool ConsumeImpactShakeRequest();

    // ── Hybrid encounter pattern: Blood Moon Hunt ────────────────────────────
    void DrawEliteTelegraph() const override;
    void DebugForceEliteSignature() override;
    void DebugForceElitePhaseTwo() override;
    const char* GetEliteSignatureStateName() const override;

    // Character Animator support
    const char* GetTuningName() const override { return "Werewolf"; }
    int         GetEditorAnimCount() const override { return 8; }
    const char* GetEditorAnimName(int index) const override;
    void        PlayEditorAnim(int index) override;
    int         GetCurrentAnimSlot() const override;

private:
    enum class State
    {
        Chasing,
        Combo1,         // first swipe
        Combo2,         // forward-stepping second swipe
        Combo3,         // phase 1+ third swipe (Rabid Combo)
        PounceCharging,
        Airborne,
        Landing,
        Howling,
        Recovery
    };

    static void EnsureSharedResourcesLoaded();
    void SetAnimation(const Texture2D& sheet, float frameTime, bool resetFrame);
    void HandleChasing(float dt, Vector2 heroWorldPos);
    void HandleCombo(int step, float dt);   // step 1..3 (3 unlocks at phase 1)
    void HandleAirborne(float dt);
    void TryDealContactDamage();
    Rectangle GetBodyContactRec() const;
    Vector2 GetPushDirectionToPlayer() const;
    void HandleAnimation(float dt);
    bool IsFrenzied() const { return _frenzyTimer > 0.f; }

    State _state = State::Chasing;
    float _stateTimer       = 0.f;
    float _comboCooldown    = 0.f;
    float _contactCooldown  = 0.f;
    float _pounceCooldown   = 0.f;
    float _frenzyTimer      = 0.f;
    bool  _howledAt66       = false;
    bool  _howledAt33       = false;
    Vector2 _pounceStart{};
    Vector2 _pounceTarget{};
    float _airborneTimer    = 0.f;
    bool  _swipeDamageApplied = false;
    bool  _landingDamageApplied = false;
    bool  _pounceChainUsed  = false;   // phase 2: one extra immediate pounce per leap
    bool  _impactShakeRequested = false;
    float _circleSign       = 1.f;

    // ── Pounce fairness ──────────────────────────────────────────────────────
    // The landing marker tracks the player for most of the windup, then LOCKS
    // (clamped to navigable ground) — after lock it never moves, so the marker
    // the player reads is always the true landing point.
    bool _pounceLocked = false;
    static constexpr float _pounceLockFraction = 0.68f;   // lock at 68% of the windup

    // ── Survival set piece: Claw Hunt / Blood Moon Hunt ──────────────────────
    // Phase two (RABID): three sequential claw lanes with safe gaps, then a
    // locked pounce through the final lane. Phase three (BLOOD MOON): TWO
    // individually telegraphed pounces — if both miss, a long exhausted
    // recovery is the reward for dodging twice.
    enum class SetPieceStep { None, ClawLanes, Pouncing, Exhausted };
    void BeginSetPiece();
    void UpdateSetPiece(float dt);
    void OnSetPieceLanding(bool landedHit);

    SetPieceStep _setPieceStep = SetPieceStep::None;
    float _setPieceTimer = 0.f;
    float _setPieceCooldown = 9.f;
    int   _setPieceLanesRemaining = 0;
    int   _setPiecePouncesRemaining = 0;
    bool  _setPieceAnyPounceHit = false;
    const std::vector<Vector2>* _framePropCenters = nullptr;   // frame-scoped, for the lock clamp
    float _stableFrameW = 0.f;
    float _stableFrameH = 0.f;

    static constexpr int   _sheetFrameCount     = 6;
    static constexpr float _bossScale           = 5.4f;
    static constexpr float _moveSpeed           = 305.f;
    static constexpr float _meleeRange          = 185.f;
    static constexpr float _comboCooldownBase   = 1.5f;
    static constexpr float _contactCooldownBase = 0.75f;
    static constexpr float _bossDamagePerHit    = 0.5f;
    static constexpr float _bossPushSpeed       = 1350.f;
    static constexpr float _pounceChargeDuration = 0.6f;
    static constexpr float _pounceCooldownBase  = 5.0f;
    static constexpr float _pounceAirDuration   = 0.5f;
    static constexpr float _landingRadius       = 190.f;
    static constexpr float _howlDuration        = 1.6f;
    static constexpr float _frenzyDuration      = 6.f;
    static constexpr float _recoveryDuration    = 0.45f;
    static constexpr int   _bossBaseExpValue    = 15;

    static Texture2D _sharedIdleAnim;
    static Texture2D _sharedWalkAnim;
    static Texture2D _sharedMeleeAnim;
    static Texture2D _sharedMelee2Anim;
    static Texture2D _sharedJumpAnim;
    static Texture2D _sharedHowlAnim;
    static Texture2D _sharedHurtAnim;
    static Texture2D _sharedDeathAnim;
    static Sound     _sharedAttackSound;
    static Sound     _sharedHurtSound;
    static Sound     _sharedDeathSound;
    static Sound     _sharedHowlSound;
    static bool      _sharedResourcesLoaded;
};
