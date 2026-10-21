#include "ClassUnlockRules.h"

ClassUnlockStatus GetClassUnlockStatus(PlayerClass playerClass, const ClassUnlockProfile& profile)
{
    switch (playerClass)
    {
    case PlayerClass::Mage:
    case PlayerClass::Warrior:
    case PlayerClass::Hunter:
        return { true, "" };
    // Milestones are evaluated in MetaProgressionManager::EvaluateClassMilestones.
    case PlayerClass::Rogue:
        return { profile.rogueUnlocked, profile.rogueUnlocked ? "" : "Defeat any domain boss" };
    case PlayerClass::Warlock:
        return { profile.warlockUnlocked, profile.warlockUnlocked ? "" : "Clear 3 different domains" };
    case PlayerClass::Paladin:
        return { profile.gameCompleted, profile.gameCompleted ? "" : "Win a run or clear 5 domains" };
    default:
        return { false, "Unavailable" };
    }
}
