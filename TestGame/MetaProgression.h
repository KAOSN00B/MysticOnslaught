#pragma once

#include "AbilityType.h"
#include "PlayerClass.h"
#include "DamageSource.h"
#include "SaveStore.h"
#include <map>
enum class Biome;
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// META PROGRESSION — Mystic Onslaught (Dead Cells style)
//
// Enemies drop Echoes (internally "cells", a separate currency from gold). The
// player carries them during a run; they are BANKED when the player makes it
// home safely (choosing "Return to Village" after a domain boss, or winning).
// Dying or abandoning a run loses all carried (unbanked) Echoes. Class mastery
// and first-clear milestones bank automatically.
//
// Banked Echoes are spent at Poe's altar on PERMANENT unlocks that persist
// across runs and deaths (ability-pool unlocks, run-start bonuses, utilities).
//
// Full game: Saves/profile.mosave in the per-user data folder (SaveFiles.h),
// a signed JSON document (SaveStore.h, docs/SAVE_FORMAT.md).
// ─────────────────────────────────────────────────────────────────────────────

enum class MetaUnlockType
{
    // ── Ability unlocks — add the ability to the run's offer pools ────────────
    UnlockFireBolt,
    UnlockIceBolt,
    UnlockElectricBolt,
    UnlockFireUltimate,
    UnlockIceUltimate,
    UnlockElectricUltimate,
    // ── Permanent run bonuses (tiered ones require the previous tier) ─────────
    StartingGold1,      // +30 starting gold
    StartingGold2,      // +30 more (total +60)
    StartingGold3,      // +60 more (total +120)
    GoldRetention1,     // keep 20% of gold when you die
    GoldRetention2,     // keep 40%
    GoldRetention3,     // keep 60%
    ManaFlow1,          // +50% mana regeneration
    ManaFlow2,          // +100% mana regeneration
    Vitality1,          // +20% max HP at run start
    Vitality2,          // +35% max HP at run start (replaces Vitality I)
    // Indices 16/17 were "5th/6th ability slot". The kit is fixed at three
    // abilities + one ultimate, so those unlocks never worked; the save keys
    // (unlock_16/unlock_17) now grant working, choice-expanding effects.
    FreeShopReroll,     // Zeph's first reroll each visit is free
    // ── Deeper / transformative unlocks ──────────────────────────────────────
    BonusStarterAbility,// begin each run with a second class ability (needs FreeShopReroll)
    Bulwark,            // +10 Defense at run start
    Heirloom,           // start each run with a random relic
    CellSurge,          // +50% Mystic Cells from kills
    SecondWind,         // revive once per run at 40% HP
    Cartographer,       // dungeon map reveals every room's type without visiting
    Count               // keep last
};

struct MetaUnlockInfo
{
    const char*    name;
    const char*    description;
    int            cost;          // banked cells required
    MetaUnlockType prerequisite;  // must own this first (Count = none)
};

// Static info for every unlock, indexed by (int)MetaUnlockType.
const MetaUnlockInfo& GetMetaUnlockInfo(MetaUnlockType type);

class MetaProgressionManager
{
public:
    void Load();
    void Save() const;
    // True once the player has anything worth continuing (drives Continue).
    bool HasAnyProgress() const;
    // New Game over an existing profile: back to a fresh profile and save.
    // (The previous generation stays in the .bak until the next save.)
    void ResetProfile();
    // How the last Load went, and why a file was rejected (empty when fine).
    SaveLoadResult GetLoadResult() const { return _loadResult; }
    const std::string& GetLoadProblem() const { return _loadProblem; }
    const std::string& GetLastSaveProblem() const { return _lastSaveProblem; }

    // The profile document (stable names, see SaveIds.h). FromJson validates
    // everything and only then fills `out`; false (with the reason) otherwise.
    JsonValue ToJson() const;
    static bool FromJson(const JsonValue& document, MetaProgressionManager& out, std::string& error);
    // Ordinary room clears defer their disk write (a flush on the clear frame
    // hitches); call at calm moments such as village arrival or shutdown.
    void SaveIfDirty() const { if (_dirty) Save(); }

    // ── Banked cells ──────────────────────────────────────────────────────────
    int  GetBankedCells() const { return _bankedCells; }
    void BankCells(int amount);

