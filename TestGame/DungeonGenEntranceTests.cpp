#include "DungeonGen.h"
#include "GameBalance.h"

#include <cassert>
#include <algorithm>
#include <cstdio>
#include <queue>
#include <random>
#include <vector>

namespace
{
    std::vector<int> gRandomScript;
    std::size_t gRandomScriptIndex = 0;
    std::mt19937 gRandomEngine{ 0x12345678u };

    void SetRandomScript(std::initializer_list<int> values)
    {
        gRandomScript.assign(values);
        gRandomScriptIndex = 0;
        gRandomEngine.seed(0x12345678u);
    }

    void AssertAllRoomsReachable(const DungeonGen& generator)
    {
        const auto& rooms = generator.GetRooms();
        std::vector<bool> visited(rooms.size(), false);
        std::queue<int> frontier;
        const int start = generator.GetStartIndex();
        visited[(std::size_t)start] = true;
        frontier.push(start);

        while (!frontier.empty())
        {
            const int current = frontier.front();
            frontier.pop();
            const DungeonRoom& room = rooms[(std::size_t)current];

            const int neighbors[4] = {
                room.hasNorth ? generator.GetNeighborIndex(current, -1, 0) : -1,
                room.hasSouth ? generator.GetNeighborIndex(current, 1, 0) : -1,
                room.hasEast  ? generator.GetNeighborIndex(current, 0, 1) : -1,
                room.hasWest  ? generator.GetNeighborIndex(current, 0, -1) : -1
            };
            for (int neighbor : neighbors)
            {
                if (neighbor < 0 || visited[(std::size_t)neighbor]) continue;
                visited[(std::size_t)neighbor] = true;
                frontier.push(neighbor);
            }
        }

        assert(std::all_of(visited.begin(), visited.end(), [](bool value) { return value; }));
    }
}

extern "C" int GetRandomValue(int min, int max)
{
    if (gRandomScriptIndex < gRandomScript.size())
        return std::clamp(gRandomScript[gRandomScriptIndex++], min, max);
    return std::uniform_int_distribution<int>(min, max)(gRandomEngine);
}

