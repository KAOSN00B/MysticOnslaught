#pragma once

#include "GameTypes.h"

#include <string>

// The Grimoire: what a profile has found (relics, creatures) and earned
// (feats). The tables here are pure data so they can be tested on their own;
// the screen and the discovery hooks are in EngineGrimoire.cpp.

struct BestiaryEntry
{
    const char* type;    // Engine::GetDungeonSnapshotType base name ("SkeletonArcher")
    const char* name;    // shown name ("Skeleton Archer")
    const char* role;    // "Ranged", "Elite", "Forest guardian"
    const char* lore;    // one short, honest line about how it fights
    bool        boss;
    Biome       home;    // bosses: the domain they guard
};

int                  BestiaryCount();
const BestiaryEntry& GetBestiaryEntry(int index);
// Index for an enemy's snapshot type ("Elite_Infernal", "SlimeBig" ...), or -1.
int                  BestiaryIndexForType(const std::string& snapshotType);

enum class Feat
{
    FirstBlood,      // defeat a boss
    Pathfinder,      // reach the third domain in a run
    FinalDescent,    // reach Demon's Insides
    Conqueror,       // win a run
    Ascendant,       // win a run on Ascension 1+
    SwiftVictory,    // win a run in under 45 minutes
    Untouched,       // clear a boss room without taking damage
    EliteHunter,     // slay 3 elites in one run
    Exterminator,    // slay 250 enemies in one run
    Collector,       // hold 8 relics at once
    Hoarder,         // carry 1,000 gold at once
    CurseBreaker,    // outlast a Cursed Cache's curse
    Reshaped,        // take a Mutation Altar's gift
    EchoKeeper,      // bank 1,000 Echoes over all runs
    BeastScholar,    // record 15 creatures in the Bestiary
    RelicSage,       // record 40 relics in the Grimoire
    Count
};

struct FeatInfo
{
    const char* name;
    const char* description;
};

const FeatInfo& GetFeatInfo(Feat feat);

// Thresholds the feats test against (shared by the game and the tests).
inline constexpr float kSwiftVictorySeconds   = 45.f * 60.f;
inline constexpr int   kEliteHunterKills      = 3;
inline constexpr int   kExterminatorKills     = 250;
inline constexpr int   kCollectorRelics       = 8;
inline constexpr int   kHoarderGold           = 1000;
inline constexpr int   kEchoKeeperBanked      = 1000;
inline constexpr int   kBeastScholarCreatures = 15;
inline constexpr int   kRelicSageRelics       = 40;
