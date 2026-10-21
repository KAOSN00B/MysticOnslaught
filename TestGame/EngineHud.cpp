// =============================================================================
// EngineHud.cpp — the top-left HUD panel.
//
// Replaces the old bottom HUD (HP/MP orbs, rage/EXP bars, ability row) so the
// player is never hidden behind it. One compact panel in the top-left corner:
//   HP and MP star bars with their values
//   the class mechanic (RAGE / FAITH / COMBO / MARK) as one orange bar;
//     mana-only classes (Mage, Warlock) have no such row
//   the ability slots, then gold, Echoes, level and XP on the right
//   owned relics to the right of the panel
// The room fills the screen, so the panel and relics fade while the hero, an
// enemy or an enemy shot is under them (UpdateHudCover). There is no corner
// minimap: the dungeon map is held open with Tab / Select instead.
// Art: UI/HUD/, the HUD pack's pieces at native size (the pack ships them 3x),
// drawn at kHudArtScale so every pixel stays crisp. The panel has no backing
// or border (the text carries its own shadow); the minimap uses the full frame
// (DrawHudFrame), see-through.
// =============================================================================

#include "Engine.h"
#include "AssetPaths.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr int   kHudArtScale     = 2;      // native HUD art x2 (pack art is 3x of native)
    constexpr float kPanelPad        = 14.f;   // panel edge to content
    constexpr float kRowH            = 34.f;   // one bar row
    constexpr float kBarValueGap     = 8.f;    // bar to its value text
    constexpr float kSlotGap         = 4.f;    // between ability slots
    constexpr float kSlotRowGap      = 4.f;    // last bar row to the slot row
    constexpr float kRightColumnGap  = 22.f;   // slot row to the gold/Echoes/level column
    constexpr int   kMinSlotColumns  = 4;      // the right column never moves in for fewer slots
    constexpr float kRelicGap        = 4.f;
    constexpr float kRelicOffsetX    = 12.f;   // panel edge to the relic row
    constexpr float kRelicOffsetY    = 8.f;
    constexpr int   kRelicsPerRow    = 8;
    constexpr int   kValueFs         = 20;     // bar values, counters, level
    constexpr int   kSmallFs         = 10;     // key prompts, ability level badges
    constexpr int   kAbilityMaxBadgeLevel = 3; // the level badge turns gold from here (as before)
    constexpr float kLowHpFraction   = 0.30f;  // HP value pulses red at or below this
    constexpr float kLowHpPulseHz    = 1.f / 3.f;
    constexpr float kPrimedPulseRate = 8.f;    // full class mechanic: label pulses toward white
    constexpr float kPrimedWhiteMix  = 0.35f;
    constexpr float kCantCastAlpha   = 0.35f;  // ability icon when it can't be cast
    constexpr float kCooldownShade   = 0.62f;
    constexpr float kHoverShade      = 0.12f;
    constexpr float kShadowAlpha     = 0.75f;  // text drop shadow
    constexpr float kEmptyKeyAlpha   = 0.35f;  // key prompt on an empty slot
    constexpr float kKeyAlpha        = 0.85f;
    // Small placements inside a row or slot (virtual px).
    constexpr float kRowTextDY       = 6.f;    // value text inside a 32px bar row
    constexpr float kKeyDX           = 6.f;    // key prompt from the slot frame's corner: just inside
    constexpr float kKeyDY           = 5.f;    // the gold border, on the dark slot, so it reads
    constexpr float kBadgeInset      = 3.f;    // level badge from the frame's bottom-right
    constexpr float kHoverNameGap    = 6.f;    // panel bottom to the hovered ability's name
    constexpr float kCounterIconDX   = 6.f;    // gold coin inside the right column
    constexpr float kCounterIconDY   = 2.f;
    constexpr float kCounterTextDX   = 36.f;   // counter value after its icon
    constexpr int   kCoinScale       = 3;      // the coin is 7x10: one step larger than the HUD reads better
    constexpr float kLevelTextDX     = 4.f;
    constexpr float kXpBarDX         = -8.f;   // XP slider's arrow caps overhang the column
    constexpr float kXpBarDY         = 28.f;   // below the level text

    // The room fills the screen, so the hero, enemies and their shots can pass
    // under the panel. While anything is under it, the panel fades.
    constexpr float kCoveredAlpha     = 0.3f;   // the panel's opacity while something is under it
    constexpr float kCoverFadeOutRate = 8.f;    // opacity per second, fading out (about 0.1 s)
    constexpr float kCoverFadeInRate  = 3.f;    // and back in, gentler so it doesn't flicker
    constexpr float kCoverMargin      = 12.f;   // px around the panel that already counts as under it

    // Native-pixel geometry of the HUD art (UI/HUD/*.png at 1x).
    constexpr Rectangle kStarBarFill { 11, 7, 42, 2 };   // Health_03_Bar*: the fill strip
    constexpr Rectangle kXpBarFill   { 16, 7, 32, 2 };   // Settings_Bar03: the fill strip
    constexpr float     kStarBarW    = 64.f;             // Health_03 / Settings canvas width
    constexpr float     kSlotArt     = 32.f;             // Inventory_Slot_1 canvas
    constexpr float     kSlotFrameIn = 5.f;              // slot frame starts this far in
    constexpr float     kSlotFrame   = 22.f;             // visible slot frame size
    constexpr float     kSlotBorder  = 2.f;              // frame border, for the inner area
    constexpr float     kIconBox     = 16.f;             // ability icon box (native)
    constexpr int       kFrameCorner = 7;                // Inventory_9Slices corner
    constexpr int       kFrameSeg    = 18;               // its dashed edge piece (repeated)
    constexpr int       kFrameFar    = 25;               // where the far corner starts

    const Color kHpValueColor   = RAYWHITE;
    const Color kLowHpColor     = Color{ 255, 70, 60, 255 };
    const Color kMpValueColor   = Color{ 150, 210, 255, 255 };
    const Color kMechanicColor  = Color{ 255, 150, 50, 255 };
    const Color kGoldColor      = Color{ 255, 214, 90, 255 };
    const Color kEchoColor      = Color{ 255, 140, 220, 255 };
    const Color kLevelColor     = Color{ 150, 210, 255, 255 };
    const Color kBadgeColor     = Color{ 150, 210, 255, 230 };
    const Color kBadgeMaxColor  = GOLD;

    // The opacity the panel draws at (DrawHudPanel sets it from _hudCoverAlpha).
    // Every drawing helper below multiplies its colour by it, so the whole
    // panel fades together.
    float panelAlpha = 1.f;
    Color PanelTint(Color c) { return Fade(c, c.a / 255.f * panelAlpha); }

    void Blit(const Texture2D& t, Rectangle src, float x, float y, int s, Color tint = WHITE)
    {
        DrawTexturePro(t, src, Rectangle{ x, y, src.width * s, src.height * s }, Vector2{ 0, 0 }, 0.f, PanelTint(tint));
    }
    void BlitAll(const Texture2D& t, float x, float y, int s, Color tint = WHITE)
    {
        Blit(t, Rectangle{ 0, 0, (float)t.width, (float)t.height }, x, y, s, tint);
    }
    // A fill strip shares its frame's canvas; draws the left `fraction` of it.
    void FillStrip(const Texture2D& fill, Rectangle strip, float x, float y, int s, float fraction)
    {
        const float w = std::round(strip.width * std::clamp(fraction, 0.f, 1.f));
        if (w > 0.f) Blit(fill, Rectangle{ strip.x, strip.y, w, strip.height }, x + strip.x * s, y + strip.y * s, s);
    }
    void ShadowText(const char* text, float x, float y, int fs, Color c)
    {
        const int off = std::max(1, fs / 10);   // one font pixel
        DrawText(text, (int)x + off, (int)y + off, fs, Fade(BLACK, kShadowAlpha * panelAlpha));
        DrawText(text, (int)x, (int)y, fs, PanelTint(c));
    }
}

