#pragma once

// =============================================================================
// GameBalance.h — THE single source of truth for Mystic Onslaught's tuning.
//
// Target feel (chosen 2026-07-06): "Moderate / Hades-like" — the base run has
// real teeth and a first clear is an achievement, but it's fair. All *extra*
// difficulty lives in the Ascension ladder (see Ascension.h), NOT here.
//
// NUMBER SCALE (2026-10-18): HP, damage and healing were multiplied by 10 so
// hits have room for crits and stat-based tuning (heroes 60-100 HP, opening
// hits ~20-40, late hits into the high hundreds). Multipliers, chances,
// durations, XP and prices did not change.
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

// ── Player stats: Defense, crits and the stat cards ──────────────────────────
// PLACEHOLDERS (2026-10-18) until the gear/formula pass. Defense replaced the
// old armour pips: a hit is multiplied by K / (K + Defense), the reduction is
// capped so a stacked build still takes real damage, and a landed hit always
// deals at least 1 (DamagePolicy.h). Defense applies to hits only; burn,
// poison and other ticks ignore it. Every player hit rolls the crit chance and
// a crit adds the crit damage on top.
namespace Balance::Stats
{
    inline constexpr float kDefenseK            = 60.f;    // was 100: a 10-damage grunt hit lost ~1 at Defense 10
    inline constexpr float kMaxDefenseReduction = 0.60f;   // reached at 90 Defense
    inline constexpr int   kMinHitDamage        = 1;

    inline constexpr float kBaseCritChance    = 0.05f;
    inline constexpr float kBaseCritDamage    = 0.50f;     // a crit deals +50%
    inline constexpr float kMaxCritChance     = 1.f;
    inline constexpr float kDeadeyeCritChance = 0.20f;     // Deadeye relic

    // Defense sources. The old pips converted as: one spent pip -> minor; two
    // pips or a permanent armour slot -> major.
    inline constexpr float kClassStartDefense = 10.f;      // Warrior, Paladin
    inline constexpr float kDefenseMinor      = 5.f;       // Defense card, Unbreakable, Survivalist, Iron Case
    inline constexpr float kDefenseMajor      = 10.f;      // Iron Skin, Juggernaut, Sanctuary, Stone Skin,
                                                           // Bulwark, Fortify, Iron Hide, Arcane Graft, Ironroot Tonic
    inline constexpr float kIroncladDefensePerRoom = 3.f;
    inline constexpr float kIroncladDefenseMax     = 15.f;
    inline constexpr float kRallyDefense        = 20.f;    // War Cry / Shield of Faith, timed
    inline constexpr float kRallyDefenseSeconds = 6.f;

    // Stat cards.
    inline constexpr float kAttackCard     = 5.f;
    inline constexpr float kMaxHealthCard  = 20.f;
    inline constexpr float kSpeedCard      = 6.f;          // movement grows only a little
    inline constexpr float kCritChanceCard = 0.03f;
    inline constexpr float kCritDamageCard = 0.15f;
}

// ── Ability damage ───────────────────────────────────────────────────────────
namespace Balance::AbilityDamage
{
    inline constexpr float kSpreadBase     = 10.0f;
    inline constexpr float kSpreadBurnBase = 10.0f;
    inline constexpr float kBoltBase       = 50.0f;
    inline constexpr float kUltimateBase   = 60.0f;
    // Instant Mage spreads (Frost Nova, lightning dash) hit once, so they deal
    // double spread damage; Flame Wall keeps x1 because it ticks ~11 times.
    inline constexpr int   kInstantSpreadMult = 2;
    inline constexpr float kBoltBurnBase   = 20.0f;
    inline constexpr int   kLevelHitBonus  = 10;     // elemental spell damage per ability level above 1
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
// OPENING RULE: no class kills a standard enemy with one basic attack at the
// start of a run. The strongest opening basic hit is the Warrior's 40 (see
// PlayerClass.cpp) and the strongest opening ability hit is 50 (see
// ClassAbilityBalance.h), so every standard enemy starts at 60+ HP. That also
// covers a swing on a Cleave-vulnerable target (40 x 1.25 = 50).
// OpeningBalanceTests.cpp enforces this against the class table.
// Deliberate one-hit pops are exempt: Bomber Imp (explodes on death) and the
// small slimes a big slime splits into.
namespace Balance::PlayerBasic
{
    // Ranged basics (Mage / Hunter / Warlock) deal this fraction of attack power:
    // 2/3 of the opening attack 30 is a 20-damage shot.
    inline constexpr float kRangedScale = 2.f / 3.f;
}

namespace Balance::Grunt
{
    inline constexpr int   kBaseExpValue    = 3;
    inline constexpr float kBaseHealth      = 60.f;
    inline constexpr float kBaseAttack      = 10.f;
    inline constexpr float kBaseSpeed       = 185.f;
    inline constexpr float kBaseAttackDelay = 1.5f;
}

// ── Standard enemy HP at power level 1 (the ladder above the grunt) ──────────
namespace Balance::Fodder
{
    inline constexpr float kFragileHealth      = 60.f;  // archers, phantoms, wisps, blades
    inline constexpr float kSporelingHealth    = 60.f;
    inline constexpr float kBigSlimeHealth     = 60.f;
    inline constexpr float kSmallSlimeHealth   = 10.f;  // split fragment (exempt)
    inline constexpr float kShieldbearerHealth = 80.f;  // tank
    inline constexpr float kWarchiefHealth     = 80.f;  // support
    inline constexpr float kBomberImpHealth    = 10.f;  // explodes on death (exempt)
}

// ── Elite minibosses (Ogre / Cyclops) base HP at power level 1 ───────────────
namespace Balance::Elite
{
    inline constexpr float kOgreHealth    = 100.f;
    inline constexpr float kCyclopsHealth = 70.f;

