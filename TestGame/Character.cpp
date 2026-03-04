#include "Character.h"
#include "raymath.h"

Character::Character()
{
    _idle = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Hero\\Hero_Idle.png");
    _walk = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Hero\\Hero_Walk.png");
    _attack = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Hero\\Hero_Slash.png");
    _death = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Hero\\Hero_Death.png");
	_takeDamageAnim = LoadTexture("C:\\Users\\rober\\Desktop\\Lasalle\\Semester 4\\2DGamesProgramming\\ClassNotes\\TestGame\\Hero\\Hero_TakeDamage.png");

    _texture = _idle;

    _width = 32.f;
    _height = _texture.height;
    _scale = 6.f;
    _speed = 500.f;
    _health = 4;
	_knockbackStrength = 2000.f;

    _maxFrames = _texture.width / _width;
}

void Character::Tick(float dt)
{
    _worldPosLastFrame = _worldPos;

    // physics/state timers always run
    ApplyVelocity(dt);
    UpdateHit(dt);
    UpdateDeath(dt);

    // Only block CONTROL, not animation/draw
    if (!_dying && !_takingDamage)
    {
        HandleInput();
        HandleMovement(dt);
        HandleAttack();
    }

    // Animation + draw ALWAYS run so hurt/death anim can play
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
}

void Character::HandleMovement(float dt)
{
    // Do not move while attacking, hurt, or dying
    if (_attacking || _takingDamage || _dying) return;

    if (Vector2Length(_direction) > 0)
    {
        _worldPos = Vector2Add( _worldPos,  Vector2Scale(Vector2Normalize(_direction), _speed * dt));

        if (_direction.x < 0) _rightLeft = -1;
        if (_direction.x > 0) _rightLeft = 1;

        _texture = _walk;
    }
    else
    {
        _texture = _idle;
    }
}

void Character::HandleAttack()
{
    // Cannot attack if hurt or dying
    if (_takingDamage || _dying) return;

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !_attacking)
    {
        _attacking = true;
        _damageApplied = false;

        _texture = _attack;

        _frame = 0;
        _runningTime = 0.f;

        _maxFrames = _texture.width / _width;
        _updateTime = _attackUpdateTime;
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
        enemy.TakeDamage(1, _worldPos);
        _damageApplied = true;
    }
}

void Character::DrawPlayer()
{
    float w = _width * _scale;
    float h = _height * _scale;

    Rectangle source{ _frame * _width, 0.f, _rightLeft * _width, _height};

    Rectangle dest{ (GetScreenWidth() - w) * 0.5f, (GetScreenHeight() - h) * 0.5f, w, h};

    DrawTexturePro(_texture, source, dest, Vector2{}, 0.f, WHITE);
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
            // death animation stops on last frame
            if (_dying)
            {
                _frame = _maxFrames - 1;
                return;
            }

            // damage animation finishes and returns to idle
            if (_takingDamage)
            {
                _takingDamage = false;

                _texture = _idle;
                _updateTime = 1.f / 8.f;
                _maxFrames = _texture.width / _width;

                _frame = 0;
                return;
            }

            // attack animation finishes
            if (_attacking)
            {
                _attacking = false;

                _texture = _idle;
                _updateTime = 1.f / 8.f;
                _maxFrames = _texture.width / _width;
            }

            _frame = 0;
        }
    }
}

void Character::Death()
{
    // stop movement and control but don't teleport
    _velocity = Vector2Zero();
}
