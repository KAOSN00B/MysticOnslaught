#include "WorldMapManager.h"
#include "VirtualCanvas.h"
#include "Character.h"
#include "VirtualCanvas.h"
#include "AbilityType.h"
#include "VirtualCanvas.h"
#include "raylib.h"
#include "VirtualCanvas.h"

#include <algorithm>
#include <cmath>
#include <set>

// ── Biome pool for the 4 selectable middle zones ───────────────────────────────
static constexpr Biome kBiomePool[] = {
    Biome::AncientCastle, Biome::DreamRealm, Biome::Forest,   Biome::Graveyard,
    Biome::Jungle,        Biome::LostCity,   Biome::TheSanctuary, Biome::Wastelands
};
static constexpr int kBiomePoolCount = (int)(sizeof(kBiomePool) / sizeof(kBiomePool[0]));

// ── Layout constants ───────────────────────────────────────────────────────────
static constexpr int   kTotalTiers  = 6;  // 0=Caverns … 5=DemonsInsides
static constexpr int   kMidNodes    = 3;  // nodes per middle tier (1-4)
static constexpr float kFadeDur     = 0.40f;

// normX positions within the graph area for each column (0/1/2)
static constexpr float kNormX[3] = { 0.18f, 0.50f, 0.82f };

// ── Fixed connection table ────────────────────────────────────────────────────
// GetNextConnections(tier, tierIdx, out[3])
// tier 0 (1 node) → all 3 tier-1 nodes
// tier 1-3 (3 nodes) → branching connections
// tier 4 (3 nodes) → tier 5 single node
// tier 5 (1 node) → no outgoing connections
void WorldMapManager::GetNextConnections(int tier, int tierIdx, int outNext[3])
{
    outNext[0] = outNext[1] = outNext[2] = -1;

    if (tier == 0)
    {
        // Single Caverns node connects to all three tier-1 nodes
        outNext[0] = 0; outNext[1] = 1; outNext[2] = 2;
        return;
    }

    if (tier >= 1 && tier <= 3)
    {
        // Left: 0→{0,1}  Middle: 1→{0,1,2}  Right: 2→{1,2}
        if (tierIdx == 0) { outNext[0] = 0; outNext[1] = 1; }
        else if (tierIdx == 1) { outNext[0] = 0; outNext[1] = 1; outNext[2] = 2; }
        else                   { outNext[0] = 1; outNext[1] = 2; }
        return;
    }

    if (tier == 4)
    {
        // All tier-4 nodes lead to the single DemonsInsides node (index 0)
        outNext[0] = 0;
        return;
    }
    // tier 5: no connections
}

// ── Static helpers ─────────────────────────────────────────────────────────────
bool WorldMapManager::PointInNode(Vector2 point, Vector2 nodePos, float radius)
{
    float dx = point.x - nodePos.x;
    float dy = point.y - nodePos.y;
    return (dx * dx + dy * dy) <= (radius * radius);
}

// ── Biome display names ────────────────────────────────────────────────────────
const char* WorldMapManager::GetBiomeName(Biome b)
{
    switch (b)
    {
    case Biome::AncientCastle: return "Ancient Castle";
    case Biome::Caverns:       return "Caverns";
    case Biome::DemonsInsides: return "Demon's Insides";
    case Biome::DreamRealm:    return "Dream Realm";
    case Biome::Forest:        return "Forest";
    case Biome::Graveyard:     return "Graveyard";
    case Biome::Jungle:        return "Jungle";
    case Biome::LostCity:      return "Lost City";
    case Biome::TheSanctuary:  return "The Sanctuary";
    case Biome::Wastelands:    return "Wastelands";
    default:                   return "Unknown";
    }
}

// ── Biome circle colours ───────────────────────────────────────────────────────
Color WorldMapManager::GetBiomeColor(Biome b)
{
    switch (b)
    {
    case Biome::AncientCastle: return Color{ 148, 128, 168, 255 }; // muted purple-grey
    case Biome::Caverns:       return Color{ 160, 100,  50, 255 }; // earthy brown-orange
    case Biome::DemonsInsides: return Color{ 200,  30,  30, 255 }; // deep crimson
    case Biome::DreamRealm:    return Color{ 155,  90, 220, 255 }; // vivid purple
    case Biome::Forest:        return Color{  35, 145,  55, 255 }; // forest green
    case Biome::Graveyard:     return Color{  80,  95,  85, 255 }; // muted grey-green
    case Biome::Jungle:        return Color{  70, 200,  55, 255 }; // bright lime
    case Biome::LostCity:      return Color{ 200, 168,  45, 255 }; // sandy gold
    case Biome::TheSanctuary:  return Color{  75, 165, 230, 255 }; // sky blue
    case Biome::Wastelands:    return Color{ 195,  95,  28, 255 }; // burnt orange
    default:                   return GRAY;
    }
}

