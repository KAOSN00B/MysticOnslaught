#pragma once
#include "raylib.h"
#include <algorithm>
#include <cmath>

struct Capsule2D
{
    Vector2 center;
    float   halfHeight; // half of the straight middle section; 0 = pure circle
    float   radius;     // radius of the rounded end caps (= half the total width)
};

// Returns the MTV that pushes capsule A out of capsule B.
// Both capsules are assumed to be vertically oriented.
inline bool CheckCapsuleCapsule(const Capsule2D& a, const Capsule2D& b, Vector2& outMTV)
{
    const float sumR  = a.radius + b.radius;
    const float ayTop = a.center.y - a.halfHeight;
    const float ayBot = a.center.y + a.halfHeight;
    const float byTop = b.center.y - b.halfHeight;
    const float byBot = b.center.y + b.halfHeight;

    Vector2 pA, pB;
    const float overlapTop = std::max(ayTop, byTop);
    const float overlapBot = std::min(ayBot, byBot);

    if (overlapTop <= overlapBot)
    {
        // Segments overlap vertically — closest points share a y
        const float midY = (overlapTop + overlapBot) * 0.5f;
        pA = { a.center.x, midY };
        pB = { b.center.x, midY };
    }
    else
    {
        // No vertical overlap — find the nearest endpoint pair
        const float dxAB = b.center.x - a.center.x;
        const float d1sq  = dxAB*dxAB + (byTop - ayBot)*(byTop - ayBot);
        const float d2sq  = dxAB*dxAB + (byBot - ayTop)*(byBot - ayTop);
        if (d1sq <= d2sq) { pA = { a.center.x, ayBot }; pB = { b.center.x, byTop }; }
        else              { pA = { a.center.x, ayTop }; pB = { b.center.x, byBot }; }
    }

    const float dx     = pA.x - pB.x;
    const float dy     = pA.y - pB.y;
    const float distSq = dx*dx + dy*dy;
    if (distSq >= sumR * sumR) return false;

    const float dist = sqrtf(distSq);
    if (dist < 0.001f) { outMTV = { sumR, 0.f }; return true; }
    const float inv = (sumR - dist) / dist;
    outMTV = { dx * inv, dy * inv };
    return true;
}

// Returns the MTV that pushes the capsule out of the rectangle.
// Capsule is assumed to be vertically oriented.
inline bool CheckCapsuleRect(const Capsule2D& cap, const Rectangle& rect, Vector2& outMTV)
{
    const float sx    = cap.center.x;
    const float syTop = cap.center.y - cap.halfHeight;
    const float syBot = cap.center.y + cap.halfHeight;
    const float rr    = rect.x + rect.width;
    const float rb    = rect.y + rect.height;

    // Closest x on rect to spine
    const float cpRx = std::max(rect.x, std::min(sx, rr));

    // Closest pair of y-values between spine [syTop,syBot] and rect [rect.y,rb]
    float segPtY, rectPtY;
    if (syBot < rect.y)     { segPtY = syBot;  rectPtY = rect.y; }
    else if (syTop > rb)    { segPtY = syTop;  rectPtY = rb;     }
    else
    {
        const float midOvlp = (std::max(syTop, rect.y) + std::min(syBot, rb)) * 0.5f;
        segPtY = rectPtY = midOvlp;
    }

    const float dx     = sx      - cpRx;
    const float dy     = segPtY  - rectPtY;
    const float distSq = dx*dx + dy*dy;
    if (distSq >= cap.radius * cap.radius) return false;

    const float dist = sqrtf(distSq);
    if (dist < 0.001f)
    {
        const float pushL = sx - rect.x + cap.radius;
        const float pushR = rr - sx     + cap.radius;
        outMTV = (pushL < pushR) ? Vector2{ -pushL, 0.f } : Vector2{ pushR, 0.f };
        return true;
    }
    const float inv = (cap.radius - dist) / dist;
    outMTV = { dx * inv, dy * inv };
    return true;
}

// Draw a capsule outline at the given SCREEN-SPACE centre.
inline void DrawCapsule2DLines(Vector2 screenCenter, float halfHeight, float radius, Color color)
{
    const int cx = (int)screenCenter.x;
    const int cy = (int)screenCenter.y;
    const int r  = (int)radius;
    const int hh = (int)halfHeight;
    if (hh > 0)
    {
        DrawLine(cx - r, cy - hh, cx - r, cy + hh, color);
        DrawLine(cx + r, cy - hh, cx + r, cy + hh, color);
        DrawCircleLines(cx, cy - hh, (float)r, color);
        DrawCircleLines(cx, cy + hh, (float)r, color);
    }
    else
    {
        DrawCircleLines(cx, cy, (float)r, color);
    }
}
