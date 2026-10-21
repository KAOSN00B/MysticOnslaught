// =============================================================================
// EngineGrimoire.cpp — the Grimoire (relics, bestiary, run chronicle, feats).
//
// Opened from the pause menu, in the village or mid-run. Four tabs:
//   RELICS     every relic, named and explained once a run has carried it
//   BESTIARY   every creature, described once you have slain one
//   CHRONICLE  the run history written by the run summary (run_history.cfg)
//   FEATS      the in-game achievements and what each one asks
// Discoveries and feats are stored in the profile (MetaProgression) and only
// count in real runs: never in debug sessions, the prologue or editor tests.
// =============================================================================

#include "Engine.h"
#include "Grimoire.h"
#include "RunLog.h"
#include "SaveFiles.h"
#include "SaveStore.h"
#include "WorldMapManager.h"
#include "raymath.h"

#include <algorithm>
#include <climits>
#include <string>
#include <vector>

namespace
{
    constexpr int kTabCount = 4;
    const char* const kTabNames[kTabCount] = { "RELICS", "BESTIARY", "CHRONICLE", "FEATS" };
    constexpr int kRelicRows = 20;      // relic grid: 3 columns of 20
    constexpr int kCreatureRows = 13;   // bestiary grid: 2 columns of 13
    constexpr int kFeatRows = 8;        // feats: 2 columns of 8

    int EntryCount(int tab)
    {
        switch (tab)
        {
        case 0:  return (int)RelicType::Count;
        case 1:  return BestiaryCount();
        case 3:  return (int)Feat::Count;
        default: return 0;
        }
    }

    int RowsPerColumn(int tab) { return tab == 0 ? kRelicRows : tab == 1 ? kCreatureRows : kFeatRows; }

    // Greedy word wrap for the detail panel.
    std::vector<std::string> Wrap(const std::string& text, int fontSize, float width)
    {
        std::vector<std::string> lines;
        std::string line, word;
        auto flushWord = [&]()
        {
            if (word.empty()) return;
            const std::string candidate = line.empty() ? word : line + " " + word;
            if (!line.empty() && MeasureText(candidate.c_str(), fontSize) > width) { lines.push_back(line); line = word; }
            else line = candidate;
            word.clear();
        };
        for (char c : text)
        {
            if (c == ' ' || c == '\n') { flushWord(); if (c == '\n') { lines.push_back(line); line.clear(); } }
            else word += c;
        }
        flushWord();
        if (!line.empty()) lines.push_back(line);
        return lines;
    }

    Color RarityColor(RelicRarity rarity)
    {
        return rarity == RelicRarity::Epic ? Color{ 205, 140, 255, 255 }
             : rarity == RelicRarity::Rare ? Color{ 120, 190, 255, 255 }
                                           : Color{ 225, 225, 225, 255 };
    }
}

bool Engine::GrimoireCounts() const
{
    return _isMainGameRun && !_debug.IsActive() && !_prologueActive && !_editorPlaytestActive;
}

void Engine::RecordRelicFound(RelicType type)
{
    if (!GrimoireCounts() || type == RelicType::Count) return;
    _meta.MarkRelicFound((int)type);
    if (_meta.FoundRelicCount() >= kRelicSageRelics) EarnFeat(Feat::RelicSage);
    if (_player.GetRelicCount() >= kCollectorRelics) EarnFeat(Feat::Collector);
}

void Engine::RecordCreatureSlain(Enemy& enemy)
{
    if (!GrimoireCounts()) return;
    const int index = BestiaryIndexForType(GetDungeonSnapshotType(enemy));
    if (index < 0) return;
    _meta.MarkCreatureFound(index);
    if (_meta.FoundCreatureCount() >= kBeastScholarCreatures) EarnFeat(Feat::BeastScholar);
}

void Engine::EarnFeat(Feat feat)
{
    if (!GrimoireCounts()) return;
    if (!_meta.EarnFeat((int)feat)) return;   // already earned
    _featToastText  = std::string("FEAT EARNED:  ") + GetFeatInfo(feat).name;
    _featToastTimer = 4.f;
}

void Engine::CheckRunFeats()
{
    if (!GrimoireCounts()) return;
    if (_runElitesSlain >= kEliteHunterKills)       EarnFeat(Feat::EliteHunter);
    if (_enemiesKilled >= kExterminatorKills)       EarnFeat(Feat::Exterminator);
    if (_player.GetGold() >= kHoarderGold)          EarnFeat(Feat::Hoarder);
    if (_player.GetRelicCount() >= kCollectorRelics) EarnFeat(Feat::Collector);
    if (_meta.GetLifetimeCells() >= kEchoKeeperBanked) EarnFeat(Feat::EchoKeeper);
}

