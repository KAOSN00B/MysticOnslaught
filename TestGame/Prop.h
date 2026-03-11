#pragma once
#include "raylib.h"

class Prop
{
public:
	Prop() = default;
	Prop(Vector2 pos, Texture2D tex);
	void Render(Vector2 heroWorldPos);
	Rectangle GetCollisionRec();

protected:
	Texture2D _texture{};
	Vector2 _worldPos{};
	float _scale = 4.0f;
};