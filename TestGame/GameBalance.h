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

// ── Player base stats (reference defaults; per-class values live in PlayerClass.cpp)
namespace Balance::Player
{
    inline constexpr int   kStartHealth      = 8;
    inline constexpr float kStartAttackPower = 3.f;
    inline constexpr float kStartSpeed       = 380.f;
    inline constexpr int   kStartMana        = 0;
    inline constexpr int   kStartMaxMana     = 10;
}

// ── Player levelling ─────────────────────────────────────────────────────────
namespace Balance::Levelling
{
    inline constexpr int kMaxLevel         = 20;
    inline constexpr int kExpToNextBase    = 15;   // threshold at level 1
    inline constexpr int kExpThresholdStep = 20;   // added each level-up (15->35->55...)
}

// ── Ability damage ───────────────────────────────────────────────────────────
namespace Balance::AbilityDamage
{
    inline constexpr float kSpreadBase     = 1.0f;
    inline constexpr float kSpreadBurnBase = 1.0f;
    inline constexpr float kBoltBase       = 3.0f;   // 3x spread per shot
    inline constexpr float kBoltBurnBase   = 2.0f;
    inline constexpr float kUpgradeBonus   = 0.25f;  // _abilityDamageMultiplier per upgrade
}

// ── Enemy power-level curve ──────────────────────────────────────────────────
// A single global multiplier that advances as the run deepens. Every enemy and
// boss shares it (applied in Enemy::ApplyEnemyPowerLevel), so the whole cast
// toughens together — the roguelite ramp. Per-level growth is deliberately
// gentle so player upgrades stay ahead of the curve on a normal run.
namespace Balance::Curve
{
    inline constexpr int   kRoomsPerPowerLevel = 5;      // power up every N rooms entered
    inline constexpr float kHealthPerLevel     = 0.16f;  // +16% max HP per level above 1
    inline constexpr float kDamagePerLevel     = 0.08f;  // +8%  damage
    inline constexpr float kSpeedPerLevel      = 0.04f;  // +4%  move speed
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

// ── Economy ──────────────────────────────────────────────────────────────────
namespace Balance::Economy
{
    inline constexpr int   kHealDropChancePercent = 8;     // bonus heal-drop base chance
    inline constexpr int   kPotionPrice           = 25;
    inline constexpr int   kRerollBaseCost        = 20;
    inline constexpr int   kRerollStep            = 20;    // added per reroll
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
