#include "ShopManager.h"
#include "GameBalance.h"
#include "VirtualCanvas.h"
#include "NineSlice.h"
#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

// ── File-local helpers ────────────────────────────────────────────────────────

const char* ShopManager::GetWareName(ShopWareType type)
{
    switch (type)
    {
    case ShopWareType::IronrootTonic:   return "Ironroot Tonic";
    case ShopWareType::FieldDressing:   return "Field Dressing";
    case ShopWareType::ManaPrism:       return "Mana Prism";
    case ShopWareType::SharpeningStone: return "Sharpening Stone";
    case ShopWareType::FreecastSeal:    return "Freecast Seal";
    case ShopWareType::WardCharm:       return "Ward Charm";
    case ShopWareType::GildedCompass:   return "Gilded Compass";
    case ShopWareType::EchoSatchel:     return "Echo Satchel";
    default:                            return "Unknown Ware";
    }
}

static const char* ShopContractName(ShopContractType type)
{
    switch (type)
    {
    case ShopContractType::Untouched:        return "Untouched";
    case ShopContractType::ArcaneRestraint:  return "Arcane Restraint";
    case ShopContractType::AgainstTheClock:  return "Against the Clock";
    default:                                 return "Unknown Contract";
    }
}

static const char* ShopContractObjective(ShopContractType type)
{
    switch (type)
    {
    case ShopContractType::Untouched:       return "Clear the next fight without losing HP.";
    case ShopContractType::ArcaneRestraint: return "Clear the next fight using at most 2 abilities.";
    case ShopContractType::AgainstTheClock: return "Clear the next fight before Zeph's timer expires.";
    default:                                return "";
    }
}

static const char* ShopContractReward(ShopContractType type)
{
    switch (type)
    {
    case ShopContractType::Untouched:       return "Reward: upgrade your weakest ability.";
    case ShopContractType::ArcaneRestraint: return "Reward: Ward Charm + 2 free casts.";
    case ShopContractType::AgainstTheClock: return "Reward: choose a relic.";
    default:                                return "";
    }
}

static float DrawShopWrappedText(const std::string& text, float centerX, float y,
                                 float maxWidth, int fontSize, Color color,
                                 int maxLines = 3)
{
    std::istringstream words(text);
    std::vector<std::string> lines;
    std::string line;
    std::string word;
    while (words >> word)
    {
        std::string trial = line.empty() ? word : line + " " + word;
        if (!line.empty() && MeasureText(trial.c_str(), fontSize) > (int)maxWidth)
        {
            lines.push_back(line);
            line = word;
        }
        else
        {
            line = trial;
        }
    }
    if (!line.empty()) lines.push_back(line);

    if ((int)lines.size() > maxLines)
    {
        lines.resize(maxLines);
        std::string& last = lines.back();
        while (!last.empty() && MeasureText((last + "...").c_str(), fontSize) > (int)maxWidth)
            last.pop_back();
        last += "...";
    }

    for (const std::string& wrapped : lines)
    {
        int width = MeasureText(wrapped.c_str(), fontSize);
        DrawText(wrapped.c_str(), (int)(centerX - width * 0.5f), (int)y, fontSize, color);
        y += fontSize + 3.f;
    }
    return y;
}

static const char* ShopWareDesc(ShopWareType type)
{
    switch (type)
    {
    case ShopWareType::IronrootTonic:   return "Gain 1 armour. It lasts until broken.";
    case ShopWareType::FieldDressing:   return "Recover 1 HP. Limited emergency care.";
    case ShopWareType::ManaPrism:       return "Restore 2 mana. No maximum increase.";
    case ShopWareType::SharpeningStone: return "Your next 6 basic hits deal +15% damage.";
    case ShopWareType::FreecastSeal:   return "Next normal ability costs no mana.";
    case ShopWareType::WardCharm:      return "Next unarmoured hit deals 1 less damage.";
    case ShopWareType::GildedCompass:  return "Next 5 gold finds are worth 25% more.";
    case ShopWareType::EchoSatchel:    return "Next 5 Echo pickups give +25% Echoes.";
    default:                           return "";
    }
}

static int ShopWarePrice(ShopWareType type)
{
    switch (type)
    {
    case ShopWareType::ManaPrism:       return 35;
    case ShopWareType::IronrootTonic:   return 45;
    case ShopWareType::SharpeningStone: return 45;
    case ShopWareType::GildedCompass:  return 40;
    case ShopWareType::EchoSatchel:    return 40;
    case ShopWareType::FreecastSeal:   return 50;
    case ShopWareType::FieldDressing:  return 55;
    case ShopWareType::WardCharm:      return 65;
    default:                           return 50;
    }
}

static bool ShopWareDailyDealEligible(ShopWareType type)
{
    return type != ShopWareType::Count;
}

static UpgradeRarity ShopWareRarity(ShopWareType type)
{
    switch (type)
    {
    case ShopWareType::WardCharm:
    case ShopWareType::FreecastSeal:
        return UpgradeRarity::Rare;
    default:
        return UpgradeRarity::Common;
    }
}

static std::string ShopWarePreview(const Character& player, ShopWareType type)
{
    switch (type)
    {
    case ShopWareType::IronrootTonic:
        return TextFormat("Armour %d/%d -> %d/%d", player.GetArmour(), player.GetMaxArmour(),
            std::min(player.GetMaxArmour(), player.GetArmour() + 1), player.GetMaxArmour());
    case ShopWareType::FieldDressing:
        return TextFormat("HP %.0f -> %.0f", player.GetHealthValue(),
            std::min(player.GetMaxHealthValue(), player.GetHealthValue() + 1.f));
    case ShopWareType::ManaPrism:
        return TextFormat("Mana %d -> %d", player.GetMana(), std::min(player.GetMaxMana(), player.GetMana() + 2));
    case ShopWareType::SharpeningStone:
        return TextFormat("Charged hits: %d -> %d", player.GetShopWhetstoneHits(),
            std::min(12, player.GetShopWhetstoneHits() + 6));
    case ShopWareType::FreecastSeal:
        return TextFormat("Free casts: %d -> %d", player.GetShopFreeCasts(),
            std::min(2, player.GetShopFreeCasts() + 1));
    case ShopWareType::WardCharm:
        return TextFormat("Ward charges: %d -> %d", player.GetShopWardHits(),
            std::min(2, player.GetShopWardHits() + 1));
    case ShopWareType::GildedCompass:
        return TextFormat("Boosted pickups: %d -> %d", player.GetShopGoldPickups(),
            std::min(10, player.GetShopGoldPickups() + 5));
    case ShopWareType::EchoSatchel:
        return TextFormat("Boosted pickups: %d -> %d", player.GetShopCellPickups(),
            std::min(10, player.GetShopCellPickups() + 5));
    default:
        return "";
    }
}

bool ShopManager::ApplyWare(Character& player, ShopWareType type)
{
    switch (type)
    {
    case ShopWareType::IronrootTonic:
        if (player.GetArmour() >= player.GetMaxArmour()) return false;
        player.AddArmour(1); return true;
    case ShopWareType::FieldDressing:
        if (player.GetHealthValue() >= player.GetMaxHealthValue()) return false;
        player.Heal(1); return true;
    case ShopWareType::ManaPrism:
        if (player.GetMana() >= player.GetMaxMana()) return false;
        player.RestoreMana(2); return true;
    case ShopWareType::SharpeningStone:
        player.GrantShopWhetstone(); return true;
    case ShopWareType::FreecastSeal:
        player.GrantShopFreeCast(); return true;
    case ShopWareType::WardCharm:
        player.GrantShopWard(); return true;
    case ShopWareType::GildedCompass:
        player.GrantShopGoldCompass(); return true;
    case ShopWareType::EchoSatchel:
        player.GrantShopEchoSatchel(); return true;
    default:
        return false;
    }
}

static const char* ShopUpgradeName(UpgradeType t)
{
    switch (t)
    {
    case UpgradeType::AttackPower:      return "Execution Weight";
    case UpgradeType::AttackRange:      return "Long Grip";
    case UpgradeType::MaxHealth:        return "Toughened Vial";
    case UpgradeType::MaxMana:          return "Mana Reservoir";
    case UpgradeType::Defense:          return "Emergency Plate";
    case UpgradeType::MoveSpeed:        return "Fleet Boots";
    case UpgradeType::IronConstitution: return "Iron Constitution";
    case UpgradeType::SwiftFeet:        return "Predator Step";
    case UpgradeType::Ferocity:         return "Ferocity";
    case UpgradeType::ArcaneMind:       return "Mana Conduit";
    case UpgradeType::IronSkin:         return "Spiked Guard";
    case UpgradeType::BladeEdge:        return "Serrated Edge";
    case UpgradeType::WarGod:           return "War God's Mark";
    case UpgradeType::Resilience:       return "Last Stand Plate";
    case UpgradeType::BladeStorm:       return "Blade Storm Rig";
    case UpgradeType::Juggernaut:       return "Juggernaut Frame";
    case UpgradeType::ArcaneColossus:   return "Overchannel Core";
    // Power Choice additions — not sold by Zeph, but named here so any shared
    // UI path that meets them shows a real label instead of "Unknown".
    case UpgradeType::ManaFlow:         return "Mana Flow";
    case UpgradeType::ClassAttunement:  return "Class Attunement";
    case UpgradeType::Overload:         return "Overload";
    default:                            return "Unknown";
    }
}

