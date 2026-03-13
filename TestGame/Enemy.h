#pragma once
#include "BaseCharacter.h"
#include "Character.h"
#include <vector>
#include <memory>

class Enemy : public BaseCharacter
{
public:
    Enemy(Vector2 pos);
    ~Enemy() override;

    void Update(float dt, Vector2 heroWorldPos, const std::vector<std::unique_ptr<Enemy>>& enemies);

    void SetTarget(Character* character) { _target = character; }
    void Init();
    void DrawEnemy(Vector2 heroWorldPos);
	void PlayAttackSound() override;
    void PlayDeathSound() override;
    void PlayHurtSound() override;

private:

    void HandleMovement(float dt, const std::vector<std::unique_ptr<Enemy>>& enemies);
    void HandleAttack();

	void HandleAnimation(float dt);
    void DrawHealthBar(Vector2 screenPos, float w, float h);

    Character* _target = nullptr;

    bool _attacking = false;
    bool _damageApplied = false;

    float _attackRange = 80.f;
    float _attackUpdateTime = 1.f / 8.f;

    float _attackCooldown = 0.f;
    float _attackDelay = 0.6f;

    Vector2 _homePos;


};