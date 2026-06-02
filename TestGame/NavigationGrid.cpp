#include "NavigationGrid.h"
#include "raymath.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <queue>

// ── Internal helpers (file-local) ─────────────────────────────────────────────

static int NavIndex(int col, int row, int cols)
{
    return row * cols + col;
}

static bool NavCellBlocked(const std::vector<bool>& blocked,
                           int cols, int rows, int col, int row)
{
    if (col < 0 || col >= cols || row < 0 || row >= rows)
        return true;
    return blocked[NavIndex(col, row, cols)];
}

static bool NavFindNearestOpen(const std::vector<bool>& blocked,
                               int cols, int rows, int& col, int& row)
{
    if (!NavCellBlocked(blocked, cols, rows, col, row))
        return true;

    for (int radius = 1; radius < std::max(cols, rows); ++radius)
    {
        for (int dc = -radius; dc <= radius; ++dc)
        {
            for (int dr = -radius; dr <= radius; ++dr)
            {
                if (std::abs(dc) != radius && std::abs(dr) != radius)
                    continue;
                int nc = col + dc, nr = row + dr;
                if (!NavCellBlocked(blocked, cols, rows, nc, nr))
                {
                    col = nc; row = nr;
                    return true;
                }
            }
        }
    }
    return false;
}

static int NavClosestOpenIndex(const std::vector<bool>& blocked,
                               int cols, int rows, int col, int row)
{
    int nc = std::max(0, std::min(col, cols - 1));
    int nr = std::max(0, std::min(row, rows - 1));
    if (!NavFindNearestOpen(blocked, cols, rows, nc, nr))
        return -1;
    return NavIndex(nc, nr, cols);
}

// ── NavigationGrid::BuildDistanceField (runs on worker thread) ────────────────

NavigationGrid::RefreshResult NavigationGrid::BuildDistanceField(
    const std::vector<bool>& blocked, int cols, int rows,
    float cellSize, Vector2 playerFeet)
{
    RefreshResult result{};
    if (cols <= 0 || rows <= 0)
        return result;

    result.distanceField.assign(cols * rows, std::numeric_limits<int>::max());

    int playerCol = std::max(0, std::min((int)(playerFeet.x / cellSize), cols - 1));
    int playerRow = std::max(0, std::min((int)(playerFeet.y / cellSize), rows - 1));
    result.playerNavIndex = NavClosestOpenIndex(blocked, cols, rows, playerCol, playerRow);
    if (result.playerNavIndex < 0)
        return result;

    using QueueItem = std::pair<int, int>;
    std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> frontier;
    result.distanceField[result.playerNavIndex] = 0;
    frontier.push({ 0, result.playerNavIndex });

    struct SearchOffset { int col, row, cost; };
    static const std::array<SearchOffset, 8> offsets{{
        { 1, 0,10},{ -1, 0,10},{ 0, 1,10},{ 0,-1,10},
        { 1, 1,14},{ 1,-1,14},{-1, 1,14},{-1,-1,14}
    }};

    while (!frontier.empty())
    {
        int cost  = frontier.top().first;
        int cur   = frontier.top().second;
        frontier.pop();

        if (cost > result.distanceField[cur])
            continue;

        int curCol = cur % cols;
        int curRow = cur / cols;

        for (const SearchOffset& off : offsets)
        {
            int nc = curCol + off.col;
            int nr = curRow + off.row;
            if (NavCellBlocked(blocked, cols, rows, nc, nr))
                continue;
            // Block diagonal moves through tight corners
            if (off.col != 0 && off.row != 0)
            {
                if (NavCellBlocked(blocked, cols, rows, curCol + off.col, curRow) ||
                    NavCellBlocked(blocked, cols, rows, curCol, curRow + off.row))
                    continue;
            }
            int nIdx     = NavIndex(nc, nr, cols);
            int nextCost = cost + off.cost;
            if (nextCost >= result.distanceField[nIdx])
                continue;
            result.distanceField[nIdx] = nextCost;
            frontier.push({ nextCost, nIdx });
        }
    }

    return result;
}

// ── Setup ─────────────────────────────────────────────────────────────────────

