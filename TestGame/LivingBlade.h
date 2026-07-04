#pragma once

#include "Enemy.h"

// =============================================================================
// LivingBlade — a possessed sword that moves ONLY in dash bursts:
//   REST (vulnerable, wobbling in place) -> WINDUP (brief aim, glinting)
//   -> DASH (spinning burst toward the player with slight aim scatter).
// It damages anything it passes through while dashing and is hardest to hit
// mid-dash — punish it during the rest beats.
// Variants: rusted -> shadow -> blood.
// =============================================================================
class LivingBlade : public Enemy
{
public:
    LivingBlade(Vector2 pos);
    ~LivingBlade() override;

    void Init();
    void ResetForSpawn(Vector2 pos) override;
    static void UnloadSharedResources();

    void Update(float dt, Vector2 heroWorldPos, Vector2 navigationTarget, bool hasNavigationTarget,
        const std::vector<std::unique_ptr<Enemy>>& enemies,
        const std::vector<Vector2>& propCenters) override;
    void SetWaveScale(int wave) override;
    void SetVariantTier(int tier) override;
    void DrawEnemy(Vector2 heroWorldPos) override;
    Rectangle GetCollisionRec() const override;
    Capsule2D GetCapsule() const override;

    LivingBlade* AsLivingBlade() override { return this; }
    const char* GetTuningName() const override { return "LivingBlade"; }
    bool UsesDirectPursuit() const override { return true; }

    void PlayAttackSound() override;

private:
    enum class BladeState { Resting, WindingUp, Dashing };

    static void EnsureSharedResourcesLoaded();
    void HandleAnimationLoop(float dt);

    int        _variantTier = 0;   // 0 rusted, 1 shadow, 2 blood
    BladeState _state       = BladeState::Resting;
    float      _stateTimer  = 0.f;
    Vector2    _dashDirection{ 1.f, 0.f };
    bool       _dashHitApplied = false;
    float      _spinAngle   = 0.f;
    float      _wobbleTimer = 0.f;

    static constexpr float kRestDuration   = 0.55f;
    static constexpr float kWindupDuration = 0.30f;
    static constexpr float kDashDuration   = 0.30f;
    static constexpr float kDashSpeed      = 820.f;
    static constexpr float kAimScatter     = 0.45f;   // radians of random aim error

    static constexpr int kVariantCount = 3;
    static Texture2D _sharedIdleAnim[kVariantCount];
    static Texture2D _sharedWalkAnim[kVariantCount];
    static Texture2D _sharedAttackAnim[kVariantCount];
    static Texture2D _sharedHurtAnim[kVariantCount];
    static Texture2D _sharedDeathAnim[kVariantCount];
    static Sound     _sharedDashSound;
    static Sound     _sharedHurtSound;
    static Sound     _sharedDeathSound;
    static bool      _sharedResourcesLoaded;
};
