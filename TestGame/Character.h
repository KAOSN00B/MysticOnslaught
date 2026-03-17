#pragma once
#include "BaseCharacter.h"
#include <vector>

class Character : public BaseCharacter
{
public:
    enum class CastType
    {
        None,
        Fireball,
        SwordBeam,
        Freeze
    };

    Character();
    ~Character() override;

    void Init();
    void Update(float dt);

    void TakeDamage(int damage, Vector2 attackerPos) override;
    void TakeFractionalDamage(float damage, Vector2 attackerPos);
    virtual void Death() override;

    void DrawPlayer(Vector2 cameraPos);
    void DashParticles(float h, Vector2 playerScreenCenter);
    void SetWorldPos(Vector2 pos);
    void PlayHurtSound() override;
    void StartForcedPush(Vector2 direction, float speed);
    void OnForcedPushCollision();
    void ApplyBurnTicks(float tickDelay, int tickCount, float damagePerTick, Vector2 sourcePos);
    void GrantInvulnerability(float duration);
    bool IsBeingForcedPushed() const { return _forcedPushActive; }
    bool IsForceLocked() const { return _forcedPushActive || _forcedPushStunTimer > 0.f; }

    void AddFireballAmmo(int amount);
    int  GetFireballAmmo() const;
    void AddSwordBeamAmmo(int amount);
    int  GetSwordBeamAmmo() const;
    void AddFreezeAmmo(int amount);
    int  GetFreezeAmmo() const;
    int  GetSelectedAbility() const { return _selectedAbility; }
    CastType ConsumeCastRequest();
    bool CanApplyMeleeDamage() const;
    void ConsumeMeleeDamageFrame();
    Rectangle GetAttackCollisionRec() const;
    Vector2 GetCastOrigin() const;
    Vector2 GetFacingDirection() const;
    Vector2 GetFeetWorldPos() const;
    int ConsumeHealEffectRequests();
    float GetDashCooldownPercent() const { return (_dashCooldownTime > 0.f) ? 1.f - (_dashCooldown / _dashCooldownTime) : 1.f; }

    void AddExp(int amount);
    void Heal(int amount);
    int  GetLevel() const { return _level; }
    int  GetExp() const { return _exp; }
    int  GetExpToNext() const { return _expToNextLevel; }

    // Combat damage accessors -------------------------------------------------
    // Melee scales through _attackPower from level-ups, so it already has a
    // strong progression path. Special abilities are burst / piercing tools,
    // so they only receive a small multiplier-based increase over the run.
    int   GetMeleeDamage() const { return (int)_attackPower; }
    int   GetSpecialDamageBonus() const;
    int   GetFireballHitDamage() const;
    int   GetFireballBurnDamage() const;
    int   GetSwordBeamDamage() const;
    int   GetFreezeDamage() const;

private:
    void HandleInput();
    void HandleMovement(float dt);
    void HandleAttackInput();
    bool HandleForcedPush(float dt);
    void UpdatePendingBurns(float dt);
    void ApplyBurnTickDamage(float damage, Vector2 sourcePos);

    void HandleAnimation(float dt);
    bool Dashing(float dt);

    Texture _dashAnim{};
    Texture _staffAnim{};

    Vector2 _direction{};
    Vector2 _dashDirection{};
    Vector2 _forcedPushDirection{};

    bool _attacking = false;
    bool _damageApplied = false;
    bool _castingAbility = false;
    bool _isDashing = false;
    bool _dashAnimPlaying = false;
    bool _playDashParticles = false;
    bool _dashInvincible = false;
    bool _forcedPushActive = false;
    int  _selectedAbility = 0;   // 0=Fireball 1=SwordBeam 2=Freeze 3=(future)

    float _attackUpdateTime = 1.f / 14.f;
    float _staffCastUpdateTime = 1.f / 12.f;
    float _dashSpeed = 1500.f;
    float _dashDuration = 0.18f;
    float _dashTimer = 0.f;

    float _dashCooldown = 0.f;
    float _dashCooldownTime = 1.3f;
    float _invincibleTimer = 0.f;
    float _invincibleDuration = 0.4f;
    float _forcedPushSpeed = 0.f;
    float _forcedPushStunTimer = 0.f;
    float _forcedPushImpactStunDuration = 0.35f;

    int _fireballAmmo = 0;
    int _swordBeamAmmo = 0;
    int _freezeAmmo = 0;
    CastType _queuedCast = CastType::None;
    int _pendingHealEffects = 0;

    int _exp = 0;
    int _level = 0;
    int _expToNextLevel = 10;
    static constexpr int _maxLevel = 10;

    // Fractional player burn is kept separate from the normal integer hit path
    // because lavaball damage arrives as two small delayed ticks instead of one
    // large melee-style hit. Each entry counts down independently so multiple
    // projectiles can stack cleanly.
    struct PendingBurnTick
    {
        float timer = 0.f;
        float damage = 0.f;
        Vector2 sourcePos{};
    };
    std::vector<PendingBurnTick> _pendingBurnTicks;

    // Base ability damage values ---------------------------------------------
    // Special abilities use small flat base values so level progression can add
    // readable integer bonuses instead of percentage math.
    static constexpr float _fireballBaseDamage = 1.0f;
    static constexpr float _fireballBurnBaseDamage = 1.0f;
    static constexpr float _swordBeamBaseDamage = 2.0f;
    static constexpr float _freezeBaseDamage = 1.0f;
};
