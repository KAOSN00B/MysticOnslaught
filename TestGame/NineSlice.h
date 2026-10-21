#pragma once
#include "raylib.h"
#include <algorithm>
#include <cmath>

// ── 9-Slice (9-patch) texture drawing ────────────────────────────────────────
//
// Symmetric version: one srcCorner value controls all four edges equally.
// srcCorner — how many px from each edge of the source texture form a corner
// dstCorner — how large each corner appears on screen in pixels
// ─────────────────────────────────────────────────────────────────────────────
inline void DrawNineSlice(Texture2D tex,
                          float     srcCorner,
                          float     dstCorner,
                          Rectangle dest,
                          Color     tint)
{
    if (tex.id == 0) return;

    const float tw  = (float)tex.width;
    const float th  = (float)tex.height;
    const float sc  = srcCorner;
    const float dc  = dstCorner;
    const float smw = tw - sc * 2.f;
    const float smh = th - sc * 2.f;
    const float dmw = dest.width  - dc * 2.f;
    const float dmh = dest.height - dc * 2.f;
    const float dx  = dest.x;
    const float dy  = dest.y;

    Rectangle srcTL = { 0,       0,       sc,  sc  };
    Rectangle srcTC = { sc,      0,       smw, sc  };
    Rectangle srcTR = { tw - sc, 0,       sc,  sc  };
    Rectangle srcML = { 0,       sc,      sc,  smh };
    Rectangle srcMC = { sc,      sc,      smw, smh };
    Rectangle srcMR = { tw - sc, sc,      sc,  smh };
    Rectangle srcBL = { 0,       th - sc, sc,  sc  };
    Rectangle srcBC = { sc,      th - sc, smw, sc  };
    Rectangle srcBR = { tw - sc, th - sc, sc,  sc  };

    Rectangle dstTL = { dx,            dy,            dc,  dc  };
    Rectangle dstTC = { dx + dc,       dy,            dmw, dc  };
    Rectangle dstTR = { dx + dc + dmw, dy,            dc,  dc  };
    Rectangle dstML = { dx,            dy + dc,       dc,  dmh };
    Rectangle dstMC = { dx + dc,       dy + dc,       dmw, dmh };
    Rectangle dstMR = { dx + dc + dmw, dy + dc,       dc,  dmh };
    Rectangle dstBL = { dx,            dy + dc + dmh, dc,  dc  };
    Rectangle dstBC = { dx + dc,       dy + dc + dmh, dmw, dc  };
    Rectangle dstBR = { dx + dc + dmw, dy + dc + dmh, dc,  dc  };

    DrawTexturePro(tex, srcTL, dstTL, {}, 0.f, tint);
    DrawTexturePro(tex, srcTC, dstTC, {}, 0.f, tint);
    DrawTexturePro(tex, srcTR, dstTR, {}, 0.f, tint);
    DrawTexturePro(tex, srcML, dstML, {}, 0.f, tint);
    DrawTexturePro(tex, srcMC, dstMC, {}, 0.f, tint);
    DrawTexturePro(tex, srcMR, dstMR, {}, 0.f, tint);
    DrawTexturePro(tex, srcBL, dstBL, {}, 0.f, tint);
    DrawTexturePro(tex, srcBC, dstBC, {}, 0.f, tint);
    DrawTexturePro(tex, srcBR, dstBR, {}, 0.f, tint);
}

