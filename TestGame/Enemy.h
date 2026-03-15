#pragma once
#include "BaseCharacter.h"
#include "Character.h"
#include "raymath.h"
#include <vector>
#include <memory>

class Enemy : public BaseCharacter
{
public:
    Enemy(Vector2 pos);
    ~Enemy() override;
    static void UnloadSharedResources();

    void Update(float dt, Vector2 heroWorldPos, Vector2 navigationTarget, bool hasNavigationTarget,
        const std::vector<std::unique_ptr<Enemy>>& enemies, const std::vector<Vector2>& propCenters);
    void SetWaveScale(int wave);

    void SetTarget(Character* character) { _target = character; }
    void Init();
    void ResetForSpawn(Vector2 pos);
    void DrawEnemy(Vector2 heroWorldPos);
    void ApplyBurn(float delay, int damage, Vector2 sourcePos);
    void ApplyFreeze(float duration);
    bool IsFrozen()     const { return _freezeTimer > 0.f; }
    int  GetExpValue()  const { return _expValue; }
    bool IsDying()      const { return _dying; }
    bool IsActive()     const { return _isActive; }
    void SetActive(bool active) { _isActive = active; }
    void Teleport(Vector2 pos) { _worldPos = pos; _worldPosLastFrame = pos; _velocity = Vector2Zero(); }
	void PlayAttackSound() override;
    void PlayDeathSound() override;
    void PlayHurtSound() override;

private:
    static void EnsureSharedResourcesLoaded();

    void HandleMovement(float dt, Vector2 navigationTarget, bool hasNavigationTarget,
        const std::vector<std::unique_ptr<Enemy>>& enemies, const std::vector<Vector2>& propCenters);
    void HandleAttack();
    void UpdateBurns(float dt);

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
