#include "SaveIds.h"

#include "AbilityType.h"
#include "GameTypes.h"
#include "Grimoire.h"
#include "MetaProgression.h"
#include "PlayerClass.h"
#include "Relic.h"

#include <iterator>

namespace
{
    constexpr const char* kAbilityIds[] = {
        "FireSpread", "IceSpread", "ElectricSpread", "FireBolt", "IceBolt", "ElectricBolt",
        "FireUltimate", "IceUltimate", "ElectricUltimate", "WarCleave", "Whirlwind", "ThrowingAxe",
        "Rend", "ShieldBash", "WarCry", "GroundSlam", "Rampage", "Earthshatter", "FanOfKnives",
        "Shadowstep", "PoisonVial", "Backstab", "SmokeBomb", "Eviscerate", "DeathMark", "BladeDance",
        "RainOfBlades", "PiercingShot", "Multishot", "FrostTrap", "ExplosiveArrow", "Roll", "Volley",
        "ArrowStorm", "Deadeye", "PiercingBarrage", "Smite", "Consecrate", "ShieldOfFaith", "HolyBolt",
        "HammerThrow", "LayOnHands", "DivineStorm", "AvengingWrath", "HammerOfJustice", "ShadowBolt",
        "DrainLife", "Curse", "CorruptionPool", "Hellfire", "SoulSiphon", "Cataclysm", "DemonForm",
        "ShadowNova",
    };
    static_assert(std::size(kAbilityIds) == (std::size_t)AbilityType::Count, "every AbilityType value needs a save id");

    constexpr const char* kBiomeIds[] = {
        "AncientCastle", "Caverns", "DemonsInsides", "DreamRealm", "Forest", "Graveyard", "Jungle",
        "LostCity", "TheSanctuary", "Wastelands",
    };
    static_assert(std::size(kBiomeIds) == (std::size_t)MetaProgressionManager::kBiomeCount, "every Biome value needs a save id");

    constexpr const char* kFeatIds[] = {
        "FirstBlood", "Pathfinder", "FinalDescent", "Conqueror", "Ascendant", "SwiftVictory",
        "Untouched", "EliteHunter", "Exterminator", "Collector", "Hoarder", "CurseBreaker", "Reshaped",
        "EchoKeeper", "BeastScholar", "RelicSage",
    };
    static_assert(std::size(kFeatIds) == (std::size_t)Feat::Count, "every Feat value needs a save id");

    constexpr const char* kUnlockIds[] = {
        "UnlockFireBolt", "UnlockIceBolt", "UnlockElectricBolt", "UnlockFireUltimate",
        "UnlockIceUltimate", "UnlockElectricUltimate", "StartingGold1", "StartingGold2",
        "StartingGold3", "GoldRetention1", "GoldRetention2", "GoldRetention3", "ManaFlow1",
        "ManaFlow2", "Vitality1", "Vitality2", "FreeShopReroll", "BonusStarterAbility", "Bulwark",
        "Heirloom", "CellSurge", "SecondWind", "Cartographer",
    };
    static_assert(std::size(kUnlockIds) == (std::size_t)MetaUnlockType::Count, "every MetaUnlockType value needs a save id");

    constexpr const char* kClassIds[] = {
        "Mage", "Warrior", "Hunter", "Rogue", "Paladin", "Warlock",
    };
    static_assert(std::size(kClassIds) == (std::size_t)PlayerClass::Count, "every PlayerClass value needs a save id");

    constexpr const char* kRelicIds[] = {
        "EmberHeart", "Wildfire", "Permafrost", "ShatterStrike", "Overcharge", "StormsReach",
        "KeenEdge", "Executioner", "Deadeye", "Bloodlust", "GlassCannon", "StoneSkin", "SecondWind",
        "Bulwark", "ThornmailRunes", "MidasTouch", "SoulSiphon", "Scavenger", "SwiftBoots",
        "ArcaneBattery", "Vampirism", "Berserker", "Momentum", "Reaper", "SmolderingBrand",
        "WarlordsCrest", "KindlingCore", "FrostLens", "StormCoil", "ResonancePrism", "QuarrySigil",
        "VenomGland", "ShadowReserve", "RuthlessEdge", "MirrorPlate", "ZealotsMark", "DarkCovenant",
        "HexLantern", "BloodChalice", "AttunementStone", "SpellbladeSigil", "HearthCharm",
        "ManaSiphon", "PyreHeart", "GlacialBattery", "ChainReaction", "SoulHarvest", "GiantSlayer",
        "Ambush", "Duelist", "GildedGauntlet", "Bully", "Afterimage", "Retaliation", "SpikedPauldrons",
        "LastStand", "Ironclad", "Hourglass", "CoinPurse", "Stoneheart",
    };
    static_assert(std::size(kRelicIds) == (std::size_t)RelicType::Count, "every RelicType value needs a save id");

    template <std::size_t N>
    const char* NameAt(const char* const (&names)[N], int index)
    {
        return (index >= 0 && index < (int)N) ? names[index] : nullptr;
    }

    template <typename Enum, std::size_t N>
    bool FindName(const char* const (&names)[N], const std::string& id, Enum& out)
    {
        for (std::size_t i = 0; i < N; ++i)
            if (id == names[i]) { out = (Enum)i; return true; }
        return false;
    }
}

const char* SaveIdOf(AbilityType value)    { return NameAt(kAbilityIds, (int)value); }
const char* SaveIdOf(Biome value)          { return NameAt(kBiomeIds, (int)value); }
const char* SaveIdOf(Feat value)           { return NameAt(kFeatIds, (int)value); }
const char* SaveIdOf(MetaUnlockType value) { return NameAt(kUnlockIds, (int)value); }
const char* SaveIdOf(PlayerClass value)    { return NameAt(kClassIds, (int)value); }
const char* SaveIdOf(RelicType value)      { return NameAt(kRelicIds, (int)value); }

bool SaveIdTo(const std::string& id, AbilityType& out)    { return FindName(kAbilityIds, id, out); }
bool SaveIdTo(const std::string& id, Biome& out)          { return FindName(kBiomeIds, id, out); }
bool SaveIdTo(const std::string& id, Feat& out)           { return FindName(kFeatIds, id, out); }
bool SaveIdTo(const std::string& id, MetaUnlockType& out) { return FindName(kUnlockIds, id, out); }
bool SaveIdTo(const std::string& id, PlayerClass& out)    { return FindName(kClassIds, id, out); }
bool SaveIdTo(const std::string& id, RelicType& out)      { return FindName(kRelicIds, id, out); }

const char* BestiarySaveId(int bestiaryIndex)
{
    return (bestiaryIndex >= 0 && bestiaryIndex < BestiaryCount()) ? GetBestiaryEntry(bestiaryIndex).type : nullptr;
}

bool BestiaryIndexFromSaveId(const std::string& id, int& out)
{
    for (int i = 0; i < BestiaryCount(); ++i)
        if (id == GetBestiaryEntry(i).type) { out = i; return true; }
    return false;
}
