#pragma once

#include "RoomLayout.h"

bool IsSolidRoomTile(TileType tile);
bool IsRoomFallPoint(const RoomLayout& room, Vector2 worldPoint,
                     float cellWidth, float cellHeight);
bool RoomBodyIntersectsFall(const RoomLayout& room, Rectangle worldBody,
                            float cellWidth, float cellHeight);
Vector2 RoomFallPullDirection(const RoomLayout& room, Vector2 worldPoint,
                              float cellWidth, float cellHeight);
// Validates the full world-space spawn footprint against terrain, painted
// collision/fall data, precise authored rectangles, and caller-supplied prop
// collision rectangles.
bool IsRoomSpawnAreaValid(const RoomLayout& room, Rectangle worldBody,
                          float cellWidth, float cellHeight,
                          const std::vector<Rectangle>& extraBlockers = {});
bool RoomPlacementClearsAtDoor(Rectangle occupiedTiles,
                               const RoomLayout& room);
// Handcrafted open exits reveal only authored Ground. Procedural rooms retain
// their generated floor base because they do not require a painted Ground layer.
bool ShouldDrawGeneratedRoomFloor(int col, int row, const RoomLayout& room);
// Handcrafted rooms suppress generated terrain art. Runtime reward tiles remain
// visible because they are gameplay objects placed over the authored Ground.
bool ShouldDrawGeneratedRoomTile(TileType tile, const RoomLayout& room);
// True when an authored non-ground visual placement should be hidden because it
// intersects an open Door Zone. Ground placements are never hidden.
bool RoomVisualClearedByOpenDoor(const RoomTilePlacement& visual,
                                 const RoomLayout& room);
Vector2 ResolveHandcraftedTileMovement(const RoomLayout& room,
                                       Vector2 previousWorldPos,
                                       Vector2 desiredWorldPos,
                                       Rectangle collisionAtDesiredPos,
                                       float cellWidth, float cellHeight);
Vector2 FindNearestSafeRoomPosition(const RoomLayout& room,
                                    Vector2 desiredWorldPos,
                                    float cellWidth, float cellHeight);
