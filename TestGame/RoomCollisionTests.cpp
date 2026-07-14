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

    // ── Authored wall + Door Zone: closed keeps the wall, open reveals ground ──
    RoomLayout dz{};
    dz.handcrafted = true;
    for (int r = 0; r < RoomLayout::kRows; ++r)
        for (int c = 0; c < RoomLayout::kCols; ++c) dz.tiles[r][c] = TileType::Floor;
    // Painted collision spanning a top-edge wall the designer drew through the door.
    for (int c = 11; c <= 16; ++c) dz.solid[0][c] = true;
    dz.doorZones[0] = { true, { 12.f, 0.f, 3.f, 1.f } };   // top zone over part of the wall

    RoomTilePlacement wallVisual   { "Forest",  TileType::WallBody, false, { 0.f, 0.f, 16.f, 16.f }, 13, 0 };
    RoomTilePlacement groundVisual  { "Ground",  TileType::Floor,    true,  { 0.f, 0.f, 16.f, 16.f }, 13, 0 };
    RoomTilePlacement outsideVisual { "Forest",  TileType::WallBody, false, { 0.f, 0.f, 16.f, 16.f },  2, 0 };
    RoomTilePlacement wideVisual    { "Forest",  TileType::WallBody, false, { 0.f, 0.f, 48.f, 16.f }, 11, 0 }; // 3 tiles wide

    // Closed door: wall visuals stay (2), collision stays solid (3).
    assert(!RoomVisualClearedByOpenDoor(wallVisual, dz));
    assert(!RoomPlacementClearsAtDoor({ 13.f, 0.f, 1.f, 1.f }, dz));

    dz.doorZoneOpen[0] = true;   // open the entry door
    assert(RoomVisualClearedByOpenDoor(wallVisual, dz));    // (4) non-ground visual hides
    assert(!RoomVisualClearedByOpenDoor(groundVisual, dz)); // (6) ground stays
    assert(!RoomVisualClearedByOpenDoor(outsideVisual, dz));// (7) outside the zone stays
    assert(RoomVisualClearedByOpenDoor(wideVisual, dz));    // (8) multi-tile hides as one
    assert(RoomPlacementClearsAtDoor({ 13.f, 0.f, 1.f, 1.f }, dz));   // (5) collision clears
    assert(!RoomPlacementClearsAtDoor({ 16.f, 0.f, 1.f, 1.f }, dz));  // wall cell outside zone stays solid

    // Movement agrees on both sides of the state: open door is passable through
    // the authored edge wall, a closed door blocks on it.
    const Vector2 edgeOld{ 13.5f * cellW, 2.0f * cellH };
    const Vector2 edgeNew{ 13.5f * cellW, 0.4f * cellH };
    const Rectangle edgeBody{ edgeNew.x - 10.f, edgeNew.y - 10.f, 20.f, 20.f };
    Vector2 edgeOpen = ResolveHandcraftedTileMovement(dz, edgeOld, edgeNew, edgeBody, cellW, cellH);
    assert(Near(edgeOpen.y, edgeNew.y));
    dz.doorZoneOpen[0] = false;
    Vector2 edgeClosed = ResolveHandcraftedTileMovement(dz, edgeOld, edgeNew, edgeBody, cellW, cellH);
    assert(Near(edgeClosed.y, edgeOld.y));

    // (9) Entry door open independently while the other exits stay closed.
    RoomLayout multi{};
    multi.handcrafted = true;
    for (int r = 0; r < RoomLayout::kRows; ++r)
        for (int c = 0; c < RoomLayout::kCols; ++c) multi.tiles[r][c] = TileType::Floor;
    multi.doorZones[0] = { true, { 12.f, 0.f,  3.f, 1.f } };  // top (entry)
    multi.doorZones[1] = { true, { 12.f, 15.f, 3.f, 1.f } };  // bottom
    multi.doorZoneOpen[0] = true;
    multi.doorZoneOpen[1] = false;
    assert(RoomPlacementClearsAtDoor({ 13.f, 0.f,  1.f, 1.f }, multi));
    assert(!RoomPlacementClearsAtDoor({ 13.f, 15.f, 1.f, 1.f }, multi));
    return 0;
}
