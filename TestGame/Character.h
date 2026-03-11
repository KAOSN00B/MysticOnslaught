#pragma once
#include "BaseCharacter.h"

class Character : public BaseCharacter
{
public:
    Character();
    ~Character() override;

    void Init();
    void Update(float dt);

    void DealDamage(BaseCharacter& enemy);
    virtual void Death() override;

    void DrawPlayer();
    int GetHealth() const;

private:

    void HandleInput();
    void HandleMovement(float dt);
    void HandleAttack();
    
    void HandleAnimation(float dt);
    bool Dashing(float dt);

    Texture _dashAnim{};

    Vector2 _direction{};
    Vector2 _dashDirection{};

    bool _attacking = false;
    bool _damageApplied = false;
    bool _isDashing = false;
    bool _dashAnimPlaying = false;
    float _playDashParticles = false;

    float _attackUpdateTime = 1.f / 14.f;
    float _dashSpeed = 1500.f;
    float _dashDuration = 0.18f;
    float _dashTimer = 0.f;

    float _dashCooldown = 0.f;
    float _dashCooldownTime = 0.8f;
 

    int _dashFrame = 0;
    float _dashAnimTimer = 0.f;
    float _dashAnimSpeed = 1.f / 12.f;
    int _dashMaxFrames = 0;

    // put proper dash animations in
    
};