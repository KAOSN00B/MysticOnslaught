#include "OverlayRenderer.h"
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

    const char* title = "THANKS FOR PLAYING!";
    int titleSz = 64;
    DrawText(title, (int)(sw * 0.5f - MeasureText(title, titleSz) * 0.5f), (int)cy, titleSz, Color{220, 180, 255, 255});
    cy += titleSz + 18.f;

    const char* gameName = "Mystic Onslaught  -  DEMO";
    int nameSz = 30;
    DrawText(gameName, (int)(sw * 0.5f - MeasureText(gameName, nameSz) * 0.5f), (int)cy, nameSz, Color{160, 130, 210, 220});
    cy += nameSz + 50.f;

    DrawLineEx({ sw * 0.25f, cy }, { sw * 0.75f, cy }, 1.f, Color{120, 60, 200, 120});
    cy += 30.f;

    const char* lines[] = {
        "You've completed the demo - thank you for your time!",
        "The full game is currently in development.",
        "Your feedback means the world.",
    };
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
        TextFormat("Gold collected:    %dg", ctx.playerGold),
        TextFormat("Player level:      %d", ctx.playerLevel),
    };
    int statSz = 22;
    for (auto& s : stats)
    {
        DrawText(s, (int)(sw * 0.5f - MeasureText(s, statSz) * 0.5f), (int)cy, statSz, Color{180, 220, 180, 210});
        cy += statSz + 10.f;
    }
    cy += 40.f;

    if (ctx.demoCompleted)
    {
        const char* hint = "Debug Mode unlocked - use the Debug button on the main menu, then press F1 or F10 in-game";
        int hintSz = 16;
        DrawText(hint, (int)(sw * 0.5f - MeasureText(hint, hintSz) * 0.5f), (int)cy, hintSz, Color{160, 120, 255, 180});
        cy += hintSz + 30.f;
    }

    const char* returnHint = ctx.touchModeActive
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

