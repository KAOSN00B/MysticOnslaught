#include "ShopManager.h"
#include "NineSlice.h"
#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <string>

// ── File-local helpers ────────────────────────────────────────────────────────

static const char* ShopUpgradeName(UpgradeType t)
{
    switch (t)
    {
    case UpgradeType::AttackPower:      return "Attack Power";
    case UpgradeType::AttackRange:      return "Attack Range";
    case UpgradeType::MaxHealth:        return "Max Health";
    case UpgradeType::MaxMana:          return "Max Mana";
    case UpgradeType::Defense:          return "Defense";
    case UpgradeType::MoveSpeed:        return "Move Speed";
    case UpgradeType::IronConstitution: return "Iron Constitution";
    case UpgradeType::SwiftFeet:        return "Swift Feet";
    case UpgradeType::Ferocity:         return "Ferocity";
    case UpgradeType::ArcaneMind:       return "Arcane Mind";
    case UpgradeType::IronSkin:         return "Iron Skin";
    case UpgradeType::BladeEdge:        return "Blade Edge";
    case UpgradeType::WarGod:           return "War God";
    case UpgradeType::Resilience:       return "Resilience";
    case UpgradeType::BladeStorm:       return "Blade Storm";
    case UpgradeType::Juggernaut:       return "Juggernaut";
    case UpgradeType::ArcaneColossus:   return "Arcane Colossus";
    default:                            return "Unknown";
    }
}

static const char* ShopUpgradeDesc(UpgradeType t)
{
    switch (t)
    {
    case UpgradeType::AttackPower:      return "+10% attack";
    case UpgradeType::AttackRange:      return "+10% range";
    case UpgradeType::MaxHealth:        return "+10 max HP";
    case UpgradeType::MaxMana:          return "+10 max MP";
    case UpgradeType::Defense:          return "+5% defense";
    case UpgradeType::MoveSpeed:        return "+5% speed";
    case UpgradeType::IronConstitution: return "+25% max HP";
    case UpgradeType::SwiftFeet:        return "+15% speed";
    case UpgradeType::Ferocity:         return "+15% attack";
    case UpgradeType::ArcaneMind:       return "+40 max mana";
    case UpgradeType::IronSkin:         return "+8% defense";
    case UpgradeType::BladeEdge:        return "+15% range";
    case UpgradeType::WarGod:           return "+20% atk  +10% range";
    case UpgradeType::Resilience:       return "+30% HP  heal 3";
    case UpgradeType::BladeStorm:       return "+18% atk  +18% spd";
    case UpgradeType::Juggernaut:       return "+20% HP  +8% def";
    case UpgradeType::ArcaneColossus:   return "+50 mana  +15% atk";
    default:                            return "";
    }
}

static UpgradeRarity ShopUpgradeRarity(UpgradeType t)
{
    if (t <= UpgradeType::MoveSpeed)  return UpgradeRarity::Common;
    if (t <= UpgradeType::BladeEdge)  return UpgradeRarity::Rare;
    return UpgradeRarity::Epic;
}

static int ShopUpgradePrice(UpgradeType t)
{
    switch (ShopUpgradeRarity(t))
    {
    case UpgradeRarity::Common: return 30;
    case UpgradeRarity::Rare:   return 60;
    default:                    return 100;
    }
}

static const char* ShopAbilityName(AbilityType t)
{
    switch (t)
    {
    case AbilityType::FireSpread:      return "Fire Spread";
    case AbilityType::IceSpread:       return "Ice Spread";
    case AbilityType::ElectricSpread:  return "Electric Spread";
    case AbilityType::FireBolt:        return "Fire Bolt";
    case AbilityType::IceBolt:         return "Ice Bolt";
    case AbilityType::ElectricBolt:    return "Electric Bolt";
    default:                           return "Ability";
    }
}

static const char* ShopAbilityDesc(AbilityType t)
{
    switch (t)
    {
    case AbilityType::FireSpread:      return "8-way fire burst  2 MP";
    case AbilityType::IceSpread:       return "8-way ice burst  2 MP";
    case AbilityType::ElectricSpread:  return "8-way shock burst  2 MP";
    case AbilityType::FireBolt:        return "Aimed fire bolt  4 MP";
    case AbilityType::IceBolt:         return "Aimed ice bolt  4 MP";
    case AbilityType::ElectricBolt:    return "Aimed shock bolt  4 MP";
    default:                           return "";
    }
}

static int ShopAbilityPrice(AbilityType t)
{
    switch (t)
    {
    case AbilityType::FireBolt:
    case AbilityType::IceBolt:
    case AbilityType::ElectricBolt: return 75;
    default:                        return 50;
    }
}

static Color ShopRarityColor(UpgradeRarity r)
{
    switch (r)
    {
    case UpgradeRarity::Common: return Color{ 80, 80,  80, 255};
    case UpgradeRarity::Rare:   return Color{ 55,100, 200, 255};
    default:                    return Color{130, 45, 200, 255};
    }
}

// ── Setup ─────────────────────────────────────────────────────────────────────

void ShopManager::Init(const ShopTextures& tex)
{
    _tex = tex;
}

void ShopManager::Enter(Vector2 npcWorldPos, Character& player)
{
    _npcPos     = npcWorldPos;
    _nearNpc    = false;
    _tab        = 0;
    _rerollCost = 100;
    _dialogue   = "Welcome to Zeph's Wares! What do you need?";
    GenerateInventory(player);
}

