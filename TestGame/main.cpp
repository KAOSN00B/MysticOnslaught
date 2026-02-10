#include "raylib.h"
#include "raymath.h"
#include "Character.h"

int main()
{
	
	const int windowWidth = 1200;
	const int windowHeight = 800;

	InitWindow(windowWidth, windowHeight, "Top Down Game");

	Texture2D map = LoadTexture("C:/Users/rober/Desktop/Lasalle/Semester 4/2DGamesProgramming/ClassNotes/TestGame/TileSet/Map.png");
	Vector2 mapPos = { 0.0f, 0.0f };
	const float mapScale = 5.5f;

	Character hero{ windowWidth, windowHeight };
	SetTargetFPS(60);

	while (!WindowShouldClose())
	{
		BeginDrawing();
		ClearBackground(WHITE);


		//draw map
		DrawTextureEx(map, mapPos, 0.0f, mapScale, WHITE);

		mapPos = Vector2Scale(hero.GetWorldPos(), -1.0f);
		hero.Tick(GetFrameTime());

		// checking map bounds
		if (hero.GetWorldPos().x < 0.0f || hero.GetWorldPos().y < 0.0f
			|| hero.GetWorldPos().x + windowWidth > map.width * mapScale
			|| hero.GetWorldPos().y + windowHeight  > map.height * mapScale)
		{
			hero.UndoMovement();
		}
		
		EndDrawing();
	}

	UnloadTexture(map);
	CloseWindow();
	
	return 0;
}
