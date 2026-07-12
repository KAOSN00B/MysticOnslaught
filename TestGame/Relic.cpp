#include "Relic.h"

// Order must match the RelicType enum exactly.
// Balance truth pass: every description below states the REAL runtime effect
// (Character.cpp AddRelic / ScaleOutgoingDamage). The old text advertised far
// bigger numbers than the code ever applied — code is canon, text was fixed.
static const RelicInfo kRelicTable[(int)RelicType::Count] = {
    // Fire
    { "Ember Heart",    "Burning enemies take\n+25% damage",           RelicRarity::Rare,   RelicArchetype::Fire },
    { "Wildfire",       "Enemies that die burning\nexplode in flames", RelicRarity::Epic,   RelicArchetype::Fire },
    // Ice
    { "Permafrost",     "Frozen enemies take\n+25% damage",            RelicRarity::Rare,   RelicArchetype::Ice },
    { "Shatter Strike", "Killing a frozen enemy\nfreezes nearby foes", RelicRarity::Epic,   RelicArchetype::Ice },
    // Electric
    { "Overcharge",     "Charged enemies take\n+25% damage",           RelicRarity::Rare,   RelicArchetype::Electric },
    { "Storm's Reach",  "Charged enemies that die\nshock nearby foes", RelicRarity::Epic,   RelicArchetype::Electric },
    // Offense
    { "Keen Edge",      "+15% to all damage",                          RelicRarity::Common, RelicArchetype::Offense },
    { "Executioner",    "+35% damage to enemies\nbelow 25% HP",        RelicRarity::Rare,   RelicArchetype::Offense },
    { "Deadeye",        "20% chance to crit\nfor +50% damage",         RelicRarity::Rare,   RelicArchetype::Offense },
    { "Bloodlust",      "+25% to all damage",                          RelicRarity::Common, RelicArchetype::Offense },
    { "Glass Cannon",   "+35% damage, but\n-2 max HP",                 RelicRarity::Epic,   RelicArchetype::Offense },
    // Defense
    { "Stone Skin",     "+1 armour slot",                              RelicRarity::Common, RelicArchetype::Defense },
    { "Second Wind",    "+20% max HP",                                 RelicRarity::Rare,   RelicArchetype::Defense },
    { "Bulwark",        "+1 armour slot and\nheal 3 HP",              RelicRarity::Rare,   RelicArchetype::Defense },
    { "Thornmail Runes","+2 max HP",                                   RelicRarity::Common, RelicArchetype::Defense },
    // Economy
    { "Midas Touch",    "+60% gold from kills",                        RelicRarity::Common, RelicArchetype::Economy },
    { "Soul Siphon",    "+60% Echoes\nfrom kills",              RelicRarity::Rare,   RelicArchetype::Economy },
    { "Scavenger",      "Enemies drop healing\nfar more often",        RelicRarity::Common, RelicArchetype::Economy },
    // Utility
    { "Swift Boots",    "+8% move speed",                             RelicRarity::Common, RelicArchetype::Utility },
    { "Arcane Battery", "+8 max mana and\n+12% mana regen",           RelicRarity::Rare,   RelicArchetype::Utility },
    { "Vampirism",      "Heal 1 HP every\n5 kills",                   RelicRarity::Rare,   RelicArchetype::Utility },
    { "Berserker",      "+30% damage while\nbelow 40% HP",            RelicRarity::Rare,   RelicArchetype::Utility },
    { "Momentum",       "+10% move speed and\n+10% damage",          RelicRarity::Common, RelicArchetype::Utility },
    { "Reaper",         "+15% damage; heal 4 HP\non elite/boss kill", RelicRarity::Rare,   RelicArchetype::Utility },
};

const RelicInfo& GetRelicInfo(RelicType type)
{
    return kRelicTable[(int)type];
}

const char* GetRelicRarityName(RelicRarity rarity)
{
    switch (rarity)
    {
    case RelicRarity::Common: return "Common";
    case RelicRarity::Rare:   return "Rare";
    case RelicRarity::Epic:   return "Epic";
    default:                  return "";
    }
}

const char* GetRelicArchetypeName(RelicArchetype archetype)
{
    switch (archetype)
    {
    case RelicArchetype::Fire:     return "Fire";
    case RelicArchetype::Ice:      return "Ice";
    case RelicArchetype::Electric: return "Electric";
    case RelicArchetype::Offense:  return "Offense";
    case RelicArchetype::Defense:  return "Defense";
    case RelicArchetype::Economy:  return "Economy";
    case RelicArchetype::Utility:  return "Utility";
    default:                       return "";
    }
}
