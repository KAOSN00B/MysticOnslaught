#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// RELICS — Mystic Onslaught's build-variety engine.
//
// Relics are passive modifiers collected DURING a run (from Elite rooms, boss
// kills, and treasure). They stack and combine into synergy builds — the whole
// point is that an Ice player who grabs Permafrost + Deep Freeze plays nothing
// like a Fire player stacking Ember Heart + Wildfire.
//
// Effects live in three buckets by where they hook:
//   PASSIVE   — applied to Character stats the moment the relic is gained
//               (Character::ApplyRelicPassive).
//   ON-HIT    — scales outgoing damage per hit based on the target's status
//               (Character::ScaleOutgoingDamage).
//   ON-KILL   — fires when an enemy dies (Engine::ApplyRelicOnKill): lifesteal,
//               corpse explosions, bonus gold/cells.
//
// New relics: add to the enum (before Count), add a row to kRelicTable in
// Relic.cpp, then implement its effect in the matching hook.
// ─────────────────────────────────────────────────────────────────────────────

enum class RelicRarity { Common, Rare, Epic };

enum class RelicArchetype { Fire, Ice, Electric, Offense, Defense, Economy, Utility };

enum class RelicType
{
    // Comments state the real runtime effect (Relic.cpp kRelicTable / RelicDamageBonus).
    // ── Fire ─────────────────────────────────────────────────────────────────
    EmberHeart,       // burning enemies take +25% damage
    Wildfire,         // enemies that die while burning explode in a fire burst
    // ── Ice ──────────────────────────────────────────────────────────────────
    Permafrost,       // frozen/stunned enemies take +25% damage
    ShatterStrike,    // killing a frozen enemy freezes nearby enemies
    // ── Electric ─────────────────────────────────────────────────────────────
    Overcharge,       // charged enemies take +25% damage
    StormsReach,      // charged enemies that die shock nearby enemies
    // ── Offense ──────────────────────────────────────────────────────────────
    KeenEdge,         // first strike: +35% damage to enemies at full health
    Executioner,      // finisher: +35% damage to enemies at or below 25% HP
    Deadeye,          // +20% crit chance
    Bloodlust,        // kill chain: each kill grants +25% damage for 4 s (refreshes)
    GlassCannon,      // +35% damage, -20 max HP
    // ── Defense ──────────────────────────────────────────────────────────────
    StoneSkin,        // +10 Defense
    SecondWind,       // +20% max HP
    Bulwark,          // +10 Defense and heal 30
    ThornmailRunes,   // +20 max HP
    // ── Economy ──────────────────────────────────────────────────────────────
    MidasTouch,       // +60% gold from kills
    SoulSiphon,       // +60% Echoes from kills
    Scavenger,        // enemies drop more healing
    // ── Utility ──────────────────────────────────────────────────────────────
    SwiftBoots,       // mobility: +12% move speed, dash recharges 20% faster
    ArcaneBattery,    // +8 max mana, +12% mana regen
    Vampirism,        // heal 10 HP every 5 kills
    Berserker,        // +30% damage while below 40% HP
    Momentum,         // +10% move speed; +15% damage while moving
    Reaper,           // +15% damage; heal 40 HP on elite/boss kill

