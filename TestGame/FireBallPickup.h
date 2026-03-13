#pragma once
#include "raylib.h"

class FireBallPickup
{
public:
	FireBallPickup();
	~FireBallPickup();

	void Init(Vector2 spawnPos);
	void Draw(Vector2 worldOffset);
	void Destroy();
	Vector2 GetPosition() const;
	Rectangle GetCollisionRec();

	bool IsActive() const;




private:

	float _damage;
	float _scale = 4.f;
	Vector2 _worldPos{};
	Texture _texture;
	bool _isActive = false;


};