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
    // ── Fire ─────────────────────────────────────────────────────────────────
    EmberHeart,       // burning enemies take +40% damage
    Wildfire,         // enemies that die while burning explode in a fire burst
    // ── Ice ──────────────────────────────────────────────────────────────────
    Permafrost,       // frozen enemies take +60% damage
    ShatterStrike,    // killing a frozen enemy freezes nearby enemies
    // ── Electric ─────────────────────────────────────────────────────────────
    Overcharge,       // charged/stunned enemies take +50% damage
    StormsReach,      // charged enemies that die shock nearby enemies
    // ── Offense ──────────────────────────────────────────────────────────────
    KeenEdge,         // +15% all damage
    Executioner,      // +120% damage to enemies below 25% HP
    Deadeye,          // 20% chance to crit for double damage
    Bloodlust,        // +8% damage per Bloodlust stack... (single: +25% damage)
    GlassCannon,      // +80% damage, -2 max HP
    // ── Defense ──────────────────────────────────────────────────────────────
    StoneSkin,        // +1 max armour
    SecondWind,       // +30% max HP
    Bulwark,          // +1 max armour and heal 3
    ThornmailRunes,   // (reserved — contact reflect; passive HP for now +2)
    // ── Economy ──────────────────────────────────────────────────────────────
    MidasTouch,       // +60% gold from kills
    SoulSiphon,       // +60% Mystic Cells from kills
    Scavenger,        // enemies drop more healing
    // ── Utility ──────────────────────────────────────────────────────────────
    SwiftBoots,       // +14% move speed
    ArcaneBattery,    // +40 max mana, +40% mana regen
    Vampirism,        // heal 1 HP every 5 kills
    Berserker,        // +30% damage while below 40% HP
    Momentum,         // +10% move speed and +10% damage (glass-speed hybrid)
    Reaper,           // kills grant a brief +damage surge... (single: +15% dmg, +heal on elite kill)
    Count
};

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
