#include "Character.h"

#include "raymath.h"

Character::Character()
{
    _worldPos = Vector2Zero();
    _direction = Vector2Zero();
    _dashDirection = Vector2Zero();

    _hasIFrames = false;
    _dashInvincible = false;
    _isDashing = false;
    _attacking = false;
    _takingDamage = false;
    _dying = false;
    _damageApplied = false;
    _castingAbility = false;
    _playDashParticles = false;
    _queuedCast = CastType::None;
    _fireballAmmo  = 0;
    _swordBeamAmmo = 0;
    _freezeAmmo    = 0;

    _invincibleTimer = 0.f;
    _dashTimer = 0.f;
    _dashCooldown = 0.f;
    _runningTime = 0.f;
    _frame = 0;
    _rightLeft = 1;
}

Character::~Character()
{
    UnloadTexture(_idleAnim);
    UnloadTexture(_walkAnim);
    UnloadTexture(_attackAnim);
    UnloadTexture(_staffAnim);
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
    _staffAnim = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Hero\\Hero_Staff.png");
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
    _updateTime = 1.f / 8.f;

    _hasIFrames = false;
    _dashInvincible = false;
    _isDashing = false;
    _attacking = false;
    _takingDamage = false;
    _dying = false;
    _damageApplied = false;
    _castingAbility = false;
    _playDashParticles = false;
    _queuedCast = CastType::None;
    _fireballAmmo    = 0;
    _swordBeamAmmo   = 0;
    _freezeAmmo      = 0;
    _selectedAbility = 0;

    _exp            = 0;
    _level          = 0;
    _expToNextLevel = 10;

    _invincibleTimer = 0.f;
    _dashTimer = 0.f;
    _dashCooldown = 0.f;
    _runningTime = 0.f;
    _frame = 0;
    _stepTimer = 0.f;
    _rightLeft = 1;
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

        HandleAttackInput();
    }

    if (_hasIFrames)
    {
        _invincibleTimer -= dt;

        if (_invincibleTimer <= 0.f)
        {
            _invincibleTimer = 0.f;
            _hasIFrames = false;
        }
    }

    HandleAnimation(dt);
}

void Character::HandleInput()
{
    _direction = Vector2Zero();

    if (IsKeyDown(KEY_A)) _direction.x -= 1;
    if (IsKeyDown(KEY_D)) _direction.x += 1;
    if (IsKeyDown(KEY_W)) _direction.y -= 1;
    if (IsKeyDown(KEY_S)) _direction.y += 1;

    if (_isDashing)
    {
        if (IsKeyDown(KEY_A)) _direction.x -= 1;
        if (IsKeyDown(KEY_D)) _direction.x += 1;
        if (IsKeyDown(KEY_W)) _direction.y -= 1;
        if (IsKeyDown(KEY_S)) _direction.y += 1;
    }

    // 1-2-3-4 selects active ability slot
    if (IsKeyPressed(KEY_ONE))   _selectedAbility = 0;
    if (IsKeyPressed(KEY_TWO))   _selectedAbility = 1;
    if (IsKeyPressed(KEY_THREE)) _selectedAbility = 2;
    if (IsKeyPressed(KEY_FOUR))  _selectedAbility = 3;

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

        if (Vector2Length(_direction) > 0.f)
            _dashDirection = Vector2Normalize(_direction);
        else
            _dashDirection = Vector2{ (float)_rightLeft, 0.f };
    }
}

void Character::HandleMovement(float dt)
{
    if (_attacking || _castingAbility || _takingDamage || _dying)
        return;

    if (Vector2Length(_direction) > 0.f)
    {
        _worldPos = Vector2Add(_worldPos, Vector2Scale(Vector2Normalize(_direction), _speed * dt));

        if (_direction.x < 0) _rightLeft = -1;
        if (_direction.x > 0) _rightLeft = 1;

        _texture = _walkAnim;

        _stepTimer -= dt;

        if (_stepTimer <= 0.f)
        {
            PlayFootStepSound();
            _stepTimer = _stepDelay;
        }
    }
    else if (!_attacking && !_castingAbility)
    {
        _texture = _idleAnim;
        _stepTimer = 0.f;
    }
}

