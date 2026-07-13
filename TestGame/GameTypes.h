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

// Decision-room "special" layered onto a Standard room (no new RoomType — the
// generator tags a Standard room's DungeonRoomState with one of these). None =
// an ordinary combat room. See ROOM_EVENTS_INTEGRATION_PLAN.txt.
enum class RoomSpecialType
{
    None = 0,
    RiskShrine,     // pick a contract: danger later for reward later
    CursedReward,   // take a reward now, accept a curse later
    OmenProphecy,   // choose a modifier for the next room
    LockedArmory,   // pay to open cases
    MutationAltar,  // reshape the build with a drawback
    RelicForge,     // salvage / reroll / overcharge relics
    Count,
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
    ClassSelect,          // pick a class at the start of a run
    MetaShop,             // Poe's Altar — spend banked Mystic Cells on permanent unlocks
    CurseShrine,          // risk/reward: pick a blessing paired with a curse
    DecisionRoom,         // mid-dungeon decision room (Risk Shrine, etc.) card pick
    RelicChoice,          // pick 1 of 3 relics after an elite/boss kill
    Bestiary,             // enemy kill-count catalogue
    Settings,             // settings screen (display, audio)
    TileMapper,           // interactive tilesheet assignment tool (debug)
    NineSliceEditor,      // visual srcCorner/dstCorner editor for any PNG asset
    CharacterAnimator,    // enemy hitbox + animation tuning tool (debug)
    AttackEditor,         // player/boss attack FX + hitbox editor (debug)
    MapEditor,            // layered village/interior tile-map painter (debug)
    VillagePlayground,    // playable village-builder placement test (debug)
    Village,              // main-game hub village: build with gold, respawn at Poe's Graveyard
    DeathRevive,          // Zelda-style death beat + Poe's revive dialogue -> class/look select
    BossChoice,           // after every boss: return to village vs push onward (double-or-nothing)
    CosmeticShop,         // village Mirror/Tailor: change the player's look (skin), gold sink later
};

enum class MusicCue
{
    None,
    Title,
    Pause,
    Dungeon,       // ruins/underground biomes (Caverns, Lost City, Wastelands)
    Forest,        // nature biomes (Forest, Jungle)
    BiomeDark,     // dark/evil biomes (Demon's Insides, Graveyard)
    BiomeGrand,    // grand/holy biomes (Ancient Castle, The Sanctuary)
    BiomeEthereal, // dreamlike biomes (Dream Realm)
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
