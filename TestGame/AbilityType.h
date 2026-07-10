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

    // ── WARRIOR — melee bruiser kit (mana used as the ability resource) ─────────
    // Six normal abilities + three ultimates, each mechanically distinct.
    WarCleave,     // frontal shockwave arc, knockback
    Whirlwind,     // 360° spin, hits everything around the player
    ThrowingAxe,   // long piercing line hit thrown forward
    Rend,          // lunge forward + bleed (damage over time)
    ShieldBash,    // shoulder charge dash that stuns those it strikes
    WarCry,        // self-buff: temporary bonus damage + a small heal
    GroundSlam,    // ULT: huge radial quake, heavy damage + stun (all mana)
    Rampage,       // ULT: berserk — bonus damage + lifesteal for a while (all mana)
    Earthshatter,  // ULT: erupting spike line straight ahead, massive damage (all mana)

    // ── ROGUE — fragile, high-burst assassin (fast dagger basic attack) ─────────
    FanOfKnives,   // forward cone of thrown daggers
    Shadowstep,    // teleport dash forward, cutting anything crossed
    PoisonVial,    // lobbed vial → lingering poison pool that ticks enemies
    Backstab,      // one guaranteed heavy strike in front
    SmokeBomb,     // slow nearby foes + ambush damage buff on self
    Eviscerate,    // rapid flurry combo straight ahead
    DeathMark,     // ULT: screen-wide assassination burst (all mana)
    BladeDance,    // ULT: spinning blades around you + lifesteal buff (all mana)
    RainOfBlades,  // ULT: massive dagger barrage over a wide area ahead (all mana)

    // ── HUNTER — trap-laying skirmisher (bow + armed ground traps) ─────────────
    PiercingShot,   // long single piercing arrow
    Multishot,      // forward cone of arrows
    FrostTrap,      // FREEZING TRAP — armed, snaps to freeze foes that trip it
    ExplosiveArrow, // EXPLOSIVE TRAP — armed, snaps for an AoE blast + knockback
    Roll,           // quick dodge dash + brief aim buff
    Volley,         // PUNCTURE SHOT — shield-piercing heavy arrow
    ArrowStorm,     // ULT: arrows rain across the whole arena (all mana)
    Deadeye,        // ULT: huge damage + speed buff for a while (all mana)
    PiercingBarrage,// ULT: devastating arena-long piercing line (all mana)

    // ── PALADIN — holy tank (sword + shield) ───────────────────────────────────
    Smite,          // forward holy strike
    Consecrate,     // holy ground zone that burns enemies
    ShieldOfFaith,  // self: damage buff + heal
    HolyBolt,       // ranged holy line
    HammerThrow,    // thrown hammer line that stuns
    LayOnHands,     // big self heal
    DivineStorm,    // ULT: radial holy nova (all mana)
    AvengingWrath,  // ULT: damage + lifesteal + heal buff (all mana)
    HammerOfJustice,// ULT: massive holy line + stun (all mana)

    // ── WARLOCK — dark caster (drain & curses) ─────────────────────────────────
    ShadowBolt,     // ranged dark line
    DrainLife,      // forward hit that heals you
    Curse,          // applies a damage-over-time curse ahead
    CorruptionPool, // lingering dark damage zone
    Hellfire,       // radial dark burst
    SoulSiphon,     // radial hit + lifesteal buff
    Cataclysm,      // ULT: screen-wide dark nuke (all mana)
    DemonForm,      // ULT: huge damage + lifesteal buff (all mana)
    ShadowNova,     // ULT: massive dark line + curse (all mana)

    Count   // keep last
};

