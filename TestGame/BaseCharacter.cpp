#include "BaseCharacter.h"
#include "raymath.h"

BaseCharacter::BaseCharacter()
{
     _worldPos = Vector2Zero();
     _worldPosLastFrame = Vector2Zero();
}

void BaseCharacter::UndoMovement()
{
    _worldPos = _worldPosLastFrame;
}

Rectangle BaseCharacter::GetCollisionRec() 
{
    float w = _width * _scale * 0.5f;
    float h = _height * _scale * 0.4f;

    return Rectangle{
        _worldPos.x - w / 2.f,
        _worldPos.y - h / 2.f + (_height * _scale * 0.3f),
        w,
        h
    };
}