static const char* ShopUpgradeDesc(UpgradeType t)
{
    switch (t)
    {
    case UpgradeType::AttackPower:      return "Basic hits gain modest damage.";
    case UpgradeType::AttackRange:      return "Slightly wider melee reach.";
    case UpgradeType::MaxHealth:        return "A small max HP bump. Does not heal.";
    case UpgradeType::MaxMana:          return "Larger mana pool. Does not refill it.";
    case UpgradeType::Defense:          return "Gain 1 armour slot now.";
    case UpgradeType::MoveSpeed:        return "A small movement speed bump.";
    case UpgradeType::IronConstitution: return "+15% max HP and heal the gain.";
    case UpgradeType::SwiftFeet:        return "+8% move speed.";
    case UpgradeType::Ferocity:         return "+10% attack, minimum small gain.";
    case UpgradeType::ArcaneMind:       return "Max MP and slight regen.";
    case UpgradeType::IronSkin:         return "Refill 2 armour slots.";
    case UpgradeType::BladeEdge:        return "Damage plus slight attack reach.";
    case UpgradeType::WarGod:           return "Strong damage and reach boost.";
    case UpgradeType::Resilience:       return "+18% max HP and heal the gain.";
    case UpgradeType::BladeStorm:       return "Damage plus +8% move speed.";
    case UpgradeType::Juggernaut:       return "+12% max HP and 2 armour.";
    case UpgradeType::ArcaneColossus:   return "Max MP, damage, slight regen.";
    case UpgradeType::ManaFlow:         return "+1 max mana, +15% mana regen.";
    case UpgradeType::ClassAttunement:  return "Class resource gain +25%.";
    case UpgradeType::Overload:         return "+15% ability damage, casts cost +1 mana.";
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
    case UpgradeRarity::Common: return 70;
    case UpgradeRarity::Rare:   return 135;
    default:                    return 240;
    }
}

static std::string FormatStatPreview(const char* label, float before, float after, int decimals = 0)
{
    if (decimals <= 0)
        return TextFormat("%s %.0f -> %.0f", label, before, after);
    if (decimals == 1)
        return TextFormat("%s %.1f -> %.1f", label, before, after);
    return TextFormat("%s %.2f -> %.2f", label, before, after);
}

static std::string ShopUpgradePreview(const Character& player, UpgradeType t)
{
    float atk = player.GetAttackPowerValue();
    float hp = player.GetMaxHealthValue();
    float speed = player.GetMoveSpeedValue();
    float rangePct = player.GetAttackRangeMultiplierValue() * 100.f;
    float regen = player.GetManaRegenPerSecond();

    switch (t)
    {
    case UpgradeType::AttackPower:
        return TextFormat("Melee %.1f -> %.1f", atk, atk + 0.5f);
    case UpgradeType::AttackRange:
        return FormatStatPreview("Reach", rangePct, rangePct + 8.f);
    case UpgradeType::MaxHealth:
        return FormatStatPreview("Max HP", hp, hp + 2.f);
    case UpgradeType::MaxMana:
        return TextFormat("Max MP %d -> %d", player.GetMaxMana(), player.GetMaxMana() + 4);
    case UpgradeType::Defense:
        return TextFormat("Armour %d/%d -> %d/%d", player.GetArmour(), player.GetMaxArmour(), std::min(player.GetMaxArmour(), player.GetArmour() + 1), player.GetMaxArmour());
    case UpgradeType::MoveSpeed:
        return FormatStatPreview("Speed", speed, speed + 12.f);
    case UpgradeType::IronConstitution:
        return FormatStatPreview("Max HP", hp, std::ceil(hp * 1.15f));
    case UpgradeType::SwiftFeet:
        return FormatStatPreview("Speed", speed, speed * 1.08f);
    case UpgradeType::Ferocity:
        return TextFormat("Melee %d -> %d", (int)atk, (int)(atk + std::max(1.0f, atk * 0.10f)));
    case UpgradeType::ArcaneMind:
        return TextFormat("Regen %.2f/s -> %.2f/s", regen, Character::kManaRegenBase * (regen / Character::kManaRegenBase + 0.10f));
    case UpgradeType::IronSkin:
        return TextFormat("Armour %d/%d -> %d/%d", player.GetArmour(), player.GetMaxArmour(), std::min(player.GetMaxArmour(), player.GetArmour() + 2), player.GetMaxArmour());
    case UpgradeType::BladeEdge:
        return TextFormat("Melee %d -> %d | Reach +10%%", (int)atk, (int)(atk + 1.f));
    case UpgradeType::WarGod:
        return TextFormat("Melee %d -> %d | Reach +12%%", (int)atk, (int)(atk + std::max(1.75f, atk * 0.15f)));
    case UpgradeType::Resilience:
        return FormatStatPreview("Max HP", hp, std::ceil(hp * 1.18f));
    case UpgradeType::BladeStorm:
        return TextFormat("Melee %d -> %d | Speed +8%%", (int)atk, (int)(atk + 1.5f));
    case UpgradeType::Juggernaut:
        return TextFormat("Max HP %.0f -> %.0f | Armour +2", hp, std::ceil(hp * 1.12f));
    case UpgradeType::ArcaneColossus:
        return TextFormat("MP +10 | Melee %d -> %d", (int)atk, (int)(atk + 1.5f));
    default:
        return "";
    }
}
// Names/descriptions come from the single source of truth in AbilityType.h so
// every class ability shows its real name & text (not a generic "Ability").
static const char* ShopAbilityName(AbilityType t)
{
    return GetAbilityName(t);
}

// One-line card description: the curated two-line text flattened to a single line
// with the mana cost appended (ultimates already say "all MP" in their text).
static const char* ShopAbilityDesc(AbilityType t)
{
    static std::string s;
    s.clear();
    for (const char* p = GetAbilityDesc(t); *p; ++p)
        s += (*p == '\n') ? ' ' : *p;
    if (!IsUltimateAbility(t))
        s += "  " + std::to_string(GetAbilityManaCost(t)) + " MP";
    return s.c_str();
}