// Every learnable ability, in enum order. Iterated by the shop / reward screens
// so new classes appear automatically once their entries are added above.
inline constexpr AbilityType kAllAbilities[] = {
    AbilityType::FireSpread,     AbilityType::IceSpread,     AbilityType::ElectricSpread,
    AbilityType::FireBolt,       AbilityType::IceBolt,       AbilityType::ElectricBolt,
    AbilityType::FireUltimate,   AbilityType::IceUltimate,   AbilityType::ElectricUltimate,
    AbilityType::WarCleave,      AbilityType::Whirlwind,     AbilityType::ThrowingAxe,
    AbilityType::Rend,           AbilityType::ShieldBash,    AbilityType::WarCry,
    AbilityType::GroundSlam,     AbilityType::Rampage,       AbilityType::Earthshatter,
    AbilityType::FanOfKnives,    AbilityType::Shadowstep,    AbilityType::PoisonVial,
    AbilityType::Backstab,       AbilityType::SmokeBomb,     AbilityType::Eviscerate,
    AbilityType::DeathMark,      AbilityType::BladeDance,    AbilityType::RainOfBlades,
    AbilityType::PiercingShot,   AbilityType::Multishot,     AbilityType::FrostTrap,
    AbilityType::ExplosiveArrow, AbilityType::Roll,          AbilityType::Volley,
    AbilityType::ArrowStorm,     AbilityType::Deadeye,       AbilityType::PiercingBarrage,
    AbilityType::Smite,          AbilityType::Consecrate,    AbilityType::ShieldOfFaith,
    AbilityType::HolyBolt,       AbilityType::HammerThrow,   AbilityType::LayOnHands,
    AbilityType::DivineStorm,    AbilityType::AvengingWrath, AbilityType::HammerOfJustice,
    AbilityType::ShadowBolt,     AbilityType::DrainLife,     AbilityType::Curse,
    AbilityType::CorruptionPool, AbilityType::Hellfire,      AbilityType::SoulSiphon,
    AbilityType::Cataclysm,      AbilityType::DemonForm,     AbilityType::ShadowNova,
};
inline constexpr int kAllAbilityCount = (int)(sizeof(kAllAbilities) / sizeof(kAllAbilities[0]));