void OverlayRenderer::DrawExpTally(const ExpTallyRenderContext& ctx) const
{
    DrawRectangle(0, 0, kVirtualWidth, kVirtualHeight, Fade(BLACK, 0.65f));

    const float sw = (float)kVirtualWidth;
    const float sh = (float)kVirtualHeight;
    const float cx = sw * 0.5f;
    const int levelsGained = std::max(0, ctx.player->GetLevel() - ctx.tallyStartLevel);

    const char* title = (levelsGained > 0) ? "Level Up!" : "Room Cleared!";
    static constexpr int kTitleSize = 52;
    int titleW = MeasureText(title, kTitleSize);
    DrawText(title, (int)(cx - titleW * 0.5f), (int)(sh * 0.28f), kTitleSize, RAYWHITE);

    int level = ctx.player->GetLevel();
    int maxLevel = ctx.player->GetMaxLevel();
    const char* levelStr = TextFormat("Level  %d", level);
    static constexpr int kLevelSize = 38;
    int levelW = MeasureText(levelStr, kLevelSize);
    DrawText(levelStr, (int)(cx - levelW * 0.5f), (int)(sh * 0.41f), kLevelSize, Color{255, 210, 0, 255});

    if (levelsGained > 0)
    {
        auto prevInt = [levelsGained](int currentValue, int perLevelGain) -> int
        {
            return currentValue - perLevelGain * levelsGained;
        };
        auto prevFloat = [levelsGained](float currentValue, float perLevelGain) -> float
        {
            return currentValue - perLevelGain * (float)levelsGained;
        };

        std::string hpLine   = "HP  " + std::to_string(prevInt((int)ctx.player->GetMaxHealthValue(), Character::kLevelHpGain))
            + " -> " + std::to_string((int)ctx.player->GetMaxHealthValue());
        std::string atkLine  = "ATK  " + std::to_string((int)std::ceil(prevFloat(ctx.player->GetAttackPowerValue(), Character::kLevelAttackGain)))
            + " -> " + std::to_string((int)std::ceil(ctx.player->GetAttackPowerValue()));
        std::string manaLine = "MP  " + std::to_string(prevInt(ctx.player->GetMaxMana(), Character::kLevelManaGain))
            + " -> " + std::to_string(ctx.player->GetMaxMana());

        // Armour does not auto-gain on level-up — it is earned through upgrades only.

        static constexpr int kGainSize = 24;
        float gainY = sh * 0.465f;
        DrawText(hpLine.c_str(),   (int)(cx - MeasureText(hpLine.c_str(),   kGainSize) * 0.5f), (int)gainY,          kGainSize, Color{190, 255, 190, 255});
        DrawText(atkLine.c_str(),  (int)(cx - MeasureText(atkLine.c_str(),  kGainSize) * 0.5f), (int)(gainY + 30.f), kGainSize, Color{255, 210, 160, 255});
        DrawText(manaLine.c_str(), (int)(cx - MeasureText(manaLine.c_str(), kGainSize) * 0.5f), (int)(gainY + 60.f), kGainSize, Color{165, 195, 255, 255});
    }

    static const Color kExpFill = { 255, 210, 0, 230 };
    static constexpr float kBarW = 520.f;
    static constexpr float kBarH = 38.f;
    const float barX = cx - kBarW * 0.5f;
    const float barY = (levelsGained > 0) ? sh * 0.64f : sh * 0.51f;

    int curExp = ctx.player->GetExp();
    int expToNext = ctx.player->GetExpToNext();
    float expPct = (level < maxLevel && expToNext > 0) ? (float)curExp / (float)expToNext : 1.f;

    DrawRectangleRounded({ barX, barY, kBarW, kBarH }, 0.3f, 6, Fade(BLACK, 0.75f));
    if (level < maxLevel)
        DrawRectangleRounded({ barX, barY, kBarW * expPct, kBarH }, 0.3f, 6, kExpFill);
    DrawRectangleRoundedLines({ barX, barY, kBarW, kBarH }, 0.3f, 6, Fade(WHITE, 0.30f));

    const char* expLabel = (level < maxLevel) ? TextFormat("%d / %d  EXP", curExp, expToNext) : "MAX LEVEL";
    int expLabelW = MeasureText(expLabel, 20);
    DrawText(expLabel, (int)(cx - expLabelW * 0.5f), (int)(barY + kBarH * 0.5f - 10.f), 20, RAYWHITE);

    if (ctx.pendingExp > 0.f)
    {
        const char* pendingStr = TextFormat("+%d EXP incoming", (int)ctx.pendingExp);
        int pendingW = MeasureText(pendingStr, 26);
        DrawText(pendingStr, (int)(cx - pendingW * 0.5f), (int)(barY + kBarH + 14.f), 26, Fade(kExpFill, 0.85f));
    }

    float pulse = 0.60f + 0.40f * sinf((float)GetTime() * 4.f);
    if (ctx.expTallyDone)
    {
        const char* hint = nullptr;
        if (ctx.tallyLevelUpsRemaining > 0 && !ctx.tallyChoiceChaining)
            hint = ctx.touchModeActive ? "Tap to choose an upgrade!" : "Space / Enter  -  Choose an Upgrade!";
        else
            hint = ctx.touchModeActive ? "Tap to Continue" : "Space / Enter  to Continue";
        int hintW = MeasureText(hint, 26);
        DrawText(hint, (int)(cx - hintW * 0.5f), (int)(sh * 0.70f), 26, Fade(RAYWHITE, pulse));
    }
    else if (!ctx.touchModeActive)
    {
        const char* skipHint = "Space / Enter  to skip";
        int skipW = MeasureText(skipHint, 20);
        DrawText(skipHint, (int)(cx - skipW * 0.5f), (int)(sh * 0.70f), 20, Fade(RAYWHITE, 0.45f));
    }
}

