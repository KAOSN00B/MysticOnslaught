#pragma once

#ifndef CHARACTER_H
#define CHARACTER_H

#include "raylib.h"
#include "BaseCharacter.h"

class Character : public BaseCharacter
{
public:
    Character(int winWidth, int winHeight);
    void Tick(float deltaTime);

    float GetWidth() const { return _width * _scale; }
    float GetHeight() const { return _height * _scale; }
    Vector2 GetWorldPos() { return _worldPos; }

private:
	bool _attacking = false;
    float _attackUpdateTime = 1.f / 8.f;   // slower attack animation
};
#endif