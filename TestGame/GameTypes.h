#pragma once

#include "raylib.h"

#include <vector>

enum class Biome { AncientCastle, Caverns, DemonsInsides, DreamRealm, Forest, Graveyard, Jungle, LostCity, TheSanctuary, Wastelands };

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
    TreasureChest,  // treasure room chest — mixed upgrade + ability cards
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
    DungeonRun,           // active procedural dungeon run
    WorldMap,             // biome selection map shown after each boss clear
    MetaShop,             // Legacy Altar — spend banked Mystic Cells on permanent unlocks
    Settings,             // settings screen (display, audio)
    TileMapper,           // interactive tilesheet assignment tool (debug)
    NineSliceEditor,      // visual srcCorner/dstCorner editor for any PNG asset
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
