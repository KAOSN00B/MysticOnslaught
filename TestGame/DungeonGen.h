#pragma once
#include "GameTypes.h"
#include "raylib.h"
#include <vector>

// One room in the dungeon grid.
struct DungeonRoom
{
    int      col      = 0;
    int      row      = 0;
    RoomType type     = RoomType::Standard;
    EncounterProfile encounterProfile = EncounterProfile::Skirmish;
    int      depth    = 0; // shortest graph distance from the dungeon entrance
    bool     hasNorth = false;
    bool     hasSouth = false;
    bool     hasEast  = false;
    bool     hasWest  = false;
    bool     startsEmpty = false; // generated room begins cleared, with no encounter
};

// Generates a branching dungeon layout on a small grid.
// The result is a list of rooms with their grid positions and door directions.
// No tile data yet — this is purely the room graph used by the dungeon graph.
class DungeonGen
{
public:
    static constexpr int kGridSize    = 8;   // dungeon fits inside an 8x8 cell grid
    static constexpr int kTargetRooms = 12;  // default size (dev tools, editor playtests)
    static constexpr int kMinRooms    = 8;
    static constexpr int kMaxRooms    = 16;  // a quarter of the grid: big, never sprawling

    // How big a dungeon is and how much it holds. A run's domains grow as it
    // goes deeper (PlanForDomain). Every room always sits inside the 8x8 grid
    // that the corner minimap draws in full, so the map can't outgrow it.
    struct Plan
    {
        int rooms     = kTargetRooms;
        int elites    = 1;
        int treasures = 1;
    };
    static Plan PlanForDomain(int domainIndex);   // 0 = first domain ... 5 = final domain
    void SetPlan(const Plan& plan) { _plan = plan; }   // used by every later Generate()
    const Plan& GetPlan() const { return _plan; }

    void Generate();
    void GeneratePrologue();
    // The crossroads between domains: one room at the grid's centre with a
    // "gateway" room behind each requested door (west / north / east). There
    // is no boss or key; the engine turns walking into a gateway into a choice.
    void GenerateCrossroads(bool west, bool north, bool east);
    // Same full-size abstract graph as Generate(), but every grid adjacency is
    // reciprocal so a room-library playtest can traverse the whole layout.
    void GenerateEditorPlaytest();
    const std::vector<DungeonRoom>& GetRooms() const { return _rooms; }
    int GetStartIndex() const { return _startIdx; }
    bool IsEntranceRoom(int roomIdx) const { return roomIdx >= 0 && roomIdx == _startIdx; }
    int GetBossIndex()  const { return _bossIdx;  }
    int GetKeyIndex()   const { return _keyIdx;   }

    // Overrides one room's type after Generate() has run. Used to seat the fixed
    // opening Shop as room 0 of a new game's first dungeon.
    void ForceRoomType(int roomIdx, RoomType type);

    // Mid-run resume: reinstate a saved graph. Rejects (and leaves the current
    // graph untouched) data that is out of the grid, overlapping, or whose
    // special-room indices don't exist.
    bool Restore(const std::vector<DungeonRoom>& rooms, int startIdx, int bossIdx, int keyIdx);

    // Returns the room index adjacent to roomIdx in direction (dr, dc), or -1 if none.
    int GetNeighborIndex(int roomIdx, int dr, int dc) const;

private:
    void GrowRooms();
    void AssignSpecialRooms();
    void BuildConnections();
    int  FindFurthest(int fromIdx) const;
    int  DistanceBFS(int fromIdx, int toIdx) const;

    std::vector<DungeonRoom> _rooms;
    int _grid[kGridSize][kGridSize]{};   // room index at each cell, or -1 if empty

    int _startIdx = -1;
    int _bossIdx  = -1;
    int _keyIdx   = -1;
    Plan _plan;
};