// ── Per-frame (Store room) ────────────────────────────────────────────────────

bool ShopManager::UpdateNpc(Character& player)
{
    // Collision push (MTV against a 40×60 world-unit box)
    const float nHalfW = 20.f, nHalfH = 30.f;
    Rectangle npcRect = { _npcPos.x - nHalfW, _npcPos.y - nHalfH,
                          nHalfW * 2.f, nHalfH * 2.f };
    Rectangle playerRect = player.GetCollisionRec();
    if (CheckCollisionRecs(npcRect, playerRect))
    {
        float overlapX = std::min(playerRect.x + playerRect.width,  npcRect.x + npcRect.width)
                       - std::max(playerRect.x, npcRect.x);
        float overlapY = std::min(playerRect.y + playerRect.height, npcRect.y + npcRect.height)
                       - std::max(playerRect.y, npcRect.y);
        Vector2 pp = player.GetWorldPos();
        if (overlapX < overlapY)
        {
            float dir = (playerRect.x + playerRect.width  * 0.5f < npcRect.x + npcRect.width  * 0.5f) ? -1.f : 1.f;
            player.SetWorldPos({ pp.x + dir * overlapX, pp.y });
        }
        else
        {
            float dir = (playerRect.y + playerRect.height * 0.5f < npcRect.y + npcRect.height * 0.5f) ? -1.f : 1.f;
            player.SetWorldPos({ pp.x, pp.y + dir * overlapY });
        }
    }

    // Proximity check
    float dist = Vector2Distance(player.GetWorldPos(), _npcPos);
    _nearNpc   = (dist < 130.f);

    if (_nearNpc && IsKeyPressed(KEY_E))
    {
        _dialogue = "Welcome to Zeph's Wares! What do you need?";
        _tab      = 0;
        return true;   // engine: transition to GameState::Shop
    }
    return false;
}

void ShopManager::DrawNpc(Vector2 worldOffset) const
{
    const float sw2 = GetScreenWidth()  * 0.5f;
    const float sh2 = GetScreenHeight() * 0.5f;
    float sx = _npcPos.x + worldOffset.x + sw2;
    float sy = _npcPos.y + worldOffset.y + sh2;
    const float nW = 40.f, nH = 60.f;

    DrawRectangle((int)(sx - nW * 0.5f), (int)(sy - nH * 0.5f),
                  (int)nW, (int)nH, ORANGE);
    DrawRectangleLines((int)(sx - nW * 0.5f), (int)(sy - nH * 0.5f),
                       (int)nW, (int)nH, Color{200, 120, 0, 255});

    const char* nameplate = "Zeph";
    int npFs = 16, npW = MeasureText(nameplate, npFs);
    DrawRectangle((int)(sx - npW * 0.5f - 5), (int)(sy - nH * 0.5f - 24),
                  npW + 10, 20, Fade(BLACK, 0.6f));
    DrawText(nameplate, (int)(sx - npW * 0.5f),
             (int)(sy - nH * 0.5f - 22), npFs, GOLD);

    if (_nearNpc)
    {
        const char* prompt = "[E] Talk";
        int prFs = 18, prW = MeasureText(prompt, prFs);
        DrawRectangle((int)(sx - prW * 0.5f - 6), (int)(sy - nH * 0.5f - 52),
                      prW + 12, 24, Fade(BLACK, 0.70f));
        DrawText(prompt, (int)(sx - prW * 0.5f),
                 (int)(sy - nH * 0.5f - 50), prFs, RAYWHITE);
    }
}

// ── Per-frame (GameState::Shop) ───────────────────────────────────────────────

