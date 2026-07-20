#include "DebugPanel.h"
#include "VirtualCanvas.h"
#include "Character.h"    // UpgradeType

#include <algorithm>
#include <string>
#include <vector>

namespace
{
    AbilityType DebugAbilityForLearnType(UpgradeType type)
    {
        switch (type)
        {
        case UpgradeType::LearnFireSpread:       return AbilityType::FireSpread;
        case UpgradeType::LearnIceSpread:        return AbilityType::IceSpread;
        case UpgradeType::LearnElectricSpread:   return AbilityType::ElectricSpread;
        case UpgradeType::LearnFireBolt:         return AbilityType::FireBolt;
        case UpgradeType::LearnIceBolt:          return AbilityType::IceBolt;
        case UpgradeType::LearnElectricBolt:     return AbilityType::ElectricBolt;
        case UpgradeType::LearnFireUltimate:     return AbilityType::FireUltimate;
        case UpgradeType::LearnIceUltimate:      return AbilityType::IceUltimate;
        case UpgradeType::LearnElectricUltimate: return AbilityType::ElectricUltimate;
        default:                                 return AbilityForLearnType(type);
        }
    }

    AbilityType DebugAbilityForUpgradeType(UpgradeType type)
    {
        switch (type)
        {
        case UpgradeType::UpgradeFireSpread:       return AbilityType::FireSpread;
        case UpgradeType::UpgradeIceSpread:        return AbilityType::IceSpread;
        case UpgradeType::UpgradeElectricSpread:   return AbilityType::ElectricSpread;
        case UpgradeType::UpgradeFireBolt:         return AbilityType::FireBolt;
        case UpgradeType::UpgradeIceBolt:          return AbilityType::IceBolt;
        case UpgradeType::UpgradeElectricBolt:     return AbilityType::ElectricBolt;
        case UpgradeType::UpgradeFireUltimate:     return AbilityType::FireUltimate;
        case UpgradeType::UpgradeIceUltimate:      return AbilityType::IceUltimate;
        case UpgradeType::UpgradeElectricUltimate: return AbilityType::ElectricUltimate;
        default:                                   return AbilityForUpgradeType(type);
        }
    }

