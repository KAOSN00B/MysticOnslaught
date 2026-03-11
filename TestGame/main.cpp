#include "Engine.h"

int main()
{
	Engine engine;
	engine.Run();

	CloseAudioDevice();   // AFTER engine destroyed
	return 0;
}	