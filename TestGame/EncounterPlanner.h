#pragma once

#include "Enemy.h"
#include "RoomCapacity.h"

#include <cstdint>
#include <deque>
#include <vector>

enum class EnemySpawnKind : unsigned char
{
    Shadow, Archer, Slime, FlameWisp, Sporeling,
    Shieldbearer, Phantom, Bomber, Warchief, LivingBlade
};

enum class SpecialistClass : unsigned char
{
    None, Ranged, Tank, Support, Assassin, Zoner, Count
};

struct EncounterSpawnEntry
{
    EnemySpawnKind kind = EnemySpawnKind::Shadow;
    EnemyRole role = EnemyRole::Grunt;
    SpecialistClass specialist = SpecialistClass::None;
    int populationCost = 1;
    int pressureCost = 1;
    bool swarmProfile = false;
};

struct EncounterRequest
{
    int tier = 0;
    std::uint32_t seed = 1;
    int hazardPressure = 0;
    int populationBonus = 0;
    bool swarm = false;
    EncounterProfile profile = EncounterProfile::Skirmish;
    RoomCombatCapacity capacity{};
    int learnedAbilityCount = 0;
};

struct EncounterPlanDebug
{
    int targetPopulation = 0;
    int plannedPopulation = 0;
    int openingPopulation = 0;
    int openingPressure = 0;
    int totalPressure = 0;
    int expensiveUnits = 0;
    int openingBodyCap = 0;
    int totalBodyCap = 0;
    int pressureCap = 0;
    int specialistCounts[(int)SpecialistClass::Count]{};
};

struct EncounterPlan
{
    std::vector<EncounterSpawnEntry> opening;
    std::deque<EncounterSpawnEntry> reinforcements;
    EncounterPlanDebug debug{};
    bool swarm = false;
};

class EncounterPlanner
{
public:
    static EncounterPlan Build(const EncounterRequest& request);
};

inline int EncounterTypeId(EnemySpawnKind kind)
{
    return static_cast<int>(kind);
}
