#include "Enemy.h"
#include "raymath.h"

Enemy::Enemy(Vector2 pos, Texture2D idleTex, Texture2D walkTex)
{
    _worldPos = pos;

    _idle = idleTex;
    _walk = walkTex;
    _texture = _idle;

    _width = 32.f;                          // Explicit frame width
    _height = _texture.height;
    _maxFrames = _texture.width / _width;
}

void Enemy::Tick(float dt, Vector2 heroWorldPos)
{
    if (_target == nullptr) return;

    // Save last position BEFORE movement
    _worldPosLastFrame = _worldPos;

    // Direction toward target
    Vector2 toTarget = Vector2Subtract(_target->GetWorldPos(), _worldPos);

    if (Vector2Length(toTarget) > 1.f)
    {
        toTarget = Vector2Normalize(toTarget);

        // Move with deltaTime
        _worldPos = Vector2Add(
            _worldPos,
            Vector2Scale(toTarget, _speed * dt)
        );

        // Flip sprite
        if (toTarget.x < 0.f)
            _rightLeft = -1.f;
        else if (toTarget.x > 0.f)
            _rightLeft = 1.f;

        _texture = _walk;
    }
    else
    {
        _texture = _idle;
    }

    // Animation update
    _runningTime += dt;
    if (_runningTime >= _updateTime)
    {
        _frame++;
        _runningTime = 0.f;
        if (_frame >= _maxFrames) _frame = 0;
    }

    // World → screen
    float w = _width * _scale;
    float h = _height * _scale;

    Vector2 screenPos = Vector2Subtract(_worldPos, heroWorldPos);
    screenPos.x += GetScreenWidth() / 2.f;
    screenPos.y += GetScreenHeight() / 2.f;

    Rectangle source{ _frame * _width, 0.f, _rightLeft * _width, _height };
    Rectangle dest{
        screenPos.x - w / 2.f,
        screenPos.y - h / 2.f,
        w, h
    };

    DrawTexturePro(_texture, source, dest, Vector2{}, 0.f, WHITE);

}