    // ── Elite signature kits (shared) ────────────────────────────────────────
    // Health stays on the existing 2.5x elite conversion — fight length comes
    // from mechanics, not inflated HP. These knobs shape the mechanics.
    inline constexpr float kPhaseThreshold       = 0.50f;  // one-time escalation at 50% max HP
    inline constexpr float kGuardLinksDamageTaken = 0.40f; // linked elite takes 40% (visible reduction, never immune)
    inline constexpr int   kSignatureZoneCapacity = 64;    // bounded attack-zone pool in CombatDirector

    // Ogre — The Battering Ram
    inline constexpr float kOgreWallStunMin      = 0.70f;  // wall punish window never falls below this
    inline constexpr float kOgreRetargetPause    = 0.55f;  // visible pause between phase-two charges

    // Infernal — The Living Furnace
    inline constexpr float kInfernalMarchTelegraph = 0.55f;
    inline constexpr float kInfernalMarchActive    = 1.60f;  // committed line walk
    inline constexpr float kInfernalMarchRecovery  = 0.45f;
    inline constexpr float kInfernalPatchSpacing   = 95.f;   // gap between flame patches (crossable)
    inline constexpr float kInfernalPatchLifetime  = 2.4f;   // short-lived — room never stays divided
    inline constexpr int   kInfernalPatchCap       = 6;
    inline constexpr float kInfernalBurstTelegraph = 0.70f;
    inline constexpr float kInfernalBurstActive    = 0.35f;
    inline constexpr float kInfernalBurstRecovery  = 0.90f;
    inline constexpr float kInfernalBurstRecoveryP2 = 1.35f; // longer exhausted window after OVERHEATED
    inline constexpr float kInfernalFissureLength  = 420.f;
    inline constexpr float kInfernalFissureWidth   = 58.f;   // half-width; gaps between the 3 fissures stay walkable
    inline constexpr float kInfernalPatchRadius    = 70.f;   // each flame patch the march leaves
    inline constexpr float kInfernalSignatureCooldown = 5.0f;
    inline constexpr float kInfernalPhaseMeleeMult = 0.85f;  // slightly faster melee cadence in phase two

    // Bonechill — The Frozen Wall
    inline constexpr float kBonechillFrontDamageTaken = 0.55f; // frontal frost armour: 45% reduction, never zero
    inline constexpr float kBonechillSlamTelegraph = 0.80f;
    inline constexpr float kBonechillSlamActive    = 0.30f;
    inline constexpr float kBonechillSlamRecovery  = 0.85f;
    inline constexpr float kBonechillSlamReach     = 150.f;  // the slam disc lands this far in front
    inline constexpr float kBonechillSlamRadius    = 140.f;
    inline constexpr float kBonechillLaneLength    = 460.f;
    inline constexpr float kBonechillLaneWidth     = 52.f;
    inline constexpr float kBonechillSignatureCooldown = 5.5f;
    inline constexpr float kBonechillPhaseSpeed    = 1.25f;  // ARMOUR SHATTERED movement bonus
    inline constexpr float kBonechillPhaseCooldownMult = 0.80f;

    // Stormclub — The Thunder Breaker
    inline constexpr float kStormclubLeapTelegraph = 0.70f;
    inline constexpr float kStormclubLeapSpeed     = 900.f;  // continuous travel — never a teleport
    inline constexpr float kStormclubLeapRange     = 520.f;
    inline constexpr float kStormclubLeapRangeP2   = 360.f;  // Tempest leaps are shorter
    inline constexpr float kStormclubImpactRadius  = 120.f;
    inline constexpr float kStormclubBranchLength  = 380.f;
    inline constexpr float kStormclubBranchWidth   = 46.f;
    inline constexpr float kStormclubHitRecovery   = 0.70f;
    inline constexpr float kStormclubMissRecovery  = 1.30f;  // embedded-club punish window
    inline constexpr float kStormclubSignatureCooldown = 5.0f;

    // Venomfang — The Ambush Predator
    inline constexpr float kVenomfangPounceTelegraph = 0.60f;
    inline constexpr float kVenomfangPounceSpeed     = 980.f;
    inline constexpr float kVenomfangPounceRange     = 460.f;
    inline constexpr float kVenomfangPounceRecovery  = 0.80f;
    inline constexpr float kVenomfangSignatureCooldown = 4.5f;
    inline constexpr int   kPredatorMarkCap          = 3;
    inline constexpr float kPredatorMarkDuration     = 6.f;   // expires without another bite
    inline constexpr float kVenomfangTrailLifetime   = 2.0f;
    inline constexpr float kVenomfangPhaseCircleSpeed = 1.30f; // BLOOD SCENT circling bonus
    inline constexpr float kVenomfangSecondPounceDelay = 0.85f;

