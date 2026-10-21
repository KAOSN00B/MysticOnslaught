#include "Relic.h"
#include "PlayerClass.h"

#include <algorithm>

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
    { "Keen Edge",      "+35% damage to enemies\nat full health",      RelicRarity::Common, RelicArchetype::Offense },
    { "Executioner",    "+35% damage to enemies\nbelow 25% HP",        RelicRarity::Rare,   RelicArchetype::Offense },
    { "Deadeye",        "+20% crit chance",                            RelicRarity::Rare,   RelicArchetype::Offense },
    { "Bloodlust",      "Kills grant +25% damage\nfor 4 seconds",      RelicRarity::Common, RelicArchetype::Offense },
    { "Glass Cannon",   "+35% damage, but\n-20 max HP",                RelicRarity::Epic,   RelicArchetype::Offense },
    // Defense
    { "Stone Skin",     "+10 Defense",                                 RelicRarity::Common, RelicArchetype::Defense },
    { "Second Wind",    "+20% max HP",                                 RelicRarity::Rare,   RelicArchetype::Defense },
    { "Bulwark",        "+10 Defense and\nheal 30 HP",                RelicRarity::Rare,   RelicArchetype::Defense },
    { "Thornmail Runes","+20 max HP",                                  RelicRarity::Common, RelicArchetype::Defense },
    // Economy
    { "Midas Touch",    "+60% gold from kills",                        RelicRarity::Common, RelicArchetype::Economy },
    { "Soul Siphon",    "+60% Echoes\nfrom kills",              RelicRarity::Rare,   RelicArchetype::Economy },
    { "Scavenger",      "Enemies drop healing\nfar more often",        RelicRarity::Common, RelicArchetype::Economy },
    // Utility
    { "Swift Boots",    "+12% move speed, dash\nrecharges 20% faster", RelicRarity::Common, RelicArchetype::Utility },
    { "Arcane Battery", "+8 max mana and\n+12% mana regen",           RelicRarity::Rare,   RelicArchetype::Utility },
    { "Vampirism",      "Heal 10 HP every\n5 kills",                  RelicRarity::Rare,   RelicArchetype::Utility },
    { "Berserker",      "+30% damage while\nbelow 40% HP",            RelicRarity::Rare,   RelicArchetype::Utility },
    { "Momentum",       "+10% move speed; +15%\ndamage on the move",   RelicRarity::Common, RelicArchetype::Utility },
    { "Reaper",         "+15% damage; heal 40 HP\non elite/boss kill",RelicRarity::Rare,   RelicArchetype::Utility },

    // ── Expansion ────────────────────────────────────────────────────────────
    // Class mechanics
    { "Smoldering Brand", "Rage drains\n50% slower",                   RelicRarity::Rare,   RelicArchetype::Offense },
    { "Warlord's Crest",  "+25% ability damage\nat full Rage",         RelicRarity::Epic,   RelicArchetype::Offense },
    { "Kindling Core",    "+20% fire spell\ndamage",                   RelicRarity::Common, RelicArchetype::Fire },
    { "Frost Lens",       "+20% ice spell\ndamage",                    RelicRarity::Common, RelicArchetype::Ice },
    { "Storm Coil",       "+20% lightning spell\ndamage",              RelicRarity::Common, RelicArchetype::Electric },
    { "Resonance Prism",  "Elemental reactions\ndeal +50% damage",     RelicRarity::Epic,   RelicArchetype::Utility },
    { "Quarry Sigil",     "Marked prey takes\n+15% more damage",       RelicRarity::Rare,   RelicArchetype::Offense },
    { "Venom Gland",      "Poison deals\n+50% damage",                 RelicRarity::Rare,   RelicArchetype::Offense },
    { "Shadow Reserve",   "+1 max combo point",                        RelicRarity::Rare,   RelicArchetype::Utility },
    { "Ruthless Edge",    "Finishers gain +10%\ndamage per combo point", RelicRarity::Epic, RelicArchetype::Offense },
    { "Mirror Plate",     "Aegis reflects\n50% more damage",           RelicRarity::Rare,   RelicArchetype::Defense },
    { "Zealot's Mark",    "Each Retribution stack\nadds +6% damage",   RelicRarity::Rare,   RelicArchetype::Offense },
    { "Dark Covenant",    "Cursed enemies take\n+15% more damage",     RelicRarity::Rare,   RelicArchetype::Offense },
    { "Hex Lantern",      "Curses last\n50% longer",                   RelicRarity::Common, RelicArchetype::Utility },
    { "Blood Chalice",    "Life drain is\n50% stronger",               RelicRarity::Rare,   RelicArchetype::Defense },
    { "Attunement Stone", "Your class resource\nbuilds 25% faster",    RelicRarity::Common, RelicArchetype::Utility },
    { "Spellblade Sigil", "+15% ability damage",                       RelicRarity::Rare,   RelicArchetype::Offense },
    { "Hearth Charm",     "Heal 20 HP whenever\nyou clear a room",     RelicRarity::Common, RelicArchetype::Defense },
    // On kill
    { "Mana Siphon",      "Kills restore\n1 mana",                     RelicRarity::Common, RelicArchetype::Utility },
    { "Pyre Heart",       "Burning kills\nheal 10 HP",                 RelicRarity::Rare,   RelicArchetype::Fire },
    { "Glacial Battery",  "Frozen kills restore\n3 mana",              RelicRarity::Rare,   RelicArchetype::Ice },
    { "Chain Reaction",   "Kills have a 25% chance\nto blast foes for 30", RelicRarity::Epic, RelicArchetype::Offense },
    { "Soul Harvest",     "Elite and boss kills\ngrant 15 Echoes",     RelicRarity::Rare,   RelicArchetype::Economy },
    // On hit
    { "Giant Slayer",     "+25% damage to\nelites and bosses",         RelicRarity::Rare,   RelicArchetype::Offense },
    { "Ambush",           "+40% damage in the first\n4 seconds of a fight", RelicRarity::Common, RelicArchetype::Offense },
    { "Duelist",          "+30% damage when only\none enemy remains",  RelicRarity::Rare,   RelicArchetype::Offense },
    { "Gilded Gauntlet",  "+5% damage per 100 gold\ncarried (max +30%)", RelicRarity::Rare, RelicArchetype::Economy },
    { "Bully",            "+20% damage to enemies\nbelow half health", RelicRarity::Common, RelicArchetype::Offense },
    { "Afterimage",       "+40% damage for\n1 second after a dash",    RelicRarity::Rare,   RelicArchetype::Utility },
    { "Retaliation",      "Getting hit grants +30%\ndamage for 3 seconds", RelicRarity::Rare, RelicArchetype::Defense },
    // Defense, utility, economy
    { "Spiked Pauldrons", "When hit, deal 20 damage\nto enemies near you", RelicRarity::Common, RelicArchetype::Defense },
    { "Last Stand",       "Once per room, a lethal\nhit leaves you at 10 HP", RelicRarity::Epic, RelicArchetype::Defense },
    { "Ironclad",         "Clearing a room: +3\nDefense (max +15)",     RelicRarity::Rare,   RelicArchetype::Defense },
    { "Hourglass",        "Ability cooldowns\nrecover 25% faster",     RelicRarity::Epic,   RelicArchetype::Utility },
    { "Coin Purse",       "+15 gold whenever\nyou clear a room",       RelicRarity::Common, RelicArchetype::Economy },
    { "Stoneheart",       "+40% max HP, but\n-10% move speed",         RelicRarity::Rare,   RelicArchetype::Defense },
};

