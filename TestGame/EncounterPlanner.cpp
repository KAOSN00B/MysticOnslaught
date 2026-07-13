#include "EncounterPlanner.h"

#include "GameBalance.h"

#include <algorithm>
#include <array>

namespace
{
    struct Candidate
    {
        EncounterSpawnEntry entry;
        int weights[3];
    };

    constexpr Candidate kCandidates[] = {
        { { EnemySpawnKind::Shadow,       EnemyRole::Grunt,    SpecialistClass::None,     1, 1, false }, { 40, 26, 18 } },
        { { EnemySpawnKind::Archer,       EnemyRole::Ranged,   SpecialistClass::Ranged,   1, 2, false }, {  9, 11, 11 } },
        { { EnemySpawnKind::Slime,        EnemyRole::Charger,  SpecialistClass::None,     1, 1, false }, {  9, 10, 10 } },
        { { EnemySpawnKind::FlameWisp,    EnemyRole::Zoner,    SpecialistClass::Zoner,    1, 2, false }, {  6, 10, 10 } },
        { { EnemySpawnKind::Sporeling,    EnemyRole::Grunt,    SpecialistClass::None,     1, 1, false }, {  8,  9,  9 } },
        { { EnemySpawnKind::Shieldbearer, EnemyRole::Tank,     SpecialistClass::Tank,     1, 2, false }, {  7,  9,  9 } },
        { { EnemySpawnKind::Phantom,      EnemyRole::Assassin, SpecialistClass::Assassin, 1, 2, false }, {  5,  9,  9 } },
        { { EnemySpawnKind::Bomber,       EnemyRole::Zoner,    SpecialistClass::Zoner,    1, 2, false }, {  5,  8,  8 } },
        { { EnemySpawnKind::Warchief,     EnemyRole::Support,  SpecialistClass::Support,  1, 3, false }, {  0,  4,  4 } },
        { { EnemySpawnKind::LivingBlade,  EnemyRole::Assassin, SpecialistClass::Assassin, 1, 2, false }, {  5,  8,  8 } },
    };

    class DeterministicRng
    {
    public:
        explicit DeterministicRng(std::uint32_t seed) : _state(seed ? seed : 0x9e3779b9u) {}

        std::uint32_t Next()
        {
            _state ^= _state << 13;
            _state ^= _state >> 17;
            _state ^= _state << 5;
            return _state;
        }

        int Range(int minValue, int maxValue)
        {
            return minValue + static_cast<int>(Next() % static_cast<std::uint32_t>(maxValue - minValue + 1));
        }

    private:
        std::uint32_t _state;
    };

    int SpecialistCap(SpecialistClass specialist, int tier)
    {
        switch (specialist)
        {
        case SpecialistClass::Ranged:   return Balance::Pressure::kRangedCap[tier];
        case SpecialistClass::Tank:     return Balance::Pressure::kTankCap[tier];
        case SpecialistClass::Support:  return Balance::Pressure::kSupportCap[tier];
        case SpecialistClass::Assassin: return Balance::Pressure::kAssassinCap[tier];
        case SpecialistClass::Zoner:    return Balance::Pressure::kZonerCap[tier];
        default:                        return 1000;
        }
    }

    bool CanAdd(const EncounterSpawnEntry& entry, const EncounterPlanDebug& debug, int tier)
    {
        if (entry.specialist == SpecialistClass::None)
            return true;
        const int index = static_cast<int>(entry.specialist);
        return debug.specialistCounts[index] < SpecialistCap(entry.specialist, tier)
            && debug.expensiveUnits < Balance::Pressure::kExpensiveUnitCap[tier];
    }

    void CountEntry(const EncounterSpawnEntry& entry, EncounterPlanDebug& debug)
    {
        debug.plannedPopulation += entry.populationCost;
        debug.totalPressure += entry.pressureCost;
        if (entry.specialist != SpecialistClass::None)
        {
            ++debug.specialistCounts[static_cast<int>(entry.specialist)];
            ++debug.expensiveUnits;
        }
    }

    EncounterSpawnEntry CheapFiller(DeterministicRng& rng, bool swarmProfile)
    {
        constexpr EnemySpawnKind kinds[] = {
            EnemySpawnKind::Shadow, EnemySpawnKind::Slime, EnemySpawnKind::Sporeling
        };
        EncounterSpawnEntry entry = kCandidates[0].entry;
        entry.kind = kinds[rng.Range(0, 2)];
        entry.role = entry.kind == EnemySpawnKind::Slime ? EnemyRole::Charger : EnemyRole::Grunt;
        entry.swarmProfile = swarmProfile;
        return entry;
    }
}

EncounterPlan EncounterPlanner::Build(const EncounterRequest& request)
{
    EncounterPlan plan{};
    const int tier = std::clamp(request.tier, 0, 2);
    DeterministicRng rng(request.seed);
    plan.swarm = request.swarm;

    const int minPopulation = request.swarm ? Balance::Pressure::kSwarmPeakMin
                                            : Balance::Pressure::kPopulationMin[tier];
    const int maxPopulation = request.swarm ? Balance::Pressure::kSwarmPeakMax
                                            : Balance::Pressure::kPopulationMax[tier];
    plan.debug.targetPopulation = std::clamp(rng.Range(minPopulation, maxPopulation)
                                             + request.populationBonus,
                                             minPopulation, maxPopulation);

    std::vector<EncounterSpawnEntry> authored;
    authored.reserve(plan.debug.targetPopulation);
    const int ordinaryTarget = request.swarm
        ? std::min(Balance::Pressure::kPopulationMax[tier], plan.debug.targetPopulation)
        : plan.debug.targetPopulation;

    int totalWeight = 0;
    for (const Candidate& candidate : kCandidates)
        totalWeight += candidate.weights[tier];

    while (plan.debug.plannedPopulation < ordinaryTarget)
    {
        const Candidate* selected = nullptr;
        for (int attempt = 0; attempt < 16 && selected == nullptr; ++attempt)
        {
            int roll = rng.Range(1, totalWeight);
            for (const Candidate& candidate : kCandidates)
            {
                roll -= candidate.weights[tier];
                if (roll <= 0)
                {
                    if (candidate.weights[tier] > 0 && CanAdd(candidate.entry, plan.debug, tier))
                        selected = &candidate;
                    break;
                }
            }
        }

        EncounterSpawnEntry entry = selected ? selected->entry : CheapFiller(rng, false);
        authored.push_back(entry);
        CountEntry(entry, plan.debug);
    }

    while (plan.debug.plannedPopulation < plan.debug.targetPopulation)
    {
        EncounterSpawnEntry entry = CheapFiller(rng, true);
        authored.push_back(entry);
        CountEntry(entry, plan.debug);
    }

    const int openingBodyCap = Balance::Pressure::kOpeningBodyCap[tier];
    const int openingDangerCap = std::max(1, Balance::Pressure::kDangerCap[tier]
                                             - std::max(0, request.hazardPressure));
    for (const EncounterSpawnEntry& entry : authored)
    {
        if ((int)plan.opening.size() < openingBodyCap
            && plan.debug.openingPressure + entry.pressureCost <= openingDangerCap)
        {
            plan.opening.push_back(entry);
            plan.debug.openingPopulation += entry.populationCost;
            plan.debug.openingPressure += entry.pressureCost;
        }
        else
        {
            plan.reinforcements.push_back(entry);
        }
    }

    return plan;
}
