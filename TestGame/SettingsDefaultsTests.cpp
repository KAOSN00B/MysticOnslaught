#include "SettingsManager.h"

#include <cassert>
#include <cstdio>

int main()
{
    GameSettings settings{};
    assert(settings.abilityAimToggle);
    std::puts("Settings default tests passed");
    return 0;
}
