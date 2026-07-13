#pragma once

#include "Enemy.h"
#include "BehaviourTree.h"

#include <vector>
#include <memory>

// Molarbeast is the first boss enemy. It stays inside the shared enemy pool so
// the existing wave-clear checks, minimap, death handling, and enemy update
// loops continue to work without a parallel boss system.
class Molarbeast : public Enemy
{
public:
    Molarbeast(Vector2 pos);
    CreatureFamily GetCreatureFamily() const override { return CreatureFamily::Beast; }
    ~Molarbeast() override;

    void Init();
    void ResetForSpawn(Vector2 pos) override;
    static void UnloadSharedResources();

    void Update(float dt, Vector2 heroWorldPos, Vector2 navigationTarget, bool hasNavigationTarget,
        const std::vector<std::unique_ptr<Enemy>>& enemies,
        const std::vector<Vector2>& propCenters) override;
    void SetWaveScale(int wave) override;
    void DrawEnemy(Vector2 cameraRef) override;
    void TakeDamage(int damage, Vector2 attackerPos) override;
    void PlayHurtSound() override;
    void ApplyBurn(float delay, int damage, Vector2 sourcePos) override;
    void ApplyFreeze(float duration) override;
    void ApplyElectricCharge() override;
    Rectangle GetCollisionRec() const override;
    Capsule2D GetCapsule()      const override;

    Molarbeast* AsMolarbeast() override { return this; }
    bool IgnoresPropCollisions() const override { return false; }
    bool IsBoss() const override { return true; }
    int GetExpValue() const override { return _expValue; }

    bool WantsToFireLavaBall() const { return _pendingLavaBallShot; }
    Vector2 GetLavaBallSpawnPos() const;
    Vector2 GetQueuedLavaBallTarget() const { return _queuedLavaBallTarget; }
    void OnLavaBallSpawned();

    bool IsDashing() const { return _state == State::Dashing; }
    void OnDashBlocked();
    bool ConsumeImpactShakeRequest();

private:
    enum class State
    {
        Chasing,
        MeleeAttacking,
        DashCharging,
        Dashing,
        RangedCharging,
        RangedVolley,
        Recovery
    };

    static void EnsureSharedResourcesLoaded();
    void BuildBehaviourTree();

    void SetIdleAnimation(bool resetFrame);
    void SetWalkAnimation(bool resetFrame);
    void SetMeleeAnimation(bool resetFrame);
    void SetRangedAnimation(bool resetFrame);
    void SetDashAnimation(bool resetFrame);
    void SetHurtAnimation(bool resetFrame);
    void SetDeathAnimation(bool resetFrame);

    void HandleChasing(float dt, const std::vector<Vector2>& propCenters);
    void HandleMelee();
    void HandleDashCharge(float dt);
    void HandleDash(float dt, const std::vector<std::unique_ptr<Enemy>>& enemies);
    void HandleRangedCharge(float dt);
    void HandleRangedVolley(float dt);
    void HandleRecovery(float dt);
    void HandleAnimation(float dt);
    void TryDealContactDamage();
    Rectangle GetBodyContactRec() const;
    Rectangle GetThreatCollisionRec() const;
    void ScatterEnemy(Enemy& enemy) const;
    bool HasAlreadyDashedEnemy(const Enemy* enemy) const;
    void ResetSpecialCooldown();
    void ResetMeleeCooldown();
    void ResetContactCooldown();
    void FinishDash(bool blockedByArena);
    void BeginRandomSpecial();
    bool IsInReducedDamageState() const;
    Vector2 GetPushDirectionToPlayer() const;
    float GetSpecialCooldownMin() const;
    float GetSpecialCooldownMax() const;
    float GetChargeDuration() const;

