#pragma once

#ifndef BASECHARACTER_H
#define BASECHARACTER_H

#include "raylib.h"

class BaseCharacter
{
public:
    BaseCharacter();
    void UndoMovement();
    Rectangle GetCollisionRec();
	Vector2 GetWorldPos() const { return _worldPos; }

protected:
    // Textures
    Texture2D _texture{};
    Texture2D _idle{};
    Texture2D _walk{};
    Texture2D _attack{};

    // World data
    Vector2 _worldPos{};
    Vector2 _worldPosLastFrame{};

    // Animation
    float _runningTime = 0.f;
    float _updateTime = 1.f / 8.f;
	float _speed = 150.0f;
    int _frame = 0;
    int _maxFrames = 1;


    // Sprite info
    float _width = 0.f;
    float _height = 0.f;
    float _scale = 4.f;

    float _rightLeft = 1.f;
    Vector2 _velocity = {0.f, 0.f};
};

#endif 