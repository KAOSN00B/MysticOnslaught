#pragma once

#include "GameTypes.h"

#include <vector>

class RoomDirector
{
public:
    int& CurrentActRef() { return _currentAct; }
    int& CurrentRoomRef() { return _currentRoom; }
    RoomType& CurrentRoomTypeRef() { return _currentRoomType; }
    bool& PendingRoomChoiceRef() { return _pendingRoomChoice; }
    bool& RoomClearPendingRef() { return _roomClearPending; }
    float& RoomClearTimerRef() { return _roomClearTimer; }
    bool& StartBiomeDungeonRef() { return _startBiomeDungeon; }
    std::vector<Biome>& BiomeSequenceRef() { return _biomeSequence; }

    void ResetForNewRun(int totalActs);
    void StartNextRoom(RoomType type, int& wave);
    void ApplyMapNodeEntry(const MapNode& node, int& wave);
    void AdvanceActAfterBoss();
    Biome GetBiomeForAct(int act) const;

private:
    static bool UsesClearDelay(RoomType type);

    int _currentAct = 1;
    int _currentRoom = 0;
    RoomType _currentRoomType = RoomType::Standard;
    bool _pendingRoomChoice = false;
    bool _roomClearPending = false;
    float _roomClearTimer = 0.f;
    bool _startBiomeDungeon = true;
    std::vector<Biome> _biomeSequence;
};
