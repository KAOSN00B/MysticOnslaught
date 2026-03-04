#include "raylib.h"
#include "raymath.h"
#include "Character.h"
#include "Prop.h"
#include "Enemy.h"
int main()
{
	
	const int windowWidth = 1200;
	const int windowHeight = 800;

	InitWindow(windowWidth, windowHeight, "Top Down Game");

	Texture2D map = LoadTexture("C:/Users/rober/Desktop/Lasalle/Semester 4/2DGamesProgramming/ClassNotes/TestGame/TileSet/Map.png");
	Vector2 mapPos = { 0.0f, 0.0f };
	const float mapScale = 5.5f;

	Character hero;

	Texture2D pillarTex = LoadTexture("C:/Users/rober/Desktop/Lasalle/Semester 4/2DGamesProgramming/ClassNotes/TestGame/TileSet/Pillar.png");
	Prop props[2]
	{
		Prop{ Vector2{500.0f, 500.0f}, pillarTex },
		Prop{ Vector2{1300.0f, 700.0f}, pillarTex }
	};

	Enemy goblin
	{ 
		Vector2{1000.0f, 500.0f}, 
	};

	goblin.SetTarget(&hero);

	SetTargetFPS(60);

	while (!WindowShouldClose())
	{
		
		BeginDrawing();
		ClearBackground(WHITE);
		mapPos = Vector2Scale(hero.GetWorldPos(), -1.0f);

		//draw map
		DrawTextureEx(map, mapPos, 0.0f, mapScale, WHITE);
		
		for (auto& prop : props)
		{
			prop.Render(hero.GetWorldPos());
			
		}

		

		// checking map bounds
		if (hero.GetWorldPos().x < 0.0f || hero.GetWorldPos().y < 0.0f
			|| hero.GetWorldPos().x + windowWidth > map.width * mapScale
			|| hero.GetWorldPos().y + windowHeight  > map.height * mapScale)
		{
			hero.UndoMovement();
		}

		for (auto& prop : props)
		{

			if (CheckCollisionRecs(
				prop.GetCollisionRec(),
				hero.GetCollisionRec()))
			{
				hero.UndoMovement();
			}
		}
		hero.Tick(GetFrameTime());
		goblin.Tick(GetFrameTime(), hero.GetWorldPos());
		hero.DealDamage(goblin);
	
		
		
		EndDrawing();
	}

	UnloadTexture(map);
	CloseWindow();
	
	return 0;
}
