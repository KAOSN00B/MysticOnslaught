#pragma once
#include "raylib.h"

class Prop
{
public:
	Prop() = default;
	Prop(Vector2 pos, Texture2D tex);
	// Animated prop: tex is a horizontal sprite sheet with frameCount frames.
	// frameXOffset shifts the source X so frames don't have to start at column 0
	// (use when the sheet has leading padding, e.g. PillarTorch.png).
	Prop(Vector2 pos, Texture2D tex, int frameCount, int frameWidth, int frameHeight,
	     float scale = 4.f, float frameTime = 1.f / 10.f, int frameXOffset = 0);
	void Render(Vector2 heroWorldPos);
	Rectangle GetCollisionRec() const;
	Vector2 GetEnemyCollisionCenter() const;
	float GetEnemyCollisionRadius() const;
	Vector2 GetWorldPos() const { return _worldPos; }
	// Trims the collision box from the top by a fraction (0.25 = top quarter is passable).
	void SetCollisionTopFraction(float f) { _collisionTopFraction = f; }
	// Trims each side inward by a fraction (0.3 = 30% trimmed per side, leaving 40% centered).
	void SetCollisionSideFraction(float f) { _collisionSideFraction = f; }

protected:
	Texture2D _texture{};
	Vector2 _worldPos{};
	float _scale = 4.0f;
	float _collisionTopFraction  = 0.f;  // fraction of height removed from the top of the hitbox
	float _collisionSideFraction = 0.f;  // fraction of width trimmed from each side

	// Animation — only active when _frameCount > 1
	int   _frameCount   = 1;
	int   _frameWidth   = 0;
	int   _frameHeight  = 0;
	int   _frameXOffset = 0;   // column in the sheet where frame 0 starts
	float _frameTime    = 1.f / 10.f;
};
