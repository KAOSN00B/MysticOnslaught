#pragma once

#include "AbilityType.h"
#include "GameTypes.h"
#include "Json.h"
#include "PlayerClass.h"
#include "Relic.h"
#include "RunLog.h"

#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// RUN CHECKPOINT — the suspended run (Saves/run.mosave) as plain data.
//
// Engine and Character fill a RunCheckpoint at calm moments (EngineRunSave.cpp)
// and apply one on Continue. The file's shape and every limit live here, so
// they are tested without the engine: RunCheckpointFromJson checks types,
// ranges, finite numbers, list sizes, stable names and the room graph before
// anything is applied, and a file that fails is rejected as a whole.
//
// A checkpoint names its run (runId). Continue is offered only while the
// profile still lists that run as active (MetaProgression), so a backup of a
// run that has since ended can never come back.
// ─────────────────────────────────────────────────────────────────────────────

namespace RunCheckpointLimits
{
    // Sizes of enums stored as numbers. EngineRunSave.cpp static_asserts each
    // against the real enum, so adding a value fails the build until this
    // table is updated.
    constexpr int kRoomTypes         = 6;     // RoomType
    constexpr int kEncounterProfiles = 4;     // EncounterProfile
    constexpr int kRoomSpecials      = 8;     // RoomSpecialType::Count
    constexpr int kDoorSides         = 4;     // Engine::DungeonDoorSide (None = -1)
    constexpr int kPathBoons         = 4;     // Engine::PathBoon::Count (that value means "none yet")
    constexpr int kShopWares         = 8;     // ShopWareType::Count
    constexpr int kShopContracts     = 3;     // ShopContractType::Count (that value means "none")
    constexpr int kUpgrades          = 178;   // UpgradeType::Count
    constexpr int kGridSize          = 8;     // DungeonGen::kGridSize

    constexpr int kCrossroadsDoors = 3;
    constexpr int kSpecialOptions  = 3;
    constexpr int kMaxDungeonRooms = kGridSize * kGridSize;
    constexpr int kMaxModifiers    = 16;
    constexpr int kMaxShopItems    = 32;
    constexpr int kMaxRouteEntries = 64;      // domains and map nodes in a run
    constexpr int kMaxAbilitySlots = 8;
    constexpr int kMaxRelics       = 256;
    constexpr int kMaxCharacterStats = 256;

    // Generous on purpose: they reject absurd values, with plenty of room
    // above the x10 HP/damage scale.
    constexpr long long kMaxCount      = 1000000000;   // kills, rooms, gold, Echoes, experience
    constexpr double    kMaxStat       = 1.0e9;        // any saved character number, by magnitude
    constexpr double    kMaxChance     = 1.0;          // probabilities (crit chance)
    constexpr float     kMaxMultiplier = 1.0e6f;
    constexpr float     kMaxSeconds    = 1.0e8f;       // about three years of play
    constexpr float     kMaxCoordinate = 1.0e6f;       // world position in pixels
    constexpr int       kMaxLevel      = 10000;
    constexpr int       kMaxSmallCount = 1000;         // tiers, pending picks, rooms a modifier lasts
    constexpr int       kMaxEnumValue  = 1000;         // elite mechanics and event options
    constexpr long long kMaxPrice      = 1000000000;
    constexpr long long kMaxRunId      = 1LL << 53;    // exact in every JSON reader
}

struct CharacterCheckpoint
{
    struct Slot
    {
        AbilityType ability = AbilityType::None;   // None = empty slot
        int level = 1;
    };
    PlayerClass cls = PlayerClass::Warrior;
    std::string appearance;
    JsonValue stats = JsonValue::MakeObject();     // Character::VisitRunFields name -> number or bool
    std::vector<Slot> abilities;                   // one per ability slot
    std::vector<RelicType> relics;                 // acquisition order, each once
};

