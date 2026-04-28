#include "Engine.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#ifdef __EMSCRIPTEN__
static void RunWebFrame(void* arg)
{
    Engine* engine = static_cast<Engine*>(arg);
    engine->RunFrame();
}
#endif

int main()
{
	Engine engine;

#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop_arg(RunWebFrame, &engine, 0, true);
#else
	engine.Run();
#endif

	CloseAudioDevice();   // AFTER engine destroyed
	return 0;
}	