void NavigationGrid::Rebuild(float mapWorldW, float mapWorldH,
                             const std::vector<Rectangle>& propRects)
{
    _cols = std::max(1, (int)std::ceil(mapWorldW / _cellSize));
    _rows = std::max(1, (int)std::ceil(mapWorldH / _cellSize));
    _blocked.assign(_cols * _rows, false);
    _distance.assign(_cols * _rows, std::numeric_limits<int>::max());

    const float pad = _cellSize * 0.5f;

    for (int row = 0; row < _rows; ++row)
    {
        for (int col = 0; col < _cols; ++col)
        {
            Rectangle cell{
                col * _cellSize, row * _cellSize,
                _cellSize,       _cellSize
            };
            for (const Rectangle& rec : propRects)
            {
                Rectangle inflated{ rec.x - pad, rec.y - pad,
                                    rec.width + pad * 2.f, rec.height + pad * 2.f };
                if (CheckCollisionRecs(cell, inflated))
                {
                    _blocked[GetIndex(col, row)] = true;
                    break;
                }
            }
        }
    }
}

void NavigationGrid::CancelAndReset()
{
    if (_refreshJob.valid())
        _refreshJob.wait();
    _refreshInFlight    = false;
    _lastPlayerNavIndex = -1;
    _refreshTimer       = 0.f;
}

// ── Per-frame ─────────────────────────────────────────────────────────────────

void NavigationGrid::TickRefresh(float dt, Vector2 playerFeetWorldPos)
{
    if (_cols <= 0 || _rows <= 0)
        return;

    _refreshTimer -= dt;

    int playerCol = std::max(0, std::min((int)(playerFeetWorldPos.x / _cellSize), _cols - 1));
    int playerRow = std::max(0, std::min((int)(playerFeetWorldPos.y / _cellSize), _rows - 1));
    int playerIdx = GetClosestOpenIndex(playerCol, playerRow);

    if (!_refreshInFlight &&
        (_refreshTimer <= 0.f || playerIdx != _lastPlayerNavIndex))
    {
        std::vector<bool> blockedCopy = _blocked;
        int   cols     = _cols;
        int   rows     = _rows;
        float cellSize = _cellSize;

#ifdef __EMSCRIPTEN__
        RefreshResult result = BuildDistanceField(blockedCopy, cols, rows, cellSize, playerFeetWorldPos);
        _distance           = std::move(result.distanceField);
        _lastPlayerNavIndex = result.playerNavIndex;
#else
        _refreshJob = std::async(std::launch::async,
            [blockedCopy = std::move(blockedCopy), cols, rows, cellSize, playerFeetWorldPos]() mutable
            {
                return BuildDistanceField(blockedCopy, cols, rows, cellSize, playerFeetWorldPos);
            });
        _refreshInFlight = true;
#endif
        _refreshTimer = _refreshInterval;
    }
}

