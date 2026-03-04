#pragma once
#include "raylib.h"

class BaseCharacter
{
public:
    BaseCharacter();

    Rectangle GetCollisionRec() const;

    virtual void TakeDamage(int damage, Vector2 attackerPos);
    virtual void Death();

    void ApplyVelocity(float dt);
    void UpdateDeath(float dt);
    void UndoMovement();
    void UpdateHit(float dt);

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

    int _health = 3;
    bool _takingDamage = false;
    bool _attacking = false;
    float _hitTimer = 0.f;
    bool _dying = false;
    float _deathTimer = 0.4f;

};