// ── Generate ──────────────────────────────────────────────────────────────────
void WorldMapManager::Generate(
    const std::vector<Biome>& completedBiomes,
    const std::vector<int>&   chosenIndices,
    int                       nextZone,
    int                       windowWidth,
    int                       windowHeight)
{
    _nodes.clear();
    _nextZone     = nextZone;
    _hoveredIdx   = -1;
    _confirmIdx   = -1;
    _confirmActive = false;
    _openTimer    = 0.35f;
    _fadeAlpha    = 255.f;
    _fadingOut    = false;
    _done         = false;

    const float sw  = (float)windowWidth;
    const float sh  = (float)windowHeight;
    const float mapL = sw * _mapLeft;
    const float mapR = sw * _mapRight;
    const float mapT = _mapTop;
    const float mapB = sh - _mapBotPad;
    const float rowH = (mapB - mapT) / (float)(kTotalTiers - 1);

    // Build the set of already-played biomes so they cannot appear in new choices.
    std::set<Biome> playedSet(completedBiomes.begin(), completedBiomes.end());

    // Shuffle the available pool to pick fresh biomes for the current tier.
    std::vector<Biome> available;
    available.reserve(kBiomePoolCount);
    for (int i = 0; i < kBiomePoolCount; ++i)
        if (playedSet.find(kBiomePool[i]) == playedSet.end())
            available.push_back(kBiomePool[i]);

    // Fisher-Yates shuffle of available biomes.
    for (int i = (int)available.size() - 1; i > 0; --i)
    {
        int j = GetRandomValue(0, i);
        std::swap(available[i], available[j]);
    }

    // Pick up to kMidNodes unique biomes for the current choice tier.
    // If the pool is smaller than 3, wrap around (pick with repetition from shuffled pool).
    std::vector<Biome> choicesBiomes;
    choicesBiomes.reserve(kMidNodes);
    for (int i = 0; i < kMidNodes; ++i)
    {
        if (available.empty())
        {
            // Fallback: take from full pool, picking least-recently-used
            choicesBiomes.push_back(kBiomePool[GetRandomValue(0, kBiomePoolCount - 1)]);
        }
        else
        {
            choicesBiomes.push_back(available[i % (int)available.size()]);
        }
    }

    // ── Build all nodes ────────────────────────────────────────────────────────

    // Tier 0: Caverns (single node, always completed)
    {
        Node n;
        n.tier        = 0;
        n.tierIdx     = 0;
        n.biome       = Biome::Caverns;
        n.hasBiome    = true;
        n.isCompleted = true;
        n.isLocked    = false;
        n.drawPos     = { mapL + 0.5f * (mapR - mapL), mapB };
        _nodes.push_back(n);
    }

    // Tiers 1-4: middle zones
    for (int tier = 1; tier <= 4; ++tier)
    {
        // Determine which tierIdx (0/1/2) was chosen here (if completed).
        bool tierCompleted = (tier < nextZone);
        int  chosenIdx = tierCompleted
                       ? chosenIndices[(int)std::min((int)chosenIndices.size() - 1, tier - 1)]
                       : -1;

        // Figure out reachability: which nodes in this tier are reachable
        // from the completed path? We trace from tier 0 through each chosen node.
        bool reachable[kMidNodes] = { false, false, false };
        if (tier == nextZone)
        {
            if (nextZone == 1)
            {
                // Caverns (tier 0) connects to all tier-1 nodes
                reachable[0] = reachable[1] = reachable[2] = true;
            }
            else
            {
                // Reachable via the last completed node's connections
                int lastChosen = chosenIndices[(int)chosenIndices.size() - 1];
                int conn[3];
                GetNextConnections(nextZone - 1, lastChosen, conn);
                for (int c : conn)
                    if (c >= 0 && c < kMidNodes)
                        reachable[c] = true;
            }
        }

        for (int i = 0; i < kMidNodes; ++i)
        {
            Node n;
            n.tier    = tier;
            n.tierIdx = i;

            float y = mapB - tier * rowH;
            float x = mapL + kNormX[i] * (mapR - mapL);
            n.drawPos = { x, y };

            if (tierCompleted && i == chosenIdx)
            {
                // This is the node the player actually played
                n.biome       = completedBiomes[tier - 1];
                n.hasBiome    = true;
                n.isCompleted = true;
                n.isLocked    = false;
            }
            else if (tier == nextZone)
            {
                // Current choice tier
                n.biome      = choicesBiomes[i];
                n.hasBiome   = true;
                n.isLocked   = false;
                n.isReachable = reachable[i];
            }
            else
            {
                // Future tier or unchosen completed tier: show as "?"
                n.isLocked = true;
                n.hasBiome = false;
            }

            _nodes.push_back(n);
        }
    }

    // Tier 5: DemonsInsides (single node, always at top)
    {
        Node n;
        n.tier    = 5;
        n.tierIdx = 0;
        n.biome   = Biome::DemonsInsides;
        n.hasBiome = true;
        n.isLocked = true; // shown as coloured but not clickable
        n.drawPos  = { mapL + 0.5f * (mapR - mapL), mapB - 5 * rowH };
        _nodes.push_back(n);
    }
}

// ── Reset ──────────────────────────────────────────────────────────────────────
void WorldMapManager::Reset()
{
    _nodes.clear();
    _hoveredIdx     = -1;
    _confirmIdx     = -1;
    _confirmActive  = false;
    _openTimer      = 0.f;
    _fadeAlpha      = 255.f;
    _fadingOut      = false;
    _done           = false;
    _lastTouchCount = 0;
}

