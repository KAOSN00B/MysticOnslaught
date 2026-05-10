#pragma once
#include "raylib.h"
#include "CapsuleCollision.h"

class BaseCharacter
{
public:
    BaseCharacter();
    virtual ~BaseCharacter() = default;

    virtual Rectangle GetCollisionRec() const;

    virtual void TakeDamage(int damage, Vector2 attackerPos);
    virtual void Death();

    int GetHealth() const;
    float GetHealthValue()    const { return _health; }
    float GetMaxHealthValue() const { return _maxHealth; }


    void ApplyVelocity(float dt);
    void UndoMovement();
    void UpdateHit(float dt);
	virtual void PlayFootStepSound();
	virtual void PlayAttackSound();
    virtual void PlayDeathSound();
    virtual void PlayHurtSound();

    bool UpdateDeath(float dt);
    bool IsAlive() const { return _health > 0; }

    Vector2          GetWorldPos()    const { return _worldPos; }
    const Texture2D& GetIdleAnim()   const { return _idleAnim; }
    float            GetSpriteWidth() const { return _width; }

    // Collision shape — readable/writable so the debug hitbox editor can nudge
    // them at runtime and print the result. Both default to {0,0}; GetCollisionRec
    // lazy-initialises them from the original ratios on the first call after
    // the sprite is loaded (_width > 0). Setting a non-zero _collisionSize in
    // a constructor skips the lazy-init, letting you bake in exported values.
    void    EnsureCollisionShape();
    Vector2 GetCollisionOffset() const { return _collisionOffset; }
    Vector2 GetCollisionSize()   const { return _collisionSize;   }
    void    SetCollisionOffset(Vector2 v) { _collisionOffset = v; }
    void    SetCollisionSize(Vector2 v)   { _collisionSize   = v; }

    // Capsule collider — used for body-body and body-prop collision
    virtual Capsule2D GetCapsule() const;
    float   GetCapsuleRadius()     const { return _capsuleRadius;     }
    float   GetCapsuleHalfHeight() const { return _capsuleHalfHeight; }
    Vector2 GetCapsuleOffset()     const { return _capsuleOffset;     }
    void    SetCapsuleRadius(float v)     { _capsuleRadius     = std::max(4.f, v);  }
    void    SetCapsuleHalfHeight(float v) { _capsuleHalfHeight = std::max(0.f, v);  }
    void    SetCapsuleOffset(Vector2 v)   { _capsuleOffset     = v;                 }

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


    Vector2 _collisionOffset{};   // pixel offset from _worldPos to rect centre (x usually 0)
    Vector2 _collisionSize{};     // pixel width/height; {0,0} = lazy-init pending

    float   _capsuleRadius     = 0.f;  // half-width of capsule; 0 = uninitialised
    float   _capsuleHalfHeight = 0.f;  // half of straight section; 0 = pure circle
    Vector2 _capsuleOffset     = {};   // world-space offset of capsule centre from _worldPos

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
