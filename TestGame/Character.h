#pragma once
#include "raylib.h"

class Character
{
public:
	Character(int winWidth, int winHeight);
	//functions
	void Tick(float deltaTime);
	void UndoMovement();

	//getters and setters
	float GetWidth() const { return _width * _scale; }
	float GetHeight() const { return _height * _scale; }
	Vector2 GetWorldPos();


private:
	Texture2D _texture{ LoadTexture("C:/Users/rober/Desktop/Lasalle/Semester 4/2DGamesProgramming/ClassNotes/TestGame/Hero 003/Hero_Idle.png") };
	Texture2D _idle{ LoadTexture("C:/Users/rober/Desktop/Lasalle/Semester 4/2DGamesProgramming/ClassNotes/TestGame/Hero 003/Hero_Idle.png") };;
	Texture2D _walk{ LoadTexture("C:/Users/rober/Desktop/Lasalle/Semester 4/2DGamesProgramming/ClassNotes/TestGame/Hero 003/Hero_Walk.png") };
	Vector2 _screenPos{};
	Vector2 _worldPos{};
	Vector2 _worldPosLastFrame{};

	float _rightLeft = 1.0f; //-1 left +1 right
	//animation variable
	float _runningTime = 0.0f;
	int _frame = 0.0f;
	int _maxFrames = 6.0f;
	float _updateTime = 1.0f / 12.0f;
	float _speed = 7.0f;

	float _width{};
	float _height{};
	float _scale = 6.0f;

};