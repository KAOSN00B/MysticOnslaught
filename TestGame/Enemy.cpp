#include "Enemy.h"
#include "raymath.h"
#include <iostream>

Enemy::Enemy(Vector2 pos)
{
    _worldPos = pos;
    _homePos = pos;
}

Enemy::~Enemy()
{
    UnloadTexture(_idleAnim);
    UnloadTexture(_walkAnim);
    UnloadTexture(_attackAnim);
    UnloadTexture(_takeDamageAnim);
    UnloadTexture(_deathAnim);

    UnloadSound(_attackSound);
    UnloadSound(_hurtSound);
    UnloadSound(_deathSound);
}

void Enemy::Init()
{
    _idleAnim = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Enemy\\EnemyIdle.png");
    _walkAnim = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Enemy\\EnemyWalk.png");
    _attackAnim = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Enemy\\EnemyAttack.png");
    _takeDamageAnim = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Enemy\\EnemyDamage.png");
    _deathAnim = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Enemy\\EnemyDeath.png");

    _attackSound = LoadSound("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Sounds\\SwordSwipe2.wav");
    _hurtSound = LoadSound("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Sounds\\SmallMonsterDamage.wav");
    _deathSound = LoadSound("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Sounds\\PlayerDeath.wav");

    _texture = _idleAnim;

    _width = 32.f;
    _height = _texture.height;
    _scale = 6.f;
    _speed = 200.f;

    _health = 3.f;
    _maxHealth = 3.f;

    _attackPower = 1.f;

    _maxFrames = _texture.width / _width;
    _frame = GetRandomValue(0, _maxFrames - 1);
    _runningTime = GetRandomValue(0, 200) / 100.f * _updateTime;

}

void Enemy::Update(float dt, Vector2 heroWorldPos, const std::vector<std::unique_ptr<Enemy>>& enemies)
{
    UpdateDeath(dt);

    _worldPosLastFrame = _worldPos;

    ApplyVelocity(dt);
    UpdateHit(dt);

    if (!_dying)
    {
        if (_target == nullptr) return;

        _attackCooldown -= dt;

        HandleMovement(dt, enemies);
        HandleAttack();
    }

    HandleAnimation(dt);
    DrawEnemy(heroWorldPos);
}

void Enemy::HandleMovement(float dt, const std::vector<std::unique_ptr<Enemy>>& enemies)
{
    if (_target == nullptr) return;
    if (_dying) return;

    Vector2 playerPos = _target->GetWorldPos();
    Vector2 toPlayer = Vector2Subtract(playerPos, _worldPos);

    Vector2 moveDir = Vector2Zero();

    if (Vector2Length(toPlayer) > 0.01f)
        moveDir = Vector2Normalize(toPlayer);

    // --- separation force ---
    Vector2 separation = Vector2Zero();

    for (const auto& enemy : enemies)
    {
        if (enemy.get() == this) continue;

        float dist = Vector2Distance(_worldPos, enemy->_worldPos);

        if (dist < 60.f && dist > 0.f)
        {
            Vector2 away = Vector2Subtract(_worldPos, enemy->_worldPos);

            if (Vector2Length(away) > 0.01f)
            {
                float strength = (60.f - dist) / 60.f;
                separation = Vector2Add(separation,
                    Vector2Scale(Vector2Normalize(away), strength));
            }
        }
    }

    // reduce separation influence so enemies don't freeze
    separation = Vector2Scale(separation, 0.6f);

    moveDir = Vector2Add(moveDir, separation);

    if (Vector2Length(moveDir) > 0.01f)
        moveDir = Vector2Normalize(moveDir);

    // store previous position
    Vector2 oldPos = _worldPos;

    // move enemy (only if not attacking or taking damage)
    if (!_attacking && !_takingDamage)
    {
        _worldPos = Vector2Add(_worldPos, Vector2Scale(moveDir, _speed * dt));
    }

    Vector2 movement = Vector2Subtract(_worldPos, oldPos);

    if (Vector2Length(movement) > 0.01f)
    {
        _texture = _walkAnim;

        if (moveDir.x < 0) _rightLeft = -1;
        if (moveDir.x > 0) _rightLeft = 1;
    }
    else
    {
        _texture = _idleAnim;
    }
}

