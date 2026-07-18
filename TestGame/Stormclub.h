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

    void PlayAttackSound() override;
    void PlayDeathSound() override;

private:
    static void EnsureSharedResourcesLoaded();
    void SetIdleAnimation(bool resetFrame);

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
