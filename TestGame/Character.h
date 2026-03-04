#pragma once
#include "BaseCharacter.h"

class Character : public BaseCharacter
{
public:
    Character();

    void Tick(float dt);

    void DealDamage(BaseCharacter& enemy);
    virtual void Death() override;



private:

    void HandleInput();
    void HandleMovement(float dt);
    void HandleAttack();
    void DrawPlayer();
    void HandleAnimation(float dt);

    Vector2 _direction{};

    bool _attacking = false;
    bool _damageApplied = false;

    float _attackUpdateTime = 1.f / 14.f;
};