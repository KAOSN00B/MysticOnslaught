#include "Character.h"
#include "raymath.h"

Character::Character(int winWidth, int winHeight)
{
	_width = _texture.width / _maxFrames;
	_height = _texture.height;

	_screenPos = {
		static_cast<float>(winWidth) / 2.0f - _scale * (0.5f * _width),
		static_cast<float>(winHeight) / 2.0f - _scale * (0.5f * _height) };
}

Vector2 Character::GetWorldPos() { return _worldPos; }

void Character::Tick(float deltaTime)
{
	_worldPosLastFrame = _worldPos;

	Vector2 direction{};

	if (IsKeyDown(KEY_A)) direction.x -= 1.0;
	if (IsKeyDown(KEY_D)) direction.x += 1.0;
	if (IsKeyDown(KEY_W)) direction.y -= 1.0;
	if (IsKeyDown(KEY_S)) direction.y += 1.0;

	if (Vector2Length(direction) != 0.0f)
	{
		//set worldPos = worldPos + direction            
		_worldPos = Vector2Add(_worldPos, Vector2Scale(Vector2Normalize(direction), _speed));
		direction.x < 0.0f ? _rightLeft = -1.0f : _rightLeft = 1.0f;
		_texture = _walk;
	}
	else
	{
		_texture = _idle;
	}

	_runningTime += deltaTime; //delta time function
	if (_runningTime >= _updateTime)
	{
		_frame++;
		_runningTime = 0.0f;
		if (_frame > _maxFrames) _frame = 0; //resets the frame

	}

	//draw character
	Rectangle source{ _frame * _width, 0.0f, _rightLeft * _width , _height };
	Rectangle dest{ _screenPos.x, _screenPos.y, _scale * _width , _scale * _height };
	Vector2 origin{};
	DrawTexturePro(_texture, source, dest, origin, 0.0f, WHITE);
}

void Character::UndoMovement()
{
	_worldPos = _worldPosLastFrame;
}
