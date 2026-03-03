#pragma once

#include "BaseCharacter.h"
#include "Character.h"  

class Enemy : public BaseCharacter
{
public:
    Enemy(Vector2 pos, Texture2D idleTex, Texture2D walkTex);

    void Tick(float deltaTime, Vector2 heroWorldPos);

	void SetTarget(Character* character) { _target = character; }

private:
    float _updateTime = 1.f / 3.f;
    Character* _target;
};