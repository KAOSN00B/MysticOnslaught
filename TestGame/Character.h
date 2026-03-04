#pragma once
#include "BaseCharacter.h"

class Character : public BaseCharacter
{
public:
    Character();

    void Init();
    void Update(float dt);

    void DealDamage(BaseCharacter& enemy);
    virtual void Death() override;
    void DrawPlayer();

private:

    void HandleInput();
    void HandleMovement(float dt);
    void HandleAttack();
    
    void HandleAnimation(float dt);

    Vector2 _direction{};

    bool _attacking = false;
    bool _damageApplied = false;

    float _attackUpdateTime = 1.f / 14.f;
};