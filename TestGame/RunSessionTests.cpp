#include "RunSession.h"

#include <cassert>

int main()
{
    RunSession run;
    run.Begin();

    assert(run.IsActive());
    assert(!run.IsPausedInVillage());
    assert(run.NeedsFirstForest());
    assert(!run.HasGeneratedWorldMap());
    assert(run.GetWorldZone() == 0);

    run.RecordBiomeClear(Biome::Forest);
    run.RecordMapChoice(Biome::Caverns, 2);
    run.MarkWorldMapGenerated();
    run.PauseInVillage();

    assert(run.IsPausedInVillage());
    assert(run.HasGeneratedWorldMap());
    assert(!run.NeedsFirstForest());
    assert(run.GetWorldZone() == 1);
    assert(run.GetCompletedBiomes().size() == 1);
    assert(run.GetCompletedBiomes().front() == Biome::Forest);
    assert(run.GetChosenNodeIndices().size() == 1);
    assert(run.GetChosenNodeIndices().front() == 2);

    run.Resume();
    assert(run.IsActive());
    assert(!run.IsPausedInVillage());
    assert(run.GetCompletedBiomes().front() == Biome::Forest);
    assert(run.GetChosenNodeIndices().front() == 2);

    const BossExitPolicy village = GetBossExitPolicy(BossExitChoice::ReturnToVillage);
    assert(village.restoreHealth);
    assert(village.restoreMana);
    assert(village.pauseRun);

    const BossExitPolicy onward = GetBossExitPolicy(BossExitChoice::Continue);
    assert(!onward.restoreHealth);
    assert(!onward.restoreMana);
    assert(!onward.pauseRun);

    run.Reset();
    assert(!run.IsActive());
    assert(run.GetCompletedBiomes().empty());
    assert(run.GetChosenNodeIndices().empty());
    return 0;
}
