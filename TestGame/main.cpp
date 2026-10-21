#include "Engine.h"

int main()
{
#if !MO_DEV_TOOLS && !MO_ITCH_DEMO
    // Resolve packaged assets even when launched from a shortcut elsewhere.
    ChangeDirectory(GetApplicationDirectory());
#endif

	{
	Engine engine;
	engine.Run();
	} // Engine members release resources before the devices close
	if (IsAudioDeviceReady()) CloseAudioDevice();
	if (IsWindowReady()) CloseWindow();
	return Engine::ProcessExitCode();   // non-zero only when the dev flow self-test failed
}	