void Engine::LoadHudArt()
{
    auto load = [](const char* rel) {
        Texture2D t = LoadTexture(AssetPath(rel).c_str());
        if (t.id == 0) TraceLog(LOG_WARNING, "HUD: missing %s", rel);
        return t;
    };
    _hudArt.starBar   = load("UI/HUD/Health_03.png");
    _hudArt.hpFill    = load("UI/HUD/Health_03_Bar01.png");
    _hudArt.classFill = load("UI/HUD/Health_03_Bar02.png");
    _hudArt.mpFill    = load("UI/HUD/Health_03_Bar03.png");
    _hudArt.slot      = load("UI/HUD/Inventory_Slot_1.png");
    _hudArt.frame     = load("UI/HUD/Inventory_9Slices.png");
    _hudArt.xpBar     = load("UI/HUD/Settings.png");
    _hudArt.xpFill    = load("UI/HUD/Settings_Bar03.png");
    _hudArt.gold      = load("TileSet/SingleGold.png");
    _hudArt.echo      = load("PowerUps/MysticCellSmall.png");
}

void Engine::UnloadHudArt()
{
    for (Texture2D* t : { &_hudArt.starBar, &_hudArt.hpFill, &_hudArt.classFill, &_hudArt.mpFill, &_hudArt.slot,
                          &_hudArt.frame, &_hudArt.xpBar, &_hudArt.xpFill, &_hudArt.gold, &_hudArt.echo })
    {
        if (t->id != 0) UnloadTexture(*t);
        *t = Texture2D{};
    }
}

