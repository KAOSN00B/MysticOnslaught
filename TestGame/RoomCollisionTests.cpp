#include "RoomCollision.h"

#include <cassert>
#include <cmath>

namespace
{
    bool Near(float a, float b) { return std::abs(a - b) < 0.01f; }
}

int main()
{
    RoomLayout room{};
    room.handcrafted = true;
    for (int row = 0; row < RoomLayout::kRows; ++row)
        for (int col = 0; col < RoomLayout::kCols; ++col)
            room.tiles[row][col] = TileType::Floor;

    constexpr float cellW = 48.f;
    constexpr float cellH = 48.f;
    room.tiles[5][6] = TileType::WallBody;
    room.tiles[7][8] = TileType::Void;
    room.fall[9][10] = true;

    assert(IsSolidRoomTile(TileType::WallBody));
    assert(IsSolidRoomTile(TileType::Void));
    assert(IsSolidRoomTile(TileType::DoorLocked));
    assert(!IsSolidRoomTile(TileType::Floor));
    assert(!IsSolidRoomTile(TileType::DoorOpen));

    const Vector2 oldPos{ 6.f * cellW - 30.f, 5.f * cellH + 12.f };
    const Vector2 newPos{ 6.f * cellW + 12.f, 5.f * cellH + 18.f };
    const Rectangle bodyAtNew{ newPos.x - 10.f, newPos.y - 10.f, 20.f, 20.f };
    Vector2 resolved = ResolveHandcraftedTileMovement(
        room, oldPos, newPos, bodyAtNew, cellW, cellH);
    assert(Near(resolved.x, oldPos.x));
    assert(Near(resolved.y, newPos.y));

    assert(IsRoomFallPoint(room, { 10.5f * cellW, 9.5f * cellH }, cellW, cellH));
    assert(!IsRoomFallPoint(room, { 4.5f * cellW, 4.5f * cellH }, cellW, cellH));

    Vector2 desired{ 10.5f * cellW, 9.5f * cellH };
    Vector2 safe = FindNearestSafeRoomPosition(room, desired, cellW, cellH);
    assert(!IsRoomFallPoint(room, safe, cellW, cellH));
    assert(!IsSolidRoomTile(room.tiles[(int)(safe.y / cellH)][(int)(safe.x / cellW)]));
    return 0;
}
