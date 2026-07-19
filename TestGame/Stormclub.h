#pragma once

#include "Enemy.h"
#include "raymath.h"
#include <vector>
#include <memory>

// =============================================================================
// Stormclub — a hulking ogre with a crackling club; the STORM member of the
// curated elite-bruiser pool. Shares the base melee AI, but its thunderous
// smash BLASTS the player away on contact (Character::ApplyKnockbackImpulse —
// a strong decaying shove, not the wall-locked ogre push) with a lightning
// crack. Role: Charger — getting caught by the club costs you your position.
// =============================================================================
class Stormclub : public Enemy
{
public:
    Stormclub(Vector2 pos);
    ~Stormclub() override;

    void Init();
    void ResetForSpawn(Vector2 pos) override;
    static void UnloadSharedResources();

    void SetWaveScale(int wave) override;
    void SetVariantTier(int tier) override;
    Rectangle GetCollisionRec() const override;
    Capsule2D GetCapsule()      const override;

    Stormclub* AsStormclub() override { return this; }
    CreatureFamily GetCreatureFamily() const override { return CreatureFamily::Flesh; }

    EliteArchetype GetEliteArchetype() const override { return EliteArchetype::Stormclub; }
    EnemyRole   GetEncounterRole() const override { return EnemyRole::Charger; }
    int         GetSpawnCost()     const override { return 4; }
    const char* GetTuningName()    const override { return "Stormclub"; }

    // Storm identity: every landed melee hit blasts the player backward.
    void OnMeleeHitPlayer(Character* target) override;

    // ── Elite signature: The Thunder Breaker ─────────────────────────────────
    // Thunder Leap: a landing marker appears, LOCKS after the windup, and the
    // elite travels there (never teleports, never retargets). Landing spawns a
    // central impact plus three lightning branches with visible angular gaps.
    // A miss leaves the club embedded — the long punish window. TEMPEST at 50%
    // chains two shorter leaps; the second target locks only after the first
    // landing. Signatures run only for the elite-room miniboss.
    bool UpdateEliteSignature(float deltaTime, Vector2 navigationTarget,
        bool hasNavigationTarget, const std::vector<std::unique_ptr<Enemy>>& enemies,
        const std::vector<Vector2>& propCenters) override;
    void DrawEliteTelegraph() const override;
    void DebugForceEliteSignature() override;
    void DebugForceElitePhaseTwo() override;
    const char* GetEliteSignatureStateName() const override;

    void PlayAttackSound() override;
    void PlayDeathSound() override;

private:
    static void EnsureSharedResourcesLoaded();
    void SetIdleAnimation(bool resetFrame);
    void SetSignatureSheet(const Texture2D& sheet);
    void BeginLeapTelegraph(float telegraphSeconds, float leapRange);

    enum class SignatureState { None, LeapTelegraph, Leaping, Recovery };
    SignatureState _signatureState = SignatureState::None;
    float   _signatureTimer    = 0.f;
    float   _signatureCooldown = 0.f;
    Vector2 _leapTarget{};          // locked at the end of the telegraph
    float   _activeLeapRange = 0.f; // full range normally, shorter for Tempest
    int     _leapsRemaining  = 0;   // TEMPEST performs two chained leaps
    bool    _landedOnPlayer  = false;

    int _variantTier = 0;

    // Knockback shove applied on a landed club hit (decays via ApplyVelocity).
    static constexpr float kSmashKnockbackSpeed = 4200.f;

    static constexpr int kVariantCount = 4;
    static Texture2D _sharedIdleAnim[kVariantCount];
    static Texture2D _sharedWalkAnim[kVariantCount];
    static Texture2D _sharedAttackAnim[kVariantCount];
    static Texture2D _sharedTakeDamageAnim[kVariantCount];
    static Texture2D _sharedDeathAnim[kVariantCount];
    static Sound     _sharedAttackSound;
    static Sound     _sharedHurtSound;
    static Sound     _sharedDeathSound;
    static bool      _sharedResourcesLoaded;
};
