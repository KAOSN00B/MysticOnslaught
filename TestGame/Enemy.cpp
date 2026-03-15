#include "Enemy.h"

#include "raymath.h"

Texture2D Enemy::_sharedIdleAnim{};
Texture2D Enemy::_sharedWalkAnim{};
Texture2D Enemy::_sharedAttackAnim{};
Texture2D Enemy::_sharedTakeDamageAnim{};
Texture2D Enemy::_sharedDeathAnim{};
Sound Enemy::_sharedAttackSound{};
Sound Enemy::_sharedHurtSound{};
Sound Enemy::_sharedDeathSound{};
bool Enemy::_sharedResourcesLoaded = false;

Enemy::Enemy(Vector2 pos)
{
    _worldPos = pos;
    _homePos = pos;
}

Enemy::~Enemy()
{
}

void Enemy::Init()
{
    EnsureSharedResourcesLoaded();

    _idleAnim = _sharedIdleAnim;
    _walkAnim = _sharedWalkAnim;
    _attackAnim = _sharedAttackAnim;
    _takeDamageAnim = _sharedTakeDamageAnim;
    _deathAnim = _sharedDeathAnim;
    _attackSound = _sharedAttackSound;
    _hurtSound = _sharedHurtSound;
    _deathSound = _sharedDeathSound;

    ResetForSpawn(_worldPos);
}

void Enemy::ResetForSpawn(Vector2 pos)
{
    _worldPos = pos;
    _worldPosLastFrame = pos;
    _homePos = pos;
    _velocity = Vector2Zero();
    _isActive = true;
    _texture = _idleAnim;
    _updateTime = 1.f / 8.f;

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
    _hitTimer = 0.f;
    _deathTimer = 0.4f;
    _freezeTimer = 0.f;
    _attacking = false;
    _damageApplied = false;
    _takingDamage = false;
    _dying = false;
    _pendingBurns.clear();
    _stuckTimer    = 0.f;
    _stuckCheckPos = _worldPos;
}

void Enemy::Update(float dt, Vector2 heroWorldPos, Vector2 navigationTarget, bool hasNavigationTarget,
    const std::vector<std::unique_ptr<Enemy>>& enemies, const std::vector<Vector2>& propCenters)
{
    if (!_isActive)
        return;

    // UpdateDeath is intentionally NOT called here.
    // It is called once per frame in Engine::UpdateEnemyCount so the
    // drop world position can be captured before Death() teleports the enemy.

    _worldPosLastFrame = _worldPos;

    ApplyVelocity(dt);
    UpdateHit(dt);
    UpdateBurns(dt);

    if (_freezeTimer > 0.f)
        _freezeTimer -= dt;

    if (!_dying)
    {
        if (_target == nullptr)
            return;

        _attackCooldown -= dt;

        HandleMovement(dt, navigationTarget, hasNavigationTarget, enemies, propCenters);
        HandleAttack();
    }

    HandleAnimation(dt);
}