    // ── Unlock purchases (Poe's Altar) ───────────────────────────────────────
    bool IsUnlocked(MetaUnlockType type) const;
    bool CanPurchase(MetaUnlockType type) const;   // affordable + prerequisite owned + not owned
    bool Purchase(MetaUnlockType type);            // deducts cells and saves on success

    // ── Ability pool gating ───────────────────────────────────────────────────
    // Spreads are always available; Bolts and Ultimates need their unlock.
    bool IsAbilityUnlocked(AbilityType type) const;

    // ── Permanent bonus queries (applied by Engine at run start) ─────────────
    int   GetStartingGoldBonus() const;
    float GetGoldRetentionPercent() const;   // 0.0 - 0.6
    // Gold a death leaves in the wallet under Deep Pockets (rounded down).
    int   GetGoldKeptOnDeath(int carriedGold) const;
    float GetManaRegenMultiplier() const;    // 1.0 / 1.5 / 2.0
    // Max HP added at run start: +20% / +35% of the class's base max HP.
    static constexpr int kVitalityPercent1 = 20;
    static constexpr int kVitalityPercent2 = 35;
    int   GetVitalityBonus(int baseMaxHealth) const;
    // Echoes a death keeps (Poe's salvage). Banking at a camp keeps them all.
    static constexpr int kEchoSalvagePercent = 50;
    static int GetEchoesKeptOnDeath(int carriedEchoes);
    bool  HasFreeShopReroll() const { return IsUnlocked(MetaUnlockType::FreeShopReroll); }
    bool  HasBonusStarterAbility() const { return IsUnlocked(MetaUnlockType::BonusStarterAbility); }
    float GetStartingDefenseBonus() const;
    bool  HasStartingRelic() const { return IsUnlocked(MetaUnlockType::Heirloom); }
    float GetCellGainMultiplier() const { return IsUnlocked(MetaUnlockType::CellSurge) ? 1.5f : 1.0f; }
    bool  HasSecondWind() const { return IsUnlocked(MetaUnlockType::SecondWind); }
    bool  HasCartographer() const { return IsUnlocked(MetaUnlockType::Cartographer); }

    // ── Runs (mid-run save) ───────────────────────────────────────────────────
    // Every main run gets an id, and the run checkpoint names it. Continue is
    // offered only while the profile still lists that run as active, so a
    // checkpoint restored from a backup after its run ended can't revive it.
    // Ids keep counting up through New Game.
    static constexpr long long kMaxRunId = 1LL << 53;   // exact in every JSON reader
    long long BeginRun();                    // a new id, now the active run; saves
    void EndActiveRun();                     // no run is active any more; saves
    long long GetActiveRunId() const { return _activeRunId; }

    // ── Deaths (lifetime; the death lessons read these) ───────────────────────
    // Counted by cause ("enemy/Ogre", "status/Burn", "hazard/LavaPool") and
    // by domain. Causes beyond kMaxDeathCauses count only in the total.
    static constexpr int kMaxDeathCauses = 256;
    void RecordDeath(const DamageSource& cause, Biome biome);   // saves
    static std::string DeathCauseKey(const DamageSource& cause);
    int GetDeathCount() const { return _deathTotal; }
    int GetDeathsByCause(const std::string& key) const;
    int GetDeathsInDomain(Biome biome) const;

    // ── Village wallet ────────────────────────────────────────────────────────
    // Gold the player holds while NOT carrying it through a run (in the village,
    // or after a death/victory). Stored under the legacy "gold_carryover" key, so
    // older profiles keep any carryover they had. Engine writes it on village
    // arrival / village spending and zeroes it when a run takes the gold out.
    int  GetVillageWallet() const { return _goldCarryover; }
    void SetVillageWallet(int gold);

    // ── Village layout (player-placed buildings) ──────────────────────────────
    // Stored in the SAME profile file as the wallet so one atomic save always
    // records a building and the gold paid/refunded for it together: a failed
    // write keeps the previous consistent pair (backup + checksum), never half.
    struct VillagePlacement
    {
        std::string asset;   // .vasset object name (letters, digits, _ - . only)
        int x = 0, y = 0;    // world origin in pixels
    };
    static constexpr int kMaxVillagePlacements = 256;
    static constexpr int kMaxVillageCoordinate = 100000;   // world pixels on either axis
    bool HasVillageLayout() const { return _hasVillageLayout; }
    const std::vector<VillagePlacement>& GetVillagePlacements() const { return _villagePlacements; }
    // Replaces the layout and wallet together and saves once.
    void SetVillageState(const std::vector<VillagePlacement>& placements, int wallet);
    static bool IsValidVillageAssetName(const std::string& name);