    // Player poison (applied by Venomfang bites/trail; distinct from burn)
    inline constexpr int   kPoisonStackCap   = 3;
    inline constexpr float kPoisonDuration   = 4.0f;
    inline constexpr float kPoisonTickInterval = 0.8f;
    inline constexpr float kPoisonDamagePerTick = 5.f;    // per stack
}

// ── Bosses — unified role model ──────────────────────────────────────────────
// Every boss HP derives from ONE budget x a role multiplier, so the cast forms a
// smooth spread instead of ad-hoc numbers. Bosses are then power-scaled by the
// curve above at the depth they actually spawn, so a tanky boss met late is
// genuinely tanky. To make ALL bosses tankier/squishier, change kBaseHealth.
//   The first domain is always the Forest (Werewolf); the final boss is the
//   Molarbeast in Demon's Insides, the wall. The other bosses can appear at any
//   zone 1-4, so they share the mid budget and differ only by combat role.
namespace Balance::Boss
{
    inline constexpr float kBaseHealth = 460.f;  // "standard mid-boss" budget

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
    inline constexpr float kMolarbeastHealth  = kBaseHealth * kRoleFinal;    // ~713, the final boss
    inline constexpr float kChompBugHealth    = kBaseHealth * kRoleGlass;    // ~368
    inline constexpr float kWerewolfHealth    = kBaseHealth * kRoleFast;     // ~391
    inline constexpr float kOsirisHealth      = kBaseHealth * kRoleCaster;   // ~437
    inline constexpr float kPumpkinJackHealth = kBaseHealth * kRoleStandard; // ~460
    inline constexpr float kToxicVerminHealth = kBaseHealth * kRoleStandard; // ~460
    inline constexpr float kAncientBearHealth = kBaseHealth * kRoleHeavy;    // ~529
    inline constexpr float kMinotaurHealth    = kBaseHealth * kRoleTank;     // ~575
    inline constexpr float kTitanGuardHealth  = kBaseHealth * kRoleTank;     // ~575
    inline constexpr float kAbyssSlimeHealth  = kBaseHealth * kRoleStandard; // ~460, the Caverns boss
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
    inline constexpr int   kLifestealCapPerRoom   = 60;     // HP returned by lifesteal per room
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
    // Shop tier = RunProgression::ShopTierForZone (one tier per world-map domain).
    inline constexpr float kActPriceInflation     = 0.25f; // +25% ability prices per shop tier
    inline constexpr float kWareTierInflation     = 0.15f; // +15% ware prices per shop tier
    inline constexpr int   kAbilityUpgradeBase    = 110;   // level 1 -> 2
    inline constexpr int   kAbilityUpgradeStep    = 70;    // each further level
    // Domain bounty paid on every domain-boss clear: 50, 100, ... 250 gold, so
    // income keeps pace with the per-domain shop tiers (Engine::CompleteDomain).
    inline constexpr int   kDomainBountyBase      = 50;
    inline constexpr int   kDomainBountyStep      = 50;
    // One-time gift when a brand-new profile first speaks with Zeph, so the
    // village shop does something on the very first visit (one ware).
    inline constexpr int   kNewProfileWelcomeGold = 60;
}

// ── Domain pacing ────────────────────────────────────────────────────────────
// Every domain (one dungeon) must clear in 15 minutes or less. The planned
// seconds below include walking between rooms and are PLANNING estimates, not
// measurements: replace them with the median clear time per room kind from the
// room telemetry (F1/F10 panel) and re-check the plans. DungeonGenEntranceTests
// enforces PlannedDomainSeconds(...) <= kDomainMaxSeconds for every domain.
namespace Balance::Pacing
{
    inline constexpr float kDomainMaxSeconds           = 900.f;   // 15 minutes
    // Planning prices per room kind. Standard and treasure rooms share the first
    // value; elite and boss rooms are priced by their fight length. ALL FOUR ARE
    // ESTIMATES until the per-room telemetry from real runs replaces them.
    inline constexpr float kPlannedSecondsPerRoom      = 60.f;
    inline constexpr float kPlannedSecondsPerEliteRoom = 90.f;
    inline constexpr float kPlannedSecondsPerBossRoom  = 150.f;

    // Room counts per domain (index 0 = first domain, last = final domain).
    inline constexpr int kRoomsByDomain[6]     = { 8, 9, 10, 11, 12, 12 };
    inline constexpr int kElitesByDomain[6]    = { 1, 1,  2,  2,  2,  2 };
    inline constexpr int kTreasuresByDomain[6] = { 1, 1,  1,  2,  2,  2 };

    // Planned clear time for one domain: one boss room, elites at their own rate,
    // and every other room (standard and treasure) at the base rate.
    constexpr float PlannedDomainSeconds(int rooms, int elites)
    {
        return (float)(rooms - elites - 1) * kPlannedSecondsPerRoom
             + (float)elites * kPlannedSecondsPerEliteRoom
             + kPlannedSecondsPerBossRoom;
    }
}

// ── Room / spawn structure ───────────────────────────────────────────────────
namespace Balance::Rooms
{
    inline constexpr int kRoomsPerAct      = 6;   // 5 normal + 1 boss
    inline constexpr int kMaxActiveEnemies = 16;
    // Special rooms per domain (rest site and decision rooms together).
    inline constexpr int kSpecialsEarly         = 1;   // domains before kSpecialsLateFromZone
    inline constexpr int kSpecialsLate          = 2;
    inline constexpr int kSpecialsLateFromZone  = 2;   // zone index: domain 3 is index 2
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

// ── Enemy facing & directional combat ────────────────────────────────────────
// Facing commitment stops enemies flipping toward the player every frame, which
// made shield fronts unbeatable and backstabs impossible. Front/rear checks use
// a dot-product cone against the enemy's horizontal facing vector (sprites face
// left/right), not a raw x-comparison.
namespace Balance::Facing
{
    // Ranged classes turn to face their shot, then fire after this window, so the
    // shot leaves the side of the sprite the player is now facing.
    inline constexpr float kRangedTurnSeconds = 0.08f;
    // cos-space cone edges: dot(facing, toAttacker) >= kFrontConeDot counts as
    // "in front" (0.25 ≈ a 150° shielded arc); dot <= -kRearConeDot counts as
    // "behind" (a matching 150° rear arc). The sides belong to neither.
    inline constexpr float kFrontConeDot = 0.25f;
    inline constexpr float kRearConeDot  = 0.25f;
    // Minimum seconds between voluntary facing flips while walking.
    inline constexpr float kTurnCommitInterval      = 0.22f;
    // Heavy shield units pivot much slower — circling them is a real tactic.
    inline constexpr float kHeavyTurnCommitInterval = 0.55f;
    // Extra facing lock after an attack animation ends (recovery window the
    // player can exploit by repositioning).
    inline constexpr float kAttackRecoveryFacingLock = 0.35f;
    // Rogue Backstab: damage multiplier applied ONLY when the rear check passes.
    inline constexpr float kBackstabRearMult = 2.2f;
}

// ── Shieldbearer guard (placeholder values) ─────────────────────────────────
// The raised shield blocks hits arriving from the front, but every blocked hit
// fills a guard meter. A full meter breaks the guard: that hit lands, the
// Shieldbearer staggers with its shield down, then raises it with an empty
// meter. It also lowers the shield to swing and raises it a moment after.
// Damage over time (poison, bleed, burns, ground fire) always gets through.
namespace Balance::ShieldGuard
{
    inline constexpr float kBreakAtMaxHpFraction   = 1.0f;   // blocked damage that breaks the guard, x its max HP
    inline constexpr float kBreakStaggerSeconds    = 2.0f;   // shield down, no moving or attacking
    inline constexpr float kRecoverDelaySeconds    = 1.5f;   // seconds without a block before the meter drains
    inline constexpr float kRecoverPerSecond       = 0.25f;  // meter drained per second, as a share of a full meter
    inline constexpr float kRaiseAfterSwingSeconds = 0.35f;  // shield stays down this long after a swing ends
}

// ── Room pressure budget ─────────────────────────────────────────────────────
// How much simultaneous danger a combat room may field. Every threat costs
// pressure (grunts 1, specialists 2, Warchief 3, hazards per type); the room's
// composition is clamped to the cap for its tier, and bodies beyond the opening
// cap arrive as reinforcement waves instead of an unreadable wall of enemies.
// Tier index everywhere: 0 = early rooms, 1 = mid, 2 = late (cleared-room count).
namespace Balance::Pressure
{
    // Population controls spectacle; danger controls how many costly roles may
    // threaten the player simultaneously. Keeping these separate lets a room
    // contain a large fragile crowd without also becoming a wall of tanks and
    // ranged fire.
    inline constexpr int   kPopulationMin[3]    = { 5, 7, 9 };
    inline constexpr int   kPopulationMax[3]    = { 7, 10, 13 };
    inline constexpr int   kOpeningBodyCap[3]   = { 4, 6, 8 };
    inline constexpr int   kDangerCap[3]        = { 7, 10, 13 };
    inline constexpr int   kSwarmPeakMin        = 14;
    inline constexpr int   kSwarmPeakMax        = 18;

