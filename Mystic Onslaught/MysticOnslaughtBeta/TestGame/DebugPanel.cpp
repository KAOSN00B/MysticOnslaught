#include "DebugPanel.h"
#include "Character.h"    // UpgradeType

#include <algorithm>
#include <string>
#include <vector>

namespace
{
    const char* GetDebugUpgradeName(UpgradeType type)
    {
        switch (type)
        {
        case UpgradeType::AttackPower:             return "Attack+";
        case UpgradeType::AttackRange:             return "Range+";
        case UpgradeType::MaxHealth:               return "Max HP+";
        case UpgradeType::MaxMana:                 return "Max MP+";
        case UpgradeType::Defense:                 return "Defense+";
        case UpgradeType::MoveSpeed:               return "Speed+";
        case UpgradeType::IronConstitution:        return "Iron Con";
        case UpgradeType::SwiftFeet:               return "Swift Feet";
        case UpgradeType::Ferocity:                return "Ferocity";
        case UpgradeType::ArcaneMind:              return "Arcane Mind";
        case UpgradeType::IronSkin:                return "Iron Skin";
        case UpgradeType::BladeEdge:               return "Blade Edge";
        case UpgradeType::WarGod:                  return "War God";
        case UpgradeType::Resilience:              return "Resilience";
        case UpgradeType::BladeStorm:              return "Blade Storm";
        case UpgradeType::Juggernaut:              return "Juggernaut";
        case UpgradeType::ArcaneColossus:          return "Arc Colossus";
        case UpgradeType::LearnFireSpread:         return "Learn F Spread";
        case UpgradeType::LearnIceSpread:          return "Learn I Spread";
        case UpgradeType::LearnElectricSpread:     return "Learn E Spread";
        case UpgradeType::LearnFireBolt:           return "Learn F Bolt";
        case UpgradeType::LearnIceBolt:            return "Learn I Bolt";
        case UpgradeType::LearnElectricBolt:       return "Learn E Bolt";
        case UpgradeType::LearnFireUltimate:       return "Learn F Ult";
        case UpgradeType::LearnIceUltimate:        return "Learn I Ult";
        case UpgradeType::LearnElectricUltimate:   return "Learn E Ult";
        case UpgradeType::UpgradeFireSpread:       return "Up F Spread";
        case UpgradeType::UpgradeIceSpread:        return "Up I Spread";
        case UpgradeType::UpgradeElectricSpread:   return "Up E Spread";
        case UpgradeType::UpgradeFireBolt:         return "Up F Bolt";
        case UpgradeType::UpgradeIceBolt:          return "Up I Bolt";
        case UpgradeType::UpgradeElectricBolt:     return "Up E Bolt";
        case UpgradeType::UpgradeFireUltimate:     return "Up F Ult";
        case UpgradeType::UpgradeIceUltimate:      return "Up I Ult";
        case UpgradeType::UpgradeElectricUltimate: return "Up E Ult";
        default:                                   return "Upgrade";
        }
    }

    struct DebugButtonSpec
    {
        std::string     label;
        Rectangle       rect{};
        Color           fill{};
        DebugActionKind action = DebugActionKind::None;
        int             value  = 0;
    };

    using ItemList = std::vector<std::pair<std::string, std::pair<DebugActionKind, int>>>;

    void AppendDebugButtons(std::vector<DebugButtonSpec>& out, float padX, float contentW,
                            float& cursorY, int cols, Color fill, const ItemList& items)
    {
        const float gap   = 8.f;
        const float cellH = 30.f;
        const float cellW = (contentW - gap * (cols - 1)) / cols;

        for (int i = 0; i < (int)items.size(); ++i)
        {
            int col = i % cols;
            int row = i / cols;
            out.push_back(DebugButtonSpec{
                items[i].first,
                Rectangle{ padX + col * (cellW + gap), cursorY + row * (cellH + gap), cellW, cellH },
                fill,
                items[i].second.first,
                items[i].second.second
            });
        }

        cursorY += ((int)items.size() + cols - 1) / cols * (cellH + gap) + 8.f;
    }