bool ShopManager::Update(Character& player)
{
    const float sw  = (float)GetScreenWidth();
    const float sh  = (float)GetScreenHeight();
    const float pad = 16.f;

    Vector2 mouse   = GetMousePosition();
    bool    clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    // ── Layout (must match Draw exactly) ─────────────────────────────────
    static constexpr float kBorderDst_u = 32.f;
    const float leftW  = sw * 0.30f;
    const float leaveH = 48.f;
    const float leaveY = sh - pad - leaveH;
    const float dialH  = std::max(sh * 0.12f, kBorderDst_u * 2.f + 30.f);
    const float dialY  = leaveY - pad * 0.5f - dialH;
    const float shopX  = pad + leftW + pad;
    const float shopY  = pad;
    const float shopW  = sw - shopX - pad;
    const float shopH  = dialY - pad * 0.5f - shopY;
    const float iPad   = kBorderDst_u;

    // ── Leave button ─────────────────────────────────────────────────────
    const float leaveW  = 180.f;
    const float rerollW = 200.f;
    const float leaveX  = shopX + shopW * 0.5f + 8.f;
    const float rerollX = shopX + shopW * 0.5f - rerollW - 8.f;
    Rectangle leaveBtn  = { leaveX,  leaveY, leaveW,  leaveH };
    Rectangle rerollBtn = { rerollX, leaveY, rerollW, leaveH };

    if (clicked && CheckCollisionPointRec(mouse, leaveBtn))
    {
        _dialogue = "Safe travels, adventurer.";
        return true;   // engine: transition to GameState::Play
    }
    if (clicked && CheckCollisionPointRec(mouse, rerollBtn))
    {
        if (player.GetGold() >= _rerollCost)
        {
            player.AddGold(-_rerollCost);
            _rerollCost += 50;
            GenerateInventory(player);
            _dialogue = "Fresh stock, just for you!";
        }
        else
        {
            _dialogue = "You don't have enough gold for a reroll!";
        }
        return false;
    }

    // ── Tab buttons ───────────────────────────────────────────────────────
    const float titleH = 46.f;
    const float tabH   = 38.f;
    const float tabW   = (shopW - iPad * 2.f) * 0.5f - 4.f;
    const float tabY   = shopY + titleH;
    Rectangle tabWares = { shopX + iPad,               tabY, tabW, tabH };
    Rectangle tabAb    = { shopX + iPad + tabW + 8.f,  tabY, tabW, tabH };

    if (clicked && CheckCollisionPointRec(mouse, tabWares)) _tab = 0;
    if (clicked && CheckCollisionPointRec(mouse, tabAb))    _tab = 1;

    // ── Content area ──────────────────────────────────────────────────────
    const float contentY = tabY + tabH + iPad;
    const float contentH = shopH - titleH - tabH - iPad * 2.f;
    const float contentW = shopW - iPad * 2.f;

    if (_tab == 0)
    {
        // 2 rows × 3 cols of items
        const float cols   = 3.f, rows = 2.f, gap = 10.f;
        const float itemW  = (contentW - gap * (cols - 1.f)) / cols;
        const float itemH  = (contentH - gap * (rows - 1.f)) / rows;
        const float buyH   = std::min(30.f, itemH * 0.20f);

        for (int idx = 0; idx < (int)_inventory.size(); idx++)
        {
            ShopItem& item = _inventory[idx];
            if (item.purchased) continue;

            int   col  = idx % 3;
            int   row  = idx / 3;
            float ix   = shopX + iPad + col * (itemW + gap);
            float iy   = contentY + row * (itemH + gap);

            Rectangle buyBtn = { ix + 4.f, iy + itemH - buyH - 4.f, itemW - 8.f, buyH };
            if (clicked && CheckCollisionPointRec(mouse, buyBtn))
            {
                if (player.GetGold() >= item.price)
                {
                    player.AddGold(-item.price);
                    if (item.isAbility)
                        player.LearnAbility(item.abilityType);
                    else
                        player.ApplyUpgrade(item.upgradeType);
                    item.purchased = true;
                    _dialogue      = "Pleasure doing business with you!";
                }
                else
                {
                    _dialogue = "I'm sorry, it seems you're a bit short on gold...";
                }
            }
        }
    }
    else
    {
        // Abilities tab — upgrade / remove rows
        const float rowH   = std::min(70.f, contentH / (float)std::max(1, player.GetLearnedCount()));
        const float btnW   = std::min(100.f, contentW * 0.28f);
        const float btnH   = std::min(28.f, rowH * 0.44f);
        const float btnGap = 8.f;

        for (int i = 0; i < player.GetLearnedCount(); i++)
        {
            AbilityType ab   = player.GetLearnedAbility(i);
            float       ry   = contentY + i * rowH;
            float       btnY2 = ry + rowH * 0.5f - btnH * 0.5f;

            // Upgrade button
            Rectangle upBtn = { shopX + iPad + contentW - btnW * 2.f - btnGap, btnY2, btnW, btnH };
            if (clicked && CheckCollisionPointRec(mouse, upBtn))
            {
                if (player.GetGold() >= 100)
                {
                    if (player.CanUpgradeAbility(ab))
                    {
                        UpgradeType ut = UpgradeType::Count;
                        switch (ab)
                        {
                        case AbilityType::FireSpread:     ut = UpgradeType::UpgradeFireSpread;     break;
                        case AbilityType::IceSpread:      ut = UpgradeType::UpgradeIceSpread;      break;
                        case AbilityType::ElectricSpread: ut = UpgradeType::UpgradeElectricSpread; break;
                        case AbilityType::FireBolt:       ut = UpgradeType::UpgradeFireBolt;       break;
                        case AbilityType::IceBolt:        ut = UpgradeType::UpgradeIceBolt;        break;
                        case AbilityType::ElectricBolt:   ut = UpgradeType::UpgradeElectricBolt;   break;
                        default: break;
                        }
                        if (ut != UpgradeType::Count)
                        {
                            player.AddGold(-100);
                            player.ApplyUpgrade(ut);
                            _dialogue = "Power well spent!";
                        }
                    }
                    else { _dialogue = "That ability is already at its peak."; }
                }
                else { _dialogue = "I'm sorry, it seems you're a bit short on gold..."; }
            }

            // Remove button
            Rectangle rmBtn = { shopX + iPad + contentW - btnW, btnY2, btnW, btnH };
            if (clicked && CheckCollisionPointRec(mouse, rmBtn))
            {
                if (player.GetGold() >= 100)
                {
                    player.AddGold(-100);
                    player.RemoveAbilityAtSlot(i);
                    _dialogue = "Consider it done.";
                }
                else { _dialogue = "I'm sorry, it seems you're a bit short on gold..."; }
            }
        }
    }

    return false;
}