    std::string GetDebugUpgradeName(UpgradeType type)
    {
        if (AbilityType ability = DebugAbilityForLearnType(type); ability != AbilityType::None)
            return std::string("Learn ") + GetAbilityName(ability);
        if (AbilityType ability = DebugAbilityForUpgradeType(type); ability != AbilityType::None)
            return std::string(GetAbilityName(ability)) + " +";

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

        // Power Choice pool (names mirror the level-up cards in Engine.cpp).
        case UpgradeType::ManaFlow:                return "Mana Flow";
        case UpgradeType::ClassAttunement:         return "Class Attunement";
        case UpgradeType::Overload:                return "Overload";

        case UpgradeType::MagePyromancy:           return "Pyromancy";
        case UpgradeType::MageInfernalMastery:     return "Infernal Mastery";
        case UpgradeType::MageCryomancy:           return "Cryomancy";
        case UpgradeType::MageGlacialMastery:      return "Glacial Mastery";
        case UpgradeType::MageStormAttunement:     return "Storm Attunement";
        case UpgradeType::MageTempestMastery:      return "Tempest Mastery";
        case UpgradeType::MageComboResonance:      return "Combo Resonance";
        case UpgradeType::MageArcaneHaste:         return "Arcane Haste";

        case UpgradeType::WarriorSmolderingFury:   return "Smoldering Fury";
        case UpgradeType::WarriorFuriousMight:     return "Furious Might";
        case UpgradeType::WarriorBattleTrance:     return "Battle Trance";
        case UpgradeType::WarriorUnbreakable:      return "Unbreakable";
        case UpgradeType::WarriorColossus:         return "Colossus";
        case UpgradeType::WarriorWarlordsReach:    return "Warlord's Reach";
        case UpgradeType::WarriorBattleMeditation: return "Battle Meditation";
        case UpgradeType::WarriorWeaponMaster:     return "Weapon Master";

        case UpgradeType::HunterPredatorsRhythm:   return "Predator's Rhythm";
        case UpgradeType::HunterQuarry:            return "Quarry";
        case UpgradeType::HunterApexPredator:      return "Apex Predator";
        case UpgradeType::HunterFletcher:          return "Fletcher";
        case UpgradeType::HunterTrappersCunning:   return "Trapper's Cunning";
        case UpgradeType::HunterSwiftQuiver:       return "Swift Quiver";
        case UpgradeType::HunterSurvivalist:       return "Survivalist";
        case UpgradeType::HunterFocusedBreathing:  return "Focused Breathing";

        case UpgradeType::RogueDeepReserves:       return "Deep Reserves";
        case UpgradeType::RogueRuthlessFinisher:   return "Ruthless Finisher";
        case UpgradeType::RogueExsanguinate:       return "Exsanguinate";
        case UpgradeType::RogueToxinExpert:        return "Toxin Expert";
        case UpgradeType::RogueMasterPoisoner:     return "Master Poisoner";
        case UpgradeType::RogueFleetFootwork:      return "Fleet Footwork";
        case UpgradeType::RogueShadowConditioning: return "Shadow Conditioning";
        case UpgradeType::RogueOpportunist:        return "Opportunist";

        case UpgradeType::PaladinHolyMight:        return "Holy Might";
        case UpgradeType::PaladinDivineWrath:      return "Divine Wrath";
        case UpgradeType::PaladinZealotsFury:      return "Zealot's Fury";
        case UpgradeType::PaladinMirroredAegis:    return "Mirrored Aegis";
        case UpgradeType::PaladinDevotion:         return "Devotion";
        case UpgradeType::PaladinCrusadersVitality:return "Crusader's Vitality";
        case UpgradeType::PaladinSanctuary:        return "Sanctuary";
        case UpgradeType::PaladinDivineConduit:    return "Divine Conduit";

        case UpgradeType::WarlockGrimHarvest:      return "Grim Harvest";
        case UpgradeType::WarlockDarkPact:         return "Dark Pact";
        case UpgradeType::WarlockSoulBargain:      return "Soul Bargain";
        case UpgradeType::WarlockLingeringMalice:  return "Lingering Malice";
        case UpgradeType::WarlockVoidAttunement:   return "Void Attunement";
        case UpgradeType::WarlockCorruptedVitality:return "Corrupted Vitality";
        case UpgradeType::WarlockSoulConduit:      return "Soul Conduit";
        case UpgradeType::WarlockOccultPower:      return "Occult Power";

        default:                                   return "Upgrade";
        }
    }

    void DrawFittedButtonText(const std::string& label, Rectangle rect, int preferredSize, int minSize)
    {
        int fs = preferredSize;
        const int maxW = (int)(rect.width - 10.f);
        while (fs > minSize && MeasureText(label.c_str(), fs) > maxW)
            --fs;

        int tw = MeasureText(label.c_str(), fs);
        int x = (int)(rect.x + rect.width * 0.5f - tw * 0.5f);
        int y = (int)(rect.y + rect.height * 0.5f - fs * 0.5f - 1.f);
        DrawText(label.c_str(), x, y, fs, RAYWHITE);
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

    // New enemy / boss spawn lists — indices must match kDebugEnemyList and
    // kDebugBossList in Engine.cpp exactly.
    ItemList NewEnemyItems()
    {
        return {
            { "Sk. Archer",  { DebugActionKind::SpawnNewEnemy, 0 } },
            { "Flame Wisp",  { DebugActionKind::SpawnNewEnemy, 1 } },
            { "Slime",       { DebugActionKind::SpawnNewEnemy, 2 } },
            { "Sporeling",   { DebugActionKind::SpawnNewEnemy, 3 } },
            { "Shieldbearer",{ DebugActionKind::SpawnNewEnemy, 4 } },
            { "Phantom",     { DebugActionKind::SpawnNewEnemy, 5 } },
            { "Bomber Imp",  { DebugActionKind::SpawnNewEnemy, 6 } },
            { "Warchief",    { DebugActionKind::SpawnNewEnemy, 7 } },
            { "Living Blade",{ DebugActionKind::SpawnNewEnemy, 8 } },
            { "Infernal",    { DebugActionKind::SpawnNewEnemy, 9 } },
            { "Bonechill",   { DebugActionKind::SpawnNewEnemy, 10 } },
            { "Stormclub",   { DebugActionKind::SpawnNewEnemy, 11 } },
            { "Venomfang",   { DebugActionKind::SpawnNewEnemy, 12 } },
        };
    }

    ItemList NewBossItems()
    {
        return {
            { "Werewolf",    { DebugActionKind::SpawnNewBoss, 0 } },
            { "ChompBug",    { DebugActionKind::SpawnNewBoss, 1 } },
            { "Osiris",      { DebugActionKind::SpawnNewBoss, 2 } },
            { "Titan Guard", { DebugActionKind::SpawnNewBoss, 3 } },
            { "Toxic Vermin",{ DebugActionKind::SpawnNewBoss, 4 } },
            { "Ancient Bear",{ DebugActionKind::SpawnNewBoss, 5 } },
            { "Abyss Slime", { DebugActionKind::SpawnNewBoss, 6 } },
            { "Pumpkin Jack",{ DebugActionKind::SpawnNewBoss, 7 } },
            { "Minotaur",    { DebugActionKind::SpawnNewBoss, 8 } },
        };
    }

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

        skip(38.f);  // "Elite Modifiers" section header
        AppendDebugButtons(buttons, padX, contentW, cursorY, 2, Color{ 150, 74, 130, 220 }, {
            { "Random",      { DebugActionKind::SetEliteMechanic, -1 } },
            { "Cage",        { DebugActionKind::SetEliteMechanic,  0 } },
            { "Guard Links", { DebugActionKind::SetEliteMechanic,  1 } },
            { "Enrage",      { DebugActionKind::SetEliteMechanic,  2 } },
            { "Pressure",    { DebugActionKind::SetEliteMechanic,  3 } },
        });

        skip(38.f);  // "Elite Type" section header
        AppendDebugButtons(buttons, padX, contentW, cursorY, 2, Color{ 104, 96, 150, 220 }, {
            { "Random",    { DebugActionKind::SetEliteType, -1 } },
            { "Ogre",      { DebugActionKind::SetEliteType,  0 } },
            { "Infernal",  { DebugActionKind::SetEliteType,  1 } },
            { "Bonechill", { DebugActionKind::SetEliteType,  2 } },
            { "Stormclub", { DebugActionKind::SetEliteType,  3 } },
            { "Venomfang", { DebugActionKind::SetEliteType,  4 } },
        });

        skip(38.f);  // "Encounter Actions" section header
        AppendDebugButtons(buttons, padX, contentW, cursorY, 2, Color{ 150, 104, 74, 220 }, {
            { "Force Signature", { DebugActionKind::ForceEliteSignature, 0 } },
            { "Advance Phase",   { DebugActionKind::ForceElitePhaseTwo,  0 } },
        });

        skip(38.f);  // "Spawns" section header
        AppendDebugButtons(buttons, padX, contentW, cursorY, 2, Color{ 110, 74, 52, 220 }, {
            { "Add Grunt",   { DebugActionKind::SpawnGrunt,   0 } },
            { "Add Cyclops", { DebugActionKind::SpawnCyclops, 0 } },
            { "Add Ogre",    { DebugActionKind::SpawnOgre,    0 } },
            { "Add Boss",    { DebugActionKind::SpawnBoss,    0 } },
        });

        skip(38.f);  // "New Enemies" section header
        AppendDebugButtons(buttons, padX, contentW, cursorY, 2, Color{ 70, 108, 120, 220 }, NewEnemyItems());

        skip(38.f);  // "New Bosses" section header
        AppendDebugButtons(buttons, padX, contentW, cursorY, 2, Color{ 132, 70, 60, 220 }, NewBossItems());

        skip(38.f);  // "Relics" section header
        AppendDebugButtons(buttons, padX, contentW, cursorY, 2, Color{ 150, 120, 60, 220 }, {
            { "Grant Random", { DebugActionKind::GrantRandomRelic, 0 } },
            { "Grant ALL",    { DebugActionKind::GrantAllRelics,   0 } },
            { "Ascension +1", { DebugActionKind::UnlockAscension,  0 } },
        });

        skip(38.f);  // "Resources" section header
        AppendDebugButtons(buttons, padX, contentW, cursorY, 2, Color{ 60, 112, 92, 220 }, {
            { "HP +10",         { DebugActionKind::Heal,          10 } },
            { "MP +40",         { DebugActionKind::RestoreMana,   40 } },
            { "Gold +25",       { DebugActionKind::AddGold,       25 } },
            { "Spawn Loot",     { DebugActionKind::SpawnLoot,      0 } },
            { "Force Level-Up", { DebugActionKind::ForceLevelUp,   0 } },
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
    _forcedEliteType     = -1;
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

    const float sw = (float)kVirtualWidth;
    const float sh = (float)kVirtualHeight;
    const Rectangle panel{ sw * 0.67f, 86.f, sw * 0.30f, sh - 126.f };
    const Rectangle contentClip{ panel.x + 12.f, panel.y + 56.f,
                                  panel.width - 24.f, panel.height - 68.f };
    const float padX     = panel.x + 18.f;
    const float contentW = panel.width - 36.f;

    Vector2 mouse = GetVirtualMousePos();

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

    const float sw = (float)kVirtualWidth;
    const float sh = (float)kVirtualHeight;
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

    section("Elite Modifiers");
    AppendDebugButtons(buttons, padX, contentW, cursorY, 2, Color{ 150, 74, 130, 220 }, {
        { "Random",      { DebugActionKind::SetEliteMechanic, -1 } },
        { "Cage",        { DebugActionKind::SetEliteMechanic,  0 } },
        { "Guard Links", { DebugActionKind::SetEliteMechanic,  1 } },
        { "Enrage",      { DebugActionKind::SetEliteMechanic,  2 } },
        { "Pressure",    { DebugActionKind::SetEliteMechanic,  3 } },
    });

    section("Elite Type");
    AppendDebugButtons(buttons, padX, contentW, cursorY, 2, Color{ 104, 96, 150, 220 }, {
        { "Random",    { DebugActionKind::SetEliteType, -1 } },
        { "Ogre",      { DebugActionKind::SetEliteType,  0 } },
        { "Infernal",  { DebugActionKind::SetEliteType,  1 } },
        { "Bonechill", { DebugActionKind::SetEliteType,  2 } },
        { "Stormclub", { DebugActionKind::SetEliteType,  3 } },
        { "Venomfang", { DebugActionKind::SetEliteType,  4 } },
    });

    section("Encounter Actions");
    AppendDebugButtons(buttons, padX, contentW, cursorY, 2, Color{ 150, 104, 74, 220 }, {
        { "Force Signature", { DebugActionKind::ForceEliteSignature, 0 } },
        { "Advance Phase",   { DebugActionKind::ForceElitePhaseTwo,  0 } },
    });

    section("Spawns");
    AppendDebugButtons(buttons, padX, contentW, cursorY, 2, Color{ 110, 74, 52, 220 }, {
        { "Add Grunt",   { DebugActionKind::SpawnGrunt,   0 } },
        { "Add Cyclops", { DebugActionKind::SpawnCyclops, 0 } },
        { "Add Ogre",    { DebugActionKind::SpawnOgre,    0 } },
        { "Add Boss",    { DebugActionKind::SpawnBoss,    0 } },
    });

    section("New Enemies");
    AppendDebugButtons(buttons, padX, contentW, cursorY, 2, Color{ 70, 108, 120, 220 }, NewEnemyItems());

    section("New Bosses");
    AppendDebugButtons(buttons, padX, contentW, cursorY, 2, Color{ 132, 70, 60, 220 }, NewBossItems());

    section("Relics");
    AppendDebugButtons(buttons, padX, contentW, cursorY, 2, Color{ 150, 120, 60, 220 }, {
        { "Grant Random", { DebugActionKind::GrantRandomRelic, 0 } },
        { "Grant ALL",    { DebugActionKind::GrantAllRelics,   0 } },
        { "Ascension +1", { DebugActionKind::UnlockAscension,  0 } },
    });

    section("Resources");
    AppendDebugButtons(buttons, padX, contentW, cursorY, 2, Color{ 60, 112, 92, 220 }, {
        { "HP +10",         { DebugActionKind::Heal,          10 } },
        { "MP +40",         { DebugActionKind::RestoreMana,   40 } },
        { "Gold +25",       { DebugActionKind::AddGold,       25 } },
        { "Spawn Loot",     { DebugActionKind::SpawnLoot,      0 } },
        { "Force Level-Up", { DebugActionKind::ForceLevelUp,   0 } },
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
        DrawFittedButtonText(btn.label, btn.rect, fs, 10);
    }

    EndScissorMode();

    const char* footer = TextFormat("Act %d  Room %d  |  %s Area", act, room, roomTypeName);
    DrawText(footer, (int)(panel.x + 18.f), (int)(panel.y + panel.height - 26.f),
             16, Color{ 170, 165, 150, 220 });
}