void Engine::DrawFeatToast() const
{
    // Announced over play and the end-of-run screens, never over menus.
    const bool shown = _gameState == GameState::DungeonRun || _gameState == GameState::Village ||
                       _gameState == GameState::RunSummary || _gameState == GameState::DemoEnd;
    if (_featToastTimer <= 0.f || !shown) return;
    const float sw = (float)kVirtualWidth;
    const float alpha = std::min(1.f, _featToastTimer / 0.5f);
    const int fs = 30;
    const float w = (float)MeasureText(_featToastText.c_str(), fs) + 60.f;
    const Rectangle pill{ sw * 0.5f - w * 0.5f, 250.f, w, 54.f };
    DrawRectangleRounded(pill, 0.5f, 10, Fade(Color{ 30, 22, 10, 255 }, 0.88f * alpha));
    DrawRectangleRoundedLinesEx(pill, 0.5f, 10, 2.f, Fade(GOLD, alpha));
    DrawText(_featToastText.c_str(), (int)(pill.x + 30.f), (int)(pill.y + 12.f), fs, Fade(GOLD, alpha));
}

void Engine::OpenGrimoire(GameState returnTo)
{
    _grimoireReturnState = returnTo;
    _grimoireCursor = 0;
    _grimoireOpenTimer = 0.2f;

    // The chronicle is read once per visit.
    _chronicle.clear();
    _chronicleRuns = _chronicleWins = 0;
    constexpr int kChronicleRows = 30;   // the newest runs the chronicle lists
    RunLog log;
    SaveFile file(SaveFiles::RunLogPath(), SaveKind::RunLog, SaveFiles::kMaxRunLogBytes);
    const SaveLoadResult result = file.Load([&](const JsonValue& document, std::string& error) {
        return RunLog::FromJson(document, log, error);
    });
    if (result == SaveLoadResult::Loaded || result == SaveLoadResult::RestoredBackup)
    {
        _chronicleRuns = (int)std::min<long long>(log.TotalRuns(), INT_MAX);
        _chronicleWins = (int)std::min<long long>(log.Wins(), INT_MAX);
        for (const RunLogEntry& run : log.Runs())
        {
            if ((int)_chronicle.size() >= kChronicleRows) break;
            ChronicleRow row;
            row.victory = run.outcome == RunOutcome::Victory;
            row.cls = run.cls;
            row.domain = run.domainReached;
            row.rooms = run.roomsCleared;
            row.relics = (int)run.relics.size();
            row.seconds = run.seconds;
            row.ending = run.ending;
            _chronicle.push_back(row);
        }
    }
    _gameState = GameState::Grimoire;
}

void Engine::UpdateGrimoire(float dt)
{
    if (_grimoireOpenTimer > 0.f) { _grimoireOpenTimer -= dt; return; }
    _gamepad.Update(_gamepadBindingsEdit);
    const bool gp = IsGamepadAvailable(0);

    if (IsKeyPressed(KEY_ESCAPE) || (gp && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)))
    {
        _gameState = _grimoireReturnState;
        return;
    }

    // Tabs: Q / E, shoulder buttons, or click.
    int tab = _grimoireTab;
    if (IsKeyPressed(KEY_Q) || (gp && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_TRIGGER_1)))  tab = (tab + kTabCount - 1) % kTabCount;
    if (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_TAB) ||
        (gp && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_TRIGGER_1)))                         tab = (tab + 1) % kTabCount;
    const Vector2 mouse = GetVirtualMousePos();
    for (int i = 0; i < kTabCount; ++i)
    {
        const Rectangle r{ 160.f + i * 410.f, 140.f, 380.f, 56.f };
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouse, r)) tab = i;
    }
    if (tab != _grimoireTab) { _grimoireTab = tab; _grimoireCursor = 0; return; }

    // Grid cursor: column-major, so up/down walks a column and left/right jumps one.
    const int count = EntryCount(_grimoireTab);
    if (count <= 0) return;
    const int rows = RowsPerColumn(_grimoireTab);
    int cursor = _grimoireCursor;
    if (IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S) || (gp && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN)))  cursor += 1;
    if (IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W) || (gp && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_UP)))    cursor -= 1;
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D) || (gp && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT))) cursor += rows;
    if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A) || (gp && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT)))  cursor -= rows;
    _grimoireCursor = std::clamp(cursor, 0, count - 1);

    // Hover picks an entry.
    const float colW = _grimoireTab == 0 ? 380.f : 560.f;
    for (int i = 0; i < count; ++i)
    {
        const Rectangle r{ 120.f + (float)(i / rows) * colW, 230.f + (float)(i % rows) * (_grimoireTab == 3 ? 86.f : 34.f),
                           colW - 20.f, _grimoireTab == 3 ? 80.f : 32.f };
        if (CheckCollisionPointRec(mouse, r) && Vector2LengthSqr(GetMouseDelta()) > 0.f) _grimoireCursor = i;
    }
}