// ── Update ─────────────────────────────────────────────────────────────────────
bool WorldMapManager::Update(float dt)
{
    // Handle fade-in from black
    if (!_fadingOut && _fadeAlpha > 0.f)
    {
        _fadeAlpha -= (255.f / kFadeDur) * dt;
        if (_fadeAlpha < 0.f) _fadeAlpha = 0.f;
    }

    // Handle fade-out after confirm
    if (_fadingOut)
    {
        _fadeAlpha += (255.f / kFadeDur) * dt;
        if (_fadeAlpha >= 255.f)
        {
            _fadeAlpha = 255.f;
            _done = true;
            return true; // signal Engine to load new biome
        }
        return false;
    }

    _openTimer -= dt;
    bool ready = (_openTimer <= 0.f && _fadeAlpha <= 0.f);

    if (!ready) return false;

    // ── Confirmation popup input ───────────────────────────────────────────────
    if (_confirmActive && _confirmIdx >= 0)
    {
        float sw = (float)kVirtualWidth;
        float sh = (float)kVirtualHeight;
        float cx = sw * 0.5f;
        float cy = sh * 0.5f;

        Rectangle popupRect = { cx - _confirmW * 0.5f, cy - _confirmH * 0.5f, _confirmW, _confirmH };
        float btnY      = popupRect.y + _confirmH - _btnH - 22.f;
        float btnW      = (_confirmW - 60.f) * 0.5f;
        Rectangle yesBtn = { popupRect.x + 20.f, btnY, btnW, _btnH };
        Rectangle noBtn  = { popupRect.x + _confirmW * 0.5f + 10.f, btnY, btnW, _btnH };

        // Gather input (mouse or first touch)
        Vector2 clickPos  = { -1.f, -1.f };
        bool    didClick  = false;

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            clickPos = GetVirtualMousePos();
            didClick = true;
        }
        else if (GetTouchPointCount() > 0)
        {
            int nowCount = GetTouchPointCount();
            if (nowCount > _lastTouchCount)
            {
                clickPos = GetVirtualTouchPos(0);
                didClick = true;
            }
            _lastTouchCount = nowCount;
        }
        else
        {
            _lastTouchCount = 0;
        }

        if (didClick)
        {
            if (CheckCollisionPointRec(clickPos, yesBtn))
            {
                // Confirmed — start fade-out
                _selectedBiome   = _nodes[_confirmIdx].biome;
                _selectedTierIdx = _nodes[_confirmIdx].tierIdx;
                _fadingOut       = true;
                _confirmActive   = false;
                _lastTouchCount  = 0; // reset so next open doesn't mis-fire
            }
            else if (CheckCollisionPointRec(clickPos, noBtn) ||
                     !CheckCollisionPointRec(clickPos, popupRect))
            {
                // Cancelled
                _confirmActive  = false;
                _confirmIdx     = -1;
                _lastTouchCount = 0;
            }
        }

        if (IsKeyPressed(KEY_ESCAPE))
        {
            _confirmActive = false;
            _confirmIdx    = -1;
        }

        return false;
    }

    // ── Node hover and click ───────────────────────────────────────────────────
    Vector2 cursorPos = GetVirtualMousePos();
    if (GetTouchPointCount() > 0)
        cursorPos = GetVirtualTouchPos(0);

    _hoveredIdx = -1;

    for (int i = 0; i < (int)_nodes.size(); ++i)
    {
        const Node& n = _nodes[i];
        if (!n.isReachable || n.isLocked) continue;

        float hitR = _nodeRad + 8.f;
        if (PointInNode(cursorPos, n.drawPos, hitR))
        {
            _hoveredIdx = i;

            // Click — show confirm popup
            bool clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
            if (!clicked)
            {
                int now = GetTouchPointCount();
                if (now > _lastTouchCount) clicked = true;
                _lastTouchCount = now;
            }
            else
            {
                _lastTouchCount = 0;
            }

            if (clicked)
            {
                _confirmIdx    = i;
                _confirmActive = true;
            }
            break;
        }
    }

    return false;
}

// ── Draw helpers ───────────────────────────────────────────────────────────────

// Small helper: scrolling green squares background (same as existing map screen).
static void DrawScrollingCheckerboardGreen(float sw, float sh)
{
    const Color dark  = { 30,  74, 42, 255 };
    const Color light = { 48, 112, 66, 255 };
    const int   cell  = 80;
    const float t     = (float)GetTime();
    const int   period = cell * 2;
    int offX = (int)fmodf(t * 18.f, (float)period);
    int offY = (int)fmodf(t * 10.f, (float)period);
    int phaseX = offX / cell, phaseY = offY / cell;
    int pixX   = offX % cell, pixY   = offY % cell;

    for (int gy = -1; gy <= (int)(sh / cell) + 1; ++gy)
        for (int gx = -1; gx <= (int)(sw / cell) + 1; ++gx)
        {
            bool isDark = (((gx + phaseX) + (gy + phaseY)) % 2 + 2) % 2 == 0;
            DrawRectangle(gx * cell - pixX, gy * cell - pixY,
                          cell, cell, isDark ? dark : light);
        }
}