static int ShopAbilityPrice(AbilityType t)
{
    switch (t)
    {
    case AbilityType::FireBolt:
    case AbilityType::IceBolt:
    case AbilityType::ElectricBolt: return 140;
    default:                        return 95;
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

    Rectangle GetZephPromptRect(float sx, float sy, InputPromptMode promptMode)
    {
        const bool touchMode = (promptMode == InputPromptMode::Touch);
        const char* prompt = PromptShopNpc(promptMode);
        int fontSize = touchMode ? 40 : 36;
        int textW = MeasureText(prompt, fontSize);
        float promptH = touchMode ? 56.f : 50.f;
        return Rectangle{
             sx - textW * 0.5f - 10.f,
            sy - kZephSpriteHeight * 0.5f - promptH - 40.f,
             (float)textW + 20.f,
             promptH
        };
    }

    float EaseOutQuad(float t) { return 1.f - (1.f - t) * (1.f - t); }
    float EaseOutBack(float t)
    {
        const float c1 = 1.70158f, c3 = c1 + 1.f;
        return 1.f + c3 * powf(t - 1.f, 3.f) + c1 * powf(t - 1.f, 2.f);
    }
}

// ── Setup ─────────────────────────────────────────────────────────────────────

Rectangle ShopManager::GetNpcTouchBtnRect(float sx, float sy) const
{
    return Rectangle{
        sx + _uiNpcBtnOffsetX - _uiNpcBtnW * 0.5f,
        sy + _uiNpcBtnOffsetY - _uiNpcBtnH * 0.5f,
        _uiNpcBtnW,
        _uiNpcBtnH
    };
}

void ShopManager::Init(const ShopTextures& tex)
{
    _tex = tex;
}

void ShopManager::Enter(Vector2 npcWorldPos, Character& player, int act)
{
    _npcPos               = npcWorldPos;
    _nearNpc              = false;
    _touchPromptMode      = false;
    _npcTouchHeld         = false;
    _tab                  = 0;
    _rerollCost           = Balance::Economy::kRerollBaseCost;
    _act                  = std::max(1, act);
    _gamepadCursorIdx      = 0;
    _gamepadNavActive      = false;
    _gamepadConfirmPending = false;
    _gamepadNavCooldown    = 0.f;
    _gamepadBottomActive   = false;
    _gamepadBottomIdx      = 0;
    _gamepadRerollPending  = false;
    _gamepadTabActive      = false;
    _gamepadTabCursor      = 0;
    _gamepadLPActive       = false;
    _gamepadLPIdx          = 0;
    _gamepadLPUpgSlot      = -1;
    _gamepadLPRemSlot      = -1;
    _purchaseMadeThisVisit = false;
    _acceptedContract      = ShopContractType::Count;
    _dialogue        = "Welcome to Zeph's Wares! What do you need?";
    _introFirstEntry = true;
    _introPhase      = IntroPhase::Off;
    GenerateInventory(player);
}

// ── Per-frame (Store room) ────────────────────────────────────────────────────

bool ShopManager::UpdateNpc(Character& player, Vector2 worldOffset, bool touchMode, bool gamepadInteractPressed)
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

    const float sw2 = kVirtualWidth  * 0.5f;
    const float sh2 = kVirtualHeight * 0.5f;
    float sx = _npcPos.x + worldOffset.x + sw2;
    float sy = _npcPos.y + worldOffset.y + sh2;
    Rectangle promptRect = GetZephPromptRect(sx, sy, _promptMode);

    bool touchDown = touchMode && GetTouchPointCount() > 0;
    bool mouseTap  = touchMode && GetTouchPointCount() == 0 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    bool touchTap  = mouseTap || (touchDown && !_npcTouchHeld);
    _npcTouchHeld  = touchDown;

    bool openPressed = false;
    if (_nearNpc)
    {
        if (!touchMode && (IsKeyPressed(KEY_E) || gamepadInteractPressed))
        {
            openPressed = true;
        }
        else if (touchMode && touchTap)
        {
            Vector2 tapPos = GetTouchPointCount() > 0
                             ? GetVirtualTouchPos(0)
                             : GetVirtualMousePos();
            Rectangle btnRect = GetNpcTouchBtnRect(sx, sy);
            openPressed = CheckCollisionPointRec(tapPos, btnRect);
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
    const float sw2 = kVirtualWidth  * 0.5f;
    const float sh2 = kVirtualHeight * 0.5f;
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
    int npFs = 32, npW = MeasureText(nameplate, npFs);
    const float npH = 40.f;
    const float npY = sy - kZephSpriteHeight * 0.5f - npH + 8.f;
    DrawRectangle((int)(sx - npW * 0.5f - 10), (int)npY,
                  npW + 20, (int)npH, Fade(BLACK, 0.6f));
    DrawText(nameplate, (int)(sx - npW * 0.5f),
             (int)(npY + npH * 0.5f - npFs * 0.5f), npFs, GOLD);

    if (_nearNpc)
    {
        if (_touchPromptMode)
        {
            Rectangle btnRect = GetNpcTouchBtnRect(sx, sy);
            DrawRectangleRounded(btnRect, 0.3f, 8, Fade(GOLD, 0.85f));
            DrawRectangleRoundedLines(btnRect, 0.3f, 8, Fade(WHITE, 0.9f));
            const char* label = PromptShopNpc(InputPromptMode::Touch);
            int fs = (int)_uiNpcBtnFs;
            int lw = MeasureText(label, fs);
            DrawText(label,
                (int)(btnRect.x + btnRect.width * 0.5f - lw * 0.5f),
                (int)(btnRect.y + btnRect.height * 0.5f - fs * 0.5f),
                fs, BLACK);
        }
        else
        {
            const char* prompt = PromptShopNpc(_promptMode);
            int prFs = 36;
            int prW = MeasureText(prompt, prFs);
            Rectangle promptRect = GetZephPromptRect(sx, sy, _promptMode);
            DrawRectangleRounded(promptRect, 0.28f, 6, Fade(BLACK, 0.76f));
            DrawRectangleRoundedLines(promptRect, 0.28f, 6, Fade(GOLD, 0.55f));
            DrawText(prompt, (int)(sx - prW * 0.5f),
                     (int)(promptRect.y + promptRect.height * 0.5f - prFs * 0.5f), prFs, RAYWHITE);
        }
    }
}

// ── Per-frame (GameState::Shop) ───────────────────────────────────────────────

bool ShopManager::Update(Character& player, bool debugActive)
{
    const float dt = GetFrameTime();

    // ── Intro animation ───────────────────────────────────────────────────────
    if (_introSkipLock > 0.f) _introSkipLock -= dt;

    if (_introPhase == IntroPhase::Off)
    {
        _introFull          = _introFirstEntry;
        _introFirstEntry    = false;
        _introPhase         = IntroPhase::PanelSettle;
        _introTimer         = 0.f;
        _introFullAlpha     = kIntroOverlayAlpha;
        _introZephAlpha     = _introFull ? kIntroOverlayAlpha : 0.f;
        _introContentAlpha  = _introFull ? kIntroOverlayAlpha : 0.f;
        _introPanelYOff     = kIntroPanelStartYOff;
        _introZephScaleMult = _introFull ? kIntroZephScaleStart : 1.f;
    }

    if (_introPhase != IntroPhase::Done)
    {
        int  tc       = GetTouchPointCount();
        bool touchTap = (tc > 0) && !_introTouchWasDown;
        _introTouchWasDown = (tc > 0);

        bool skipPressed = _introSkipLock <= 0.f &&
                           (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) ||
                            IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) ||
                            IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ESCAPE) ||
                            touchTap || _gamepadConfirmPending);
        if (skipPressed)
        {
            _gamepadConfirmPending = false;
            _introPhase        = IntroPhase::Done;
            _introSkipLock     = 0.08f;
            _introFullAlpha    = 0.f;
            _introZephAlpha    = 0.f;
            _introContentAlpha = 0.f;
            _introZephScaleMult = 1.f;
            _introPanelYOff    = 0.f;
            _introTouchWasDown = (tc > 0);
            return false;
        }

        _introTimer += dt;

        switch (_introPhase)
        {
        case IntroPhase::PanelSettle:
        {
            float dur  = _introFull ? kIntroPanelDur : kIntroShortDur;
            float t    = std::min(_introTimer / dur, 1.f);
            float ease = EaseOutQuad(t);
            _introPanelYOff = kIntroPanelStartYOff * (1.f - ease);
            _introFullAlpha = kIntroOverlayAlpha   * (1.f - ease);
            if (t >= 1.f)
            {
                _introPanelYOff = 0.f;
                _introFullAlpha = 0.f;
                if (_introFull) { _introPhase = IntroPhase::ZephReveal; _introTimer = 0.f; }
                else            { _introZephAlpha = 0.f; _introContentAlpha = 0.f; _introPhase = IntroPhase::Done; }
            }
            break;
        }
        case IntroPhase::ZephReveal:
        {
            float t = std::min(_introTimer / kIntroZephDur, 1.f);
            _introZephAlpha     = kIntroOverlayAlpha * (1.f - EaseOutQuad(t));
            _introZephScaleMult = kIntroZephScaleStart + (1.f - kIntroZephScaleStart) * EaseOutBack(t);
            _introZephScaleMult = std::max(0.01f, _introZephScaleMult);
            if (t >= 1.f)
            {
                _introZephAlpha     = 0.f;
                _introZephScaleMult = 1.f;
                _introPhase         = IntroPhase::ContentReveal;
                _introTimer         = 0.f;
            }
            break;
        }
        case IntroPhase::ContentReveal:
        {
            float t = std::min(_introTimer / kIntroContentDur, 1.f);
            _introContentAlpha = kIntroOverlayAlpha * (1.f - EaseOutQuad(t));
            if (t >= 1.f)
            {
                _introContentAlpha = 0.f;
                _introPhase        = IntroPhase::Done;
            }
            break;
        }
        default: break;
        }

        return false;  // shop is non-interactive during intro
    }

    // ── UI Editor ────────────────────────────────────────────────────────────
    if (debugActive)
    {
        if (IsKeyPressed(KEY_NINE))
            _isUIEditorActive = !_isUIEditorActive;

        if (_isUIEditorActive)
        {
            constexpr int kVarCount = 34;
            if (IsKeyPressed(KEY_UP))
                _uiEditorSelectedIndex = (_uiEditorSelectedIndex - 1 + kVarCount) % kVarCount;
            if (IsKeyPressed(KEY_DOWN))
                _uiEditorSelectedIndex = (_uiEditorSelectedIndex + 1) % kVarCount;

            float* vars[] = {
                &_uiPad, &_uiLeftPanelW, &_uiTitleFs, &_uiStatFs, &_uiSlotFs,
                &_uiSlotBtnFs, &_uiHpFs, &_uiTabH, &_uiBuyBtnH,
                &_uiItemNameFs, &_uiItemDescFs, &_uiItemTextOffsetY, &_uiPriceFs,
                &_uiDialNameFs, &_uiDialTextFs, &_uiPotionH, &_uiPotionFs,
                &_uiAbilTitleFs, &_uiBtnH, &_uiLeaveW, &_uiRerollW, &_uiBtnFs,
                &_uiRarityFs, &_uiRarityPad,
                &_uiZephScale, &_uiZephPosX, &_uiZephPosY,
                &_uiDialPosX, &_uiDialPosY,
                &_uiNpcBtnW, &_uiNpcBtnH, &_uiNpcBtnOffsetX, &_uiNpcBtnOffsetY, &_uiNpcBtnFs
            };
            float step = (_uiEditorSelectedIndex == 1)  ? 0.01f :
                         (_uiEditorSelectedIndex == 24) ? 0.1f  : 1.0f;
            if (IsKeyDown(KEY_RIGHT)) *vars[_uiEditorSelectedIndex] += step;
            if (IsKeyDown(KEY_LEFT))  *vars[_uiEditorSelectedIndex] -= step;

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
                TraceLog(LOG_INFO, "_uiReservedH       = %.2ff;", _uiPotionH);
                TraceLog(LOG_INFO, "_uiReservedFs      = %.2ff;", _uiPotionFs);
                TraceLog(LOG_INFO, "_uiAbilTitleFs     = %.2ff;", _uiAbilTitleFs);
                TraceLog(LOG_INFO, "_uiBtnH            = %.2ff;", _uiBtnH);
                TraceLog(LOG_INFO, "_uiLeaveW          = %.2ff;", _uiLeaveW);
                TraceLog(LOG_INFO, "_uiRerollW         = %.2ff;", _uiRerollW);
                TraceLog(LOG_INFO, "_uiBtnFs           = %.2ff;", _uiBtnFs);
                TraceLog(LOG_INFO, "_uiRarityFs        = %.2ff;", _uiRarityFs);
                TraceLog(LOG_INFO, "_uiRarityPad       = %.2ff;", _uiRarityPad);
                TraceLog(LOG_INFO, "_uiZephScale       = %.2ff;", _uiZephScale);
                TraceLog(LOG_INFO, "_uiZephPosX        = %.2ff;", _uiZephPosX);
                TraceLog(LOG_INFO, "_uiZephPosY        = %.2ff;", _uiZephPosY);
                TraceLog(LOG_INFO, "_uiDialPosX        = %.2ff;", _uiDialPosX);
                TraceLog(LOG_INFO, "_uiDialPosY        = %.2ff;", _uiDialPosY);
                TraceLog(LOG_INFO, "_uiNpcBtnW         = %.2ff;", _uiNpcBtnW);
                TraceLog(LOG_INFO, "_uiNpcBtnH         = %.2ff;", _uiNpcBtnH);
                TraceLog(LOG_INFO, "_uiNpcBtnOffsetX   = %.2ff;", _uiNpcBtnOffsetX);
                TraceLog(LOG_INFO, "_uiNpcBtnOffsetY   = %.2ff;", _uiNpcBtnOffsetY);
                TraceLog(LOG_INFO, "_uiNpcBtnFs        = %.2ff;", _uiNpcBtnFs);
            }

            return false;   // block all shop interaction while editor is open
        }
    }
    else
    {
        _isUIEditorActive = false;
    }

    const float sw  = (float)kVirtualWidth;
    const float sh  = (float)kVirtualHeight;
    const float pad = _uiPad;

    Vector2 mouse   = GetVirtualMousePos();
    bool    clicked  = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    bool    rclicked = IsMouseButtonPressed(MOUSE_RIGHT_BUTTON);   // Reserve Stock

    // ── Layout (must match Draw exactly) ─────────────────────────────────
    static constexpr float kBorderDst_u  = 32.f;
    const float leftW   = sw * _uiLeftPanelW;
    const float leaveH  = _uiBtnH;
    const float leaveY  = sh - pad - leaveH;
    const float dialH   = std::max(sh * 0.12f, kBorderDst_u * 2.f + 30.f);
    const float dialY   = leaveY - pad * 0.5f - dialH;
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
        _dialogue   = "Safe travels, adventurer.";
        _introPhase = IntroPhase::Off;  // re-open will play short fade
        return true;   // engine: transition to GameState::Play
    }
    if (clicked && CheckCollisionPointRec(mouse, rerollBtn))
    {
        if (player.GetGold() >= _rerollCost)
        {
            player.AddGold(-_rerollCost);
            _rerollCost += Balance::Economy::kRerollStep;
            RerollInventory(player);
            _gamepadCursorIdx = 0;
        }
        else
        {
            _dialogue = "You don't have enough gold for a reroll!";
        }
        return false;
    }

    // Gamepad bottom-section pending actions
    if (_gamepadRerollPending)
    {
        _gamepadRerollPending = false;
        if (player.GetGold() >= _rerollCost)
        {
            player.AddGold(-_rerollCost);
            _rerollCost += Balance::Economy::kRerollStep;
            RerollInventory(player);
            _gamepadCursorIdx = 0;
        }
        else { _dialogue = "You don't have enough gold for a reroll!"; }
        return false;
    }
    if (_gamepadLPUpgSlot >= 0)
    {
        int slotIdx = _gamepadLPUpgSlot;
        _gamepadLPUpgSlot = -1;
        AbilityType ab = player.GetLearnedAbility(slotIdx);
        if (ab != AbilityType::None)
        {
            int upgCost = player.GetAbilityLevel(ab) * 100;
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
        }
        return false;
    }
    if (_gamepadLPRemSlot >= 0)
    {
        int slotIdx = _gamepadLPRemSlot;
        _gamepadLPRemSlot = -1;
        if (player.GetGold() < 100)
            _dialogue = "You need 100g to remove an ability.";
        else
        {
            player.AddGold(-100);
            player.RemoveAbilityAtSlot(slotIdx);
            _gamepadLPIdx = 0;
            _dialogue = "That power is gone. Choose wisely next time.";
        }
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

    // ── Content area ──────────────────────────────────────────────────────
    // One-page shop: the header strip (drawn in Draw) replaces the old tab
    // buttons; the strip's geometry is kept so the grid position is unchanged.
    const float titleH = 46.f;
    const float tabH   = _uiTabH;
    const float tabY   = shopY + titleH;
    const float contentY = tabY + tabH + iPad;
    const float contentH = shopH - titleH - tabH - iPad * 2.f;
    const float contentW = shopW - iPad * 2.f;

    {
        // 2 rows × 3 cols of mixed items (wares + abilities, one grid)
        const float cols   = 3.f, rows = 2.f, gap = 10.f;
        const float itemW  = (contentW - gap * (cols - 1.f)) / cols;
        const float itemH  = (contentH - gap * (rows - 1.f)) / rows;
        const float buyH   = _uiBuyBtnH;
        int displayIdx = 0;

        for (int idx = 0; idx < (int)_inventory.size(); idx++)
        {
            ShopItem& item = _inventory[idx];
            if (item.purchased) continue;

            int   col  = displayIdx % 3;
            int   row  = displayIdx / 3;
            float ix   = shopX + iPad + col * (itemW + gap);
            float iy   = contentY + row * (itemH + gap);

            Rectangle cardRect = { ix, iy, itemW, itemH };

            // Reserve Stock: right-click (gamepad: X) locks this card so it
            // survives rerolls, for a small fee. One reserved card at a time.
            bool reserveActivated = (rclicked && CheckCollisionPointRec(mouse, cardRect)) ||
                                    (_gamepadReservePending && displayIdx == _gamepadCursorIdx);
            if (reserveActivated)
            {
                _gamepadReservePending = false;
                bool anotherReserved = false;
                for (const auto& other : _inventory)
                    if (other.reserved && !other.purchased) { anotherReserved = true; break; }
                if (item.isContract)
                    _dialogue = "Contracts cannot be reserved. The work changes daily.";
                else if (item.reserved)
                    _dialogue = "That one's already set aside for you.";
                else if (anotherReserved)
                    _dialogue = "I can only hold ONE item aside at a time.";
                else if (player.GetGold() >= Balance::Economy::kReserveStockCost)
                {
                    player.AddGold(-Balance::Economy::kReserveStockCost);
                    item.reserved = true;
                    _dialogue = "Reserved! That one survives any reroll.";
                }
                else
                {
                    _dialogue = TextFormat("Reserving costs %d gold, friend.",
                                           Balance::Economy::kReserveStockCost);
                }
            }

            bool cardActivated = (clicked && CheckCollisionPointRec(mouse, cardRect)) ||
                                 (_gamepadConfirmPending && displayIdx == _gamepadCursorIdx);
            if (cardActivated)
            {
                _gamepadConfirmPending = false;
                if (item.isAbility && !item.upgradesAbility && player.GetLearnedCount() >= 3)
                {
                    _dialogue = "You can only hold 3 abilities. Remove one first.";
                    return false;
                }
                if (item.isContract)
                {
                    if (_acceptedContract != ShopContractType::Count)
                    {
                        _dialogue = "One contract at a time. Finish the work first.";
                        return false;
                    }
                    _acceptedContract = item.contractType;
                    item.purchased = true;
                    _purchaseMadeThisVisit = true;
                    _gamepadCursorIdx = 0;
                    _dialogue = "Contract accepted. It begins in your next fight.";
                }
                else if (player.GetGold() >= item.price)
                {
                    player.AddGold(-item.price);
                    if (item.isAbility)
                    {
                        if (item.upgradesAbility)
                            player.UpgradeAbility(item.abilityType);
                        else
                            player.LearnAbility(item.abilityType);
                    }
                    else
                    {
                        if (!ApplyWare(player, item.wareType))
                        {
                            player.AddGold(item.price);
                            _dialogue = "That ware would have no effect right now.";
                            return false;
                        }
                    }
                    item.purchased = true;
                    _purchaseMadeThisVisit = true;
                    _gamepadCursorIdx = 0;
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

    return false;
}

bool ShopManager::UpdateGamepadNav(float dt, const Character& player)
{
    _gamepadConfirmPending = false;
    _gamepadRerollPending  = false;
    _gamepadReservePending = false;
    _gamepadLPUpgSlot      = -1;
    _gamepadLPRemSlot      = -1;

    if (!IsGamepadAvailable(0))
    {
        _gamepadNavActive = false;
        return false;
    }
    _gamepadNavActive = true;

    // Count visible items — one mixed page, so every unpurchased item counts.
    int itemCount = 0;
    for (const auto& item : _inventory)
        if (!item.purchased)
            ++itemCount;
    itemCount = std::min(itemCount, 6);
    if (itemCount > 0) _gamepadCursorIdx = std::min(_gamepadCursorIdx, itemCount - 1);
    else               _gamepadCursorIdx = 0;

    if (_gamepadNavCooldown > 0.f) _gamepadNavCooldown -= dt;

    float axisX = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
    float axisY = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
    constexpr float kNavCooldown = 0.18f;
    bool navLeft  = IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT)  || (axisX < -0.5f && _gamepadNavCooldown <= 0.f);
    bool navRight = IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT) || (axisX >  0.5f && _gamepadNavCooldown <= 0.f);
    bool navUp    = IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_UP)    || (axisY < -0.5f && _gamepadNavCooldown <= 0.f);
    bool navDown  = IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN)  || (axisY >  0.5f && _gamepadNavCooldown <= 0.f);
    bool gpA      = IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);

    // Build left-panel button list (slot_idx, isRem) for occupied slots
    int lpCount = 0;
    int lpSlotIndex[6] = {};   // slot index for each flat entry pair
    {
        int slotTotal = player.GetMaxAbilitySlots();
        for (int i = 0; i < slotTotal; i++)
            if (player.GetLearnedAbility(i) != AbilityType::None)
            {
                lpSlotIndex[lpCount] = i;
                ++lpCount;
            }
    }
    int lpEntryCount = lpCount * 2; // upg + rem per occupied slot

    if (_gamepadLPActive)
    {
        // Left panel: up/down navigates Upg/Rem buttons across occupied slots
        if (navUp   && _gamepadLPIdx > 0)              { _gamepadLPIdx--; _gamepadNavCooldown = kNavCooldown; }
        if (navDown && _gamepadLPIdx < lpEntryCount - 1){ _gamepadLPIdx++; _gamepadNavCooldown = kNavCooldown; }
        if (navRight) { _gamepadLPActive = false; _gamepadNavCooldown = kNavCooldown; }

        if (gpA && lpEntryCount > 0)
        {
            int pairIdx    = _gamepadLPIdx / 2;   // which occupied slot
            bool isRem     = (_gamepadLPIdx % 2) == 1;
            int  slotIdx   = lpSlotIndex[pairIdx];
            if (isRem) _gamepadLPRemSlot = slotIdx;
            else       _gamepadLPUpgSlot = slotIdx;
        }
    }
    else if (_gamepadBottomActive)
    {
        // Bottom section: [Reroll=0][Leave=1]
        if      (navLeft  && _gamepadBottomIdx > 0) { _gamepadBottomIdx--; _gamepadNavCooldown = kNavCooldown; }
        else if (navRight && _gamepadBottomIdx < 1) { _gamepadBottomIdx++; _gamepadNavCooldown = kNavCooldown; }
        else if (navUp) { _gamepadBottomActive = false; _gamepadNavCooldown = kNavCooldown; }

        if (gpA)
        {
            if (_gamepadBottomIdx == 0)
                _gamepadRerollPending = true;
            else
            {
                _dialogue   = "Safe travels, adventurer.";
                _introPhase = IntroPhase::Off;
                return true;
            }
        }
    }
    else
    {
        // Item grid navigation
        int col = _gamepadCursorIdx % 3;
        int row = _gamepadCursorIdx / 3;

        if      (navLeft  && col > 0)
        {
            --_gamepadCursorIdx;
            _gamepadNavCooldown = kNavCooldown;
        }
        else if (navLeft  && col == 0 && lpEntryCount > 0)
        {
            // Enter left panel
            _gamepadLPActive = true;
            _gamepadLPIdx    = 0;
            _gamepadNavCooldown = kNavCooldown;
        }
        else if (navRight && col < 2 && _gamepadCursorIdx + 1 < itemCount)
        {
            ++_gamepadCursorIdx;
            _gamepadNavCooldown = kNavCooldown;
        }
        else if (navUp    && row > 0)
        {
            _gamepadCursorIdx -= 3;
            _gamepadNavCooldown = kNavCooldown;
        }
        else if (navDown && _gamepadCursorIdx + 3 < itemCount)
        {
            _gamepadCursorIdx += 3;
            _gamepadNavCooldown = kNavCooldown;
        }
        else if (navDown)
        {
            _gamepadBottomActive = true;
            _gamepadBottomIdx    = 0;
            _gamepadNavCooldown  = kNavCooldown;
        }

        if (gpA) _gamepadConfirmPending = true;
        // X / Square reserves the highlighted card (Reserve Stock).
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT))
            _gamepadReservePending = true;
    }

    // B / Circle = leave the shop from anywhere
    if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT))
    {
        _dialogue   = "Safe travels, adventurer.";
        _introPhase = IntroPhase::Off;
        return true;
    }

    return false;
}

