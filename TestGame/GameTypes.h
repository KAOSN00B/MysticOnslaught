#pragma once

#include "raylib.h"

#include <vector>

enum class Biome { Dungeon, Forest, Swamp, Volcano, Tundra, Crypt, Desert, Ruins };

enum class RoomType
{
    Standard,
    Elite,
    Rest,
    Treasure,
    Store,
    Boss,
};

enum class LevelUpOfferContext
{
    NormalLevel,
    TreasureBasic,
    EliteReward,
    StoreStock,
};

enum class GameState
{
    Menu,
    Play,
    GameOver,
    Pause,
    HowToPlay,
    Keybindings,
    LevelUpChoice,
    AbilityChoice,
    ExpTally,
    Map,
    Shop,
    DemoEnd,
    TouchButtonMapping,   // drag-to-place touch button layout editor
};

enum class MusicCue
{
    None,
    Title,
    Pause,
    Dungeon,
    Forest,
    BossBattle,
    Shop,
    BattleVictory,
    BossVictory,
    GameOver,
};

struct MapNode
{
    int row = 0;
    float normX = 0.5f;
    RoomType type = RoomType::Standard;
    bool completed = false;
    bool available = false;
    std::vector<int> nextNodes;
    Vector2 drawPos{};
};
