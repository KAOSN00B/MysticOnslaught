// Regression coverage for the campaign-depth policy (RunProgression.h) and the
// shop tier pricing it feeds (ShopPricing.h). Before this policy, the real run
// route never advanced Engine::_wave/_currentAct, so every domain fought power
// level 1 enemies and every shop charged first-tier prices.
//
// Build: tools\run_tests.ps1 -Suite RunProgression

#include "RunProgression.h"
#include "ShopPricing.h"
#include "ShopManager.h"   // ShopManager::NextRerollCost (inline, no raylib calls)

#include <cmath>
#include <cstdio>
#include <cstdlib>

static int g_checks = 0;
#define CHECK(x) do { ++g_checks; if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
static bool Near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

int main()
{
    using namespace RunProgression;

    // 1. First-room baseline: the opening Forest is untouched authored tuning.
    CHECK(EnemyPowerLevelForZone(0) == 1);
    const EnemyStatScale base = StatScaleForPowerLevel(EnemyPowerLevelForZone(0));
    CHECK(Near(base.health, 1.f) && Near(base.damage, 1.f) && Near(base.speed, 1.f) && Near(base.attackDelay, 1.f));
    CHECK(base.expBonus == 0);
    CHECK(BehaviourWaveForPowerLevel(1) == 1);
    CHECK((BehaviourWaveForPowerLevel(1) - 1) / Balance::Curve::kRoomsPerPowerLevel == 0); // legacy behaviour tier 0

    // 2. Intended mid-run scaling (third domain = power level 3).
    const EnemyStatScale mid = StatScaleForPowerLevel(EnemyPowerLevelForZone(2));
    CHECK(EnemyPowerLevelForZone(2) == 3);
    CHECK(Near(mid.health, 1.24f));
    CHECK(Near(mid.speed, 1.08f));
    CHECK(Near(mid.attackDelay, 0.90f));
    CHECK((BehaviourWaveForPowerLevel(3) - 1) / Balance::Curve::kRoomsPerPowerLevel == 2); // archer/ogre tier 2

    // 3. Final-domain scaling (power level 6) and concrete HP outcomes.
    const EnemyStatScale fin = StatScaleForPowerLevel(EnemyPowerLevelForZone(kFinalZone));
    CHECK(EnemyPowerLevelForZone(kFinalZone) == 6);
    CHECK(Near(fin.health, 1.60f));
    CHECK(Near(fin.attackDelay, 0.75f));
    CHECK(fin.attackDelay >= Balance::Curve::kMinAttackDelayFactor);
    CHECK(Near(Balance::Grunt::kBaseHealth * fin.health, 96.f));                    // grunt 60 -> 96
    CHECK(std::ceil(Balance::Boss::kMolarbeastHealth * fin.health) == 1141.f);       // final boss ~713 -> 1141
    CHECK(std::ceil(Balance::Boss::kAbyssSlimeHealth * base.health) == 460.f);       // Caverns boss: mid budget
    CHECK(Balance::Boss::kMolarbeastHealth > Balance::Boss::kMinotaurHealth);         // nothing outlasts the final boss

    // 4. Monotonic, bounded curve: deeper is never easier, cadence never below the floor.
    for (int z = 1; z <= kFinalZone; ++z)
    {
        const EnemyStatScale a = StatScaleForPowerLevel(EnemyPowerLevelForZone(z - 1));
        const EnemyStatScale b = StatScaleForPowerLevel(EnemyPowerLevelForZone(z));
        CHECK(b.health > a.health);
        CHECK(b.speed > a.speed);
        CHECK(b.attackDelay <= a.attackDelay);
        CHECK(b.attackDelay >= Balance::Curve::kMinAttackDelayFactor);
        CHECK(ShopTierForZone(z) == ShopTierForZone(z - 1) + 1);
    }

    // 5. Depth is a pure function of the committed domain: re-entering rooms,
    //    menus or village pauses cannot move it, and out-of-range input clamps.
    for (int repeat = 0; repeat < 3; ++repeat)
        CHECK(EnemyPowerLevelForZone(3) == 4);
    CHECK(EnemyPowerLevelForZone(-7) == 1);    // a reset run (zone 0) or bad state starts at the baseline
    CHECK(EnemyPowerLevelForZone(99) == 6);    // nothing exceeds the final domain
    CHECK(ShopTierForZone(-1) == 1 && ShopTierForZone(42) == 6);

    // 6. Restored enemy state: a survivor keeps its share of HP across a re-entry.
    CHECK(Near(RestoredSurvivorHealth(10.f, 0.35f), 4.f));    // 3.5 rounds up, never down to a free kill
    CHECK(Near(RestoredSurvivorHealth(10.f, 1.0f), 10.f));
    CHECK(Near(RestoredSurvivorHealth(10.f, 0.01f), 1.f));    // never below 1 HP
    CHECK(Near(RestoredSurvivorHealth(10.f, 0.f), 10.f));     // missing data = authored full HP
    CHECK(Near(RestoredSurvivorHealth(10.f, NAN), 10.f));
    {
        // Same domain -> same scaled max HP, so the restored value equals what was left.
        const float maxHp = std::ceil(Balance::Grunt::kBaseHealth * mid.health);   // 75
        const float leftAtExit = 30.f;
        CHECK(Near(RestoredSurvivorHealth(maxHp, leftAtExit / maxHp), leftAtExit));
    }

    // 7. Shop progression: tier 1 is the authored price list; prices rise per domain.
    CHECK(ShopPricing::Scaled(95, ShopPricing::AbilityInflation(ShopTierForZone(0))) == 95);
    CHECK(ShopPricing::Scaled(55, ShopPricing::WareInflation(ShopTierForZone(0))) == 55);
    CHECK(ShopPricing::AbilityUpgradePrice(1, 1) == 110);
    CHECK(ShopPricing::AbilityUpgradePrice(2, 1) == 180);
    CHECK(ShopPricing::AbilityUpgradePrice(1, 3) == 165);                  // +50% at tier 3
    CHECK(Near(ShopPricing::AbilityInflation(ShopTierForZone(kFinalZone)), 2.25f));
    CHECK(Near(ShopPricing::WareInflation(ShopTierForZone(kFinalZone)), 1.75f));
    CHECK(ShopPricing::Scaled(95, ShopPricing::AbilityInflation(6)) == 214);
    CHECK(ShopPricing::AbilityInflation(0) == ShopPricing::AbilityInflation(1));   // bad tier clamps

    // 8. Rerolls: Arcane Attunement's free reroll is followed by the normal ladder.
    CHECK(ShopManager::NextRerollCost(0) == Balance::Economy::kRerollBaseCost);
    CHECK(ShopManager::NextRerollCost(Balance::Economy::kRerollBaseCost) ==
          Balance::Economy::kRerollBaseCost + Balance::Economy::kRerollStep);

    std::printf("Run progression tests passed (%d checks): baseline, mid/final scaling, monotonic curve, "
                "stable depth, survivor restore, shop tiers, reroll ladder.\n", g_checks);
    return 0;
}
