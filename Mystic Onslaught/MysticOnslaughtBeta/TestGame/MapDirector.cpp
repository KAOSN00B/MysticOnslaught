#include "MapDirector.h"

#include "RoomDirector.h"

#include <algorithm>
#include <cmath>

void MapDirector::Reset()
{
    _actMap.clear();
    _currentMapNodeIdx = -1;
    _mapKeySelectedIdx = -1;
    _mapOpenTimer = 0.f;
}

void MapDirector::GenerateActMap(int windowWidth, int windowHeight)
{
    Reset();

    const int rowCounts[6] = { 1, 3, 4, 4, 3, 1 };
    int rowStart[6] = {};
    for (int r = 1; r < 6; r++)
        rowStart[r] = rowStart[r - 1] + rowCounts[r - 1];

    RoomType row2Types[4] = {
        RoomType::Standard, RoomType::Standard, RoomType::Elite, RoomType::Treasure
    };
    RoomType row3Types[4] = {
        RoomType::Standard, RoomType::Standard, RoomType::Store, RoomType::Rest
    };

    for (int i = 0; i < 4; i++)
    {
        int j = GetRandomValue(i, 3);
        RoomType tmp = row2Types[i]; row2Types[i] = row2Types[j]; row2Types[j] = tmp;
    }
    for (int i = 0; i < 4; i++)
    {
        int j = GetRandomValue(i, 3);
        RoomType tmp = row3Types[i]; row3Types[i] = row3Types[j]; row3Types[j] = tmp;
    }

    for (int r = 0; r < 6; r++)
    {
        for (int i = 0; i < rowCounts[r]; i++)
        {
            MapNode n;
            n.row = r;
            n.normX = (rowCounts[r] == 1)
                ? 0.5f
                : 0.2f + (float)i / (rowCounts[r] - 1) * 0.6f;

            if      (r == 0) n.type = RoomType::Standard;
            else if (r == 5) n.type = RoomType::Boss;
            else if (r == 2) n.type = row2Types[i];
            else if (r == 3) n.type = row3Types[i];
            else             n.type = RoomType::Standard;

            n.available = (r == 0);
            n.completed = false;
            _actMap.push_back(n);
        }
    }

    for (int r = 0; r < 5; r++)
    {
        int sR  = rowStart[r],     cR  = rowCounts[r];
        int sR1 = rowStart[r + 1], cR1 = rowCounts[r + 1];

        for (int i = 0; i < cR; i++)
        {
            float t = (cR > 1) ? (float)i / (cR - 1) : 0.5f;
            int j = (int)roundf(t * (float)(cR1 - 1));
            j = std::max(0, std::min(cR1 - 1, j));
            int src = sR + i, dst = sR1 + j;
            auto& nxt = _actMap[src].nextNodes;
            if (std::find(nxt.begin(), nxt.end(), dst) == nxt.end())
                nxt.push_back(dst);
        }

        for (int j = 0; j < cR1; j++)
        {
            int dst = sR1 + j;
            bool found = false;
            for (int i = 0; i < cR && !found; i++)
            {
                auto& nxt = _actMap[sR + i].nextNodes;
                found = std::find(nxt.begin(), nxt.end(), dst) != nxt.end();
            }
            if (!found)
            {
                float dX = _actMap[dst].normX;
                int bestSrc = sR;
                float bestD = 999.f;
                for (int i = 0; i < cR; i++)
                {
                    float d = fabsf(_actMap[sR + i].normX - dX);
                    if (d < bestD) { bestD = d; bestSrc = sR + i; }
                }
                _actMap[bestSrc].nextNodes.push_back(dst);
            }
        }

        for (int i = 0; i < cR; i++)
        {
            if (cR1 > 1 && GetRandomValue(0, 9) < 3)
            {
                auto& nxt = _actMap[sR + i].nextNodes;
                if (nxt.empty()) continue;
                int baseJ = nxt[0] - sR1;
                int altJ  = (baseJ > 0) ? baseJ - 1 : baseJ + 1;
                altJ = std::max(0, std::min(cR1 - 1, altJ));
                int alt = sR1 + altJ;
                if (std::find(nxt.begin(), nxt.end(), alt) == nxt.end())
                    nxt.push_back(alt);
            }
        }
    }

    const float mapTop  = 130.f;
    const float mapBot  = (float)windowHeight - 110.f;
    const float rowH    = (mapBot - mapTop) / 5.f;
    const float mapLeft = (float)windowWidth  * 0.30f;
    const float mapRight= (float)windowWidth  * 0.76f;

    for (auto& node : _actMap)
    {
        float y = mapBot - node.row * rowH;
        float x = mapLeft + node.normX * (mapRight - mapLeft);
        node.drawPos = { x, y };
    }
}

bool MapDirector::TryEnterNode(int idx, RoomDirector& roomDirector, int& wave)
{
    if (idx < 0 || idx >= (int)_actMap.size())
        return false;

    int enteredRow = _actMap[idx].row;
    for (auto& n : _actMap)
        if (n.row <= enteredRow)
            n.available = false;

    _currentMapNodeIdx = idx;
    roomDirector.ApplyMapNodeEntry(_actMap[idx], wave);
    return true;
}

void MapDirector::CompleteCurrentNode()
{
    if (_currentMapNodeIdx < 0 || _currentMapNodeIdx >= (int)_actMap.size())
        return;

    _actMap[_currentMapNodeIdx].completed = true;
    for (int next : _actMap[_currentMapNodeIdx].nextNodes)
        if (next >= 0 && next < (int)_actMap.size())
            _actMap[next].available = true;
}
