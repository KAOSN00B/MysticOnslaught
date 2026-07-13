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
                if (!IsSolidRoomTile(room.tiles[row][col])) continue;
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
            !IsSolidRoomTile(room.tiles[desiredRow][desiredCol]))
            return desiredWorldPos;
    }

    Vector2 best = desiredWorldPos;
    float bestDistanceSq = std::numeric_limits<float>::max();
    for (int row = 0; row < RoomLayout::kRows; ++row)
    {
        for (int col = 0; col < RoomLayout::kCols; ++col)
        {
            if (room.fall[row][col] || IsSolidRoomTile(room.tiles[row][col])) continue;
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
