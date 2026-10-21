#include "Grimoire.h"

namespace
{
    // Ordered as the Bestiary lists them: the rank and file, the elite
    // bruisers, then each domain's guardian, the final one last.
    const BestiaryEntry kBestiary[] = {
        { "Basic",          "Grunt",           "Foot soldier",          "The domains' rank and file: they close in and swing.", false, Biome::Forest },
        { "Cyclops",        "Cyclops",         "Heavy",                 "Sweeps a laser beam and fires scatter shots.",          false, Biome::Forest },
        { "SkeletonArcher", "Skeleton Archer", "Ranged",                "Keeps its distance and looses aimed arrows.",           false, Biome::Forest },
        { "FlameWisp",      "Flame Wisp",      "Caster",                "Blinks around you and lobs fire bolts.",                false, Biome::Forest },
        { "Slime",          "Slime",           "Swarm",                 "Big slimes split in two when they die.",                false, Biome::Forest },
        { "Sporeling",      "Sporeling",       "Hazard",                "Bursts into a poison cloud when it dies.",              false, Biome::Forest },
        { "Shieldbearer",   "Shieldbearer",    "Defender",              "Its shield blocks every frontal hit: flank it.",        false, Biome::Forest },
        { "Phantom",        "Phantom",         "Ghost",                 "Unhittable while phased: strike while it is solid.",    false, Biome::Forest },
        { "BomberImp",      "Bomber Imp",      "Kamikaze",              "Explodes beside you, and when it is killed.",           false, Biome::Forest },
        { "Warchief",       "Warchief",        "Commander",             "Its banner makes nearby enemies 30% faster.",           false, Biome::Forest },
        { "LivingBlade",    "Living Blade",    "Duelist",               "Dashes in bursts: punish it while it rests.",           false, Biome::Forest },
        { "Ogre",           "Ogre",            "Elite bruiser",         "Telegraphs, then charges through anything in its path.", false, Biome::Forest },
        { "Infernal",       "Infernal Brute",  "Elite bruiser",         "Every hit it lands sets you on fire.",                  false, Biome::Forest },
        { "Bonechill",      "Bonechill",       "Elite bruiser",         "Every hit it lands slows you.",                         false, Biome::Forest },
        { "Stormclub",      "Stormclub",       "Elite bruiser",         "Its smash blasts you across the room.",                 false, Biome::Forest },
        { "Venomfang",      "Venomfang",       "Elite bruiser",         "Fast, and every bite leaves venom.",                    false, Biome::Forest },
        { "Werewolf",       "Werewolf",        "Forest guardian",       "Fast claw combos and a telegraphed pounce.",            true,  Biome::Forest },
        { "ChompBug",       "Chomp Bug",       "Jungle guardian",       "Flies, dive-bombs along a line and spits acid.",        true,  Biome::Jungle },
        { "PumpkinJack",    "Pumpkin Jack",    "Graveyard guardian",    "Teleports and volleys; enraged, he appears behind you.", true, Biome::Graveyard },
        { "Minotaur",       "Minotaur",        "Ancient Castle guardian", "Bull rushes: bait one into a wall to stun him.",      true,  Biome::AncientCastle },
        { "Osiris",         "Osiris",          "Lost City guardian",    "Teleports away and fires rings and volleys of bolts.",  true,  Biome::LostCity },
        { "TitanGuard",     "Titan Guard",     "Sanctuary guardian",    "His shield always faces you: hit him from behind.",     true,  Biome::TheSanctuary },
        { "ToxicVermin",    "Toxic Vermin",    "Wastelands guardian",   "Burrows, erupts beneath you, leaves poison pools.",     true,  Biome::Wastelands },
        { "AncientBear",    "Ancient Bear",    "Dream Realm guardian",  "Roars to drag you in, then slams.",                     true,  Biome::DreamRealm },
        { "AbyssSlime",     "Abyss Slime",     "Caverns guardian",      "Hops at you, leaves acid, and leaps to crush.",         true,  Biome::Caverns },
        { "Molarbeast",     "Molarbeast",      "The final guardian",    "Lava volleys, charges and a stampede of fire.",         true,  Biome::DemonsInsides },
    };
    constexpr int kBestiaryCount = (int)(sizeof(kBestiary) / sizeof(kBestiary[0]));

    const FeatInfo kFeats[(int)Feat::Count] = {
        { "First Blood",     "Defeat a domain's guardian." },
        { "Pathfinder",      "Reach the third domain in one run." },
        { "The Final Descent", "Reach Demon's Insides." },
        { "Conqueror",       "Win a run." },
        { "Ascendant",       "Win a run on Ascension 1 or higher." },
        { "Swift Victory",   "Win a run in under 45 minutes." },
        { "Untouched",       "Clear a boss room without taking damage." },
        { "Elite Hunter",    "Slay 3 elites in one run." },
        { "Exterminator",    "Slay 250 enemies in one run." },
        { "Collector",       "Hold 8 relics at once." },
        { "Hoarder",         "Carry 1,000 gold at once." },
        { "Curse Breaker",   "Outlast a Cursed Cache's curse." },
        { "Reshaped",        "Accept a Mutation Altar's gift." },
        { "Echo Keeper",     "Bank 1,000 Echoes over all your runs." },
        { "Beast Scholar",   "Record 15 creatures in the Bestiary." },
        { "Relic Sage",      "Record 40 relics in the Grimoire." },
    };
}

int BestiaryCount() { return kBestiaryCount; }

const BestiaryEntry& GetBestiaryEntry(int index)
{
    return kBestiary[(index >= 0 && index < kBestiaryCount) ? index : 0];
}

int BestiaryIndexForType(const std::string& snapshotType)
{
    std::string type = snapshotType;
    if (type.rfind("Elite_", 0) == 0) type = type.substr(6);
    if (type == "SlimeSmall" || type == "SlimeBig") type = "Slime";
    for (int i = 0; i < kBestiaryCount; ++i)
        if (type == kBestiary[i].type) return i;
    return -1;
}

const FeatInfo& GetFeatInfo(Feat feat)
{
    const int i = (int)feat;
    return kFeats[(i >= 0 && i < (int)Feat::Count) ? i : 0];
}
