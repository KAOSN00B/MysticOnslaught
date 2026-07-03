#include "HUDRenderer.h"
#include "VirtualCanvas.h"

#include "Character.h"
#include "VirtualCanvas.h"

#include <algorithm>
#include <cmath>
#include <string>

void HUDRenderer::DrawMiniMap(const MiniMapContext& ctx) const
{
    const float mapW = ctx.map->width * ctx.mapScale;
    const float mapH = ctx.map->height * ctx.mapScale;

    const float miniW = 160.f;
    const float miniH = miniW * (mapH / mapW);
    const float padding = 12.f;
    const float originX = padding;
    const float originY = 112.f;

    DrawRectangleRounded(Rectangle{ originX - 2.f, originY - 2.f, miniW + 4.f, miniH + 4.f },
        0.08f, 4, Fade(BLACK, 0.85f));
    DrawRectangle((int)originX, (int)originY, (int)miniW, (int)miniH, Fade(DARKGRAY, 0.5f));

    auto toMini = [&](Vector2 worldPos) -> Vector2 {
        return Vector2{
            originX + (worldPos.x / mapW) * miniW,
            originY + (worldPos.y / mapH) * miniH
        };
    };

    for (const auto& enemy : *ctx.enemies)
    {
        if (!enemy->IsActive() || !enemy->IsAlive())
            continue;

        Vector2 dot = toMini(enemy->GetWorldPos());
        if (enemy->AsMolarbeast() != nullptr)
            DrawCircleV(dot, 6.f, Fade(MAROON, 0.95f));
        else if (enemy->AsCyclops() != nullptr)
            DrawCircleV(dot, 4.f, Fade(ORANGE, 0.9f));
        else if (enemy->AsOgre() != nullptr)
            DrawCircleV(dot, 4.f, Fade(BROWN, 0.9f));
        else
            DrawCircleV(dot, 3.f, Fade(RED, 0.9f));
    }

    for (const auto& prop : *ctx.props)
    {
        Vector2 dot = toMini(prop.GetWorldPos());
        DrawCircleV(dot, 3.f, Fade(SKYBLUE, 0.55f));
    }

    for (const auto& pickup : *ctx.pickups)
    {
        if (!pickup->IsActive())
            continue;

        Vector2 dot = toMini(pickup->GetWorldPos());
        if (pickup->GetType() == PickupType::Gold)
            DrawCircleV(dot, 4.f, Fade(GOLD, 0.95f));
        else if (pickup->GetType() == PickupType::Cell)
            DrawCircleV(dot, 4.f, Fade(Color{ 255, 90, 200, 255 }, 0.95f));
        else
            DrawCircleV(dot, 4.f, Fade(GREEN, 0.9f));
    }

    Vector2 playerDot = toMini(ctx.player->GetWorldPos());
    DrawCircleV(playerDot, 5.f, Fade(YELLOW, 1.0f));

    DrawRectangleLinesEx(Rectangle{ originX, originY, miniW, miniH }, 1.f, Fade(LIGHTGRAY, 0.6f));
}

