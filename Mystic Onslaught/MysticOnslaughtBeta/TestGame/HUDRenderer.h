#pragma once

#include "DebugPanel.h"
#include "Enemy.h"
#include "GameTypes.h"
#include "Pickup.h"
#include "Prop.h"
#include "TouchControls.h"

#include <functional>
#include <memory>
#include <vector>

class Character;

struct MiniMapContext
{
    Texture2D* map = nullptr;
    float mapScale = 1.f;
    Character* player = nullptr;
    std::vector<std::unique_ptr<Enemy>>* enemies = nullptr;
    std::vector<Prop>* props = nullptr;
    std::vector<std::unique_ptr<Pickup>>* pickups = nullptr;
};

struct HUDRenderContext
{
    Character* player = nullptr;
    int currentAct = 1;
    int currentRoom = 1;
    RoomType currentRoomType = RoomType::Standard;
    int eliteMechanic = -1;
    float eliteEnrageWarningTimer = 0.f;
    float bossWarningTimer = 0.f;
    bool touchModeActive = false;
    int windowWidth = 1920;
    int windowHeight = 1080;
    TouchControls* touch = nullptr;
    DebugPanel* debug = nullptr;
    MiniMapContext minimap;
    std::function<int()> getActiveEnemyCount;
    std::function<void()> drawAbilityBar;
    std::function<void()> drawTouchAbilityArc;
};

class HUDRenderer
{
public:
    void DrawMiniMap(const MiniMapContext& ctx) const;
    void DrawHUD(const HUDRenderContext& ctx) const;
};
