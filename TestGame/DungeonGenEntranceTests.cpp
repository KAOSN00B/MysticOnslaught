#include "DungeonGen.h"

#include <cassert>

extern "C" int GetRandomValue(int min, int max)
{
    (void)max;
    return min;
}

int main()
{
    DungeonGen generator;
    generator.Generate();
    const int start = generator.GetStartIndex();
    const DungeonRoom& room = generator.GetRooms()[start];

    assert(generator.IsEntranceRoom(start));
    assert(room.type == RoomType::Standard);
    assert(room.hasNorth);
    assert(!room.hasSouth);
    assert(!room.hasEast);
    assert(!room.hasWest);

    generator.GeneratePrologue();
    const auto& prologue = generator.GetRooms();
    assert(prologue.size() == 3);
    assert(prologue[0].hasEast && !prologue[0].hasWest);
    assert(prologue[1].hasEast && prologue[1].hasWest);
    assert(!prologue[2].hasEast && prologue[2].hasWest);
    assert(prologue[0].row == prologue[1].row);
    assert(prologue[1].row == prologue[2].row);
    assert(prologue[0].col + 1 == prologue[1].col);
    assert(prologue[1].col + 1 == prologue[2].col);
    return 0;
}