// ── DrawLeftPanel ──────────────────────────────────────────────────────────────
void WorldMapManager::DrawLeftPanel(const Character& player) const
{
    const float sw = (float)kVirtualWidth;
    const float sh = (float)kVirtualHeight;
    (void)sw;

    const float pX  = _panelX;
    const float pY  = _panelY;
    const float pW  = _panelW;
    const float pH  = sh - pY - 98.f;
    const float pad = 28.f;

    DrawRectangleRounded({ pX, pY, pW, pH }, 0.05f, 8, Fade(Color{12, 71, 84, 255}, 0.92f));
    DrawRectangleRoundedLines({ pX, pY, pW, pH }, 0.05f, 8, Fade(Color{130, 235, 255, 255}, 0.30f));

    float cy        = pY + pad;
    float rightEdge = pX + pW - pad;

    // Title
    const float titleFs  = _statTitleFs;
    const float statFs   = _statFs;
    const float statRowH = 44.f;

    DrawText("PLAYER STATS", (int)(pX + pad), (int)cy, (int)titleFs, Color{255, 214, 102, 255});
    cy += titleFs + 6.f;
    DrawLineEx({ pX + pad, cy }, { rightEdge, cy }, 1.f, Fade(Color{130, 235, 255, 255}, 0.55f));
    cy += 14.f;

    auto statRow = [&](const char* lbl, const char* val, Color valCol = RAYWHITE)
    {
        DrawText(lbl, (int)(pX + pad), (int)cy, (int)statFs, Color{188, 228, 238, 228});
        int vw = MeasureText(val, (int)statFs);
        DrawText(val, (int)(rightEdge - vw), (int)cy, (int)statFs, valCol);
        cy += statRowH;
    };

    statRow("Level",
            TextFormat("%d / %d", player.GetLevel(), player.GetMaxLevel()));

    float hpPct = (player.GetMaxHealthValue() > 0.f)
                ? player.GetHealthValue() / player.GetMaxHealthValue() : 0.f;
    Color hpCol = hpPct > 0.6f ? GREEN : hpPct > 0.30f ? YELLOW : RED;
    statRow("HP",
            TextFormat("%d / %d",
                (int)std::ceil(player.GetHealthValue()),
                (int)std::ceil(player.GetMaxHealthValue())), hpCol);

    statRow("MP",
            TextFormat("%d / %d", player.GetMana(), player.GetMaxMana()),
            Color{100, 160, 255, 255});

    statRow("Attack",
            TextFormat("%d", (int)std::ceil((float)player.GetMeleeDamage())));

    statRow("Armour",
            TextFormat("%d / %d", player.GetArmour(), player.GetMaxArmour()),
            Color{120, 180, 255, 255});

    if (player.GetLevel() < player.GetMaxLevel())
        statRow("EXP",
                TextFormat("%d / %d", player.GetExp(), player.GetExpToNext()),
                Color{220, 190, 50, 220});

    // Abilities section
    cy += 10.f;
    DrawLineEx({ pX + pad, cy }, { rightEdge, cy }, 1.f, Fade(Color{130, 235, 255, 255}, 0.24f));
    cy += 14.f;

    const float abilFs   = _abilFs;
    const float abilRowH = 38.f;
    DrawText("ABILITIES", (int)(pX + pad), (int)cy, (int)titleFs, Color{255, 214, 102, 255});
    cy += titleFs + 8.f;

    int slotCount = player.GetMaxAbilitySlots();
    bool anyAbility = false;
    for (int i = 0; i < slotCount; ++i)
    {
        AbilityType ability = player.GetLearnedAbility(i);
        if (ability == AbilityType::None) continue;
        anyAbility = true;
        const char* abilName = GetAbilityName(ability);
        DrawText(TextFormat("%d.", i + 1), (int)(pX + pad), (int)cy, (int)abilFs, Color{180, 220, 240, 200});
        DrawText(abilName, (int)(pX + pad + 32.f), (int)cy, (int)abilFs, RAYWHITE);
        cy += abilRowH;
    }
    if (!anyAbility)
        DrawText("None equipped", (int)(pX + pad), (int)cy, (int)abilFs, Color{140, 140, 150, 200});
}

// ── DrawRightPanel ─────────────────────────────────────────────────────────────
void WorldMapManager::DrawRightPanel() const
{
    const float sw = (float)kVirtualWidth;
    const float sh = (float)kVirtualHeight;

    const float jX  = sw * _journeyX;
    const float jY  = _panelY;
    const float jW  = sw - jX - 48.f;
    const float jH  = sh - jY - 98.f;
    const float pad = _journeyPad;

    DrawRectangleRounded({ jX, jY, jW, jH }, 0.05f, 8, Fade(Color{12, 71, 84, 255}, 0.92f));
    DrawRectangleRoundedLines({ jX, jY, jW, jH }, 0.05f, 8, Fade(Color{130, 235, 255, 255}, 0.30f));

    float cy = jY + pad;

    DrawText("JOURNEY", (int)(jX + pad), (int)cy, (int)_journeyTitleFs, Color{255, 214, 102, 255});
    cy += _journeyTitleFs + 6.f;
    DrawLineEx({ jX + pad, cy }, { jX + jW - pad, cy }, 1.f, Fade(Color{130, 235, 255, 255}, 0.55f));
    cy += 14.f;

    // Count completed nodes to determine if we've started
    bool anyCompleted = false;
    for (const auto& n : _nodes)
        if (n.isCompleted) { anyCompleted = true; break; }

    if (!anyCompleted)
    {
        DrawText("Journey begins...", (int)(jX + pad), (int)cy, 24, Color{180, 220, 240, 160});
        return;
    }

    // Draw completed biomes as coloured circles in a vertical list
    const float circleR     = _journeyCircleR;
    const float rowH        = circleR * 2.f + _journeyGap;
    const float textX       = jX + pad + circleR * 2.f + 14.f;
    const float maxTextW    = jX + jW - pad - textX;
    const int   nameFs      = (int)_journeyNameFs;
    (void)maxTextW;

    for (const auto& n : _nodes)
    {
        if (!n.isCompleted || !n.hasBiome) continue;

        float circleCX = jX + pad + circleR;
        float circleCY = cy + circleR;

        Color col = GetBiomeColor(n.biome);

        // Filled circle + border
        DrawCircleV({ circleCX, circleCY }, circleR, Fade(col, 0.65f));
        DrawCircleLines((int)circleCX, (int)circleCY, circleR, col);

        // Checkmark in centre
        float ck = circleR * 0.38f;
        DrawLineEx({ circleCX - ck, circleCY },
                   { circleCX,      circleCY + ck }, 2.5f, WHITE);
        DrawLineEx({ circleCX,      circleCY + ck },
                   { circleCX + ck * 1.3f, circleCY - ck }, 2.5f, WHITE);

        // Biome name
        DrawText(GetBiomeName(n.biome),
                 (int)textX, (int)(circleCY - nameFs * 0.5f),
                 nameFs, RAYWHITE);

        cy += rowH;

        if (cy + rowH > jY + jH - pad) break; // don't overflow panel
    }
}

