#pragma once

#include "GameTypes.h"
#include "InputPrompts.h"
#include "raylib.h"

#include <functional>

class Character;

struct DemoEndRenderContext
{
    bool touchModeActive = false;
    InputPromptMode promptMode = InputPromptMode::KeyboardMouse;
    bool demoCompleted = false;
    int enemiesKilled = 0;
    int bossesDefeated = 0;
    int playerGold = 0;
    int playerLevel = 1;
};

struct ExpTallyRenderContext
{
    Character* player = nullptr;
    int tallyStartLevel = 1;
    float pendingExp = 0.f;
    bool expTallyDone = false;
    int tallyLevelUpsRemaining = 0;
    bool tallyChoiceChaining = false;
    bool touchModeActive = false;
    InputPromptMode promptMode = InputPromptMode::KeyboardMouse;
};

struct HowToPlayRenderContext
{
    GameState howToPlayFrom = GameState::Menu;
    int* htpTab = nullptr;
    float* htpSlideOffset = nullptr;
    InputPromptMode promptMode = InputPromptMode::KeyboardMouse;

    Texture2D* shopBorderTex = nullptr;
    Texture2D* abilityIconFireTex = nullptr;
    Texture2D* abilityIconIceTex = nullptr;
    Texture2D* abilityIconElectricTex = nullptr;
    Texture2D* mapIconNormal = nullptr;
    Texture2D* mapIconElite = nullptr;
    Texture2D* mapIconShop = nullptr;
    Texture2D* mapIconTreasure = nullptr;
    Texture2D* mapIconBoss = nullptr;
    Texture2D* mapIconRest = nullptr;

    std::function<void()> onUiConfirm;
    std::function<void()> onBack;
};

class OverlayRenderer
{
public:
    void DrawDemoEnd(const DemoEndRenderContext& ctx) const;
    void DrawExpTally(const ExpTallyRenderContext& ctx) const;
    void DrawHowToPlay(const HowToPlayRenderContext& ctx) const;
};