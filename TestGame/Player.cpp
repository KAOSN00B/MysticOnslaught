#include "Player.h"
#include <iostream>

Player::Player(float speed, int health)
	: _speed(speed), _health(health) { }

void Player::Attack()
{

}

int Player::GetHealth()
{
	return _health;
}


