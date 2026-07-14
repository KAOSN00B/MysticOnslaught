#pragma once

#include "RoomLayout.h"

bool IsSolidRoomTile(TileType tile);
bool IsRoomFallPoint(const RoomLayout& room, Vector2 worldPoint,
                     float cellWidth, float cellHeight);
bool RoomPlacementClearsAtDoor(Rectangle occupiedTiles,
                               const RoomLayout& room);
Vector2 ResolveHandcraftedTileMovement(const RoomLayout& room,
                                       Vector2 previousWorldPos,
                                       Vector2 desiredWorldPos,
                                       Rectangle collisionAtDesiredPos,
                                       float cellWidth, float cellHeight);
Vector2 FindNearestSafeRoomPosition(const RoomLayout& room,
                                    Vector2 desiredWorldPos,
                                    float cellWidth, float cellHeight);
