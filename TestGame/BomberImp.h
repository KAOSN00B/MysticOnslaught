#pragma once

#include "Enemy.h"

// =============================================================================
// BomberImp — a flying demon head that exists only to explode on you.
// Accelerates toward the player; once close it FUSES (flashing, locked path)
// and detonates in a damaging blast. Killing it also sets it off wherever it
// is — so shoot it at range, never sword it point-blank.
// Variants: pale -> red -> purple.
// =============================================================================
class BomberImp : public Enemy
{
public:
    BomberImp(Vector2 pos);
    CreatureFamily GetCreatureFamily() const override { return CreatureFamily::Small; }
    ~BomberImp() override;

    void Init();
    void ResetForSpawn(Vector2 pos) override;
    static void UnloadSharedResources();

    void Update(float dt, Vector2 heroWorldPos, Vector2 navigationTarget, bool hasNavigationTarget,
        const std::vector<std::unique_ptr<Enemy>>& enemies,
        const std::vector<Vector2>& propCenters) override;
    void SetWaveScale(int wave) override;
    void SetVariantTier(int tier) override;
    void TakeDamage(int damage, Vector2 attackerPos) override;
    void DrawEnemy(Vector2 heroWorldPos) override;
    Rectangle GetCollisionRec() const override;
    Capsule2D GetCapsule() const override;

    BomberImp* AsBomberImp() override { return this; }
    const char* GetTuningName() const override { return "BomberImp"; }
    bool UsesDirectPursuit()     const override { return true; }
    bool IgnoresPropCollisions() const override { return true; }

    bool ConsumeImpactShakeRequest();

private:
    enum class ImpState { Seeking, Fusing, Detonating };

    static void EnsureSharedResourcesLoaded();
    void BeginDetonation();
    void HandleAnimationLoop(float dt);

    int      _variantTier = 0;    // 0 pale, 1 red, 2 purple
    ImpState _state       = ImpState::Seeking;
    float    _fuseTimer   = 0.f;
    float    _detonateTimer = 0.f;
    bool     _blastDamageApplied = false;
    bool     _impactShakeRequested = false;
    Vector2  _flyVelocity{};
    float    _bobTimer = 0.f;

    static constexpr float kAcceleration   = 620.f;
    static constexpr float kMaxFlySpeed    = 430.f;
    static constexpr float kFuseDistance   = 110.f;
    static constexpr float kFuseDuration   = 0.55f;
    static constexpr float kBlastRadius    = 165.f;
    static constexpr float kDetonateDuration = 0.45f;

    static constexpr int kVariantCount = 3;
    static Texture2D _sharedIdleAnim[kVariantCount];
    static Texture2D _sharedWalkAnim[kVariantCount];
    static Texture2D _sharedAttackAnim[kVariantCount];
    static Texture2D _sharedHurtAnim[kVariantCount];
    static Texture2D _sharedDeathAnim[kVariantCount];
    static Sound     _sharedFuseSound;
    static Sound     _sharedHurtSound;
    static Sound     _sharedBlastSound;
    static bool      _sharedResourcesLoaded;
};
