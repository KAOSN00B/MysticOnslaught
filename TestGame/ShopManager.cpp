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
    case UpgradeType::AttackPower:      return "+1.0 Attack";
    case UpgradeType::AttackRange:      return "Slightly extends attack range";
    case UpgradeType::MaxHealth:        return "+2 Max HP";
    case UpgradeType::MaxMana:          return "+3 Max MP";
    case UpgradeType::Defense:          return "+1 Defense";
    case UpgradeType::MoveSpeed:        return "Slightly increases movement speed";
    case UpgradeType::IronConstitution: return "+4 Max HP";
    case UpgradeType::SwiftFeet:        return "Moderately increases movement speed";
    case UpgradeType::Ferocity:         return "+2.0 Attack";
    case UpgradeType::ArcaneMind:       return "+5 Max MP, +0.10 Regen";
    case UpgradeType::IronSkin:         return "+2 Defense";
    case UpgradeType::BladeEdge:        return "+1.0 Attack, Slightly extends range";
    case UpgradeType::WarGod:           return "+3.0 Attack, Greatly extends range";
    case UpgradeType::Resilience:       return "+6 Max HP (Heals 6)";
    case UpgradeType::BladeStorm:       return "+2.0 Attack, Greatly increases movement speed";
    case UpgradeType::Juggernaut:       return "+4 Max HP, +3 Defense";
    case UpgradeType::ArcaneColossus:   return "+5 Max MP, +2.0 Attack, +0.25 Regen";
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
    default:                    return 120;
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
    default:                    return Color{220,110,  20, 255};
    }
}

namespace
{
    constexpr float kZephSpriteHeight = 192.f;
    constexpr float kZephCollisionWidth = 72.f;
    constexpr float kZephCollisionHeight = 112.f;

    Rectangle GetZephPromptRect(float sx, float sy, bool touchMode)
    {
        const char* prompt = touchMode ? "Tap to Shop" : "[E] Shop";
        int fontSize = touchMode ? 20 : 18;
        int textW = MeasureText(prompt, fontSize);
        return Rectangle{
            sx - textW * 0.5f - 10.f,
            sy - kZephSpriteHeight * 0.5f - 54.f,
            (float)textW + 20.f,
            touchMode ? 32.f : 24.f
        };
    }
}

// ── Setup ─────────────────────────────────────────────────────────────────────

void ShopManager::Init(const ShopTextures& tex)
{
    _tex = tex;
}

void ShopManager::Enter(Vector2 npcWorldPos, Character& player, int act)
{
    _npcPos         = npcWorldPos;
    _nearNpc        = false;
    _touchPromptMode = false;
    _npcTouchHeld   = false;
    _tab            = 0;
    _rerollCost     = 20;
    _act            = std::max(1, act);
    _dialogue       = "Welcome to Zeph's Wares! What do you need?";
    GenerateInventory(player);
}

// ── Per-frame (Store room) ────────────────────────────────────────────────────

bool ShopManager::UpdateNpc(Character& player, Vector2 worldOffset, bool touchMode)
{
    _touchPromptMode = touchMode;
    // Collision push (MTV against a 40×60 world-unit box)
    Rectangle npcRect = {
        _npcPos.x - kZephCollisionWidth * 0.5f,
        _npcPos.y + kZephSpriteHeight * 0.5f - kZephCollisionHeight,
        kZephCollisionWidth,
        kZephCollisionHeight
    };
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
    _nearNpc   = (dist < 155.f);

    const float sw2 = GetScreenWidth()  * 0.5f;
    const float sh2 = GetScreenHeight() * 0.5f;
    float sx = _npcPos.x + worldOffset.x + sw2;
    float sy = _npcPos.y + worldOffset.y + sh2;
    Rectangle promptRect = GetZephPromptRect(sx, sy, touchMode);

    bool touchDown = touchMode && GetTouchPointCount() > 0;
    bool touchTap = touchDown && !_npcTouchHeld;
    _npcTouchHeld = touchDown;

    bool openPressed = false;
    if (_nearNpc)
    {
        if (!touchMode && IsKeyPressed(KEY_E))
            openPressed = true;
        else if (touchMode && touchTap)
        {
            Vector2 touchPos = GetTouchPosition(0);
            openPressed = CheckCollisionPointRec(touchPos, promptRect);
        }
    }

    if (openPressed)
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

    if (_tex.zephIdle && _tex.zephIdle->id > 0)
    {
        static constexpr int   kZephFrameCount = 6;
        static constexpr float kZephLoopSeconds = 3.f;
        const float frameTime = kZephLoopSeconds / (float)kZephFrameCount;
        int frame = (int)(fmod(GetTime(), kZephLoopSeconds) / frameTime);
        frame = std::max(0, std::min(kZephFrameCount - 1, frame));

        const float frameW = (float)_tex.zephIdle->width / (float)kZephFrameCount;
        const float frameH = (float)_tex.zephIdle->height;
        Rectangle src = { frame * frameW, 0.f, frameW, frameH };

        const float dstH = kZephSpriteHeight;
        const float dstW = dstH * (frameW / frameH);
        Rectangle dst = { sx - dstW * 0.5f, sy - dstH * 0.5f, dstW, dstH };
        DrawTexturePro(*_tex.zephIdle, src, dst, {}, 0.f, WHITE);
    }
    else
    {
        DrawRectangle((int)(sx - nW * 0.5f), (int)(sy - nH * 0.5f),
                      (int)nW, (int)nH, ORANGE);
        DrawRectangleLines((int)(sx - nW * 0.5f), (int)(sy - nH * 0.5f),
                           (int)nW, (int)nH, Color{200, 120, 0, 255});
    }

    const char* nameplate = "Zeph";
    int npFs = 16, npW = MeasureText(nameplate, npFs);
    DrawRectangle((int)(sx - npW * 0.5f - 5), (int)(sy - kZephSpriteHeight * 0.5f - 24),
                  npW + 10, 20, Fade(BLACK, 0.6f));
    DrawText(nameplate, (int)(sx - npW * 0.5f),
             (int)(sy - kZephSpriteHeight * 0.5f - 22), npFs, GOLD);

    if (_nearNpc)
    {
        const char* prompt = _touchPromptMode ? "Tap to Shop" : "[E] Shop";
        int prFs = _touchPromptMode ? 20 : 18;
        int prW = MeasureText(prompt, prFs);
        Rectangle promptRect = GetZephPromptRect(sx, sy, _touchPromptMode);
        DrawRectangleRounded(promptRect, 0.28f, 6, Fade(BLACK, 0.76f));
        DrawRectangleRoundedLines(promptRect, 0.28f, 6, Fade(GOLD, 0.55f));
        DrawText(prompt, (int)(sx - prW * 0.5f),
                 (int)(promptRect.y + promptRect.height * 0.5f - prFs * 0.5f), prFs, RAYWHITE);
    }
}