struct ModifierCheckpoint
{
    std::string label;
    float enemyHpMult = 1.f, enemyDmgMult = 1.f, goldMult = 1.f, xpMult = 1.f;
    unsigned tintRgba = 0;
    bool  active = false;
    int   roomsRemaining = 1;
    int   shopContract = RunCheckpointLimits::kShopContracts;   // "none"
    bool  curse = false;
};

struct CrossroadsDoorCheckpoint
{
    bool  open = false;
    Biome biome = Biome::Caverns;
    int   boon = 0;
};

struct RoomCheckpoint
{
    int  col = 0, row = 0, type = 0, profile = 0, depth = 0;
    bool north = false, south = false, east = false, west = false;
    bool startsEmpty = false;
    std::string layout;               // authored room id
    bool  hasProgress = false;        // the fields below were saved for this room
    bool  cleared = false, visited = false;
    int   eliteMechanic = -1;
    float holdout = -1.f;
    int   special = 0;
    bool  specialClaimed = false;
    int   specialChoice = -1;
    int   specialOptions[RunCheckpointLimits::kSpecialOptions] = { -1, -1, -1 };
};

struct DungeonCheckpoint
{
    std::vector<RoomCheckpoint> rooms;
    int   start = 0, boss = 0, key = -1, current = 0;
    int   entrySide = -1;             // DungeonDoorSide; -1 = None
    float entryX = 0.f, entryY = 0.f;
    bool  hasMagicGem = false, magicGemSpawned = false, magicGemCollected = false;
    bool  bossBarrierUnlocked = false, chestSpawned = false, chestBroken = false;
};

struct ShopItemCheckpoint
{
    bool isAbility = false, isContract = false, upgradesAbility = false;
    bool purchased = false, reserved = false, unavailable = false;
    int  ware = 0;
    int  contract = RunCheckpointLimits::kShopContracts;   // "none"
    int  upgrade = 0;
    AbilityType ability = AbilityType::None;
    int  price = 0;
};

struct ShopCheckpoint
{
    int  zone = 0;
    int  rerollCost = 0;
    int  dailyDeal = -1;
    bool purchaseMade = false;
    std::vector<ShopItemCheckpoint> items;
};

struct RunCheckpoint
{
    long long runId = 0;
    bool active = false;              // false: the run has ended (only runId is stored)

    CharacterCheckpoint character;

    Biome biome = Biome::Forest;
    int   worldZone = 0;
    std::vector<Biome> completedBiomes;
    std::vector<int>   chosenNodes;
    bool  worldMapGenerated = false;
    std::vector<Biome> sessionCompleted, sessionChosen;
    std::vector<int>   sessionNodes;
    int   ascensionTier = 0;
    bool  ascensionRecorded = false;
    float playerDamageMult = 1.f, enemyHealthMult = 1.f, enemyDamageMult = 1.f;
    int   wagerTier = 0;
    bool  curseShrineUsed = false, wagerAccessGranted = false, secondWind = false;
    int   pendingRelicChoices = 0;
    int   enemiesKilled = 0, bossesDefeated = 0, roomsCleared = 0, elitesSlain = 0;
    float timer = 0.f, gameTimer = 0.f;
    unsigned seed = 0;
    std::vector<ModifierCheckpoint> modifiers;
    std::vector<RunLogDomain> domainLog;   // domains finished this run (the run log's record)
    int   domainStartRooms = 0;            // rooms cleared and run time when the current domain began
    float domainStartSeconds = 0.f;

    bool atWorldMap = false, atCrossroads = false;
    CrossroadsDoorCheckpoint crossroads[RunCheckpointLimits::kCrossroadsDoors];
    bool hasDungeon = false;          // in a dungeon (not at the world map or a crossroads)
    DungeonCheckpoint dungeon;
    bool hasShop = false;             // Zeph's stock for this domain's camp
    ShopCheckpoint shop;
};

JsonValue RunCheckpointToJson(const RunCheckpoint& checkpoint);
// Validates the whole document; fills `out` only when every check passes.
bool RunCheckpointFromJson(const JsonValue& document, RunCheckpoint& out, std::string& error);
