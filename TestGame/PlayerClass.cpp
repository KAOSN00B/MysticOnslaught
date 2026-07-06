#include "PlayerClass.h"
#include <string>
#include <vector>
#include <cstdio>

// Order must match the PlayerClass enum.
static const PlayerClassInfo kClassTable[(int)PlayerClass::Count] = {
    // name        prefix      description                                   playstyle                    HP  MP  atk  spd   arm  regen
    { "Mage",     "Mage",    "Master of the elements. Spells for every foe.", "Ranged caster - fragile",     7, 12, 2.f, 380.f, 0, 1.4f },
    { "Warrior",  "Warrior", "A wall of steel. Wades in and cleaves.",        "Tanky bruiser - high HP",    11,  8, 5.f, 360.f, 1, 1.0f },
    { "Ranger",   "Ranger",  "Swift hunter who never lets you close.",        "Mobile skirmisher - fast",    8,  9, 3.f, 415.f, 0, 1.0f },
    { "Rogue",    "Rogue",   "Fragile blur of daggers and crits.",            "Glass assassin - burst",      6,  8, 3.f, 435.f, 0, 1.0f },
    // Paladin/Warlock reuse existing art (Warrior/Mage) but have their own stats.
    { "Paladin",  "Warrior", "Holy bulwark. Punishes those who strike it.",   "Armoured holy tank",         12,  9, 4.f, 350.f, 2, 1.1f },
    { "Warlock",  "Mage",    "Dark caster who drains life to survive.",       "Dark caster - life drain",    8, 11, 2.f, 375.f, 0, 1.2f },
};

const PlayerClassInfo& GetPlayerClassInfo(PlayerClass cls)
{
    return kClassTable[(int)cls];
}

// ── Appearance roster ─────────────────────────────────────────────────────────
// Hero01..Hero29 are selectable looks; Hero30 is reserved as the cells-shop NPC.
// Files are extracted from the TopDown Character Sprites packs into Hero/.
static const int kAppearanceRosterCount = 29;

// Lazily-built, stable string storage so the const char* returns stay valid.
static const std::vector<std::string>& AppearancePrefixes()
{
    static std::vector<std::string> prefixes = [] {
        std::vector<std::string> v;
        for (int i = 1; i <= kAppearanceRosterCount; i++)
        {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "Hero%02d", i);
            v.emplace_back(buf);
        }
        return v;
    }();
    return prefixes;
}
static const std::vector<std::string>& AppearanceNames()
{
    static std::vector<std::string> names = [] {
        std::vector<std::string> v;
        for (int i = 1; i <= kAppearanceRosterCount; i++)
        {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "Hero %02d", i);
            v.emplace_back(buf);
        }
        return v;
    }();
    return names;
}

bool ClassUsesRangedBasic(PlayerClass cls)
{
    return cls == PlayerClass::Mage || cls == PlayerClass::Warlock || cls == PlayerClass::Ranger;
}

int GetAppearanceCount() { return kAppearanceRosterCount; }

const char* GetAppearancePrefix(int index)
{
    if (index < 0 || index >= kAppearanceRosterCount) index = 0;
    return AppearancePrefixes()[index].c_str();
}

const char* GetAppearanceName(int index)
{
    if (index < 0 || index >= kAppearanceRosterCount) index = 0;
    return AppearanceNames()[index].c_str();
}

const char* GetDefaultAppearancePrefix() { return "Hero03"; }   // classic blonde hero
const char* GetCellsShopMerchantPrefix() { return "Hero30"; }   // reserved NPC look

bool ClassAllowsAbility(PlayerClass cls, AbilityType ability)
{
    // Helper element groupings over the existing elemental abilities.
    auto isFire = [](AbilityType a) {
        return a == AbilityType::FireSpread || a == AbilityType::FireBolt || a == AbilityType::FireUltimate;
    };
    auto isIce = [](AbilityType a) {
        return a == AbilityType::IceSpread || a == AbilityType::IceBolt || a == AbilityType::IceUltimate;
    };
    auto isElectric = [](AbilityType a) {
        return a == AbilityType::ElectricSpread || a == AbilityType::ElectricBolt || a == AbilityType::ElectricUltimate;
    };
    auto isWarrior = [](AbilityType a) {
        return a == AbilityType::WarCleave  || a == AbilityType::Whirlwind || a == AbilityType::ThrowingAxe ||
               a == AbilityType::Rend       || a == AbilityType::ShieldBash|| a == AbilityType::WarCry ||
               a == AbilityType::GroundSlam || a == AbilityType::Rampage   || a == AbilityType::Earthshatter;
    };
    auto isRogue = [](AbilityType a) {
        return a == AbilityType::FanOfKnives || a == AbilityType::Shadowstep || a == AbilityType::PoisonVial ||
               a == AbilityType::Backstab    || a == AbilityType::SmokeBomb  || a == AbilityType::Eviscerate ||
               a == AbilityType::DeathMark   || a == AbilityType::BladeDance || a == AbilityType::RainOfBlades;
    };
    auto isRanger = [](AbilityType a) {
        return a == AbilityType::PiercingShot   || a == AbilityType::Multishot || a == AbilityType::FrostTrap ||
               a == AbilityType::ExplosiveArrow || a == AbilityType::Roll      || a == AbilityType::Volley ||
               a == AbilityType::ArrowStorm     || a == AbilityType::Deadeye   || a == AbilityType::PiercingBarrage;
    };
    auto isPaladin = [](AbilityType a) {
        return a == AbilityType::Smite       || a == AbilityType::Consecrate    || a == AbilityType::ShieldOfFaith ||
               a == AbilityType::HolyBolt    || a == AbilityType::HammerThrow   || a == AbilityType::LayOnHands ||
               a == AbilityType::DivineStorm || a == AbilityType::AvengingWrath || a == AbilityType::HammerOfJustice;
    };
    auto isWarlock = [](AbilityType a) {
        return a == AbilityType::ShadowBolt || a == AbilityType::DrainLife || a == AbilityType::Curse ||
               a == AbilityType::CorruptionPool || a == AbilityType::Hellfire || a == AbilityType::SoulSiphon ||
               a == AbilityType::Cataclysm  || a == AbilityType::DemonForm || a == AbilityType::ShadowNova;
    };

    switch (cls)
    {
    case PlayerClass::Mage:
        return isFire(ability) || isIce(ability) || isElectric(ability); // full elemental arsenal
    case PlayerClass::Warrior:
        return isWarrior(ability);   // dedicated melee bruiser kit
    case PlayerClass::Ranger:
        return isRanger(ability);    // dedicated bow kit
    case PlayerClass::Rogue:
        return isRogue(ability);     // dedicated assassin kit
    case PlayerClass::Paladin:
        return isPaladin(ability);   // dedicated holy-tank kit
    case PlayerClass::Warlock:
        return isWarlock(ability);   // dedicated dark-caster kit
    default:
        return false;
    }
}