// True for abilities the Mage's elemental cast pipeline (CastType) handles.
// Everything else is a "class ability" routed through the generic dispatcher.
inline bool IsElementalAbility(AbilityType type)
{
    return type == AbilityType::FireSpread || type == AbilityType::IceSpread || type == AbilityType::ElectricSpread ||
           type == AbilityType::FireBolt   || type == AbilityType::IceBolt   || type == AbilityType::ElectricBolt   ||
           type == AbilityType::FireUltimate || type == AbilityType::IceUltimate || type == AbilityType::ElectricUltimate;
}

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
    case AbilityType::WarCleave:       return "Cleave Wave";
    case AbilityType::Whirlwind:       return "Whirlwind";
    case AbilityType::ThrowingAxe:     return "Throwing Axe";
    case AbilityType::Rend:            return "Rend";
    case AbilityType::ShieldBash:      return "Shoulder Charge";
    case AbilityType::WarCry:          return "War Cry";
    case AbilityType::GroundSlam:      return "Ground Slam";
    case AbilityType::Rampage:         return "Rampage";
    case AbilityType::Earthshatter:    return "Earthshatter";
    case AbilityType::FanOfKnives:     return "Fan of Knives";
    case AbilityType::Shadowstep:      return "Shadowstep";
    case AbilityType::PoisonVial:      return "Poison Vial";
    case AbilityType::Backstab:        return "Backstab";
    case AbilityType::SmokeBomb:       return "Smoke Bomb";
    case AbilityType::Eviscerate:      return "Eviscerate";
    case AbilityType::DeathMark:       return "Death Mark";
    case AbilityType::BladeDance:      return "Blade Dance";
    case AbilityType::RainOfBlades:    return "Rain of Blades";
    case AbilityType::PiercingShot:    return "Piercing Shot";
    case AbilityType::Multishot:       return "Multishot";
    case AbilityType::FrostTrap:       return "Freezing Trap";
    case AbilityType::ExplosiveArrow:  return "Explosive Trap";
    case AbilityType::Roll:            return "Roll";
    case AbilityType::Volley:          return "Puncture Shot";
    case AbilityType::ArrowStorm:      return "Arrow Storm";
    case AbilityType::Deadeye:         return "Deadeye";
    case AbilityType::PiercingBarrage: return "Piercing Barrage";
    case AbilityType::Smite:           return "Smite";
    case AbilityType::Consecrate:      return "Consecrate";
    case AbilityType::ShieldOfFaith:   return "Aegis";
    case AbilityType::HolyBolt:        return "Holy Bolt";
    case AbilityType::HammerThrow:     return "Hammer Throw";
    case AbilityType::LayOnHands:      return "Vengeful Ward";
    case AbilityType::DivineStorm:     return "Divine Storm";
    case AbilityType::AvengingWrath:   return "Avenging Wrath";
    case AbilityType::HammerOfJustice: return "Hammer of Justice";
    case AbilityType::ShadowBolt:      return "Shadow Bolt";
    case AbilityType::DrainLife:       return "Drain Life";
    case AbilityType::Curse:           return "Curse";
    case AbilityType::CorruptionPool:  return "Corruption Pool";
    case AbilityType::Hellfire:        return "Hellfire";
    case AbilityType::SoulSiphon:      return "Soul Siphon";
    case AbilityType::Cataclysm:       return "Cataclysm";
    case AbilityType::DemonForm:       return "Demon Form";
    case AbilityType::ShadowNova:      return "Shadow Nova";
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
    case AbilityType::WarCleave:       return "Frontal shockwave\nknocks foes back";
    case AbilityType::Whirlwind:       return "Spin attack\nhits all around you";
    case AbilityType::ThrowingAxe:     return "Piercing axe\nflies straight ahead";
    case AbilityType::Rend:            return "Lunge + deep cut\nbleeds over time";
    case AbilityType::ShieldBash:      return "Charge forward\nstuns what you hit";
    case AbilityType::WarCry:          return "Roar: +damage\nand a small heal";
    case AbilityType::GroundSlam:      return "Quake around you\nbig dmg + stun, all MP";
    case AbilityType::Rampage:         return "Berserk: +dmg &\nlifesteal, all MP";
    case AbilityType::Earthshatter:    return "Spike line ahead\nmassive dmg, all MP";
    case AbilityType::FanOfKnives:     return "Cone of daggers\nsprays ahead";
    case AbilityType::Shadowstep:      return "Blink forward\ncutting all crossed";
    case AbilityType::PoisonVial:      return "Lobbed vial\nlingering poison pool";
    case AbilityType::Backstab:        return "One brutal strike\nguaranteed heavy hit";
    case AbilityType::SmokeBomb:       return "Slows nearby foes\n+ ambush damage";
    case AbilityType::Eviscerate:      return "Rapid flurry\nshreds what's ahead";
    case AbilityType::DeathMark:       return "Mark & execute all\non screen, all MP";
    case AbilityType::BladeDance:      return "Spin of blades +\nlifesteal, all MP";
    case AbilityType::RainOfBlades:    return "Dagger storm over\na wide area, all MP";
    case AbilityType::PiercingShot:    return "Long arrow that\npierces enemies";
    case AbilityType::Multishot:       return "Cone of arrows\nsprayed ahead";
    case AbilityType::FrostTrap:       return "Armed trap:\nfreezes on trip";
    case AbilityType::ExplosiveArrow:  return "Armed trap:\nbursts on trip";
    case AbilityType::Roll:            return "Dodge dash +\nbrief aim bonus";
    case AbilityType::Volley:          return "Pierces shields,\nheavy arrow";
    case AbilityType::ArrowStorm:      return "Arrows rain the\narena, all MP";
    case AbilityType::Deadeye:         return "+dmg & speed for\na while, all MP";
    case AbilityType::PiercingBarrage: return "Arena-long piercing\nvolley, all MP";
    case AbilityType::Smite:           return "Holy strike\nahead of you";
    case AbilityType::Consecrate:      return "Blessed ground\nburns foes";
    case AbilityType::ShieldOfFaith:   return "+damage and\nreflect blows";
    case AbilityType::HolyBolt:        return "Ranged bolt of\nholy light";
    case AbilityType::HammerThrow:     return "Thrown hammer\nstuns on hit";
    case AbilityType::LayOnHands:      return "Reflect + full\nretribution";
    case AbilityType::DivineStorm:     return "Holy nova; wrath\nper foe, all MP";
    case AbilityType::AvengingWrath:   return "+dmg & reflect,\nall MP";
    case AbilityType::HammerOfJustice: return "Huge holy line +\nstun, all MP";
    case AbilityType::ShadowBolt:      return "Ranged bolt of\ndark energy";
    case AbilityType::DrainLife:       return "Strike that heals\nyou for damage";
    case AbilityType::Curse:           return "Curses foes ahead\nto rot over time";
    case AbilityType::CorruptionPool:  return "Festering dark\npool ahead";
    case AbilityType::Hellfire:        return "Dark blast erupts\naround you";
    case AbilityType::SoulSiphon:      return "Drains around you\n+ lifesteal buff";
    case AbilityType::Cataclysm:       return "Dark ruin across\nthe screen, all MP";
    case AbilityType::DemonForm:       return "+dmg & lifesteal\nfor a while, all MP";
    case AbilityType::ShadowNova:      return "Massive dark line\n+ curse, all MP";
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
    // ── Warrior ──────────────────────────────────────────────────────────────
    case AbilityType::ThrowingAxe:
    case AbilityType::Rend:
        return 3;   // committed single-target strikes
    case AbilityType::GroundSlam:
    case AbilityType::Rampage:
    case AbilityType::Earthshatter:
        return 1;   // ultimates: drain everything (see AbilityDrainsAllMana)
    case AbilityType::WarCleave:
    case AbilityType::Whirlwind:
    case AbilityType::ShieldBash:
    case AbilityType::WarCry:
        return 2;   // bread-and-butter melee tools
    // ── Rogue ────────────────────────────────────────────────────────────────
    case AbilityType::Backstab:
    case AbilityType::PoisonVial:
        return 3;   // committed burst / area denial
    case AbilityType::DeathMark:
    case AbilityType::BladeDance:
    case AbilityType::RainOfBlades:
        return 1;   // ultimates: drain everything (see AbilityDrainsAllMana)
    case AbilityType::FanOfKnives:
    case AbilityType::Shadowstep:
    case AbilityType::SmokeBomb:
    case AbilityType::Eviscerate:
        return 2;   // quick assassin tools
    // ── Hunter / Paladin / Warlock ultimates: drain everything ────────────────
    case AbilityType::ArrowStorm:
    case AbilityType::Deadeye:
    case AbilityType::PiercingBarrage:
    case AbilityType::DivineStorm:
    case AbilityType::AvengingWrath:
    case AbilityType::HammerOfJustice:
    case AbilityType::Cataclysm:
    case AbilityType::DemonForm:
    case AbilityType::ShadowNova:
        return 1;
    // Heavier committed casts.
    case AbilityType::ExplosiveArrow:
    case AbilityType::FrostTrap:
    case AbilityType::HammerThrow:
    case AbilityType::LayOnHands:
    case AbilityType::CorruptionPool:
    case AbilityType::DrainLife:
        return 3;
    default:
        return 2;   // spread is the bread-and-butter crowd-clear cast
    }
}

