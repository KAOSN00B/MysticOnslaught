#pragma once

#include "Enemy.h"
#include "raymath.h"
#include <vector>
#include <memory>

// =============================================================================
// Ragebrute — a slab of green muscle; the RAGE member of the curated elite-
// bruiser pool. No element, pure escalation: once its HP drops below half it
// ENRAGES — permanently faster with half the delay between pummels — and
// announces it with a floating "ENRAGED" callout. The fight has two halves:
// burst it down before the flip, or survive the frenzy.
// Role: Charger.
// =============================================================================
class Ragebrute : public Enemy
{
public:
    Ragebrute(Vector2 pos);
    ~Ragebrute() override;

    void Init();
    void ResetForSpawn(Vector2 pos) override;
    static void UnloadSharedResources();

    void SetWaveScale(int wave) override;
    void SetVariantTier(int tier) override;
    Rectangle GetCollisionRec() const override;
    Capsule2D GetCapsule()      const override;

    Ragebrute* AsRagebrute() override { return this; }
    CreatureFamily GetCreatureFamily() const override { return CreatureFamily::Flesh; }

    EnemyRole   GetEncounterRole() const override { return EnemyRole::Charger; }
    int         GetSpawnCost()     const override { return 4; }
    const char* GetTuningName()    const override { return "Ragebrute"; }

    // Rage identity: crossing half HP enrages it (one-way until respawn).
    void TakeDamage(int damage, Vector2 attackerPos) override;

    void PlayAttackSound() override;
    void PlayDeathSound() override;

private:
    static void EnsureSharedResourcesLoaded();
    void SetIdleAnimation(bool resetFrame);

    int  _variantTier = 0;
    bool _raging      = false;   // latched once HP crosses the threshold

    static constexpr float kRageHpFraction = 0.5f;   // enrage below this HP

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