bool Engine::HudHasClassMechanicRow() const
{
    switch (_player.GetClass())
    {
    case PlayerClass::Warrior: case PlayerClass::Rogue:
    case PlayerClass::Hunter:  case PlayerClass::Paladin: return true;
    default:                                              return false;   // mana-only classes
    }
}

Engine::HudPanelLayout Engine::GetHudPanelLayout() const
{
    const float s = (float)kHudArtScale;
    HudPanelLayout l{};
    l.hasClassRow = HudHasClassMechanicRow();
    l.hasSlots    = !_touchModeActive;   // touch mode keeps its own ability arc and XP bar
    l.barX   = _hudCfg.panelX + kPanelPad;
    l.hpY    = _hudCfg.panelY + kPanelPad;
    l.mpY    = l.hpY + kRowH;
    l.classY = l.mpY + kRowH;
    const float barsEnd = (l.hasClassRow ? l.classY : l.mpY) + kRowH;
    l.valueX = l.barX + kStarBarW * s + kBarValueGap;
    l.slotsY = barsEnd + kSlotRowGap;
    const int columns = std::max(kMinSlotColumns, _player.GetMaxAbilitySlots());
    l.rightX = l.barX + columns * (kSlotArt * s + kSlotGap) + kRightColumnGap;
    const float contentRight  = l.rightX + kStarBarW * s;
    const float contentBottom = l.hasSlots ? l.slotsY + kSlotArt * s : barsEnd;
    l.panel = Rectangle{ _hudCfg.panelX, _hudCfg.panelY,
                         contentRight + kPanelPad - _hudCfg.panelX, contentBottom + kPanelPad - _hudCfg.panelY };
    l.relics = Rectangle{ l.panel.x + l.panel.width + kRelicOffsetX, l.panel.y + kRelicOffsetY, 0.f, 0.f };
    const int relics = _player.GetRelicCount();
    if (relics > 0)
    {
        const float cell = kIconBox * s;
        const int relicColumns = std::min(relics, kRelicsPerRow);
        const int relicRows = (relics + kRelicsPerRow - 1) / kRelicsPerRow;
        l.relics.width  = relicColumns * (cell + kRelicGap) - kRelicGap;
        l.relics.height = relicRows * (cell + kRelicGap) - kRelicGap;
    }
    return l;
}

