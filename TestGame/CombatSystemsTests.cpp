#include "DamageNumberManager.h"
#include "EncounterPlanner.h"

#include <cassert>
#include <cstdio>

namespace
{
    void TestNormalHitsMergeButCriticalStaysSeparate()
    {
        DamageNumberManager manager;
        manager.Init();

        DamageNumberEvent first{};
        first.targetId = 7;
        first.attackId = 11;
        first.worldPos = { 100.f, 100.f };
        first.finalDamage = 4;
        manager.Submit(first);

        DamageNumberEvent second = first;
        second.worldPos = { 104.f, 100.f };
        second.finalDamage = 6;
        manager.Submit(second);

        assert(manager.GetStats().active == 1);
        assert(manager.DebugValueForTarget(7, DamageNumberOutcome::Normal) == 10);

        DamageNumberEvent critical = first;
        critical.finalDamage = 8;
        critical.outcome = DamageNumberOutcome::Critical;
        manager.Submit(critical);

        assert(manager.GetStats().active == 2);
        assert(manager.DebugOutcomeCount(DamageNumberOutcome::Critical) == 1);
    }

    void TestCapacityPrioritizesCriticalsAndNeverGrows()
    {
        DamageNumberManager manager;
        manager.Init();
        for (int i = 0; i < DamageNumberManager::kCapacity; ++i)
        {
            DamageNumberEvent event{};
            event.targetId = (std::uint64_t)(100 + i);
            event.attackId = (std::uint32_t)i;
            event.finalDamage = 1;
            manager.Submit(event);
        }
        assert(manager.GetStats().active == DamageNumberManager::kCapacity);

        DamageNumberEvent critical{};
        critical.targetId = 999;
        critical.attackId = 999;
        critical.finalDamage = 20;
        critical.outcome = DamageNumberOutcome::Critical;
        manager.Submit(critical);

        assert(manager.GetStats().active == DamageNumberManager::kCapacity);
        assert(manager.GetStats().replaced == 1);
        assert(manager.DebugOutcomeCount(DamageNumberOutcome::Critical) == 1);
    }

    void TestUpdateExpiresAndClearResetsTelemetry()
    {
        DamageNumberManager manager;
        manager.Init();
        DamageNumberEvent event{};
        event.targetId = 1;
        event.finalDamage = 3;
        manager.Submit(event);
        manager.Update(manager.GetSettings().lifetime + 0.01f);
        assert(manager.GetStats().active == 0);

        manager.Submit(event);
        manager.Clear();
        assert(manager.GetStats().active == 0);
        assert(manager.GetStats().submitted == 0);
        assert(manager.GetStats().merged == 0);
        assert(manager.GetStats().suppressed == 0);
        assert(manager.GetStats().replaced == 0);
    }

    void TestActualDamageUsesHealthRemoved()
    {
        assert(CalculateActualDamage(10.f, 7.f) == 3);
        assert(CalculateActualDamage(10.f, 10.f) == 0);
        assert(CalculateActualDamage(3.f, 0.f) == 3);
    }

    void TestEncounterPlansRespectPopulationAndSafetyCaps()
    {
        for (int tier = 0; tier < 3; ++tier)
        {
            for (std::uint32_t seed = 1; seed <= 1000; ++seed)
            {
                EncounterRequest request{};
                request.tier = tier;
                request.seed = seed;
                EncounterPlan plan = EncounterPlanner::Build(request);
                assert(plan.debug.plannedPopulation >= Balance::Pressure::kPopulationMin[tier]);
                assert(plan.debug.plannedPopulation <= Balance::Pressure::kPopulationMax[tier]);
                assert(plan.debug.openingPopulation <= Balance::Pressure::kOpeningBodyCap[tier]);
                assert(plan.debug.openingPressure <= Balance::Pressure::kDangerCap[tier]);
                assert(plan.debug.specialistCounts[(int)SpecialistClass::Ranged] <= Balance::Pressure::kRangedCap[tier]);
                assert(plan.debug.specialistCounts[(int)SpecialistClass::Tank] <= Balance::Pressure::kTankCap[tier]);
                assert(plan.debug.specialistCounts[(int)SpecialistClass::Support] <= Balance::Pressure::kSupportCap[tier]);
                assert(plan.debug.expensiveUnits <= Balance::Pressure::kExpensiveUnitCap[tier]);
                assert((int)(plan.opening.size() + plan.reinforcements.size()) == plan.debug.plannedPopulation);
            }
        }
    }

    void TestSwarmPlansReachPeakWithCheapFiller()
    {
        for (std::uint32_t seed = 1; seed <= 250; ++seed)
        {
            EncounterRequest request{};
            request.tier = 2;
            request.seed = seed;
            request.swarm = true;
            EncounterPlan plan = EncounterPlanner::Build(request);
            assert(plan.debug.plannedPopulation >= Balance::Pressure::kSwarmPeakMin);
            assert(plan.debug.plannedPopulation <= Balance::Pressure::kSwarmPeakMax);
            for (const EncounterSpawnEntry& entry : plan.reinforcements)
                if (entry.swarmProfile)
                    assert(entry.pressureCost == 1);
        }
    }
}

#ifdef COMBAT_SYSTEMS_TEST_MAIN
int main()
{
    TestNormalHitsMergeButCriticalStaysSeparate();
    TestCapacityPrioritizesCriticalsAndNeverGrows();
    TestUpdateExpiresAndClearResetsTelemetry();
    TestActualDamageUsesHealthRemoved();
    TestEncounterPlansRespectPopulationAndSafetyCaps();
    TestSwarmPlansReachPeakWithCheapFiller();
    std::puts("Combat systems tests passed");
    return 0;
}
#endif
