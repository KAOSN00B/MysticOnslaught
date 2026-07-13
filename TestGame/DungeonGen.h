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
    bool     hasNorth = false;
    bool     hasSouth = false;
    bool     hasEast  = false;
    bool     hasWest  = false;
};

// Generates a branching dungeon layout on a small grid.
// The result is a list of rooms with their grid positions and door directions.
// No tile data yet — this is purely the room graph used by the dungeon graph.
class DungeonGen
{
public:
    static constexpr int kGridSize    = 8;   // dungeon fits inside an 8x8 cell grid
    static constexpr int kTargetRooms = 12;  // how many rooms to aim for per dungeon

    void Generate();
    void GeneratePrologue();

    const std::vector<DungeonRoom>& GetRooms() const { return _rooms; }
    int GetStartIndex() const { return _startIdx; }
    bool IsEntranceRoom(int roomIdx) const { return roomIdx >= 0 && roomIdx == _startIdx; }
    int GetBossIndex()  const { return _bossIdx;  }
    int GetKeyIndex()   const { return _keyIdx;   }

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
};