void NavigationGrid::ApplyPendingRefresh()
{
#ifdef __EMSCRIPTEN__
    return;
#else
    if (!_refreshInFlight || !_refreshJob.valid())
        return;
    if (_refreshJob.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;

    RefreshResult result = _refreshJob.get();
    _distance           = std::move(result.distanceField);
    _lastPlayerNavIndex = result.playerNavIndex;
    _refreshInFlight    = false;
#endif
}

void NavigationGrid::RefreshSync(Vector2 playerFeetWorldPos)
{
    if (_cols <= 0 || _rows <= 0)
        return;

    // Complete any in-flight job before overwriting
    if (_refreshJob.valid())
        _refreshJob.wait();
    _refreshInFlight = false;

    RefreshResult result = BuildDistanceField(_blocked, _cols, _rows, _cellSize, playerFeetWorldPos);
    _distance           = std::move(result.distanceField);
    _lastPlayerNavIndex = result.playerNavIndex;
}

// ── Navigation queries ────────────────────────────────────────────────────────

bool NavigationGrid::IsCellBlocked(int col, int row) const
{
    if (col < 0 || col >= _cols || row < 0 || row >= _rows)
        return true;
    return _blocked[GetIndex(col, row)];
}

bool NavigationGrid::FindNearestOpenCell(int& col, int& row) const
{
    if (!IsCellBlocked(col, row))
        return true;
    for (int radius = 1; radius < std::max(_cols, _rows); ++radius)
    {
        for (int dc = -radius; dc <= radius; ++dc)
        {
            for (int dr = -radius; dr <= radius; ++dr)
            {
                if (std::abs(dc) != radius && std::abs(dr) != radius)
                    continue;
                int nc = col + dc, nr = row + dr;
                if (!IsCellBlocked(nc, nr))
                {
                    col = nc; row = nr;
                    return true;
                }
            }
        }
    }
    return false;
}

int NavigationGrid::GetClosestOpenIndex(int col, int row) const
{
    int nc = std::max(0, std::min(col, std::max(0, _cols - 1)));
    int nr = std::max(0, std::min(row, std::max(0, _rows - 1)));
    if (!FindNearestOpenCell(nc, nr))
        return -1;
    return GetIndex(nc, nr);
}

bool NavigationGrid::HasLineOfSight(Vector2 start, Vector2 end) const
{
    Vector2 dir  = Vector2Subtract(end, start);
    float   dist = Vector2Length(dir);
    if (dist < 0.01f)
        return true;

    dir = Vector2Normalize(dir);
    Vector2 perp{ -dir.y, dir.x };
    const float tubeHalfWidth = _cellSize * 0.4f;
    const float step          = _cellSize * 0.5f;

    for (float t = 0.f; t < dist; t += step)
    {
        Vector2 center = Vector2Add(start, Vector2Scale(dir, t));
        for (float side : { 0.f, tubeHalfWidth, -tubeHalfWidth })
        {
            Vector2 point = Vector2Add(center, Vector2Scale(perp, side));
            int col = (int)(point.x / _cellSize);
            int row = (int)(point.y / _cellSize);
            if (IsCellBlocked(col, row))
                return false;
        }
    }
    return true;
}

Vector2 NavigationGrid::GetTarget(Vector2 startWorldPos, Vector2 targetWorldPos) const
{
    if (_cols <= 0 || _rows <= 0)
        return targetWorldPos;

    int startCol = std::max(0, std::min((int)(startWorldPos.x / _cellSize), _cols - 1));
    int startRow = std::max(0, std::min((int)(startWorldPos.y / _cellSize), _rows - 1));

    if (!FindNearestOpenCell(startCol, startRow))
        return targetWorldPos;

    int startIndex = GetIndex(startCol, startRow);
    int bestIndex  = startIndex;
    int bestCost   = _distance[startIndex];

    static const int dc[] = { 1,-1, 0, 0, 1, 1,-1,-1 };
    static const int dr[] = { 0, 0, 1,-1, 1,-1, 1,-1 };

    for (int i = 0; i < 8; ++i)
    {
        int nc = startCol + dc[i];
        int nr = startRow + dr[i];
        if (IsCellBlocked(nc, nr))
            continue;
        if (i >= 4 && (IsCellBlocked(startCol + dc[i], startRow) ||
                       IsCellBlocked(startCol, startRow + dr[i])))
            continue;
        int nextIndex = GetIndex(nc, nr);
        int nextCost  = _distance[nextIndex];
        if (nextCost < bestCost)
        {
            bestCost  = nextCost;
            bestIndex = nextIndex;
        }
    }

    int nextCol = bestIndex % _cols;
    int nextRow = bestIndex / _cols;
    return {
        nextCol * _cellSize + _cellSize * 0.5f,
        nextRow * _cellSize + _cellSize * 0.5f
    };
}

bool NavigationGrid::HasReachablePath(Vector2 from) const
{
    if (_cols <= 0 || _rows <= 0)
        return false;

    // Convert world position to the nearest open grid cell.
    int col = std::max(0, std::min((int)(from.x / _cellSize), _cols - 1));
    int row = std::max(0, std::min((int)(from.y / _cellSize), _rows - 1));
    if (!FindNearestOpenCell(col, row))
        return false;

    // A finite cost means the BFS wave reached this cell — there IS a path.
    return _distance[GetIndex(col, row)] < std::numeric_limits<int>::max();
}

std::vector<Vector2> NavigationGrid::GetWaypointPath(Vector2 from, Vector2 to, int maxSteps) const
{
    std::vector<Vector2> waypoints;
    if (_cols <= 0 || _rows <= 0 || maxSteps <= 0)
        return waypoints;

    // Start at the nearest open cell to the enemy's world position.
    int col = std::max(0, std::min((int)(from.x / _cellSize), _cols - 1));
    int row = std::max(0, std::min((int)(from.y / _cellSize), _rows - 1));
    if (!FindNearestOpenCell(col, row))
        return waypoints;

    int goalCol = std::max(0, std::min((int)(to.x / _cellSize), _cols - 1));
    int goalRow = std::max(0, std::min((int)(to.y / _cellSize), _rows - 1));

    static const int dc[] = { 1,-1, 0, 0, 1, 1,-1,-1 };
    static const int dr[] = { 0, 0, 1,-1, 1,-1, 1,-1 };

    // Walk the gradient of the distance field step by step, collecting
    // cell centres as waypoints. This is O(path_length) and reuses the
    // already-computed BFS result — no second A* search needed.
    for (int step = 0; step < maxSteps; ++step)
    {
        // Record the centre of the current cell as a waypoint.
        waypoints.push_back({
            col * _cellSize + _cellSize * 0.5f,
            row * _cellSize + _cellSize * 0.5f
        });

        if (col == goalCol && row == goalRow)
            break;

        // Choose the neighbour with the lowest distance cost (steepest descent).
        int curCost  = _distance[GetIndex(col, row)];
        int bestCost = curCost;
        int bestCol  = col;
        int bestRow  = row;

        for (int i = 0; i < 8; ++i)
        {
            int nc = col + dc[i];
            int nr = row + dr[i];
            if (IsCellBlocked(nc, nr))
                continue;
            // Block diagonal moves through tight wall corners.
            if (i >= 4 && (IsCellBlocked(col + dc[i], row) ||
                           IsCellBlocked(col, row + dr[i])))
                continue;
            int nCost = _distance[GetIndex(nc, nr)];
            if (nCost < bestCost)
            {
                bestCost = nCost;
                bestCol  = nc;
                bestRow  = nr;
            }
        }

        // If no cheaper neighbour exists the gradient has ended — stop here.
        if (bestCol == col && bestRow == row)
            break;

        col = bestCol;
        row = bestRow;
    }

    return waypoints;
}

Vector2 NavigationGrid::GetAStarTarget(Vector2 startWorldPos, Vector2 targetWorldPos) const
{
    if (_cols <= 0 || _rows <= 0)
        return targetWorldPos;

    int startCol = std::max(0, std::min((int)(startWorldPos.x  / _cellSize), _cols - 1));
    int startRow = std::max(0, std::min((int)(startWorldPos.y  / _cellSize), _rows - 1));
    int goalCol  = std::max(0, std::min((int)(targetWorldPos.x / _cellSize), _cols - 1));
    int goalRow  = std::max(0, std::min((int)(targetWorldPos.y / _cellSize), _rows - 1));

    if (!FindNearestOpenCell(startCol, startRow))
        return targetWorldPos;
    FindNearestOpenCell(goalCol, goalRow);

    int startIdx = GetIndex(startCol, startRow);
    int goalIdx  = GetIndex(goalCol,  goalRow);

    if (startIdx == goalIdx)
        return targetWorldPos;

    const int total = _cols * _rows;
    std::vector<int>  gCost(total, std::numeric_limits<int>::max());
    std::vector<int>  cameFrom(total, -1);
    std::vector<bool> closed(total, false);

    auto heuristic = [&](int idx) {
        int c = idx % _cols, r = idx / _cols;
        return (int)(10.f * Vector2Length({ (float)(c - goalCol), (float)(r - goalRow) }));
    };

    using QueueItem = std::pair<int,int>;
    std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> frontier;
    gCost[startIdx] = 0;
    frontier.push({ heuristic(startIdx), startIdx });

    static const int dc[] = { 1,-1, 0, 0, 1, 1,-1,-1 };
    static const int dr[] = { 0, 0, 1,-1, 1,-1, 1,-1 };
    static const int stepCost[] = { 10,10,10,10, 14,14,14,14 };

    bool found = false;
    while (!frontier.empty())
    {
        int cur = frontier.top().second;
        frontier.pop();
        if (closed[cur]) continue;
        closed[cur] = true;
        if (cur == goalIdx) { found = true; break; }

        int col = cur % _cols;
        int row = cur / _cols;

        for (int i = 0; i < 8; ++i)
        {
            int nc = col + dc[i], nr = row + dr[i];
            if (nc < 0 || nc >= _cols || nr < 0 || nr >= _rows) continue;
            int nIdx = GetIndex(nc, nr);
            if (_blocked[nIdx] || closed[nIdx]) continue;
            if (i >= 4 && (IsCellBlocked(col + dc[i], row) ||
                           IsCellBlocked(col, row + dr[i])))
                continue;
            int ng = gCost[cur] + stepCost[i];
            if (ng >= gCost[nIdx]) continue;
            gCost[nIdx]    = ng;
            cameFrom[nIdx] = cur;
            frontier.push({ ng + heuristic(nIdx), nIdx });
        }
    }

    if (!found)
        return GetTarget(startWorldPos, targetWorldPos);

    // Walk back one step from goal toward start
    int step = goalIdx;
    while (cameFrom[step] != -1 && cameFrom[step] != startIdx)
        step = cameFrom[step];

    int stepCol = step % _cols;
    int stepRow = step / _cols;
    return {
        stepCol * _cellSize + _cellSize * 0.5f,
        stepRow * _cellSize + _cellSize * 0.5f
    };
}
