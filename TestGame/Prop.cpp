#include "Prop.h"
#include "raymath.h"

Prop::Prop(Vector2 pos, Texture2D tex)
	: _worldPos{ pos }, _texture{ tex }
{
}

void Prop::Render(Vector2 heroWorldPos)
{
    Vector2 screenPos = Vector2Subtract(_worldPos, heroWorldPos);
    screenPos.x += GetScreenWidth() / 2.0f;
    screenPos.y += GetScreenHeight() / 2.0f;
	DrawTextureEx(_texture, screenPos, 0.0f, _scale, WHITE);
}

Rectangle Prop::GetCollisionRec() const
{
    float hitboxWidth = _texture.width * _scale;
    float hitboxHeight = _texture.height * _scale;

    return Rectangle{
        _worldPos.x + (_texture.width * _scale - hitboxWidth) / 2,
        _worldPos.y + (_texture.height * _scale - hitboxHeight) / 2,
        hitboxWidth,
        hitboxHeight
    };
}
