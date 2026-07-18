#pragma once

#include "Enemy.h"
#include "raymath.h"
#include <vector>
#include <memory>

// =============================================================================
// Infernal (Infernal Brute) — a hulking horned demon; the first of the curated
// "elite bruiser" pool. It uses the shared melee AI (approach + swing) but its
// strikes are fire-infused: every landed melee hit sets the PLAYER on fire
// (a burn DoT via Character::ApplyBurnTicks). Bigger than the normal roster so
// it reads as a mini-boss even before the elite buff is applied.
// =============================================================================
class Infernal : public Enemy
{
public:
    Infernal(Vector2 pos);
    ~Infernal() override;

    void Init();
    void ResetForSpawn(Vector2 pos) override;
    static void UnloadSharedResources();

    void SetWaveScale(int wave) override;
    void SetVariantTier(int tier) override;
    Rectangle GetCollisionRec() const override;
    Capsule2D GetCapsule()      const override;

    Infernal* AsInfernal() override { return this; }
    CreatureFamily GetCreatureFamily() const override { return CreatureFamily::Flesh; }

    // Curated role: a slow, heavy anchor. Melee, so it also gets the swing-weight
    // lean (UsesAttackLunge defaults true for the Tank role).
    EliteArchetype GetEliteArchetype() const override { return EliteArchetype::Infernal; }
    EnemyRole   GetEncounterRole() const override { return EnemyRole::Tank; }
    int         GetSpawnCost()     const override { return 4; }
    const char* GetTuningName()    const override { return "Infernal"; }

    // Fire identity: burn the player on every landed melee hit.
    void OnMeleeHitPlayer(Character* target) override;

    // ── Elite signature: The Living Furnace ──────────────────────────────────
    // Cinder March (committed line walk that drops spaced flame patches) and
    // Furnace Burst (three forward fissures with walkable gaps). OVERHEATED at
    // 50% adds one staggered fissure wave, slightly faster melee, and a longer
    // exhausted recovery. Signatures run only for the elite-room miniboss —
    // pack Infernals keep plain burning melee.
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
    void EmitFurnaceFissures(float spreadRadians, int fissureCount);

    enum class SignatureState { None, MarchTelegraph, Marching, BurstTelegraph, BurstRecovery };
    SignatureState _signatureState = SignatureState::None;
    float   _signatureTimer     = 0.f;   // time remaining in the current state
    float   _signatureCooldown  = 0.f;   // countdown to the next signature
    Vector2 _signatureDirection{ 1.f, 0.f };  // locked when the telegraph begins
    float   _marchPatchAccumulator = 0.f;     // distance since the last patch
    int     _marchPatchesDropped   = 0;
    bool    _secondWavePending  = false;      // OVERHEATED staggered wave
    float   _secondWaveTimer    = 0.f;

    int   _variantTier = 0;

    // Burn applied to the player on a fire strike (delay, ticks, dmg/tick).
    static constexpr float kBurnTickDelay   = 0.5f;
    static constexpr int   kBurnTickCount   = 3;
    static constexpr float kBurnDamagePerTick = 0.4f;

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
