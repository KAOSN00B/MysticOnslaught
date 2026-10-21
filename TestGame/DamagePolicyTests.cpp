// Regression coverage for DamagePolicy.h: one rounding step per hit with the
// fractional remainder carried, instead of ceil() at several stages.
//
// Build: tools\run_tests.ps1 -Suite DamagePolicy

#include "DamagePolicy.h"
#include "GameBalance.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

static int g_checks = 0;
#define CHECK(x) do { ++g_checks; if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)

static int Deal(float nominal, int hits, float& carry, int* maxHit = nullptr)
{
    int total = 0, biggest = 0;
    for (int i = 0; i < hits; ++i)
    {
        const int d = DamagePolicy::ResolveHitDamage(nominal, carry);
        total += d;
        if (d > biggest) biggest = d;
    }
    if (maxHit) *maxHit = biggest;
    return total;
}

int main()
{
    using DamagePolicy::ResolveHitDamage;

    // 1. No modifiers: authored integers pass through untouched, no remainder.
    {
        float carry = 0.f;
        for (int i = 0; i < 50; ++i) CHECK(ResolveHitDamage(4.f, carry) == 4);
        CHECK(carry < 1e-3f);
    }

    // 2. The reported breakpoint: +15% on a 2-damage hit. The old ceil() made
    //    every hit 3 (+50%); now the first hit is 2 and the average is 2.3.
    {
        float carry = 0.f;
        const float nominal = 2.f * 1.15f;
        CHECK(ResolveHitDamage(nominal, carry) == 2);
        carry = 0.f;
        int biggest = 0;
        CHECK(Deal(nominal, 10, carry, &biggest) == 23);
        CHECK(biggest == 3);
        carry = 0.f;
        CHECK(Deal(nominal, 1000, carry) == 2300);
        // Hits to kill a 6 HP target (a grunt before the x10 number rescale): the
        // old rule needed 2 (3+3); now 3 (2+2+2).
        constexpr int kSmallTargetHealth = 6;
        carry = 0.f;
        int hp = kSmallTargetHealth, hits = 0;
        while (hp > 0) { hp -= ResolveHitDamage(nominal, carry); ++hits; }
        CHECK(hits == 3);
        CHECK((int)std::ceil(kSmallTargetHealth / std::ceil(nominal)) == 2);   // old behaviour, for the record
    }

    // 3. Any landed hit deals at least 1, without banking "debt".
    {
        float carry = 0.f;
        for (int i = 0; i < 5; ++i) CHECK(ResolveHitDamage(0.4f, carry) == 1);
        CHECK(carry == 0.f);
    }

    // 4. Non-positive or invalid nominal damage deals nothing and leaves the carry alone.
    {
        float carry = 0.5f;
        CHECK(ResolveHitDamage(0.f, carry) == 0);
        CHECK(ResolveHitDamage(-3.f, carry) == 0);
        CHECK(ResolveHitDamage(NAN, carry) == 0);
        CHECK(carry == 0.5f);
    }

    // 5. Float noise never drifts an exact value: the ranged scale keeps the
    //    authored opening shot at 2 for every one of 10,000 shots.
    {
        float carry = 0.f;
        const float rangedOpening = 3.f * Balance::PlayerBasic::kRangedScale;
        int biggest = 0;
        CHECK(Deal(rangedOpening, 10000, carry, &biggest) == 20000);
        CHECK(biggest == 2);
    }

    // 6. Fractional attack upgrades now count (they were truncated to int before).
    {
        float carry = 0.f;
        CHECK(Deal(3.5f, 100, carry) == 350);
        carry = 0.f;
        CHECK(Deal(4.0f, 100, carry) == 400);
    }

    // 7. A fully stacked hit rounds once: base 5 x relic 1.35 x Deadeye 1.5 x
    //    frozen combo 1.25 = 12.66 -> 12 now, carrying 0.66 to the next hit.
    //    (The old staged rounding produced ceil(ceil(5*1.35)*1.5)=11 -> lround(11*1.25)=14.)
    {
        float carry = 0.f;
        const float nominal = 5.f * 1.35f * 1.5f * 1.25f;
        CHECK(ResolveHitDamage(nominal, carry) == 12);
        CHECK(std::fabs(carry - (nominal - 12.f)) < 1e-3f);
        CHECK(ResolveHitDamage(nominal, carry) == 13);   // the carried 0.66 pays out
    }

    // 8. Defense on player hits: K / (K + Defense), capped, at least 1, bad input ignored.
    {
        using namespace DamagePolicy;
        using namespace Balance::Stats;
        CHECK(DefenseReduction(0.f) == 0.f && DefenseReduction(-5.f) == 0.f && DefenseReduction(NAN) == 0.f);
        CHECK(std::fabs(DefenseReduction(kDefenseK) - 0.5f) < 1e-4f);                         // Defense K halves a hit
        const float capDefense = kDefenseK * kMaxDefenseReduction / (1.f - kMaxDefenseReduction);   // 150
        CHECK(std::fabs(DefenseReduction(capDefense) - kMaxDefenseReduction) < 1e-4f);
        CHECK(DefenseReduction(1e6f) == kMaxDefenseReduction);                               // stacking never passes the cap
        CHECK(ApplyDefenseToHit(40, 0.f) == 40);
        CHECK(ApplyDefenseToHit(40, kDefenseK) == 20);
        CHECK(ApplyDefenseToHit(100, 1e6f) == 40);                                           // the cap leaves 40%
        CHECK(ApplyDefenseToHit(1, 1e6f) == kMinHitDamage);                                  // a landed hit always counts
        CHECK(ApplyDefenseToHit(0, kDefenseK) == 0 && ApplyDefenseToHit(-3, kDefenseK) == 0);
        CHECK(std::fabs(ApplyDefenseToFractionalHit(15.f, kDefenseK) - 7.5f) < 1e-4f);
        CHECK(ApplyDefenseToFractionalHit(0.f, kDefenseK) == 0.f && ApplyDefenseToFractionalHit(NAN, kDefenseK) == 0.f);
    }

    std::printf("Damage policy tests passed (%d checks): exact integers, +15%% breakpoint, "
                "minimum 1, invalid input, no float drift, fractional attack, single rounding, defense.\n", g_checks);
    return 0;
}
