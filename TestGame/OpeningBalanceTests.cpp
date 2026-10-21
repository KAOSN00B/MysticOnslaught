// Enforces the opening rule in GameBalance.h: at the start of a run, no class
// kills a standard enemy with one basic attack. Mirrors the two basic-attack
// formulas in Engine (melee swing / ranged basic shot) at power level 1.
#include "ClassAbilityBalance.h"
#include "GameBalance.h"
#include "PlayerClass.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

// Largest single basic hit at the opening, under DamagePolicy.h: the nominal
// value is float attack power (x ranged scale); a hit can carry at most the
// remainder of earlier hits, so the biggest possible hit is ceil(nominal).
static int OpeningBasicHit(PlayerClass cls)
{
    const float attack = GetPlayerClassInfo(cls).baseAttackPower;   // Character::GetAttackPowerValue
    const float nominal = ClassUsesRangedBasic(cls)
        ? attack * Balance::PlayerBasic::kRangedScale : attack;
    return std::max(1, (int)std::ceil(nominal - 1e-4f));
}

int main()
{
    struct Target { const char* name; float hp; };
    const Target standard[] = {
        { "Grunt",        Balance::Grunt::kBaseHealth },
        { "Fragile",      Balance::Fodder::kFragileHealth },
        { "Sporeling",    Balance::Fodder::kSporelingHealth },
        { "Big slime",    Balance::Fodder::kBigSlimeHealth },
        { "Shieldbearer", Balance::Fodder::kShieldbearerHealth },
        { "Warchief",     Balance::Fodder::kWarchiefHealth },
    };

    bool ok = true;
    int strongest = 0;
    for (int c = 0; c < (int)PlayerClass::Count; ++c)
    {
        const PlayerClass cls = (PlayerClass)c;
        const int hit = OpeningBasicHit(cls);
        strongest = std::max(strongest, hit);
        if (hit < 1) { std::printf("FAIL: %s basic hit is %d\n", GetPlayerClassInfo(cls).name, hit); ok = false; }
        for (const Target& t : standard)
        {
            const int hits = (int)std::ceil(t.hp / hit);
            if (hits < 2)
            {
                std::printf("FAIL: %s one-shots %s (%d dmg vs %.0f HP)\n",
                            GetPlayerClassInfo(cls).name, t.name, hit, t.hp);
                ok = false;
            }
        }
        std::printf("  %-8s opening basic hit %d -> grunt in %d hits\n",
                    GetPlayerClassInfo(cls).name, hit,
                    (int)std::ceil(Balance::Grunt::kBaseHealth / hit));
    }

    // The common grunt also survives the strongest swing on a Cleave-vulnerable
    // target (+25%), so the opener is never a single swing after one ability.
    const int vulnerableHit = (int)std::lround(strongest * 1.25f);
    if (Balance::Grunt::kBaseHealth <= vulnerableHit)
    {
        std::printf("FAIL: grunt (%.0f HP) dies to a vulnerable swing of %d\n",
                    Balance::Grunt::kBaseHealth, vulnerableHit);
        ok = false;
    }

    // Abilities: every non-ultimate class ability's plain level-1 hit stays at
    // or under the cap, and the cap never kills the weakest standard enemy.
    float weakest = standard[0].hp;
    for (const Target& t : standard) weakest = std::min(weakest, t.hp);
    const int cap = Balance::ClassAbility::kOpeningAbilityHitCap;
    if (cap >= weakest)
    {
        std::printf("FAIL: ability cap %d kills a %.0f HP standard enemy\n", cap, weakest);
        ok = false;
    }
    for (const auto& spec : Balance::ClassAbility::kHits)
    {
        int owner = -1;
        for (int c = 0; c < (int)PlayerClass::Count; ++c)
            if (ClassAllowsAbility((PlayerClass)c, spec.type)) { owner = c; break; }
        if (owner < 0)
        {
            std::printf("FAIL: ability %d in the hit table belongs to no class\n", (int)spec.type);
            ok = false;
            continue;
        }
        if (IsUltimateAbility(spec.type)) continue;
        const float atk = GetPlayerClassInfo((PlayerClass)owner).baseAttackPower;
        const int hit = std::max(1, (int)std::round(spec.base + atk * spec.perAtk));
        if (hit > cap)
        {
            std::printf("FAIL: %s ability %d opens at %d (cap %d)\n",
                        GetPlayerClassInfo((PlayerClass)owner).name, (int)spec.type, hit, cap);
            ok = false;
        }
    }
    // Character uses these same constants. Lightning Blink can hit a target on
    // both the path and landing burst, so check its combined direct damage.
    const int mageBolt = (int)Balance::AbilityDamage::kBoltBase;
    const int mageInstantSpread = (int)Balance::AbilityDamage::kSpreadBase * Balance::AbilityDamage::kInstantSpreadMult;
    if (mageBolt > cap || mageInstantSpread > cap || mageInstantSpread * 2 >= weakest)
    {
        std::printf("FAIL: Mage bolt %d / instant spread %d exceed cap %d\n", mageBolt, mageInstantSpread, cap);
        ok = false;
    }

    if (!ok) return 1;
    std::puts("Opening balance tests passed: baseline basic/table damage and Mage direct overlap stay below standard enemy HP.");
    return 0;
}
