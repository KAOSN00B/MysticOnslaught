#include "RoomCollision.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
    bool RectanglesOverlap(Rectangle a, Rectangle b)
    {
        return a.x < b.x + b.width && a.x + a.width > b.x &&
               a.y < b.y + b.height && a.y + a.height > b.y;
    }

    // A border cell is a wall if the designer painted collision there OR the
    // terrain tile is inherently solid. This lets an authored wall (trees, rocks,
    // any tileset) block on the room edge without relying on the legacy wall
    // TileTypes — collision is what the designer painted, not automatic art.
    bool RoomEdgeCellSolid(const RoomLayout& room, int col, int row)
    {
        return room.solid[row][col] || IsSolidRoomTile(room.tiles[row][col]);
    }

    bool HitsAdjustedRoomEdge(const RoomLayout& room, Rectangle body,
                              float cellWidth, float cellHeight)
    {
        const float roomWidth = RoomLayout::kCols * cellWidth;
        const float roomHeight = RoomLayout::kRows * cellHeight;

        for (int col = 0; col < RoomLayout::kCols; ++col)
        {
            const float x = col * cellWidth;
            // An open Door Zone punches a passable hole through the edge wall so
            // the authored opening becomes walkable without deleting any art.
            if (RoomEdgeCellSolid(room, col, 0) &&
                !RoomPlacementClearsAtDoor({ (float)col, 0.f, 1.f, 1.f }, room) &&
                RectanglesOverlap(body, { x, 0.f, cellWidth,
                                          room.wallTopDepth * cellHeight }))
                return true;
            if (RoomEdgeCellSolid(room, col, RoomLayout::kRows - 1) &&
                !RoomPlacementClearsAtDoor({ (float)col, (float)(RoomLayout::kRows - 1),
                                            1.f, 1.f }, room) &&
                RectanglesOverlap(body, { x, roomHeight - room.wallBottomDepth * cellHeight,
                                          cellWidth, room.wallBottomDepth * cellHeight }))
                return true;
        }

        for (int row = 0; row < RoomLayout::kRows; ++row)
        {
            const float y = row * cellHeight;
            if (RoomEdgeCellSolid(room, 0, row) &&
                !RoomPlacementClearsAtDoor({ 0.f, (float)row, 1.f, 1.f }, room) &&
                RectanglesOverlap(body, { 0.f, y,
                                          room.wallLeftDepth * cellWidth, cellHeight }))
                return true;
            if (RoomEdgeCellSolid(room, RoomLayout::kCols - 1, row) &&
                !RoomPlacementClearsAtDoor({ (float)(RoomLayout::kCols - 1), (float)row,
                                            1.f, 1.f }, room) &&
                RectanglesOverlap(body, { roomWidth - room.wallRightDepth * cellWidth, y,
                                          room.wallRightDepth * cellWidth, cellHeight }))
                return true;
        }
        return false;
    }
}

bool IsSolidRoomTile(TileType tile)
{
    switch (tile)
    {
    case TileType::WallBody:
    case TileType::WallTopFace:
    case TileType::WallCornerTL:
    case TileType::WallCornerTR:
    case TileType::WallInnerCornerL:
    case TileType::WallInnerCornerR:
    case TileType::WallLeft:
    case TileType::WallRight:
    case TileType::WallBottom:
    case TileType::WallCornerBL:
    case TileType::WallCornerBR:
    case TileType::WallInnerCornerBL:
    case TileType::WallInnerCornerBR:
    case TileType::Void:
    case TileType::DoorLocked:
    case TileType::None:
        return true;
    default:
        return false;
    }
}

bool IsRoomFallPoint(const RoomLayout& room, Vector2 worldPoint,
                     float cellWidth, float cellHeight)
{
    if (!room.handcrafted || cellWidth <= 0.f || cellHeight <= 0.f) return false;
    const int col = (int)std::floor(worldPoint.x / cellWidth);
    const int row = (int)std::floor(worldPoint.y / cellHeight);
    if (col < 0 || col >= RoomLayout::kCols || row < 0 || row >= RoomLayout::kRows)
        return false;
    return room.fall[row][col];
}

bool RoomPlacementClearsAtDoor(Rectangle occupiedTiles, const RoomLayout& room)
{
    for (int i = 0; i < 4; ++i)
    {
        const RoomDoorZone& zone = room.doorZones[i];
        if (!zone.enabled || (!room.roomCleared && !room.doorZoneOpen[i])) continue;
        if (RectanglesOverlap(occupiedTiles, zone.tiles)) return true;
    }
    return false;
}

bool RoomVisualClearedByOpenDoor(const RoomTilePlacement& visual, const RoomLayout& room)
{
    // Ground stays no matter what — an opened door reveals the floor beneath the
    // wall, it never removes the floor itself.
    if (visual.ground) return false;
    // Whole-placement rectangle test: a multi-tile wall piece that overlaps an
    // open Door Zone is hidden as one unit (no pixel-level clipping in this phase).
    const Rectangle occupied{ (float)visual.col, (float)visual.row,
                              visual.src.width / 16.f, visual.src.height / 16.f };
    return RoomPlacementClearsAtDoor(occupied, room);
}

