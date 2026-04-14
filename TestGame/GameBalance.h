#pragma once

// =============================================================================
// GameBalance.h — centralised design constants for Mystic Onslaught.
//
// Change values here rather than hunting through .cpp files.
// All values are constexpr so the compiler inlines them at zero runtime cost.
// =============================================================================

// ── Player base stats ────────────────────────────────────────────────────────
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
    inline constexpr int   kMaxLevel       = 20;
    inline constexpr int   kExpToNextBase  = 15;   // threshold at level 1
    inline constexpr int   kExpThresholdStep = 20; // added each level-up  (15→35→55…)
}

// ── Ability damage ───────────────────────────────────────────────────────────
namespace Balance::AbilityDamage
{
    inline constexpr float kSpreadBase      = 1.0f;
    inline constexpr float kSpreadBurnBase  = 1.0f;
    inline constexpr float kBoltBase        = 3.0f;   // 3× spread per shot
    inline constexpr float kBoltBurnBase    = 2.0f;
    inline constexpr float kUpgradeBonus    = 0.25f;  // _abilityDamageMultiplier per upgrade
}

// ── Enemy — grunt base stats (tier 0) ────────────────────────────────────────
namespace Balance::Grunt
{
    inline constexpr float kBaseHealth      = 6.f;
    inline constexpr float kHealthPerTier   = 4.f;
    inline constexpr float kBaseAttack      = 1.f;
    inline constexpr float kAttackPerTier   = 0.5f;
    inline constexpr float kBaseSpeed       = 190.f;
    inline constexpr float kSpeedPerTier    = 15.f;
    inline constexpr float kAttackDelayBase = 0.6f;   // decreases each tier
    inline constexpr float kAttackDelayMin  = 0.25f;
    inline constexpr float kAttackDelayStep = 0.06f;  // subtracted per tier
}

// ── Enemy — Cyclops tuning ───────────────────────────────────────────────────
namespace Balance::Cyclops
{
    inline constexpr float kChargeDurationBase     = 1.5f;
    inline constexpr float kChargeDurationMin      = 0.8f;
    inline constexpr float kChargeDurationStep     = 0.12f;  // subtracted per tier
    inline constexpr float kAttackCooldownBase     = 3.5f;
    inline constexpr float kAttackCooldownMin      = 1.8f;
    inline constexpr float kAttackCooldownStep     = 0.28f;
    inline constexpr float kChargeRangeBase        = 480.f;
    inline constexpr float kChargeRangeMax         = 640.f;
    inline constexpr float kChargeRangeStep        = 20.f;   // added per tier
    inline constexpr float kFleeRange              = 220.f;
    inline constexpr float kFleeSpeed              = 110.f;
}

// ── Enemy — Ogre tuning ──────────────────────────────────────────────────────
namespace Balance::Ogre
{
    inline constexpr float kChargeDurationBase      = 3.0f;
    inline constexpr float kChargeDurationMin       = 1.5f;
    inline constexpr float kChargeDurationStep      = 0.25f;
    inline constexpr float kCooldownBase            = 6.0f;
    inline constexpr float kCooldownMin             = 3.0f;
    inline constexpr float kCooldownStep            = 0.5f;
}

// ── Room / act structure ──────────────────────────────────────────────────────
namespace Balance::Rooms
{
    inline constexpr int kRoomsPerAct  = 6;   // 5 normal + 1 boss
    inline constexpr int kRoomsPerTier = 10;  // enemy power advances every 10 rooms entered
}

// ── Wave / enemy power scaling (legacy namespace — use Balance::Rooms) ────────
namespace Balance::Waves
{
    inline constexpr int kWavesPerTier   = 5;  // kept for reference; superseded by kRoomsPerTier
    inline constexpr int kMaxActiveEnemies = 16;
}

// ── Pickup spawn timing ──────────────────────────────────────────────────────
namespace Balance::Pickups
{
    inline constexpr float kDefaultInterval = 60.f;  // seconds between timed drops
    inline constexpr float kBossInterval    = 2.f;   // aggressive cadence during boss
}

// ── Boss support adds ────────────────────────────────────────────────────────
namespace Balance::BossSupport
{
    inline constexpr float kRespawnDelay         = 20.f;  // seconds before a dead support respawns
    inline constexpr float kMinPlayerDistance    = 520.f; // minimum spawn distance from player
}

// ── Timings / miscellaneous ──────────────────────────────────────────────────
namespace Balance::Misc
{
    inline constexpr float kWaveSpawnProtection = 2.f;   // player invulnerability at wave start
    inline constexpr float kBossWarningDuration = 4.f;   // on-screen "DON'T GET TOO CLOSE" timer
}
