#pragma once

#include "Enemy.h"
#include "raymath.h"
#include <vector>
#include <memory>

// =============================================================================
// Cyclops — 1-to-1 copy of Enemy movement using Cyclops sprites.
// HandleMovement is virtual so an attack pass can override it later.
// =============================================================================

class Cyclops : public Enemy
{
public:
    enum class FireMode
    {
        Sweep,
        Scatter
    };

    Cyclops(Vector2 pos);
    ~Cyclops() override;

    void Init();
    void ResetForSpawn(Vector2 pos) override;
    static void UnloadSharedResources();

    // Cyclops uses the same pooled enemy interface as regular enemies, but
    // overrides the update to support charge-and-fire ranged attacks.
    void Update(float dt, Vector2 heroWorldPos,
                Vector2 navigationTarget, bool hasNavigationTarget,
                const std::vector<std::unique_ptr<Enemy>>& enemies,
                const std::vector<Vector2>& propCenters) override;

    void SetWaveScale(int wave) override;

    void DrawEnemy(Vector2 cameraRef) override;
    Rectangle GetCollisionRec()    const override;
    Capsule2D GetCapsule()         const override;
    Rectangle GetHitCollisionRec() const override;

    void ApplyFreeze(float duration) override;
    void ApplyBurn(float delay, int damage, Vector2 sourcePos) override;
    void ApplyExternalImpulse(Vector2 impulse, bool cancelLockedAnimation) override;
    void OnForcedPushCollision() override;
    void TakeDamage(int damage, Vector2 attackerPos) override;
    void ApplyElectricCharge() override;
    Cyclops* AsCyclops() override { return this; }

    // Laser firing interface (read by Engine after Update)
    bool    WantsToFire()      const { return _wantsToFire; }
    Vector2 GetFireDirection() const { return _fireDirection; }
    FireMode GetFireMode()     const { return _queuedFireMode; }
    void    OnFired();

    void PlayAttackSound() override;
    void PlayDeathSound()  override;
    void PlayHurtSound()   override;

protected:

    virtual void HandleMovement(float dt, Vector2 navigationTarget, bool hasNavigationTarget,
                                const std::vector<std::unique_ptr<Enemy>>& enemies,
                                const std::vector<Vector2>& propCenters);

private:

    static void EnsureSharedResourcesLoaded();

    // Applies the idle sprite sheet and its frame metadata.
    // This keeps all idle-state setup in one place so the charge logic,
    // hurt recovery, and spawn reset all use the same values.
    void SetIdleAnimation(bool resetFrame);

    // Applies the walk sprite sheet and its frame metadata.
    // Movement code calls this when the cyclops is actually translating,
    // which keeps frame sizing consistent with the current sheet.
    void SetWalkAnimation(bool resetFrame);

    void HandleAnimation(float dt);
    void UpdateBurns(float dt);
    void DrawHealthBar(Vector2 screenPos, float w, float h);
    void CancelCharge();

    struct PendingBurn
    {
        float   timer     = 0.f;
        int     damage    = 0;
        Vector2 sourcePos = {};
    };

    float _facingTimer  = 0.f;   // debounce — flip only after holding new direction

    // Charge + ranged attack
    float   _chargeTimer      = 0.f;
    float   _attackCooldown   = 0.f;
    bool    _charging         = false;
    bool    _wantsToFire      = false;
    Vector2 _fireDirection    = {};
    FireMode _queuedFireMode  = FireMode::Sweep;
    float   _postFireLockTimer = 0.f;

    static constexpr float _stuckThreshold = 0.8f;
    static constexpr float _stuckMinMove   = 8.f;

    // These start at their design-default values and are tuned by SetWaveScale
    // so higher-tier cyclops charge faster and fire more often.
    float _chargeDurationInst   = 1.0f;
    float _attackCooldownMaxInst = 3.5f;
    float _chargeRangeInst      = 480.f;
    float _scatterRangeInst     = 170.f;

    std::vector<PendingBurn> _pendingBurns;

    static Texture2D _sharedIdleAnim;
    static Texture2D _sharedWalkAnim;
    static Texture2D _sharedAttackAnim;
    static Texture2D _sharedTakeDamageAnim;
    static Texture2D _sharedDeathAnim;
    static Sound     _sharedAttackSound;
    static Sound     _sharedHurtSound;
    static Sound     _sharedDeathSound;
    static bool      _sharedResourcesLoaded;
};