void ShopManager::Draw(const Character& player, bool debugActive) const
{
    const float sw  = (float)kVirtualWidth;
    const float sh  = (float)kVirtualHeight;
    const float pad = _uiPad;
    const float introYOff = _introPanelYOff;

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
    const float leftW   = sw * _uiLeftPanelW;
    const float leaveH  = _uiBtnH;
    const float leaveY  = sh - pad - leaveH + introYOff;
    const float dialH   = std::max(sh * 0.12f, kBorderDst_layout * 2.f + 30.f);
    const float dialY   = leaveY - pad * 0.5f - dialH;
    const float shopX   = pad + leftW + pad;
    const float shopY   = pad + introYOff;
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
    const float lx = pad, ly = pad + introYOff;
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

        // Stats grid — explicit size avoids MSVC deduction issues with std::string members
        struct StatRow { const char* label; std::string value; };
        StatRow stats[3] = {
            { "ATK",    TextFormat("%.1f", player.GetAttackPowerValue())                               },
            { "ARMOUR", TextFormat("%d / %d", player.GetArmour(), player.GetMaxArmour())               },
            { "GOLD",   std::to_string(player.GetGold())                                               },
        };
        float statFs   = _uiStatFs;
        float statRowH = statFs + 9.f;
        for (int si = 0; si < 3; si++)
        {
            DrawText(stats[si].label, (int)(lx + cp + 4.f), (int)cy, (int)statFs, BLACK);
            int vw = MeasureText(stats[si].value.c_str(), (int)statFs);
            DrawText(stats[si].value.c_str(), (int)(lx + leftW - cp - vw - 18.f), (int)cy, (int)statFs, BLACK);
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

        int lpDrawEntry = 0;  // flat index into left-panel button list (for cursor highlight)

        const float btnH    = _uiSlotBtnFs + 12.f;
        const float btnGap  = 4.f;
        const float slotGap = 3.f;
        const float availH  = (ly + lh - cp) - cy;
        float slotH = (availH - (float)occupiedCount * (btnH + btnGap)
                               - (float)slotCount * slotGap) / (float)slotCount;
        slotH = std::max(30.f, slotH);

        float slotFs = _uiSlotFs;
        const float btnW = (cw - 8.f) * 0.5f;
        Vector2 mpLeft = GetVirtualMousePos();

        // Ability inspect: hovering an owned slot (mouse) or holding Y/Triangle while
        // its slot is focused (gamepad) shows that ability's full description below.
        AbilityType inspectAbil = AbilityType::None;
        const bool inspectHeld  = IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_UP);

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

            // Mouse hover over an owned slot inspects that ability.
            if (ab != AbilityType::None && CheckCollisionPointRec(mpLeft, sr))
                inspectAbil = ab;

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

                bool gpUpgFocus = (_gamepadLPActive && _gamepadLPIdx == lpDrawEntry);
                bool gpRemFocus = (_gamepadLPActive && _gamepadLPIdx == lpDrawEntry + 1);
                // Holding Y/Triangle while this owned slot is focused inspects it.
                if (inspectHeld && (gpUpgFocus || gpRemFocus))
                    inspectAbil = ab;
                lpDrawEntry += 2;

                bool upgHov = CheckCollisionPointRec(mpLeft, upgBtn) || gpUpgFocus;
                Color upgBg = (canUpg && canAffUpg) ? (upgHov ? Color{45,90,45,240} : Color{30,60,30,220})
                                                    : Color{25,25,30,140};
                Color upgBo = (canUpg && canAffUpg) ? (upgHov ? Color{120,220,120,255} : Color{80,180,80,220})
                                                    : Color{60,60,70,120};
                if (gpUpgFocus)
                    DrawRectangleRoundedLines({ upgBtn.x - 3.f, upgBtn.y - 3.f, upgBtn.width + 6.f, upgBtn.height + 6.f },
                                             0.12f, 6, Color{255, 200, 0, 220});
                smallBox(upgBtn, upgBg, upgBo);
                const char* upgLbl = canUpg ? TextFormat("Upg %dg", upgCost) : "MAX";
                int ulW = MeasureText(upgLbl, ubFs);
                DrawText(upgLbl,
                    (int)(upgBtn.x + upgBtn.width  * 0.5f - ulW * 0.5f),
                    (int)(upgBtn.y + upgBtn.height * 0.5f - ubFs * 0.5f),
                    ubFs, (canUpg && canAffUpg) ? Color{180,255,180,255} : Fade(RAYWHITE, 0.35f));

                bool remHov = CheckCollisionPointRec(mpLeft, remBtn) || gpRemFocus;
                Color remBg = canAffRem ? (remHov ? Color{110,30,30,240} : Color{70,20,20,200})
                                        : Color{25,25,30,140};
                Color remBo = canAffRem ? (remHov ? Color{255,80,80,255} : Color{200,60,60,200})
                                        : Color{60,60,70,120};
                if (gpRemFocus)
                    DrawRectangleRoundedLines({ remBtn.x - 3.f, remBtn.y - 3.f, remBtn.width + 6.f, remBtn.height + 6.f },
                                             0.12f, 6, Color{255, 200, 0, 220});
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

        // ── Ability inspect card (hover / Y-Triangle) — drawn on top of the
        //    portrait so the player can read what an owned ability actually does.
        if (inspectAbil != AbilityType::None)
        {
            const char* inName  = GetAbilityName(inspectAbil);
            const char* inDesc  = GetAbilityDesc(inspectAbil);   // curated two-line text
            const char* inMana  = IsUltimateAbility(inspectAbil)
                                ? "Ultimate - drains all mana"
                                : TextFormat("Mana cost: %d", player.GetAbilityCost(inspectAbil));   // includes Overload surcharge

            float bw = cw;
            float bh = 158.f;
            float bx = lx + cp;
            float by = ly + lh - kBorderDst - bh - 6.f;   // anchored to panel bottom
            DrawRectangleRounded({ bx, by, bw, bh }, 0.10f, 6, Fade(Color{ 12, 14, 22, 255 }, 0.95f));
            DrawRectangleRoundedLines({ bx, by, bw, bh }, 0.10f, 6, Color{ 120, 150, 220, 255 });

            float tx = bx + 14.f;
            float ty = by + 12.f;
            int nameFs2 = (int)std::min(_uiSlotFs, 34.f);
            DrawText(inName, (int)tx, (int)ty, nameFs2, kGold);
            ty += nameFs2 + 8.f;
            DrawText(inDesc, (int)tx, (int)ty, (int)_uiItemDescFs, RAYWHITE);   // renders 2 lines
            ty += _uiItemDescFs * 2.f + 10.f;
            DrawText(inMana, (int)tx, (int)ty, (int)std::max(18.f, _uiItemDescFs - 2.f), SKYBLUE);
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

        // One-page shop: the old WARES/ABILITIES tabs are gone. A single header
        // strip keeps the same geometry so the card grid below doesn't move.
        const float tabY   = shopY + titleH;
        Rectangle tabStrip = { shopX + iPad, tabY, shopW - iPad * 2.f, tabH };
        smallBox(tabStrip, Color{ 20, 25, 40, 180 }, Color{ 80, 100, 140, 180 });
        {
            const char* stripLabel = "TODAY'S STOCK - SUPPLIES, CONTRACTS & ABILITIES";
            float fs = std::min(_uiStatFs, tabStrip.height - 8.f);
            fs = std::max(8.f, fs);
            while (fs > 8.f && MeasureText(stripLabel, (int)fs) > (int)(tabStrip.width - 16.f))
                fs -= 1.f;
            int tw = MeasureText(stripLabel, (int)fs);
            DrawText(stripLabel,
                (int)(tabStrip.x + tabStrip.width  * 0.5f - tw * 0.5f),
                (int)(tabStrip.y + tabStrip.height * 0.5f - fs * 0.5f),
                (int)fs, kDim);
        }

        const float contentY = tabY + tabH + iPad;
        const float contentH = shopH - titleH - tabH - iPad * 2.f;
        const float contentW = shopW - iPad * 2.f;

        // Icon lookup — maps item type to the appropriate texture pointer
        auto getShopIcon = [&](const ShopItem& si) -> Texture2D*
        {
            if (si.isContract)
                return _tex.upgradeSpeed;
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
                default:
                {
                    int idx = (int)si.abilityType;
                    if (_tex.abilityIcons && idx >= 0 && idx < _tex.abilityIconCount &&
                        _tex.abilityIcons[idx].id != 0)
                        return &_tex.abilityIcons[idx];
                    return nullptr;
                }
                }
            }
            // Wares map to the nearest stat-icon family.
            switch (si.wareType)
            {
            case ShopWareType::SharpeningStone:
                return _tex.upgradeAttack;
            case ShopWareType::FieldDressing:
                return _tex.upgradeHealth;
            case ShopWareType::ManaPrism:
            case ShopWareType::FreecastSeal:
            case ShopWareType::EchoSatchel:
                return _tex.upgradeMagic;
            case ShopWareType::IronrootTonic:
            case ShopWareType::WardCharm:
                return _tex.upgradeDefense;
            case ShopWareType::GildedCompass:
                return _tex.upgradeSpeed;
            default: return nullptr;
            }
        };

        {
            // One mixed grid: abilities and wares share the same 2×3 layout, in
            // the SAME order the purchase loop walks — draw and click can never
            // disagree again (the old tabbed draw skipped ability items while
            // the click loop counted them, misaligning the whole grid).
            const float cols = 3.f, rows = 2.f, gap = 10.f;
            const float itemW = (contentW - gap * (cols - 1.f)) / cols;
            const float itemH = (contentH - gap * (rows - 1.f)) / rows;
            const float buyH   = _uiBuyBtnH;
            const float iconSz = std::min(itemW * 0.40f, itemH * 0.35f);
            const float nameFs = _uiItemNameFs;
            const float descFsBase = _uiItemDescFs;
            bool slotsFull = (player.GetLearnedCount() >= 3);

            // Category badge text — what the item IS, in merchant language.
            auto ShopItemCategoryLabel = [](const ShopItem& it) -> const char*
            {
                if (it.isAbility)
                    return it.upgradesAbility ? "UPGRADE" : "ABILITY";
                return it.isContract ? "CONTRACT" : "SUPPLY";
            };

            int availableItems = 0;
            for (const auto& item : _inventory)
                if (!item.purchased)
                    availableItems++;

            if (availableItems == 0)
            {
                DrawText("Nothing left in stock.", (int)(shopX + iPad + 8.f),
                    (int)(contentY + 20.f), 18, kDim);
            }

            int displayIdx = 0;
            for (int idx = 0; idx < (int)_inventory.size(); idx++)
            {
                const ShopItem& item = _inventory[idx];
                if (item.purchased) continue;

                int   col = displayIdx % 3, row = displayIdx / 3;
                float ix  = shopX + iPad + col * (itemW + gap);
                float iy  = contentY + row * (itemH + gap);

                // Card colour: wares tint by rarity; abilities keep their
                // signature purple so the two kinds read apart at a glance.
                UpgradeRarity rar = item.isAbility ? UpgradeRarity::Rare : ShopWareRarity(item.wareType);
                Color rarCol = item.isContract ? Color{ 215, 155, 70, 255 }
                             : item.isAbility  ? Color{ 120, 80, 220, 255 }
                                               : ShopRarityColor(rar);
                Color cardBg    = { (uint8_t)(rarCol.r / 3), (uint8_t)(rarCol.g / 3), (uint8_t)(rarCol.b / 3), 220 };
                Color cardBgHov = { (uint8_t)(rarCol.r / 2), (uint8_t)(rarCol.g / 2), (uint8_t)(rarCol.b / 2), 240 };
                Color cardBo    = Fade(rarCol, 0.55f);
                Vector2 mouse = GetVirtualMousePos();
                bool    hov   = CheckCollisionPointRec(mouse, { ix, iy, itemW, itemH })
                                || (_gamepadNavActive && displayIdx == _gamepadCursorIdx);
                if (hov) { cardBo = Fade(rarCol, 0.90f); }
                smallBox({ ix, iy, itemW, itemH }, hov ? cardBgHov : cardBg, cardBo);

                DrawRectangle((int)ix, (int)iy, 5, (int)itemH, Fade(rarCol, 0.80f));

                BeginScissorMode((int)ix + 6, (int)iy, (int)itemW - 6, (int)itemH);

                // Category badge — top-left corner. Merchant language (what the
                // item IS), not levelling-screen rarity words.
                {
                    const char* catLbl = ShopItemCategoryLabel(item);
                    constexpr int kInner = 3;
                    int rlFs = (int)_uiRarityFs;
                    int rlW  = MeasureText(catLbl, rlFs);
                    float rlX = ix + 6.f + _uiRarityPad;
                    float rlY = iy + _uiRarityPad;
                    DrawRectangle((int)rlX, (int)rlY, rlW + kInner * 2, rlFs + kInner * 2, Fade(BLACK, 0.65f));
                    DrawText(catLbl, (int)(rlX + kInner), (int)(rlY + kInner), rlFs, WHITE);
                }

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
                // Resolve card text by item kind (the old path pushed everything
                // through the legacy UpgradeType tables, mislabelling wares).
                const char* name;
                const char* desc;
                std::string preview;
                if (item.isAbility)
                {
                    name = ShopAbilityName(item.abilityType);
                    desc = ShopAbilityDesc(item.abilityType);
                    preview = item.upgradesAbility
                        ? TextFormat("Lv %d -> %d", player.GetAbilityLevel(item.abilityType),
                                                    player.GetAbilityLevel(item.abilityType) + 1)
                        : (slotsFull ? "Ability slots full" : "Learn: new ability");
                }
                else if (item.isContract)
                {
                    name = ShopContractName(item.contractType);
                    desc = ShopContractObjective(item.contractType);
                    preview = ShopContractReward(item.contractType);
                }
                else
                {
                    name = GetWareName(item.wareType);
                    desc = ShopWareDesc(item.wareType);
                    preview = ShopWarePreview(player, item.wareType);
                }

                float fittedNameFs = nameFs;
                while (fittedNameFs > 26.f && MeasureText(name, (int)fittedNameFs) > (int)(itemW - 20.f))
                    fittedNameFs -= 1.f;
                int nw = MeasureText(name, (int)fittedNameFs);
                DrawText(name, (int)(ix + itemW * 0.5f - nw * 0.5f + 3.f),
                    (int)cy2, (int)fittedNameFs, RAYWHITE);
                cy2 += fittedNameFs + 3.f;

                const float maxDescW = itemW - 20.f;
                int descFs = (int)std::max(26.f, descFsBase);
                cy2 = DrawShopWrappedText(desc, ix + itemW * 0.5f + 3.f, cy2,
                                          maxDescW, descFs, kDim, 2);
                int previewFs = std::max(25, std::min(descFs, 28));
                DrawShopWrappedText(preview, ix + itemW * 0.5f + 3.f, cy2,
                                    maxDescW, previewFs, Color{ 190, 255, 180, 235 }, 2);

                // Corner badge: RESERVED (locked through rerolls) beats DEAL.
                if (item.reserved)
                {
                    const char* keptLbl = "RESERVED";
                    int dlFs = 13, dlW = MeasureText(keptLbl, dlFs);
                    DrawRectangle((int)(ix + itemW - dlW - 10.f), (int)iy, dlW + 10, 20,
                        Color{ 90, 160, 255, 230 });
                    DrawText(keptLbl, (int)(ix + itemW - dlW - 5.f), (int)(iy + 3.f),
                        dlFs, BLACK);
                }
                else if (idx == _dailyDealIndex)
                {
                    const char* dealLbl = "DEAL!";
                    int dlFs = 13, dlW = MeasureText(dealLbl, dlFs);
                    DrawRectangle((int)(ix + itemW - dlW - 10.f), (int)iy, dlW + 10, 20,
                        Color{255, 200, 0, 230});
                    DrawText(dealLbl, (int)(ix + itemW - dlW - 5.f), (int)(iy + 3.f),
                        dlFs, BLACK);
                }

                EndScissorMode();

                // A NEW ability with full slots is blocked (upgrades still fine).
                bool blocked   = item.isAbility && !item.upgradesAbility && slotsFull;
                bool canAfford = item.isContract || (player.GetGold() >= item.price);
                bool canBuy    = canAfford && !blocked;
                Color buyBg = canBuy ? Color{30,90,30,220} : Color{60,30,30,180};
                Color buyBo = canBuy ? Color{80,200,80,255} : Color{160,60,60,200};
                Rectangle buyBtn = { ix + 4.f, iy + itemH - buyH - 4.f, itemW - 8.f, buyH };
                smallBox(buyBtn, buyBg, buyBo);
                int prFs = (int)_uiPriceFs;
                const char* prLbl = blocked ? "SLOTS FULL"
                    : item.isContract ? "ACCEPT"
                    : (idx == _dailyDealIndex)
                        ? TextFormat("%dg  DEAL", item.price)
                        : TextFormat("%dg", item.price);
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

        const char* dText = _dialogue.c_str();
        int ntW = MeasureText(nameTag, ntFs);

        DrawText(nameTag,
            (int)_uiDialPosX,
            (int)(_uiDialPosY - ntFs * 0.5f),
            ntFs, BLACK);
        DrawText(dText,
            (int)(_uiDialPosX + ntW + 10.f),
            (int)(_uiDialPosY - dtFs * 0.5f),
            dtFs, BLACK);
    }

    // ── ZEPH PORTRAIT (dialogue box, bottom-right) ───────────────────────
    if (_tex.zephIdle && _tex.zephIdle->id > 0)
    {
        constexpr int   kZFrames   = 6;
        constexpr float kZLoop     = 3.f;
        int zFrame = (int)(fmod(GetTime(), kZLoop) / (kZLoop / kZFrames));
        zFrame = std::max(0, std::min(kZFrames - 1, zFrame));

        const float frameW = (float)_tex.zephIdle->width  / (float)kZFrames;
        const float frameH = (float)_tex.zephIdle->height;
        Rectangle   zSrc   = { zFrame * frameW, 0.f, frameW, frameH };

        constexpr float kBaseH = 180.f;
        float dstH = kBaseH * _uiZephScale * _introZephScaleMult;
        float dstW = dstH * (frameW / frameH);
        Rectangle zDst = {
            _uiZephPosX - dstW * 0.5f,
            _uiZephPosY - dstH * 0.5f,
            dstW, dstH
        };
        DrawTexturePro(*_tex.zephIdle, zSrc, zDst, {}, 0.f, WHITE);
    }
    // ── REROLL + LEAVE BUTTONS ────────────────────────────────────────────
    const float leaveW  = _uiLeaveW;
    const float rerollW = _uiRerollW;
    const float leaveX  = shopX + shopW * 0.5f + 8.f;
    const float rerollX = shopX + shopW * 0.5f - rerollW - 8.f;
    Rectangle leaveBtn  = { leaveX,  leaveY, leaveW,  leaveH };
    Rectangle rerollBtn = { rerollX, leaveY, rerollW, leaveH };

    Vector2 mpos = GetVirtualMousePos();
    bool leaveHov  = CheckCollisionPointRec(mpos, leaveBtn)  || (_gamepadBottomActive && _gamepadBottomIdx == 1);
    bool rerollHov = CheckCollisionPointRec(mpos, rerollBtn) || (_gamepadBottomActive && _gamepadBottomIdx == 0);
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

    // ── Intro animation overlays (drawn on top of shop, under debug panel) ──
    if (_introZephAlpha > 0.001f)
        DrawRectangle((int)shopX, (int)(dialY - 4.f),
                      (int)(sw - shopX), (int)(sh - (dialY - 4.f)),
                      Fade(BLACK, _introZephAlpha));
    if (_introContentAlpha > 0.001f)
        DrawRectangle((int)(shopX - 1.f), (int)(shopY - 1.f),
                      (int)(shopW + 2.f), (int)(shopH + 2.f),
                      Fade(BLACK, _introContentAlpha));
    if (_introFullAlpha > 0.001f)
        DrawRectangle(0, 0, (int)sw, (int)sh, Fade(BLACK, _introFullAlpha));

    // ── UI Editor debug panel ─────────────────────────────────────────────
    if (debugActive && _isUIEditorActive)
    {
        constexpr int kN = 34;
        const char* varNames[kN] = {
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
            "15 Reserved Height",
            "16 Reserved Font",
            "17 Abil Title Font",
            "18 Btn Height",
            "19 Leave Width",
            "20 Reroll Width",
            "21 Btn Font",
            "22 Rarity Label Font",
            "23 Rarity Label Pad",
            "24 Zeph Scale",
            "25 Zeph Pos X",
            "26 Zeph Pos Y",
            "27 Dial Pos X",
            "28 Dial Pos Y",
            "29 NPC Btn Width",
            "30 NPC Btn Height",
            "31 NPC Btn Offset X",
            "32 NPC Btn Offset Y",
            "33 NPC Btn Font",
        };
        const float* varPtrs[kN] = {
            &_uiPad, &_uiLeftPanelW, &_uiTitleFs, &_uiStatFs, &_uiSlotFs,
            &_uiSlotBtnFs, &_uiHpFs, &_uiTabH, &_uiBuyBtnH,
            &_uiItemNameFs, &_uiItemDescFs, &_uiItemTextOffsetY, &_uiPriceFs,
            &_uiDialNameFs, &_uiDialTextFs, &_uiPotionH, &_uiPotionFs,
            &_uiAbilTitleFs, &_uiBtnH, &_uiLeaveW, &_uiRerollW, &_uiBtnFs,
            &_uiRarityFs, &_uiRarityPad,
            &_uiZephScale, &_uiZephPosX, &_uiZephPosY,
            &_uiDialPosX, &_uiDialPosY,
            &_uiNpcBtnW, &_uiNpcBtnH, &_uiNpcBtnOffsetX, &_uiNpcBtnOffsetY, &_uiNpcBtnFs
        };

        // Two-column layout: left col = 0..half-1, right col = half..kN-1
        constexpr int   kHalf  = (kN + 1) / 2;  // 15
        constexpr float colW   = 310.f;
        constexpr float colGap = 20.f;
        constexpr float rowH   = 32.f;
        const float panW = colW * 2.f + colGap;
        const float panH = 40.f + kHalf * rowH;
        const float panX = sw * 0.5f - panW * 0.5f;
        const float panY = 10.f;

        DrawRectangle((int)panX, (int)panY, (int)panW, (int)panH, Fade(BLACK, 0.82f));
        DrawRectangleLines((int)panX, (int)panY, (int)panW, (int)panH, DARKGRAY);
        // Divider between columns
        DrawLine((int)(panX + colW + colGap * 0.5f), (int)(panY + 36.f),
                 (int)(panX + colW + colGap * 0.5f), (int)(panY + panH - 4.f), DARKGRAY);

        DrawText("UI EDITOR  [9] close", (int)(panX + 8.f), (int)(panY + 6.f), 11, GRAY);
        DrawText("[UP/DOWN] select  [L/R] nudge  [S] export",
            (int)(panX + 8.f), (int)(panY + 20.f), 10, DARKGRAY);

        for (int i = 0; i < kN; i++)
        {
            int   col = i / kHalf;
            int   row = i % kHalf;
            float cx  = panX + col * (colW + colGap);
            float ry  = panY + 38.f + row * rowH;
            bool  sel = (i == _uiEditorSelectedIndex);
            Color col_c = sel ? YELLOW : WHITE;

            if (sel)
                DrawText("->", (int)(cx + 4.f), (int)(ry + rowH * 0.5f - 7.f), 13, YELLOW);

            DrawText(varNames[i],
                (int)(cx + 26.f), (int)(ry + rowH * 0.5f - 7.f), 13, col_c);

            const char* valStr = TextFormat("%.2f", *varPtrs[i]);
            int valW = MeasureText(valStr, 13);
            DrawText(valStr,
                (int)(cx + colW - valW - 6.f), (int)(ry + rowH * 0.5f - 7.f), 13, col_c);
        }
    }
}