void Character::HandleAttackInput()
{
    if (_takingDamage || _dying)
        return;

    if (!_attacking && !_castingAbility && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
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

    // Right-click fires the currently selected ability slot (1=Fireball, 2=SwordBeam, 3=Freeze)
    if (!_attacking && !_castingAbility && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
    {
        _castingAbility = true;
        _queuedCast = CastType::None;

        if (_selectedAbility == 0 && _fireballAmmo > 0)
        {
            _fireballAmmo--;
            _queuedCast = CastType::Fireball;
        }
        else if (_selectedAbility == 1 && _swordBeamAmmo > 0)
        {
            _swordBeamAmmo--;
            _queuedCast = CastType::SwordBeam;
        }
        else if (_selectedAbility == 2 && _freezeAmmo > 0)
        {
            _freezeAmmo--;
            _queuedCast = CastType::Freeze;
        }

        _texture = _staffAnim;
        _frame = 0;
        _runningTime = 0.f;
        _maxFrames = _texture.width / _width;
        _updateTime = _staffCastUpdateTime;
    }
}

void Character::DrawPlayer()
{
    float w = _width * _scale;
    float h = _height * _scale;

    Rectangle source{ _frame * _width, 0.f, _rightLeft * _width, _height };
    Rectangle dest{ (GetScreenWidth() - w) * 0.5f, (GetScreenHeight() - h) * 0.5f, w, h };

    float shadowWidth = w * 0.40f;
    float shadowHeight = h * 0.05f;

    float shadowOffsetX = -_rightLeft * 3.f;
    float shadowX = dest.x + w * 0.5f + shadowOffsetX;
    float shadowY = dest.y + h - 2.f;

    DrawEllipse(shadowX, shadowY, shadowWidth, shadowHeight, Color{ 0, 0, 0, 70 });
    DrawEllipse(shadowX, shadowY, shadowWidth * 0.7f, shadowHeight * 0.7f, Color{ 0, 0, 0, 40 });

    if (_playDashParticles)
        DashParticles(h);

    DrawTexturePro(_texture, source, dest, Vector2{}, 0.f, WHITE);
}

void Character::DashParticles(float h)
{
    Vector2 screenPos{ GetScreenWidth() / 2.f, GetScreenHeight() / 2.f + h * 0.15f };

    float dashPercent = _dashTimer / _dashDuration;
    float trailLength = 160.f * dashPercent;

    Vector2 dashTrail = Vector2Scale(_dashDirection, -trailLength);
    Vector2 perp{ -_dashDirection.y, _dashDirection.x };
    Vector2 offset = Vector2Scale(perp, 18.f);

    Color c = Fade(SKYBLUE, dashPercent);

    DrawLineEx(screenPos, Vector2Add(screenPos, dashTrail), 18, c);
    DrawLineEx(Vector2Add(screenPos, offset), Vector2Add(Vector2Add(screenPos, dashTrail), offset), 12, c);
    DrawLineEx(Vector2Subtract(screenPos, offset), Vector2Subtract(Vector2Add(screenPos, dashTrail), offset), 12, c);
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

            if (_castingAbility)
            {
                _castingAbility = false;
                _texture = _idleAnim;
                _updateTime = 1.f / 8.f;
                _maxFrames = _texture.width / _width;
            }

            _frame = 0;
        }
    }
}

bool Character::Dashing(float dt)
{
    if (_dashCooldown > 0.f)
        _dashCooldown -= dt;

    if (!_isDashing)
    {
        _playDashParticles = false;
        _dashInvincible = false;
        return false;
    }

    _playDashParticles = true;
    _dashTimer -= dt;
    _worldPos = Vector2Add(_worldPos, Vector2Scale(_dashDirection, _dashSpeed * dt));

    if (_dashTimer <= 0.f)
    {
        _isDashing = false;
        _dashInvincible = false;
        _playDashParticles = false;
        _dashTimer = 0.f;

        _texture = _idleAnim;
        _frame = 0;
        _runningTime = 0.f;
        _maxFrames = _texture.width / _width;
        _updateTime = 1.f / 8.f;
    }

    return true;
}

void Character::Death()
{
    _velocity = Vector2Zero();
}

void Character::TakeDamage(int damage, Vector2 attackerPos)
{
    if (_hasIFrames || _dashInvincible)
        return;

    _hasIFrames = true;
    _invincibleTimer = 0.4f;

    BaseCharacter::TakeDamage(damage, attackerPos);
}

void Character::SetWorldPos(Vector2 pos)
{
    _worldPos = pos;
}

void Character::PlayHurtSound()
{
    float pitch = GetRandomValue(85, 110) / 100.f;
    SetSoundPitch(_hurtSound, pitch);
    SetSoundVolume(_hurtSound, 0.15f);
    PlaySound(_hurtSound);
}

void Character::AddFireballAmmo(int amount)  { _fireballAmmo  += amount; }
int  Character::GetFireballAmmo()  const      { return _fireballAmmo; }

void Character::AddSwordBeamAmmo(int amount) { _swordBeamAmmo += amount; }
int  Character::GetSwordBeamAmmo() const      { return _swordBeamAmmo; }

Character::CastType Character::ConsumeCastRequest()
{
    CastType queuedCast = _queuedCast;
    _queuedCast = CastType::None;
    return queuedCast;
}

bool Character::CanApplyMeleeDamage() const
{
    return _attacking && !_damageApplied && _frame == 2;
}

void Character::ConsumeMeleeDamageFrame()
{
    _damageApplied = true;
}

Rectangle Character::GetAttackCollisionRec() const
{
    Rectangle attackRec = GetCollisionRec();
    attackRec.x += _rightLeft * 50.f;
    return attackRec;
}

Vector2 Character::GetCastOrigin() const
{
    // Spawn slightly in front of the player so projectiles visually leave the body
    return Vector2Add(_worldPos, Vector2{ _rightLeft * 50.f, 0.f });
}

Vector2 Character::GetFacingDirection() const
{
    return Vector2{ (float)_rightLeft, 0.f };
}

void Character::AddFreezeAmmo(int amount)    { _freezeAmmo    += amount; }
int  Character::GetFreezeAmmo()    const      { return _freezeAmmo; }

void Character::AddExp(int amount)
{
    if (_level >= _maxLevel)
        return;

    _exp += amount;

    while (_exp >= _expToNextLevel && _level < _maxLevel)
    {
        _exp -= _expToNextLevel;
        _level++;
        _expToNextLevel *= 2;  // 10 → 20 → 40 → 80 ...

        // Level-up bonuses: +1 max HP, heal 1, +1 attack power
        _maxHealth += 1;
        _attackPower += 1.f;
        Heal(1);
    }

    // Clamp leftover EXP at max level
    if (_level >= _maxLevel)
        _exp = 0;
}

void Character::Heal(int amount)
{
    _health += amount;

    if (_health > _maxHealth)
        _health = _maxHealth;
}
