#pragma once

#include "Enemy.h"
#include "raymath.h"
#include <vector>
#include <memory>

// =============================================================================
// Venomfang — a darting reptile; the POISON member of the curated elite-bruiser
// pool. The fastest bruiser: it uses the base melee AI's flanking to slip in
// off-angle, and every bite leaves lingering venom — a long, weak damage-over-
// time (several small delayed ticks) that punishes shrugging the hit off.
// Role: Assassin — quick, fragile for a bruiser, always at your back.
// =============================================================================
class Venomfang : public Enemy
{
public:
    Venomfang(Vector2 pos);
    ~Venomfang() override;

    void Init();
    void ResetForSpawn(Vector2 pos) override;
    static void UnloadSharedResources();

    void SetWaveScale(int wave) override;
    void SetVariantTier(int tier) override;
    Rectangle GetCollisionRec() const override;
    Capsule2D GetCapsule()      const override;

    Venomfang* AsVenomfang() override { return this; }
    CreatureFamily GetCreatureFamily() const override { return CreatureFamily::Beast; }

    EliteArchetype GetEliteArchetype() const override { return EliteArchetype::Venomfang; }
    EnemyRole   GetEncounterRole() const override { return EnemyRole::Assassin; }
    int         GetSpawnCost()     const override { return 3; }
    const char* GetTuningName()    const override { return "Venomfang"; }

    // Poison identity: every landed bite leaves a lingering venom DoT.
    void OnMeleeHitPlayer(Character* target) override;

    void PlayAttackSound() override;
    void PlayDeathSound() override;

private:
    static void EnsureSharedResourcesLoaded();
    void SetIdleAnimation(bool resetFrame);

    int _variantTier = 0;

    // Venom applied per bite: slower, weaker, longer than the Infernal's burn.
    static constexpr float kVenomTickDelay     = 0.7f;
    static constexpr int   kVenomTickCount     = 5;
    static constexpr float kVenomDamagePerTick = 0.3f;

    // Hit-and-run disengage dart after each landed bite.
    static constexpr float kDisengageSpeed    = 1400.f;
    static constexpr float kDisengageDuration = 0.35f;

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