const RelicInfo& GetRelicInfo(RelicType type)
{
    return kRelicTable[(int)type];
}

float RelicDamageBonus(const bool* owned, const RelicHitContext& hit)
{
    auto has = [&](RelicType t) { return owned != nullptr && owned[(int)t]; };
    float bonus = 0.f;

    // Unconditional bonuses each carry a cost or a second effect.
    if (has(RelicType::GlassCannon)) bonus += 0.35f;   // -20 max HP
    if (has(RelicType::Reaper))      bonus += 0.15f;   // + elite/boss kill heal

    // Situational bonuses: each rewards a different way of fighting.
    if (has(RelicType::KeenEdge)    && hit.targetHpFraction >= 0.999f) bonus += 0.35f;  // openers
    if (has(RelicType::Executioner) && hit.targetHpFraction <= 0.25f)  bonus += 0.35f;  // finishers
    if (has(RelicType::Bloodlust)   && hit.bloodlustActive)            bonus += 0.25f;  // crowds
    if (has(RelicType::Momentum)    && hit.playerMoving)               bonus += 0.15f;  // hit-and-run
    if (has(RelicType::Berserker)   && hit.playerHpFraction < 0.40f)   bonus += 0.30f;  // low HP

    // Expansion situations.
    if (has(RelicType::GiantSlayer) && hit.targetEliteOrBoss)       bonus += 0.25f;  // big targets
    if (has(RelicType::Ambush)      && hit.earlyInFight)            bonus += 0.40f;  // fast openings
    if (has(RelicType::Duelist)     && hit.lastEnemy)               bonus += 0.30f;  // the last one standing
    if (has(RelicType::Bully)       && hit.targetHpFraction < 0.5f) bonus += 0.20f;  // pressing the wounded
    if (has(RelicType::Afterimage)  && hit.recentlyDashed)          bonus += 0.40f;  // dash-strike
    if (has(RelicType::Retaliation) && hit.recentlyHurt)            bonus += 0.30f;  // trading blows
    if (has(RelicType::GildedGauntlet))                                              // hoarding
        bonus += std::min(0.30f, 0.05f * (float)(std::max(0, hit.playerGold) / 100));

    // Status synergies.
    if (hit.targetFrozen  && has(RelicType::Permafrost)) bonus += 0.25f;
    if (hit.targetBurning && has(RelicType::EmberHeart)) bonus += 0.25f;
    if (hit.targetCharged && has(RelicType::Overcharge)) bonus += 0.25f;
    return bonus;
}