    inline constexpr int   kRangedCap[3]        = { 1, 2, 3 };
    inline constexpr int   kTankCap[3]          = { 1, 1, 2 };
    inline constexpr int   kSupportCap[3]       = { 0, 1, 1 };
    inline constexpr int   kAssassinCap[3]      = { 1, 2, 2 };
    inline constexpr int   kZonerCap[3]         = { 1, 2, 2 };
    inline constexpr int   kExpensiveUnitCap[3] = { 1, 2, 3 };
    inline constexpr int   kEnemyProjectileCap[3]       = { 12, 16, 20 };
    inline constexpr int   kEnvironmentalProjectileCap[3] = { 8, 10, 12 };

    // Legacy names remain temporarily for non-standard encounters while the
    // standard-room planner uses the clearer population/danger vocabulary.
    inline constexpr int   kRoomPressureCap[3]  = { 7, 10, 13 };
    // Max enemies alive when the fight OPENS; the surplus becomes waves.
    inline constexpr int   kOpeningActiveCap[3] = { 4, 6, 8 };
    // Standard-room body counts rolled before the pressure clamp.
    inline constexpr int   kMinBasics[3] = { 5, 7, 9 };
    inline constexpr int   kMaxBasics[3] = { 7, 10, 13 };
    // A reinforcement wave releases when live enemies drop to this count,
    // or on the interval timer — whichever comes first.
    inline constexpr int   kReinforceRefillActive = 4;
    inline constexpr float kReinforceInterval     = 6.f;
}

namespace Balance::Squad
{
    // ── Crowd behaviour: threat tiers, formation coherence, role tactics ─────
    // The CombatDirector reads the battlefield once per frame (player health,
    // ally count, who is attacking) and hands every enemy a SquadDirective.
    // Enemies use it to fight like a pack instead of a queue of solo chargers.

    // Aggression (threat assessment). 1.0 = neutral. Enemies smell blood when
    // the player is hurt and grow cautious when outnumbered themselves.
    inline constexpr float kAggressionMin            = 0.70f;
    inline constexpr float kAggressionMax            = 1.30f;
    inline constexpr float kPlayerLowHealthFrac      = 0.35f;  // below this → frenzy bonus
    inline constexpr float kPlayerHealthyFrac        = 0.80f;  // above this → respect
    inline constexpr float kLowHealthAggroBonus      = 0.25f;
    inline constexpr float kHealthyAggroPenalty      = 0.10f;
    inline constexpr int   kPackCourageCount         = 5;      // this many allies → bolder
    inline constexpr float kPackCourageBonus         = 0.15f;
    inline constexpr int   kLonelyCount              = 2;      // this few allies → warier
    inline constexpr float kLonelyPenalty            = 0.15f;

    // What aggression does. Frenzied enemies move faster and one extra attacker
    // may commit; wary enemies hold a standoff ring while an ally has the fight.
    inline constexpr float kFrenzyThreshold          = 1.15f;  // above → frenzy effects
    inline constexpr float kWaryThreshold            = 0.85f;  // below → standoff effects
    inline constexpr float kFrenzySpeedMult          = 1.12f;
    inline constexpr float kWarySpeedMult            = 0.92f;
    inline constexpr int   kFrenzyExtraAttackers     = 1;      // added to the 2-attacker slot cap
    inline constexpr float kStandoffRadius           = 330.f;  // wary grunts circle here
    inline constexpr float kEngagedNearPlayerRadius  = 260.f;  // "ally is on the player" check

