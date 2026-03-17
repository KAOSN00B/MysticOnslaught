#pragma once
#include "raylib.h"

class BaseCharacter
{
public:
    BaseCharacter();
    virtual ~BaseCharacter() = default;

    virtual Rectangle GetCollisionRec() const;

    virtual void TakeDamage(int damage, Vector2 attackerPos);
    virtual void Death();

    int GetHealth() const;
    float GetHealthValue() const { return _health; }


    void ApplyVelocity(float dt);
    void UndoMovement();
    void UpdateHit(float dt);
	virtual void PlayFootStepSound();
	virtual void PlayAttackSound();
    virtual void PlayDeathSound();
    virtual void PlayHurtSound();

    bool UpdateDeath(float dt);
    bool IsAlive() const { return _health > 0; }

    Vector2 GetWorldPos() const { return _worldPos; }

protected:

    void Draw(Vector2 screenPos);

    Texture2D _texture{};
    Texture2D _idleAnim{};
    Texture2D _walkAnim{};
    Texture2D _attackAnim{};
    Texture2D _takeDamageAnim{};
    Texture2D _deathAnim{};

    Sound _footStepSound{};
    Sound _hurtSound{};
    Sound _deathSound{};
    Sound _attackSound{};


    Vector2 _worldPos{};
    Vector2 _worldPosLastFrame{};
    Vector2 _velocity{};

    float _runningTime{};
    float _updateTime = 1.f / 8.f;

    int _frame{};
    int _maxFrames{};

    float _width{};
    float _height{};
    float _scale = 4.f;

    float _rightLeft = 1.f;
    float _speed = 150.f;
    float _stepTimer = 0.f;
    float _stepDelay = 0.35f; // time between steps

    float _health = 3.f;
    float _maxHealth = 5.f;
    float _attackPower = 1.f;
    bool _takingDamage = false;
    bool _attacking = false;
    float _hitTimer = 0.f;
    bool _dying = false;
    bool _hasIFrames = false;
    float _deathTimer = 0.4f;

};
