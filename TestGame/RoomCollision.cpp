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
    const Vector2 tilePoint{ worldPoint.x / cellWidth, worldPoint.y / cellHeight };
    for (const Rectangle& rect : room.fallRects)
        if (tilePoint.x >= rect.x && tilePoint.x < rect.x + rect.width &&
            tilePoint.y >= rect.y && tilePoint.y < rect.y + rect.height)
            return true;
    const int col = (int)std::floor(worldPoint.x / cellWidth);
    const int row = (int)std::floor(worldPoint.y / cellHeight);
    if (col < 0 || col >= RoomLayout::kCols || row < 0 || row >= RoomLayout::kRows)
        return false;
    return room.fall[row][col];
}

bool RoomBodyIntersectsFall(const RoomLayout& room, Rectangle worldBody,
                            float cellWidth, float cellHeight)
{
    if (!room.handcrafted || cellWidth <= 0.f || cellHeight <= 0.f ||
        worldBody.width <= 0.f || worldBody.height <= 0.f)
        return false;

    for (const Rectangle& fallRect : room.fallRects)
    {
        const Rectangle worldFall{
            fallRect.x * cellWidth, fallRect.y * cellHeight,
            fallRect.width * cellWidth, fallRect.height * cellHeight
        };
        if (RectanglesOverlap(worldBody, worldFall)) return true;
    }

    constexpr float kEdgeEpsilon = 0.001f;
    const int firstCol = std::max(0, (int)std::floor(worldBody.x / cellWidth));
    const int lastCol = std::min(RoomLayout::kCols - 1, (int)std::floor(
        (worldBody.x + worldBody.width - kEdgeEpsilon) / cellWidth));
    const int firstRow = std::max(0, (int)std::floor(worldBody.y / cellHeight));
    const int lastRow = std::min(RoomLayout::kRows - 1, (int)std::floor(
        (worldBody.y + worldBody.height - kEdgeEpsilon) / cellHeight));
    if (firstCol > lastCol || firstRow > lastRow) return false;

    for (int row = firstRow; row <= lastRow; ++row)
        for (int col = firstCol; col <= lastCol; ++col)
            if (room.fall[row][col]) return true;
    return false;
}

Vector2 RoomFallPullDirection(const RoomLayout& room, Vector2 worldPoint,
                              float cellWidth, float cellHeight)
{
    if (!IsRoomFallPoint(room, worldPoint, cellWidth, cellHeight))
        return { 0.f, 0.f };

    const Vector2 tilePoint{ worldPoint.x / cellWidth, worldPoint.y / cellHeight };
    const int baseCol = (int)std::floor(tilePoint.x);
    const int baseRow = (int)std::floor(tilePoint.y);
    Vector2 target{};
    float bestDistanceSq = std::numeric_limits<float>::max();
    auto considerSection = [&](float left, float top, float right, float bottom)
    {
        if (right <= left || bottom <= top) return;
        const Vector2 candidate{
            (left + right) * 0.5f * cellWidth,
            (top + bottom) * 0.5f * cellHeight
        };
        const float dx = candidate.x - worldPoint.x;
        const float dy = candidate.y - worldPoint.y;
        const float distanceSq = dx * dx + dy * dy;
        if (distanceSq < bestDistanceSq)
        {
            bestDistanceSq = distanceSq;
            target = candidate;
        }
    };

    if (baseCol >= 0 && baseCol < RoomLayout::kCols &&
        baseRow >= 0 && baseRow < RoomLayout::kRows &&
        room.fall[baseRow][baseCol])
        considerSection((float)baseCol, (float)baseRow,
                        (float)baseCol + 1.f, (float)baseRow + 1.f);

    for (const Rectangle& rect : room.fallRects)
    {
        if (tilePoint.x < rect.x || tilePoint.x >= rect.x + rect.width ||
            tilePoint.y < rect.y || tilePoint.y >= rect.y + rect.height)
            continue;
        // A large free-size fall rectangle is divided into local tile-sized
        // sections. Pull toward the section entered, not the rectangle centre.
        considerSection(std::max(rect.x, (float)baseCol),
                        std::max(rect.y, (float)baseRow),
                        std::min(rect.x + rect.width, (float)baseCol + 1.f),
                        std::min(rect.y + rect.height, (float)baseRow + 1.f));
    }

    if (bestDistanceSq == std::numeric_limits<float>::max()) return { 0.f, 0.f };
    Vector2 direction{
        target.x - worldPoint.x,
        target.y - worldPoint.y
    };
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (length < 1.f) return { 0.f, 0.f };
    return { direction.x / length, direction.y / length };
}