    // Shared button layout — populates the button list starting at cursorY.
    // Used by both Update() (no drawing) and Draw() (after background is set up).
    void BuildButtonLayout(std::vector<DebugButtonSpec>& buttons, bool godMode,
                           float padX, float contentW, float& cursorY)
    {
        auto skip = [&](float h) { cursorY += h; };  // advances cursor without drawing

        skip(38.f);  // "Run" section header
        buttons.push_back({ godMode ? "God Mode: ON" : "God Mode: OFF",
            { padX, cursorY, contentW, 32.f }, Color{ 128, 60, 40, 210 }, DebugActionKind::None, 0 });
        cursorY += 40.f;
        buttons.push_back({ "Grant 5s Invulnerability",
            { padX, cursorY, contentW, 32.f }, Color{ 80, 110, 180, 210 }, DebugActionKind::GrantInvuln, 0 });
        cursorY += 40.f;
        buttons.push_back({ "Clear Enemies + Continue",
            { padX, cursorY, contentW, 32.f }, Color{ 60, 150, 90, 210 }, DebugActionKind::ClearEnemiesContinue, 0 });
        cursorY += 40.f;

        skip(38.f);  // "Areas" section header
        AppendDebugButtons(buttons, padX, contentW, cursorY, 2, Color{ 88, 78, 122, 220 }, {
            { "Standard", { DebugActionKind::RestartRoom, 0 } },
            { "Elite",    { DebugActionKind::RestartRoom, 1 } },
            { "Rest",     { DebugActionKind::RestartRoom, 2 } },
            { "Treasure", { DebugActionKind::RestartRoom, 3 } },
            { "Shop",     { DebugActionKind::RestartRoom, 4 } },
            { "Boss",     { DebugActionKind::RestartRoom, 5 } },
        });

        skip(38.f);  // "Elite Mechanics" section header
        AppendDebugButtons(buttons, padX, contentW, cursorY, 2, Color{ 150, 74, 130, 220 }, {
            { "Random",  { DebugActionKind::SetEliteMechanic, -1 } },
            { "Cage",    { DebugActionKind::SetEliteMechanic,  0 } },
            { "Links",   { DebugActionKind::SetEliteMechanic,  1 } },
            { "Enrage",  { DebugActionKind::SetEliteMechanic,  2 } },
            { "Leap",    { DebugActionKind::SetEliteMechanic,  3 } },
            { "Hazards", { DebugActionKind::SetEliteMechanic,  4 } },
        });

        skip(38.f);  // "Spawns" section header
        AppendDebugButtons(buttons, padX, contentW, cursorY, 2, Color{ 110, 74, 52, 220 }, {
            { "Add Grunt",   { DebugActionKind::SpawnGrunt,   0 } },
            { "Add Cyclops", { DebugActionKind::SpawnCyclops, 0 } },
            { "Add Ogre",    { DebugActionKind::SpawnOgre,    0 } },
            { "Add Boss",    { DebugActionKind::SpawnBoss,    0 } },
        });

        skip(38.f);  // "Resources" section header
        AppendDebugButtons(buttons, padX, contentW, cursorY, 2, Color{ 60, 112, 92, 220 }, {
            { "HP +10",         { DebugActionKind::Heal,          10 } },
            { "MP +40",         { DebugActionKind::RestoreMana,   40 } },
            { "Gold +25",       { DebugActionKind::AddGold,       25 } },
            { "XP +25",         { DebugActionKind::AddExp,        25 } },
            { "Treasure Cards", { DebugActionKind::TreasureCards,  0 } },
            { "Elite Reward",   { DebugActionKind::EliteReward,    0 } },
            { "Ability Reward", { DebugActionKind::AbilityReward,  0 } },
        });

        skip(38.f);  // "Upgrades" section header
        {
            ItemList items;
            for (int i = 0; i < (int)UpgradeType::Count; ++i)
                items.push_back({ GetDebugUpgradeName((UpgradeType)i), { DebugActionKind::ApplyUpgrade, i } });
            AppendDebugButtons(buttons, padX, contentW, cursorY, 3, Color{ 90, 82, 46, 220 }, items);
        }
    }

} // namespace

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void DebugPanel::Activate()
{
    _active              = true;
    _open                = true;
    _godMode             = false;
    _scrollY             = 0.f;
    _forcedEliteMechanic = -1;
}

void DebugPanel::Deactivate()
{
    _active  = false;
    _open    = false;
    _godMode = false;
}

// ── Update ────────────────────────────────────────────────────────────────────