void Engine::DrawGrimoire() const
{
    const float sw = (float)kVirtualWidth, sh = (float)kVirtualHeight;
    DrawRectangleGradientV(0, 0, (int)sw, (int)sh, Color{ 30, 20, 40, 255 }, Color{ 10, 8, 16, 255 });
    const char* title = "GRIMOIRE";
    DrawText(title, (int)(sw * 0.5f - MeasureText(title, 68) * 0.5f), 40, 68, Color{ 230, 200, 120, 255 });

    // Tabs, with how much of each has been found.
    for (int i = 0; i < kTabCount; ++i)
    {
        const Rectangle r{ 160.f + i * 410.f, 140.f, 380.f, 56.f };
        const bool active = i == _grimoireTab;
        DrawRectangleRounded(r, 0.3f, 8, active ? Color{ 90, 60, 120, 240 } : Color{ 40, 28, 54, 220 });
        DrawRectangleRoundedLinesEx(r, 0.3f, 8, 2.f, active ? Color{ 230, 200, 120, 255 } : Color{ 110, 90, 130, 255 });
        std::string label = kTabNames[i];
        if (i == 0) label += TextFormat("  %d/%d", _meta.FoundRelicCount(), (int)RelicType::Count);
        if (i == 1) label += TextFormat("  %d/%d", _meta.FoundCreatureCount(), BestiaryCount());
        if (i == 3) label += TextFormat("  %d/%d", _meta.FeatCount(), (int)Feat::Count);
        DrawText(label.c_str(), (int)(r.x + r.width * 0.5f - MeasureText(label.c_str(), 28) * 0.5f), (int)(r.y + 14.f), 28,
                 active ? Color{ 250, 235, 200, 255 } : Color{ 180, 170, 195, 255 });
    }

    const Rectangle detail{ 1290.f, 230.f, 520.f, 700.f };
    const Color dim{ 120, 112, 132, 255 };
    auto drawDetail = [&](const std::string& name, Color nameColor, const std::string& sub, const std::string& body)
    {
        DrawRectangleRounded(detail, 0.04f, 8, Fade(Color{ 14, 10, 20, 255 }, 0.85f));
        DrawRectangleRoundedLinesEx(detail, 0.04f, 8, 2.f, Fade(nameColor, 0.6f));
        float y = detail.y + 30.f;
        for (const std::string& line : Wrap(name, 40, detail.width - 60.f))
        {
            DrawText(line.c_str(), (int)(detail.x + 30.f), (int)y, 40, nameColor);
            y += 48.f;
        }
        if (!sub.empty())
        {
            DrawText(sub.c_str(), (int)(detail.x + 30.f), (int)y + 4, 24, Color{ 190, 180, 205, 255 });
            y += 48.f;
        }
        y += 10.f;
        for (const std::string& line : Wrap(body, 28, detail.width - 60.f))
        {
            DrawText(line.c_str(), (int)(detail.x + 30.f), (int)y, 28, Color{ 235, 230, 222, 255 });
            y += 38.f;
        }
    };

    const int count = EntryCount(_grimoireTab);
    const int rows = RowsPerColumn(_grimoireTab);
    if (_grimoireTab == 0)   // relics
    {
        for (int i = 0; i < count; ++i)
        {
            const bool found = _meta.HasFoundRelic(i);
            const RelicInfo& info = GetRelicInfo((RelicType)i);
            const float x = 120.f + (float)(i / rows) * 380.f, y = 230.f + (float)(i % rows) * 34.f;
            if (i == _grimoireCursor) DrawRectangleRounded({ x - 10.f, y - 3.f, 370.f, 32.f }, 0.3f, 6, Fade(GOLD, 0.18f));
            DrawText(found ? info.name : "???", (int)x, (int)y, 26, found ? RarityColor(info.rarity) : dim);
        }
        const RelicType sel = (RelicType)std::clamp(_grimoireCursor, 0, count - 1);
        if (_meta.HasFoundRelic((int)sel))
        {
            const RelicInfo& info = GetRelicInfo(sel);
            std::string fits;
            for (int c = 0; c < (int)PlayerClass::Count; ++c)
                if (RelicFitsClass(sel, (PlayerClass)c))
                    fits += std::string(fits.empty() ? "" : ", ") + GetPlayerClassInfo((PlayerClass)c).name;
            drawDetail(info.name, RarityColor(info.rarity),
                       std::string(GetRelicRarityName(info.rarity)) + "  " + GetRelicArchetypeName(info.archetype),
                       std::string(info.description) + "\n\nFor: " + fits);
        }
        else
            drawDetail("Unknown relic", dim, "", "Not found yet. Relics come from elites, bosses, treasure, events and the crossroads.");
    }
    else if (_grimoireTab == 1)   // bestiary
    {
        for (int i = 0; i < count; ++i)
        {
            const bool found = _meta.HasFoundCreature(i);
            const BestiaryEntry& e = GetBestiaryEntry(i);
            const float x = 120.f + (float)(i / rows) * 560.f, y = 230.f + (float)(i % rows) * 34.f;
            if (i == _grimoireCursor) DrawRectangleRounded({ x - 10.f, y - 3.f, 540.f, 32.f }, 0.3f, 6, Fade(GOLD, 0.18f));
            DrawText(found ? e.name : "???", (int)x, (int)y, 26,
                     !found ? dim : e.boss ? Color{ 255, 170, 120, 255 } : Color{ 225, 225, 225, 255 });
            if (found) DrawText(e.role, (int)(x + 260.f), (int)(y + 3.f), 22, Color{ 160, 150, 175, 255 });
        }
        const BestiaryEntry& e = GetBestiaryEntry(std::clamp(_grimoireCursor, 0, count - 1));
        if (_meta.HasFoundCreature(_grimoireCursor))
            drawDetail(e.name, e.boss ? Color{ 255, 170, 120, 255 } : Color{ 230, 220, 200, 255 },
                       e.boss ? std::string("Guardian of ") + GetBiomeName(e.home) : std::string(e.role), e.lore);
        else
            drawDetail("Unknown creature", dim, "", "Slay one to record it here.");
    }
    else if (_grimoireTab == 2)   // chronicle
    {
        DrawText(TextFormat("Runs: %d     Wins: %d", _chronicleRuns, _chronicleWins), 120, 230, 32, Color{ 230, 200, 120, 255 });
        if (_chronicle.empty())
            DrawText("No runs recorded yet.", 120, 300, 28, dim);
        float y = 300.f;
        for (size_t i = 0; i < _chronicle.size() && i < 15; ++i)
        {
            const ChronicleRow& r = _chronicle[i];
            const int t = (int)r.seconds;
            DrawText(r.victory ? "WON" : "FELL", 120, (int)y, 26, r.victory ? GOLD : Color{ 235, 110, 110, 255 });
            DrawText(GetPlayerClassInfo(r.cls).name, 220, (int)y, 26, Color{ 230, 225, 215, 255 });
            DrawText(TextFormat("Domain %d/6   %d rooms   %d relics   %d:%02d", r.domain, r.rooms, r.relics, t / 60, t % 60),
                     420, (int)y, 26, Color{ 200, 195, 210, 255 });
            DrawText(r.ending.c_str(), 1100, (int)y, 24, Color{ 170, 160, 185, 255 });
            y += 44.f;
        }
    }
    else   // feats
    {
        for (int i = 0; i < count; ++i)
        {
            const bool earned = _meta.HasFeat(i);
            const FeatInfo& f = GetFeatInfo((Feat)i);
            const float x = 120.f + (float)(i / rows) * 560.f, y = 230.f + (float)(i % rows) * 86.f;
            if (i == _grimoireCursor) DrawRectangleRounded({ x - 10.f, y - 6.f, 540.f, 80.f }, 0.2f, 6, Fade(GOLD, 0.14f));
            DrawCircleV({ x + 14.f, y + 18.f }, 12.f, earned ? GOLD : Color{ 60, 52, 70, 255 });
            DrawText(f.name, (int)(x + 40.f), (int)y, 30, earned ? GOLD : Color{ 170, 162, 182, 255 });
            DrawText(f.description, (int)(x + 40.f), (int)(y + 38.f), 22, earned ? Color{ 230, 220, 200, 255 } : dim);
        }
        const FeatInfo& f = GetFeatInfo((Feat)std::clamp(_grimoireCursor, 0, count - 1));
        drawDetail(f.name, _meta.HasFeat(_grimoireCursor) ? GOLD : dim, _meta.HasFeat(_grimoireCursor) ? "Earned" : "Not yet earned",
                   f.description);
    }

    const char* hint = IsGamepadAvailable(0) ? "LB / RB: Tabs    D-Pad: Browse    B: Back"
                                             : "Q / E: Tabs    Arrows: Browse    Esc: Back";
    DrawText(hint, (int)(sw * 0.5f - MeasureText(hint, 24) * 0.5f), (int)(sh - 60.f), 24, Color{ 170, 160, 185, 255 });
}
