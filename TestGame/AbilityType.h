#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// COMBAT ARCHITECTURE — Mystic Onslaught
//
// The primary combat system is mana-based elemental abilities:
//   Spread  (8-way burst)  3 mana   FireSpread / IceSpread / ElectricSpread
//   Bolt    (aimed single) 2 mana   FireBolt   / IceBolt   / ElectricBolt
//   Ultimate (full drain)  all mana FireUltimate / IceUltimate / ElectricUltimate
//
// HOW ABILITIES ARE LEARNED:
//   1. Player earns XP by killing enemies and levels up.
//   2. Engine calls GenerateLevelUpOptions() and shows 3 upgrade cards.
//      At level 3 a second row of all 3 ultimates also appears.
//   3. Player picks a card → Engine calls player.ApplyUpgrade(UpgradeType).
//   4. ApplyUpgrade calls player.LearnAbility(AbilityType).
//   5. Ability appears in the HUD bar (slots 1–4, keys 1/2/3/4).
//
// HOW ABILITIES ARE TRIGGERED:
//   1. Player presses a hotkey (1–4) or clicks a HUD slot icon.
//   2. Character::TriggerAbilityCast(slot) validates mana, deducts cost,
//      and queues a CastType for the engine to read next frame.
//   3. Engine::UpdateGamePlay calls ConsumeCastRequest() each frame.
//   4. Engine spawns the matching projectile(s) or triggers the cinematic.
//
// RESOURCE FLOW:
//   Mana   — restored passively (0.2/sec regen) and via shop purchases.
//   Health — restored by HealPickup (+1 HP).
//   Both drop at ~22% chance per enemy kill.
//   Timed pickups also spawn on a fixed interval (faster during boss waves).
// ─────────────────────────────────────────────────────────────────────────────

// All learnable ability types. New abilities are added here first.
enum class AbilityType
{
    None = -1,
    FireSpread,
    IceSpread,
    ElectricSpread,
    // Single aimed bolt — same element effect, higher damage per shot
    FireBolt,
    IceBolt,
    ElectricBolt,
    // Elemental ultimate — drains all mana, scatters blasts across the arena
    FireUltimate,
    IceUltimate,
    ElectricUltimate,
    Count   // keep last
};

inline const char* GetAbilityName(AbilityType type)
{
    switch (type)
    {
    case AbilityType::FireSpread:      return "Fire Spread";
    case AbilityType::IceSpread:       return "Ice Spread";
    case AbilityType::ElectricSpread:  return "Electric Spread";
    case AbilityType::FireBolt:        return "Fire Bolt";
    case AbilityType::IceBolt:         return "Ice Bolt";
    case AbilityType::ElectricBolt:    return "Electric Bolt";
    case AbilityType::FireUltimate:    return "Fire Ultimate";
    case AbilityType::IceUltimate:     return "Ice Ultimate";
    case AbilityType::ElectricUltimate:return "Elec. Ultimate";
    default:                           return "Unknown";
    }
}

inline const char* GetAbilityDesc(AbilityType type)
{
    switch (type)
    {
    case AbilityType::FireSpread:      return "8 fireballs\nburn on hit";
    case AbilityType::IceSpread:       return "8 ice shards\nfreeze on hit";
    case AbilityType::ElectricSpread:  return "8 bolts\nstun randomly";
    case AbilityType::FireBolt:        return "Aimed fireball\nhigh damage + burn";
    case AbilityType::IceBolt:         return "Aimed shard\nhigh damage + freeze";
    case AbilityType::ElectricBolt:    return "Aimed bolt\nhigh damage + stun";
    case AbilityType::FireUltimate:    return "Blasts everywhere\n4 dmg + burn, all MP";
    case AbilityType::IceUltimate:     return "Blasts everywhere\n4 dmg + freeze, all MP";
    case AbilityType::ElectricUltimate:return "Blasts everywhere\n4 dmg + stun, all MP";
    default:                           return "";
    }
}

inline int GetAbilityManaCost(AbilityType type)
{
    switch (type)
    {
    case AbilityType::FireBolt:
    case AbilityType::IceBolt:
    case AbilityType::ElectricBolt:
        return 4;   // stronger single-target spell, higher mana commitment
    case AbilityType::FireUltimate:
    case AbilityType::IceUltimate:
    case AbilityType::ElectricUltimate:
        return 1;   // needs at least 1 mana; actual cast drains everything
    default:
        return 2;   // spread is the bread-and-butter crowd-clear cast
    }
}

// Returns true for abilities that drain ALL current mana when cast.
inline bool AbilityDrainsAllMana(AbilityType type)
{
    return type == AbilityType::FireUltimate  ||
           type == AbilityType::IceUltimate   ||
           type == AbilityType::ElectricUltimate;
}
