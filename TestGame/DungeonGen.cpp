#include "DungeonGen.h"
#include <algorithm>
#include <queue>
#include <limits>

void DungeonGen::Generate()
{
    _rooms.clear();
    _startIdx = _bossIdx = _keyIdx = -1;

    for (int r = 0; r < kGridSize; r++)
        for (int c = 0; c < kGridSize; c++)
            _grid[r][c] = -1;

    GrowRooms();
    BuildConnections();
    AssignSpecialRooms();
}

void DungeonGen::GrowRooms()
{
    // The run always begins in Zeph's shop on the bottom edge. The first real
    // dungeon path is directly north, so "up" is always forward from spawn.
    int startRow = kGridSize - 1;
    int startCol = kGridSize / 2;

    DungeonRoom start;
    start.col  = startCol;
    start.row  = startRow;
    start.type = RoomType::Store;
    _grid[startRow][startCol] = 0;
    _rooms.push_back(start);
    _startIdx = 0;

    DungeonRoom firstRoom;
    firstRoom.col  = startCol;
    firstRoom.row  = startRow - 1;
    firstRoom.type = RoomType::Standard;
    _grid[firstRoom.row][firstRoom.col] = 1;
    _rooms.push_back(firstRoom);

    // Cardinal direction offsets: N, S, E, W
    static const int dc[] = {  0,  0, 1, -1 };
    static const int dr[] = { -1,  1, 0,  0 };

    // Grow outward by randomly picking an existing room and trying to add a neighbour.
    int attempts = 0;
    while ((int)_rooms.size() < kTargetRooms && attempts < 2000)
    {
        attempts++;

        int srcIdx = GetRandomValue(0, (int)_rooms.size() - 1);
        int dir    = GetRandomValue(0, 3);
        int nc = _rooms[srcIdx].col + dc[dir];
        int nr = _rooms[srcIdx].row + dr[dir];

        if (nc < 0 || nc >= kGridSize || nr < 0 || nr >= kGridSize)
            continue;
        if (_grid[nr][nc] != -1)
            continue;

        DungeonRoom room;
        room.col  = nc;
        room.row  = nr;
        room.type = RoomType::Standard;
        _grid[nr][nc] = (int)_rooms.size();
        _rooms.push_back(room);
    }
}

void DungeonGen::BuildConnections()
{
    for (auto& room : _rooms)
    {
        room.hasNorth = (room.row > 0              && _grid[room.row - 1][room.col] != -1);
        room.hasSouth = (room.row < kGridSize - 1  && _grid[room.row + 1][room.col] != -1);
        room.hasEast  = (room.col < kGridSize - 1  && _grid[room.row][room.col + 1] != -1);
        room.hasWest  = (room.col > 0              && _grid[room.row][room.col - 1] != -1);
    }
}

void DungeonGen::AssignSpecialRooms()
{
    if (_rooms.empty())
        return;

    // Boss = furthest room from start.
    _bossIdx = FindFurthest(_startIdx);
    if (_bossIdx >= 0)
        _rooms[_bossIdx].type = RoomType::Boss;

    // Key = second-furthest non-boss room.
    int bestDist = -1;
    _keyIdx = -1;
    for (int i = 0; i < (int)_rooms.size(); i++)
    {
        if (i == _startIdx || i == _bossIdx)
            continue;
        int d = DistanceBFS(_startIdx, i);
        if (d > bestDist) { bestDist = d; _keyIdx = i; }
    }

    // Collect all remaining Standard rooms and shuffle them.
    std::vector<int> pool;
    for (int i = 0; i < (int)_rooms.size(); i++)
    {
        if (i == _startIdx || i == _bossIdx || i == _keyIdx)
            continue;
        pool.push_back(i);
    }
    for (int i = (int)pool.size() - 1; i > 0; i--)
        std::swap(pool[i], pool[GetRandomValue(0, i)]);

    // Assign exactly one of each special type from the shuffled pool.
    // No Shop rooms — the shop is a post-boss reward handled separately.
    int idx = 0;
    if (idx < (int)pool.size()) { _rooms[pool[idx++]].type = RoomType::Elite;    }
    if (idx < (int)pool.size()) { _rooms[pool[idx++]].type = RoomType::Rest;     }
    if (idx < (int)pool.size()) { _rooms[pool[idx++]].type = RoomType::Treasure; }
    // All remaining rooms stay Standard.

    // Store room only ever exits north — random growth can attach rooms east/west/south
    // of the start cell, so strip those connections after the fact.
    _rooms[_startIdx].hasSouth = false;
    _rooms[_startIdx].hasEast  = false;
    _rooms[_startIdx].hasWest  = false;

    // The room directly north of the Store has no south door — once the player
    // exits the Store they can never walk back in from above.
    int northOfStart = _grid[_rooms[_startIdx].row - 1][_rooms[_startIdx].col];
    if (northOfStart >= 0)
        _rooms[northOfStart].hasSouth = false;
}

int DungeonGen::FindFurthest(int fromIdx) const
{
    int bestIdx  = fromIdx;
    int bestDist = -1;

    for (int i = 0; i < (int)_rooms.size(); i++)
    {
        if (i == fromIdx)
            continue;
        int d = DistanceBFS(fromIdx, i);
        if (d > bestDist)
        {
            bestDist = d;
            bestIdx  = i;
        }
    }
    return bestIdx;
}

int DungeonGen::GetNeighborIndex(int roomIdx, int dr, int dc) const
{
    if (roomIdx < 0 || roomIdx >= (int)_rooms.size())
        return -1;
    int nr = _rooms[roomIdx].row + dr;
    int nc = _rooms[roomIdx].col + dc;
    if (nr < 0 || nr >= kGridSize || nc < 0 || nc >= kGridSize)
        return -1;
    return _grid[nr][nc];
}

int DungeonGen::DistanceBFS(int fromIdx, int toIdx) const
{
    if (fromIdx == toIdx)
        return 0;
    if (_rooms.empty())
        return -1;

    std::vector<int> dist((int)_rooms.size(), -1);
    std::queue<int>  frontier;
    dist[fromIdx] = 0;
    frontier.push(fromIdx);

    static const int dc[] = {  0,  0, 1, -1 };
    static const int dr[] = { -1,  1, 0,  0 };

    while (!frontier.empty())
    {
        int cur = frontier.front();
        frontier.pop();

        const DungeonRoom& room = _rooms[cur];
        for (int d = 0; d < 4; d++)
        {
            int nc = room.col + dc[d];
            int nr = room.row + dr[d];
            if (nc < 0 || nc >= kGridSize || nr < 0 || nr >= kGridSize)
                continue;
            int nIdx = _grid[nr][nc];
            if (nIdx < 0 || dist[nIdx] >= 0)
                continue;
            dist[nIdx] = dist[cur] + 1;
            if (nIdx == toIdx)
                return dist[nIdx];
            frontier.push(nIdx);
        }
    }

    return dist[toIdx];
}
