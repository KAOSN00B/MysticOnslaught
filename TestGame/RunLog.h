#pragma once

#include "AbilityType.h"
#include "DamageSource.h"
#include "GameTypes.h"
#include "Json.h"
#include "PlayerClass.h"
#include "Relic.h"

#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// RUN LOG — one record per finished run (Saves/runlog.mosave), newest first.
//
// What each run was (class, look, Ascension), how it went (domains in order
// with rooms and time, where it ended, kills), the build it ended with, and
// for a death: what killed the player, the statuses on them and the situation
// (the 50 death lessons and difficulty tuning read these later). The totals
// count every run ever logged; the records keep the newest kMaxRuns.
// ─────────────────────────────────────────────────────────────────────────────

enum class RunOutcome { Victory, Death, Abandoned };

struct RunLogDomain
{
    Biome biome = Biome::Forest;
    int   roomsCleared = 0;
    float seconds = 0.f;
    bool  bossDefeated = false;
};

struct RunLogAbility
{
    AbilityType ability = AbilityType::None;
    int level = 1;
};

// The moment of death. Context values the run had no use for stay at their
// "not applicable" defaults (-1 seconds, -1 boss health).
struct RunLogDeath
{
    DamageSource cause;
    bool  burning = false, poisoned = false, chilled = false;
    bool  inEliteCage = false;        // an elite's cage was up
    bool  manaFull = false;
    bool  emptyAbilitySlot = false;
    bool  classResourceFull = false;  // Rage, Faith or combo points at their cap
    bool  curseActive = false;        // a Cursed Cache curse was running
    std::vector<std::string> activeModifiers;   // labels of the run modifiers in effect
    int   roomType = -1;              // RoomType of the room it happened in
    int   roomDepth = -1;             // its distance from the domain entrance
    int   roomSpecial = 0;            // RoomSpecialType of that room
    std::string roomAffix;            // the room's affix name, "" for none
    float secondsInRoom = -1.f;
    float secondsSinceDash = -1.f;
    float bossHealthFraction = -1.f;  // the room's boss, when one was alive
    int   goldCarried = 0;
};

struct RunLogEntry
{
    long long  runId = 0;
    long long  endedAt = 0;           // seconds since 1970-01-01 UTC
    RunOutcome outcome = RunOutcome::Death;
    PlayerClass cls = PlayerClass::Warrior;
    std::string skin;
    int   ascension = 0;
    std::vector<RunLogDomain> domains;   // in the order played; the last is where the run ended
    int   domainReached = 1;             // 1-based
    float seconds = 0.f;
    int   roomsCleared = 0, enemiesKilled = 0, elitesSlain = 0, bossesDefeated = 0;
    int   gold = 0, echoes = 0;          // carried when the run ended
    int   level = 1;
    float maxHealth = 0.f;
    std::vector<RunLogAbility> abilities;
    std::vector<RelicType> relics;       // acquisition order
    std::string ending;                  // the summary line ("Fell to the Minotaur in Ancient Castle")
    bool  hasDeath = false;
    RunLogDeath death;
};

class RunLog
{
public:
    static constexpr int kMaxRuns = 100;

    void Add(const RunLogEntry& entry);   // newest first; counts it in the totals
    const std::vector<RunLogEntry>& Runs() const { return _runs; }
    long long TotalRuns() const { return _totalRuns; }
    long long Wins() const { return _wins; }

    JsonValue ToJson() const;
    // Validates the whole document; fills `out` only when every check passes.
    static bool FromJson(const JsonValue& document, RunLog& out, std::string& error);

private:
    std::vector<RunLogEntry> _runs;
    long long _totalRuns = 0;
    long long _wins = 0;
};

const char* RunOutcomeName(RunOutcome outcome);