// ── Asymmetric version: each edge set independently ──────────────────────────
// srcTop/srcBot/srcLeft/srcRight — source corner px per edge
// dstCorner — destination corner size in screen px (all four corners equal)
// ─────────────────────────────────────────────────────────────────────────────
inline void DrawNineSliceEx(Texture2D tex,
                            float     srcTop,  float srcBot,
                            float     srcLeft, float srcRight,
                            float     dstCorner,
                            Rectangle dest,
                            Color     tint)
{
    if (tex.id == 0) return;

    const float tw  = (float)tex.width;
    const float th  = (float)tex.height;
    const float sT  = srcTop,  sB = srcBot;
    const float sL  = srcLeft, sR = srcRight;
    const float smw = tw - sL - sR;
    const float smh = th - sT - sB;
    const float dc  = dstCorner;
    const float dmw = dest.width  - dc * 2.f;
    const float dmh = dest.height - dc * 2.f;
    const float dx  = dest.x;
    const float dy  = dest.y;

    Rectangle srcTL = { 0,       0,       sL,  sT  };
    Rectangle srcTC = { sL,      0,       smw, sT  };
    Rectangle srcTR = { tw - sR, 0,       sR,  sT  };
    Rectangle srcML = { 0,       sT,      sL,  smh };
    Rectangle srcMC = { sL,      sT,      smw, smh };
    Rectangle srcMR = { tw - sR, sT,      sR,  smh };
    Rectangle srcBL = { 0,       th - sB, sL,  sB  };
    Rectangle srcBC = { sL,      th - sB, smw, sB  };
    Rectangle srcBR = { tw - sR, th - sB, sR,  sB  };

    Rectangle dstTL = { dx,            dy,            dc,  dc  };
    Rectangle dstTC = { dx + dc,       dy,            dmw, dc  };
    Rectangle dstTR = { dx + dc + dmw, dy,            dc,  dc  };
    Rectangle dstML = { dx,            dy + dc,       dc,  dmh };
    Rectangle dstMC = { dx + dc,       dy + dc,       dmw, dmh };
    Rectangle dstMR = { dx + dc + dmw, dy + dc,       dc,  dmh };
    Rectangle dstBL = { dx,            dy + dc + dmh, dc,  dc  };
    Rectangle dstBC = { dx + dc,       dy + dc + dmh, dmw, dc  };
    Rectangle dstBR = { dx + dc + dmw, dy + dc + dmh, dc,  dc  };

    DrawTexturePro(tex, srcTL, dstTL, {}, 0.f, tint);
    DrawTexturePro(tex, srcTC, dstTC, {}, 0.f, tint);
    DrawTexturePro(tex, srcTR, dstTR, {}, 0.f, tint);
    DrawTexturePro(tex, srcML, dstML, {}, 0.f, tint);
    DrawTexturePro(tex, srcMC, dstMC, {}, 0.f, tint);
    DrawTexturePro(tex, srcMR, dstMR, {}, 0.f, tint);
    DrawTexturePro(tex, srcBL, dstBL, {}, 0.f, tint);
    DrawTexturePro(tex, srcBC, dstBC, {}, 0.f, tint);
    DrawTexturePro(tex, srcBR, dstBR, {}, 0.f, tint);
}

// ── Whole-pixel nine-slice (pixel art) ───────────────────────────────────────
// cuts  — source pixels kept on each edge (whole numbers, from the 9-slice editor)
// scale — one whole-number multiplier for every piece, so the pixels stay square
// dest  — its edges snap to multiples of scale, so no piece lands on a half pixel
// ─────────────────────────────────────────────────────────────────────────────
struct NineSliceCuts
{
    int top;
    int bottom;
    int left;
    int right;
};

inline void DrawNineSliceScaled(Texture2D tex, NineSliceCuts cuts, int scale, Rectangle dest, Color tint)
{
    if (tex.id == 0 || scale <= 0) return;

    const float s  = (float)scale;
    const float tw = (float)tex.width;
    const float th = (float)tex.height;
    const float sT = (float)cuts.top,  sB = (float)cuts.bottom;
    const float sL = (float)cuts.left, sR = (float)cuts.right;

    // Snap the destination's edges to whole multiples of the scale.
    const float left   = std::round(dest.x / s) * s;
    const float top    = std::round(dest.y / s) * s;
    const float right  = std::round((dest.x + dest.width)  / s) * s;
    const float bottom = std::round((dest.y + dest.height) / s) * s;

    const float dL  = sL * s, dR = sR * s;
    const float dT  = sT * s, dB = sB * s;
    const float dmw = std::max(0.f, (right - left) - dL - dR);
    const float dmh = std::max(0.f, (bottom - top) - dT - dB);

    // Index 0 = start edge, 1 = middle, 2 = end edge.
    const float srcX[3] = { 0.f,  sL,  tw - sR };
    const float srcW[3] = { sL,   tw - sL - sR, sR };
    const float srcY[3] = { 0.f,  sT,  th - sB };
    const float srcH[3] = { sT,   th - sT - sB, sB };
    const float dstX[3] = { left, left + dL, left + dL + dmw };
    const float dstW[3] = { dL,   dmw, dR };
    const float dstY[3] = { top,  top + dT, top + dT + dmh };
    const float dstH[3] = { dT,   dmh, dB };

    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col)
            DrawTexturePro(tex,
                           { srcX[col], srcY[row], srcW[col], srcH[row] },
                           { dstX[col], dstY[row], dstW[col], dstH[row] },
                           {}, 0.f, tint);
}