void OverlayRenderer::DrawHowToPlay(const HowToPlayRenderContext& ctx) const
{
    const float sw = (float)kVirtualWidth;
    const float sh = (float)kVirtualHeight;
    const float dt = GetFrameTime();

    const int titleSz = (int)(sh * 0.062f);
    const int headerSz = (int)(sh * 0.034f);
    const int labelSz = (int)(sh * 0.027f);
    const int descSz = (int)(sh * 0.022f);
    const int tabSz = (int)(sh * 0.026f);

    *ctx.htpSlideOffset *= (1.f - std::min(dt * 14.f, 1.f));

    DrawScrollingCheckerboard(sw, sh, Color{ 96, 34, 86, 255 }, Color{ 132, 54, 116, 255 }, 22.f, 12.f);

    const float titleBarH = sh * 0.095f;
    DrawRectangle(0, 0, (int)sw, (int)titleBarH, Fade(Color{ 50, 14, 56, 255 }, 0.88f));
    const char* title = "HOW TO PLAY";
    int titleW = MeasureText(title, titleSz);
    for (int ox = -2; ox <= 2; ox += 2)
        for (int oy = -2; oy <= 2; oy += 2)
            if (ox || oy)
                DrawText(title, (int)(sw / 2.f - titleW / 2.f) + ox, (int)(titleBarH / 2.f - titleSz / 2.f) + oy, titleSz, BLACK);
    DrawText(title, (int)(sw / 2.f - titleW / 2.f), (int)(titleBarH / 2.f - titleSz / 2.f), titleSz, Color{ 255, 194, 92, 255 });

    const char* tabLabels[] = { "BASICS", "ELEMENTS", "THE WORLD", "TOUCH" };
    const int tabCount = 4;
    const float tabBarY = titleBarH + sh * 0.008f;
    const float tabBarH = sh * 0.063f;
    const float tabW = sw * 0.175f;
    const float tabGap = sw * 0.012f;
    const float tabsTotal = tabCount * tabW + (tabCount - 1) * tabGap;
    const float tabStartX = sw / 2.f - tabsTotal / 2.f;

    for (int i = 0; i < tabCount; i++)
    {
        float tx = tabStartX + i * (tabW + tabGap);
        Rectangle tabRect = { tx, tabBarY, tabW, tabBarH };
        bool isActive = (*ctx.htpTab == i);
        bool tabHov = CheckCollisionPointRec(GetVirtualMousePos(), tabRect);

        Color bgCol = isActive ? Color{ 185, 130, 30, 240 }
            : tabHov ? Color{ 100, 50,  90, 200 }
                     : Color{ 58,  22,  56, 200 };
        Color edgeCol = isActive ? Color{ 255, 210, 80, 255 }
            : Fade(Color{ 200, 140, 220, 255 }, 0.45f);

        DrawRectangleRounded(tabRect, 0.22f, 6, bgCol);
        DrawRectangleRoundedLines(tabRect, 0.22f, 6, edgeCol);
        if (isActive)
            DrawRectangle((int)(tx + tabW * 0.1f), (int)(tabBarY + tabBarH - 4.f), (int)(tabW * 0.8f), 4, Color{ 255, 210, 80, 255 });

        int lw = MeasureText(tabLabels[i], tabSz);
        Color textCol = isActive ? Color{ 20, 10, 5, 255 } : Color{ 220, 185, 240, 255 };
        DrawText(tabLabels[i], (int)(tx + tabW / 2.f - lw / 2.f), (int)(tabBarY + tabBarH / 2.f - tabSz / 2.f), tabSz, textCol);

        if (tabHov && !isActive && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            *ctx.htpSlideOffset = (i > *ctx.htpTab ? 1.f : -1.f) * sw * 0.12f;
            *ctx.htpTab = i;
            if (ctx.onUiConfirm)
                ctx.onUiConfirm();
        }
    }

    const float panelX = sw * 0.04f;
    const float panelY = tabBarY + tabBarH + sh * 0.010f;
    const float panelW = sw * 0.92f;
    const float panelH = sh * 0.755f;
    Rectangle panelRect = { panelX, panelY, panelW, panelH };

    if (ctx.shopBorderTex && ctx.shopBorderTex->id != 0)
        DrawNineSlice(*ctx.shopBorderTex, 16.f, 28.f, panelRect, Fade(WHITE, 0.88f));
    else
        DrawRectangleRounded(panelRect, 0.04f, 8, Fade(Color{ 45, 12, 52, 255 }, 0.88f));

    const float cx = panelX + sw * 0.03f + *ctx.htpSlideOffset;
    const float cy = panelY + sh * 0.025f;
    const float cw = panelW - sw * 0.06f;

    BeginScissorMode((int)panelX + 2, (int)panelY + 2, (int)panelW - 4, (int)panelH - 4);

    if (*ctx.htpTab == 0)
    {
        const float colW = cw * 0.45f;
        const float midGap = cw * 0.10f;
        const float leftX = cx;
        const float rightX = cx + colW + midGap;
        const float divX = cx + colW + midGap / 2.f;

        const float basicsHeaderY = cy + sh * 0.022f;
        DrawText("KEYBOARD & MOVEMENT", (int)leftX, (int)basicsHeaderY, headerSz, Color{ 255, 194, 92, 255 });
        DrawText("ACTIONS & ABILITIES", (int)rightX, (int)basicsHeaderY, headerSz, Color{ 255, 194, 92, 255 });
        DrawLineEx({ divX, basicsHeaderY + headerSz + 4.f }, { divX, panelY + panelH - sh * 0.03f }, 1.5f, Fade(Color{ 220, 160, 240, 255 }, 0.35f));

        struct KBEntry { const char* key; const char* desc; };
        KBEntry kb[] = {
            { "W / A / S / D",  "Move" },
            { "SPACE",          "Dash  (brief invincibility)" },
            { "Left Click",     "Melee attack" },
            { "1  2  3  4",     "Use ability in that slot" },
            { "Scroll Wheel",   "Cycle active ability" },
            { "ESC",            "Pause / unpause" },
        };
        float rowY = basicsHeaderY + headerSz + sh * 0.022f;
        for (auto& k : kb)
        {
            int kw = MeasureText(k.key, labelSz);
            float badgeH = (float)labelSz + 10.f;
            DrawRectangleRounded({ leftX, rowY - 4.f, (float)kw + 18.f, badgeH }, 0.28f, 4, Fade(Color{ 70, 18, 66, 255 }, 0.80f));
            DrawRectangleRoundedLines({ leftX, rowY - 4.f, (float)kw + 18.f, badgeH }, 0.28f, 4, Fade(Color{ 255, 182, 236, 255 }, 0.55f));
            DrawText(k.key, (int)leftX + 9, (int)rowY, labelSz, Color{ 255, 194, 92, 255 });
            DrawText(k.desc, (int)(leftX + kw + 30.f), (int)rowY, descSz, Color{ 20, 15, 25, 255 });
            rowY += sh * 0.072f;
        }

        rowY += sh * 0.010f;
        DrawText("EXP & GOLD", (int)leftX, (int)rowY, headerSz, Color{ 255, 194, 92, 255 });
        rowY += headerSz + sh * 0.012f;
        const char* expLines[] = {
            "Kill enemies to earn EXP and level up.",
            "Choose 1 of 3 upgrade cards each level.",
            "Enemies drop Gold coins when defeated.",
            "Spend Gold at Zeph's shop between biomes.",
        };
        for (auto& line : expLines)
        {
            DrawText(line, (int)leftX, (int)rowY, descSz, Color{ 20, 15, 25, 255 });
            rowY += descSz + sh * 0.008f;
        }

        struct ActionEntry { const char* name; const char* line1; const char* line2; };
        ActionEntry acts[] = {
            { "DASH", "Quick burst of movement in any direction.", "You are invincible for its full duration." },
            { "MELEE", "Left-click to swing your sword.", "Hits all enemies in a forward arc." },
            { "ABILITIES", "Press 1-4 to cast a learned ability.", "Each ability costs Mana to activate." },
            { "MANA", "Blue gems drop from defeated enemies.", "Pick them up to fuel your abilities." },
        };
        float rowR = basicsHeaderY + headerSz + sh * 0.022f;
        for (auto& a : acts)
        {
            DrawText(a.name, (int)rightX, (int)rowR, labelSz, Color{ 255, 194, 92, 255 });
            rowR += labelSz + sh * 0.005f;
            DrawText(a.line1, (int)rightX, (int)rowR, descSz, Color{ 20, 15, 25, 255 });
            rowR += descSz + sh * 0.003f;
            DrawText(a.line2, (int)rightX, (int)rowR, descSz, Color{ 20, 15, 25, 255 });
            rowR += descSz + sh * 0.030f;
        }
    }
    else if (*ctx.htpTab == 1)
    {
        const char* elemTitle = "THE MAGIC SYSTEM";
        DrawText(elemTitle, (int)(cx + cw / 2.f - MeasureText(elemTitle, headerSz) / 2.f), (int)cy, headerSz, Color{ 255, 194, 92, 255 });

        struct ElemEntry
        {
            const char* name;
            const char* effect;
            const char* desc1;
            const char* desc2;
            Color nameCol;
            Texture2D* icon;
        };
        ElemEntry elems[] = {
            { "FIRE", "BURN  -  Damage over time", "Enemies ignite and take periodic damage", "for several seconds after being hit.", Color{ 255, 120, 40, 255 }, ctx.abilityIconFireTex },
            { "ICE", "FREEZE  -  Stuns the enemy", "Frozen enemies cannot move or attack.", "Break the freeze with a melee hit for bonus damage.", Color{ 100, 210, 255, 255 }, ctx.abilityIconIceTex },
            { "ELECTRIC", "SHOCK  -  Amplifies melee damage", "Shocked enemies take greatly increased damage", "from your next melee strike.", Color{ 220, 220, 50, 255 }, ctx.abilityIconElectricTex },
        };

        const float iconSz = sh * 0.095f;
        const float rowSpacing = sh * 0.220f;
        float elemY = cy + headerSz + sh * 0.040f;

        for (auto& el : elems)
        {
            Rectangle iconRect = { cx, elemY, iconSz, iconSz };
            DrawRectangleRounded(iconRect, 0.22f, 6, Fade(el.nameCol, 0.18f));
            DrawRectangleRoundedLines(iconRect, 0.22f, 6, Fade(el.nameCol, 0.65f));
            if (el.icon && el.icon->id != 0)
                DrawTexturePro(*el.icon, { 0.f, 0.f, (float)el.icon->width, (float)el.icon->height }, { cx + iconSz * 0.1f, elemY + iconSz * 0.1f, iconSz * 0.8f, iconSz * 0.8f }, {}, 0.f, WHITE);
            else
                DrawCircleV({ cx + iconSz / 2.f, elemY + iconSz / 2.f }, iconSz * 0.35f, Fade(el.nameCol, 0.75f));

            float textX = cx + iconSz + sw * 0.025f;
            float textY = elemY;
            DrawText(el.name, (int)textX, (int)textY, headerSz, el.nameCol);
            textY += headerSz + sh * 0.006f;

            int effW = MeasureText(el.effect, labelSz);
            DrawRectangleRounded({ textX, textY - 2.f, (float)effW + 16.f, (float)labelSz + 10.f }, 0.3f, 4, Fade(el.nameCol, 0.22f));
            DrawRectangleRoundedLines({ textX, textY - 2.f, (float)effW + 16.f, (float)labelSz + 10.f }, 0.3f, 4, Fade(el.nameCol, 0.55f));
            DrawText(el.effect, (int)textX + 8, (int)textY, labelSz, el.nameCol);
            textY += labelSz + sh * 0.016f;
            DrawText(el.desc1, (int)textX, (int)textY, descSz, Color{ 210, 178, 220, 255 });
            textY += descSz + sh * 0.005f;
            DrawText(el.desc2, (int)textX, (int)textY, descSz, Color{ 170, 140, 185, 255 });

            elemY += rowSpacing;
        }
    }
    else if (*ctx.htpTab == 2)
    {
        const float colW = cw * 0.45f;
        const float midGap = cw * 0.10f;
        const float leftX = cx;
        const float rightX = cx + colW + midGap;
        const float divX = cx + colW + midGap / 2.f;

        const float worldHeaderY = cy + sh * 0.022f;
        DrawText("MAP LEGEND", (int)leftX, (int)worldHeaderY, headerSz, Color{ 255, 194, 92, 255 });
        DrawText("ZEPH'S SHOP", (int)rightX, (int)worldHeaderY, headerSz, Color{ 255, 194, 92, 255 });
        DrawLineEx({ divX, worldHeaderY + headerSz + 4.f }, { divX, panelY + panelH - sh * 0.03f }, 1.5f, Fade(Color{ 220, 160, 240, 255 }, 0.35f));

        struct MapEntry { Texture2D* tex; Color fallback; const char* name; const char* desc; };
        MapEntry mapIcons[] = {
            { ctx.mapIconNormal,   GRAY,   "NORMAL ROOM", "Clear all enemies to proceed." },
            { ctx.mapIconElite,    ORANGE, "ELITE ROOM",  "Tougher enemies, better drops." },
            { ctx.mapIconShop,     GOLD,   "SHOP",        "Buy upgrades and potions from Zeph." },
            { ctx.mapIconTreasure, YELLOW, "TREASURE",    "Find a powerful item for free." },
            { ctx.mapIconBoss,     RED,    "BOSS",        "Defeat the boss to complete the biome."},
            { ctx.mapIconRest,     GREEN,  "REST SITE",   "Recover HP between battles." },
        };
        const float mapIconSz = sh * 0.052f;
        float mapY = worldHeaderY + headerSz + sh * 0.022f;
        for (auto& mi : mapIcons)
        {
            if (mi.tex && mi.tex->id != 0)
                DrawTexturePro(*mi.tex, { 0.f, 0.f, (float)mi.tex->width, (float)mi.tex->height }, { leftX, mapY, mapIconSz, mapIconSz }, {}, 0.f, WHITE);
            else
            {
                DrawRectangleRounded({ leftX, mapY, mapIconSz, mapIconSz }, 0.28f, 4, Fade(mi.fallback, 0.65f));
                DrawRectangleRoundedLines({ leftX, mapY, mapIconSz, mapIconSz }, 0.28f, 4, BLACK);
            }
            float tx = leftX + mapIconSz + sw * 0.015f;
            DrawText(mi.name, (int)tx, (int)mapY, labelSz, Color{ 255, 194, 92, 255 });
            DrawText(mi.desc, (int)tx, (int)(mapY + labelSz + 3.f), descSz, Color{ 20, 15, 25, 255 });
            mapY += sh * 0.095f;
        }

        struct ShopLine { const char* text; bool isHeader; };
        ShopLine shopLines[] = {
            { "Between biomes you visit Zeph's Shop.", false },
            { "", false },
            { "UPGRADES", true },
            { "Buy powerful passive boosts for your run.", false },
            { "Epic-tier items offer rare, powerful effects.", false },
            { "", false },
            { "REROLL", true },
            { "Pay gold to refresh the item selection.", false },
            { "Cost increases with each reroll.", false },
            { "", false },
            { "POTIONS", true },
            { "Health Potion  -  restore HP instantly.", false },
            { "Mana Potion  -  restore Mana instantly.", false },
            { "", false },
            { "DAILY DEAL", true },
            { "One item each visit is 25% off.", false },
            { "Look for the gold price tag.", false },
        };
        float shopY = worldHeaderY + headerSz + sh * 0.022f;
        for (auto& sl : shopLines)
        {
            if (sl.text[0] == '\0') { shopY += descSz * 0.5f; continue; }
            Color col = sl.isHeader ? Color{ 255, 194, 92, 255 } : Color{ 20, 15, 25, 255 };
            int sz = sl.isHeader ? labelSz : descSz;
            DrawText(sl.text, (int)rightX, (int)shopY, sz, col);
            shopY += sz + sh * 0.007f;
        }
    }
    else if (*ctx.htpTab == 3)
    {
        const char* touchTitle = "TOUCH CONTROLS";
        DrawText(touchTitle, (int)(cx + cw / 2.f - MeasureText(touchTitle, headerSz) / 2.f), (int)cy, headerSz, Color{ 255, 194, 92, 255 });

        const float diagW = cw * 0.78f;
        const float diagH = sh * 0.285f;
        const float diagX = cx + cw / 2.f - diagW / 2.f;
        const float diagY = cy + headerSz + sh * 0.025f;
        const float halfW = diagW / 2.f;

        DrawRectangleRounded({ diagX, diagY, diagW, diagH }, 0.06f, 8, Fade(Color{ 30, 10, 36, 255 }, 0.90f));
        DrawRectangleRoundedLines({ diagX, diagY, diagW, diagH }, 0.06f, 8, Fade(Color{ 220, 160, 240, 255 }, 0.55f));
        DrawRectangleRounded({ diagX, diagY, halfW, diagH }, 0.06f, 8, Fade(Color{ 40, 80, 120, 255 }, 0.35f));
        float jsX = diagX + halfW / 2.f;
        float jsY = diagY + diagH / 2.f;
        DrawCircleV({ jsX, jsY }, diagH * 0.28f, Fade(Color{ 80, 130, 200, 255 }, 0.30f));
        DrawCircleLinesV({ jsX, jsY }, diagH * 0.28f, Fade(Color{ 120, 180, 255, 255 }, 0.65f));
        DrawCircleV({ jsX, jsY }, diagH * 0.11f, Fade(Color{ 160, 210, 255, 255 }, 0.80f));
        DrawText("MOVE", (int)(jsX - MeasureText("MOVE", descSz) / 2.f), (int)(diagY + diagH - descSz - sh * 0.015f), descSz, Color{ 150, 200, 255, 255 });

        DrawLineEx({ diagX + halfW, diagY + sh * 0.015f }, { diagX + halfW, diagY + diagH - sh * 0.015f }, 1.5f, Fade(WHITE, 0.30f));
        DrawRectangleRounded({ diagX + halfW, diagY, halfW, diagH }, 0.06f, 8, Fade(Color{ 100, 40, 80, 255 }, 0.35f));
        float btnR = diagH * 0.18f;
        float b1X = diagX + halfW + halfW * 0.32f;
        float b2X = diagX + halfW + halfW * 0.70f;
        float btY = diagY + diagH * 0.42f;

        DrawCircleV({ b1X, btY }, btnR, Fade(Color{ 200, 80, 80, 255 }, 0.55f));
        DrawCircleLinesV({ b1X, btY }, btnR, Fade(WHITE, 0.50f));
        DrawText("ATK", (int)(b1X - MeasureText("ATK", descSz) / 2.f), (int)(btY - descSz / 2.f), descSz, WHITE);

        DrawCircleV({ b2X, btY }, btnR, Fade(Color{ 80, 120, 220, 255 }, 0.55f));
        DrawCircleLinesV({ b2X, btY }, btnR, Fade(WHITE, 0.50f));
        DrawText("DASH", (int)(b2X - MeasureText("DASH", descSz) / 2.f), (int)(btY - descSz / 2.f), descSz, WHITE);

        DrawText("COMBAT", (int)(diagX + halfW + halfW / 2.f - MeasureText("COMBAT", descSz) / 2.f), (int)(diagY + diagH - descSz - sh * 0.015f), descSz, Color{ 255, 150, 200, 255 });

        struct TipEntry { const char* label; const char* desc; };
        TipEntry tips[] = {
            { "JOYSTICK:",  "Drag anywhere on the left half to move your character." },
            { "ATTACK:",    "Tap the ATK button to swing your sword." },
            { "DASH:",      "Tap DASH for a quick invincible burst of speed." },
            { "ABILITIES:", "Tap any ability icon at the bottom of the screen." },
            { "PAUSE:",     "Tap the pause icon in the top-right corner." },
        };
        const float tipsX = cx + cw / 2.f - cw * 0.35f;
        float tipY = diagY + diagH + sh * 0.030f;
        for (auto& tip : tips)
        {
            int lw = MeasureText(tip.label, labelSz);
            DrawText(tip.label, (int)tipsX, (int)tipY, labelSz, Color{ 255, 194, 92, 255 });
            DrawText(tip.desc, (int)(tipsX + lw + sw * 0.012f), (int)tipY, descSz, Color{ 20, 15, 25, 255 });
            tipY += sh * 0.068f;
        }
    }

    EndScissorMode();

    const float btnW = sw * 0.14f;
    const float btnH = sh * 0.055f;
    const float btnX = sw / 2.f - btnW / 2.f;
    const float btnY = sh - btnH - sh * 0.016f;
    Rectangle backBtn{ btnX, btnY, btnW, btnH };
    bool hovered = CheckCollisionPointRec(GetVirtualMousePos(), backBtn);

    DrawRectangleRounded(backBtn, 0.3f, 6, hovered ? Color{ 196, 86, 165, 240 } : Color{ 142, 58, 132, 228 });
    DrawRectangleRoundedLines(backBtn, 0.3f, 6, Fade(Color{ 255, 194, 92, 255 }, 0.68f));

    const char* backLabel = (ctx.howToPlayFrom == GameState::Pause) ? "Resume Game" : "< Back";
    int backLabelSz = (int)(sh * 0.030f);
    int backW = MeasureText(backLabel, backLabelSz);
    DrawText(backLabel, (int)(btnX + btnW / 2.f - backW / 2.f), (int)(btnY + btnH / 2.f - backLabelSz / 2.f), backLabelSz, Color{ 255, 243, 214, 255 });

    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && ctx.onBack)
        ctx.onBack();
}
