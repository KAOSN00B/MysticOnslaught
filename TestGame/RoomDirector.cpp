#include "RoomDirector.h"

static constexpr float kUtilityRoomClearDelay = 5.0f;

bool RoomDirector::UsesClearDelay(RoomType type)
{
    return type == RoomType::Rest || type == RoomType::Store;
}

void RoomDirector::ResetForNewRun(int totalActs)
{
    _currentAct = 1;
    _currentRoom = 0;
    _currentRoomType = RoomType::Standard;
    _pendingRoomChoice = false;
    _roomClearPending = false;
    _roomClearTimer = 0.f;

    static constexpr Biome kAllBiomes[] = {
        Biome::Caverns, Biome::Forest
    };
    static constexpr int kBiomeCount = (int)(sizeof(kAllBiomes) / sizeof(kAllBiomes[0]));

    _biomeSequence.clear();
    Biome last = (Biome)-1;
    for (int i = 0; i < totalActs; i++)
    {
        Biome pick;
        do { pick = kAllBiomes[GetRandomValue(0, kBiomeCount - 1)]; }
        while (pick == last);
        _biomeSequence.push_back(pick);
        last = pick;
    }

    if (!_biomeSequence.empty())
        _startBiomeDungeon = (_biomeSequence[0] == Biome::Caverns);
}

void RoomDirector::StartNextRoom(RoomType type, int& wave)
{
    _currentRoom++;
    if (_currentRoom > 6)
    {
        _currentRoom = 1;
        _currentAct++;
    }

    wave++;
    _currentRoomType = type;
    _roomClearPending = false;
    _roomClearTimer = UsesClearDelay(type) ? kUtilityRoomClearDelay : 0.f;
}

void RoomDirector::ApplyMapNodeEntry(const MapNode& node, int& wave)
{
    _roomClearPending = false;
    _currentRoom = node.row + 1;
    _currentRoomType = node.type;
    _roomClearTimer = UsesClearDelay(node.type) ? kUtilityRoomClearDelay : 0.f;
    wave++;
}

void RoomDirector::AdvanceActAfterBoss()
{
    _currentAct++;
}

Biome RoomDirector::GetBiomeForAct(int act) const
{
    int idx = act - 1;
    if (idx >= 0 && idx < (int)_biomeSequence.size())
        return _biomeSequence[idx];

    bool isDungeon = _startBiomeDungeon ? ((act % 2) == 1) : ((act % 2) == 0);
    return isDungeon ? Biome::Caverns : Biome::Forest;
}