// ── Per-frame (GameState::Shop) ───────────────────────────────────────────────

bool ShopManager::Update(Character& player, bool debugActive)
{
    // ── UI Editor ────────────────────────────────────────────────────────────
    if (debugActive)
    {
        if (IsKeyPressed(KEY_NINE))
            _isUIEditorActive = !_isUIEditorActive;

        if (_isUIEditorActive)
        {
            constexpr int kVarCount = 22;
            if (IsKeyPressed(KEY_UP))
                _uiEditorSelectedIndex = (_uiEditorSelectedIndex - 1 + kVarCount) % kVarCount;
            if (IsKeyPressed(KEY_DOWN))
                _uiEditorSelectedIndex = (_uiEditorSelectedIndex + 1) % kVarCount;

            float* vars[] = {
                &_uiPad, &_uiLeftPanelW, &_uiTitleFs, &_uiStatFs, &_uiSlotFs,
                &_uiSlotBtnFs, &_uiHpFs, &_uiTabH, &_uiBuyBtnH,
                &_uiItemNameFs, &_uiItemDescFs, &_uiItemTextOffsetY, &_uiPriceFs,
                &_uiDialNameFs, &_uiDialTextFs, &_uiPotionH, &_uiPotionFs,
                &_uiAbilTitleFs, &_uiBtnH, &_uiLeaveW, &_uiRerollW, &_uiBtnFs
            };
            float step = (_uiEditorSelectedIndex == 1) ? 0.01f : 1.0f;
            if (IsKeyPressed(KEY_RIGHT)) *vars[_uiEditorSelectedIndex] += step;
            if (IsKeyPressed(KEY_LEFT))  *vars[_uiEditorSelectedIndex] -= step;

            if (IsKeyPressed(KEY_S))
            {
                TraceLog(LOG_INFO, "=== Shop UI Editor Export ===");
                TraceLog(LOG_INFO, "_uiPad             = %.2ff;", _uiPad);
                TraceLog(LOG_INFO, "_uiLeftPanelW      = %.2ff;", _uiLeftPanelW);
                TraceLog(LOG_INFO, "_uiTitleFs         = %.2ff;", _uiTitleFs);
                TraceLog(LOG_INFO, "_uiStatFs          = %.2ff;", _uiStatFs);
                TraceLog(LOG_INFO, "_uiSlotFs          = %.2ff;", _uiSlotFs);
                TraceLog(LOG_INFO, "_uiSlotBtnFs       = %.2ff;", _uiSlotBtnFs);
                TraceLog(LOG_INFO, "_uiHpFs            = %.2ff;", _uiHpFs);
                TraceLog(LOG_INFO, "_uiTabH            = %.2ff;", _uiTabH);
                TraceLog(LOG_INFO, "_uiBuyBtnH         = %.2ff;", _uiBuyBtnH);
                TraceLog(LOG_INFO, "_uiItemNameFs      = %.2ff;", _uiItemNameFs);
                TraceLog(LOG_INFO, "_uiItemDescFs      = %.2ff;", _uiItemDescFs);
                TraceLog(LOG_INFO, "_uiItemTextOffsetY = %.2ff;", _uiItemTextOffsetY);
                TraceLog(LOG_INFO, "_uiPriceFs         = %.2ff;", _uiPriceFs);
                TraceLog(LOG_INFO, "_uiDialNameFs      = %.2ff;", _uiDialNameFs);
                TraceLog(LOG_INFO, "_uiDialTextFs      = %.2ff;", _uiDialTextFs);
                TraceLog(LOG_INFO, "_uiPotionH         = %.2ff;", _uiPotionH);
                TraceLog(LOG_INFO, "_uiPotionFs        = %.2ff;", _uiPotionFs);
                TraceLog(LOG_INFO, "_uiAbilTitleFs     = %.2ff;", _uiAbilTitleFs);
                TraceLog(LOG_INFO, "_uiBtnH            = %.2ff;", _uiBtnH);
                TraceLog(LOG_INFO, "_uiLeaveW          = %.2ff;", _uiLeaveW);
                TraceLog(LOG_INFO, "_uiRerollW         = %.2ff;", _uiRerollW);
                TraceLog(LOG_INFO, "_uiBtnFs           = %.2ff;", _uiBtnFs);
            }

            return false;   // block all shop interaction while editor is open
        }
    }
    else
    {
        _isUIEditorActive = false;
    }

    const float sw  = (float)GetScreenWidth();
    const float sh  = (float)GetScreenHeight();
    const float pad = _uiPad;

    Vector2 mouse   = GetMousePosition();
    bool    clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    // ── Layout (must match Draw exactly) ─────────────────────────────────
    static constexpr float kBorderDst_u  = 32.f;
    static constexpr float kPotionPrice  = 25;
    const float leftW   = sw * _uiLeftPanelW;
    const float leaveH  = _uiBtnH;
    const float leaveY  = sh - pad - leaveH;
    const float potionY = leaveY - pad * 0.5f - _uiPotionH;
    const float dialH   = std::max(sh * 0.12f, kBorderDst_u * 2.f + 30.f);
    const float dialY   = potionY - pad * 0.5f - dialH;
    const float shopX   = pad + leftW + pad;
    const float shopY   = pad;
    const float shopW   = sw - shopX - pad;
    const float shopH   = dialY - pad * 0.5f - shopY;
    const float iPad    = kBorderDst_u;

    // ── Leave button ─────────────────────────────────────────────────────
    const float leaveW  = _uiLeaveW;
    const float rerollW = _uiRerollW;
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
            _rerollCost += 20;
            GenerateInventory(player);
            _dialogue = "Fresh stock, just for you!";
        }
        else
        {
            _dialogue = "You don't have enough gold for a reroll!";
        }
        return false;
    }

    // ── Potion buttons (fixed — not affected by rerolls) ──────────────────
    const float potBtnW  = (shopW - 8.f) * 0.5f;
    Rectangle hpotBtn = { shopX,                   potionY, potBtnW, _uiPotionH };
    Rectangle mpotBtn = { shopX + potBtnW + 8.f,   potionY, potBtnW, _uiPotionH };

    if (clicked && CheckCollisionPointRec(mouse, hpotBtn))
    {
        if (player.GetGold() >= (int)kPotionPrice)
        {
            player.AddGold(-(int)kPotionPrice);
            player.Heal((int)(player.GetMaxHealthValue() * 0.25f));
            _dialogue = "Drink up! That's 25% HP back.";
        }
        else { _dialogue = "You can't afford that right now."; }
        return false;
    }
    if (clicked && CheckCollisionPointRec(mouse, mpotBtn))
    {
        if (player.GetGold() >= (int)kPotionPrice)
        {
            player.AddGold(-(int)kPotionPrice);
            player.RestoreMana(player.GetMaxMana() / 2);
            _dialogue = "Your mana flows freely again.";
        }
        else { _dialogue = "You can't afford that right now."; }
        return false;
    }

    // ── Left panel ability slot buttons ──────────────────────────────────
    {
        const float lx = pad, ly = pad;
        const float lh = sh - pad * 2.f;
        const float cp = iPad;
        const float cw = leftW - cp * 2.f;
        // Mirror the cy progression from Draw() to land on the slot positions
        float cy = ly + cp + 34.f + 12.f;
        const float hpFs2 = _uiHpFs;
        cy += (hpFs2 + 8.f) + (hpFs2 + 14.f);
        const float statFs2   = _uiStatFs;
        const float statRowH2 = statFs2 + 9.f;
        cy += 3.f * statRowH2 + 10.f + 10.f + _uiAbilTitleFs + 6.f;

        const int slotCount = player.GetMaxAbilitySlots();
        int occupiedCount = 0;
        for (int i = 0; i < slotCount; i++)
            if (player.GetLearnedAbility(i) != AbilityType::None) occupiedCount++;

        const float btnH    = _uiSlotBtnFs + 12.f;
        const float btnGap  = 4.f;
        const float slotGap = 3.f;
        const float availH  = (ly + lh - cp) - cy;
        float slotH = (availH - (float)occupiedCount * (btnH + btnGap)
                               - (float)slotCount * slotGap) / (float)slotCount;
        slotH = std::max(30.f, slotH);
        const float btnW = (cw - 8.f) * 0.5f;

        for (int i = 0; i < slotCount; i++)
        {
            AbilityType ab = player.GetLearnedAbility(i);
            cy += slotH + slotGap;   // skip over the slot box (same as Draw)

            if (ab != AbilityType::None)
            {
                Rectangle upgBtn = { lx + cp,              cy, btnW, btnH };
                Rectangle remBtn = { lx + cp + btnW + 8.f, cy, btnW, btnH };
                int upgCost = player.GetAbilityLevel(ab) * 100;

                if (clicked && CheckCollisionPointRec(mouse, upgBtn))
                {
                    if (!player.CanUpgradeAbility(ab))
                        _dialogue = "That ability is already at its peak.";
                    else if (player.GetGold() < upgCost)
                        _dialogue = "Not enough gold to upgrade that.";
                    else
                    {
                        player.AddGold(-upgCost);
                        player.UpgradeAbility(ab);
                        _dialogue = "Power flows through you. A worthy investment!";
                    }
                    return false;
                }
                if (clicked && CheckCollisionPointRec(mouse, remBtn))
                {
                    if (player.GetGold() < 100)
                        _dialogue = "You need 100g to remove an ability.";
                    else
                    {
                        player.AddGold(-100);
                        player.RemoveAbilityAtSlot(i);
                        _dialogue = "That power is gone. Choose wisely next time.";
                    }
                    return false;
                }
                cy += btnH + btnGap;
            }
        }
    }

    // ── Tab buttons ───────────────────────────────────────────────────────
    const float titleH = 46.f;
    const float tabH   = _uiTabH;
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
        const float buyH   = _uiBuyBtnH;
        int displayIdx = 0;

        for (int idx = 0; idx < (int)_inventory.size(); idx++)
        {
            ShopItem& item = _inventory[idx];
            if (item.purchased || item.isAbility) continue;

            int   col  = displayIdx % 3;
            int   row  = displayIdx / 3;
            float ix   = shopX + iPad + col * (itemW + gap);
            float iy   = contentY + row * (itemH + gap);

            Rectangle cardRect = { ix, iy, itemW, itemH };
            if (clicked && CheckCollisionPointRec(mouse, cardRect))
            {
                if (item.isAbility && player.GetLearnedCount() >= 3)
                {
                    _dialogue = "You can only hold 3 abilities. Remove one first.";
                    return false;
                }
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
            displayIdx++;
        }
    }
    else
    {
        // 2 rows × 3 cols of purchasable ability items
        const float cols   = 3.f, rows = 2.f, gap = 10.f;
        const float itemW  = (contentW - gap * (cols - 1.f)) / cols;
        const float itemH  = (contentH - gap * (rows - 1.f)) / rows;
        const float buyH   = _uiBuyBtnH;
        int displayIdx = 0;

        for (int idx = 0; idx < (int)_inventory.size(); idx++)
        {
            ShopItem& item = _inventory[idx];
            if (item.purchased || !item.isAbility) continue;

            int   col  = displayIdx % 3;
            int   row  = displayIdx / 3;
            float ix   = shopX + iPad + col * (itemW + gap);
            float iy   = contentY + row * (itemH + gap);

            Rectangle cardRect = { ix, iy, itemW, itemH };
            if (clicked && CheckCollisionPointRec(mouse, cardRect))
            {
                if (player.GetLearnedCount() >= 3)
                {
                    _dialogue = "You can only hold 3 abilities. Remove one first.";
                }
                else if (player.GetGold() >= item.price)
                {
                    player.AddGold(-item.price);
                    player.LearnAbility(item.abilityType);
                    item.purchased = true;
                    _dialogue = "A fine choice. Put it to good use!";
                }
                else
                {
                    _dialogue = "I'm sorry, it seems you're a bit short on gold...";
                }
            }
            displayIdx++;
        }
    }

    return false;
}