// ── DrawConfirmPopup ───────────────────────────────────────────────────────────
void WorldMapManager::DrawConfirmPopup(float sw, float sh) const
{
    if (!_confirmActive || _confirmIdx < 0 || _confirmIdx >= (int)_nodes.size())
        return;

    const Node& chosen = _nodes[_confirmIdx];
    const char* biomeName = chosen.hasBiome ? GetBiomeName(chosen.biome) : "?";
    Color biomeCol = chosen.hasBiome ? GetBiomeColor(chosen.biome) : GRAY;

    float cx = sw * 0.5f;
    float cy = sh * 0.5f;

    Rectangle popupRect = { cx - _confirmW * 0.5f, cy - _confirmH * 0.5f, _confirmW, _confirmH };

    // Dark panel with teal border
    DrawRectangleRounded(popupRect, 0.08f, 8, Fade(Color{8, 22, 32, 255}, 0.96f));
    DrawRectangleRoundedLines(popupRect, 0.08f, 8, Fade(Color{130, 235, 255, 255}, 0.60f));

    // "Are you sure you want to go to"
    const char* line1 = "Are you sure you want to go to";
    int l1W = MeasureText(line1, (int)(_confirmFs * 0.75f));
    DrawText(line1, (int)(cx - l1W * 0.5f),
             (int)(popupRect.y + 24.f), (int)(_confirmFs * 0.75f),
             Color{206, 242, 255, 220});

    // Biome name with its colour
    int nameW = MeasureText(biomeName, (int)_confirmFs);
    DrawText(biomeName, (int)(cx - nameW * 0.5f),
             (int)(popupRect.y + 24.f + _confirmFs * 0.85f + 10.f),
             (int)_confirmFs, biomeCol);

    // Colour dot beside name
    DrawCircleV(
        { cx - nameW * 0.5f - _confirmFs * 0.6f,
          popupRect.y + 24.f + _confirmFs * 0.85f + 10.f + _confirmFs * 0.45f },
        _confirmFs * 0.35f, biomeCol);

    // Buttons
    float btnY  = popupRect.y + _confirmH - _btnH - 22.f;
    float btnW  = (_confirmW - 60.f) * 0.5f;
    Rectangle yesBtn = { popupRect.x + 20.f, btnY, btnW, _btnH };
    Rectangle noBtn  = { popupRect.x + _confirmW * 0.5f + 10.f, btnY, btnW, _btnH };

    Vector2 mouse = GetVirtualMousePos();
    if (GetTouchPointCount() > 0) mouse = GetVirtualTouchPos(0);

    bool yesHov = CheckCollisionPointRec(mouse, yesBtn);
    bool noHov  = CheckCollisionPointRec(mouse, noBtn);

    DrawRectangleRounded(yesBtn, 0.2f, 6,
        yesHov ? Color{60, 200, 100, 255} : Color{30, 120, 60, 255});
    DrawRectangleRoundedLines(yesBtn, 0.2f, 6, Color{100, 255, 140, 200});
    DrawRectangleRounded(noBtn, 0.2f, 6,
        noHov ? Color{180, 80, 80, 255} : Color{100, 40, 40, 255});
    DrawRectangleRoundedLines(noBtn, 0.2f, 6, Color{255, 120, 120, 180});

    const char* yesLabel = "YES, LET'S GO";
    const char* noLabel  = "NOT YET";
    const int   btnFs    = (int)(_btnH * 0.45f);

    int yw = MeasureText(yesLabel, btnFs);
    DrawText(yesLabel, (int)(yesBtn.x + yesBtn.width * 0.5f - yw * 0.5f),
             (int)(yesBtn.y + yesBtn.height * 0.5f - btnFs * 0.5f), btnFs, WHITE);

    int nw = MeasureText(noLabel, btnFs);
    DrawText(noLabel, (int)(noBtn.x + noBtn.width * 0.5f - nw * 0.5f),
             (int)(noBtn.y + noBtn.height * 0.5f - btnFs * 0.5f), btnFs, WHITE);
}

