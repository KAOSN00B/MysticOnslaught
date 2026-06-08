#include "Prop.h"
#include "VirtualCanvas.h"
#include "raymath.h"
#include "VirtualCanvas.h"
#include <algorithm>

Prop::Prop(Vector2 pos, Texture2D tex)
	: _worldPos{ pos }, _texture{ tex }
{
}

Prop::Prop(Vector2 pos, Texture2D tex, int frameCount, int frameWidth, int frameHeight,
           float scale, float frameTime, int frameXOffset)
	: _worldPos{ pos }, _texture{ tex }, _frameCount{ frameCount },
	  _frameWidth{ frameWidth }, _frameHeight{ frameHeight },
	  _scale{ scale }, _frameTime{ frameTime }, _frameXOffset{ frameXOffset }
{
}

void Prop::Render(Vector2 heroWorldPos)
{
    Vector2 screenPos = Vector2Subtract(_worldPos, heroWorldPos);
    screenPos.x += kVirtualWidth / 2.0f;
    screenPos.y += kVirtualHeight / 2.0f;

    if (_frameCount > 1)
    {
        // Advance frame based on wall-clock time so all torches animate smoothly
        // without needing a per-prop Update() call.
        int frame = (int)(GetTime() / _frameTime) % _frameCount;
        Rectangle src{ (float)(_frameXOffset + frame * _frameWidth), 0.f, (float)_frameWidth, (float)_frameHeight };
        Rectangle dst{ screenPos.x, screenPos.y, (float)_frameWidth * _scale, (float)_frameHeight * _scale };
        DrawTexturePro(_texture, src, dst, { 0.f, 0.f }, 0.f, WHITE);
    }
    else
    {
        DrawTextureEx(_texture, screenPos, 0.0f, _scale, WHITE);
    }
}

Rectangle Prop::GetCollisionRec() const
{
    float hitboxWidth  = (_frameCount > 1 ? (float)_frameWidth  : (float)_texture.width)  * _scale;
    float hitboxHeight = (_frameCount > 1 ? (float)_frameHeight : (float)_texture.height) * _scale;

    float topInset  = hitboxHeight * _collisionTopFraction;
    float sideInset = hitboxWidth  * _collisionSideFraction;
    return Rectangle{
        _worldPos.x + sideInset,
        _worldPos.y + topInset,
        hitboxWidth  - sideInset * 2.f,
        hitboxHeight - topInset
    };
}

Vector2 Prop::GetEnemyCollisionCenter() const
{
    Rectangle rec = GetCollisionRec();
    return Vector2{ rec.x + rec.width * 0.5f, rec.y + rec.height * 0.5f };
}

float Prop::GetEnemyCollisionRadius() const
{
    Rectangle rec = GetCollisionRec();

    // Enemy-vs-prop collision uses a circular proxy instead of the full
    // rectangle so enemies do not snag as hard on the pillar corners. This
    // keeps props solid while making navigation around them feel rounder.
    return std::min(rec.width, rec.height) * 0.42f;
}
