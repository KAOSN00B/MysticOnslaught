// =============================================================================
// EngineCharacterSheet.cpp — the second pause screen: the hero's current stats
// and the relics carried. A plain layout on purpose; the UI overhaul restyles it.
// Opened from the pause menu (the Character button, or Q / E / Tab and the
// shoulder buttons, like switching tabs); the same keys or Back return to it.
// =============================================================================
#include "Engine.h"
#include "DamagePolicy.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace
{
    constexpr float kOpenDelay = 0.2f;   // swallows the press that opened the page
    constexpr float kPercent   = 100.f;

    // Layout (virtual 1920 x 1080).
    constexpr int   kTitleFontSize    = 68;
    constexpr float kTitleY           = 40.f;
    constexpr float kPanelY           = 150.f;
    constexpr float kPanelHeight      = 800.f;
    constexpr float kStatsPanelX      = 160.f;
    constexpr float kRelicPanelX      = 1000.f;
    constexpr float kPanelWidth       = 760.f;
    constexpr float kPanelRoundness   = 0.04f;
    constexpr int   kPanelSegments    = 8;
    constexpr float kPanelLine        = 2.f;
    constexpr float kPanelPad         = 36.f;
    constexpr int   kHeaderFontSize   = 36;
    constexpr float kHeaderGap        = 64.f;
    constexpr int   kRowFontSize      = 30;
    constexpr float kRowHeight        = 46.f;
    constexpr float kValueColumn      = 330.f;   // from the panel's left edge
    constexpr int   kNoteFontSize     = 22;
    constexpr float kNoteGap          = 16.f;
    constexpr float kNoteDrop         = 6.f;     // aligns the smaller note with the row
    constexpr int   kRelicFontSize    = 26;
    constexpr float kRelicRowHeight   = 36.f;
    constexpr float kRelicColumnWidth = 344.f;
    constexpr int   kRelicColumns     = 2;
    constexpr int   kHintFontSize     = 22;
    constexpr float kHintBottom       = 56.f;

    constexpr Color kBackTop    = { 30, 20, 40, 255 };
    constexpr Color kBackBottom = { 10, 8, 16, 255 };
    constexpr Color kPanelFill  = { 14, 10, 20, 220 };
    constexpr Color kPanelEdge  = { 110, 90, 130, 255 };
    constexpr Color kGoldText   = { 230, 200, 120, 255 };
    constexpr Color kLabelText  = { 190, 180, 205, 255 };
    constexpr Color kValueText  = { 245, 240, 230, 255 };
    constexpr Color kNoteText   = { 140, 132, 150, 255 };

    std::string Percent(float fraction)
    {
        return std::to_string((int)std::lround(fraction * kPercent)) + "%";
    }
}

void Engine::OpenCharacterSheet(GameState returnTo)
{
    _characterSheetReturnState = returnTo;
    _characterSheetOpenTimer   = kOpenDelay;
    _gameState                 = GameState::CharacterSheet;
}

void Engine::UpdateCharacterSheet(float dt)
{
    if (_characterSheetOpenTimer > 0.f) { _characterSheetOpenTimer -= dt; return; }
    _gamepad.Update(_gamepadBindingsEdit);
    const bool gp = IsGamepadAvailable(0);
    const bool back = IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_TAB) ||
        (gp && (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT) ||
                IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_TRIGGER_1) ||
                IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_TRIGGER_1)));
    if (back)
        _gameState = _characterSheetReturnState;
}