void ShopManager::Draw(const Character& player) const
{
    const float sw  = (float)GetScreenWidth();
    const float sh  = (float)GetScreenHeight();
    const float pad = 16.f;

    // ── Scrolling checkerboard background ────────────────────────────────
    {
        const int   cell   = 80;
        const Color dark   = Color{ 22, 16, 10, 255 };
        const Color light  = Color{ 34, 24, 14, 255 };
        const int   period = cell * 2;
        float t    = (float)GetTime();
        int   offX = (int)fmodf(t * 22.f, (float)period);
        int   offY = (int)fmodf(t * 12.f, (float)period);
        int   phX  = offX / cell, phY = offY / cell;
        int   pxX  = offX % cell, pxY = offY % cell;
        for (int gy = -1; gy <= (int)(sh / cell) + 1; gy++)
            for (int gx = -1; gx <= (int)(sw / cell) + 1; gx++)
            {
                bool isDark = (((gx + phX) + (gy + phY)) % 2 + 2) % 2 == 0;
                DrawRectangle(gx * cell - pxX, gy * cell - pxY, cell, cell, isDark ? dark : light);
            }
    }

    // ── Layout ───────────────────────────────────────────────────────────
    static constexpr float kBorderDst_layout = 32.f;
    const float leftW  = sw * 0.30f;
    const float leaveH = 48.f;
    const float leaveY = sh - pad - leaveH;
    const float dialH  = std::max(sh * 0.12f, kBorderDst_layout * 2.f + 30.f);
    const float dialY  = leaveY - pad * 0.5f - dialH;
    const float shopX  = pad + leftW + pad;
    const float shopY  = pad;
    const float shopW  = sw - shopX - pad;
    const float shopH  = dialY - pad * 0.5f - shopY;

    static constexpr float kBorderSrc = 16.f;
    static constexpr float kBorderDst = 32.f;
    const float iPad = kBorderDst;

    const Color kGold       = BLACK;
    const Color kDim        = Color{160,170,190, 200};
    const Color kSlotBg     = Color{ 20, 20, 30, 160};
    const Color kSlotBgFull = Color{ 30, 40, 60, 200};

    // Main panel: 9-slice border texture or rounded rect fallback
    auto box = [&](Rectangle r, Color tint)
    {
        if (_tex.border && _tex.border->id != 0)
            DrawNineSlice(*_tex.border, kBorderSrc, kBorderDst, r, tint);
        else
        {
            DrawRectangleRounded(r, 0.06f, 8, Color{14,18,30,235});
            DrawRectangleRoundedLines(r, 0.06f, 8, Color{80,100,140,180});
        }
    };
    auto smallBox = [](Rectangle r, Color bg, Color border)
    {
        DrawRectangleRounded(r, 0.12f, 6, bg);
        DrawRectangleRoundedLines(r, 0.12f, 6, border);
    };

    // ── LEFT PANEL ───────────────────────────────────────────────────────
    const float lx = pad, ly = pad;
    const float lh = sh - pad * 2.f;
    box({ lx, ly, leftW, lh }, WHITE);

    BeginScissorMode((int)(lx + kBorderDst), (int)(ly + kBorderDst),
                     (int)(leftW - kBorderDst * 2.f), (int)(lh - kBorderDst * 2.f));
    {
        const float cp = iPad;
        float       cy = ly + cp;
        const float cw = leftW - cp * 2.f;

        // Title
        DrawText("PLAYER", (int)(lx + cp), (int)cy, 28, kGold);
        cy += 34.f;
        DrawLineEx({ lx + cp, cy }, { lx + leftW - cp, cy }, 1.f, Fade(kGold, 0.35f));
        cy += 12.f;

        // HP bar
        float maxHp = player.GetMaxHealthValue();
        float curHp = player.GetHealthValue();
        float hpPct = (maxHp > 0.f) ? curHp / maxHp : 0.f;
        const float barH = std::min(26.f, cw * 0.13f);
        Color hpCol = (hpPct > 0.5f) ? GREEN : (hpPct > 0.25f) ? YELLOW : RED;
        DrawRectangleRounded({ lx + cp, cy, cw * hpPct, barH }, 0.4f, 4, hpCol);
        DrawRectangleRoundedLines({ lx + cp, cy, cw, barH }, 0.4f, 4, Fade(WHITE, 0.2f));
        const char* hpLbl = TextFormat("HP  %.0f / %.0f", curHp, maxHp);
        int hpFs = (int)std::min(18.f, barH * 0.85f);
        DrawText(hpLbl, (int)(lx + cp + 4.f), (int)(cy + barH * 0.5f - hpFs * 0.5f), hpFs, RAYWHITE);
        cy += barH + 8.f;

        // MP bar
        float manaPct = (player.GetMaxMana() > 0) ? (float)player.GetMana() / player.GetMaxMana() : 0.f;
        DrawRectangleRounded({ lx + cp, cy, cw * manaPct, barH }, 0.4f, 4, Color{60,120,255,230});
        DrawRectangleRoundedLines({ lx + cp, cy, cw, barH }, 0.4f, 4, Fade(WHITE, 0.2f));
        const char* mpLbl = TextFormat("MP  %d / %d", player.GetMana(), player.GetMaxMana());
        DrawText(mpLbl, (int)(lx + cp + 4.f), (int)(cy + barH * 0.5f - hpFs * 0.5f), hpFs, RAYWHITE);
        cy += barH + 14.f;

        // Stats grid
        struct StatRow { const char* label; std::string value; };
        StatRow stats[] = {
            { "ATK",  TextFormat("%.1f", player.GetAttackPowerValue())   },
            { "SPD",  TextFormat("%.0f", player.GetMoveSpeedValue())     },
            { "DEF",  TextFormat("%.0f%%", player.GetDefense() * 100.f) },
            { "GOLD", std::to_string(player.GetGold())                   },
        };
        float statFs   = std::min(20.f, cw * 0.09f);
        float statRowH = statFs + 9.f;
        for (auto& s : stats)
        {
            DrawText(s.label, (int)(lx + cp), (int)cy, (int)statFs, kDim);
            int vw = MeasureText(s.value.c_str(), (int)statFs);
            DrawText(s.value.c_str(), (int)(lx + leftW - cp - vw), (int)cy, (int)statFs, RAYWHITE);
            cy += statRowH;
        }
        cy += 10.f;

        // Abilities section
        DrawLineEx({ lx + cp, cy }, { lx + leftW - cp, cy }, 1.f, Fade(kGold, 0.35f));
        cy += 10.f;
        DrawText("ABILITIES", (int)(lx + cp), (int)cy, 22, kGold);
        cy += 28.f;

        int   slotCount = player.GetMaxAbilitySlots();
        float slotH     = std::min(50.f, (lh - (cy - ly) - cp) / (float)slotCount);
        float slotFs    = std::min(18.f, slotH * 0.40f);

        for (int i = 0; i < slotCount; i++)
        {
            AbilityType ab = player.GetLearnedAbility(i);
            Rectangle sr   = { lx + cp, cy, cw, slotH - 4.f };
            Color sbg = (ab == AbilityType::None) ? kSlotBg     : kSlotBgFull;
            Color sbo = (ab == AbilityType::None) ? Color{50,50,60,120} : Color{80,110,160,200};
            smallBox(sr, sbg, sbo);

            if (ab != AbilityType::None)
            {
                DrawText(ShopAbilityName(ab),
                    (int)(sr.x + 8.f),
                    (int)(sr.y + slotH * 0.5f - slotFs * 0.5f - 2.f),
                    (int)std::min(18.f, slotFs + 2.f), RAYWHITE);
                int lv  = player.GetAbilityLevel(ab);
                const char* lvLbl = TextFormat("Lv%d", lv);
                int lvW = MeasureText(lvLbl, (int)slotFs);
                DrawText(lvLbl, (int)(sr.x + sr.width - lvW - 6.f),
                    (int)(sr.y + slotH * 0.5f - slotFs * 0.5f),
                    (int)slotFs, lv >= 3 ? GOLD : SKYBLUE);
            }
            else
            {
                DrawText("-- empty --", (int)(sr.x + 8.f),
                    (int)(sr.y + slotH * 0.5f - slotFs * 0.5f),
                    (int)slotFs, Fade(RAYWHITE, 0.30f));
            }
            cy += slotH;
        }
    }
    EndScissorMode();

    // ── RIGHT PANEL (SHOP) ────────────────────────────────────────────────
    box({ shopX, shopY, shopW, shopH }, WHITE);

    BeginScissorMode((int)(shopX + kBorderDst), (int)(shopY + kBorderDst),
                     (int)(shopW - kBorderDst * 2.f), (int)(shopH - kBorderDst * 2.f));
    {
        const float titleH = kBorderDst + 34.f;
        const float tabH   = 38.f;

        DrawText("ZEPH'S WARES", (int)(shopX + iPad), (int)(shopY + iPad), 28, kGold);
        DrawLineEx({ shopX + iPad, shopY + titleH - 4.f },
                   { shopX + shopW - iPad, shopY + titleH - 4.f },
                   1.f, Fade(kGold, 0.30f));

        const float tabW   = (shopW - iPad * 2.f) * 0.5f - 4.f;
        const float tabY   = shopY + titleH;
        Rectangle tabWares = { shopX + iPad,               tabY, tabW, tabH };
        Rectangle tabAb    = { shopX + iPad + tabW + 8.f,  tabY, tabW, tabH };

        auto drawTab = [&](Rectangle r, const char* label, bool active)
        {
            Color bg = active ? Color{40,60,110,240} : Color{20,25,40,180};
            Color bo = active ? Color{100,150,255,255} : Color{80,100,140,180};
            smallBox(r, bg, bo);
            int fs = (int)std::min(16.f, r.height * 0.44f);
            int tw = MeasureText(label, fs);
            DrawText(label,
                (int)(r.x + r.width  * 0.5f - tw * 0.5f),
                (int)(r.y + r.height * 0.5f - fs * 0.5f),
                fs, active ? RAYWHITE : kDim);
        };
        drawTab(tabWares, "WARES",     _tab == 0);
        drawTab(tabAb,    "ABILITIES", _tab == 1);

        const float contentY = tabY + tabH + iPad;
        const float contentH = shopH - titleH - tabH - iPad * 2.f;
        const float contentW = shopW - iPad * 2.f;

        // Icon lookup — maps item type to the appropriate texture pointer
        auto getShopIcon = [&](const ShopItem& si) -> Texture2D*
        {
            if (si.isAbility)
            {
                switch (si.abilityType)
                {
                case AbilityType::FireSpread:    case AbilityType::FireBolt:    case AbilityType::FireUltimate:
                    return _tex.abilityFire;
                case AbilityType::IceSpread:     case AbilityType::IceBolt:     case AbilityType::IceUltimate:
                    return _tex.abilityIce;
                case AbilityType::ElectricSpread: case AbilityType::ElectricBolt: case AbilityType::ElectricUltimate:
                    return _tex.abilityElectric;
                default: return nullptr;
                }
            }
            switch (si.upgradeType)
            {
            case UpgradeType::AttackPower: case UpgradeType::Ferocity:
            case UpgradeType::WarGod:      case UpgradeType::BladeStorm:
                return _tex.upgradeAttack;
            case UpgradeType::AttackRange: case UpgradeType::BladeEdge:
                return _tex.upgradeRange;
            case UpgradeType::MaxHealth:   case UpgradeType::IronConstitution:
            case UpgradeType::Resilience:  case UpgradeType::Juggernaut:
                return _tex.upgradeHealth;
            case UpgradeType::MaxMana:     case UpgradeType::ArcaneMind:
            case UpgradeType::ArcaneColossus:
                return _tex.upgradeMagic;
            case UpgradeType::Defense:     case UpgradeType::IronSkin:
                return _tex.upgradeDefense;
            case UpgradeType::MoveSpeed:   case UpgradeType::SwiftFeet:
                return _tex.upgradeSpeed;
            default: return nullptr;
            }
        };

        if (_tab == 0)
        {
            const float cols = 3.f, rows = 2.f, gap = 10.f;
            const float itemW = (contentW - gap * (cols - 1.f)) / cols;
            const float itemH = (contentH - gap * (rows - 1.f)) / rows;
            const float buyH   = std::min(30.f, itemH * 0.20f);
            const float iconSz = std::min(itemW * 0.40f, itemH * 0.35f);
            const float nameFs = std::min(32.f, itemH * 0.22f);
            const float descFsBase = std::min(26.f, itemH * 0.16f);

            if (_inventory.empty())
            {
                DrawText("Nothing left in stock.", (int)(shopX + iPad + 8.f),
                    (int)(contentY + 20.f), 18, kDim);
            }

            for (int idx = 0; idx < (int)_inventory.size(); idx++)
            {
                const ShopItem& item = _inventory[idx];
                if (item.purchased) continue;

                int   col = idx % 3, row = idx / 3;
                float ix  = shopX + iPad + col * (itemW + gap);
                float iy  = contentY + row * (itemH + gap);

                UpgradeRarity rar = item.isAbility
                    ? UpgradeRarity::Rare
                    : ShopUpgradeRarity(item.upgradeType);
                Color rarCol = ShopRarityColor(rar);
                Color cardBg = Color{18, 20, 32, 220};
                Color cardBo = Fade(rarCol, 0.55f);
                Vector2 mouse = GetMousePosition();
                bool    hov   = CheckCollisionPointRec(mouse, { ix, iy, itemW, itemH });
                if (hov) { cardBg = Color{28,32,52,240}; cardBo = Fade(rarCol, 0.90f); }
                smallBox({ ix, iy, itemW, itemH }, cardBg, cardBo);

                DrawRectangle((int)ix, (int)iy, 5, (int)itemH, Fade(rarCol, 0.80f));

                BeginScissorMode((int)ix + 6, (int)iy, (int)itemW - 6, (int)itemH);

                const float iconCX = ix + itemW * 0.5f;
                const float iconCY = iy + 8.f + iconSz * 0.5f;
                Texture2D* icon = getShopIcon(item);
                if (icon && icon->id != 0)
                {
                    float scale = std::min(iconSz / (float)icon->width,
                                          iconSz / (float)icon->height);
                    float iw = icon->width  * scale;
                    float ih = icon->height * scale;
                    DrawTexturePro(*icon,
                        { 0, 0, (float)icon->width, (float)icon->height },
                        { iconCX - iw * 0.5f, iconCY - ih * 0.5f, iw, ih },
                        {}, 0.f, WHITE);
                }
                else
                {
                    DrawCircleV({ iconCX, iconCY }, iconSz * 0.4f, Fade(rarCol, 0.35f));
                    DrawCircleLinesV({ iconCX, iconCY }, iconSz * 0.4f, Fade(rarCol, 0.70f));
                }

                float cy2 = iconCY + iconSz * 0.5f + 100.f;
                const char* name = item.isAbility
                    ? ShopAbilityName(item.abilityType)
                    : ShopUpgradeName(item.upgradeType);
                const char* desc = item.isAbility
                    ? ShopAbilityDesc(item.abilityType)
                    : ShopUpgradeDesc(item.upgradeType);

                int nw = MeasureText(name, (int)nameFs);
                DrawText(name, (int)(ix + itemW * 0.5f - nw * 0.5f + 3.f),
                    (int)cy2, (int)nameFs, RAYWHITE);
                cy2 += nameFs + 3.f;

                const float maxDescW = itemW - 20.f;
                float descFs = descFsBase;
                while (descFs > 8.f && MeasureText(desc, (int)descFs) > (int)maxDescW)
                    descFs -= 1.f;
                int dw = MeasureText(desc, (int)descFs);
                DrawText(desc, (int)(ix + itemW * 0.5f - dw * 0.5f + 3.f),
                    (int)cy2, (int)descFs, kDim);

                EndScissorMode();

                bool canAfford = (player.GetGold() >= item.price);
                Color buyBg = canAfford ? Color{30,90,30,220} : Color{60,30,30,180};
                Color buyBo = canAfford ? Color{80,200,80,255} : Color{160,60,60,200};
                Rectangle buyBtn = { ix + 4.f, iy + itemH - buyH - 4.f, itemW - 8.f, buyH };
                smallBox(buyBtn, buyBg, buyBo);
                int prFs = (int)std::min(14.f, buyH * 0.55f);
                const char* prLbl = TextFormat("%dg", item.price);
                int prW = MeasureText(prLbl, prFs);
                DrawText(prLbl,
                    (int)(buyBtn.x + buyBtn.width * 0.5f - prW * 0.5f),
                    (int)(buyBtn.y + buyBtn.height * 0.5f - prFs * 0.5f),
                    prFs, canAfford ? Color{180,255,180,255} : Color{255,140,140,220});
            }
        }
        else
        {
            if (player.GetLearnedCount() == 0)
            {
                DrawText("You haven't learned any abilities yet.",
                    (int)(shopX + iPad + 8.f), (int)(contentY + 20.f), 16, kDim);
            }
            else
            {
                const float rowH  = std::min(68.f, contentH / (float)player.GetLearnedCount());
                const float btnW  = std::min(100.f, contentW * 0.26f);
                const float btnH  = std::min(28.f, rowH * 0.45f);
                const float btnGp = 8.f;
                const float rowFs = std::min(16.f, rowH * 0.28f);

                for (int i = 0; i < player.GetLearnedCount(); i++)
                {
                    AbilityType ab = player.GetLearnedAbility(i);
                    float       ry = contentY + i * rowH;
                    float       btnY2 = ry + rowH * 0.5f - btnH * 0.5f;

                    smallBox({ shopX + iPad, ry + 2.f, contentW, rowH - 4.f },
                        Color{25,30,50,200}, Color{60,80,120,160});

                    DrawText(ShopAbilityName(ab),
                        (int)(shopX + iPad + 10.f),
                        (int)(ry + rowH * 0.5f - rowFs * 0.5f),
                        (int)rowFs, RAYWHITE);
                    int lv = player.GetAbilityLevel(ab);
                    DrawText(TextFormat("Lv %d", lv),
                        (int)(shopX + iPad + 10.f + MeasureText(ShopAbilityName(ab), (int)rowFs) + 10.f),
                        (int)(ry + rowH * 0.5f - rowFs * 0.5f),
                        (int)rowFs, lv >= 3 ? GOLD : SKYBLUE);

                    bool canUpg    = player.CanUpgradeAbility(ab);
                    bool canAfford = (player.GetGold() >= 100);

                    float upgX = shopX + iPad + contentW - btnW * 2.f - btnGp;
                    Color upgBg = (canUpg && canAfford) ? Color{30,60,100,220} : Color{30,30,40,140};
                    Color upgBo = (canUpg && canAfford) ? Color{80,140,255,255} : Color{60,60,80,160};
                    smallBox({ upgX, btnY2, btnW, btnH }, upgBg, upgBo);
                    int upFs = (int)std::min(13.f, btnH * 0.55f);
                    DrawText("Upgrade 100g",
                        (int)(upgX + btnW * 0.5f - MeasureText("Upgrade 100g", upFs) * 0.5f),
                        (int)(btnY2 + btnH * 0.5f - upFs * 0.5f),
                        upFs, (canUpg && canAfford) ? RAYWHITE : Fade(RAYWHITE, 0.35f));

                    float rmX = shopX + iPad + contentW - btnW;
                    Color rmBg = canAfford ? Color{90,25,25,200} : Color{40,20,20,140};
                    Color rmBo = canAfford ? Color{220,60,60,255} : Color{100,40,40,160};
                    smallBox({ rmX, btnY2, btnW, btnH }, rmBg, rmBo);
                    DrawText("Remove 100g",
                        (int)(rmX + btnW * 0.5f - MeasureText("Remove 100g", upFs) * 0.5f),
                        (int)(btnY2 + btnH * 0.5f - upFs * 0.5f),
                        upFs, canAfford ? Color{255,140,140,255} : Fade(RAYWHITE, 0.35f));
                }
            }
        }
    }
    EndScissorMode();

    // ── DIALOGUE BOX ─────────────────────────────────────────────────────
    box({ shopX, dialY, shopW, dialH }, WHITE);
    {
        const float innerTop = dialY + kBorderDst;
        const float innerH   = dialH - kBorderDst * 2.f;
        const float innerMid = innerTop + innerH * 0.5f;

        const char* nameTag = "Zeph:";
        int ntFs = (int)std::min(20.f, innerH * 0.55f);
        int dtFs = (int)std::min(18.f, innerH * 0.48f);

        float totalH = ntFs + 4.f + dtFs;
        float startY = innerMid - totalH * 0.5f;

        int ntW = MeasureText(nameTag, ntFs);
        DrawText(nameTag, (int)(shopX + iPad), (int)startY, ntFs, BLACK);

        const char* dText = _dialogue.c_str();
        DrawText(dText,
            (int)(shopX + iPad + ntW + 10.f),
            (int)(startY + ntFs * 0.5f - dtFs * 0.5f),
            dtFs, BLACK);
    }

    // ── REROLL + LEAVE BUTTONS ────────────────────────────────────────────
    const float leaveW  = 180.f;
    const float rerollW = 200.f;
    const float leaveX  = shopX + shopW * 0.5f + 8.f;
    const float rerollX = shopX + shopW * 0.5f - rerollW - 8.f;
    Rectangle leaveBtn  = { leaveX,  leaveY, leaveW,  leaveH };
    Rectangle rerollBtn = { rerollX, leaveY, rerollW, leaveH };

    Vector2 mpos = GetMousePosition();
    bool leaveHov  = CheckCollisionPointRec(mpos, leaveBtn);
    bool rerollHov = CheckCollisionPointRec(mpos, rerollBtn);
    bool canReroll = (player.GetGold() >= _rerollCost);

    smallBox(leaveBtn,
        leaveHov ? Color{80,20,20,240} : Color{50,14,14,220},
        leaveHov ? Color{220,80,80,255} : Color{140,50,50,200});
    int lvFs = (int)std::min(18.f, leaveH * 0.50f);
    int lvW  = MeasureText("LEAVE SHOP", lvFs);
    DrawText("LEAVE SHOP",
        (int)(leaveX + leaveW * 0.5f - lvW * 0.5f),
        (int)(leaveY + leaveH * 0.5f - lvFs * 0.5f),
        lvFs, leaveHov ? Color{255,160,160,255} : Color{220,120,120,220});

    Color rrBg = canReroll
        ? (rerollHov ? Color{20,60,20,240} : Color{14,40,14,220})
        : Color{30,30,30,180};
    Color rrBo = canReroll
        ? (rerollHov ? Color{80,220,80,255} : Color{50,140,50,200})
        : Color{80,80,80,160};
    smallBox(rerollBtn, rrBg, rrBo);
    const char* rrLabel = TextFormat("REROLL  %dg", _rerollCost);
    int rrFs = (int)std::min(18.f, leaveH * 0.50f);
    int rrW  = MeasureText(rrLabel, rrFs);
    DrawText(rrLabel,
        (int)(rerollX + rerollW * 0.5f - rrW * 0.5f),
        (int)(leaveY + leaveH * 0.5f - rrFs * 0.5f),
        rrFs, canReroll ? (rerollHov ? Color{180,255,180,255} : Color{140,220,140,220})
                        : Fade(RAYWHITE, 0.35f));
}

