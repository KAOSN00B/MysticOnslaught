#include "Character.h"
#include "raymath.h"

Character::Character()
{
    _worldPos = Vector2Zero();
}

Character::~Character()
{
    UnloadTexture(_idleAnim);
    UnloadTexture(_walkAnim);
    UnloadTexture(_attackAnim);
    UnloadTexture(_takeDamageAnim);
    UnloadTexture(_deathAnim);
    UnloadTexture(_dashAnim);

    UnloadSound(_footStepSound);
    UnloadSound(_attackSound);
    UnloadSound(_hurtSound);
    UnloadSound(_deathSound);
}

void Character::Init()
{
    _idleAnim = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Hero\\Hero_Idle.png");
    _walkAnim = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Hero\\Hero_Walk.png");
    _attackAnim = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Hero\\Hero_Slash.png");
    _dashAnim = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Hero\\Hero_Dash.png");
    _deathAnim = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Hero\\Hero_Death.png");
    _takeDamageAnim = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Hero\\Hero_TakeDamage.png");

    _footStepSound = LoadSound("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Sounds\\FootSteps.wav");
    _attackSound = LoadSound("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Sounds\\SwordSwipe.wav");
    _hurtSound = LoadSound("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Sounds\\PlayerHurt.wav");
    _deathSound = LoadSound("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Sounds\\PlayerDeath.wav");

    _texture = _idleAnim;

    _width = 32.f;
    _height = _texture.height;
    _scale = 6.f;

    _speed = 500.f;

    _health = 5;
    _maxHealth = 5;

    _attackPower = 2.f;

    _maxFrames = _texture.width / _width;
}

void Character::Update(float dt)
{
    _worldPosLastFrame = _worldPos;

    ApplyVelocity(dt);
    UpdateHit(dt);
    UpdateDeath(dt);

    if (!_dying && !_takingDamage)
    {
        HandleInput();

        if (!Dashing(dt))
            HandleMovement(dt);

        HandleAttack();
    }

    if (_hasIFrames)
    {
        _invincibleTimer -= dt;

        if (_invincibleTimer <= 0)
            _hasIFrames = false;
    }

    HandleAnimation(dt);
    DrawPlayer();
}

void Character::HandleInput()
{
    _direction = {};

    if (IsKeyDown(KEY_A)) _direction.x -= 1;
    if (IsKeyDown(KEY_D)) _direction.x += 1;
    if (IsKeyDown(KEY_W)) _direction.y -= 1;
    if (IsKeyDown(KEY_S)) _direction.y += 1;

    if (IsKeyPressed(KEY_SPACE) && !_isDashing && _dashCooldown <= 0.f)
    {
        _isDashing = true;
        _dashInvincible = true;

        _dashTimer = _dashDuration;
        _dashCooldown = _dashCooldownTime;

        _texture = _dashAnim;
        _frame = 0;
        _runningTime = 0.f;

        _maxFrames = _texture.width / _width;
        _updateTime = 1.f / 16.f;

        _dashDirection = _direction;

        if (Vector2Length(_direction) > 0)
            _dashDirection = Vector2Normalize(_direction);
        else
            _dashDirection = Vector2{ (float)_rightLeft, 0.f };
    }
}

void Character::HandleMovement(float dt)
{
    if (_attacking || _takingDamage || _dying) return;

    if (Vector2Length(_direction) > 0)
    {
        _worldPos = Vector2Add(
            _worldPos,
            Vector2Scale(Vector2Normalize(_direction), _speed * dt)
        );

        if (_direction.x < 0) _rightLeft = -1;
        if (_direction.x > 0) _rightLeft = 1;

        _texture = _walkAnim;

        _stepTimer -= dt;

        if (_stepTimer <= 0)
        {
            PlayFootStepSound();
            _stepTimer = _stepDelay;
        }
    }
    else
    {
        _texture = _idleAnim;
        _stepTimer = 0.f;
    }
}