    // Formation coherence. Grunts farther from the player than the tank rally
    // toward a slot behind it and advance as a pack until the fight is joined.
    inline constexpr float kLeaderMaxRange           = 900.f;  // leader too far → ignore
    inline constexpr float kLeaderFollowMargin       = 150.f;  // must be this much farther than leader
    inline constexpr float kLeaderSlotBehind         = 130.f;  // rally point offset behind the tank
    inline constexpr float kLeaderPullWeight         = 0.85f;  // blend strength toward the slot
    inline constexpr float kLeaderBreakoffDist       = 380.f;  // this close to player → free hunt

    // Role tactics.
    inline constexpr float kTankLeadSpeedMult        = 1.08f;  // tank leads the charge
    inline constexpr int   kTankLeadMinPack          = 3;      // needs a pack behind to push
    inline constexpr float kSupportHangBackDist      = 430.f;  // support keeps this far from player
    inline constexpr float kSupportAllyPullWeight    = 0.75f;  // pull toward the ally centroid
    inline constexpr int   kSupportMinAllies         = 2;      // alone → fight normally
    inline constexpr float kAssassinFlankDepth       = 150.f;  // aim point past/behind the player
}

// ── Enemy engagement rhythm (CombatEngagement) ───────────────────────────────
// Deterministic Commit/Support/Reposition policy: only a limited number of
// normal enemies may actively commit to an attack at once. Tier index matches
// Balance::Pressure — 0 = early rooms, 1 = mid, 2 = late (cleared-room count).
namespace Balance::Rhythm
{
    inline constexpr int   kCommittersByTier[3]    = { 2, 3, 3 };
    inline constexpr int   kSwarmExtraCommitters   = 1;   // fragile swarm-profile bonus slot
    inline constexpr float kPostCommitRepositionSeconds = 0.65f; // forced reposition after releasing a slot

    // Reinforcement spawn telegraph (ReinforcementPacing): purple circle warning,
    // then a smoke transition, then the enemy is instantiated. ~0.95s total —
    // see docs/superpowers/specs/2026-08-13-hades-style-enemy-combat-rhythm-design.md,
    // "Spawn Telegraph and VFX".
    inline constexpr float kSpawnCircleSeconds = 0.65f;   // purple circle warning
    inline constexpr float kSpawnSmokeSeconds  = 0.30f;   // smoke burst before spawn

    // Short arrival/orientation delay after a telegraphed reinforcement is
    // instantiated (design: "receives a short arrival/orientation delay
    // before it may move or attack"). Brief on purpose — this is a readability
    // beat, not a stun mechanic.
    inline constexpr float kSpawnArrivalDelaySeconds = 0.20f;

    // Distance from a held Reposition/Support engagement point within which an
    // enemy stops closing entirely (holds/idles) instead of continuously
    // overshooting and correcting every frame — a raw target-seek with no
    // deceleration oscillates back and forth across a stationary hold point,
    // which reads as the enemy "shaking." See Enemy::HandleMovement.
    inline constexpr float kEngagementArrivalDeadzone = 28.f;

    // Sporeling / volatile-death poison clouds arm this long after the enemy
    // dies before they deal damage or show their persistent decal — a tick-
    // down warning ring telegraphs the danger instead of it appearing
    // instantly on death. See Engine::SpawnPoisonCloud/UpdatePoisonClouds.
    inline constexpr float kPoisonCloudArmDelaySeconds = 2.0f;
}

namespace Balance::DamageNumbers
{
    inline constexpr int   kVisibleCap      = 32;
    inline constexpr int   kMinFontSize     = 20;
    inline constexpr int   kMaxFontSize     = 42;
    inline constexpr float kDamageReference = 120.f;
    inline constexpr float kRiseSpeed       = 92.f;
    inline constexpr float kLifetime        = 0.90f;
    inline constexpr float kOutline         = 2.f;
    inline constexpr float kMergeWindow     = 0.20f;
}

// ── Room hazards (RoomHazardDirector) ────────────────────────────────────────
// Shared tuning for environmental hazards (Fire Totem / Lava Pool / Fireball
// Torch...). Damage stays modest on purpose — hazards are movement pressure,
// not damage races. Frequencies/placement are consumed in generation (Phase 6).
namespace Balance::Hazards
{
    inline constexpr float kTelegraphSeconds   = 1.1f;   // warning before a hazard arms
    inline constexpr float kDisabledSeconds    = 4.f;    // downtime after the player disables one
    inline constexpr float kFirstActionGrace   = 1.5f;   // no firing right as the player enters
    inline constexpr int   kEnvProjectileCap   = 10;     // hard cap on environmental shots in flight
    inline constexpr int   kHazardTickDamage   = 10;
    inline constexpr float kHazardTickInterval = 0.5f;
    // Fraction of COMBAT rooms that roll a hazard, by tier.
    inline constexpr float kRoomFrequencyByTier[3] = { 0.38f, 0.58f, 0.72f };
    // Safe-placement rules (world px).
    inline constexpr float kMinDistFromDoorway = 240.f;
    inline constexpr float kMinDistFromEntry   = 320.f;
    inline constexpr float kMinDistBetween     = 260.f;
    inline constexpr float kRoomEdgeMargin     = 120.f;

    // Hazard fireballs are deliberately SMALL — a wisp's bolt at 55% size for
    // both the sprite and the hitbox. This is the knob if they still feel big.
    // Damage scales with the run: base below × the enemy damage growth curve
    // (Balance::Curve::kDamagePerLevel per power level), computed per room.
    inline constexpr float kHazardBoltScale   = 0.55f;

    // Fire Totem: aims with a visible telegraph line, then fires one bolt.
    inline constexpr float kTotemFireInterval = 3.2f;   // seconds between shots
    inline constexpr float kTotemAimSeconds   = 0.9f;   // telegraph line duration
    inline constexpr float kTotemAimLock      = 0.35f;  // aim freezes this long before firing (dodge window)
    inline constexpr int   kTotemBoltDamage   = 10;     // BASE damage (scaled by power level)
    inline constexpr float kTotemHealth       = 3.f;    // player hits to destroy
    inline constexpr float kTotemScale        = 5.5f;   // 16x32 sprite -> world size

