#pragma once

#include "GameTypes.h"
#include "InputPrompts.h"
#include "raylib.h"

#include <functional>

struct DemoEndRenderContext
{
    bool touchModeActive = false;
    InputPromptMode promptMode = InputPromptMode::KeyboardMouse;
    bool demoCompleted = false;
    int enemiesKilled = 0;
    int bossesDefeated = 0;
    int playerGold = 0;
    int playerLevel = 1;
    const char* unlockLine = "";   // e.g. "NEW CLASS UNLOCKED: Paladin" (empty = none)
};

class OverlayRenderer
{
public:
    void DrawDemoEnd(const DemoEndRenderContext& ctx) const;
};