void Character::HandleAttack()
{
    if (_takingDamage || _dying) return;

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !_attacking)
    {
        _attacking = true;
        _damageApplied = false;

        _texture = _attackAnim;

        _frame = 0;
        _runningTime = 0.f;

        _maxFrames = _texture.width / _width;
        _updateTime = _attackUpdateTime;

        PlayAttackSound();
    }
}

void Character::DealDamage(BaseCharacter& enemy)
{
    if (!_attacking || _damageApplied) return;
    if (_frame != 2) return;

    Rectangle attackRec = GetCollisionRec();
    attackRec.x += _rightLeft * 50;

    if (CheckCollisionRecs(attackRec, enemy.GetCollisionRec()))
    {
        enemy.TakeDamage(_attackPower, _worldPos);
        _damageApplied = true;
    }
}

void Character::DrawPlayer()
{
    float w = _width * _scale;
    float h = _height * _scale;

    Rectangle source{ _frame * _width, 0.f, _rightLeft * _width, _height };

    Rectangle dest{
        (GetScreenWidth() - w) * 0.5f,
        (GetScreenHeight() - h) * 0.5f,
        w,
        h
    };

    if (_playDashParticles)
        DashParticles(h);

    DrawTexturePro(_texture, source, dest, Vector2{}, 0.f, WHITE);
}

void Character::DashParticles(float h)
{
    Vector2 screenPos{
        GetScreenWidth() / 2.f,
        GetScreenHeight() / 2.f + h * 0.15f
    };

    float dashPercent = _dashTimer / _dashDuration;
    float trailLength = 160.f * dashPercent;

    Vector2 dashTrail = Vector2Scale(_dashDirection, -trailLength);

    Vector2 perp{ -_dashDirection.y, _dashDirection.x };
    Vector2 offset = Vector2Scale(perp, 18.f);

    Color c = Fade(SKYBLUE, dashPercent);

    DrawLineEx(screenPos, Vector2Add(screenPos, dashTrail), 18, c);
    DrawLineEx(Vector2Add(screenPos, offset),
        Vector2Add(Vector2Add(screenPos, dashTrail), offset), 12, c);

    DrawLineEx(Vector2Subtract(screenPos, offset),
        Vector2Subtract(Vector2Add(screenPos, dashTrail), offset), 12, c);
}

void Character::HandleAnimation(float dt)
{
    _runningTime += dt;

    if (_runningTime >= _updateTime)
    {
        _frame++;
        _runningTime = 0.f;

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
                _updateTime = 1.f / 8.f;
                _maxFrames = _texture.width / _width;

                _frame = 0;
                return;
            }

            if (_attacking)
            {
                _attacking = false;

                _texture = _idleAnim;
                _updateTime = 1.f / 8.f;
                _maxFrames = _texture.width / _width;
            }

            _frame = 0;
        }
    }
}

void Character::Death()
{
    _velocity = Vector2Zero();
}

bool Character::Dashing(float dt)
{
    if (_dashCooldown > 0)
        _dashCooldown -= dt;

    if (!_isDashing)
    {
        _playDashParticles = false;
        _dashInvincible = false;
        return false;
    }

    _playDashParticles = true;

    _dashTimer -= dt;

    _worldPos = Vector2Add(
        _worldPos,
        Vector2Scale(_dashDirection, _dashSpeed * dt)
    );

    if (_dashTimer <= 0)
    {
        _isDashing = false;
        _dashInvincible = false;
        _playDashParticles = false;

        _texture = _idleAnim;
        _frame = 0;
        _runningTime = 0.f;

        _maxFrames = _texture.width / _width;
        _updateTime = 1.f / 8.f;
    }

    return true;
}

int Character::GetHealth() const
{
    return _health;
}

void Character::TakeDamage(int damage, Vector2 attackerPos)
{
    if (_hasIFrames || _dashInvincible)
        return;

    _hasIFrames = true;
    _invincibleTimer = 0.4f;

    BaseCharacter::TakeDamage(damage, attackerPos);
}