#pragma once

#include "GameTypes.h"

#include <vector>

class RoomDirector;

class MapDirector
{
public:
    std::vector<MapNode>& ActMapRef() { return _actMap; }
    int& CurrentMapNodeIdxRef() { return _currentMapNodeIdx; }
    int& KeySelectedIdxRef() { return _mapKeySelectedIdx; }
    float& MapOpenTimerRef() { return _mapOpenTimer; }

    void Reset();
    void GenerateActMap(int windowWidth, int windowHeight);
    bool TryEnterNode(int idx, RoomDirector& roomDirector, int& wave);
    void CompleteCurrentNode();

private:
    std::vector<MapNode> _actMap;
    int _currentMapNodeIdx = -1;
    int _mapKeySelectedIdx = -1;
    float _mapOpenTimer = 0.f;
};
