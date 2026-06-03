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
    // Place starting room in the centre of the grid.
    int startCol = kGridSize / 2;
    int startRow = kGridSize / 2;

    DungeonRoom start;
    start.col  = startCol;
    start.row  = startRow;
    start.type = RoomType::Standard;
    _grid[startRow][startCol] = 0;
    _rooms.push_back(start);
    _startIdx = 0;

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

    // Boss room = room furthest from start (most doors to traverse).
    _bossIdx = FindFurthest(_startIdx);
    if (_bossIdx >= 0)
        _rooms[_bossIdx].type = RoomType::Boss;

    // Key room = furthest room that isn't start or boss.
    int bestDist = -1;
    _keyIdx = -1;
    for (int i = 0; i < (int)_rooms.size(); i++)
    {
        if (i == _startIdx || i == _bossIdx)
            continue;
        int d = DistanceBFS(_startIdx, i);
        if (d > bestDist)
        {
            bestDist = d;
            _keyIdx  = i;
        }
    }

    // Sprinkle Elite, Rest, Treasure, and Shop rooms across the remainder.
    for (int i = 0; i < (int)_rooms.size(); i++)
    {
        if (i == _startIdx || i == _bossIdx || i == _keyIdx)
            continue;
        int roll = GetRandomValue(0, 9);
        if      (roll <= 1) _rooms[i].type = RoomType::Elite;
        else if (roll <= 2) _rooms[i].type = RoomType::Rest;
        else if (roll <= 3) _rooms[i].type = RoomType::Treasure;
        else if (roll <= 4) _rooms[i].type = RoomType::Store;
        // else stays Standard
    }
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
