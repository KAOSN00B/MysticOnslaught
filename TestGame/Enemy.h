#pragma once
#include "BaseCharacter.h"
#include "Character.h"
#include "raymath.h"
#include <vector>
#include <memory>

class Cyclops;
class Ogre;
class Molarbeast;

class Enemy : public BaseCharacter
{
public:
    Enemy(Vector2 pos);
    ~Enemy() override;
    static void UnloadSharedResources();

    // Main enemy tick. Engine calls this for every active enemy each frame.
    // Derived enemy types can override it when they need custom combat logic,
    // but they still participate in the same pooled enemy vector.
    virtual void Update(float dt, Vector2 heroWorldPos, Vector2 navigationTarget, bool hasNavigationTarget,
        const std::vector<std::unique_ptr<Enemy>>& enemies, const std::vector<Vector2>& propCenters);
    virtual void SetWaveScale(int wave);
    virtual void ApplyEnemyPowerLevel(int enemyPowerLevel);

    void SetTarget(Character* character) { _target = character; }
    void Init();
    virtual void ResetForSpawn(Vector2 pos);
    virtual void DrawEnemy(Vector2 heroWorldPos);
    virtual void ApplyBurn(float delay, int damage, Vector2 sourcePos);
    virtual void ApplyFreeze(float duration);
    virtual void ApplyExternalImpulse(Vector2 impulse, bool cancelLockedAnimation);
    virtual bool IsFrozen()     const { return _freezeTimer > 0.f; }
    virtual int  GetExpValue()  const { return _expValue; }
    virtual int  GetAttackPower() const { return (int)_attackPower; }
    bool IsDying()      const { return _dying; }
    virtual bool IsActive()     const { return _isActive; }
    virtual void SetActive(bool active) { _isActive = active; }
    virtual void Teleport(Vector2 pos) { _worldPos = pos; _worldPosLastFrame = pos; _velocity = Vector2Zero(); }
    virtual Cyclops* AsCyclops() { return nullptr; }
    virtual Ogre* AsOgre() { return nullptr; }
    virtual Molarbeast* AsMolarbeast() { return nullptr; }
    virtual bool UsesDirectPursuit() const { return false; }
    virtual bool IgnoresPropCollisions() const { return false; }
    virtual bool IsBoss() const { return false; }
	void PlayAttackSound() override;
    void PlayDeathSound() override;
    void PlayHurtSound() override;

protected:
    static void EnsureSharedResourcesLoaded();

    void HandleMovement(float dt, Vector2 navigationTarget, bool hasNavigationTarget,
        const std::vector<std::unique_ptr<Enemy>>& enemies, const std::vector<Vector2>& propCenters);
    void HandleAttack();
    void UpdateBurns(float dt);
    void UpdateLaunchVisual(float dt);

	void HandleAnimation(float dt);
    void DrawHealthBar(Vector2 screenPos, float w, float h);

    struct PendingBurn
    {
        float timer = 0.f;
        int damage = 0;
        Vector2 sourcePos{};
    };

    Character* _target = nullptr;
    bool _isActive = true;

    bool  _attacking    = false;
    bool  _damageApplied = false;

    float   _freezeTimer    = 0.f;
    int     _expValue       = 1;

    // Stuck detection — if barely moved for _stuckThreshold seconds, apply a kick
    float   _stuckTimer     = 0.f;
    Vector2 _stuckCheckPos  = {};
    static constexpr float _stuckThreshold  = 0.8f;
    static constexpr float _stuckMinMove    = 8.f;   // pixels needed to not be considered stuck

    float _attackRange = 80.f;
    float _attackUpdateTime = 1.f / 8.f;

    float _attackCooldown = 0.f;
    float _attackDelay = 0.6f;
    float _launchVisualTimer = 0.f;
    bool _launchHoldingHurtPose = false;
    static constexpr float _launchVisualDuration = 0.22f;
    static constexpr float _launchVisualScaleBoost = 0.18f;
    static constexpr float _launchVisualLift = 18.f;

    // Flank tuning: regular enemies should not all aim for the exact same
    // player point. Each one picks a small left/right lane and tries to arc
    // into that lane once it gets near the player.
    float _flankSide = 1.f;
    float _flankDistance = 80.f;
    static constexpr float _flankStartDistance = 220.f;
    static constexpr float _flankBlendStrength = 0.65f;
    static constexpr float _minFlankDistance = 55.f;
    static constexpr float _maxFlankDistance = 110.f;
    static constexpr float _propSlideStrength = 0.75f;

    Vector2 _homePos;
    std::vector<PendingBurn> _pendingBurns;

    static Texture2D _sharedIdleAnim;
    static Texture2D _sharedWalkAnim;
    static Texture2D _sharedAttackAnim;
    static Texture2D _sharedTakeDamageAnim;
    static Texture2D _sharedDeathAnim;
    static Sound _sharedAttackSound;
    static Sound _sharedHurtSound;
    static Sound _sharedDeathSound;
    static bool _sharedResourcesLoaded;


};