void ShopManager::Draw(const Character& player, bool debugActive) const
{
    const float sw  = (float)GetScreenWidth();
    const float sh  = (float)GetScreenHeight();
    const float pad = _uiPad;

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
    static constexpr int   kPotionPriceDraw  = 25;
    const float leftW   = sw * _uiLeftPanelW;
    const float leaveH  = _uiBtnH;
    const float leaveY  = sh - pad - leaveH;
    const float potionY = leaveY - pad * 0.5f - _uiPotionH;
    const float dialH   = std::max(sh * 0.12f, kBorderDst_layout * 2.f + 30.f);
    const float dialY   = potionY - pad * 0.5f - dialH;
    const float shopX   = pad + leftW + pad;
    const float shopY   = pad;
    const float shopW   = sw - shopX - pad;
    const float shopH   = dialY - pad * 0.5f - shopY;

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
        DrawText("PLAYER", (int)(lx + cp), (int)cy, (int)_uiTitleFs, kGold);
        cy += 34.f;
        DrawLineEx({ lx + cp, cy }, { lx + leftW - cp, cy }, 1.f, Fade(kGold, 0.35f));
        cy += 12.f;

        // HP text
        float maxHp = player.GetMaxHealthValue();
        float curHp = player.GetHealthValue();
        int hpFs = (int)_uiHpFs;
        const char* hpLbl = TextFormat("HP  %.0f / %.0f", curHp, maxHp);
        DrawText(hpLbl, (int)(lx + cp + 4.f), (int)cy, hpFs, BLACK);
        cy += hpFs + 8.f;

        // MP text
        const char* mpLbl = TextFormat("MP  %d / %d", player.GetMana(), player.GetMaxMana());
        DrawText(mpLbl, (int)(lx + cp + 4.f), (int)cy, hpFs, BLACK);
        cy += hpFs + 14.f;

        // Stats grid
        struct StatRow { const char* label; std::string value; };
        StatRow stats[] = {
            { "ATK",  TextFormat("%.1f", player.GetAttackPowerValue())  },
            { "DEF",  TextFormat("%.0f", player.GetDefense())           },
            { "GOLD", std::to_string(player.GetGold())                  },
        };
        float statFs   = _uiStatFs;
        float statRowH = statFs + 9.f;
        for (auto& s : stats)
        {
            DrawText(s.label, (int)(lx + cp + 4.f), (int)cy, (int)statFs, BLACK);
            int vw = MeasureText(s.value.c_str(), (int)statFs);
            DrawText(s.value.c_str(), (int)(lx + leftW - cp - vw - 18.f), (int)cy, (int)statFs, BLACK);
            cy += statRowH;
        }
        cy += 10.f;

        // Abilities section
        DrawLineEx({ lx + cp, cy }, { lx + leftW - cp, cy }, 1.f, Fade(kGold, 0.35f));
        cy += 10.f;
        DrawText("ABILITIES", (int)(lx + cp), (int)cy, (int)_uiAbilTitleFs, kGold);
        cy += _uiAbilTitleFs + 6.f;

        int slotCount = player.GetMaxAbilitySlots();

        // Count occupied slots to reserve space for the external button rows
        int occupiedCount = 0;
        for (int i = 0; i < slotCount; i++)
            if (player.GetLearnedAbility(i) != AbilityType::None) occupiedCount++;

        const float btnH    = _uiSlotBtnFs + 12.f;
        const float btnGap  = 4.f;
        const float slotGap = 3.f;
        const float availH  = (ly + lh - cp) - cy;
        float slotH = (availH - (float)occupiedCount * (btnH + btnGap)
                               - (float)slotCount * slotGap) / (float)slotCount;
        slotH = std::max(30.f, slotH);

        float slotFs = _uiSlotFs;
        const float btnW = (cw - 8.f) * 0.5f;
        Vector2 mpLeft = GetMousePosition();

        for (int i = 0; i < slotCount; i++)
        {
            AbilityType ab = player.GetLearnedAbility(i);
            int lv = (ab != AbilityType::None) ? player.GetAbilityLevel(ab) : 0;

            // ── Slot box ──────────────────────────────────────────────────
            Rectangle sr = { lx + cp, cy, cw, slotH };
            Color sbg = (ab == AbilityType::None) ? kSlotBg     : kSlotBgFull;
            Color sbo = (ab == AbilityType::None) ? Color{50,50,60,120} : Color{80,110,160,200};
            smallBox(sr, sbg, sbo);

            if (ab != AbilityType::None)
            {
                const char* abilName = ShopAbilityName(ab);
                const char* lvLbl    = TextFormat("Lv%d", lv);
                const float padX     = 8.f;
                const float lblGap   = 10.f;

                // Level font — clamp to slot height so it never spills vertically
                float lvFs = std::min(slotFs, slotH - 8.f);
                lvFs = std::max(8.f, lvFs);
                int lvW = MeasureText(lvLbl, (int)lvFs);

                // Ability name — clamp height first, then shrink width to fit
                // alongside the level label without overlapping
                float availNameW = cw - padX - (float)lvW - lblGap - padX;
                float nameFs = std::min(_uiSlotFs, slotH - 8.f);
                nameFs = std::max(8.f, nameFs);
                while (nameFs > 8.f && MeasureText(abilName, (int)nameFs) > (int)availNameW)
                    nameFs -= 1.f;

                // Clip to slot rect so nothing bleeds into neighbouring slots
                BeginScissorMode((int)sr.x, (int)sr.y, (int)sr.width, (int)sr.height);

                DrawText(abilName,
                    (int)(sr.x + padX),
                    (int)(sr.y + slotH * 0.5f - nameFs * 0.5f),
                    (int)nameFs, RAYWHITE);
                DrawText(lvLbl,
                    (int)(sr.x + sr.width - lvW - padX),
                    (int)(sr.y + slotH * 0.5f - lvFs * 0.5f),
                    (int)lvFs, lv >= 3 ? GOLD : SKYBLUE);

                EndScissorMode();

                // Restore the outer left-panel scissor
                BeginScissorMode((int)(lx + kBorderDst), (int)(ly + kBorderDst),
                                 (int)(leftW - kBorderDst * 2.f), (int)(lh - kBorderDst * 2.f));
            }
            else
            {
                DrawText("-- empty --",
                    (int)(sr.x + 8.f),
                    (int)(sr.y + slotH * 0.5f - slotFs * 0.5f),
                    (int)slotFs, Fade(RAYWHITE, 0.30f));
            }
            cy += slotH + slotGap;

            // ── Upg / Rem buttons — only when slot is occupied ────────────
            if (ab != AbilityType::None)
            {
                int  upgCost   = lv * 100;
                bool canUpg    = player.CanUpgradeAbility(ab);
                bool canAffUpg = player.GetGold() >= upgCost;
                bool canAffRem = player.GetGold() >= 100;
                int  ubFs      = (int)_uiSlotBtnFs;

                Rectangle upgBtn = { lx + cp,              cy, btnW, btnH };
                Rectangle remBtn = { lx + cp + btnW + 8.f, cy, btnW, btnH };

                bool upgHov = CheckCollisionPointRec(mpLeft, upgBtn);
                Color upgBg = (canUpg && canAffUpg) ? (upgHov ? Color{45,90,45,240} : Color{30,60,30,220})
                                                    : Color{25,25,30,140};
                Color upgBo = (canUpg && canAffUpg) ? (upgHov ? Color{120,220,120,255} : Color{80,180,80,220})
                                                    : Color{60,60,70,120};
                smallBox(upgBtn, upgBg, upgBo);
                const char* upgLbl = canUpg ? TextFormat("Upg %dg", upgCost) : "MAX";
                int ulW = MeasureText(upgLbl, ubFs);
                DrawText(upgLbl,
                    (int)(upgBtn.x + upgBtn.width  * 0.5f - ulW * 0.5f),
                    (int)(upgBtn.y + upgBtn.height * 0.5f - ubFs * 0.5f),
                    ubFs, (canUpg && canAffUpg) ? Color{180,255,180,255} : Fade(RAYWHITE, 0.35f));

                bool remHov = CheckCollisionPointRec(mpLeft, remBtn);
                Color remBg = canAffRem ? (remHov ? Color{110,30,30,240} : Color{70,20,20,200})
                                        : Color{25,25,30,140};
                Color remBo = canAffRem ? (remHov ? Color{255,80,80,255} : Color{200,60,60,200})
                                        : Color{60,60,70,120};
                smallBox(remBtn, remBg, remBo);
                const char* remLbl = "Rem 100g";
                int rlW = MeasureText(remLbl, ubFs);
                DrawText(remLbl,
                    (int)(remBtn.x + remBtn.width  * 0.5f - rlW * 0.5f),
                    (int)(remBtn.y + remBtn.height * 0.5f - ubFs * 0.5f),
                    ubFs, canAffRem ? Color{255,140,140,255} : Fade(RAYWHITE, 0.35f));

                cy += btnH + btnGap;
            }
        }

        // ── Player sprite portrait ────────────────────────────────────────
        {
            const Texture2D& idleTex = player.GetIdleAnim();
            float frameW = player.GetSpriteWidth();
            if (idleTex.id > 0 && frameW > 0.f)
            {
                int   nFrames = (int)(idleTex.width / frameW);
                float frameH  = (float)idleTex.height;

                const float kFps = (float)nFrames / 3.0f;
                int animFrame = (int)(fmod(GetTime() * kFps, (float)nFrames));
                animFrame = std::max(0, std::min(nFrames - 1, animFrame));
                Rectangle src = { animFrame * frameW, 0.f, frameW, frameH };

                const float spritePad = 8.f;
                float areaX = lx + cp;
                float areaY = cy + spritePad;
                float areaW = cw;
                float areaH = (ly + lh - cp) - areaY;

                if (areaH > 40.f)
                {
                    float scale = std::min(areaW / frameW, areaH / frameH);
                    float dstW  = frameW * scale;
                    float dstH  = frameH * scale;
                    Rectangle dst = {
                        areaX + (areaW - dstW) * 0.5f,
                        areaY + (areaH - dstH) * 0.5f,
                        dstW, dstH
                    };
                    DrawTexturePro(idleTex, src, dst, {}, 0.f, WHITE);
                }
            }
        }
    }
    EndScissorMode();

    // ── RIGHT PANEL (SHOP) ────────────────────────────────────────────────
    box({ shopX, shopY, shopW, shopH }, WHITE);

    BeginScissorMode((int)(shopX + kBorderDst), (int)(shopY + kBorderDst),
                     (int)(shopW - kBorderDst * 2.f), (int)(shopH - kBorderDst * 2.f));
    {
        const float titleH = kBorderDst + 34.f;
        const float tabH   = _uiTabH;

        DrawText("ZEPH'S WARES", (int)(shopX + iPad + 8.f), (int)(shopY + iPad + 4.f), (int)_uiTitleFs, kGold);
        DrawLineEx({ shopX + iPad, shopY + titleH - 4.f },
                   { shopX + shopW - iPad, shopY + titleH - 4.f },
                   1.f, Fade(kGold, 0.30f));

        const float tabW   = (shopW - iPad * 2.f) * 0.5f - 4.f;
        const float tabY   = shopY + titleH;
        Rectangle tabWares = { shopX + iPad,               tabY, tabW, tabH };
        Rectangle tabAb    = { shopX + iPad + tabW + 8.f,  tabY, tabW, tabH };

        auto drawTab = [&](Rectangle r, const char* label, bool active)
        {
            Vector2 mTab = GetMousePosition();
            bool hov = CheckCollisionPointRec(mTab, r);
            Color bg = active ? Color{40,60,110,240} : (hov ? Color{30,38,65,220} : Color{20,25,40,180});
            Color bo = active ? Color{100,150,255,255} : (hov ? Color{120,150,220,240} : Color{80,100,140,180});
            smallBox(r, bg, bo);
            float fs = std::min(_uiStatFs, r.height - 8.f);
            fs = std::max(8.f, fs);
            while (fs > 8.f && MeasureText(label, (int)fs) > (int)(r.width - 16.f))
                fs -= 1.f;
            int tw = MeasureText(label, (int)fs);
            DrawText(label,
                (int)(r.x + r.width  * 0.5f - tw * 0.5f),
                (int)(r.y + r.height * 0.5f - fs * 0.5f),
                (int)fs, active ? RAYWHITE : (hov ? Color{200,210,230,255} : kDim));
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
            const float buyH   = _uiBuyBtnH;
            const float iconSz = std::min(itemW * 0.40f, itemH * 0.35f);
            const float nameFs = _uiItemNameFs;
            const float descFsBase = _uiItemDescFs;
            int availableWares = 0;
            for (const auto& item : _inventory)
                if (!item.purchased && !item.isAbility)
                    availableWares++;

            if (availableWares == 0)
            {
                DrawText("Nothing left in stock.", (int)(shopX + iPad + 8.f),
                    (int)(contentY + 20.f), 18, kDim);
            }

            int displayIdx = 0;
            for (int idx = 0; idx < (int)_inventory.size(); idx++)
            {
                const ShopItem& item = _inventory[idx];
                if (item.purchased || item.isAbility) continue;

                int   col = displayIdx % 3, row = displayIdx / 3;
                float ix  = shopX + iPad + col * (itemW + gap);
                float iy  = contentY + row * (itemH + gap);

                UpgradeRarity rar = ShopUpgradeRarity(item.upgradeType);
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

                float cy2 = iconCY + iconSz * 0.5f + _uiItemTextOffsetY;
                const char* name = ShopUpgradeName(item.upgradeType);
                const char* desc = ShopUpgradeDesc(item.upgradeType);

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

                // Daily deal badge
                if (idx == _dailyDealIndex)
                {
                    const char* dealLbl = "DEAL!";
                    int dlFs = 13, dlW = MeasureText(dealLbl, dlFs);
                    DrawRectangle((int)(ix + itemW - dlW - 10.f), (int)iy, dlW + 10, 20,
                        Color{255, 200, 0, 230});
                    DrawText(dealLbl, (int)(ix + itemW - dlW - 5.f), (int)(iy + 3.f),
                        dlFs, BLACK);
                }

                EndScissorMode();

                bool canAfford = (player.GetGold() >= item.price);
                Color buyBg = canAfford ? Color{30,90,30,220} : Color{60,30,30,180};
                Color buyBo = canAfford ? Color{80,200,80,255} : Color{160,60,60,200};
                Rectangle buyBtn = { ix + 4.f, iy + itemH - buyH - 4.f, itemW - 8.f, buyH };
                smallBox(buyBtn, buyBg, buyBo);
                int prFs = (int)_uiPriceFs;
                const char* prLbl = (idx == _dailyDealIndex)
                    ? TextFormat("%dg  DEAL", item.price)
                    : TextFormat("%dg", item.price);
                int prW = MeasureText(prLbl, prFs);
                DrawText(prLbl,
                    (int)(buyBtn.x + buyBtn.width * 0.5f - prW * 0.5f),
                    (int)(buyBtn.y + buyBtn.height * 0.5f - prFs * 0.5f),
                    prFs, canAfford ? Color{180,255,180,255} : Color{255,140,140,220});
                displayIdx++;
            }
        }
        else
        {
            // 2 rows × 3 cols of purchasable abilities (mirrors wares tab layout)
            const float cols = 3.f, rows = 2.f, gap = 10.f;
            const float itemW = (contentW - gap * (cols - 1.f)) / cols;
            const float itemH = (contentH - gap * (rows - 1.f)) / rows;
            const float buyH   = _uiBuyBtnH;
            const float iconSz = std::min(itemW * 0.40f, itemH * 0.35f);
            const float nameFs = _uiItemNameFs;
            const float descFsBase = _uiItemDescFs;
            bool slotsFull = (player.GetLearnedCount() >= 3);

            int availableAbilities = 0;
            for (const auto& item : _inventory)
                if (!item.purchased && item.isAbility)
                    availableAbilities++;

            if (availableAbilities == 0)
            {
                DrawText("No abilities available.", (int)(shopX + iPad + 8.f),
                    (int)(contentY + 20.f), 18, kDim);
            }

            int displayIdx = 0;
            for (int idx = 0; idx < (int)_inventory.size(); idx++)
            {
                const ShopItem& item = _inventory[idx];
                if (item.purchased || !item.isAbility) continue;

                int   col = displayIdx % 3, row = displayIdx / 3;
                float ix  = shopX + iPad + col * (itemW + gap);
                float iy  = contentY + row * (itemH + gap);

                Color cardBg = Color{18, 20, 32, 220};
                Color cardBo = Color{80, 60, 140, 140};
                Vector2 mpos = GetMousePosition();
                bool    hov  = CheckCollisionPointRec(mpos, { ix, iy, itemW, itemH });
                if (hov && !slotsFull) { cardBg = Color{28,24,52,240}; cardBo = Color{120,80,200,220}; }
                smallBox({ ix, iy, itemW, itemH }, cardBg, cardBo);

                DrawRectangle((int)ix, (int)iy, 5, (int)itemH, Fade(Color{120,80,220,255}, 0.80f));

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
                    DrawCircleV({ iconCX, iconCY }, iconSz * 0.4f, Fade(Color{120,80,220,255}, 0.35f));
                    DrawCircleLinesV({ iconCX, iconCY }, iconSz * 0.4f, Fade(Color{120,80,220,255}, 0.70f));
                }

                float cy2 = iconCY + iconSz * 0.5f + 10.f;
                const char* name = ShopAbilityName(item.abilityType);
                const char* desc = ShopAbilityDesc(item.abilityType);

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
                bool canBuy    = canAfford && !slotsFull;
                Color buyBg = canBuy ? Color{30,90,30,220} : Color{60,30,30,180};
                Color buyBo = canBuy ? Color{80,200,80,255} : Color{160,60,60,200};
                Rectangle buyBtn = { ix + 4.f, iy + itemH - buyH - 4.f, itemW - 8.f, buyH };
                smallBox(buyBtn, buyBg, buyBo);
                int prFs = (int)_uiPriceFs;
                const char* prLbl = slotsFull ? "SLOTS FULL" : TextFormat("%dg", item.price);
                int prW = MeasureText(prLbl, prFs);
                DrawText(prLbl,
                    (int)(buyBtn.x + buyBtn.width * 0.5f - prW * 0.5f),
                    (int)(buyBtn.y + buyBtn.height * 0.5f - prFs * 0.5f),
                    prFs, canBuy ? Color{180,255,180,255} : Color{255,140,140,220});
                displayIdx++;
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
        int ntFs = (int)_uiDialNameFs;
        int dtFs = (int)_uiDialTextFs;

        float totalH = ntFs + 4.f + dtFs;
        float startY = innerMid - totalH * 0.5f;

        int ntW = MeasureText(nameTag, ntFs);
        DrawText(nameTag, (int)(shopX + iPad + 8.f), (int)(startY + 4.f), ntFs, BLACK);

        const char* dText = _dialogue.c_str();
        DrawText(dText,
            (int)(shopX + iPad + 8.f + ntW + 10.f),
            (int)(startY + 4.f + ntFs * 0.5f - dtFs * 0.5f),
            dtFs, BLACK);
    }

    // ── POTION STRIP ─────────────────────────────────────────────────────
    {
        const float potBtnW  = (shopW - 8.f) * 0.5f;
        bool canAffordPot    = (player.GetGold() >= kPotionPriceDraw);
        Vector2 mpos2        = GetMousePosition();

        auto drawPotBtn = [&](Rectangle r, const char* label, Color baseBg, Color hovBg, Color border)
        {
            bool hov = CheckCollisionPointRec(mpos2, r);
            Color bg = canAffordPot ? (hov ? hovBg : baseBg) : Color{25,25,30,180};
            Color bo = canAffordPot ? border : Color{70,70,80,140};
            smallBox(r, bg, bo);
            int fs = (int)_uiPotionFs;
            int lw = MeasureText(label, fs);
            DrawText(label,
                (int)(r.x + r.width * 0.5f - lw * 0.5f),
                (int)(r.y + r.height * 0.5f - fs * 0.5f),
                fs, canAffordPot ? RAYWHITE : Fade(RAYWHITE, 0.35f));
        };

        drawPotBtn({ shopX,                   potionY, potBtnW, _uiPotionH },
            TextFormat("Health Potion  %dg   (Heals 25%% HP)", kPotionPriceDraw),
            Color{55,18,18,220}, Color{80,25,25,240}, Color{220,70,70,220});

        drawPotBtn({ shopX + potBtnW + 8.f,   potionY, potBtnW, _uiPotionH },
            TextFormat("Mana Potion  %dg   (Restores 50%% MP)", kPotionPriceDraw),
            Color{18,25,65,220}, Color{25,38,95,240}, Color{80,120,255,220});
    }

    // ── REROLL + LEAVE BUTTONS ────────────────────────────────────────────
    const float leaveW  = _uiLeaveW;
    const float rerollW = _uiRerollW;
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
    {
        // Leave — shrink font until label fits inside the button
        float lFs = std::min(_uiBtnFs, leaveH - 8.f);
        lFs = std::max(8.f, lFs);
        while (lFs > 8.f && MeasureText("LEAVE SHOP", (int)lFs) > (int)(leaveW - 16.f))
            lFs -= 1.f;
        int lW = MeasureText("LEAVE SHOP", (int)lFs);
        DrawText("LEAVE SHOP",
            (int)(leaveX + leaveW * 0.5f - lW * 0.5f),
            (int)(leaveY + leaveH * 0.5f - lFs * 0.5f),
            (int)lFs, leaveHov ? Color{255,160,160,255} : Color{220,120,120,220});
    }

    Color rrBg = canReroll
        ? (rerollHov ? Color{20,60,20,240} : Color{14,40,14,220})
        : Color{30,30,30,180};
    Color rrBo = canReroll
        ? (rerollHov ? Color{80,220,80,255} : Color{50,140,50,200})
        : Color{80,80,80,160};
    smallBox(rerollBtn, rrBg, rrBo);
    {
        // Reroll — shrink font until label fits inside the button
        const char* rrLabel = TextFormat("REROLL  %dg", _rerollCost);
        float rFs = std::min(_uiBtnFs, leaveH - 8.f);
        rFs = std::max(8.f, rFs);
        while (rFs > 8.f && MeasureText(rrLabel, (int)rFs) > (int)(rerollW - 16.f))
            rFs -= 1.f;
        int rW = MeasureText(rrLabel, (int)rFs);
        DrawText(rrLabel,
            (int)(rerollX + rerollW * 0.5f - rW * 0.5f),
            (int)(leaveY + leaveH * 0.5f - rFs * 0.5f),
            (int)rFs, canReroll ? (rerollHov ? Color{180,255,180,255} : Color{140,220,140,220})
                                : Fade(RAYWHITE, 0.35f));
    }

    // ── UI Editor debug panel ─────────────────────────────────────────────
    if (debugActive && _isUIEditorActive)
    {
        const char* varNames[22] = {
            "0  Padding",
            "1  Left Panel W",
            "2  Title Font",
            "3  Stat Font",
            "4  Slot Font",
            "5  Slot Btn Font",
            "6  HP/MP Font",
            "7  Tab Height",
            "8  Buy Btn H",
            "9  Item Name Font",
            "10 Item Desc Font",
            "11 Item Text Offset Y",
            "12 Price Font",
            "13 Dial Name Font",
            "14 Dial Text Font",
            "15 Potion Height",
            "16 Potion Font",
            "17 Abil Title Font",
            "18 Btn Height",
            "19 Leave Width",
            "20 Reroll Width",
            "21 Btn Font",
        };
        const float* varPtrs[22] = {
            &_uiPad, &_uiLeftPanelW, &_uiTitleFs, &_uiStatFs, &_uiSlotFs,
            &_uiSlotBtnFs, &_uiHpFs, &_uiTabH, &_uiBuyBtnH,
            &_uiItemNameFs, &_uiItemDescFs, &_uiItemTextOffsetY, &_uiPriceFs,
            &_uiDialNameFs, &_uiDialTextFs, &_uiPotionH, &_uiPotionFs,
            &_uiAbilTitleFs, &_uiBtnH, &_uiLeaveW, &_uiRerollW, &_uiBtnFs
        };

        constexpr float panW = 300.f, panH = 700.f;
        const float panX = sw * 0.5f - panW * 0.5f;
        const float panY = 10.f;
        DrawRectangle((int)panX, (int)panY, (int)panW, (int)panH, Fade(BLACK, 0.82f));
        DrawRectangleLines((int)panX, (int)panY, (int)panW, (int)panH, DARKGRAY);

        DrawText("UI EDITOR  [9] close", (int)(panX + 8.f), (int)(panY + 6.f), 11, GRAY);
        DrawText("[UP/DOWN] select  [L/R] nudge  [S] export",
            (int)(panX + 8.f), (int)(panY + 20.f), 10, DARKGRAY);

        const float rowH  = (panH - 40.f) / 22.f;
        for (int i = 0; i < 22; i++)
        {
            float ry = panY + 38.f + i * rowH;
            bool  sel = (i == _uiEditorSelectedIndex);
            Color col = sel ? YELLOW : WHITE;

            if (sel)
                DrawText("->", (int)(panX + 4.f), (int)(ry + rowH * 0.5f - 8.f), 14, YELLOW);

            DrawText(varNames[i],
                (int)(panX + 28.f), (int)(ry + rowH * 0.5f - 8.f), 14, col);

            const char* valStr = TextFormat("%.2f", *varPtrs[i]);
            int valW = MeasureText(valStr, 14);
            DrawText(valStr,
                (int)(panX + panW - valW - 8.f), (int)(ry + rowH * 0.5f - 8.f), 14, col);
        }
    }
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

    // Price inflation: 25% per act above Act 1 (Act 1 = 1.0×, Act 2 = 1.25×, ...)
    const float inflation = 1.0f + 0.25f * (float)(_act - 1);

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
        item.price       = (int)(ShopAbilityPrice(a) * inflation + 0.5f);
        abilityPool.push_back(item);
    }

    // Keep every learnable non-ultimate attack in the shop's ability pool.
    for (int i = (int)abilityPool.size() - 1; i > 0; i--)
        std::swap(abilityPool[i], abilityPool[GetRandomValue(0, i)]);
    for (const auto& item : abilityPool)
        _inventory.push_back(item);

    // Build stat pool and shuffle
    std::vector<ShopItem> statPool;
    for (auto u : kStatUpgrades)
    {
        ShopItem item;
        item.isAbility   = false;
        item.upgradeType = u;
        item.price       = (int)(ShopUpgradePrice(u) * inflation + 0.5f);
        statPool.push_back(item);
    }
    for (int i = (int)statPool.size() - 1; i > 0; i--)
        std::swap(statPool[i], statPool[GetRandomValue(0, i)]);

    int statSlots = std::min((int)statPool.size(), 6);
    for (int i = 0; i < statSlots; i++)
        _inventory.push_back(statPool[i]);

    // Final shuffle so abilities don't always appear first
    for (int i = (int)_inventory.size() - 1; i > 0; i--)
        std::swap(_inventory[i], _inventory[GetRandomValue(0, i)]);

    // Daily deal: one random item gets 25% off
    _dailyDealIndex = -1;
    if (!_inventory.empty())
    {
        _dailyDealIndex = GetRandomValue(0, (int)_inventory.size() - 1);
        _inventory[_dailyDealIndex].price =
            std::max(1, (int)(_inventory[_dailyDealIndex].price * 0.75f));
    }
}
