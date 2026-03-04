#include "Enemy.h"
#include "raymath.h"

Enemy::Enemy(Vector2 pos)
{
    _worldPos = pos;

    _idleAnim = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Enemy\\EnemyIdle.png");
    _walkAnim = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Enemy\\EnemyWalk.png");
    _attackAnim = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Enemy\\EnemyAttack.png");
	_takeDamageAnim = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Enemy\\EnemyDamage.png");
    _deathAnim = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Enemy\\EnemyDeath.png");

    _texture = _idleAnim;

    _width = 32.f;
    _height = _texture.height;
    _health = 5;
    _scale = 6.f;
    _speed = 200.f;
    _worldPos = pos;
    _homePos = pos;

    _maxFrames = _texture.width / _width;
}

void Enemy::Update(float dt, Vector2 heroWorldPos)
{
    UpdateDeath(dt);

    _worldPosLastFrame = _worldPos;

    ApplyVelocity(dt);
    UpdateHit(dt);

    if (!_dying)
    {
        if (_target == nullptr)
            return;

        _attackCooldown -= dt;

        HandleMovement(dt);
        HandleAttack();
    }

    HandleAnimation(dt);
    DrawEnemy(heroWorldPos);
}

void Enemy::HandleMovement(float dt)
{
    if (_attacking || _takingDamage || _dying) return;

    Vector2 playerPos = _target->GetWorldPos();
    Vector2 toPlayer = Vector2Subtract(playerPos, _worldPos);
    float playerDistance = Vector2Length(toPlayer);

    Vector2 moveDir{};

	// Detect player within chase range
    if (!_hasTarget && playerDistance <= _findPlayerRange)
    {
        _hasTarget = true;
	}

	// If we have a target, but they go out of chase range, lose them
    if (_hasTarget && playerDistance > _chaseRange)
    {
        _hasTarget = false;
	}

    if (_hasTarget)
    {
		moveDir = Vector2Normalize(toPlayer);
    }
    else
    {
        // Return home
        Vector2 toHome = Vector2Subtract(_homePos, _worldPos);
        float homeDistance = Vector2Length(toHome);
        if (homeDistance < 5.f) // if too close to home will just stay where it is
        {
            _texture = _idleAnim;
            return;
        }
		moveDir = Vector2Normalize(toHome);
    }

    // Apply movement (this was missing)
    _worldPos = Vector2Add(_worldPos, Vector2Scale(moveDir, _speed * dt));

    _texture = _walkAnim;

    if (moveDir.x < 0) _rightLeft = -1;
    if (moveDir.x > 0) _rightLeft = 1;
}

void Enemy::HandleAttack()
{
    if (_dying) return;   // ← important

    Vector2 toTarget = Vector2Subtract(_target->GetWorldPos(), _worldPos);
    float distance = Vector2Length(toTarget);

    Vector2 toTargetDir = Vector2Normalize(toTarget);
    Vector2 facingDir = { _rightLeft, 0.f };

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
    }

    if (_attacking && !_damageApplied && _frame == 2)
    {
        if (CheckCollisionRecs(GetCollisionRec(), _target->GetCollisionRec()))
        {
            _target->TakeDamage(1, _worldPos);
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

    Rectangle dest{ screenPos.x - w / 2.f, screenPos.y - h / 2.f, w, h};

    DrawTexturePro(_texture, source, dest, Vector2{}, 0.f, WHITE);
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
            // DEATH animation finishes
            if (_dying)
            {
                _frame = _maxFrames - 1; // stay on last frame
                return;
            }

            // ATTACK animation finishes
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
