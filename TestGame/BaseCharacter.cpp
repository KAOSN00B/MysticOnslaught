#include "BaseCharacter.h"
#include "raymath.h"

BaseCharacter::BaseCharacter()
{
	_worldPos = Vector2Zero();
	_worldPosLastFrame = Vector2Zero();
}

void BaseCharacter::EnsureCollisionShape()
{
    GetCollisionRec(); // triggers lazy-init as a side-effect
}

Rectangle BaseCharacter::GetCollisionRec() const
{
    if (_collisionSize.x == 0.f && _width > 0.f)
    {
        auto* s = const_cast<BaseCharacter*>(this);
        s->_collisionSize   = { 89.20f, 103.76f };
        s->_collisionOffset = { 4.00f, 49.60f };
    }
    return Rectangle{
        _worldPos.x - _collisionSize.x * 0.5f + _collisionOffset.x,
        _worldPos.y - _collisionSize.y * 0.5f + _collisionOffset.y,
        _collisionSize.x, _collisionSize.y
    };
}

void BaseCharacter::TakeDamage(int damage, Vector2 attackerPos)
{
	if (_dying) return;

	_health -= damage;

	if (_health > 0)
	{
		PlayHurtSound();
	}

	if (_health <= 0)
	{
		_health = 0;
		_dying = true;

		_attacking = false;
		_takingDamage = false;

		_deathTimer = 0.4f;

		_texture = _deathAnim;
		_frame = 0;
		_runningTime = 0.f;

		_maxFrames = _texture.width / _width;
		_updateTime = 1.f / 4.f;

		PlayDeathSound();
		return;
	}

	_attacking = false;

	_takingDamage = true;
	_hitTimer = 0.18f;

	_texture = _takeDamageAnim;
	_frame = 0;
	_runningTime = 0.f;

	_maxFrames = _texture.width / _width;
	_updateTime = 1.f / 12.f;

	Vector2 direction = Vector2Subtract(_worldPos, attackerPos);

	if (Vector2Length(direction) > 0)
		direction = Vector2Normalize(direction);

	_velocity = Vector2Scale(direction, 2000.f);
}

void BaseCharacter::ApplyVelocity(float dt)
{
	_worldPos = Vector2Add(_worldPos, Vector2Scale(_velocity, dt));

	_velocity = Vector2Scale(_velocity, 0.70f);

	if (Vector2Length(_velocity) < 5.f)
		_velocity = Vector2Zero();
}

bool BaseCharacter::UpdateDeath(float dt)
{
	if (!_dying) return false;

	_deathTimer -= dt;

	if (_deathTimer <= 0.f)
	{
		Death();
		return true;
	}

	return false;
}

int BaseCharacter::GetHealth() const
{
	return _health;
}

void BaseCharacter::Death()
{
	_worldPos = Vector2{ -1000.f, -1000.f };
}

void BaseCharacter::Draw(Vector2 screenPos)
{
	float w = _width * _scale;
	float h = _height * _scale;

	Rectangle source{ _frame * _width, 0.f, _rightLeft * _width, _height };

	Rectangle dest{ screenPos.x - w / 2.f, screenPos.y - h / 2.f, w, h };

	DrawTexturePro(_texture, source, dest, Vector2{}, 0.f, WHITE);
}

void BaseCharacter::UndoMovement()
{
	_worldPos = _worldPosLastFrame;
}

void BaseCharacter::UpdateHit(float dt)
{
	if (!_takingDamage) return;

	_hitTimer -= dt;

	if (_hitTimer <= 0.f)
	{
		_takingDamage = false;
	}
}

void BaseCharacter::PlayFootStepSound()
{
	float pitch = GetRandomValue(90, 120) / 100.f;
	SetSoundPitch(_footStepSound, pitch);
	SetSoundVolume(_footStepSound, 0.15f);
	PlaySound(_footStepSound);
}

void BaseCharacter::PlayAttackSound()
{
	float pitch = GetRandomValue(40, 70) / 100.f;
	SetSoundPitch(_attackSound, pitch);
	SetSoundVolume(_attackSound, 0.85f);
	PlaySound(_attackSound);
}

void BaseCharacter::PlayHurtSound()
{
	float pitch = GetRandomValue(40, 70) / 100.f;
	SetSoundPitch(_hurtSound, pitch);
	SetSoundVolume(_hurtSound, 0.15f);
	PlaySound(_hurtSound);
}

void BaseCharacter::PlayDeathSound()
{
	PlaySound(_deathSound);
}