void Enemy::HandleAttack()
{
    if (_dying) return;
    if (_target == nullptr) return;

    Vector2 toTarget = Vector2Subtract(_target->GetWorldPos(), _worldPos);
    float distance = Vector2Length(toTarget);

    Vector2 toTargetDir{ 0.f, 0.f };
    if (distance > 0.01f) toTargetDir = Vector2Normalize(toTarget);

    Vector2 facingDir{ (float)_rightLeft, 0.f };
    float dot = Vector2DotProduct(facingDir, toTargetDir);

    if (distance <= _attackRange && dot > 0.4f && !_attacking && _attackCooldown <= 0.f)
    {
        _attacking = true;
        _damageApplied = false;

        _attackCooldown = _attackDelay;

        _texture = _attackAnim;
        _frame = 0;
        _runningTime = 0.f;

        _maxFrames = _texture.width / _width;
        _updateTime = _attackUpdateTime;

        PlayAttackSound();
    }

    if (_attacking && !_damageApplied && _frame == 2)
    {
        Rectangle attackRec = GetCollisionRec();
        attackRec.x += _rightLeft * 40;

        if (CheckCollisionRecs(attackRec, _target->GetCollisionRec()))
        {
            _target->TakeDamage(_attackPower, _worldPos);
            _damageApplied = true;
        }
    }

    if (!_target->IsAlive())
    {
        _attacking = false;
        _texture = _idleAnim;
        _updateTime = 1.f / 8.f;
        _maxFrames = _texture.width / _width;
        _frame = 0;
        _runningTime = 0.f;
        _speed = 0.f;
        return;
    }
}

void Enemy::DrawEnemy(Vector2 heroWorldPos)
{
    float w = _width * _scale;
    float h = _height * _scale;

    Vector2 screenPos = Vector2Subtract(_worldPos, heroWorldPos);
    screenPos.x += GetScreenWidth() / 2.f;
    screenPos.y += GetScreenHeight() / 2.f;

    Rectangle source{ _frame * _width, 0.f, _rightLeft * _width, _height };
    Rectangle dest{ screenPos.x - w / 2.f, screenPos.y - h / 2.f, w, h };

    DrawTexturePro(_texture, source, dest, Vector2{}, 0.f, WHITE);

    if (_health != _maxHealth)
        DrawHealthBar(screenPos, w, h);
}

void Enemy::HandleAnimation(float dt)
{
    _runningTime += dt;

    if (_runningTime >= _updateTime)
    {
        _runningTime = 0.f;
        _frame++;

        if (_frame >= _maxFrames)
        {
            if (_dying)
            {
                _frame = _maxFrames - 1;
                return;
            }

            if (_takingDamage)
            {
                _takingDamage = false;
                _texture = _idleAnim;
                _updateTime = 1.f / 10.f;
                _maxFrames = _texture.width / _width;
                _frame = 0;
                return;
            }

            if (_attacking)
            {
                _attacking = false;
                _texture = _idleAnim;
                _updateTime = 1.f / 10.f;
                _maxFrames = _texture.width / _width;
            }

            _frame = 0;
        }
    }
}

void Enemy::PlayAttackSound()
{
    float pitch = GetRandomValue(100, 140) / 100.f;
    SetSoundPitch(_attackSound, pitch);
    SetSoundVolume(_attackSound, 0.5f);
    PlaySound(_attackSound);
}

void Enemy::DrawHealthBar(Vector2 screenPos, float w, float h)
{
    if (_health <= 0) return;

    float healthPercent = (float)_health / (float)_maxHealth;

    float barWidth = w * 0.8f;
    float barHeight = 6.f;

    float barX = screenPos.x - barWidth / 2.f;
    float barY = screenPos.y - h / 2.f - 12.f;

    DrawRectangle(barX, barY, barWidth, barHeight, RED);
    DrawRectangle(barX, barY, barWidth * healthPercent, barHeight, GREEN);
}

void Enemy::PlayDeathSound()
{
    float pitch = GetRandomValue(140, 180) / 100.f;
    SetSoundPitch(_deathSound, pitch);
    SetSoundVolume(_deathSound, 0.5f);
    PlaySound(_deathSound);
}

void Enemy::PlayHurtSound()
{
    float pitch = GetRandomValue(140, 180) / 100.f;
    SetSoundPitch(_hurtSound, pitch);
    SetSoundVolume(_hurtSound, 0.5f);
    PlaySound(_hurtSound);
}