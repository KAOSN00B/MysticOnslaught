#pragma once
#include "raylib.h"

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
