#pragma once

#include "Enemy.h"

// =============================================================================
// Shieldbearer — an armoured orc whose raised shield blocks frontal hits.
// Reuses the base grunt chase/attack behaviour, but TakeDamage ignores hits
// arriving from the side it faces (with a visible spark). Three ways through
// (Balance::ShieldGuard): flank it; hit it while it swings (the shield drops
// and comes back up a moment after); or keep hitting the shield until the
// guard meter breaks, which staggers it with the shield down. Damage over
// time always gets through.
// Variants: brown -> black -> red.
// =============================================================================
class Shieldbearer : public Enemy
{
public:
    Shieldbearer(Vector2 pos);
    CreatureFamily GetCreatureFamily() const override { return CreatureFamily::Beast; }
    ~Shieldbearer() override;

    void Init();
    void ResetForSpawn(Vector2 pos) override;
    static void UnloadSharedResources();

    void Update(float dt, Vector2 heroWorldPos, Vector2 navigationTarget, bool hasNavigationTarget,
        const std::vector<std::unique_ptr<Enemy>>& enemies, const std::vector<Vector2>& propCenters) override;
    void SetWaveScale(int wave) override;
    void SetVariantTier(int tier) override;
    void TakeDamage(int damage, Vector2 attackerPos) override;
    // Puncture Shot and damage-over-time ticks ignore the raised shield — deal
    // straight to the base HP without touching the guard meter.
    void TakeDamageUnblockable(int damage, Vector2 attackerPos) override { Enemy::TakeDamage(damage, attackerPos); }
    void DrawEnemy(Vector2 heroWorldPos) override;

    // ── Guard (Balance::ShieldGuard) ─────────────────────────────────────────
    // Up unless it is swinging, just finished a swing, or its guard is broken.
    bool  IsShieldRaised() const;
    bool  IsGuardBroken() const { return _guardBreakTimer > 0.f; }
    float GetGuardFraction() const;   // 0 = empty meter, 1 = about to break
    // Flow self-test: the swing state the shield reads, without running the AI.
    void  TestSetSwinging(bool swinging) { _attacking = swinging; }
    Rectangle GetCollisionRec() const override;
    void PlayDeathSound() override;

    Shieldbearer* AsShieldbearer() override { return this; }
    // Tank feeds the shared CombatEngagement screening logic (Task 3):
    // ComputeEngagementTarget seats a Tank-role Support intent on the line
    // between the ally centroid and the player, biased toward the player —
    // "occupying the line between that ally and the player" (design doc).
    // Shieldbearer needs no positioning code of its own; it inherits base
    // Enemy::HandleMovement's engagement consumption unchanged. It commits
    // offensively (chases/attacks normally) whenever BuildEngagementAssignments
    // grants it a Commit slot, i.e. "when its formation breaks or a slot is
    // available."
    EnemyRole GetEncounterRole() const override { return EnemyRole::Tank; }
    int       GetSpawnCost()     const override { return 3; }
    const char* GetTuningName() const override { return "Shieldbearer"; }

    void PlayAttackSound() override;

private:
    static void EnsureSharedResourcesLoaded();

    float GuardCapacity() const;
    void  DrawGuardBar(Vector2 screenPos) const;

    int   _variantTier     = 0;    // 0 brown, 1 black, 2 red
    // Shield spark after a blocked hit: GetTime() when the block happened.
    static constexpr float  kBlockFlashSeconds = 0.22f;
    static constexpr double kNoBlockFlash      = -1.0e9;   // long ago: no spark
    double _blockFlashStart = kNoBlockFlash;

    // Guard meter: blocked damage so far; it drains after a quiet spell.
    float _guard              = 0.f;
    float _guardRecoverDelay  = 0.f;   // seconds left before the meter drains
    float _guardBreakTimer    = 0.f;   // > 0 while staggered with the shield down
    float _shieldRaiseTimer   = 0.f;   // > 0 while the shield comes back up after a swing

    // Guard bar under the health bar, and the GUARD BREAK callout.
    static constexpr float kGuardBarGap          = 2.f;    // px between the health bar and the guard bar
    static constexpr float kGuardBarHeight       = 4.f;
    static constexpr float kGuardBarWidthShare   = 0.8f;   // of the sprite width, like the health bar
    static constexpr float kGuardLabelFontSize   = 20.f;
    static constexpr float kGuardLabelSpacing    = 1.f;
    static constexpr float kGuardLabelRise       = 22.f;   // px above the bars
    static constexpr float kGuardLabelSeconds    = 1.f;    // how long GUARD BREAK stays up
    static constexpr float kGuardLabelShadowPx   = 1.f;
    static constexpr float kGuardLabelShadowAlpha = 0.6f;
    static constexpr float kGuardBreakVolume     = 0.6f;
    static constexpr Color kGuardFillColor       { 200, 215, 235, 255 };   // steel: meter filling
    static constexpr Color kGuardBrokenColor     { 255, 140,  60, 255 };   // orange: shield down, time left
    static constexpr Color kGuardBackColor       {  25,  25,  35, 200 };

    static constexpr int kVariantCount = 3;
    static Texture2D _sharedIdleAnim[kVariantCount];
    static Texture2D _sharedWalkAnim[kVariantCount];
    static Texture2D _sharedAttackAnim[kVariantCount];
    static Texture2D _sharedHurtAnim[kVariantCount];
    static Texture2D _sharedDeathAnim[kVariantCount];
    static Sound     _sharedAttackSound;
    static Sound     _sharedHurtSound;
    static Sound     _sharedDeathSound;
    static Sound     _sharedBlockSound;
    static bool      _sharedResourcesLoaded;
};
