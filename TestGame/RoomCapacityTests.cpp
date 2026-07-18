#include "RoomCapacity.h"

#include <cassert>

namespace
{
    RoomLayout OpenRoom()
    {
        RoomLayout room{};
        for (int row = 0; row < RoomLayout::kRows; ++row)
            for (int col = 0; col < RoomLayout::kCols; ++col)
                room.tiles[row][col] = TileType::Floor;
        return room;
    }
}

int main()
{
    TileDefSet definitions{};

    RoomLayout open = OpenRoom();
    const RoomCombatCapacity openCapacity =
        RoomCapacityAnalyzer::Analyze(open, definitions);
    assert(openCapacity.band == RoomCapacityBand::Large);
    assert(openCapacity.openingBodyCap == 8);
    assert(openCapacity.totalBodyCap == 13);

    RoomLayout constrained = OpenRoom();
    for (int row = 0; row < RoomLayout::kRows; ++row)
        for (int col = 0; col < RoomLayout::kCols; ++col)
            constrained.solid[row][col] = !(col >= 10 && col <= 16 &&
                                             row >= 5 && row <= 10);
    const RoomCombatCapacity smallCapacity =
        RoomCapacityAnalyzer::Analyze(constrained, definitions);
    assert(smallCapacity.band == RoomCapacityBand::Small);
    assert(smallCapacity.openingBodyCap == 4);
    assert(smallCapacity.totalBodyCap == 7);
    assert(smallCapacity.specialistCap == 1);

    constrained.combatCapacityOverride = RoomCapacityOverride::Arena;
    const RoomCombatCapacity overridden =
        RoomCapacityAnalyzer::Analyze(constrained, definitions);
    assert(overridden.band == RoomCapacityBand::Arena);
    assert(overridden.openingBodyCap == 10);
    assert(overridden.totalBodyCap == 18);
    assert(overridden.authoredOverride);

    return 0;
}
