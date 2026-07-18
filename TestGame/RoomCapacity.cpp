#include "RoomCapacity.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <queue>
#include <vector>

namespace
{
    constexpr int kTilePixels = 16;

    bool IsBlockingTile(TileType tile)
    {
        switch (tile)
        {
        case TileType::WallBody:
        case TileType::WallTopFace:
        case TileType::WallCornerTL:
        case TileType::WallCornerTR:
        case TileType::WallInnerCornerL:
        case TileType::WallInnerCornerR:
        case TileType::Void:
        case TileType::DoorLocked:
        case TileType::WallLeft:
        case TileType::WallRight:
        case TileType::WallBottom:
        case TileType::WallCornerBL:
        case TileType::WallCornerBR:
        case TileType::WallInnerCornerBL:
        case TileType::WallInnerCornerBR:
            return true;
        default:
            return false;
        }
    }

    bool Overlaps(Rectangle a, Rectangle b)
    {
        return a.x < b.x + b.width && a.x + a.width > b.x &&
               a.y < b.y + b.height && a.y + a.height > b.y;
    }

    void BlockRectangle(std::array<bool, RoomLayout::kRows * RoomLayout::kCols>& blocked,
                        Rectangle rectangle)
    {
        for (int row = 0; row < RoomLayout::kRows; ++row)
        {
            for (int col = 0; col < RoomLayout::kCols; ++col)
            {
                if (Overlaps(rectangle, { (float)col, (float)row, 1.f, 1.f }))
                    blocked[row * RoomLayout::kCols + col] = true;
            }
        }
    }

    template <typename Definition>
    void BlockPlacement(std::array<bool, RoomLayout::kRows * RoomLayout::kCols>& blocked,
                        const SpritePlacement& placement, const Definition& definition)
    {
        const Rectangle collision = definition.collision;
        if (collision.width <= 0.f || collision.height <= 0.f) return;
        BlockRectangle(blocked, {
            placement.col + collision.x / kTilePixels,
            placement.row + collision.y / kTilePixels,
            collision.width / kTilePixels,
            collision.height / kTilePixels
        });
    }

    RoomCombatCapacity CapacityForBand(RoomCapacityBand band)
    {
        RoomCombatCapacity result{};
        result.band = band;
        switch (band)
        {
        case RoomCapacityBand::Small:
            result.openingBodyCap = 4; result.totalBodyCap = 7;
            result.specialistCap = 1; result.pressureCap = 7;
            break;
        case RoomCapacityBand::Medium:
            result.openingBodyCap = 6; result.totalBodyCap = 10;
            result.specialistCap = 2; result.pressureCap = 10;
            break;
        case RoomCapacityBand::Large:
            result.openingBodyCap = 8; result.totalBodyCap = 13;
            result.specialistCap = 3; result.pressureCap = 13;
            break;
        case RoomCapacityBand::Arena:
            result.openingBodyCap = 10; result.totalBodyCap = 18;
            result.specialistCap = 3; result.pressureCap = 16;
            break;
        }
        return result;
    }
}