bool IsRoomSpawnAreaValid(const RoomLayout& room, Rectangle worldBody,
                          float cellWidth, float cellHeight,
                          const std::vector<Rectangle>& extraBlockers)
{
    if (cellWidth <= 0.f || cellHeight <= 0.f ||
        worldBody.width <= 0.f || worldBody.height <= 0.f)
        return false;

    const float roomWidth = RoomLayout::kCols * cellWidth;
    const float roomHeight = RoomLayout::kRows * cellHeight;
    if (worldBody.x < 0.f || worldBody.y < 0.f ||
        worldBody.x + worldBody.width > roomWidth ||
        worldBody.y + worldBody.height > roomHeight)
        return false;

    for (const Rectangle& blocker : extraBlockers)
        if (RectanglesOverlap(worldBody, blocker))
            return false;

    for (const Rectangle& collider : room.colliders)
    {
        const Rectangle worldCollider{
            collider.x * cellWidth, collider.y * cellHeight,
            collider.width * cellWidth, collider.height * cellHeight
        };
        if (RectanglesOverlap(worldBody, worldCollider))
            return false;
    }

    if (room.handcrafted)
    {
        for (const Rectangle& fallRect : room.fallRects)
        {
            const Rectangle worldFall{
                fallRect.x * cellWidth, fallRect.y * cellHeight,
                fallRect.width * cellWidth, fallRect.height * cellHeight
            };
            if (RectanglesOverlap(worldBody, worldFall))
                return false;
        }
    }

    constexpr float kEdgeEpsilon = 0.001f;
    const int firstCol = (int)std::floor(worldBody.x / cellWidth);
    const int lastCol = (int)std::floor(
        (worldBody.x + worldBody.width - kEdgeEpsilon) / cellWidth);
    const int firstRow = (int)std::floor(worldBody.y / cellHeight);
    const int lastRow = (int)std::floor(
        (worldBody.y + worldBody.height - kEdgeEpsilon) / cellHeight);

    if (firstCol < 0 || lastCol >= RoomLayout::kCols ||
        firstRow < 0 || lastRow >= RoomLayout::kRows)
        return false;

    for (int row = firstRow; row <= lastRow; ++row)
    {
        for (int col = firstCol; col <= lastCol; ++col)
        {
            if (room.handcrafted && room.fall[row][col])
                return false;
            if (room.solid[row][col] && !RoomPlacementClearsAtDoor(
                    { (float)col, (float)row, 1.f, 1.f }, room))
                return false;

            const TileType tile = room.tiles[row][col];
            if (tile != TileType::Floor && tile != TileType::FloorVariant)
                return false;
        }
    }
    return true;
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

bool ShouldDrawGeneratedRoomFloor(int col, int row, const RoomLayout& room)
{
    (void)col;
    (void)row;
    return !room.handcrafted;
}

bool ShouldDrawGeneratedRoomTile(TileType tile, const RoomLayout& room)
{
    if (!room.handcrafted) return true;
    return tile == TileType::BossKey ||
           tile == TileType::ChestClosed ||
           tile == TileType::ChestOpen;
}

bool RoomVisualClearedByOpenDoor(const RoomTilePlacement& visual, const RoomLayout& room)
{
    // Ground stays no matter what — an opened door reveals the floor beneath the
    // wall. Visual and explicit Door-layer art both clear when they overlap it.
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
        // Free-size authored collider rectangles (may be smaller than a tile).
        for (const Rectangle& c : room.colliders)
        {
            const Rectangle world{ c.x * cellWidth, c.y * cellHeight,
                                   c.width * cellWidth, c.height * cellHeight };
            if (RectanglesOverlap(body, world)) return true;
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
    // A point sits inside an authored free collider rectangle.
    auto inCollider = [&](float x, float y) -> bool
    {
        for (const Rectangle& c : room.colliders)
            if (x >= c.x * cellWidth && x < (c.x + c.width) * cellWidth &&
                y >= c.y * cellHeight && y < (c.y + c.height) * cellHeight)
                return true;
        return false;
    };

    if (cellWidth > 0.f && cellHeight > 0.f)
    {
        const int desiredCol = (int)std::floor(desiredWorldPos.x / cellWidth);
        const int desiredRow = (int)std::floor(desiredWorldPos.y / cellHeight);
        if (desiredCol >= 0 && desiredCol < RoomLayout::kCols &&
            desiredRow >= 0 && desiredRow < RoomLayout::kRows &&
            !IsRoomFallPoint(room, desiredWorldPos, cellWidth, cellHeight) &&
            (!room.solid[desiredRow][desiredCol] ||
             RoomPlacementClearsAtDoor({(float)desiredCol,(float)desiredRow,1.f,1.f},room)) &&
            !IsSolidRoomTile(room.tiles[desiredRow][desiredCol]) &&
            !inCollider(desiredWorldPos.x, desiredWorldPos.y))
            return desiredWorldPos;
    }

    Vector2 best = desiredWorldPos;
    float bestDistanceSq = std::numeric_limits<float>::max();
    for (int row = 0; row < RoomLayout::kRows; ++row)
    {
        for (int col = 0; col < RoomLayout::kCols; ++col)
        {
            if ((room.solid[row][col] &&
                 !RoomPlacementClearsAtDoor({(float)col,(float)row,1.f,1.f},room)) ||
                IsSolidRoomTile(room.tiles[row][col])) continue;
            const Vector2 candidate{ (col + 0.5f) * cellWidth,
                                     (row + 0.5f) * cellHeight };
            if (IsRoomFallPoint(room, candidate, cellWidth, cellHeight)) continue;
            if (inCollider(candidate.x, candidate.y)) continue;
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
