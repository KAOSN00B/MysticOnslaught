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
    room.solid[6][7] = true;

    assert(IsSolidRoomTile(TileType::WallBody));
    assert(IsSolidRoomTile(TileType::Void));
    assert(IsSolidRoomTile(TileType::DoorLocked));
    assert(!IsSolidRoomTile(TileType::Floor));
    assert(!IsSolidRoomTile(TileType::DoorOpen));

    const Vector2 maskOld{ 7.5f * cellW, 7.5f * cellH };
    const Vector2 maskNew{ 7.5f * cellW, 6.5f * cellH };
    const Rectangle maskBody{ maskNew.x - 10.f, maskNew.y - 10.f, 20.f, 20.f };
    Vector2 maskBlocked = ResolveHandcraftedTileMovement(
        room, maskOld, maskNew, maskBody, cellW, cellH);
    assert(Near(maskBlocked.y, maskOld.y));

    room.doorZones[(int)0] = { true, { 7.f, 6.f, 1.f, 1.f } };
    room.doorZoneOpen[(int)0] = true;
    assert(RoomPlacementClearsAtDoor({7.f,6.f,1.f,1.f},room));
    Vector2 maskOpen = ResolveHandcraftedTileMovement(
        room, maskOld, maskNew, maskBody, cellW, cellH);
    assert(Near(maskOpen.y, maskNew.y));
    room.doorZoneOpen[(int)0] = false;

    // A deeper authored top boundary blocks below the first visual wall tile.
    room.wallTopDepth = 1.5f;
    room.tiles[0][4] = TileType::WallBody;
    const Vector2 topOld{ 4.5f * cellW, 2.0f * cellH };
    const Vector2 topNew{ 4.5f * cellW, 1.25f * cellH };
    const Rectangle topBody{ topNew.x - 10.f, topNew.y - 10.f, 20.f, 20.f };
    Vector2 topResolved = ResolveHandcraftedTileMovement(
        room, topOld, topNew, topBody, cellW, cellH);
    assert(Near(topResolved.y, topOld.y));

    // Door-open edge cells remain openings even with an adjusted boundary.
    room.tiles[0][13] = TileType::DoorOpen;
    const Vector2 doorOld{ 13.5f * cellW, 2.0f * cellH };
    const Vector2 doorNew{ 13.5f * cellW, 0.5f * cellH };
    const Rectangle doorBody{ doorNew.x - 10.f, doorNew.y - 10.f, 20.f, 20.f };
    Vector2 doorResolved = ResolveHandcraftedTileMovement(
        room, doorOld, doorNew, doorBody, cellW, cellH);
    assert(Near(doorResolved.y, doorNew.y));

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
