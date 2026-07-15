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
    room.fallRects.push_back({ 4.25f, 4.25f, 1.50f, 0.75f });
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
    assert(IsRoomFallPoint(room, { 4.5f * cellW, 4.5f * cellH }, cellW, cellH));
    assert(!IsRoomFallPoint(room, { 4.1f * cellW, 4.5f * cellH }, cellW, cellH));
    assert(!IsRoomFallPoint(room, { 2.5f * cellW, 4.5f * cellH }, cellW, cellH));

    // Dash collision uses the full swept body, so crossing only the edge of a
    // painted cell or a narrow precise Fall Rect still counts as a wall hit.
    assert(RoomBodyIntersectsFall(
        room, { 9.80f * cellW, 9.20f * cellH, 0.35f * cellW, 0.45f * cellH },
        cellW, cellH));
    assert(RoomBodyIntersectsFall(
        room, { 4.00f * cellW, 4.35f * cellH, 0.35f * cellW, 0.30f * cellH },
        cellW, cellH));
    assert(!RoomBodyIntersectsFall(
        room, { 2.00f * cellW, 2.00f * cellH, 0.50f * cellW, 0.50f * cellH },
        cellW, cellH));

    Vector2 cellPull = RoomFallPullDirection(
        room, { 10.1f * cellW, 9.5f * cellH }, cellW, cellH);
    assert(cellPull.x > 0.9f);
    assert(std::abs(cellPull.y) < 0.1f);

    Vector2 rectPull = RoomFallPullDirection(
        room, { 4.3f * cellW, 4.5f * cellH }, cellW, cellH);
    assert(rectPull.x > 0.9f);
    assert(rectPull.y > 0.f);

    Vector2 noPull = RoomFallPullDirection(
        room, { 2.5f * cellW, 2.5f * cellH }, cellW, cellH);
    assert(Near(noPull.x, 0.f));
    assert(Near(noPull.y, 0.f));

    // Large fall areas pull into the local tile/section entered, never across
    // the whole pool toward its overall centre.
    RoomLayout broadFall{};
    broadFall.handcrafted = true;
    for (int col = 10; col <= 13; ++col) broadFall.fall[9][col] = true;
    Vector2 localCellPull = RoomFallPullDirection(
        broadFall, { 10.9f * cellW, 9.5f * cellH }, cellW, cellH);
    assert(localCellPull.x < -0.9f);
    assert(std::abs(localCellPull.y) < 0.1f);

    RoomLayout broadRect{};
    broadRect.handcrafted = true;
    broadRect.fallRects.push_back({ 4.f, 4.f, 4.f, 3.f });
    Vector2 localRectPull = RoomFallPullDirection(
        broadRect, { 4.9f * cellW, 4.5f * cellH }, cellW, cellH);
    assert(localRectPull.x < -0.9f);
    assert(std::abs(localRectPull.y) < 0.1f);

    Vector2 desired{ 10.5f * cellW, 9.5f * cellH };
    Vector2 safe = FindNearestSafeRoomPosition(room, desired, cellW, cellH);
    assert(!IsRoomFallPoint(room, safe, cellW, cellH));

    Vector2 rectDesired{ 4.5f * cellW, 4.5f * cellH };
    Vector2 rectSafe = FindNearestSafeRoomPosition(room, rectDesired, cellW, cellH);
    assert(!IsRoomFallPoint(room, rectSafe, cellW, cellH));
    assert(!IsSolidRoomTile(room.tiles[(int)(safe.y / cellH)][(int)(safe.x / cellW)]));

    // Enemy spawning validates the whole body, not just its centre point.
    RoomLayout spawnRoom{};
    spawnRoom.handcrafted = true;
    for (int row = 0; row < RoomLayout::kRows; ++row)
        for (int col = 0; col < RoomLayout::kCols; ++col)
            spawnRoom.tiles[row][col] = TileType::Floor;

    const Rectangle spawnBody{ 3.15f * cellW, 3.15f * cellH,
                               0.70f * cellW, 0.70f * cellH };
    assert(IsRoomSpawnAreaValid(spawnRoom, spawnBody, cellW, cellH));

    spawnRoom.fall[3][3] = true;
    assert(!IsRoomSpawnAreaValid(spawnRoom, spawnBody, cellW, cellH));
    spawnRoom.fall[3][3] = false;

    // The body overlaps this precise fall rectangle even though its centre does not.
    spawnRoom.fallRects.push_back({ 3.80f, 3.35f, 0.35f, 0.30f });
    assert(!IsRoomSpawnAreaValid(spawnRoom, spawnBody, cellW, cellH));
    spawnRoom.fallRects.clear();

    spawnRoom.colliders.push_back({ 3.65f, 3.25f, 0.40f, 0.50f });
    assert(!IsRoomSpawnAreaValid(spawnRoom, spawnBody, cellW, cellH));
    spawnRoom.colliders.clear();

    spawnRoom.solid[3][3] = true;
    assert(!IsRoomSpawnAreaValid(spawnRoom, spawnBody, cellW, cellH));
    spawnRoom.solid[3][3] = false;

    const std::vector<Rectangle> propCollision{
        Rectangle{ 3.70f * cellW, 3.20f * cellH, 0.50f * cellW, 0.60f * cellH }
    };
    assert(!IsRoomSpawnAreaValid(
        spawnRoom, spawnBody, cellW, cellH, propCollision));

    // ── Authored wall + Door Zone: closed keeps the wall, open reveals ground ──
    RoomLayout dz{};
    dz.handcrafted = true;
    for (int r = 0; r < RoomLayout::kRows; ++r)
        for (int c = 0; c < RoomLayout::kCols; ++c) dz.tiles[r][c] = TileType::Floor;
    // Painted collision spanning a top-edge wall the designer drew through the door.
    for (int c = 11; c <= 16; ++c) dz.solid[0][c] = true;
    dz.doorZones[0] = { true, { 12.f, 0.f, 3.f, 1.f } };   // top zone over part of the wall

    RoomTilePlacement wallVisual   { "Forest",  TileType::WallBody, false, { 0.f, 0.f, 16.f, 16.f }, 13, 0 };
    RoomTilePlacement doorVisual = wallVisual;
    doorVisual.door = true;
    RoomTilePlacement groundVisual  { "Ground",  TileType::Floor,    true,  { 0.f, 0.f, 16.f, 16.f }, 13, 0 };
    RoomTilePlacement outsideVisual { "Forest",  TileType::WallBody, false, { 0.f, 0.f, 16.f, 16.f },  2, 0 };
    RoomTilePlacement wideVisual    { "Forest",  TileType::WallBody, false, { 0.f, 0.f, 48.f, 16.f }, 11, 0 }; // 3 tiles wide

    // Closed door: wall visuals stay (2), collision stays solid (3).
    assert(!RoomVisualClearedByOpenDoor(wallVisual, dz));
    assert(!RoomPlacementClearsAtDoor({ 13.f, 0.f, 1.f, 1.f }, dz));
    assert(!ShouldDrawGeneratedRoomFloor(13, 0, dz));
    assert(!ShouldDrawGeneratedRoomTile(TileType::WallBody, dz));
    assert(!ShouldDrawGeneratedRoomTile(TileType::DoorLocked, dz));
    assert(ShouldDrawGeneratedRoomTile(TileType::ChestClosed, dz));
    assert(ShouldDrawGeneratedRoomTile(TileType::ChestOpen, dz));
    assert(ShouldDrawGeneratedRoomTile(TileType::BossKey, dz));

    dz.doorZoneOpen[0] = true;   // open the entry door
    assert(RoomVisualClearedByOpenDoor(wallVisual, dz));    // Visual-layer wall art hides
    assert(RoomVisualClearedByOpenDoor(doorVisual, dz));    // explicit Door-layer art also hides
    assert(!RoomVisualClearedByOpenDoor(groundVisual, dz)); // (6) ground stays
    assert(!RoomVisualClearedByOpenDoor(outsideVisual, dz));// (7) outside the zone stays
    assert(RoomVisualClearedByOpenDoor(wideVisual, dz));    // overlapping multi-tile Visual art hides
    assert(RoomPlacementClearsAtDoor({ 13.f, 0.f, 1.f, 1.f }, dz));   // (5) collision clears
    assert(!RoomPlacementClearsAtDoor({ 16.f, 0.f, 1.f, 1.f }, dz));  // wall cell outside zone stays solid
    assert(!ShouldDrawGeneratedRoomFloor(13, 0, dz)); // authored Ground is authoritative
    assert(!ShouldDrawGeneratedRoomFloor(16, 0, dz)); // no generated base anywhere

    RoomLayout proceduralDoor = dz;
    proceduralDoor.handcrafted = false;
    assert(ShouldDrawGeneratedRoomFloor(13, 0, proceduralDoor));
    assert(ShouldDrawGeneratedRoomTile(TileType::WallBody, proceduralDoor));
    assert(ShouldDrawGeneratedRoomTile(TileType::DoorLocked, proceduralDoor));

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