    // Lava Pool: persistent floor zone, modest tick damage, entry grace.
    inline constexpr float kLavaGraceSeconds  = 0.45f;  // free window when first touched
    inline constexpr float kLavaRadius        = 100.f;  // damage circle inside the sprite
    inline constexpr float kLavaScale         = 3.4f;   // 84x96 sprite -> world size

    // Fireball Torch: wall-anchored lane launcher on a learnable rhythm.
    inline constexpr float kTorchFireInterval = 2.6f;   // lane shot cadence
    inline constexpr float kTorchPrefireFlash = 0.5f;   // bright flash before each shot
    inline constexpr int   kTorchBoltDamage   = 10;
    inline constexpr float kTorchHealth       = 3.f;
    inline constexpr float kTorchScale        = 5.f;    // 16x16 emitter sprite
    // Keep the lane clear of the east/west door band (door sits at mid-height).
    inline constexpr float kTorchDoorBandHalf = 170.f;

    // Elite "Arena Pressure" volleys (CombatDirector): the shots come out of the
    // room's walls. Each launch point glows on the wall first (an ember that
    // grows and points where it will fly), then fires. One of three patterns:
    //   barrage  a row along one wall, all flying straight across, launched as a ripple
    //   fan      a spread from the corner farthest from the player
    //   salvo    a few points on the far wall, each aimed at the player
    // Size matches an enemy fire bolt: the same 52 px hitbox, and the lava art
    // drawn at 1.5x so it shows 45-66 px across, like a fire bolt's 46-64
    // (at 2x it showed up to 88 px and looked bigger than what hits).
    inline constexpr float kVolleyDrawScale       = 1.5f;
    inline constexpr float kVolleyHitSize         = 52.f;   // = an enemy fire bolt's hitbox
    inline constexpr float kVolleyWindupSeconds   = 0.7f;   // the ember's glow before the shot leaves
    inline constexpr float kVolleyRippleSeconds   = 0.09f;  // a barrage or fan launches shot by shot, this far apart
    inline constexpr float kVolleySalvoGapSeconds = 0.2f;   // aimed salvo shots, this far apart
    inline constexpr float kVolleyWallInset       = 28.f;   // launch points sit this far inside the wall face
    inline constexpr float kVolleyCornerInset     = 60.f;   // fan origin, this far in from the corner on each axis
    inline constexpr float kVolleyAimSpreadDeg    = 8.f;    // salvo: each shot off the player by up to this
    inline constexpr float kVolleyFanArcDeg       = 50.f;   // fan: the whole spread
    inline constexpr float kVolleyMinPlayerDist   = 360.f;  // never launch this close to the player
    // A launch point on a wall tile, a prop, or under the HUD panel slides
    // toward the room centre in these steps until it is clear, or is dropped.
    inline constexpr float kVolleyLaunchSlideStep = 24.f;
    inline constexpr int   kVolleyLaunchSlideMax  = 6;
    inline constexpr int   kVolleyPatternWeights[3] = { 40, 30, 30 };   // barrage, fan, salvo
    // The ember (virtual px): grows from kVolleyEmberStartShare of its radius,
    // with a dark rim so it reads on bright floors, a pointer toward where the
    // shot will fly, and a warning ring that closes in on it: when the ring
    // reaches the ember, the shot leaves.
    inline constexpr float kVolleyEmberRadius     = 22.f;
    inline constexpr float kVolleyEmberStartShare = 0.5f;
    inline constexpr float kVolleyEmberCoreShare  = 0.45f;  // white-hot core, share of the ember
    inline constexpr float kVolleyEmberRim        = 4.f;    // dark rim width
    inline constexpr float kVolleyEmberRimAlpha   = 0.6f;
    inline constexpr float kVolleyEmberPulseHz    = 9.f;
    inline constexpr float kVolleyEmberPulseDepth = 0.25f;  // brightness swing of the pulse
    inline constexpr float kVolleyWarnRingStart   = 76.f;   // ring radius as the ember lights
    inline constexpr float kVolleyWarnRingWidth   = 4.f;
    inline constexpr float kVolleyPointerLength   = 44.f;   // from the ember's edge
    inline constexpr float kVolleyPointerWidth    = 8.f;
    inline constexpr float kVolleyAuraRadius      = 56.f;   // soft light around the ember
    inline constexpr float kVolleyAuraStrength    = 0.30f;
}

// ── Visual effects: halos and glow (VfxGlow.h) ──────────────────────────────
// Radii are in virtual pixels; strengths are the halo's alpha (0 = none, 1 = full).
namespace Balance::Vfx
{
    inline constexpr int   kHaloTextureSize   = 128;
    inline constexpr float kBoltHaloRadius    = 40.f;   // enemy and player projectiles
    inline constexpr float kBoltHaloStrength  = 0.60f;
    inline constexpr float kPickupHaloRadius  = 30.f;   // gold, heal orbs, cells
    inline constexpr float kPickupHaloStrength = 0.45f;
    inline constexpr float kDeathHaloRadius   = 80.f;   // kill burst, scaled by the flash
    inline constexpr float kDeathHaloStrength = 0.90f;