// ── Inventory ─────────────────────────────────────────────────────────────────

void ShopManager::RerollInventory(Character& player)
{
    // Reserve Stock: a reserved, unpurchased card survives the reroll. Capture
    // it, regenerate, then swap it back over the last same-kind slot so the
    // grid stays a full page (kept price and all — the lock was already paid).
    ShopItem kept{};
    bool     hasKept = false;
    for (const ShopItem& item : _inventory)
        if (item.reserved && !item.purchased) { kept = item; hasKept = true; break; }

    GenerateInventory(player);

    if (hasKept)
    {
        for (int i = (int)_inventory.size() - 1; i >= 0; --i)
        {
            if (_inventory[i].isAbility != kept.isAbility) continue;
            _inventory[i] = kept;
            if (_dailyDealIndex == i) _dailyDealIndex = -1;   // keep the locked price honest
            break;
        }
        _dialogue = "Fresh stock! I kept your reserved item aside.";
    }
    else
    {
        _dialogue = "Fresh stock, just for you!";
    }
}

void ShopManager::GenerateInventory(const Character& player)
{
    _inventory.clear();

    const float abilityInflation = 1.0f + Balance::Economy::kActPriceInflation * (float)(_act - 1);
    const float wareInflation    = 1.0f + 0.15f * (float)(_act - 1);

    // Exactly two class-legal ability offers. Prefer new abilities, then fill
    // from useful upgrades when the class has learned most of its kit.
    std::vector<ShopItem> newAbilityPool;
    std::vector<ShopItem> upgradePool;
    for (auto a : kAllAbilities)
    {
        if (IsUltimateAbility(a)) continue;
        if (_meta != nullptr && !_meta->IsAbilityUnlocked(a)) continue;
        if (!player.ClassAllows(a)) continue;

        ShopItem item;
        item.isAbility   = true;
        item.abilityType = a;
        if (!player.HasLearnedAbility(a))
        {
            item.price = (int)(ShopAbilityPrice(a) * abilityInflation + 0.5f);
            newAbilityPool.push_back(item);
        }
        else if (player.CanUpgradeAbility(a))
        {
            item.upgradesAbility = true;
            item.price = (int)((110 + (player.GetAbilityLevel(a) - 1) * 70) * abilityInflation + 0.5f);
            upgradePool.push_back(item);
        }
    }

    auto shuffleItems = [](std::vector<ShopItem>& items) {
        for (int i = (int)items.size() - 1; i > 0; --i)
            std::swap(items[i], items[GetRandomValue(0, i)]);
    };
    shuffleItems(newAbilityPool);
    shuffleItems(upgradePool);

    // Two class-legal ability cards. Act 1 prioritizes assembling a kit; later
    // acts prioritize upgrades, while the secondary pool fills any open slot.
    int abilityOffers = 0;
    const std::vector<ShopItem>& primary   = (_act <= 1) ? newAbilityPool : upgradePool;
    const std::vector<ShopItem>& secondary = (_act <= 1) ? upgradePool    : newAbilityPool;
    for (int i = 0; i < (int)primary.size() && abilityOffers < 2; ++i)
    {
        _inventory.push_back(primary[i]);
        ++abilityOffers;
    }
    for (int i = 0; i < (int)secondary.size() && abilityOffers < 2; ++i)
    {
        _inventory.push_back(secondary[i]);
        ++abilityOffers;
    }

    // Three real products plus one free contract make up the rest of Zeph's
    // six-card page. If fewer than two legal abilities exist, supplies fill the
    // empty spaces so the grid remains complete.
    std::vector<ShopWareType> warePool = {
        ShopWareType::SharpeningStone, ShopWareType::FreecastSeal,
        ShopWareType::WardCharm, ShopWareType::GildedCompass,
        ShopWareType::EchoSatchel
    };
    if (player.GetArmour() < player.GetMaxArmour())
        warePool.push_back(ShopWareType::IronrootTonic);
    if (player.GetHealthValue() < player.GetMaxHealthValue())
        warePool.push_back(ShopWareType::FieldDressing);
    if (player.GetMana() < player.GetMaxMana())
        warePool.push_back(ShopWareType::ManaPrism);

    for (int i = (int)warePool.size() - 1; i > 0; --i)
        std::swap(warePool[i], warePool[GetRandomValue(0, i)]);

    auto pushWare = [&](ShopWareType type) {
        ShopItem item;
        item.wareType = type;
        item.price = (int)(ShopWarePrice(type) * wareInflation + 0.5f);
        _inventory.push_back(item);
    };
    const int wareCount = 5 - abilityOffers; // contract always occupies the sixth slot
    for (int i = 0; i < std::min(wareCount, (int)warePool.size()); ++i)
        pushWare(warePool[i]);

    ShopItem contract;
    contract.isContract = true;
    contract.contractType = (ShopContractType)GetRandomValue(0, (int)ShopContractType::Count - 1);
    contract.price = 0;
    _inventory.push_back(contract);

    // One mixed page, so shuffle the ability offers among the wares.
    for (int i = (int)_inventory.size() - 1; i > 0; i--)
        std::swap(_inventory[i], _inventory[GetRandomValue(0, i)]);

    // Daily Deal applies only to a purchased ware, never an ability or contract.
    _dailyDealIndex = -1;
    std::vector<int> wareIndices;
    for (int i = 0; i < (int)_inventory.size(); ++i)
        if (!_inventory[i].isAbility && !_inventory[i].isContract &&
            ShopWareDailyDealEligible(_inventory[i].wareType))
            wareIndices.push_back(i);
    if (!wareIndices.empty())
    {
        _dailyDealIndex = wareIndices[GetRandomValue(0, (int)wareIndices.size() - 1)];
        _inventory[_dailyDealIndex].price =
            std::max(1, (int)(_inventory[_dailyDealIndex].price * 0.75f));
    }
}
