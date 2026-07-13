#include "RunSession.h"

#include <algorithm>

BossExitPolicy GetBossExitPolicy(BossExitChoice choice)
{
    if (choice == BossExitChoice::ReturnToVillage)
        return { true, true, true };
    return {};
}

void RunSession::Begin()
{
    Reset();
    _active = true;
}

void RunSession::Reset()
{
    _active = false;
    _pausedInVillage = false;
    _worldMapGenerated = false;
    _completedBiomes.clear();
    _chosenBiomes.clear();
    _chosenNodeIndices.clear();
}

void RunSession::PauseInVillage()
{
    if (_active)
        _pausedInVillage = true;
}

void RunSession::Resume()
{
    if (_active)
        _pausedInVillage = false;
}

void RunSession::RecordBiomeClear(Biome biome)
{
    if (std::find(_completedBiomes.begin(), _completedBiomes.end(), biome) == _completedBiomes.end())
        _completedBiomes.push_back(biome);
}

void RunSession::RecordMapChoice(Biome biome, int tierIndex)
{
    _chosenBiomes.push_back(biome);
    _chosenNodeIndices.push_back(std::clamp(tierIndex, 0, 2));
}

void RunSession::MarkWorldMapGenerated()
{
    _worldMapGenerated = true;
}
