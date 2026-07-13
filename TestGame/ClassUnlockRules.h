#pragma once

#include "PlayerClass.h"

struct ClassUnlockProfile
{
    bool rogueUnlocked = false;
    bool warlockUnlocked = false;
    bool gameCompleted = false;
};

struct ClassUnlockStatus
{
    bool unlocked = false;
    const char* reason = "";
};

ClassUnlockStatus GetClassUnlockStatus(PlayerClass playerClass, const ClassUnlockProfile& profile);