// ── Inventory ─────────────────────────────────────────────────────────────────

void ShopManager::GenerateInventory(const Character& player)
{
    _inventory.clear();

    static constexpr UpgradeType kStatUpgrades[] = {
        UpgradeType::AttackPower, UpgradeType::AttackRange,
        UpgradeType::MaxHealth,   UpgradeType::MaxMana,
        UpgradeType::Defense,     UpgradeType::MoveSpeed,
        UpgradeType::IronConstitution, UpgradeType::SwiftFeet,
        UpgradeType::Ferocity,    UpgradeType::ArcaneMind,
        UpgradeType::IronSkin,    UpgradeType::BladeEdge,
        UpgradeType::WarGod,      UpgradeType::Resilience,
        UpgradeType::BladeStorm,  UpgradeType::Juggernaut,
        UpgradeType::ArcaneColossus
    };
    static constexpr AbilityType kShopAbilities[] = {
        AbilityType::FireSpread,     AbilityType::IceSpread,     AbilityType::ElectricSpread,
        AbilityType::FireBolt,       AbilityType::IceBolt,       AbilityType::ElectricBolt
    };

    // Build unowned ability pool
    std::vector<ShopItem> abilityPool;
    for (auto a : kShopAbilities)
    {
        bool owned = false;
        for (int i = 0; i < player.GetLearnedCount(); i++)
            if (player.GetLearnedAbility(i) == a) { owned = true; break; }
        if (owned) continue;
        ShopItem item;
        item.isAbility   = true;
        item.abilityType = a;
        item.price       = ShopAbilityPrice(a);
        abilityPool.push_back(item);
    }

    // Shuffle ability pool, pick exactly 2 (or however many are available)
    for (int i = (int)abilityPool.size() - 1; i > 0; i--)
        std::swap(abilityPool[i], abilityPool[GetRandomValue(0, i)]);
    int abilitySlots = std::min((int)abilityPool.size(), 2);
    for (int i = 0; i < abilitySlots; i++)
        _inventory.push_back(abilityPool[i]);

    // Build stat pool and shuffle
    std::vector<ShopItem> statPool;
    for (auto u : kStatUpgrades)
    {
        ShopItem item;
        item.isAbility   = false;
        item.upgradeType = u;
        item.price       = ShopUpgradePrice(u);
        statPool.push_back(item);
    }
    for (int i = (int)statPool.size() - 1; i > 0; i--)
        std::swap(statPool[i], statPool[GetRandomValue(0, i)]);

    int statSlots = std::min((int)statPool.size(), 6 - abilitySlots);
    for (int i = 0; i < statSlots; i++)
        _inventory.push_back(statPool[i]);

    // Final shuffle so abilities don't always appear first
    for (int i = (int)_inventory.size() - 1; i > 0; i--)
        std::swap(_inventory[i], _inventory[GetRandomValue(0, i)]);
}