void Engine::UpdateHudCover(float frameDt)
{
    const HudPanelLayout l = GetHudPanelLayout();
    const Rectangle areas[] = { l.panel, l.relics };
    // Bodies are in world space; the HUD is on screen (the same in a dungeon
    // room, offset by the camera in the village).
    const Vector2 toScreen{ kVirtualWidth * 0.5f - _cameraPos.x, kVirtualHeight * 0.5f - _cameraPos.y };
    auto under = [&](Rectangle body)
    {
        body.x += toScreen.x;
        body.y += toScreen.y;
        for (const Rectangle& a : areas)
            if (a.width > 0.f && CheckCollisionRecs(body, Rectangle{ a.x - kCoverMargin, a.y - kCoverMargin,
                                                                     a.width + kCoverMargin * 2.f, a.height + kCoverMargin * 2.f }))
                return true;
        return false;
    };

    // A square of the hero's visible height, centred on its position, covers its art.
    const float hero = Balance::Visual::kPlayerVisibleHeight;
    const Vector2 heroPos = _player.GetWorldPos();
    bool covered = under(Rectangle{ heroPos.x - hero * 0.5f, heroPos.y - hero * 0.5f, hero, hero });
    for (const auto& enemy : _enemies)
        if (!covered && enemy && enemy->IsActive() && enemy->IsAlive())
            covered = under(enemy->GetHitCollisionRec());
    for (const EnemyProjectile& shot : _enemyProjectiles)
        if (!covered && shot.IsActive())
            covered = under(shot.GetCollisionRec());
    for (const LavaBallProjectile& shot : _lavaBalls)
        if (!covered && shot.IsActive())
            covered = under(shot.GetCollisionRec());

    if (covered) _hudCoverAlpha = std::max(kCoveredAlpha, _hudCoverAlpha - kCoverFadeOutRate * frameDt);
    else         _hudCoverAlpha = std::min(1.f, _hudCoverAlpha + kCoverFadeInRate * frameDt);
}

Rectangle Engine::AbilitySlotRect(int slot) const
{
    // The visible slot frame (the art has a transparent margin around it), so
    // clicks match what the player sees.
    const HudPanelLayout l = GetHudPanelLayout();
    const float s = (float)kHudArtScale;
    const float artX = l.barX + slot * (kSlotArt * s + kSlotGap);
    return Rectangle{ artX + kSlotFrameIn * s, l.slotsY + kSlotFrameIn * s, kSlotFrame * s, kSlotFrame * s };
}

void Engine::DrawHudFrame(Rectangle b, float alpha) const
{
    // Inventory_9Slices: corners as-is, the dashed edge piece repeated (never
    // stretched, which would smear the dashes), the flat centre stretched.
    const Texture2D& t = _hudArt.frame;
    if (t.id == 0) return;
    const int s = kHudArtScale;
    const Color tint = Fade(WHITE, alpha);
    const float c = (float)(kFrameCorner * s);
    const float innerW = b.width - 2.f * c, innerH = b.height - 2.f * c;
    DrawTexturePro(t, Rectangle{ (float)kFrameCorner, (float)kFrameCorner, (float)kFrameSeg, (float)kFrameSeg },
                   Rectangle{ b.x + c, b.y + c, innerW, innerH }, Vector2{ 0, 0 }, 0.f, tint);
    for (float o = 0.f; o < innerW; o += kFrameSeg * s)
    {
        const float part = std::min((float)kFrameSeg, (innerW - o) / s);
        Blit(t, Rectangle{ (float)kFrameCorner, 0, part, (float)kFrameCorner }, b.x + c + o, b.y, s, tint);
        Blit(t, Rectangle{ (float)kFrameCorner, (float)kFrameFar, part, (float)kFrameCorner }, b.x + c + o, b.y + b.height - c, s, tint);
    }
    for (float o = 0.f; o < innerH; o += kFrameSeg * s)
    {
        const float part = std::min((float)kFrameSeg, (innerH - o) / s);
        Blit(t, Rectangle{ 0, (float)kFrameCorner, (float)kFrameCorner, part }, b.x, b.y + c + o, s, tint);
        Blit(t, Rectangle{ (float)kFrameFar, (float)kFrameCorner, (float)kFrameCorner, part }, b.x + b.width - c, b.y + c + o, s, tint);
    }
    const Rectangle corner{ 0, 0, (float)kFrameCorner, (float)kFrameCorner };
    Blit(t, corner, b.x, b.y, s, tint);
    Blit(t, Rectangle{ (float)kFrameFar, 0, corner.width, corner.height }, b.x + b.width - c, b.y, s, tint);
    Blit(t, Rectangle{ 0, (float)kFrameFar, corner.width, corner.height }, b.x, b.y + b.height - c, s, tint);
    Blit(t, Rectangle{ (float)kFrameFar, (float)kFrameFar, corner.width, corner.height }, b.x + b.width - c, b.y + b.height - c, s, tint);
}