// ── Draw ───────────────────────────────────────────────────────────────────────
void WorldMapManager::Draw(const Character& player) const
{
    const float sw = (float)kVirtualWidth;
    const float sh = (float)kVirtualHeight;

    // Scrolling green squares background
    DrawScrollingCheckerboardGreen(sw, sh);

    // Header
    const char* header = TextFormat("Zone %d of 6  —  Choose Your Path", _nextZone);
    int hSz = (int)_headerFs;
    int hW  = MeasureText(header, hSz);
    float hcx = sw * _headerX;
    DrawText(header, (int)(hcx - hW * 0.5f), (int)_headerY, hSz, Color{255, 214, 102, 255});

    const char* sub = "Select a biome to venture into next";
    int subSz = (int)_subFs;
    int subW  = MeasureText(sub, subSz);
    DrawText(sub, (int)(hcx - subW * 0.5f), (int)_subY, subSz, Color{206, 242, 255, 210});

    // Panels
    DrawLeftPanel(player);
    DrawRightPanel();

    // ── Connection lines ───────────────────────────────────────────────────────
    // Helper: find node by tier and tierIdx
    auto findNode = [&](int tier, int tidx) -> const Node*
    {
        for (const auto& n : _nodes)
            if (n.tier == tier && n.tierIdx == tidx)
                return &n;
        return nullptr;
    };

    for (int tier = 0; tier < kTotalTiers - 1; ++tier)
    {
        int srcCount = (tier == 0 || tier == 5) ? 1 : kMidNodes;
        for (int si = 0; si < srcCount; ++si)
        {
            const Node* src = findNode(tier, si);
            if (!src) continue;

            int conn[3];
            GetNextConnections(tier, si, conn);

            for (int cidx = 0; cidx < 3; ++cidx)
            {
                int dstTierIdx = conn[cidx];
                if (dstTierIdx < 0) break;

                const Node* dst = findNode(tier + 1, dstTierIdx);
                if (!dst) continue;

                // Line colour: bright for completed path, dim for everything else
                bool onCompletedPath = (src->isCompleted && dst->isCompleted);
                bool toCurrentChoice = (src->isCompleted && dst->isReachable);

                Color lc;
                if (onCompletedPath)
                    lc = Color{ 255, 214, 102, 200 };  // gold — walked path
                else if (toCurrentChoice)
                    lc = Color{ 130, 235, 255, 160 };  // teal — reachable choice
                else
                    lc = Color{ 80, 80, 90, 80 };      // dark grey — locked path

                DrawLineEx(src->drawPos, dst->drawPos, _lineThick, lc);
            }
        }
    }

    // ── Nodes ──────────────────────────────────────────────────────────────────
    bool ready = (_openTimer <= 0.f && _fadeAlpha <= 0.f && !_confirmActive);
    Vector2 cursorPos = GetVirtualMousePos();
    if (GetTouchPointCount() > 0) cursorPos = GetVirtualTouchPos(0);

    for (int i = 0; i < (int)_nodes.size(); ++i)
    {
        const Node& n = _nodes[i];
        bool hovered = (ready && _hoveredIdx == i);

        // ── Visual state ─────────────────────────────────────────────────────
        Color col;
        float fillAlpha;
        float ringAlpha;

        if (n.isCompleted && n.hasBiome)
        {
            col       = GetBiomeColor(n.biome);
            fillAlpha = 0.55f;
            ringAlpha = 1.0f;
        }
        else if (!n.isLocked && n.isReachable && n.hasBiome)
        {
            col       = GetBiomeColor(n.biome);
            fillAlpha = 0.75f;
            ringAlpha = 1.0f;
        }
        else if (n.tier == 5 && n.hasBiome)
        {
            // DemonsInsides: always show its colour but slightly muted
            col       = GetBiomeColor(n.biome);
            fillAlpha = 0.40f;
            ringAlpha = 0.70f;
        }
        else
        {
            // Locked/future/unreachable: grey
            col       = Color{ 90, 90, 100, 255 };
            fillAlpha = 0.30f;
            ringAlpha = 0.45f;
        }

        // ── Pre-compute label so the bg box can be sized to fit ──────────────
        const char* label     = nullptr;
        bool        showLabel = false;

        if (n.isCompleted && n.hasBiome)
        {
            label     = GetBiomeName(n.biome);
            showLabel = true;
        }
        else if (!n.isLocked && n.isReachable && n.hasBiome)
        {
            label     = GetBiomeName(n.biome);
            showLabel = true;
        }
        else if (n.tier == 5 && n.hasBiome && hovered)
        {
            label     = GetBiomeName(n.biome);
            showLabel = true;
        }
        else if (n.isLocked)
        {
            label     = "?";
            showLabel = true;
        }

        int labelFs = 0;
        if (showLabel && label)
            labelFs = (n.isLocked) ? (int)(_labelFs * 0.8f) : (int)_labelFs;

        float effectiveR = _nodeRad;

        // ── Pulse glow behind reachable nodes (drawn outside the bg box) ─────
        if (n.isReachable && !n.isLocked && !n.isCompleted)
        {
            float glow = 0.12f + 0.08f * sinf((float)GetTime() * 3.2f);
            DrawCircleV(n.drawPos, effectiveR + 16.f, Fade(col, glow));
        }

        // ── Hover ring (drawn outside the bg box) ────────────────────────────
        if (hovered)
        {
            DrawCircleV(n.drawPos, effectiveR + 8.f, Fade(col, 0.35f));
            effectiveR += 3.f;
        }

        // ── Black semi-transparent background behind circle only ─────────────
        {
            float bgPad = _nodeBgPad;
            float bgX   = n.drawPos.x - effectiveR - bgPad;
            float bgY   = n.drawPos.y - effectiveR - bgPad;
            float bgW   = (effectiveR + bgPad) * 2.f;
            float bgH   = (effectiveR + bgPad) * 2.f;
            DrawRectangleRounded({ bgX, bgY, bgW, bgH }, 0.25f, 6,
                                 Fade(BLACK, 0.55f));
        }

        // ── Fill and outline ─────────────────────────────────────────────────
        DrawCircleV(n.drawPos, effectiveR, Fade(col, fillAlpha));
        DrawCircleLines((int)n.drawPos.x, (int)n.drawPos.y,
                        effectiveR, Fade(col, ringAlpha));

        // ── Completed check-mark overlay ─────────────────────────────────────
        if (n.isCompleted)
        {
            float ck = effectiveR * 0.38f;
            DrawLineEx({ n.drawPos.x - ck, n.drawPos.y },
                       { n.drawPos.x,      n.drawPos.y + ck }, 2.5f, Fade(WHITE, 0.75f));
            DrawLineEx({ n.drawPos.x,      n.drawPos.y + ck },
                       { n.drawPos.x + ck * 1.3f, n.drawPos.y - ck }, 2.5f, Fade(WHITE, 0.75f));
        }

        // ── Label below node — separate box, not touching the circle box ────
        if (showLabel && label)
        {
            Color labelCol = n.isLocked
                ? Color{120, 120, 130, 160}
                : (n.isCompleted ? Fade(col, 0.80f) : col);

            int         lw    = MeasureText(label, labelFs);
            const float lbPad = 4.f;  // padding inside the label bg box
            const float lbGap = 3.f;  // gap between circle box bottom and label box top
            float lbX = n.drawPos.x - lw * 0.5f - lbPad;
            float lbY = n.drawPos.y + effectiveR + _nodeBgPad + lbGap;
            DrawRectangleRounded(
                { lbX, lbY, (float)lw + lbPad * 2.f, (float)labelFs + lbPad * 2.f },
                0.3f, 4, Fade(BLACK, 0.55f));
            DrawText(label,
                     (int)(n.drawPos.x - lw * 0.5f),
                     (int)(lbY + lbPad),
                     labelFs, labelCol);
        }
    }

    // Confirm popup (drawn on top of everything)
    DrawConfirmPopup(sw, sh);

    // Bottom hint
    if (!_confirmActive)
    {
        const char* hint = "Click a highlighted biome to travel there";
        int hintFs = (int)_hintFs;
        int hintW  = MeasureText(hint, hintFs);
        DrawText(hint, (int)(sw * 0.5f - hintW * 0.5f),
                 (int)(sh - _hintY), hintFs, Color{173, 223, 236, 170});
    }

    // Fade overlay (black on entry and exit)
    if (_fadeAlpha > 0.f)
        DrawRectangle(0, 0, (int)sw, (int)sh, Fade(BLACK, _fadeAlpha / 255.f));
}

