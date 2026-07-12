#pragma once

// =============================================================================
// GameBalance.h — THE single source of truth for Mystic Onslaught's tuning.
//
// Target feel (chosen 2026-07-06): "Moderate / Hades-like" — the base run has
// real teeth and a first clear is an achievement, but it's fair. All *extra*
// difficulty lives in the Ascension ladder (see Ascension.h), NOT here.
//
// RULE: gameplay code must READ these constants, never hardcode the number
// beside them. If you find yourself typing a balance literal in a .cpp, add it
// here instead. Everything is constexpr, so it inlines at zero runtime cost.
//
// Consumed by: Enemy.cpp (grunt base + power curve), every boss's SetWaveScale
// (HP), Engine.cpp (power-level rate, drop chance, spawn caps), ShopManager.cpp
// (prices). Included via Enemy.h and Engine.h so it reaches all of them.
// =============================================================================

// ── Player base stats ────────────────────────────────────────────────────────
// Per-class base stats live in PlayerClass.cpp (kClassTable) — classes differ,
// so a single shared constant here would lie. A Balance::Player block used to
// sit here but was never read by any code; it was removed in the balance truth
// pass so nobody tunes dead numbers.

// ── Player levelling ─────────────────────────────────────────────────────────
namespace Balance::Levelling
{
    // Level 1 is the run start, so level 9 represents eight Power Choices.
    // The full-run target is 6-8 choices; room-clear XP below makes enemy count
    // irrelevant to that pacing.
    inline constexpr int kMaxLevel         = 9;
    inline constexpr int kExpToNextBase    = 20;  // level 1 -> 2
    inline constexpr int kExpThresholdStep = 8;   // 20, 28, 36, 44, ...

    inline constexpr int ExpToNextForLevel(int currentLevel)
    {
        return kExpToNextBase + (currentLevel - 1) * kExpThresholdStep;
    }

    // Dungeon-run XP is awarded once when a combat room is cleared. These
    // values produce roughly 90 XP per six-room act before optional contracts.
    inline constexpr int kStandardRoomClearExp = 12;
    inline constexpr int kEliteRoomClearExp    = 18;
    inline constexpr int kBossRoomClearExp     = 30;
}

// ── Ability damage ───────────────────────────────────────────────────────────
namespace Balance::AbilityDamage
{
    inline constexpr float kSpreadBase     = 1.0f;
    inline constexpr float kSpreadBurnBase = 1.0f;
    inline constexpr float kBoltBase       = 3.0f;   // 3x spread per shot
    inline constexpr float kBoltBurnBase   = 2.0f;
    inline constexpr float kUpgradeBonus   = 0.25f;  // _abilityDamageMultiplier per upgrade

    // Class-ability damage per ability level above 1 (Engine dmgVal). Level 3
    // = 1.5x base. The pre-rework formula was a raw x2/x3 level multiplier —
    // the single biggest damage snowball in the run.
    inline constexpr float kClassLevelStep = 0.25f;
}

// ── Enemy power-level curve ──────────────────────────────────────────────────
// A single global multiplier that advances as the run deepens. Every enemy and
// boss shares it (applied in Enemy::ApplyEnemyPowerLevel), so the whole cast
// toughens together — the roguelite ramp. Per-level growth is deliberately
// gentle so player upgrades stay ahead of the curve on a normal run.
namespace Balance::Curve
{
    inline constexpr int   kRoomsPerPowerLevel = 5;      // power up every N rooms entered
    // Balance rework: HP ramp softened 0.16 -> 0.12 because the player no
    // longer auto-gains +0.4 attack per level — deep enemies should press
    // harder (cadence below), not just take longer to kill.
    inline constexpr float kHealthPerLevel     = 0.12f;  // +12% max HP per level above 1
    inline constexpr float kDamagePerLevel     = 0.08f;  // +8%  damage
    inline constexpr float kSpeedPerLevel      = 0.04f;  // +4%  move speed

    // Attack cadence by depth — the new pressure lever. Enemies attack more
    // often as the run deepens (delay shrinks 5%/level), floored so no enemy
    // ever attacks faster than 65% of its authored gap.
    inline constexpr float kAttackDelayPerLevel  = 0.05f;
    inline constexpr float kMinAttackDelayFactor = 0.65f;
}

// ── Grunt (basic enemy) base profile at power level 1 ────────────────────────
namespace Balance::Grunt
{
    inline constexpr int   kBaseExpValue    = 3;
    inline constexpr float kBaseHealth      = 4.f;
    inline constexpr float kBaseAttack      = 1.f;
    inline constexpr float kBaseSpeed       = 185.f;
    inline constexpr float kBaseAttackDelay = 1.5f;
}

// ── Elite minibosses (Ogre / Cyclops) base HP at power level 1 ───────────────
namespace Balance::Elite
{
    inline constexpr float kOgreHealth    = 10.f;
    inline constexpr float kCyclopsHealth = 7.f;
}