void Engine::DrawHudClassMechanic(float barX, float y, float valueX) const
{
    float progress = 0.f;
    const char* label = "";
    bool primed = false;
    switch (_player.GetClass())
    {
    case PlayerClass::Warrior:
        progress = std::clamp(_player.GetRagePercent(), 0.f, 1.f);
        label = TextFormat("RAGE %d%%", (int)std::round(progress * 100.f));
        primed = progress >= 1.f;
        break;
    case PlayerClass::Rogue:
    {
        const int current = _player.GetComboPoints();
        const int maximum = std::max(1, _player.GetMaxComboPoints());
        progress = (float)current / (float)maximum;
        label = TextFormat("COMBO %d/%d", current, maximum);
        primed = current >= maximum;
        break;
    }
    case PlayerClass::Hunter:
    {
        const int required = std::max(1, _player.GetHunterMarkEvery());
        const int current = std::clamp(_hunterShotsSinceMark, 0, required);
        progress = (float)current / (float)required;
        label = TextFormat("MARK %d/%d", current, required);
        primed = current == required - 1;   // the next basic shot marks
        break;
    }
    case PlayerClass::Paladin:
        progress = std::clamp(_player.GetFaithPercent(), 0.f, 1.f);
        label = TextFormat("FAITH %d%%", (int)std::round(progress * 100.f));
        primed = progress >= 1.f;
        break;
    default:
        return;
    }
    BlitAll(_hudArt.starBar, barX, y, kHudArtScale);
    FillStrip(_hudArt.classFill, kStarBarFill, barX, y, kHudArtScale, progress);
    Color labelColor = kMechanicColor;
    if (primed)
    {
        const float pulse = sinf((float)GetTime() * kPrimedPulseRate) * 0.5f + 0.5f;
        labelColor = ColorLerp(kMechanicColor, WHITE, pulse * kPrimedWhiteMix);
    }
    ShadowText(label, valueX, y + kRowTextDY, kValueFs, labelColor);
}