// ── Debug editor ───────────────────────────────────────────────────────────────
void WorldMapManager::UpdateEditor()
{
    constexpr int kN = 29;
    float* vars[kN] = {
        &_mapLeft, &_mapRight, &_mapTop, &_mapBotPad,
        &_nodeRad, &_lineThick, &_labelFs,
        &_confirmW, &_confirmH, &_confirmFs, &_btnH,
        &_headerX, &_headerY, &_headerFs,
        &_subY, &_subFs, &_hintY, &_hintFs,
        &_panelW, &_journeyX, &_journeyPad, &_journeyCircleR,
        &_journeyTitleFs, &_journeyGap,
        &_statTitleFs, &_statFs, &_abilFs, &_journeyNameFs,
        &_nodeBgPad
    };
    // Indices that are fractional (0–1) get a finer step
    auto isFractional = [](int idx) {
        return idx == 0 || idx == 1 || idx == 11 || idx == 19;
    };

    if (IsKeyPressed(KEY_UP))   _editorSelIdx = (_editorSelIdx - 1 + kN + kN) % kN;
    if (IsKeyPressed(KEY_DOWN)) _editorSelIdx = (_editorSelIdx + 1) % kN;

    float step = isFractional(_editorSelIdx) ? 0.005f : 1.f;
    if (IsKeyDown(KEY_RIGHT)) *vars[_editorSelIdx] += step;
    if (IsKeyDown(KEY_LEFT))  *vars[_editorSelIdx] -= step;

    if (IsKeyPressed(KEY_S))
    {
        TraceLog(LOG_INFO, "=== WorldMap Editor Export ===");
        TraceLog(LOG_INFO, "_mapLeft        = %.3ff;", _mapLeft);
        TraceLog(LOG_INFO, "_mapRight       = %.3ff;", _mapRight);
        TraceLog(LOG_INFO, "_mapTop         = %.2ff;", _mapTop);
        TraceLog(LOG_INFO, "_mapBotPad      = %.2ff;", _mapBotPad);
        TraceLog(LOG_INFO, "_nodeRad        = %.2ff;", _nodeRad);
        TraceLog(LOG_INFO, "_lineThick      = %.2ff;", _lineThick);
        TraceLog(LOG_INFO, "_labelFs        = %.2ff;", _labelFs);
        TraceLog(LOG_INFO, "_confirmW       = %.2ff;", _confirmW);
        TraceLog(LOG_INFO, "_confirmH       = %.2ff;", _confirmH);
        TraceLog(LOG_INFO, "_confirmFs      = %.2ff;", _confirmFs);
        TraceLog(LOG_INFO, "_btnH           = %.2ff;", _btnH);
        TraceLog(LOG_INFO, "_headerX        = %.3ff;", _headerX);
        TraceLog(LOG_INFO, "_headerY        = %.2ff;", _headerY);
        TraceLog(LOG_INFO, "_headerFs       = %.2ff;", _headerFs);
        TraceLog(LOG_INFO, "_subY           = %.2ff;", _subY);
        TraceLog(LOG_INFO, "_subFs          = %.2ff;", _subFs);
        TraceLog(LOG_INFO, "_hintY          = %.2ff;", _hintY);
        TraceLog(LOG_INFO, "_hintFs         = %.2ff;", _hintFs);
        TraceLog(LOG_INFO, "_panelW         = %.2ff;", _panelW);
        TraceLog(LOG_INFO, "_journeyX       = %.3ff;", _journeyX);
        TraceLog(LOG_INFO, "_journeyPad     = %.2ff;", _journeyPad);
        TraceLog(LOG_INFO, "_journeyCircleR = %.2ff;", _journeyCircleR);
        TraceLog(LOG_INFO, "_journeyTitleFs = %.2ff;", _journeyTitleFs);
        TraceLog(LOG_INFO, "_journeyGap     = %.2ff;", _journeyGap);
        TraceLog(LOG_INFO, "_statTitleFs    = %.2ff;", _statTitleFs);
        TraceLog(LOG_INFO, "_statFs         = %.2ff;", _statFs);
        TraceLog(LOG_INFO, "_abilFs         = %.2ff;", _abilFs);
        TraceLog(LOG_INFO, "_journeyNameFs  = %.2ff;", _journeyNameFs);
        TraceLog(LOG_INFO, "_nodeBgPad      = %.2ff;", _nodeBgPad);
    }
}

