#include "ClassUnlockRules.h"

ClassUnlockStatus GetClassUnlockStatus(PlayerClass playerClass, const ClassUnlockProfile& profile)
{
    switch (playerClass)
    {
    case PlayerClass::Mage:
    case PlayerClass::Warrior:
    case PlayerClass::Hunter:
        return { true, "" };
    case PlayerClass::Rogue:
        return { profile.rogueUnlocked, profile.rogueUnlocked ? "" : "Complete Rogue milestone" };
    case PlayerClass::Warlock:
        return { profile.warlockUnlocked, profile.warlockUnlocked ? "" : "Complete Warlock milestone" };
    case PlayerClass::Paladin:
        return { profile.gameCompleted, profile.gameCompleted ? "" : "Complete a run" };
    default:
        return { false, "Unavailable" };
    }
}