void Engine::DrawHudAbilitySlots(const HudPanelLayout& l) const
{
    const int s = kHudArtScale;
    const Vector2 mouse = GetVirtualMousePos();
    int hoveredSlot = -1;
    for (int i = 0; i < _player.GetMaxAbilitySlots(); ++i)
    {
        const Rectangle frame = AbilitySlotRect(i);
        BlitAll(_hudArt.slot, frame.x - kSlotFrameIn * s, frame.y - kSlotFrameIn * s, s);
        const Rectangle inner{ frame.x + kSlotBorder * s, frame.y + kSlotBorder * s,
                               frame.width - 2.f * kSlotBorder * s, frame.height - 2.f * kSlotBorder * s };
        const AbilityType ability = _player.GetLearnedAbility(i);
        const char* keyPrompt = PromptAbilitySlot(GetPromptModeForUi(), i);
        if (ability == AbilityType::None)
        {
            ShadowText(keyPrompt, frame.x + kKeyDX, frame.y + kKeyDY, kSmallFs, Fade(RAYWHITE, kEmptyKeyAlpha));
            continue;
        }

        const bool onCooldown = _player.IsSlotOnCooldown(i);
        const bool canCast = !onCooldown && _player.GetMana() >= _player.GetAbilityCost(ability);   // includes Overload surcharge
        const bool hovered = CheckCollisionPointRec(mouse, frame);
        if (hovered) hoveredSlot = i;

        // Icon: a whole-number scale when it fits the icon box, else scaled to fit.
        const Texture2D* icon = GetAbilityIcon(ability);
        if (!icon || icon->id == 0) icon = &_abilityIconFireTex;
        const float box = kIconBox * s;
        const float largest = (float)std::max(icon->width, icon->height);
        const float scale = largest <= box ? std::floor(box / largest) : box / largest;
        const float iw = icon->width * scale, ih = icon->height * scale;
        DrawTextureEx(*icon, Vector2{ std::round(frame.x + (frame.width - iw) * 0.5f), std::round(frame.y + (frame.height - ih) * 0.5f) },
                      0.f, scale, PanelTint(canCast ? WHITE : Fade(WHITE, kCantCastAlpha)));
        if (hovered) DrawRectangleRec(inner, PanelTint(Fade(WHITE, kHoverShade)));

        // Cooldown: a dark shade that drains downward, seconds left in the middle.
        if (onCooldown)
        {
            DrawRectangleRec(Rectangle{ inner.x, inner.y, inner.width, inner.height * _player.GetSlotCooldownFraction(i) },
                             PanelTint(Fade(BLACK, kCooldownShade)));
            const char* seconds = TextFormat("%.1f", _player.GetSlotCooldownRemaining(i));
            ShadowText(seconds, frame.x + (frame.width - MeasureText(seconds, kValueFs)) * 0.5f,
                       frame.y + (frame.height - kValueFs) * 0.5f, kValueFs, RAYWHITE);
        }

        // Key prompt last, so the icon and cooldown shade never cover it.
        ShadowText(keyPrompt, frame.x + kKeyDX, frame.y + kKeyDY, kSmallFs, Fade(RAYWHITE, kKeyAlpha));

        const int level = _player.GetAbilityLevel(ability);
        if (level > 1)
        {
            const char* badge = TextFormat("Lv%d", level);
            ShadowText(badge, frame.x + frame.width - MeasureText(badge, kSmallFs) - kBadgeInset,
                       frame.y + frame.height - kSmallFs - kBadgeInset, kSmallFs,
                       level >= kAbilityMaxBadgeLevel ? kBadgeMaxColor : kBadgeColor);
        }
    }
    // The hovered ability's name under the slot row (names no longer fit inside).
    if (hoveredSlot >= 0)
    {
        const Rectangle frame = AbilitySlotRect(hoveredSlot);
        ShadowText(GetAbilityName(_player.GetLearnedAbility(hoveredSlot)), frame.x,
                   l.panel.y + l.panel.height + kHoverNameGap, kValueFs, RAYWHITE);
    }
}

void Engine::DrawOwnedRelics(float startX, float startY) const
{
    // A row of relic icons to the right of the HUD panel, wrapping every
    // kRelicsPerRow. Relics without art show their initial.
    const int s = kHudArtScale;
    const float cell = kIconBox * s;
    for (int i = 0; i < _player.GetRelicCount(); ++i)
    {
        const RelicType type = _player.GetRelicAt(i);
        const float x = startX + (i % kRelicsPerRow) * (cell + kRelicGap);
        const float y = startY + (i / kRelicsPerRow) * (cell + kRelicGap);
        const Texture2D* icon = GetRelicIcon(type);
        if (icon && icon->id != 0)
        {
            const float largest = (float)std::max(icon->width, icon->height);
            const float scale = largest <= cell ? std::floor(cell / largest) : cell / largest;
            DrawTextureEx(*icon, Vector2{ x, y }, 0.f, scale, PanelTint(WHITE));
        }
        else
        {
            const char letter[2] = { GetRelicInfo(type).name[0], '\0' };
            ShadowText(letter, x + cell * 0.25f, y + cell * 0.2f, kValueFs, RAYWHITE);
        }
    }
}