    // Glow (bloom). false = the old flat halos, nothing else glows.
    inline constexpr bool  kGlowEnabled         = true;
    inline constexpr int   kGlowBufferDivisor   = 2;      // light buffer is 1/2 of the canvas
    // How much each blur level adds: tight (1/2 size), mid (1/4), wide (1/8).
    // Raise wide for softer, bigger glow; raise tight for hotter cores.
    inline constexpr float kGlowTightWeight     = 0.15f;  // high values wash out the sprite itself
    inline constexpr float kGlowMidWeight       = 0.60f;
    inline constexpr float kGlowWideWeight      = 1.20f;
    inline constexpr int   kGlowWideBlurPasses  = 2;      // extra blurs of the 1/8 level spread it further
    inline constexpr float kGlowIntensity       = 1.00f;  // overall brightness of all glow
    inline constexpr float kGlowHaloCoreScale   = 0.70f;  // halo light core vs. the old halo radius
    inline constexpr float kGlowSpriteStrength  = 0.60f;  // spells, bolts and hit effects
    inline constexpr float kGlowHazardStrength  = 0.30f;  // lingering ground pools (acid, poison)
    inline constexpr float kGlowShockwaveStrength = 0.80f; // kill flash and ring
    inline constexpr float kGlowSlashStrength   = 0.90f;  // melee contact slashes
    inline constexpr int   kGlowRingSegments    = 40;

    // Pixel-art effects (VfxPixel.h): every effect shape and particle snaps to a
    // grid of these squares (virtual px; one art pixel at sprite scale).
    inline constexpr int   kFxPixelSize         = 4;
    inline constexpr float kGlowShapeStrength   = 0.60f;  // ability rings/lines, auras' rims, motes, orbs
    inline constexpr float kGlowTelegraphStrength = 0.35f; // aim previews and danger warnings: readable, not blinding
    // The aim line stops this far from the player (virtual px; ~1.5 inch at 96 dpi).
    // The ground-area preview still uses the full target.
    inline constexpr float kAimLineMaxLength      = 144.f;
    inline constexpr float kAuraLightBoost      = 3.0f;   // faked-glow disc alpha x this = its light strength

    // Particles (Enter the Gungeon style): chunky square pixels in short streaks
    // that run white-hot -> own colour -> dark ember (see Vfx::HeatRamp).
    inline constexpr float kSparkStreakSeconds  = 0.10f;  // streak length = speed x this (drag shortens it)
    inline constexpr float kSparkMaxLength      = 40.f;   // virtual px
    inline constexpr float kSparkTailAgeLead    = 0.30f;  // tail pixels look this much older (cooler)
    inline constexpr float kGlowSparkStrength   = 0.55f;
    inline constexpr float kSparkGlowWidth      = 2.5f;   // glow line thickness, in spark pixels
    inline constexpr float kHeatWhiteFraction   = 0.20f;  // share of a life spent white-hot
    inline constexpr float kHeatEmberDarken     = 0.45f;  // ember = own colour x this
    inline constexpr float kSmokeGrowPixels     = 2.f;    // a smoke puff grows by this many pixels
    // Melee impact burst: Hit03 redrawn by the silhouette shader along HeatRamp.
    inline constexpr float kMeleeImpactScale     = 3.f;
    inline constexpr float kMeleeImpactFrameTime = 1.f / 24.f;
    // Muzzle flash where a shot leaves: a small Hit03 burst in the shot's colour
    // plus a few sparks kicked forward (spells with their own cast sprite get
    // only the sparks).
    inline constexpr float kMuzzleFlashScale     = 2.f;
    inline constexpr float kMuzzleFlashFrameTime = 1.f / 30.f;
    inline constexpr int   kMuzzleSparkCount     = 4;
    inline constexpr float kMuzzleSparkSpeed     = 260.f;
    inline constexpr float kMuzzleSparkSpread    = 0.35f;  // radians either side of the shot
    // Dash dust: a burst of pixel puffs at the feet when a dash starts, then a
    // trail of single puffs drifting back against the dash.
    inline constexpr int   kDashDustBurstPuffs   = 6;
    inline constexpr float kDashDustTrailInterval = 0.035f; // seconds between trail puffs
    inline constexpr float kDashDustSpeed        = 60.f;
    inline constexpr float kDashDustDrift        = 90.f;   // backward drift against the dash
}

// ── Time of day (DayNight.h, VfxNight.h) ─────────────────────────────────────
// The clock always runs while you play (village and dungeon, indoors or out)
// and never shows on screen. Only outdoor places take the look of the hour:
// the village and the outdoor biomes (DayNight::IsOutdoorBiome). Indoor biomes
// keep their own art. No night art: the look recolours the art as drawn.
namespace Balance::DayNight
{
    inline constexpr float kDayLengthSeconds = 20.f * 60.f;  // one full day in real play time
    inline constexpr float kHoursPerDay      = 24.f;
    inline constexpr float kStartHour        = 8.f;          // a new profile starts in the morning

    // The look at set hours; between two of them it blends smoothly.
    //   tint        multiplies the scene (255 = unchanged)
    //   saturation  1 = the art's colours, 0 = grey
    //   lights      how strongly fire, spells and the player's light show (0 by day)
    //   haze        a colour laid over everything, by hazeAmount
    // Keep the hours in order; the last one blends into the first after midnight.
    struct Rgb { unsigned char r, g, b; };
    struct Keyframe
    {
        float hour;
        Rgb   tint;
        float saturation;
        float lights;
        Rgb   haze;
        float hazeAmount;
    };
    inline constexpr Keyframe kLooks[] = {
        //  hour    tint               sat    lights   haze               haze amount
        {  0.0f, { 105, 120, 185 },  0.60f,  1.00f,  {  20,  30,  70 },  0.00f },   // night
        {  4.5f, { 105, 120, 185 },  0.60f,  1.00f,  {  20,  30,  70 },  0.00f },   // night ends
        {  6.0f, { 205, 165, 175 },  0.80f,  0.60f,  { 255, 180, 150 },  0.06f },   // dawn: pink
        {  7.5f, { 255, 230, 195 },  0.92f,  0.15f,  { 255, 215, 160 },  0.06f },   // morning: soft gold haze
        {  9.5f, { 255, 255, 255 },  1.00f,  0.00f,  {   0,   0,   0 },  0.00f },   // day: the art as drawn
        { 16.5f, { 255, 255, 255 },  1.00f,  0.00f,  {   0,   0,   0 },  0.00f },   // day ends
        { 18.5f, { 255, 195, 150 },  0.95f,  0.25f,  { 255, 140, 100 },  0.06f },   // evening: orange
        { 20.0f, { 175, 135, 190 },  0.80f,  0.70f,  { 120,  70, 140 },  0.04f },   // dusk: purple
        { 21.5f, { 105, 120, 185 },  0.60f,  1.00f,  {  20,  30,  70 },  0.00f },   // night
    };

