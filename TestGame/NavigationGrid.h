#pragma once
#include "raylib.h"
#include <vector>
#include <future>

// ── NavigationGrid ────────────────────────────────────────────────────────────
// Self-contained A* / Dijkstra flow-field pathfinder.
// Engine owns one instance (_nav) and calls:
//   _nav.Rebuild(...)          after loading a map or changing biome
//   _nav.CancelAndReset()      before Rebuild or when resetting a run
//   _nav.TickRefresh(dt,feet)  every frame from UpdateGamePlay
//   _nav.ApplyPendingRefresh() every frame (main thread, checks async job)
//   _nav.RefreshSync(feet)     immediately after a biome swap
//   _nav.GetAStarTarget(...)   per enemy per frame
//   _nav.HasLineOfSight(...)   per enemy per frame
// ─────────────────────────────────────────────────────────────────────────────
class NavigationGrid
{
public:
    // ── Setup ────────────────────────────────────────────────────────────────
    // Builds the blocked-cell grid from prop collision rectangles.
    // mapWorldW/H = _map.width * _mapScale.
    void Rebuild(float mapWorldW, float mapWorldH,
                 const std::vector<Rectangle>& propRects);

    // Cancel any in-flight async job and reset timer / last-player-index.
    // Call before Rebuild or at run-reset.
    void CancelAndReset();

    // ── Per-frame ────────────────────────────────────────────────────────────
    // Launches an async distance-field refresh when the timer expires or the
    // player steps into a new nav cell.
    void TickRefresh(float dt, Vector2 playerFeetWorldPos);

    // Polls the async job; applies the result if it is ready.
    // Must be called on the main thread after TickRefresh.
    void ApplyPendingRefresh();

    // Blocks until the distance field is fully rebuilt.
    // Use immediately after ApplyBiome so enemies start with valid data.
    void RefreshSync(Vector2 playerFeetWorldPos);

    // ── Navigation queries (const, safe to call any frame) ───────────────────
    bool    HasLineOfSight(Vector2 start, Vector2 end) const;
    Vector2 GetTarget(Vector2 startWorldPos, Vector2 targetWorldPos) const;
    Vector2 GetAStarTarget(Vector2 startWorldPos, Vector2 targetWorldPos) const;

    // Returns true when the flow field has a finite-cost path from 'from' to
    // the player. Call this during spawn validation to reject unreachable tiles.
    bool HasReachablePath(Vector2 from) const;

    // Follows the flow-field gradient from 'from' toward 'to' and returns up
    // to maxSteps cell-centre waypoints. Enemies cache this and follow it
    // waypoint-by-waypoint instead of querying the grid every frame.
    std::vector<Vector2> GetWaypointPath(Vector2 from, Vector2 to, int maxSteps) const;

    // ── Utility ──────────────────────────────────────────────────────────────
    bool  IsCellBlocked(int col, int row) const;
    float GetCellSize() const { return _cellSize; }
    int   GetCols()     const { return _cols; }
    int   GetRows()     const { return _rows; }

private:
    // Internal result type for the async worker
    struct RefreshResult
    {
        std::vector<int> distanceField;
        int playerNavIndex = -1;
    };

    static RefreshResult BuildDistanceField(
        const std::vector<bool>& blocked, int cols, int rows,
        float cellSize, Vector2 playerFeet);

    int  GetIndex(int col, int row) const { return row * _cols + col; }
    bool FindNearestOpenCell(int& col, int& row) const;
    int  GetClosestOpenIndex(int col, int row) const;

    float _cellSize = 72.f;
    int   _cols     = 0;
    int   _rows     = 0;

    std::vector<bool> _blocked;
    std::vector<int>  _distance;

    int   _lastPlayerNavIndex = -1;
    bool  _refreshInFlight    = false;
    float _refreshTimer       = 0.f;

    static constexpr float _refreshInterval = 0.2f;

    std::future<RefreshResult> _refreshJob;
};