void Engine::DrawHudPanel() const
{
    const HudPanelLayout l = GetHudPanelLayout();
    const int s = kHudArtScale;
    panelAlpha = _hudCoverAlpha;   // faded while something is under it (UpdateHudCover)

    // HP and MP: bars show the smoothed values UpdateHudPresentation advances.
    const float maxHp = _player.GetMaxHealthValue();
    const float curHp = _player.GetHealthValue();
    BlitAll(_hudArt.starBar, l.barX, l.hpY, s);
    FillStrip(_hudArt.hpFill, kStarBarFill, l.barX, l.hpY, s, _displayHpPct);
    Color hpColor = kHpValueColor;
    if (maxHp > 0.f && curHp / maxHp <= kLowHpFraction)
    {
        const float pulse = (sinf((float)GetTime() * 2.f * PI * kLowHpPulseHz) + 1.f) * 0.5f;
        hpColor = ColorLerp(kHpValueColor, kLowHpColor, pulse);
    }
    ShadowText(TextFormat("%.0f/%.0f", curHp, maxHp), l.valueX, l.hpY + kRowTextDY, kValueFs, hpColor);

    BlitAll(_hudArt.starBar, l.barX, l.mpY, s);
    FillStrip(_hudArt.mpFill, kStarBarFill, l.barX, l.mpY, s, _displayManaPct);
    ShadowText(TextFormat("%d/%d", _player.GetMana(), _player.GetMaxMana()), l.valueX, l.mpY + kRowTextDY, kValueFs, kMpValueColor);

    if (l.hasClassRow) DrawHudClassMechanic(l.barX, l.classY, l.valueX);
    if (l.hasSlots)    DrawHudAbilitySlots(l);

    // Right column: gold and Echoes (smoothed counters), then level and XP.
    BlitAll(_hudArt.gold, l.rightX + kCounterIconDX, l.hpY + kCounterIconDY, kCoinScale);
    ShadowText(TextFormat("%d", (int)(_displayGold + 0.5f)), l.rightX + kCounterTextDX, l.hpY + kRowTextDY, kValueFs, kGoldColor);
    BlitAll(_hudArt.echo, l.rightX, l.mpY, s);
    ShadowText(TextFormat("%d", (int)(_displayCells + 0.5f)), l.rightX + kCounterTextDX, l.mpY + kRowTextDY, kValueFs, kEchoColor);
    if (l.hasSlots)
    {
        const bool maxed = _player.GetLevel() >= _player.GetMaxLevel();
        const int curExp = _player.GetExp(), maxExp = _player.GetExpToNext();
        const float xp = maxed ? 1.f : (maxExp > 0 ? std::min(1.f, (float)curExp / (float)maxExp) : 0.f);
        ShadowText(maxed ? "LV MAX" : TextFormat("LV %d", _player.GetLevel()), l.rightX + kLevelTextDX, l.slotsY + kRowTextDY,
                   kValueFs, kLevelColor);
        BlitAll(_hudArt.xpBar, l.rightX + kXpBarDX, l.slotsY + kXpBarDY, s);
        FillStrip(_hudArt.xpFill, kXpBarFill, l.rightX + kXpBarDX, l.slotsY + kXpBarDY, s, xp);
    }

    DrawOwnedRelics(l.relics.x, l.relics.y);
    panelAlpha = 1.f;
}