DebugCommand DebugPanel::Update()
{
    if (IsKeyPressed(KEY_F1) || IsKeyPressed(KEY_F10))
        _open = !_open;

    if (!_open)
        return {};

    const float sw = (float)GetScreenWidth();
    const float sh = (float)GetScreenHeight();
    const Rectangle panel{ sw * 0.67f, 86.f, sw * 0.30f, sh - 126.f };
    const Rectangle contentClip{ panel.x + 12.f, panel.y + 56.f,
                                  panel.width - 24.f, panel.height - 68.f };
    const float padX     = panel.x + 18.f;
    const float contentW = panel.width - 36.f;

    Vector2 mouse = GetMousePosition();

    if (CheckCollisionPointRec(mouse, panel))
        _scrollY = std::clamp(_scrollY - GetMouseWheelMove() * 36.f, 0.f, 2400.f);

    Rectangle closeBtn{ panel.x + panel.width - 42.f, panel.y + 12.f, 28.f, 28.f };
    if (CheckCollisionPointRec(mouse, closeBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        _open = false;
        return {};
    }

    std::vector<DebugButtonSpec> buttons;
    float cursorY = panel.y + 62.f - _scrollY;
    BuildButtonLayout(buttons, _godMode, padX, contentW, cursorY);

    if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        return {};

    for (const auto& btn : buttons)
    {
        bool visible = (btn.rect.y + btn.rect.height >= contentClip.y &&
                        btn.rect.y <= contentClip.y + contentClip.height);
        if (!visible || !CheckCollisionPointRec(mouse, btn.rect))
            continue;

        // God mode toggle is internal state — handle here, no Engine action needed.
        if (btn.action == DebugActionKind::None)
        {
            _godMode = !_godMode;
            return {};
        }

        return DebugCommand{ btn.action, btn.value, true };
    }

    return {};
}

// ── Draw ──────────────────────────────────────────────────────────────────────

void DebugPanel::Draw(int act, int room, const char* roomTypeName) const
{
    if (!_open)
        return;

    const float sw = (float)GetScreenWidth();
    const float sh = (float)GetScreenHeight();
    const Rectangle panel{ sw * 0.67f, 86.f, sw * 0.30f, sh - 126.f };
    const Rectangle contentClip{ panel.x + 12.f, panel.y + 56.f,
                                  panel.width - 24.f, panel.height - 68.f };
    const float padX     = panel.x + 18.f;
    const float contentW = panel.width - 36.f;

    DrawRectangleRounded(panel, 0.03f, 8, Fade(Color{ 24, 20, 16, 255 }, 0.96f));
    DrawRectangleRoundedLines(panel, 0.03f, 8, Fade(Color{ 255, 190, 110, 255 }, 0.38f));
    DrawText("DEBUG PANEL",  (int)(panel.x + 18.f), (int)(panel.y + 14.f), 28, Color{ 255, 210, 150, 255 });
    DrawText("F1 to toggle", (int)(panel.x + 18.f), (int)(panel.y + 40.f), 16, Color{ 190, 175, 150, 220 });

    Rectangle closeBtn{ panel.x + panel.width - 42.f, panel.y + 12.f, 28.f, 28.f };
    DrawRectangleRounded(closeBtn, 0.28f, 6, Fade(Color{ 110, 40, 35, 255 }, 0.95f));
    DrawText("X", (int)(closeBtn.x + 8.f), (int)(closeBtn.y + 3.f), 22, RAYWHITE);

    BeginScissorMode((int)contentClip.x, (int)contentClip.y,
                     (int)contentClip.width, (int)contentClip.height);

    // Rebuild button layout for drawing — section headers are drawn inline here.
    std::vector<DebugButtonSpec> buttons;
    float cursorY = panel.y + 62.f - _scrollY;

    auto section = [&](const char* title)
    {
        DrawText(title, (int)padX, (int)cursorY, 22, Color{ 255, 196, 96, 255 });
        cursorY += 28.f;
        DrawLineEx({ padX, cursorY }, { padX + contentW, cursorY }, 1.5f,
                   Fade(Color{ 255, 196, 96, 255 }, 0.35f));
        cursorY += 10.f;
    };

    section("Run");
    buttons.push_back({ _godMode ? "God Mode: ON" : "God Mode: OFF",
        { padX, cursorY, contentW, 32.f }, Color{ 128, 60, 40, 210 }, DebugActionKind::None, 0 });
    cursorY += 40.f;
    buttons.push_back({ "Grant 5s Invulnerability",
        { padX, cursorY, contentW, 32.f }, Color{ 80, 110, 180, 210 }, DebugActionKind::GrantInvuln, 0 });
    cursorY += 40.f;
    buttons.push_back({ "Clear Enemies + Continue",
        { padX, cursorY, contentW, 32.f }, Color{ 60, 150, 90, 210 }, DebugActionKind::ClearEnemiesContinue, 0 });
    cursorY += 40.f;

    section("Areas");
    AppendDebugButtons(buttons, padX, contentW, cursorY, 2, Color{ 88, 78, 122, 220 }, {
        { "Standard", { DebugActionKind::RestartRoom, 0 } },
        { "Elite",    { DebugActionKind::RestartRoom, 1 } },
        { "Rest",     { DebugActionKind::RestartRoom, 2 } },
        { "Treasure", { DebugActionKind::RestartRoom, 3 } },
        { "Shop",     { DebugActionKind::RestartRoom, 4 } },
        { "Boss",     { DebugActionKind::RestartRoom, 5 } },
    });

    section("Elite Mechanics");
    AppendDebugButtons(buttons, padX, contentW, cursorY, 2, Color{ 150, 74, 130, 220 }, {
        { "Random",  { DebugActionKind::SetEliteMechanic, -1 } },
        { "Cage",    { DebugActionKind::SetEliteMechanic,  0 } },
        { "Links",   { DebugActionKind::SetEliteMechanic,  1 } },
        { "Enrage",  { DebugActionKind::SetEliteMechanic,  2 } },
        { "Leap",    { DebugActionKind::SetEliteMechanic,  3 } },
        { "Hazards", { DebugActionKind::SetEliteMechanic,  4 } },
    });

    section("Spawns");
    AppendDebugButtons(buttons, padX, contentW, cursorY, 2, Color{ 110, 74, 52, 220 }, {
        { "Add Grunt",   { DebugActionKind::SpawnGrunt,   0 } },
        { "Add Cyclops", { DebugActionKind::SpawnCyclops, 0 } },
        { "Add Ogre",    { DebugActionKind::SpawnOgre,    0 } },
        { "Add Boss",    { DebugActionKind::SpawnBoss,    0 } },
    });

    section("Resources");
    AppendDebugButtons(buttons, padX, contentW, cursorY, 2, Color{ 60, 112, 92, 220 }, {
        { "HP +10",         { DebugActionKind::Heal,          10 } },
        { "MP +40",         { DebugActionKind::RestoreMana,   40 } },
        { "Gold +25",       { DebugActionKind::AddGold,       25 } },
        { "XP +25",         { DebugActionKind::AddExp,        25 } },
        { "Treasure Cards", { DebugActionKind::TreasureCards,  0 } },
        { "Elite Reward",   { DebugActionKind::EliteReward,    0 } },
        { "Ability Reward", { DebugActionKind::AbilityReward,  0 } },
    });

    section("Upgrades");
    {
        ItemList items;
        for (int i = 0; i < (int)UpgradeType::Count; ++i)
            items.push_back({ GetDebugUpgradeName((UpgradeType)i), { DebugActionKind::ApplyUpgrade, i } });
        AppendDebugButtons(buttons, padX, contentW, cursorY, 3, Color{ 90, 82, 46, 220 }, items);
    }

    for (const auto& btn : buttons)
    {
        DrawRectangleRounded(btn.rect, 0.20f, 6, btn.fill);
        DrawRectangleRoundedLines(btn.rect, 0.20f, 6, Fade(WHITE, 0.16f));
        int fs = (btn.rect.height > 31.f && btn.rect.width > contentW - 1.f) ? 18 : 15;
        int tw = MeasureText(btn.label.c_str(), fs);
        float textX = (fs == 18) ? (btn.rect.x + 10.f)
                                 : (btn.rect.x + btn.rect.width * 0.5f - tw * 0.5f);
        DrawText(btn.label.c_str(), (int)textX, (int)(btn.rect.y + (fs == 18 ? 6.f : 7.f)),
                 fs, RAYWHITE);
    }

    EndScissorMode();

    const char* footer = TextFormat("Act %d  Room %d  |  %s Area", act, room, roomTypeName);
    DrawText(footer, (int)(panel.x + 18.f), (int)(panel.y + panel.height - 26.f),
             16, Color{ 170, 165, 150, 220 });
}