    // Light (virtual px). Everything that glows (VfxGlow.h) also lights the
    // ground around the spot it is on right now, in its own colour, so a
    // fireball carries its light with it and leaves no lit trail.
    inline constexpr float kLightSizeFromGlow   = 3.0f;   // light radius = the glow's own radius x this
    inline constexpr float kMinLightRadius      = 48.f;
    inline constexpr float kMaxLightRadius      = 260.f;
    inline constexpr float kGlowLightStrength   = 0.8f;   // x the glow's own strength
    inline constexpr float kLineLightSpacing    = 120.f;  // beams and slashes: one light per this much length
    inline constexpr int   kMaxLightsPerLine    = 4;
    inline constexpr Rgb   kWhiteLightColor     { 255, 214, 165 };   // glows drawn in white (a sprite's own colours) light warm
    inline constexpr unsigned char kWhiteLightMin = 230;  // a glow with every channel at least this counts as white
    inline constexpr float kPlayerLightRadius   = 230.f;  // a soft light around the player, so they never get lost
    inline constexpr float kPlayerLightStrength = 0.55f;
    inline constexpr Rgb   kPlayerLightColor    { 255, 225, 180 };
    // Light brings the art back toward its daytime colours (never brighter).
    inline constexpr float kLightGain           = 1.6f;   // soft knee: how lit = 1 - exp(-light x this)
    inline constexpr float kLightTint           = 0.50f;  // how much a light's colour tints what it lights
    inline constexpr int   kLightBands          = 4;      // stepped, pixel-art light; 0 = smooth
    // The light buffer has one texel per effect pixel (Balance::Vfx::kFxPixelSize),
    // so light falls on the same square grid as the effects.

    // Debug fast-forward (juice panel, J in debug mode).
    inline constexpr float kMaxDebugClockSpeed  = 600.f;  // x real time: a 20-minute day in 2 seconds
    inline constexpr float kDebugClockSpeedStep = 10.f;
    inline constexpr float kDebugHourStep       = 0.25f;
}

// ── Animation events (Character Animator): how a fired event plays ──────────
namespace Balance::AnimEvents
{
    inline constexpr float kDefaultFxSize  = 4.f;          // FX sheet draw scale when an event sets none
    inline constexpr float kFxFrameSeconds = 1.f / 24.f;   // same pace as the boss FX
    inline constexpr float kDefaultShake   = 6.f;          // shake strength when an event sets none
    inline constexpr float kShakeSeconds   = 0.12f;
    inline constexpr int   kFxCellSize     = 64;           // FX_*.png sheets are 64 px cells
}

// ── Enemy size: standard enemies match the player's visible art height ─────
// The hero's art is 19 px tall inside its 32 px frame (measured), drawn at x4, so
// it shows as 76 px. Each standard enemy is sized by its visible art height, which
// Enemy::MeasureVisibleHeight reads from its sheet, so sheet padding doesn't matter.
// Bosses keep their authored scale.
namespace Balance::Visual
{
    inline constexpr float kPlayerDrawScale     = 4.f;    // the hero's 32 px frames, same as the village citizens
    inline constexpr float kPlayerVisibleHeight = 76.f;   // hero art 19 px x draw scale 4
    inline constexpr float kEliteSizeMult       = 1.15f;  // Ogre, Cyclops: a little bigger
    inline constexpr float kMushroomSizeMult    = 0.90f;  // Sporeling: a little smaller

    // Sprite scale that makes an enemy's visible art visibleHeight px tall x mult.
    inline constexpr float ScaleForVisibleHeight(float visibleHeight, float mult)
    {
        return visibleHeight > 0.f ? kPlayerVisibleHeight * mult / visibleHeight : 1.f;
    }
}

// ── Boss intro (Werewolf first): drop-in, roar, name banner, then the fight ──
// The room is frozen and the player stays idle for the whole intro. The fight
// starts (and the boss track begins) when it ends. Skippable after the first
// viewing by holding attack.
namespace Balance::BossIntro
{
    inline constexpr float kDropHeight        = 420.f;  // px above the room the boss starts from
    inline constexpr float kDropSeconds       = 0.8f;   // falls and lands
    inline constexpr float kLandShake         = 8.f;    // shake strength on landing
    inline constexpr float kLandShakeSeconds  = 0.2f;
    inline constexpr float kRoarAt            = 1.0f;   // roar starts here (intro clock)
    inline constexpr float kRoarShake         = 14.f;
    inline constexpr float kRoarShakeSeconds  = 0.35f;
    inline constexpr float kBannerSeconds     = 1.0f;   // name banner on screen after the roar
    inline constexpr float kSkipHoldSeconds   = 1.0f;   // hold attack this long to skip

    // Banner and skip prompt layout (virtual px).
    inline constexpr float kBannerFadeSeconds = 0.25f;
    inline constexpr float kBannerBandY       = 0.38f;  // fraction of the screen height
    inline constexpr float kBannerBandHeight  = 120.f;
    inline constexpr int   kBannerTitleSize   = 64;
    inline constexpr float kSkipRingInner     = 16.f;
    inline constexpr float kSkipRingOuter     = 22.f;
    inline constexpr float kSkipMargin        = 40.f;
    inline constexpr int   kSkipTextSize      = 20;
}

// ── Timings / miscellaneous ──────────────────────────────────────────────────
namespace Balance::Misc
{
    inline constexpr float kWaveSpawnProtection = 2.f;
    inline constexpr float kBossWarningDuration = 4.f;
}
