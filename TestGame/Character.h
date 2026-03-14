#pragma once
#include "BaseCharacter.h"

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
    virtual void Death() override;

    void DrawPlayer();
    void DashParticles(float h);
    void SetWorldPos(Vector2 pos);
    void PlayHurtSound() override;

    void AddFireballAmmo(int amount);
    int  GetFireballAmmo()      const;
    void AddSwordBeamAmmo(int amount);
    int  GetSwordBeamAmmo()     const;
    void AddFreezeAmmo(int amount);
    int  GetFreezeAmmo()        const;
    int  GetSelectedAbility()   const { return _selectedAbility; }
    CastType ConsumeCastRequest();
    bool CanApplyMeleeDamage() const;
    void ConsumeMeleeDamageFrame();
    Rectangle GetAttackCollisionRec() const;
    Vector2 GetCastOrigin() const;
    Vector2 GetFacingDirection() const;

    void AddExp(int amount);
    void Heal(int amount);
    int  GetLevel()     const { return _level; }
    int  GetExp()       const { return _exp; }
    int  GetExpToNext() const { return _expToNextLevel; }
    

private:

    void HandleInput();
    void HandleMovement(float dt);
    void HandleAttackInput();
    
    void HandleAnimation(float dt);
    bool Dashing(float dt);

    Texture _dashAnim{};
    Texture _staffAnim{};

    Vector2 _direction{};
    Vector2 _dashDirection{};

    bool _attacking = false;
    bool _damageApplied = false;
    bool _castingAbility = false;
    bool _isDashing = false;
    bool _dashAnimPlaying = false;
    bool _playDashParticles = false;
    bool _dashInvincible = false;
    int  _selectedAbility   = 0;   // 0=Fireball 1=SwordBeam 2=Freeze 3=(future)

    float _attackUpdateTime = 1.f / 14.f;
    float _staffCastUpdateTime = 1.f / 12.f;
    float _dashSpeed = 1500.f;
    float _dashDuration = 0.18f;
    float _dashTimer = 0.f;

    float _dashCooldown = 0.f;
    float _dashCooldownTime = 0.8f;
    float _invincibleTimer = 0.f;
    float _invincibleDuration = 0.4f;

    int _fireballAmmo  = 0;
    int _swordBeamAmmo = 0;
    int _freezeAmmo    = 0;
    CastType _queuedCast = CastType::None;

    int _exp            = 0;
    int _level          = 0;
    int _expToNextLevel = 10;
    static constexpr int _maxLevel = 10;

};
