#include "PlayerClass.h"
#include "GameBalance.h"
#include <string>
#include <vector>
#include <cstdio>

// Order must match the PlayerClass enum.
static const PlayerClassInfo kClassTable[(int)PlayerClass::Count] = {
    // name        prefix      description                                   playstyle                    resource explainer (shown on the select screen)                                                                     HP  MP  atk  spd   def  regen
    { "Mage",     "Mage",    "Master of the elements. Spells for every foe.", "Ranged caster - fragile",
      "MANA", "Runs on MP alone - every spell costs MP.\nBig pool and the fastest regen in the game.",                          80, 12, 30.f, 380.f, 0.f, 1.4f },
    { "Warrior",  "Warrior", "A wall of steel. Wades in and cleaves.",        "Tanky bruiser - high HP",
      "RAGE", "Builds when you land hits AND when you get hit;\nfades out of combat. Full bar = up to +35% ability\ndamage. Rage is a bonus - abilities still cost MP.", 100,  8, 40.f, 360.f, Balance::Stats::kClassStartDefense, 1.0f },
    { "Hunter",   "Hunter",  "Swift hunter who never lets you close.",        "Mobile skirmisher - fast",
      "MANA", "Runs on MP alone. Every 3rd basic shot MARKS the\ntarget - marked foes take bonus damage.",                       80,  9, 30.f, 415.f, 0.f, 1.0f },
    { "Rogue",    "Rogue",   "Fragile blur of daggers and crits.",            "Glass assassin - burst",
      "COMBO", "Quick hits bank Combo Points (the red pips).\nEviscerate spends them ALL for bonus damage.\nCombo is a bonus - abilities still cost MP.",                60,  8, 30.f, 435.f, 0.f, 1.0f },
    // Paladin/Warlock reuse existing art (Warrior/Mage) but have their own stats.
    { "Paladin",  "Warrior", "Holy bulwark. Punishes those who strike it.",   "Armoured holy tank",
      "FAITH", "Builds when you are struck and when holy hits land;\nnever decays. Up to +25% ability damage at full;\nDivine Storm consumes it. Abilities still cost MP.", 100,  9, 30.f, 350.f, Balance::Stats::kClassStartDefense, 1.1f },
    { "Warlock",  "Mage",    "Dark caster who drains life to survive.",       "Dark caster - life drain",
      "MANA", "Runs on MP alone. The kit steals life -\ndrain, curse, and lifesteal keep you standing.",                         80, 11, 30.f, 375.f, 0.f, 1.2f },
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
    return cls == PlayerClass::Mage || cls == PlayerClass::Warlock || cls == PlayerClass::Hunter;
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
    auto isHunter = [](AbilityType a) {
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
    case PlayerClass::Hunter:
        return isHunter(ability);    // dedicated bow kit
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