void HUDRenderer::DrawHUD(const HUDRenderContext& ctx) const
{
    static constexpr float kNewBarW   = 400.f;
    static constexpr float kNewBarH   = 28.f;
    static constexpr float kNewBarGap = 8.f;
    static constexpr float kNewBotPad = 12.f;
    static constexpr float kNewTopPad = 18.f;
    static constexpr float kEliteEnrageWarningDuration = 4.f;

    const float hpBarY   = kNewTopPad;
    const float manaBarY = hpBarY + kNewBarH + kNewBarGap;
    const float barX     = (float)kVirtualWidth / 2.f - kNewBarW / 2.f;

    auto drawLabelBox = [&](const char* text, float x, float y, int fontSize, Color textColor)
    {
        int textW = MeasureText(text, fontSize);
        const float padX = 12.f;
        const float padY = 8.f;
        DrawRectangleRounded(
            Rectangle{ x - padX, y - padY, textW + padX * 2.f, fontSize + padY * 2.f },
            0.18f, 6, Fade(BLACK, 0.55f));
        DrawText(text, (int)x, (int)y, fontSize, textColor);
    };

    drawLabelBox(("Gold: " + std::to_string(ctx.player->GetGold())).c_str(), 20.f, 16.f, 28, GOLD);
    // Carried Mystic Cells — banked with Zeph between zones, lost on death.
    drawLabelBox(("Cells: " + std::to_string(ctx.player->GetCells())).c_str(), 20.f, 58.f, 28, Color{ 255, 120, 210, 255 });
    drawLabelBox(("Enemies Left: " + std::to_string(ctx.getActiveEnemyCount())).c_str(), 20.f, 100.f, 28, RAYWHITE);

    {
        bool isBoss = (ctx.currentRoomType == RoomType::Boss);
        bool isElite = (ctx.currentRoomType == RoomType::Elite);
        const char* roomTypeSuffix =
            isBoss    ? "  BOSS" :
            isElite   ? "  ELITE" :
            (ctx.currentRoomType == RoomType::Rest)     ? "  REST" :
            (ctx.currentRoomType == RoomType::Treasure) ? "  TREASURE" :
            (ctx.currentRoomType == RoomType::Store)    ? "  SHOP" : "";
        const char* roomLabel = TextFormat("Act %d  Room %d%s",
            ctx.currentAct, ctx.currentRoom, roomTypeSuffix);
        int roomLabelW = MeasureText(roomLabel, 32);
        Color labelColor = isBoss ? ORANGE : (isElite ? Color{255,140,0,255} : RAYWHITE);
        drawLabelBox(roomLabel, (float)(kVirtualWidth - roomLabelW - 130), 18.f, 32, labelColor);
    }

    {
        float maxHp = ctx.player->GetMaxHealthValue();
        float curHp = ctx.player->GetHealthValue();
        float hpPct = (maxHp > 0.f) ? (curHp / maxHp) : 0.f;
        Color fillColor = (hpPct > 0.70f) ? GREEN : (hpPct > 0.30f) ? YELLOW : RED;

        if (hpPct <= 0.30f)
        {
            float pulse      = (sinf((float)GetTime() * (2.f * PI / 3.f)) + 1.f) * 0.5f;
            float exps[]     = { 4.f, 9.f, 15.f };
            float alphas[]   = { 0.45f, 0.28f, 0.13f };
            for (int i = 0; i < 3; i++)
            {
                float e = exps[i];
                DrawRectangleRounded(
                    { barX - e, hpBarY - e, kNewBarW + e * 2.f, kNewBarH + e * 2.f },
                    0.35f, 6, Fade(RED, alphas[i] * pulse));
            }
        }

        DrawRectangleRounded({ barX, hpBarY, kNewBarW * hpPct, kNewBarH }, 0.3f, 6, fillColor);
        DrawRectangleRoundedLines({ barX, hpBarY, kNewBarW, kNewBarH }, 0.3f, 6, Fade(WHITE, 0.25f));

        const char* hpLabel = TextFormat("HP  %.0f / %.0f", curHp, maxHp);
        int labelW = MeasureText(hpLabel, 18);
        DrawText(hpLabel,
            (int)(barX + kNewBarW / 2.f - labelW / 2.f),
            (int)(hpBarY + kNewBarH / 2.f - 9.f),
            18, BLACK);
    }

    {
        int curMana = ctx.player->GetMana();
        int maxMana = ctx.player->GetMaxMana();
        float manaPct = (maxMana > 0) ? (float)curMana / (float)maxMana : 0.f;
        static const Color kManaFill = { 60, 120, 255, 230 };

        DrawRectangleRounded({ barX, manaBarY, kNewBarW * manaPct, kNewBarH }, 0.3f, 6, kManaFill);
        DrawRectangleRoundedLines({ barX, manaBarY, kNewBarW, kNewBarH }, 0.3f, 6, Fade(WHITE, 0.25f));

        const char* manaLabel = TextFormat("MP  %d / %d", curMana, maxMana);
        int manaLabelW = MeasureText(manaLabel, 18);
        DrawText(manaLabel,
            (int)(barX + kNewBarW / 2.f - manaLabelW / 2.f),
            (int)(manaBarY + kNewBarH / 2.f - 9.f),
            18, BLACK);
    }

    if (ctx.currentRoomType == RoomType::Elite && ctx.eliteMechanic >= 0)
    {
        static constexpr const char* kMechanicNames[] = {
            "ARENA CONSTRICTION",
            "INVULNERABILITY LINKS",
            "PERMANENT ENRAGE",
            "GAP-CLOSER LEAP",
            "ROOM HAZARDS"
        };
        static constexpr Color kMechanicColors[] = {
            Color{220, 40, 200, 255},
            Color{180, 100, 255, 255},
            Color{255, 60,  60, 255},
            Color{255,180,  60, 255},
            Color{255,220,  80, 255},
        };
        const char* mechLabel = kMechanicNames[ctx.eliteMechanic];
        Color mechColor = kMechanicColors[ctx.eliteMechanic];
        int mw = MeasureText(mechLabel, 20);
        drawLabelBox(mechLabel, (float)(kVirtualWidth - mw - 130), 58.f, 20, mechColor);
    }

    if (ctx.currentRoomType == RoomType::Elite && ctx.eliteEnrageWarningTimer > 0.f)
    {
        const float sw = (float)kVirtualWidth;
        const float sh = (float)kVirtualHeight;

        float alpha = 1.f;
        if (ctx.eliteEnrageWarningTimer > kEliteEnrageWarningDuration - 0.5f)
            alpha = (kEliteEnrageWarningDuration - ctx.eliteEnrageWarningTimer) / 0.5f;
        else if (ctx.eliteEnrageWarningTimer < 0.5f)
            alpha = ctx.eliteEnrageWarningTimer / 0.5f;
        alpha = std::max(0.f, std::min(1.f, alpha));

        const char* line1 = "ELITE ENCOUNTER";
        const char* line2 = "CONDITION: ENRAGED  |  FAST & LETHAL";
        const int sz1 = 48;
        const int sz2 = 28;
        const float bannerH = 120.f;
        const float bannerY = sh * 0.38f;

        DrawRectangle(0, (int)bannerY, (int)sw, (int)bannerH, Fade(Color{20,0,0,220}, alpha));
        DrawRectangle(0, (int)bannerY, (int)sw, 3, Fade(Color{200,0,0,255}, alpha));
        DrawRectangle(0, (int)(bannerY + bannerH - 3), (int)sw, 3, Fade(Color{200,0,0,255}, alpha));

        int w1 = MeasureText(line1, sz1);
        int w2 = MeasureText(line2, sz2);

        DrawText(line1, (int)(sw / 2.f - w1 / 2.f), (int)(bannerY + 14.f), sz1,
            Fade(Color{255, 60, 60, 255}, alpha));
        DrawText(line2, (int)(sw / 2.f - w2 / 2.f), (int)(bannerY + 14.f + sz1 + 8.f), sz2,
            Fade(Color{255, 180, 60, 255}, alpha));
    }

    if (ctx.debug->IsActive() && !ctx.touchModeActive)
    {
        const char* debugHint = "Press F1 for Debug Menu";
        const int hintSz = 16;
        const int hintW = MeasureText(debugHint, hintSz);
        const float boxW = (float)hintW + 24.f;
        const float boxH = 34.f;
        const float boxX = (float)kVirtualWidth - boxW - 14.f;
        const float boxY = 14.f;

        DrawRectangleRounded({ boxX, boxY, boxW, boxH }, 0.22f, 6, Fade(BLACK, 0.58f));
        DrawRectangleRoundedLines({ boxX, boxY, boxW, boxH }, 0.22f, 6, Fade(Color{ 255, 214, 150, 255 }, 0.45f));
        DrawText(debugHint, (int)(boxX + 12.f), (int)(boxY + boxH * 0.5f - hintSz * 0.5f),
            hintSz, Color{ 255, 235, 210, 255 });
    }

    if (ctx.touchModeActive)
    {
        ctx.drawTouchAbilityArc();
        ctx.touch->Draw(ctx.windowWidth, ctx.windowHeight);
        if (ctx.debug->IsActive())
        {
            const Rectangle debugTab{ (float)ctx.windowWidth - 54.f, (float)ctx.windowHeight * 0.43f, 42.f, 132.f };
            DrawRectangleRounded(debugTab, 0.35f, 6, Fade(Color{ 92, 58, 26, 255 }, 0.78f));
            DrawRectangleRoundedLines(debugTab, 0.35f, 6, Fade(Color{ 255, 214, 150, 255 }, 0.66f));

            const char* tabLabel = ctx.debug->IsOpen() ? "DBG <" : "DBG >";
            const int tabSz = 18;
            const int tabW = MeasureText(tabLabel, tabSz);
            DrawText(tabLabel,
                (int)(debugTab.x + debugTab.width * 0.5f - tabW * 0.5f),
                (int)(debugTab.y + debugTab.height * 0.5f - tabSz * 0.5f),
                tabSz, RAYWHITE);
        }

        {
            static constexpr float kPauseW = 90.f;
            static constexpr float kPauseH = 48.f;
            static constexpr float kPausePad = 14.f;
            Rectangle pauseRec{ (float)ctx.windowWidth - kPauseW - kPausePad, kPausePad, kPauseW, kPauseH };
            DrawRectangleRounded(pauseRec, 0.22f, 6, Fade(BLACK, 0.55f));
            DrawRectangleRoundedLines(pauseRec, 0.22f, 6, Fade(WHITE, 0.40f));
            int pw = MeasureText("II", 26);
            DrawText("II",
                (int)(pauseRec.x + pauseRec.width / 2.f - pw / 2.f),
                (int)(pauseRec.y + pauseRec.height / 2.f - 13.f),
                26, RAYWHITE);
        }
    }
    else
    {
        ctx.drawAbilityBar();
    }

    DrawMiniMap(ctx.minimap);

    if (ctx.bossWarningTimer > 0.f)
    {
        const char* warning = "DON'T GET TOO CLOSE";
        int warningSize = 34;
        int warningWidth = MeasureText(warning, warningSize);
        drawLabelBox(warning, (float)(kVirtualWidth / 2 - warningWidth / 2), 96.f, warningSize, ORANGE);
    }
}
