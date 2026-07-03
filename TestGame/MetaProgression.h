#pragma once

#include "AbilityType.h"

// ─────────────────────────────────────────────────────────────────────────────
// META PROGRESSION — Mystic Onslaught (Dead Cells style)
//
// Enemies drop Mystic Cells (a separate currency from gold). The player carries
// them during the run; they are BANKED automatically when reaching Zeph's store
// room at the start of each zone. Dying loses all carried (unbanked) cells.
//
// Banked cells are spent at the Legacy Altar (in Zeph's store room) on
// PERMANENT unlocks that persist across runs and deaths:
//   - New abilities added to the run's ability pool (Bolts, Ultimates)
//   - Starting gold, gold retention on death, mana flow, vitality,
//     and a fifth ability slot
//
// Everything persists in metaprogress.cfg next to the executable.
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
    Vitality1,          // +2 max HP at run start
    Vitality2,          // +2 more max HP (total +4)
    FifthAbilitySlot,   // unlock a 5th ability slot
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

    // ── Banked cells ──────────────────────────────────────────────────────────
    int  GetBankedCells() const { return _bankedCells; }
    void BankCells(int amount);

    // ── Unlock purchases (Legacy Altar) ───────────────────────────────────────
    bool IsUnlocked(MetaUnlockType type) const;
    bool CanPurchase(MetaUnlockType type) const;   // affordable + prerequisite owned + not owned
    bool Purchase(MetaUnlockType type);            // deducts cells and saves on success

    // ── Ability pool gating ───────────────────────────────────────────────────
    // Spreads are always available; Bolts and Ultimates need their unlock.
    bool IsAbilityUnlocked(AbilityType type) const;

    // ── Permanent bonus queries (applied by Engine at run start) ─────────────
    int   GetStartingGoldBonus() const;
    float GetGoldRetentionPercent() const;   // 0.0 - 0.6
    float GetManaRegenMultiplier() const;    // 1.0 / 1.5 / 2.0
    int   GetVitalityBonus() const;          // 0 / 2 / 4 max HP
    bool  HasFifthAbilitySlot() const { return IsUnlocked(MetaUnlockType::FifthAbilitySlot); }

    // ── Gold retention across a death ─────────────────────────────────────────
    // Engine sets this when the player dies; the next run start consumes it.
    void SetGoldCarryover(int gold);
    int  TakeGoldCarryover();

    // Total cells banked over the profile's lifetime (stat for the altar screen).
    int GetLifetimeCells() const { return _lifetimeCells; }

private:
    int  _bankedCells   = 0;
    int  _lifetimeCells = 0;
    int  _goldCarryover = 0;
    bool _unlocked[(int)MetaUnlockType::Count] = {};
};
