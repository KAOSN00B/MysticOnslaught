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

void DungeonGen::GenerateEditorPlaytest()
{
    Generate();
    // Generate() applies campaign-only one-way entrance and boss restrictions.
    // The editor tests room connectivity, so restore every real grid adjacency.
    BuildConnections();
}

void DungeonGen::GeneratePrologue()
{
    _rooms.clear();
    _startIdx = 0;
    _bossIdx = -1;
    _keyIdx = -1;

    for (int row = 0; row < kGridSize; ++row)
        for (int col = 0; col < kGridSize; ++col)
            _grid[row][col] = -1;

    constexpr int row = kGridSize / 2;
    constexpr int firstCol = 2;
    for (int index = 0; index < 3; ++index)
    {
        DungeonRoom room;
        room.row = row;
        room.col = firstCol + index;
        room.type = RoomType::Standard;
        room.hasWest = index > 0;
        room.hasEast = index < 2;
        _grid[room.row][room.col] = index;
        _rooms.push_back(room);
    }
}

void DungeonGen::GrowRooms()
{
    // Enter from a random outer edge. Corners stay free so both the entrance
    // and its first inward connection have a readable, predictable direction.
    const int entranceEdge = GetRandomValue(0, 3); // bottom, left, right, top
    const int edgePosition = GetRandomValue(1, kGridSize - 2);
    int startRow = 0, startCol = 0;
    int firstRow = 0, firstCol = 0;
    switch (entranceEdge)
    {
    case 0: // bottom -> north
        startRow = kGridSize - 1; startCol = edgePosition;
        firstRow = startRow - 1;  firstCol = startCol;
        break;
    case 1: // left -> east
        startRow = edgePosition; startCol = 0;
        firstRow = startRow;     firstCol = startCol + 1;
        break;
    case 2: // right -> west
        startRow = edgePosition; startCol = kGridSize - 1;
        firstRow = startRow;     firstCol = startCol - 1;
        break;
    default: // top -> south
        startRow = 0;            startCol = edgePosition;
        firstRow = startRow + 1; firstCol = startCol;
        break;
    }

    DungeonRoom start;
    start.col  = startCol;
    start.row  = startRow;
    start.type = RoomType::Standard;
    start.startsEmpty = GetRandomValue(0, 2) == 0;
    _grid[startRow][startCol] = 0;
    _rooms.push_back(start);
    _startIdx = 0;

    DungeonRoom firstRoom;
    firstRoom.col  = firstCol;
    firstRoom.row  = firstRow;
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

        // Never add a second room beside the entrance. Removing that door
        // later could isolate a valid branch from the rest of the dungeon.
        if (std::abs(nr - startRow) + std::abs(nc - startCol) == 1 &&
            !(nr == firstRow && nc == firstCol))
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

    auto degree = [&](int roomIdx)
    {
        const DungeonRoom& room = _rooms[roomIdx];
        return (int)room.hasNorth + (int)room.hasSouth +
               (int)room.hasEast + (int)room.hasWest;
    };

    for (int i = 0; i < (int)_rooms.size(); ++i)
    {
        _rooms[i].depth = std::max(0, DistanceBFS(_startIdx, i));
        _rooms[i].encounterProfile = _rooms[i].depth <= 1
            ? EncounterProfile::Skirmish : EncounterProfile::Assault;
    }

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

    // Collect rooms that may hold authored reward/pressure beats. The key room
    // remains ordinary so its existing progression behaviour is preserved.
    std::vector<int> pool;
    for (int i = 0; i < (int)_rooms.size(); i++)
    {
        if (i == _startIdx || i == _bossIdx || i == _keyIdx)
            continue;
        pool.push_back(i);
    }
    auto takeBest = [&](auto score) -> int
    {
        if (pool.empty()) return -1;
        int bestPosition = 0;
        int bestScore = score(pool[0]);
        for (int position = 1; position < (int)pool.size(); ++position)
        {
            const int candidateScore = score(pool[position]);
            if (candidateScore > bestScore ||
                (candidateScore == bestScore && GetRandomValue(0, 1) == 1))
            {
                bestPosition = position;
                bestScore = candidateScore;
            }
        }
        const int result = pool[bestPosition];
        pool.erase(pool.begin() + bestPosition);
        return result;
    };

    // Treasure prefers a deep dead end, giving side branches an intentional
    // payoff. Elite pressure sits deep in the route, but does not compete with
    // the boss or treasure room for the same graph slot.
    const int treasure = takeBest([&](int roomIdx)
    {
        return _rooms[roomIdx].depth * 10 + (degree(roomIdx) == 1 ? 24 : 0);
    });
    if (treasure >= 0)
    {
        _rooms[treasure].type = RoomType::Treasure;
        _rooms[treasure].encounterProfile = EncounterProfile::Skirmish;
    }

    const int elite = takeBest([&](int roomIdx)
    {
        return _rooms[roomIdx].depth * 10 + (degree(roomIdx) >= 2 ? 4 : 0);
    });
    if (elite >= 0)
    {
        _rooms[elite].type = RoomType::Elite;
        _rooms[elite].encounterProfile = EncounterProfile::Assault;
    }

    if (_bossIdx >= 0)
        _rooms[_bossIdx].encounterProfile = EncounterProfile::Assault;

    // One holdout and one swarm create a readable rhythm in every full graph.
    // They remain Standard rooms so rewards and existing room-clear handling do
    // not need another special-room type.
    auto profileCandidate = [&](EncounterProfile profile, bool avoidBossNeighbor) -> int
    {
        int best = -1;
        int bestScore = std::numeric_limits<int>::min();
        for (int pass = 0; pass < 2 && best < 0; ++pass)
        {
            for (int roomIdx : pool)
            {
                if (_rooms[roomIdx].type != RoomType::Standard) continue;
                const bool bossNeighbor = _bossIdx >= 0 &&
                    std::abs(_rooms[roomIdx].row - _rooms[_bossIdx].row) +
                    std::abs(_rooms[roomIdx].col - _rooms[_bossIdx].col) == 1;
                if (avoidBossNeighbor && pass == 0 && bossNeighbor) continue;
                const int score = _rooms[roomIdx].depth * 10 +
                                  (degree(roomIdx) >= 2 ? 5 : 0);
                if (score > bestScore)
                {
                    best = roomIdx;
                    bestScore = score;
                }
            }
            if (!avoidBossNeighbor) break;
        }
        if (best >= 0)
        {
            _rooms[best].encounterProfile = profile;
            pool.erase(std::remove(pool.begin(), pool.end(), best), pool.end());
        }
        return best;
    };
    profileCandidate(EncounterProfile::Holdout, true);
    profileCandidate(EncounterProfile::Swarm, false);

    _rooms[_startIdx].encounterProfile = EncounterProfile::Skirmish;

    // The entrance's single inward connection was guaranteed while growing.
    // Boss room gets exactly one dungeon connector. Other adjacent rooms may
    // exist in the grid, but they should not open extra boss entrances.
    if (_bossIdx >= 0)
    {
        struct BossNeighbor
        {
            int roomIdx;
            int distFromStart;
            int sideFromBoss; // 0=N, 1=S, 2=E, 3=W
        };

        DungeonRoom& boss = _rooms[_bossIdx];
        BossNeighbor choices[4]{};
        int choiceCount = 0;

        auto addChoice = [&](int nr, int nc, int sideFromBoss)
        {
            if (nr < 0 || nr >= kGridSize || nc < 0 || nc >= kGridSize)
                return;
            int neighborIdx = _grid[nr][nc];
            if (neighborIdx < 0)
                return;
            choices[choiceCount++] = BossNeighbor{ neighborIdx, DistanceBFS(_startIdx, neighborIdx), sideFromBoss };
        };

        addChoice(boss.row - 1, boss.col, 0);
        addChoice(boss.row + 1, boss.col, 1);
        addChoice(boss.row, boss.col + 1, 2);
        addChoice(boss.row, boss.col - 1, 3);

        boss.hasNorth = false;
        boss.hasSouth = false;
        boss.hasEast  = false;
        boss.hasWest  = false;

        int best = -1;
        for (int i = 0; i < choiceCount; i++)
        {
            DungeonRoom& neighbor = _rooms[choices[i].roomIdx];
            switch (choices[i].sideFromBoss)
            {
            case 0: neighbor.hasSouth = false; break;
            case 1: neighbor.hasNorth = false; break;
            case 2: neighbor.hasWest  = false; break;
            case 3: neighbor.hasEast  = false; break;
            }

            if (best < 0 || choices[i].distFromStart < choices[best].distFromStart)
                best = i;
        }

        if (best >= 0)
        {
            DungeonRoom& neighbor = _rooms[choices[best].roomIdx];
            switch (choices[best].sideFromBoss)
            {
            case 0: boss.hasNorth = true; neighbor.hasSouth = true; break;
            case 1: boss.hasSouth = true; neighbor.hasNorth = true; break;
            case 2: boss.hasEast  = true; neighbor.hasWest  = true; break;
            case 3: boss.hasWest  = true; neighbor.hasEast  = true; break;
            }
        }
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