// ── Bosses — unified role model ──────────────────────────────────────────────
// Every boss HP derives from ONE budget x a role multiplier, so the cast forms a
// smooth spread instead of ad-hoc numbers. Bosses are then power-scaled by the
// curve above at the depth they actually spawn, so a tanky boss met late is
// genuinely tanky. To make ALL bosses tankier/squishier, change kBaseHealth.
//   First boss (Caverns) is a gentle intro; the final boss (Demon's Insides) is
//   the wall. The eight middle bosses can appear at any zone 1-4, so they share
//   the mid budget and differ only by combat role.
namespace Balance::Boss
{
    inline constexpr float kBaseHealth = 46.f;   // "standard mid-boss" budget

    // Role multipliers.
    inline constexpr float kRoleFirst    = 0.55f;  // tutorial first boss
    inline constexpr float kRoleGlass    = 0.80f;  // fast / fragile
    inline constexpr float kRoleFast     = 0.85f;
    inline constexpr float kRoleCaster   = 0.95f;
    inline constexpr float kRoleStandard = 1.00f;
    inline constexpr float kRoleHeavy    = 1.15f;  // hard-hitting bruiser
    inline constexpr float kRoleTank     = 1.25f;
    inline constexpr float kRoleFinal    = 1.55f;  // final boss

    // Per-boss resolved HP (base x role). Bosses read THESE in SetWaveScale.
    inline constexpr float kMolarbeastHealth  = kBaseHealth * kRoleFirst;    // ~25
    inline constexpr float kChompBugHealth    = kBaseHealth * kRoleGlass;    // ~37
    inline constexpr float kWerewolfHealth    = kBaseHealth * kRoleFast;     // ~39
    inline constexpr float kOsirisHealth      = kBaseHealth * kRoleCaster;   // ~44
    inline constexpr float kPumpkinJackHealth = kBaseHealth * kRoleStandard; // ~46
    inline constexpr float kToxicVerminHealth = kBaseHealth * kRoleStandard; // ~46
    inline constexpr float kAncientBearHealth = kBaseHealth * kRoleHeavy;    // ~53
    inline constexpr float kMinotaurHealth    = kBaseHealth * kRoleTank;     // ~58
    inline constexpr float kTitanGuardHealth  = kBaseHealth * kRoleTank;     // ~58
    inline constexpr float kAbyssSlimeHealth  = kBaseHealth * kRoleFinal;    // ~71
}

// ── Sustain ──────────────────────────────────────────────────────────────────
// The roguelite pressure model: HP is a resource that carries BETWEEN rooms.
// Entering a room no longer heals at all; recovery comes only from pickups,
// Zeph, abilities/relics — and all repeatable sources are capped per room so
// damage growth can't buy unlimited healing.
namespace Balance::Sustain
{
    inline constexpr float kBossClearHealFraction = 0.40f;  // partial breath after a boss, not a reset
    inline constexpr int   kMaxHealDropsPerRoom   = 2;      // chance + timed heal pickups combined
    inline constexpr int   kLifestealCapPerRoom   = 6;      // HP returned by lifesteal per room
    inline constexpr float kManaRegenPerSecond    = 0.25f;  // base regen (was 0.2) — compensates the
                                                            // removed +5 mana per level
}

// ── Economy ──────────────────────────────────────────────────────────────────
namespace Balance::Economy
{
    inline constexpr int   kHealDropChancePercent = 8;     // bonus heal-drop base chance
    inline constexpr int   kRerollBaseCost        = 45;
    inline constexpr int   kRerollStep            = 35;    // added per reroll
    inline constexpr int   kReserveStockCost      = 15;    // lock one card through rerolls
    inline constexpr float kActPriceInflation     = 0.25f; // +25% shop prices per act
}

// ── Room / spawn structure ───────────────────────────────────────────────────
namespace Balance::Rooms
{
    inline constexpr int kRoomsPerAct      = 6;   // 5 normal + 1 boss
    inline constexpr int kMaxActiveEnemies = 16;
}

// ── Pickup spawn timing ──────────────────────────────────────────────────────
namespace Balance::Pickups
{
    inline constexpr float kDefaultInterval = 60.f;  // seconds between timed drops
    inline constexpr float kBossInterval    = 2.f;
}

// ── Boss support adds ────────────────────────────────────────────────────────
namespace Balance::BossSupport
{
    inline constexpr float kRespawnDelay      = 20.f;
    inline constexpr float kMinPlayerDistance = 520.f;
}

// ── Game feel / juice ────────────────────────────────────────────────────────
namespace Balance::Feel
{
    // Damage is single-digit internally; floating combat numbers are multiplied
    // by this for display only (balance untouched). Bump for bigger, juicier
    // numbers. 25 turns an 8-damage hit into "200".
    inline constexpr int kDamageNumberScale = 25;
}

// ── Timings / miscellaneous ──────────────────────────────────────────────────
namespace Balance::Misc
{
    inline constexpr float kWaveSpawnProtection = 2.f;
    inline constexpr float kBossWarningDuration = 4.f;
}