void Engine::DrawCharacterSheet() const
{
    const float sw = (float)kVirtualWidth, sh = (float)kVirtualHeight;
    DrawRectangleGradientV(0, 0, (int)sw, (int)sh, kBackTop, kBackBottom);
    const char* title = "CHARACTER";
    DrawText(title, (int)(sw * 0.5f - MeasureText(title, kTitleFontSize) * 0.5f), (int)kTitleY, kTitleFontSize, kGoldText);

    // A framed panel with its header; returns the y of the first row.
    auto panel = [](float x, const std::string& header) -> float
    {
        const Rectangle r{ x, kPanelY, kPanelWidth, kPanelHeight };
        DrawRectangleRounded(r, kPanelRoundness, kPanelSegments, kPanelFill);
        DrawRectangleRoundedLinesEx(r, kPanelRoundness, kPanelSegments, kPanelLine, kPanelEdge);
        DrawText(header.c_str(), (int)(x + kPanelPad), (int)(kPanelY + kPanelPad), kHeaderFontSize, kGoldText);
        return kPanelY + kPanelPad + kHeaderGap;
    };

    // ── Stats ────────────────────────────────────────────────────────────────
    const PlayerClassInfo& cls = GetPlayerClassInfo(_player.GetClass());
    float y = panel(kStatsPanelX, std::string(cls.name) + TextFormat("  -  Level %d", _player.GetLevel()));
    auto row = [&](const char* label, const std::string& value, const std::string& note = std::string())
    {
        const float valueX = kStatsPanelX + kValueColumn;
        DrawText(label, (int)(kStatsPanelX + kPanelPad), (int)y, kRowFontSize, kLabelText);
        DrawText(value.c_str(), (int)valueX, (int)y, kRowFontSize, kValueText);
        if (!note.empty())
            DrawText(note.c_str(), (int)(valueX + MeasureText(value.c_str(), kRowFontSize) + kNoteGap),
                     (int)(y + kNoteDrop), kNoteFontSize, kNoteText);
        y += kRowHeight;
    };
    const float defense = _player.GetDefense();
    row("Health",        TextFormat("%.0f / %.0f", _player.GetHealthValue(), _player.GetMaxHealthValue()));
    row("Mana",          TextFormat("%d / %d", _player.GetMana(), _player.GetMaxMana()));
    row("Attack",        TextFormat("%.0f", _player.GetAttackPowerValue()));
    row("Ability Power", Percent(_player.GetAbilityPowerMult()));
    row("Defense",       TextFormat("%.0f", defense), "hits -" + Percent(DamagePolicy::DefenseReduction(defense)));
    row("Crit Chance",   Percent(_player.GetCritChance()));
    row("Crit Damage",   "+" + Percent(_player.GetCritDamage()));
    row("Speed",         TextFormat("%.0f", _player.GetMoveSpeedValue()));
    row("Reach",         TextFormat("x%.2f", _player.GetAttackRangeMultiplierValue()));
    if (_player.GetLevel() < _player.GetMaxLevel())
        row("Experience", TextFormat("%d / %d", _player.GetExp(), _player.GetExpToNext()));
    row("Gold",          std::to_string(_player.GetGold()));

    // ── Relics carried, in the order found ───────────────────────────────────
    const int relics = _player.GetRelicCount();
    const float top = panel(kRelicPanelX, TextFormat("Relics  %d", relics));
    const int rows = std::max(1, (int)((kPanelY + kPanelHeight - kPanelPad - top) / kRelicRowHeight));
    if (relics == 0)
        DrawText("None yet", (int)(kRelicPanelX + kPanelPad), (int)top, kRelicFontSize, kNoteText);
    for (int i = 0; i < relics && i < rows * kRelicColumns; ++i)
    {
        const RelicInfo& info = GetRelicInfo(_player.GetRelicAt(i));
        const float x = kRelicPanelX + kPanelPad + (float)(i / rows) * kRelicColumnWidth;
        DrawText(info.name, (int)x, (int)(top + (float)(i % rows) * kRelicRowHeight), kRelicFontSize, kValueText);
    }

    const char* hint = (GetPromptModeForUi() == InputPromptMode::Gamepad)
        ? "B / LB / RB: Back to the pause menu" : "Esc / Q / E: Back to the pause menu";
    DrawText(hint, (int)(sw * 0.5f - MeasureText(hint, kHintFontSize) * 0.5f), (int)(sh - kHintBottom),
             kHintFontSize, kNoteText);
}
