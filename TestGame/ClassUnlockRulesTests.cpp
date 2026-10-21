#include "ClassUnlockRules.h"

#include <cassert>
#include <string>

int main()
{
    ClassUnlockProfile profile{};

    assert(GetClassUnlockStatus(PlayerClass::Warrior, profile).unlocked);
    assert(GetClassUnlockStatus(PlayerClass::Hunter, profile).unlocked);
    assert(GetClassUnlockStatus(PlayerClass::Mage, profile).unlocked);

    ClassUnlockStatus rogue = GetClassUnlockStatus(PlayerClass::Rogue, profile);
    assert(!rogue.unlocked);
    assert(std::string(rogue.reason) == "Defeat any domain boss");

    ClassUnlockStatus warlock = GetClassUnlockStatus(PlayerClass::Warlock, profile);
    assert(!warlock.unlocked);
    assert(std::string(warlock.reason) == "Clear 3 different domains");

    ClassUnlockStatus paladin = GetClassUnlockStatus(PlayerClass::Paladin, profile);
    assert(!paladin.unlocked);
    assert(std::string(paladin.reason) == "Win a run or clear 5 domains");

    profile.rogueUnlocked = true;
    profile.warlockUnlocked = true;
    profile.gameCompleted = true;
    assert(GetClassUnlockStatus(PlayerClass::Rogue, profile).unlocked);
    assert(GetClassUnlockStatus(PlayerClass::Warlock, profile).unlocked);
    assert(GetClassUnlockStatus(PlayerClass::Paladin, profile).unlocked);
    return 0;
}
