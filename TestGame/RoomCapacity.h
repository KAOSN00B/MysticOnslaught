#pragma once

#include "RoomLayout.h"

enum class RoomCapacityBand : unsigned char
{
    Small,
    Medium,
    Large,
    Arena,
};

struct RoomCombatCapacity
{
    RoomCapacityBand band = RoomCapacityBand::Medium;
    int walkableTiles = 0;
    int connectedTiles = 0;
    int spawnTiles = 0;
    int chokeTiles = 0;
    int openingBodyCap = 6;
    int totalBodyCap = 10;
    int specialistCap = 2;
    int pressureCap = 10;
    bool authoredOverride = false;
};

class RoomCapacityAnalyzer
{
public:
    static RoomCombatCapacity Analyze(const RoomLayout& room,
                                      const TileDefSet& definitions);
    static const char* BandName(RoomCapacityBand band);
};