Vector2 ResolveHandcraftedTileMovement(const RoomLayout& room,
                                       Vector2 previousWorldPos,
                                       Vector2 desiredWorldPos,
                                       Rectangle collisionAtDesiredPos,
                                       float cellWidth, float cellHeight)
{
    if (!room.handcrafted || cellWidth <= 0.f || cellHeight <= 0.f)
        return desiredWorldPos;

    const Vector2 bodyOffset{
        collisionAtDesiredPos.x - desiredWorldPos.x,
        collisionAtDesiredPos.y - desiredWorldPos.y
    };
    auto bodyAt = [&](Vector2 pos) {
        return Rectangle{ pos.x + bodyOffset.x, pos.y + bodyOffset.y,
                          collisionAtDesiredPos.width, collisionAtDesiredPos.height };
    };
    auto hitsSolid = [&](Rectangle body) {
        // Door state is expressed entirely through authored collision plus the
        // open-Door-Zone clearing below (and in HitsAdjustedRoomEdge). A closed
        // zone adds no automatic collision of its own — the wall the designer
        // painted through it is what keeps it solid, so movement, projectiles,
        // spawns and navigation all read the same authored data.
        if (HitsAdjustedRoomEdge(room, body, cellWidth, cellHeight)) return true;
        const int minCol = std::max(0, (int)std::floor(body.x / cellWidth));
        const int maxCol = std::min(RoomLayout::kCols - 1,
            (int)std::floor((body.x + body.width - 0.001f) / cellWidth));
        const int minRow = std::max(0, (int)std::floor(body.y / cellHeight));
        const int maxRow = std::min(RoomLayout::kRows - 1,
            (int)std::floor((body.y + body.height - 0.001f) / cellHeight));
        for (int row = minRow; row <= maxRow; ++row)
        {
            for (int col = minCol; col <= maxCol; ++col)
            {
                // Border cells use the room's independently authored edge depths.
                if (row == 0 || row == RoomLayout::kRows - 1 ||
                    col == 0 || col == RoomLayout::kCols - 1)
                    continue;
                if (RoomPlacementClearsAtDoor({(float)col,(float)row,1.f,1.f},room)) continue;
                if (!room.solid[row][col] && !IsSolidRoomTile(room.tiles[row][col])) continue;
                const Rectangle tileRect{ col * cellWidth, row * cellHeight,
                                          cellWidth, cellHeight };
                if (RectanglesOverlap(body, tileRect)) return true;
            }
        }
        return false;
    };

    Vector2 resolved = previousWorldPos;
    Vector2 horizontal{ desiredWorldPos.x, previousWorldPos.y };
    if (!hitsSolid(bodyAt(horizontal))) resolved.x = horizontal.x;
    Vector2 vertical{ resolved.x, desiredWorldPos.y };
    if (!hitsSolid(bodyAt(vertical))) resolved.y = vertical.y;
    return resolved;
}

Vector2 FindNearestSafeRoomPosition(const RoomLayout& room,
                                    Vector2 desiredWorldPos,
                                    float cellWidth, float cellHeight)
{
    if (cellWidth > 0.f && cellHeight > 0.f)
    {
        const int desiredCol = (int)std::floor(desiredWorldPos.x / cellWidth);
        const int desiredRow = (int)std::floor(desiredWorldPos.y / cellHeight);
        if (desiredCol >= 0 && desiredCol < RoomLayout::kCols &&
            desiredRow >= 0 && desiredRow < RoomLayout::kRows &&
            !room.fall[desiredRow][desiredCol] &&
            (!room.solid[desiredRow][desiredCol] ||
             RoomPlacementClearsAtDoor({(float)desiredCol,(float)desiredRow,1.f,1.f},room)) &&
            !IsSolidRoomTile(room.tiles[desiredRow][desiredCol]))
            return desiredWorldPos;
    }

    Vector2 best = desiredWorldPos;
    float bestDistanceSq = std::numeric_limits<float>::max();
    for (int row = 0; row < RoomLayout::kRows; ++row)
    {
        for (int col = 0; col < RoomLayout::kCols; ++col)
        {
            if (room.fall[row][col] ||
                (room.solid[row][col] &&
                 !RoomPlacementClearsAtDoor({(float)col,(float)row,1.f,1.f},room)) ||
                IsSolidRoomTile(room.tiles[row][col])) continue;
            const Vector2 candidate{ (col + 0.5f) * cellWidth,
                                     (row + 0.5f) * cellHeight };
            const float dx = candidate.x - desiredWorldPos.x;
            const float dy = candidate.y - desiredWorldPos.y;
            const float distanceSq = dx * dx + dy * dy;
            if (distanceSq < bestDistanceSq)
            {
                bestDistanceSq = distanceSq;
                best = candidate;
            }
        }
    }
    return best;
}