    // ── Time of day (village.clock) ───────────────────────────────────────────
    // The hour (0-24) the game's clock was at. Absent on a new profile, so the
    // engine starts at its own default hour. Setting it never saves on its own:
    // the hour goes out with the next calm save, in quarter-hour steps so the
    // running clock does not mark the profile changed every frame.
    static constexpr float kClockSaveStepHours = 0.25f;
    bool  HasClock() const { return _hasClock; }
    float GetClockHours() const { return _clockHours; }
    void  SetClockHours(float hours);

    // -- Armory: chosen starting ability per class ----------------------------
    // Stored as the AbilityType value (-1 / None = the class default). Engine
    // validates the choice against the class kit and mastery rank at run start.
    AbilityType GetStarterChoice(PlayerClass cls) const;
    void SetStarterChoice(PlayerClass cls, AbilityType ability);

    // -- Village unlock announcements (each shown once) -----------------------
    enum VillageUnlockBit : unsigned { kSeenTrophyHall = 1u, kSeenArmory = 2u };
    bool HasSeenVillageUnlock(unsigned bit) const { return (_villageSeen & bit) != 0; }

    // ── Grimoire: what this profile has found and earned ─────────────────────
    // Indexed by RelicType, Bestiary entry and Feat. Marking only flags the
    // profile dirty; it is written at the next calm save.
    static constexpr int kMaxGrimoireEntries = 128;
    void MarkRelicFound(int index)          { MarkFlag(_relicFound, index); }
    bool HasFoundRelic(int index) const     { return GetFlag(_relicFound, index); }
    int  FoundRelicCount() const            { return CountFlags(_relicFound); }
    void MarkCreatureFound(int index)       { MarkFlag(_creatureFound, index); }
    bool HasFoundCreature(int index) const  { return GetFlag(_creatureFound, index); }
    int  FoundCreatureCount() const         { return CountFlags(_creatureFound); }
    bool EarnFeat(int index)                { if (GetFlag(_featEarned, index)) return false; MarkFlag(_featEarned, index); return GetFlag(_featEarned, index); }
    bool HasFeat(int index) const           { return GetFlag(_featEarned, index); }
    int  FeatCount() const                  { return CountFlags(_featEarned); }
    void MarkVillageUnlockSeen(unsigned bit);

    // Total cells banked over the profile's lifetime (stat for the altar screen).
    int GetLifetimeCells() const { return _lifetimeCells; }

    // ── Ascension (difficulty tiers unlocked by winning) ─────────────────────
    static constexpr int kMaxAscension = 6;
    int  GetSelectedAscension() const { return _selectedAscension; }
    int  GetMaxAscensionUnlocked() const { return _maxAscensionUnlocked; }
    void SetSelectedAscension(int tier);          // clamps to [0, maxUnlocked]; saves
    void RecordAscensionCleared(int tier);        // unlocks tier+1 on a win; saves

    // Story onboarding and class roster progression. Missing keys in older
    // profile files load as false, preserving backwards compatibility.
    bool HasCompletedOnboarding() const { return _onboardingComplete; }
    void SetOnboardingComplete(bool complete = true);
    bool IsRogueUnlocked() const { return _rogueUnlocked; }
    bool IsWarlockUnlocked() const { return _warlockUnlocked; }
    bool HasCompletedGame() const { return _gameCompleted; }
    // Paladin: win a run, or clear kPaladinDomainMilestone different domains.
    bool IsPaladinUnlocked() const { return _paladinUnlocked || _gameCompleted; }
    static constexpr int kPaladinDomainMilestone = 5;
    void SetRogueUnlocked(bool unlocked = true);
    void SetWarlockUnlocked(bool unlocked = true);
    void RecordGameCompleted();