int main()
{
    for (int edge = 0; edge < 4; ++edge)
    {
        // Entrance edge, edge-axis position, then deterministic minimums for
        // room growth. Each generated dungeon must remain full and connected.
        SetRandomScript({ edge, 3 });
        DungeonGen generator;
        generator.Generate();
        const int start = generator.GetStartIndex();
        const DungeonRoom& room = generator.GetRooms()[(std::size_t)start];

        assert(generator.IsEntranceRoom(start));
        assert(room.type == RoomType::Standard);
        if ((int)generator.GetRooms().size() != DungeonGen::kTargetRooms)
            std::fprintf(stderr, "edge %d generated %zu rooms\n", edge, generator.GetRooms().size());
        assert((int)generator.GetRooms().size() == DungeonGen::kTargetRooms);
        assert(generator.GetBossIndex() != start);
        assert(generator.GetRooms()[(std::size_t)generator.GetBossIndex()].type == RoomType::Boss);

        switch (edge)
        {
        case 0: // bottom edge, dungeon continues north
            assert(room.row == DungeonGen::kGridSize - 1);
            assert(room.hasNorth && !room.hasSouth && !room.hasEast && !room.hasWest);
            break;
        case 1: // left edge, dungeon continues east
            assert(room.col == 0);
            assert(!room.hasNorth && !room.hasSouth && room.hasEast && !room.hasWest);
            break;
        case 2: // right edge, dungeon continues west
            assert(room.col == DungeonGen::kGridSize - 1);
            assert(!room.hasNorth && !room.hasSouth && !room.hasEast && room.hasWest);
            break;
        case 3: // top edge, dungeon continues south
            assert(room.row == 0);
            assert(!room.hasNorth && room.hasSouth && !room.hasEast && !room.hasWest);
            break;
        }

        AssertAllRoomsReachable(generator);
    }

    // The empty-entrance roll is generated once with the room graph, so room
    // re-entry cannot change whether the opening room contains an encounter.
    SetRandomScript({ 0, 3, 0 });
    DungeonGen emptyEntrance;
    emptyEntrance.Generate();
    assert(emptyEntrance.GetRooms()[(std::size_t)emptyEntrance.GetStartIndex()].startsEmpty);

    SetRandomScript({ 0, 3, 1 });
    DungeonGen combatEntrance;
    combatEntrance.Generate();
    assert(!combatEntrance.GetRooms()[(std::size_t)combatEntrance.GetStartIndex()].startsEmpty);

    // Domain plans: dungeons grow from the first domain to the final one, hold
    // more elites and treasure as they grow, never pass the size cap, and every
    // room stays inside the grid the corner minimap draws.
    SetRandomScript({});
    {
        int previousRooms = 0;
        for (int domain = 0; domain <= 5; ++domain)
        {
            const DungeonGen::Plan plan = DungeonGen::PlanForDomain(domain);
            assert(plan.rooms >= previousRooms);
            assert(plan.rooms >= DungeonGen::kMinRooms && plan.rooms <= DungeonGen::kMaxRooms);
            // Every domain must fit the 15-minute pacing budget, priced per room kind.
            assert(Balance::Pacing::PlannedDomainSeconds(plan.rooms, plan.elites) <= Balance::Pacing::kDomainMaxSeconds);
            assert(DungeonGen::kMaxRooms * 4 <= DungeonGen::kGridSize * DungeonGen::kGridSize);
            previousRooms = plan.rooms;
            for (int iteration = 0; iteration < 120; ++iteration)
            {
                DungeonGen sized;
                sized.SetPlan(plan);
                sized.Generate();
                const auto& rooms = sized.GetRooms();
                assert((int)rooms.size() == plan.rooms);
                int elites = 0, treasures = 0, bosses = 0;
                for (const DungeonRoom& r : rooms)
                {
                    assert(r.row >= 0 && r.row < DungeonGen::kGridSize && r.col >= 0 && r.col < DungeonGen::kGridSize);
                    elites += r.type == RoomType::Elite;
                    treasures += r.type == RoomType::Treasure;
                    bosses += r.type == RoomType::Boss;
                    if (r.type == RoomType::Elite || r.type == RoomType::Treasure)
                        assert(r.depth >= 2);
                }
                assert(bosses == 1 && elites == plan.elites && treasures == plan.treasures);
                assert(sized.GetKeyIndex() >= 0 && sized.GetKeyIndex() != sized.GetBossIndex());
                AssertAllRoomsReachable(sized);
            }
        }
        assert(DungeonGen::PlanForDomain(0).rooms < DungeonGen::PlanForDomain(5).rooms);
        assert(DungeonGen::PlanForDomain(99).rooms == DungeonGen::PlanForDomain(5).rooms);   // clamps
        assert(DungeonGen::PlanForDomain(-3).rooms == DungeonGen::PlanForDomain(0).rooms);
    }

    // Exercise many natural rolls as a guard against disconnected branches.
    for (int iteration = 0; iteration < 200; ++iteration)
    {
        DungeonGen rolled;
        rolled.Generate();
        int bosses = 0;
        int elites = 0;
        int treasures = 0;
        int holdouts = 0;
        int swarms = 0;
        for (int index = 0; index < (int)rolled.GetRooms().size(); ++index)
        {
            const DungeonRoom& candidate = rolled.GetRooms()[(std::size_t)index];
            bosses += candidate.type == RoomType::Boss;
            elites += candidate.type == RoomType::Elite;
            treasures += candidate.type == RoomType::Treasure;
            holdouts += candidate.encounterProfile == EncounterProfile::Holdout;
            swarms += candidate.encounterProfile == EncounterProfile::Swarm;
            if (candidate.type == RoomType::Elite || candidate.type == RoomType::Treasure)
                assert(candidate.depth >= 2);
        }
        const DungeonRoom& entrance = rolled.GetRooms()[(std::size_t)rolled.GetStartIndex()];
        const bool onOuterEdge = entrance.row == 0 || entrance.row == DungeonGen::kGridSize - 1 ||
                                 entrance.col == 0 || entrance.col == DungeonGen::kGridSize - 1;
        assert(onOuterEdge);
        assert((int)rolled.GetRooms().size() == DungeonGen::kTargetRooms);
        assert(entrance.encounterProfile == EncounterProfile::Skirmish);
        assert(bosses == 1 && elites == 1 && treasures == 1);
        assert(holdouts == 1);
        assert(swarms == 1);
        assert(rolled.GetRooms()[(std::size_t)rolled.GetBossIndex()].encounterProfile !=
               EncounterProfile::Holdout);
        AssertAllRoomsReachable(rolled);
    }

    SetRandomScript({});
    DungeonGen generator;
    generator.GeneratePrologue();
    const auto& prologue = generator.GetRooms();
    assert(prologue.size() == 3);
    assert(prologue[0].hasEast && !prologue[0].hasWest);
    assert(prologue[1].hasEast && prologue[1].hasWest);
    assert(!prologue[2].hasEast && prologue[2].hasWest);
    assert(prologue[0].row == prologue[1].row);
    assert(prologue[1].row == prologue[2].row);
    assert(prologue[0].col + 1 == prologue[1].col);
    assert(prologue[1].col + 1 == prologue[2].col);

    generator.GenerateEditorPlaytest();
    const auto& editorRooms = generator.GetRooms();
    for (int i = 0; i < (int)editorRooms.size(); ++i)
    {
        const DungeonRoom& authoredSlot = editorRooms[(std::size_t)i];
        if (authoredSlot.hasNorth)
        {
            const int neighbor = generator.GetNeighborIndex(i, -1, 0);
            assert(neighbor >= 0 && editorRooms[(std::size_t)neighbor].hasSouth);
        }
        if (authoredSlot.hasSouth)
        {
            const int neighbor = generator.GetNeighborIndex(i, 1, 0);
            assert(neighbor >= 0 && editorRooms[(std::size_t)neighbor].hasNorth);
        }
        if (authoredSlot.hasEast)
        {
            const int neighbor = generator.GetNeighborIndex(i, 0, 1);
            assert(neighbor >= 0 && editorRooms[(std::size_t)neighbor].hasWest);
        }
        if (authoredSlot.hasWest)
        {
            const int neighbor = generator.GetNeighborIndex(i, 0, -1);
            assert(neighbor >= 0 && editorRooms[(std::size_t)neighbor].hasEast);
        }
    }

    return 0;
}