RoomCombatCapacity RoomCapacityAnalyzer::Analyze(const RoomLayout& room,
                                                 const TileDefSet& definitions)
{
    constexpr int cellCount = RoomLayout::kRows * RoomLayout::kCols;
    std::array<bool, cellCount> blocked{};
    for (int row = 0; row < RoomLayout::kRows; ++row)
    {
        for (int col = 0; col < RoomLayout::kCols; ++col)
        {
            const int index = row * RoomLayout::kCols + col;
            blocked[index] = room.solid[row][col] || room.fall[row][col] ||
                             IsBlockingTile(room.tiles[row][col]);
        }
    }
    for (Rectangle collider : room.colliders) BlockRectangle(blocked, collider);
    for (Rectangle fallRect : room.fallRects) BlockRectangle(blocked, fallRect);

    for (const SpritePlacement& placement : room.props)
    {
        const TileDefSet* source = ResolveRoomDefinitions(room, placement, definitions);
        if (source != nullptr && placement.defIdx >= 0 &&
            placement.defIdx < (int)source->props.size())
            BlockPlacement(blocked, placement, source->props[placement.defIdx]);
    }
    for (const SpritePlacement& placement : room.animProps)
    {
        const TileDefSet* source = ResolveRoomDefinitions(room, placement, definitions);
        if (source != nullptr && placement.defIdx >= 0 &&
            placement.defIdx < (int)source->animProps.size())
            BlockPlacement(blocked, placement, source->animProps[placement.defIdx]);
    }

    std::array<int, cellCount> component{};
    component.fill(-1);
    std::vector<int> componentSizes;
    const int dc[4]{ 1, -1, 0, 0 };
    const int dr[4]{ 0, 0, 1, -1 };
    int walkable = 0;
    for (bool isBlocked : blocked) if (!isBlocked) ++walkable;

    for (int start = 0; start < cellCount; ++start)
    {
        if (blocked[start] || component[start] >= 0) continue;
        const int id = (int)componentSizes.size();
        int size = 0;
        std::queue<int> frontier;
        frontier.push(start);
        component[start] = id;
        while (!frontier.empty())
        {
            const int current = frontier.front(); frontier.pop();
            ++size;
            const int row = current / RoomLayout::kCols;
            const int col = current % RoomLayout::kCols;
            for (int direction = 0; direction < 4; ++direction)
            {
                const int nr = row + dr[direction], nc = col + dc[direction];
                if (nr < 0 || nr >= RoomLayout::kRows || nc < 0 || nc >= RoomLayout::kCols)
                    continue;
                const int next = nr * RoomLayout::kCols + nc;
                if (!blocked[next] && component[next] < 0)
                {
                    component[next] = id;
                    frontier.push(next);
                }
            }
        }
        componentSizes.push_back(size);
    }

    int largestId = -1;
    int largestSize = 0;
    for (int id = 0; id < (int)componentSizes.size(); ++id)
    {
        if (componentSizes[id] > largestSize)
        {
            largestSize = componentSizes[id];
            largestId = id;
        }
    }

    int spawnTiles = 0;
    int chokeTiles = 0;
    for (int index = 0; index < cellCount; ++index)
    {
        if (component[index] != largestId) continue;
        const int row = index / RoomLayout::kCols;
        const int col = index % RoomLayout::kCols;
        int openNeighbors = 0;
        for (int direction = 0; direction < 4; ++direction)
        {
            const int nr = row + dr[direction], nc = col + dc[direction];
            if (nr >= 0 && nr < RoomLayout::kRows && nc >= 0 && nc < RoomLayout::kCols &&
                component[nr * RoomLayout::kCols + nc] == largestId)
                ++openNeighbors;
        }
        if (openNeighbors >= 3) ++spawnTiles;
        if (openNeighbors <= 2) ++chokeTiles;
    }

    RoomCapacityBand band = RoomCapacityBand::Large;
    const float chokeRatio = largestSize > 0 ? (float)chokeTiles / largestSize : 1.f;
    if (largestSize < 120 || spawnTiles < 45 || chokeRatio > 0.42f)
        band = RoomCapacityBand::Small;
    else if (largestSize < 260 || spawnTiles < 150 || chokeRatio > 0.24f)
        band = RoomCapacityBand::Medium;

    bool authoredOverride = room.combatCapacityOverride != RoomCapacityOverride::Auto;
    if (authoredOverride)
        band = static_cast<RoomCapacityBand>((int)room.combatCapacityOverride);

    RoomCombatCapacity result = CapacityForBand(band);
    result.walkableTiles = walkable;
    result.connectedTiles = largestSize;
    result.spawnTiles = spawnTiles;
    result.chokeTiles = chokeTiles;
    result.authoredOverride = authoredOverride;
    return result;
}

const char* RoomCapacityAnalyzer::BandName(RoomCapacityBand band)
{
    switch (band)
    {
    case RoomCapacityBand::Small: return "Small";
    case RoomCapacityBand::Medium: return "Medium";
    case RoomCapacityBand::Large: return "Large";
    case RoomCapacityBand::Arena: return "Arena";
    }
    return "Unknown";
}
