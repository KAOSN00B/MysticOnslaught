#pragma once

#include "GameTypes.h"

#include <vector>

enum class BossExitChoice
{
    ReturnToVillage,
    Continue,
};

struct BossExitPolicy
{
    bool restoreHealth = false;
    bool restoreMana = false;
    bool pauseRun = false;
};

BossExitPolicy GetBossExitPolicy(BossExitChoice choice);

class RunSession
{
public:
    void Begin();
    void Reset();
    void PauseInVillage();
    void Resume();
    void RecordBiomeClear(Biome biome);
    void RecordMapChoice(Biome biome, int tierIndex);
    void MarkWorldMapGenerated();

    bool IsActive() const { return _active; }
    bool IsPausedInVillage() const { return _pausedInVillage; }
    bool HasGeneratedWorldMap() const { return _worldMapGenerated; }
    bool NeedsFirstForest() const { return _completedBiomes.empty(); }
    int GetWorldZone() const { return (int)_chosenNodeIndices.size(); }
    const std::vector<Biome>& GetCompletedBiomes() const { return _completedBiomes; }
    const std::vector<int>& GetChosenNodeIndices() const { return _chosenNodeIndices; }

private:
    bool _active = false;
    bool _pausedInVillage = false;
    bool _worldMapGenerated = false;
    std::vector<Biome> _completedBiomes;
    std::vector<Biome> _chosenBiomes;
    std::vector<int> _chosenNodeIndices;
};
