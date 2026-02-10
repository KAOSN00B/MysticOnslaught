#pragma once
#include <string>
#include "raylib.h"

class Player
{
public:
	
	Player(float speed, int health);

	void Attack();

	int GetHealth();

	
	
private:
	float _speed;
	int _health;
	Vector2 position;

	
};