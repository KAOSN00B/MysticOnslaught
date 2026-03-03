#include "Character.h"
#include "raymath.h"

Character::Character(int winWidth, int winHeight)
{
    _idle = LoadTexture("C:/Users/rober/Desktop/Lasalle/Semester 4/2DGamesProgramming/ClassNotes/TestGame/Hero/Hero_Idle.png");
    _walk = LoadTexture("C:/Users/rober/Desktop/Lasalle/Semester 4/2DGamesProgramming/ClassNotes/TestGame/Hero/Hero_Walk.png");
	_attack = LoadTexture("C:/Users/rober/Desktop/Lasalle/Semester 4/2DGamesProgramming/ClassNotes/TestGame/Hero/Hero_Slash.png");
    _texture = _idle;
    _speed = 500.0f;

    _width = 32.f;                     // Explicit frame width
    _height = _texture.height;
    _maxFrames = _texture.width / _width;
}

void Character::Tick(float dt)
{
    _worldPosLastFrame = _worldPos;

    Vector2 direction{};

    if (IsKeyDown(KEY_A)) direction.x -= 1.f;
    if (IsKeyDown(KEY_D)) direction.x += 1.f;
    if (IsKeyDown(KEY_W)) direction.y -= 1.f;
    if (IsKeyDown(KEY_S)) direction.y += 1.f;

    // Trigger attack
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !_attacking)
    {
        _attacking = true;
        _texture = _attack;

        _frame = 0;
        _maxFrames = _texture.width / _width;
        _updateTime = _attackUpdateTime;
    }

    // Movement (disabled while attacking)
    if (!_attacking)
    {
        if (Vector2Length(direction) > 0.f)
        {
            _worldPos = Vector2Add(
                _worldPos,
                Vector2Scale(Vector2Normalize(direction), _speed * dt)
            );

            if (direction.x < 0.f)
                _rightLeft = -1.f;
            else if (direction.x > 0.f)
                _rightLeft = 1.f;

            _texture = _walk;
        }
        else
        {
            _texture = _idle;
        }
    }

    // Animation update
    _runningTime += dt;

    if (_runningTime >= _updateTime)
    {
        _frame++;
        _runningTime = 0.f;

        if (_frame >= _maxFrames)
        {
            if (_attacking)
            {
                _attacking = false;
                _texture = _idle;

                _updateTime = 1.f / 8.f; // restore normal speed
                _maxFrames = _texture.width / _width;
            }

            _frame = 0;
        }
    }

    // Draw centered
    float w = _width * _scale;
    float h = _height * _scale;

    Rectangle source{ _frame * _width, 0.f, _rightLeft * _width, _height };
    Rectangle dest{
        (GetScreenWidth() - w) * 0.5f,
        (GetScreenHeight() - h) * 0.5f,
        w, h
    };

    DrawTexturePro(_texture, source, dest, Vector2{}, 0.f, WHITE);
}