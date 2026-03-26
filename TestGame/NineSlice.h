#pragma once
#include "raylib.h"

// ── 9-Slice (9-patch) texture drawing ────────────────────────────────────────
//
// Divides the source texture into a 3x3 grid using srcCorner pixels from
// each edge. The four corners are drawn at dstCorner px (never stretched),
// the four edges stretch along one axis only, and the centre stretches freely.
// This keeps corner art crisp at any destination size.
//
// Usage:
//   DrawNineSlice(myTex, 12.f, 12.f, destRect, WHITE);
//
// srcCorner — how many px from each edge of the *source* texture form a corner
// dstCorner — how large each corner appears on screen in pixels
//             (set equal to srcCorner × artScale, e.g. 12 * 4 = 48 for 4× art)
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
    const float smw = tw - sc * 2.f;          // source middle width
    const float smh = th - sc * 2.f;          // source middle height
    const float dmw = dest.width  - dc * 2.f; // dest   middle width
    const float dmh = dest.height - dc * 2.f; // dest   middle height
    const float dx  = dest.x;
    const float dy  = dest.y;

    // ── Source rects (rows: top / mid / bot, cols: left / centre / right) ─────
    Rectangle srcTL = { 0,       0,       sc,  sc  };
    Rectangle srcTC = { sc,      0,       smw, sc  };
    Rectangle srcTR = { tw - sc, 0,       sc,  sc  };
    Rectangle srcML = { 0,       sc,      sc,  smh };
    Rectangle srcMC = { sc,      sc,      smw, smh };
    Rectangle srcMR = { tw - sc, sc,      sc,  smh };
    Rectangle srcBL = { 0,       th - sc, sc,  sc  };
    Rectangle srcBC = { sc,      th - sc, smw, sc  };
    Rectangle srcBR = { tw - sc, th - sc, sc,  sc  };

    // ── Destination rects ─────────────────────────────────────────────────────
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