    // ── Behaviour Tree ────────────────────────────────────────────────────────
    std::unique_ptr<BTNode> _behaviorTreeRoot;
    // Frame-scoped cache — set at the top of Update so BT leaf lambdas can
    // reach enemies/propCenters without changing handler signatures.
    const std::vector<std::unique_ptr<Enemy>>* _cachedEnemies     = nullptr;
    const std::vector<Vector2>*                _cachedPropCenters = nullptr;

    State _state = State::Chasing;
    float _orbitAngle = 0.f;   // current angle (radians) around the player for circling
    float _stableFrameW = 0.f;   // frozen at idle-sheet frame size, used by all collision rects
    float _stableFrameH = 0.f;
    Vector2 _navTarget{};        // A* waypoint supplied by the engine each frame
    bool _hasNavTarget = false;
    Vector2 _lockedNavTarget{};
    bool _hasLockedNavTarget = false;
    float _navTargetLockTimer = 0.f;
    Vector2 _escapeDirection{};
    float _escapeTimer = 0.f;
    float _specialTimer = 0.f;
    float _meleeCooldown = 0.f;
    float _contactCooldown = 0.f;
    float _chargeTimer = 0.f;
    float _chargeDuration = 0.f;
    float _recoveryTimer = 0.f;
    float _volleyShotTimer = 0.f;
    int _volleyShotsRemaining = 0;
    Vector2 _dashDirection{ 1.f, 0.f };
    Vector2 _queuedLavaBallTarget{};
    bool _pendingLavaBallShot = false;
    bool _impactShakeRequested = false;
    int  _dashChainRemaining = 0;   // phase 1+: zig-zag follow-up dashes
    std::vector<const Enemy*> _dashedEnemies;

    static constexpr int _sheetFrameCount = 6;
    static constexpr float _bossScale = 7.0f;
    static constexpr float _moveSpeed = 250.f;
    static constexpr float _attackRangeBase = 84.f;
    static constexpr float _attackCooldownBase = 2.0f;
    static constexpr float _contactCooldownBase = 0.75f;
    static constexpr float _bossDamagePerHit = 0.5f;
    static constexpr float _bossPushSpeed = 1450.f;
    static constexpr float _dashSpeed = 750.f;
    static constexpr float _chargeTintStrength = 220.f;
    static constexpr float _recoveryDuration = 0.35f;
    static constexpr float _volleyShotSpacing = 0.25f;
    static constexpr float _scatterImpulse = 1700.f;
    static constexpr float _navTargetReachDistance = 26.f;
    static constexpr float _navTargetRefreshDistance = 42.f;
    static constexpr float _navTargetLockDuration = 0.18f;
    static constexpr float _escapeDuration = 0.35f;
    static constexpr float _escapeBlendStrength = 0.82f;
    static constexpr float _stuckPropSearchRadius = 180.f;
    static constexpr int _bossBaseExpValue = 15;
    // Hurtbox: this is intentionally generous so the player's sword can connect
    // cleanly against the large boss sprite even though the art has a lot of
    // empty space around the head silhouette.
    static constexpr float _collisionWidthScale = 0.52f;
    static constexpr float _collisionHeightScale = 0.68f;
    static constexpr float _collisionYOffsetScale = -0.10f;
    // Body contact should be narrower than the hurtbox so the player can space
    // near the boss without taking unavoidable damage.
    static constexpr float _bodyContactInset = 36.f;
    // The actual melee swing reaches farther than the passive body collision.
    static constexpr float _attackFrontPadding = 34.f;

    static Texture2D _sharedIdleAnim;
    static Texture2D _sharedMeleeAnim;
    static Texture2D _sharedRangedAnim;
    static Texture2D _sharedDashAnim;
    static Texture2D _sharedHurtAnim;
    static Texture2D _sharedDeathAnim;
    static Texture2D _sharedSpawnFireballTex;
    static Sound _sharedAttackSound;
    static Sound _sharedHurtSound;
    static Sound _sharedDeathSound;
    static Sound _sharedWallHitSound;
    static bool _sharedResourcesLoaded;
};