void Enemy::HandleMovement(float dt, Vector2 navigationTarget, bool hasNavigationTarget,
    const std::vector<std::unique_ptr<Enemy>>& enemies, const std::vector<Vector2>& propCenters)
{
    if (_target == nullptr || _dying)
        return;

    Vector2 targetPos = hasNavigationTarget ? navigationTarget : _target->GetWorldPos();
    Vector2 toPlayer = Vector2Subtract(targetPos, _worldPos);

    Vector2 moveDir = Vector2Zero();

    if (Vector2Length(toPlayer) > 0.01f)
        moveDir = Vector2Normalize(toPlayer);

    Vector2 separation = Vector2Zero();

    for (const auto& enemy : enemies)
    {
        if (enemy.get() == this)
            continue;
        if (!enemy->IsActive() || enemy->IsDying() || !enemy->IsAlive())
            continue;

        float dist = Vector2Distance(_worldPos, enemy->_worldPos);

        if (dist < 60.f && dist > 0.f)
        {
            Vector2 away = Vector2Subtract(_worldPos, enemy->_worldPos);

            if (Vector2Length(away) > 0.01f)
            {
                float strength = (60.f - dist) / 60.f;
                separation = Vector2Add(separation, Vector2Scale(Vector2Normalize(away), strength));
            }
        }
    }

    // Prop repulsion — steer away from nearby pillars
    for (const Vector2& propCenter : propCenters)
    {
        float dist = Vector2Distance(_worldPos, propCenter);
        if (dist < 110.f && dist > 0.f)
        {
            Vector2 away = Vector2Subtract(_worldPos, propCenter);
            if (Vector2Length(away) > 0.01f)
            {
                float strength = (110.f - dist) / 110.f;
                separation = Vector2Add(separation, Vector2Scale(Vector2Normalize(away), strength * 1.8f));
            }
        }
    }

    separation = Vector2Scale(separation, 0.6f);
    moveDir = Vector2Add(moveDir, separation);

    if (Vector2Length(moveDir) > 0.01f)
        moveDir = Vector2Normalize(moveDir);

    Vector2 oldPos = _worldPos;

    if (!_attacking && !_takingDamage && !IsFrozen())
        _worldPos = Vector2Add(_worldPos, Vector2Scale(moveDir, _speed * dt));

    // Stuck detection — if not moving enough over time, kick sideways to break deadlock
    if (!_attacking && !_takingDamage && !IsFrozen())
    {
        _stuckTimer += dt;

        if (_stuckTimer >= _stuckThreshold)
        {
            float moved = Vector2Distance(_worldPos, _stuckCheckPos);

            if (moved < _stuckMinMove)
            {
                // Apply a random perpendicular impulse
                Vector2 perp = { -moveDir.y, moveDir.x };
                float sign = (GetRandomValue(0, 1) == 0) ? 1.f : -1.f;
                _velocity = Vector2Add(_velocity,
                    Vector2Scale(perp, sign * _speed * 1.2f));
            }

            _stuckTimer    = 0.f;
            _stuckCheckPos = _worldPos;
        }
    }
    else
    {
        // Reset timer whenever frozen/attacking so the check stays meaningful
        _stuckTimer    = 0.f;
        _stuckCheckPos = _worldPos;
    }

    Vector2 movement = Vector2Subtract(_worldPos, oldPos);

    // Only update texture when not in a locked animation state
    if (!_attacking && !_takingDamage)
    {
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
}

void Enemy::HandleAttack()
{
    if (_dying || _target == nullptr || IsFrozen())
        return;

    Vector2 toTarget = Vector2Subtract(_target->GetWorldPos(), _worldPos);
    float distance = Vector2Length(toTarget);

    Vector2 toTargetDir{ 0.f, 0.f };
    if (distance > 0.01f)
        toTargetDir = Vector2Normalize(toTarget);

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
        attackRec.x += _rightLeft * 28.f;
        // Shrink height and vertically center so enemies can't hit too high or too low
        float trim = attackRec.height * 0.30f;
        attackRec.y      += trim;
        attackRec.height -= trim * 2.f;

        if (CheckCollisionRecs(attackRec, _target->GetCollisionRec()))
        {
            _target->TakeDamage((int)_attackPower, _worldPos);
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
    }
}

void Enemy::DrawEnemy(Vector2 heroWorldPos)
{
    if (!_isActive)
        return;

    float w = _width * _scale;
    float h = _height * _scale;

    Vector2 screenPos = Vector2Subtract(_worldPos, heroWorldPos);
    screenPos.x += GetScreenWidth() / 2.f;
    screenPos.y += GetScreenHeight() / 2.f;

    Rectangle source{ _frame * _width, 0.f, _rightLeft * _width, _height };
    Rectangle dest{ screenPos.x - w / 2.f, screenPos.y - h / 2.f, w, h };

    bool burning = !_pendingBurns.empty();
    bool frozen  = IsFrozen();

    Color tint = frozen  ? Color{ 140, 200, 255, 255 } :
                 burning ? Color{ 255, 180, 180, 255 } :
                           WHITE;

    if (burning)
    {
        for (int i = 0; i < 3; ++i)
        {
            float flickerX = (float)GetRandomValue(-14, 14);
            float flickerY = (float)GetRandomValue(-26, -4);
            DrawCircleV(Vector2{ screenPos.x + flickerX, screenPos.y + flickerY }, 5.f, Fade(ORANGE, 0.55f));
            DrawCircleV(Vector2{ screenPos.x + flickerX * 0.7f, screenPos.y + flickerY - 6.f }, 3.f, Fade(YELLOW, 0.45f));
        }
    }

    DrawTexturePro(_texture, source, dest, Vector2{}, 0.f, tint);

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

void Enemy::DrawHealthBar(Vector2 screenPos, float w, float h)
{
    if (_health <= 0)
        return;

    float healthPercent = (float)_health / (float)_maxHealth;

    float barWidth = w * 0.8f;
    float barHeight = 6.f;

    float barX = screenPos.x - barWidth / 2.f;
    float barY = screenPos.y - h / 2.f - 12.f;

    DrawRectangle(barX, barY, barWidth, barHeight, RED);
    DrawRectangle(barX, barY, barWidth * healthPercent, barHeight, GREEN);
}

void Enemy::ApplyFreeze(float duration)
{
    if (_dying || !IsAlive())
        return;

    // Extend freeze if already frozen, otherwise start fresh
    if (duration > _freezeTimer)
        _freezeTimer = duration;
}

void Enemy::SetWaveScale(int wave)
{
    if (wave <= 3)
    {
        _health    = 3.f;
        _maxHealth = 3.f;
        _attackPower = 1.f;
        _speed     = 200.f;
    }
    else if (wave <= 5)
    {
        _health    = 6.f;
        _maxHealth = 6.f;
        _attackPower = 1.f;
        _speed     = 230.f;
    }
    else if (wave <= 7)
    {
        _health    = 10.f;
        _maxHealth = 10.f;
        _attackPower = 2.f;
        _speed     = 260.f;
    }
    else
    {
        _health    = 16.f;
        _maxHealth = 16.f;
        _attackPower = 3.f;
        _speed     = 290.f;
    }
}

void Enemy::ApplyBurn(float delay, int damage, Vector2 sourcePos)
{
    if (_dying || !IsAlive())
        return;

    _pendingBurns.push_back(PendingBurn{ delay, damage, sourcePos });
}

void Enemy::UpdateBurns(float dt)
{
    int writeIndex = 0;

    for (int i = 0; i < static_cast<int>(_pendingBurns.size()); ++i)
    {
        PendingBurn burn = _pendingBurns[i];
        burn.timer -= dt;

        if (burn.timer <= 0.f)
        {
            if (IsAlive() && !_dying)
                TakeDamage(burn.damage, burn.sourcePos);
            continue;
        }

        _pendingBurns[writeIndex++] = burn;
    }

    _pendingBurns.resize(writeIndex);
}

void Enemy::PlayAttackSound()
{
    float pitch = GetRandomValue(100, 140) / 100.f;
    SetSoundPitch(_attackSound, pitch);
    SetSoundVolume(_attackSound, 0.5f);
    PlaySound(_attackSound);
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

void Enemy::EnsureSharedResourcesLoaded()
{
    if (_sharedResourcesLoaded)
        return;

    _sharedIdleAnim = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Enemy\\EnemyIdle.png");
    _sharedWalkAnim = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Enemy\\EnemyWalk.png");
    _sharedAttackAnim = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Enemy\\EnemyAttack.png");
    _sharedTakeDamageAnim = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Enemy\\EnemyDamage.png");
    _sharedDeathAnim = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Enemy\\EnemyDeath.png");
    _sharedAttackSound = LoadSound("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Sounds\\SwordSwipe2.wav");
    _sharedHurtSound = LoadSound("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Sounds\\SmallMonsterDamage.wav");
    _sharedDeathSound = LoadSound("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Sounds\\PlayerDeath.wav");
    _sharedResourcesLoaded = true;
}

void Enemy::UnloadSharedResources()
{
    if (!_sharedResourcesLoaded)
        return;

    UnloadTexture(_sharedIdleAnim);
    UnloadTexture(_sharedWalkAnim);
    UnloadTexture(_sharedAttackAnim);
    UnloadTexture(_sharedTakeDamageAnim);
    UnloadTexture(_sharedDeathAnim);
    UnloadSound(_sharedAttackSound);
    UnloadSound(_sharedHurtSound);
    UnloadSound(_sharedDeathSound);

    _sharedIdleAnim = Texture2D{};
    _sharedWalkAnim = Texture2D{};
    _sharedAttackAnim = Texture2D{};
    _sharedTakeDamageAnim = Texture2D{};
    _sharedDeathAnim = Texture2D{};
    _sharedAttackSound = Sound{};
    _sharedHurtSound = Sound{};
    _sharedDeathSound = Sound{};
    _sharedResourcesLoaded = false;
}
