#pragma once

// =============================================================================
// VfxPixel.h - pixel-art versions of raylib's smooth shapes, for effects.
//
// raylib's DrawCircle/DrawRing/DrawLineEx draw thin, smooth vector shapes that
// clash with the sprites' chunky pixels. These draw the same shapes on a grid
// of Balance::Vfx::kFxPixelSize squares (one art pixel at sprite scale).
// Discs and rings are built from horizontal spans and lines from per-step
// strips, so a translucent shape never overlaps itself and stays even.
// `glow` (0..1) also records the shape in the glow layer (VfxGlow.h) at that
// strength. Aura() replaces a faked glow (a big see-through disc) with real
// light.
// =============================================================================

#include "raylib.h"
#include "GameBalance.h"
#include "VfxGlow.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace Px
{
    inline int Grid() { return Balance::Vfx::kFxPixelSize; }

    // The grid line at or below v.
    inline int Snap(float v)
    {
        const int p = Grid();
        return (int)floorf(v / p) * p;
    }

    // `color` with its alpha scaled by `strength`, for the glow layer.
    inline Color GlowColor(Color color, float strength)
    {
        return Fade(color, std::min(1.f, (color.a / 255.f) * strength));
    }

    // Half-width, in grid cells, of a circle of `radius` along the grid row whose
    // middle lies `rowMiddle` px from the circle's centre (0 = the row misses).
    inline int HalfSpanCells(float radius, float rowMiddle)
    {
        const float h2 = radius * radius - rowMiddle * rowMiddle;
        if (h2 <= 0.f) return 0;
        return (int)floorf(sqrtf(h2) / Grid() + 0.5f);
    }

    // Filled disc (DrawCircleV). Anything smaller than a grid cell is one cell.
    inline void Disc(Vector2 centre, float radius, Color color, float glow = 0.f)
    {
        if (radius <= 0.f || color.a == 0) return;
        const int p = Grid();
        if (radius < p)
            DrawRectangle(Snap(centre.x), Snap(centre.y), p, p, color);
        else
        {
            const int cx = Snap(centre.x + p * 0.5f), cy = Snap(centre.y + p * 0.5f);
            const int rows = (int)ceilf(radius / p);
            for (int j = -rows; j < rows; ++j)
            {
                const int half = HalfSpanCells(radius, (j + 0.5f) * p);
                if (half > 0) DrawRectangle(cx - half * p, cy + j * p, 2 * half * p, p, color);
            }
        }
        if (glow > 0.f) Vfx::Glow().AddCircle(centre, std::max(radius, (float)p), GlowColor(color, glow));
    }
    inline void Disc(float x, float y, float radius, Color color, float glow = 0.f)
    {
        Disc(Vector2{ x, y }, radius, color, glow);
    }

    // Ring between two radii (DrawRing over a full turn). Each row's inner edge
    // reaches at least to the edge of the row beyond it, so thin rings stay
    // connected where they run sideways (the top and bottom caps).
    inline void Ring(Vector2 centre, float innerRadius, float outerRadius, Color color, float glow = 0.f)
    {
        if (outerRadius <= 0.f || color.a == 0) return;
        const int p = Grid();
        const int cx = Snap(centre.x + p * 0.5f), cy = Snap(centre.y + p * 0.5f);
        const int rows = (int)ceilf(outerRadius / p);
        auto outerAt = [&](int j) { return (j < -rows || j >= rows) ? 0 : HalfSpanCells(outerRadius, (j + 0.5f) * p); };
        for (int j = -rows; j < rows; ++j)
        {
            const int outer = outerAt(j);
            if (outer <= 0) continue;
            const int beyond = outerAt(j < 0 ? j - 1 : j + 1);   // the row further from the centre
            const int inner = std::min({ HalfSpanCells(std::max(0.f, innerRadius), (j + 0.5f) * p), beyond, outer - 1 });
            const int y = cy + j * p;
            if (inner <= 0)
                DrawRectangle(cx - outer * p, y, 2 * outer * p, p, color);
            else
            {
                DrawRectangle(cx - outer * p, y, (outer - inner) * p, p, color);
                DrawRectangle(cx + inner * p, y, (outer - inner) * p, p, color);
            }
        }
        if (glow > 0.f)
            Vfx::Glow().AddRing(centre, std::max(0.f, innerRadius), std::max(outerRadius, innerRadius + p), GlowColor(color, glow));
    }

    // One-pixel outline (DrawCircleLines).
    inline void Circle(Vector2 centre, float radius, Color color, float glow = 0.f)
    {
        Ring(centre, radius - Grid(), radius, color, glow);
    }
    inline void Circle(float x, float y, float radius, Color color, float glow = 0.f)
    {
        Circle(Vector2{ x, y }, radius, color, glow);
    }

    // Thick line (DrawLineEx): a Bresenham walk over grid cells, thickened
    // across the line one strip per step.
    inline void Line(Vector2 from, Vector2 to, float thickness, Color color, float glow = 0.f)
    {
        if (color.a == 0) return;
        const int p = Grid();
        const int cells = std::max(1, (int)(thickness / p + 0.5f));
        const int off = cells / 2;
        int x = (int)floorf(from.x / p), y = (int)floorf(from.y / p);
        const int x1 = (int)floorf(to.x / p), y1 = (int)floorf(to.y / p);
        const int dx = std::abs(x1 - x), dy = -std::abs(y1 - y);
        const int sx = x < x1 ? 1 : -1, sy = y < y1 ? 1 : -1;
        const bool steep = -dy > dx;   // mostly vertical: thicken sideways
        int err = dx + dy;
        while (true)
        {
            if (steep) DrawRectangle((x - off) * p, y * p, cells * p, p, color);
            else       DrawRectangle(x * p, (y - off) * p, p, cells * p, color);
            if (x == x1 && y == y1) break;
            const int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x += sx; }
            if (e2 <= dx) { err += dx; y += sy; }
        }
        if (glow > 0.f) Vfx::Glow().AddLine(from, to, std::max(thickness, (float)p), GlowColor(color, glow));
    }

    // A faked glow (a big see-through disc) as real light: a soft halo in the
    // glow layer, its strength the disc's alpha boosted to read as light. With
    // glow off it falls back to the disc, in pixels.
    inline void Aura(Vector2 centre, float radius, Color color)
    {
        if (Vfx::Glow().Active())
            Vfx::Glow().AddHalo(centre, radius, GlowColor(color, Balance::Vfx::kAuraLightBoost));
        else
            Disc(centre, radius, color);
    }
    inline void Aura(float x, float y, float radius, Color color)
    {
        Aura(Vector2{ x, y }, radius, color);
    }
}
