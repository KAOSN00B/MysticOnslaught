#include "OverlayRenderer.h"
#include "DevTools.h"   // MO_DEV_TOOLS / MO_ITCH_DEMO
#include "VirtualCanvas.h"

#include "AbilityType.h"
#include "VirtualCanvas.h"
#include "Character.h"
#include "VirtualCanvas.h"
#include "NineSlice.h"
#include "VirtualCanvas.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace
{
    void DrawScrollingCheckerboard(float sw, float sh, Color dark, Color light, float speedX, float speedY, int cell = 80)
    {
        const int period = cell * 2;
        float t = (float)GetTime();
        int offX = (int)fmodf(t * speedX, (float)period);
        int offY = (int)fmodf(t * speedY, (float)period);
        int phaseX = offX / cell;
        int phaseY = offY / cell;
        int pixX = offX % cell;
        int pixY = offY % cell;

        for (int gy = -1; gy <= (int)(sh / cell) + 1; gy++)
        {
            for (int gx = -1; gx <= (int)(sw / cell) + 1; gx++)
            {
                bool isDark = (((gx + phaseX) + (gy + phaseY)) % 2 + 2) % 2 == 0;
                DrawRectangle(gx * cell - pixX, gy * cell - pixY, cell, cell, isDark ? dark : light);
            }
        }
    }
}

void OverlayRenderer::DrawDemoEnd(const DemoEndRenderContext& ctx) const
{
    const float sw = (float)kVirtualWidth;
    const float sh = (float)kVirtualHeight;

    DrawRectangleGradientV(0, 0, (int)sw, (int)sh, Color{8, 4, 20, 255}, Color{20, 10, 40, 255});
    DrawRectangle(0, 0, (int)sw, 6, Color{120, 60, 200, 200});
    DrawRectangle(0, (int)sh - 6, (int)sw, 6, Color{120, 60, 200, 200});

    float cy = sh * 0.12f;

    const char* title = "VICTORY!";
    int titleSz = 72;
    DrawText(title, (int)(sw * 0.5f - MeasureText(title, titleSz) * 0.5f), (int)cy, titleSz, Color{255, 220, 140, 255});
    cy += titleSz + 18.f;

    const char* gameName = "Mystic Onslaught";
    int nameSz = 30;
    DrawText(gameName, (int)(sw * 0.5f - MeasureText(gameName, nameSz) * 0.5f), (int)cy, nameSz, Color{200, 170, 235, 230});
    cy += nameSz + 50.f;

    DrawLineEx({ sw * 0.25f, cy }, { sw * 0.75f, cy }, 1.f, Color{120, 60, 200, 120});
    cy += 30.f;

#if MO_ITCH_DEMO
    // The public demo ends after the Caverns boss: say so honestly.
    const char* lines[] = {
        "You cleared the Forest and the Caverns.",
        "This is the end of the Mystic Onslaught demo.",
        "The full game continues through four more domains and the Demon within.",
    };
#else
    const char* lines[] = {
        "You conquered all six domains and felled the Demon within.",
        "The Onslaught is broken - for now.",
        "Your Echoes are banked. Dive again for a greater challenge.",
    };
#endif
    int msgSz = 26;
    for (auto& line : lines)
    {
        DrawText(line, (int)(sw * 0.5f - MeasureText(line, msgSz) * 0.5f), (int)cy, msgSz, Color{200, 200, 220, 220});
        cy += msgSz + 14.f;
    }
    cy += 30.f;

    const char* stats[] = {
        TextFormat("Enemies defeated:  %d", ctx.enemiesKilled),
        TextFormat("Bosses slain:      %d", ctx.bossesDefeated),
        TextFormat("Gold carried home: %dg", ctx.playerGold),
        TextFormat("Player level:      %d", ctx.playerLevel),
    };
    int statSz = 22;
    for (auto& s : stats)
    {
        DrawText(s, (int)(sw * 0.5f - MeasureText(s, statSz) * 0.5f), (int)cy, statSz, Color{180, 220, 180, 210});
        cy += statSz + 10.f;
    }
    cy += 24.f;

    if (ctx.unlockLine != nullptr && ctx.unlockLine[0] != 0)
    {
        int unlockSz = 30;
        DrawText(ctx.unlockLine, (int)(sw * 0.5f - MeasureText(ctx.unlockLine, unlockSz) * 0.5f), (int)cy,
                 unlockSz, Color{255, 215, 120, 255});
        cy += unlockSz + 16.f;
    }
    cy += 16.f;

#if MO_DEV_TOOLS
    // Developer builds only: Shipping and the public demo have no debug menu.
    if (ctx.demoCompleted)
    {
        const char* hint = "Debug Mode unlocked - use the Debug button on the main menu, then press F1 or F10 in-game";
        int hintSz = 16;
        DrawText(hint, (int)(sw * 0.5f - MeasureText(hint, hintSz) * 0.5f), (int)cy, hintSz, Color{160, 120, 255, 180});
        cy += hintSz + 30.f;
    }
#endif

    const char* returnHint = (ctx.promptMode == InputPromptMode::Gamepad)
        ? "A: Return to the main menu"
        : (ctx.promptMode == InputPromptMode::Touch)
            ? "Tap anywhere to return to the main menu"
            : "Press any key or click anywhere to return to the main menu";
    int returnHintSz = 18;
    DrawText(returnHint, (int)(sw * 0.5f - MeasureText(returnHint, returnHintSz) * 0.5f), (int)cy, returnHintSz, Color{210, 200, 235, 190});
    cy += returnHintSz + 24.f;

    Rectangle btn = { sw * 0.5f - 160.f, sh * 0.88f, 320.f, 54.f };
    bool hov = CheckCollisionPointRec(GetVirtualMousePos(), btn);
    DrawRectangleRounded(btn, 0.4f, 8, hov ? Color{70, 30, 130, 240} : Color{40, 16, 80, 200});
    DrawRectangleRoundedLines(btn, 0.4f, 8, hov ? Color{180, 100, 255, 255} : Color{120, 60, 200, 180});
    const char* btnTxt = "Return to Main Menu";
    int btnSz = 24;
    DrawText(btnTxt,
        (int)(btn.x + btn.width * 0.5f - MeasureText(btnTxt, btnSz) * 0.5f),
        (int)(btn.y + btn.height * 0.5f - btnSz * 0.5f),
        btnSz, hov ? Color{220, 180, 255, 255} : RAYWHITE);
}
