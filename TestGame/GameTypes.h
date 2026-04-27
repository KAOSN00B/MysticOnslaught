#pragma once

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