void WorldMapManager::DrawEditor() const
{
    constexpr int kN = 29;
    const char* varNames[kN] = {
        "0  Map Left (frac)",
        "1  Map Right (frac)",
        "2  Map Top",
        "3  Map Bot Pad",
        "4  Node Radius",
        "5  Line Thickness",
        "6  Label Font",
        "7  Confirm Width",
        "8  Confirm Height",
        "9  Confirm Font",
        "10 Button Height",
        "11 Header X (frac)",
        "12 Header Y",
        "13 Header Font",
        "14 Sub Y",
        "15 Sub Font",
        "16 Hint Y",
        "17 Hint Font",
        "18 Left Panel W",
        "19 Journey X (frac)",
        "20 Journey Pad",
        "21 Journey Circle R",
        "22 Journey Title Font",
        "23 Journey Gap",
        "24 Stat Title Font",
        "25 Stat Font",
        "26 Ability Font",
        "27 Journey Name Font",
        "28 Node Bg Pad",
    };
    const float vars[kN] = {
        _mapLeft, _mapRight, _mapTop, _mapBotPad,
        _nodeRad, _lineThick, _labelFs,
        _confirmW, _confirmH, _confirmFs, _btnH,
        _headerX, _headerY, _headerFs,
        _subY, _subFs, _hintY, _hintFs,
        _panelW, _journeyX, _journeyPad, _journeyCircleR,
        _journeyTitleFs, _journeyGap,
        _statTitleFs, _statFs, _abilFs, _journeyNameFs,
        _nodeBgPad
    };

    constexpr float rowH = 30.f;
    constexpr float panW = 300.f;
    const float panH = 42.f + kN * rowH;
    const float panX = 10.f, panY = 10.f;

    DrawRectangle((int)panX, (int)panY, (int)panW, (int)panH, Fade(BLACK, 0.84f));
    DrawRectangleLines((int)panX, (int)panY, (int)panW, (int)panH, DARKGRAY);
    DrawText("WORLD MAP EDITOR  [9] close", (int)(panX + 8.f), (int)(panY + 5.f), 11, GRAY);
    DrawText("[UP/DOWN] sel  [L/R] nudge  [S] export",
             (int)(panX + 8.f), (int)(panY + 19.f), 10, DARKGRAY);

    for (int i = 0; i < kN; ++i)
    {
        float ry  = panY + 38.f + i * rowH;
        bool  sel = (i == _editorSelIdx);
        Color col = sel ? YELLOW : WHITE;

        if (sel)
            DrawText("->", (int)(panX + 4.f), (int)(ry + rowH * 0.5f - 7.f), 13, YELLOW);
        DrawText(varNames[i], (int)(panX + 26.f), (int)(ry + rowH * 0.5f - 7.f), 13, col);

        const char* valStr = TextFormat("%.3f", vars[i]);
        int valW = MeasureText(valStr, 13);
        DrawText(valStr, (int)(panX + panW - 6.f - valW),
                 (int)(ry + rowH * 0.5f - 7.f), 13, col);
    }
}