    // ── Expansion (appended: saved runs store relics by enum value) ─────────
    // Class mechanics (passive; each fits only the classes it can help)
    SmolderingBrand,  // Warrior: Rage drains 50% slower
    WarlordsCrest,    // Warrior: +25% ability damage at full Rage
    KindlingCore,     // Mage: +20% fire spell damage
    FrostLens,        // Mage: +20% ice spell damage
    StormCoil,        // Mage: +20% lightning spell damage
    ResonancePrism,   // Mage: elemental reactions deal +50% damage
    QuarrySigil,      // Hunter: marked prey takes +15% more damage
    VenomGland,       // Rogue/Hunter/Warlock: poison deals +50% damage
    ShadowReserve,    // Rogue: +1 max combo point
    RuthlessEdge,     // Rogue: finishers +10% damage per combo point
    MirrorPlate,      // Paladin: Aegis reflects 50% more damage
    ZealotsMark,      // Paladin: each Retribution stack adds +6% more damage
    DarkCovenant,     // Warlock: cursed enemies take +15% more damage
    HexLantern,       // Warlock: curses last 50% longer
    BloodChalice,     // Warrior/Rogue/Warlock: life drain is 50% stronger
    AttunementStone,  // Warrior/Paladin/Rogue: class resource builds 25% faster
    SpellbladeSigil,  // all but Mage: +15% ability damage
    HearthCharm,      // heal 20 HP whenever you clear a room
    // On kill
    ManaSiphon,       // kills restore 1 mana
    PyreHeart,        // burning kills heal 10 HP
    GlacialBattery,   // frozen kills restore 3 mana
    ChainReaction,    // kills have a 25% chance to blast nearby foes for 30
    SoulHarvest,      // elite and boss kills grant 15 Echoes
    // On hit (situational damage)
    GiantSlayer,      // +25% damage to elites and bosses
    Ambush,           // +40% damage in the first 4 s of a fight
    Duelist,          // +30% damage when only one enemy remains
    GildedGauntlet,   // +5% damage per 100 gold carried (max +30%)
    Bully,            // +20% damage to enemies below half health
    Afterimage,       // +40% damage for 1 s after a dash
    Retaliation,      // getting hit grants +30% damage for 3 s
    // Defense, utility and economy
    SpikedPauldrons,  // when hit, deal 20 damage to enemies near you
    LastStand,        // once per room, a lethal hit leaves you at 10 HP
    Ironclad,         // clearing a room adds +3 Defense (up to +15)
    Hourglass,        // ability cooldowns recover 25% faster
    CoinPurse,        // +15 gold whenever you clear a room
    Stoneheart,       // +40% max HP, -10% move speed
    Count
};

// Everything a relic needs to know about one player hit. Relic damage bonuses
// are ADDITIVE with each other (no multiplicative relic stacking); crits and
// status/vulnerability multipliers apply on top in Engine::ScalePlayerHit.
struct RelicHitContext
{
    bool  targetFrozen     = false;   // frozen or stunned (class stuns use the freeze state)
    bool  targetCharged    = false;
    bool  targetBurning    = false;
    float targetHpFraction = 1.f;     // before this hit
    float playerHpFraction = 1.f;
    bool  playerMoving     = false;   // the player moved within kMomentumWindow seconds
    bool  bloodlustActive  = false;   // a kill happened within the Bloodlust window
    bool  targetEliteOrBoss = false;  // Giant Slayer
    bool  earlyInFight     = false;   // Ambush: within kAmbushWindow s of the room's combat
    bool  lastEnemy        = false;   // Duelist: the target is the only enemy left
    int   playerGold       = 0;       // Gilded Gauntlet
    bool  recentlyDashed   = false;   // Afterimage: a dash ended within kAfterimageWindow s
    bool  recentlyHurt     = false;   // Retaliation: hit within kRetaliationDuration s
};

// Sum of relic damage bonuses for one hit (0 = no bonus). `owned` is indexed
// by RelicType. Deadeye adds crit chance (Character::GetCritChance), not included here.
float RelicDamageBonus(const bool* owned, const RelicHitContext& hit);

inline constexpr float kBloodlustDuration = 4.f;
inline constexpr float kMomentumWindow = 0.5f;   // "moving" = moved within the last 0.5 s
inline constexpr float kSwiftBootsSpeedMult = 1.12f;
inline constexpr float kSwiftBootsDashCooldownMult = 0.80f;
inline constexpr float kAmbushWindow = 4.f;
inline constexpr float kAfterimageWindow = 1.f;
inline constexpr float kRetaliationDuration = 3.f;
inline constexpr int   kChainReactionChancePercent = 25;
inline constexpr float kHourglassCooldownRate = 1.25f;
inline constexpr float kStoneheartHealthMult = 1.40f;
inline constexpr float kStoneheartSpeedMult = 0.90f;

// Whether a relic can ever do anything for a class. Elemental relics need a
// class that applies that status; offers skip relics that would be dead picks.
enum class PlayerClass;
bool RelicFitsClass(RelicType type, PlayerClass cls);

struct RelicInfo
{
    const char*    name;
    const char*    description;
    RelicRarity    rarity;
    RelicArchetype archetype;
};

const RelicInfo& GetRelicInfo(RelicType type);

// Human-readable helpers for UI.
const char* GetRelicRarityName(RelicRarity rarity);
const char* GetRelicArchetypeName(RelicArchetype archetype);