// Returns true for abilities that drain ALL current mana when cast.
inline bool AbilityDrainsAllMana(AbilityType type)
{
    return type == AbilityType::FireUltimate  ||
           type == AbilityType::IceUltimate   ||
           type == AbilityType::ElectricUltimate ||
           type == AbilityType::GroundSlam    ||
           type == AbilityType::Rampage       ||
           type == AbilityType::Earthshatter  ||
           type == AbilityType::DeathMark     ||
           type == AbilityType::BladeDance    ||
           type == AbilityType::RainOfBlades  ||
           type == AbilityType::ArrowStorm    ||
           type == AbilityType::Deadeye       ||
           type == AbilityType::PiercingBarrage ||
           type == AbilityType::DivineStorm   ||
           type == AbilityType::AvengingWrath ||
           type == AbilityType::HammerOfJustice ||
           type == AbilityType::Cataclysm     ||
           type == AbilityType::DemonForm     ||
           type == AbilityType::ShadowNova;
}

// True for the three ultimate-tier abilities of any class (exclusive ult slot).
inline bool IsUltimateAbility(AbilityType type)
{
    return AbilityDrainsAllMana(type);
}

// Icon file stem in PowerUps/Ability_<stem>.png for non-elemental abilities.
// Mage's elemental abilities return "" (they use the element pickup textures).
inline const char* GetAbilityIconStem(AbilityType type)
{
    switch (type)
    {
    case AbilityType::WarCleave:       return "WarCleave";
    case AbilityType::Whirlwind:       return "Whirlwind";
    case AbilityType::ThrowingAxe:     return "ThrowingAxe";
    case AbilityType::Rend:            return "Rend";
    case AbilityType::ShieldBash:      return "ShieldBash";
    case AbilityType::WarCry:          return "WarCry";
    case AbilityType::GroundSlam:      return "GroundSlam";
    case AbilityType::Rampage:         return "Rampage";
    case AbilityType::Earthshatter:    return "Earthshatter";
    case AbilityType::FanOfKnives:     return "FanOfKnives";
    case AbilityType::Shadowstep:      return "Shadowstep";
    case AbilityType::PoisonVial:      return "PoisonVial";
    case AbilityType::Backstab:        return "Backstab";
    case AbilityType::SmokeBomb:       return "SmokeBomb";
    case AbilityType::Eviscerate:      return "Eviscerate";
    case AbilityType::DeathMark:       return "DeathMark";
    case AbilityType::BladeDance:      return "BladeDance";
    case AbilityType::RainOfBlades:    return "RainOfBlades";
    case AbilityType::PiercingShot:    return "PiercingShot";
    case AbilityType::Multishot:       return "Multishot";
    case AbilityType::FrostTrap:       return "FrostTrap";
    case AbilityType::ExplosiveArrow:  return "ExplosiveArrow";
    case AbilityType::Roll:            return "Roll";
    case AbilityType::Volley:          return "Volley";
    case AbilityType::ArrowStorm:      return "ArrowStorm";
    case AbilityType::Deadeye:         return "Deadeye";
    case AbilityType::PiercingBarrage: return "PiercingBarrage";
    case AbilityType::Smite:           return "Smite";
    case AbilityType::Consecrate:      return "Consecrate";
    case AbilityType::ShieldOfFaith:   return "ShieldOfFaith";
    case AbilityType::HolyBolt:        return "HolyBolt";
    case AbilityType::HammerThrow:     return "HammerThrow";
    case AbilityType::LayOnHands:      return "LayOnHands";
    case AbilityType::DivineStorm:     return "DivineStorm";
    case AbilityType::AvengingWrath:   return "AvengingWrath";
    case AbilityType::HammerOfJustice: return "HammerOfJustice";
    case AbilityType::ShadowBolt:      return "ShadowBolt";
    case AbilityType::DrainLife:       return "DrainLife";
    case AbilityType::Curse:           return "Curse";
    case AbilityType::CorruptionPool:  return "CorruptionPool";
    case AbilityType::Hellfire:        return "Hellfire";
    case AbilityType::SoulSiphon:      return "SoulSiphon";
    case AbilityType::Cataclysm:       return "Cataclysm";
    case AbilityType::DemonForm:       return "DemonForm";
    case AbilityType::ShadowNova:      return "ShadowNova";
    default:                           return "";
    }
}