    // Class roster milestones. Both derive from data every profile already
    // tracks, so a profile that earned them before the milestones existed
    // unlocks the class on its next load:
    //   Rogue   — defeat any domain boss (any class).
    //   Warlock — clear kWarlockDomainMilestone different domains (any runs).
    static constexpr int kWarlockDomainMilestone = 3;
    int GetTotalBossesDefeated() const;
    int GetClearedBiomeCount() const;
    // Returns a bit mask (1 << (int)PlayerClass) of classes newly unlocked
    // since the last call, for the "new class" announcement. Consumes it.
    unsigned TakeNewClassUnlocks();

    // Call once when an uncleared combat room/boss is actually defeated.
    // Returns milestone cells automatically deposited into the altar bank.
    static constexpr int kMasteryRanks = 5;
    static constexpr int kBiomeCount = 10;
    static constexpr int kMaxCurrency = 1000000000;
    static constexpr int kMaxRecordedClears = 1000000;
    int RecordRoomCleared(PlayerClass cls);
    int RecordBossDefeated(PlayerClass cls, Biome biome);
    int GetClassRooms(PlayerClass cls) const;
    int GetClassBosses(PlayerClass cls) const;
    int GetClassMasteryTier(PlayerClass cls) const;
    int GetClassMasteryProgress(PlayerClass cls) const; // cumulative points, capped at 100
    int GetClassMasteryTarget(PlayerClass cls) const;   // next rank threshold, 100 at maximum
    bool HasClearedBiome(Biome biome) const;
    static int GetMasteryReward(int rank);             // ranks 1..5
    static int GetMasteryThreshold(int rank);          // cumulative points for ranks 1..5; 0 for rank 0
    static int GetBiomeFirstClearReward(Biome biome);
    bool DidLastSaveSucceed() const { return _lastSaveSucceeded; }

private:
    int AwardMastery(PlayerClass cls);
    void AddBankedReward(int amount);
    // Applies the Rogue/Warlock milestones; returns the newly unlocked mask.
    unsigned EvaluateClassMilestones();
    unsigned _newClassUnlocks = 0;
    bool _hasVillageLayout = false;
    std::vector<VillagePlacement> _villagePlacements;
    bool  _hasClock = false;
    float _clockHours = 0.f;
    int  _starterChoice[(int)PlayerClass::Count] = { -1, -1, -1, -1, -1, -1 };
    unsigned _villageSeen = 0;
    bool _relicFound[kMaxGrimoireEntries] = {};
    bool _creatureFound[kMaxGrimoireEntries] = {};
    bool _featEarned[kMaxGrimoireEntries] = {};
    void MarkFlag(bool* flags, int index)
    {
        if (index < 0 || index >= kMaxGrimoireEntries || flags[index]) return;
        flags[index] = true;
        _dirty = true;
    }
    static bool GetFlag(const bool* flags, int index) { return index >= 0 && index < kMaxGrimoireEntries && flags[index]; }
    static int  CountFlags(const bool* flags) { int n = 0; for (int i = 0; i < kMaxGrimoireEntries; ++i) n += flags[i] ? 1 : 0; return n; }
    int _classRooms[(int)PlayerClass::Count] = {};
    int _classBosses[(int)PlayerClass::Count] = {};
    int _classRank[(int)PlayerClass::Count] = {};
    bool _biomeCleared[kBiomeCount] = {};
    mutable bool _lastSaveSucceeded = true;
    mutable std::string _lastSaveProblem;
    SaveLoadResult _loadResult = SaveLoadResult::Missing;
    std::string _loadProblem;
    mutable bool _dirty = false;
    int _deathTotal = 0;
    std::map<std::string, int> _deathsByCause;
    int _deathsByDomain[kBiomeCount] = {};
    long long _nextRunId   = 1;
    long long _activeRunId = 0;
    int  _bankedCells   = 0;
    int  _lifetimeCells = 0;
    int  _goldCarryover = 0;
    bool _unlocked[(int)MetaUnlockType::Count] = {};
    int  _selectedAscension    = 0;   // difficulty chosen for the next run
    int  _maxAscensionUnlocked = 0;   // highest tier the player may select
    bool _onboardingComplete = false;
    bool _rogueUnlocked = false;
    bool _warlockUnlocked = false;
    bool _paladinUnlocked = false;
    bool _gameCompleted = false;
};