bool RelicFitsClass(RelicType type, PlayerClass cls)
{
    // Which classes can apply each status (class kits in AbilityType.h and
    // Engine::HandleClassAbilityCast): burn = Mage spells and Warlock
    // Hellfire/Cataclysm; freeze/stun = every class except the Warlock (class
    // stuns use the freeze state); electric charge = Mage lightning only.
    const bool burns   = cls == PlayerClass::Mage || cls == PlayerClass::Warlock;
    const bool freezes = cls != PlayerClass::Warlock;
    const bool charges = cls == PlayerClass::Mage;
    // Class mechanics: Rage (Warrior), Faith (Paladin), combo points (Rogue);
    // poison (Rogue knives and vials, Hunter Multishot, Warlock curses); life
    // drain (Warrior Rampage, Rogue Blade Dance, Warlock drains); ability
    // power scales every class kit except the Mage's elemental spells.
    const bool poisons = cls == PlayerClass::Rogue || cls == PlayerClass::Hunter || cls == PlayerClass::Warlock;
    const bool drains  = cls == PlayerClass::Warrior || cls == PlayerClass::Rogue || cls == PlayerClass::Warlock;
    const bool hasResource = cls == PlayerClass::Warrior || cls == PlayerClass::Paladin || cls == PlayerClass::Rogue;
    switch (type)
    {
    case RelicType::EmberHeart:
    case RelicType::Wildfire:
    case RelicType::PyreHeart:       return burns;
    case RelicType::Permafrost:
    case RelicType::ShatterStrike:
    case RelicType::GlacialBattery:  return freezes;
    case RelicType::Overcharge:
    case RelicType::StormsReach:     return charges;
    case RelicType::SmolderingBrand:
    case RelicType::WarlordsCrest:   return cls == PlayerClass::Warrior;
    case RelicType::KindlingCore:
    case RelicType::FrostLens:
    case RelicType::StormCoil:
    case RelicType::ResonancePrism:  return cls == PlayerClass::Mage;
    case RelicType::QuarrySigil:     return cls == PlayerClass::Hunter;
    case RelicType::VenomGland:      return poisons;
    case RelicType::ShadowReserve:
    case RelicType::RuthlessEdge:    return cls == PlayerClass::Rogue;
    case RelicType::MirrorPlate:
    case RelicType::ZealotsMark:     return cls == PlayerClass::Paladin;
    case RelicType::DarkCovenant:
    case RelicType::HexLantern:      return cls == PlayerClass::Warlock;
    case RelicType::BloodChalice:    return drains;
    case RelicType::AttunementStone: return hasResource;
    case RelicType::SpellbladeSigil: return cls != PlayerClass::Mage;
    default:                         return true;
    }
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
