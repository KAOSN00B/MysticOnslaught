// =============================================================================
// EngineRunSummary.cpp — the end-of-run screen (death or victory).
//
// When a main run ends, the death beat or the final boss leads here first:
// how the run ended, how far it got, what it cleared and killed, the relics it
// built and the time it took. Any input continues to what followed before:
// Poe's revive scene after a death, the victory screen after a win.
//
// Every finished run (and an abandoned one) is also added to the run log
// (Saves/runlog.mosave, RunLog.h): the Grimoire's chronicle, and the record
// of what killed the player that death lessons and tuning read later.
// =============================================================================

#include "Engine.h"
#include "Grimoire.h"
#include "RunCheckpoint.h"
#include "RunLog.h"
#include "SaveFiles.h"
#include "SaveStore.h"
#include "WorldMapManager.h"

#include <algorithm>
#include <ctime>
#include <string>

namespace
{
    constexpr float kSummaryInputLock = 0.8f;   // the blow that ended the run can't skip it

    SaveFile RunLogFile()
    {
        return SaveFile(SaveFiles::RunLogPath(), SaveKind::RunLog, SaveFiles::kMaxRunLogBytes);
    }

    std::string FormatRunTime(float seconds)
    {
        const int total = std::max(0, (int)seconds);
        return total >= 3600 ? TextFormat("%d:%02d:%02d", total / 3600, (total / 60) % 60, total % 60)
                             : TextFormat("%d:%02d", total / 60, total % 60);
    }
}

void Engine::CaptureRunSummary(bool victory)
{
    RunSummary s;
    s.victory = victory;
    s.cls = _player.GetClass();
    s.ascension = _ascensionTier;
    s.domains.push_back(Biome::Forest);
    for (Biome b : _worldCompletedBiomes) s.domains.push_back(b);
    if (_worldZone >= RunProgression::kFinalZone) s.domains.push_back(Biome::DemonsInsides);
    s.domainReached = std::clamp(_worldZone + 1, 1, RunProgression::kFinalZone + 1);
    s.roomsCleared = _runRoomsCleared;
    s.enemiesKilled = _enemiesKilled;
    s.elitesSlain = _runElitesSlain;
    s.bossesDefeated = _bossesDefeated;
    s.level = _player.GetLevel();
    s.seconds = _runTimer;
    s.gold = _player.GetGold();
    s.echoes = _player.GetCells();
    for (int i = 0; i < _player.GetRelicCount(); ++i) s.relics.push_back(_player.GetRelicAt(i));

    const std::string biome = GetBiomeName(_currentBiome);
    if (victory)
        s.ending = "Every domain conquered";
    else if (_dungeonRoomIdx >= 0 && _dungeonRoomIdx == _dungeonGen.GetBossIndex())
        s.ending = std::string("Fell to the ") + GetBiomeGuardianName(_currentBiome) + " in " + biome;
    else if (_dungeonRoomIdx >= 0 && _dungeonRoomIdx < (int)_dungeonGen.GetRooms().size() &&
             _dungeonGen.GetRooms()[(size_t)_dungeonRoomIdx].type == RoomType::Elite)
        s.ending = "Fell to an elite in " + biome;
    else
        s.ending = TextFormat("Fell in %s, domain %d", biome.c_str(), s.domainReached);
    if (victory)
    {
        EarnFeat(Feat::Conqueror);
        if (s.ascension > 0) EarnFeat(Feat::Ascendant);
        if (s.seconds < kSwiftVictorySeconds) EarnFeat(Feat::SwiftVictory);
    }
    CheckRunFeats();
    _runSummary = std::move(s);
    AppendRunLog(victory ? RunOutcome::Victory : RunOutcome::Death, _runSummary.ending);
}

void Engine::OpenRunSummary()
{
    _runSummaryOpenTimer = kSummaryInputLock;
    _gameState = GameState::RunSummary;
}

void Engine::UpdateRunSummary(float dt)
{
    if (_runSummaryOpenTimer > 0.f)
    {
        _runSummaryOpenTimer -= dt;
        while (GetKeyPressed() != 0) {}   // drain presses made during the lock
        return;
    }
    bool pressed = GetKeyPressed() != 0 || IsMouseButtonPressed(MOUSE_LEFT_BUTTON) ||
                   (_touchModeActive && IsGestureDetected(GESTURE_TAP));
    if (IsGamepadAvailable(GamepadInput::kGamepad))
        for (int b = 1; b <= 17 && !pressed; ++b)
            pressed = IsGamepadButtonPressed(GamepadInput::kGamepad, (GamepadButton)b);
    if (pressed) LeaveRunSummary();
}

void Engine::LeaveRunSummary()
{
    if (_gameState != GameState::RunSummary) return;
    if (_runSummary.victory)
    {
        _victoryInputLock = kVictoryInputLockSeconds;
        _gameState = GameState::DemoEnd;
    }
    else
        BeginDeathRevive();
}

void Engine::DrawRunSummary() const
{
    const RunSummary& s = _runSummary;
    const float sw = (float)kVirtualWidth, sh = (float)kVirtualHeight;
    if (s.victory)
        DrawRectangleGradientV(0, 0, (int)sw, (int)sh, Color{ 40, 32, 14, 255 }, Color{ 14, 12, 8, 255 });
    else
        DrawRectangleGradientV(0, 0, (int)sw, (int)sh, Color{ 40, 14, 18, 255 }, Color{ 12, 6, 8, 255 });

    const char* title = s.victory ? "VICTORY" : "FALLEN";
    const Color titleColor = s.victory ? GOLD : Color{ 235, 90, 90, 255 };
    DrawText(title, (int)(sw * 0.5f - MeasureText(title, 84) * 0.5f), 50, 84, titleColor);
    DrawText(s.ending.c_str(), (int)(sw * 0.5f - MeasureText(s.ending.c_str(), 32) * 0.5f), 150, 32,
             Color{ 230, 220, 210, 255 });

    // Left: the numbers.
    const Rectangle left{ 150.f, 230.f, 760.f, 700.f };
    const Rectangle right{ 1010.f, 230.f, 760.f, 700.f };
    for (const Rectangle& panel : { left, right })
    {
        DrawRectangleRounded(panel, 0.04f, 8, Fade(Color{ 14, 12, 20, 255 }, 0.82f));
        DrawRectangleRoundedLinesEx(panel, 0.04f, 8, 2.f, Fade(titleColor, 0.55f));
    }
    DrawText("THE RUN", (int)(left.x + 36.f), (int)(left.y + 28.f), 32, titleColor);
    std::string cls = GetPlayerClassInfo(s.cls).name;
    if (s.ascension > 0) cls += TextFormat("   (Ascension %d)", s.ascension);
    const std::string rows[][2] = {
        { "Class",           cls },
        { "Level",           std::to_string(s.level) },
        { "Domains reached", TextFormat("%d of %d", s.domainReached, RunProgression::kFinalZone + 1) },
        { "Rooms cleared",   std::to_string(s.roomsCleared) },
        { "Enemies slain",   std::to_string(s.enemiesKilled) },
        { "Elites slain",    std::to_string(s.elitesSlain) },
        { "Bosses defeated", std::to_string(s.bossesDefeated) },
        { "Time",            FormatRunTime(s.seconds) },
        { "Gold",            s.victory ? TextFormat("%d, carried home", s.gold) : TextFormat("%d, lost", s.gold) },
        { "Echoes carried",  s.victory ? TextFormat("%d, banked", s.echoes) : TextFormat("%d, half salvaged by Poe", s.echoes) },
    };
    float y = left.y + 92.f;
    for (const auto& row : rows)
    {
        DrawText(row[0].c_str(), (int)(left.x + 36.f), (int)y, 28, Color{ 175, 170, 190, 255 });
        DrawText(row[1].c_str(), (int)(left.x + 330.f), (int)y, 28, Color{ 240, 236, 228, 255 });
        y += 58.f;
    }

    // Right: the road taken, then the build.
    DrawText("THE ROAD", (int)(right.x + 36.f), (int)(right.y + 28.f), 32, titleColor);
    float rx = right.x + 36.f, ry = right.y + 84.f;
    for (size_t i = 0; i < s.domains.size(); ++i)
    {
        const bool last = i + 1 == s.domains.size();
        const Color tint = WorldMapManager::GetBiomeColor(s.domains[i]);
        const Color bright{ (unsigned char)(tint.r + (255 - tint.r) * 0.45f), (unsigned char)(tint.g + (255 - tint.g) * 0.45f),
                            (unsigned char)(tint.b + (255 - tint.b) * 0.45f), 255 };
        std::string name = GetBiomeName(s.domains[i]);
        if (last && !s.victory) name += "  (fell here)";
        DrawCircleV({ rx + 10.f, ry + 13.f }, 9.f, bright);
        DrawText(name.c_str(), (int)(rx + 30.f), (int)ry, 26, bright);
        // Two columns of three domains.
        ry += 40.f;
        if (i == 2) { rx = right.x + 400.f; ry = right.y + 84.f; }
    }

    const float relicTop = right.y + 230.f;
    DrawText(TextFormat("RELICS  (%d)", (int)s.relics.size()), (int)(right.x + 36.f), (int)relicTop, 32, titleColor);
    if (s.relics.empty())
        DrawText("None this run", (int)(right.x + 36.f), (int)(relicTop + 56.f), 26, Color{ 150, 145, 160, 255 });
    const int perColumn = 10;
    for (size_t i = 0; i < s.relics.size() && i < 20; ++i)
    {
        const RelicInfo& info = GetRelicInfo(s.relics[i]);
        const Color rarity = info.rarity == RelicRarity::Epic ? Color{ 205, 140, 255, 255 }
                           : info.rarity == RelicRarity::Rare ? Color{ 120, 190, 255, 255 }
                                                              : Color{ 220, 220, 220, 255 };
        const float cx = right.x + 36.f + (float)(i / perColumn) * 360.f;
        const float cy = relicTop + 56.f + (float)(i % perColumn) * 38.f;
        DrawText(info.name, (int)cx, (int)cy, 24, rarity);
    }
    if (s.relics.size() > 20)
        DrawText(TextFormat("+%d more", (int)s.relics.size() - 20), (int)(right.x + 396.f),
                 (int)(right.y + right.height - 40.f), 22, Color{ 150, 145, 160, 255 });

    if (_runSummaryOpenTimer <= 0.f)
    {
        const char* hint = "Press any key to continue";
        DrawText(hint, (int)(sw * 0.5f - MeasureText(hint, 28) * 0.5f), (int)(sh - 90.f), 28, Color{ 200, 195, 210, 255 });
    }
}

DamageSource Engine::DamageSourceOf(Enemy& enemy)
{
    // The Bestiary's type is the stable name ("Elite_Ogre" -> "Ogre",
    // "SlimeBig" -> "Slime"); the elite and boss flags carry the rank.
    const std::string type = GetDungeonSnapshotType(enemy);
    const int entry = BestiaryIndexForType(type);
    const std::string name = entry >= 0 ? std::string(GetBestiaryEntry(entry).type) : type;
    return DamageSource::FromEnemy(name, enemy.IsEliteMiniboss(), enemy.IsBoss());
}

RunLogDomain Engine::CurrentDomainProgress(bool bossDefeated) const
{
    RunLogDomain domain;
    domain.biome = _currentBiome;
    domain.roomsCleared = std::max(0, _runRoomsCleared - _domainStartRooms);
    domain.seconds = std::max(0.f, _runTimer - _domainStartSeconds);
    domain.bossDefeated = bossDefeated;
    return domain;
}

void Engine::RecordDomainEnd(bool bossDefeated)
{
    if (!_isMainGameRun || (int)_runDomainLog.size() >= RunCheckpointLimits::kMaxRouteEntries) return;
    _runDomainLog.push_back(CurrentDomainProgress(bossDefeated));
    _domainStartRooms = _runRoomsCleared;
    _domainStartSeconds = _runTimer;
}

void Engine::RecordDeathMoment()
{
    RunLogDeath d;
    d.cause = _player.GetLastDamageSource();
    if (d.cause.kind == DamageSourceKind::Unknown)
        TraceLog(LOG_WARNING, "Run log: the killing blow came from an unnamed source (cause recorded as unknown)");
    d.burning = _player.IsBurning();
    d.poisoned = _player.IsPoisoned();
    d.chilled = _player.IsChilled();
    d.inEliteCage = _eliteMechanic == 0 && _eliteCageRadius > 0.f;
    d.manaFull = _player.GetMana() >= _player.GetMaxMana();
    d.emptyAbilitySlot = _player.GetLearnedCount() < _player.GetMaxAbilitySlots();
    switch (_player.GetClass())
    {
    case PlayerClass::Warrior: d.classResourceFull = _player.GetRage() >= Character::kMaxRage; break;
    case PlayerClass::Paladin: d.classResourceFull = _player.GetFaith() >= Character::kMaxFaith; break;
    case PlayerClass::Rogue:   d.classResourceFull = _player.GetComboPoints() >= _player.GetMaxComboPoints(); break;
    default: break;
    }
    for (const RunModifier& m : _runModifiers)
    {
        if (!m.active) continue;
        d.curseActive = d.curseActive || m.curse;
        if ((int)d.activeModifiers.size() < RunCheckpointLimits::kMaxModifiers) d.activeModifiers.push_back(m.label);
    }
    const auto& rooms = _dungeonGen.GetRooms();
    if (_dungeonRoomIdx >= 0 && _dungeonRoomIdx < (int)rooms.size())
    {
        d.roomType = (int)rooms[(size_t)_dungeonRoomIdx].type;
        d.roomDepth = rooms[(size_t)_dungeonRoomIdx].depth;
        auto state = _dungeonRoomStates.find(_dungeonRoomIdx);
        if (state != _dungeonRoomStates.end()) d.roomSpecial = (int)state->second.special;
    }
    if (_currentRoomAffix != RoomAffix::None) d.roomAffix = GetRoomAffixDef(_currentRoomAffix).name;
    d.secondsInRoom = _roomCombatSeconds;
    d.secondsSinceDash = _player.GetSecondsSinceDash();
    for (const auto& enemy : _enemies)
        if (enemy->IsActive() && enemy->IsAlive() && enemy->IsBoss() && enemy->GetMaxHealthValue() > 0.f)
            d.bossHealthFraction = std::clamp(enemy->GetHealthValue() / enemy->GetMaxHealthValue(), 0.f, 1.f);
    d.goldCarried = _player.GetGold();
    _runDeath = d;
    // Lifetime tallies in the profile: the death lessons count these.
    _meta.RecordDeath(d.cause, _currentBiome);
}

void Engine::AppendRunLog(RunOutcome outcome, const std::string& ending)
{
#if !MO_ITCH_DEMO
    if (_runId == 0) return;   // not a main run the profile knows
    RunLogEntry e;
    e.runId = _runId;
    e.endedAt = (long long)std::time(nullptr);
    e.outcome = outcome;
    e.cls = _player.GetClass();
    e.skin = _player.GetAppearance();
    e.ascension = _ascensionTier;
    e.domains = _runDomainLog;
    if (outcome != RunOutcome::Victory) e.domains.push_back(CurrentDomainProgress(false));
    e.domainReached = std::clamp(_worldZone + 1, 1, RunProgression::kFinalZone + 1);
    e.seconds = _runTimer;
    e.roomsCleared = _runRoomsCleared;
    e.enemiesKilled = _enemiesKilled;
    e.elitesSlain = _runElitesSlain;
    e.bossesDefeated = _bossesDefeated;
    e.gold = _player.GetGold();
    e.echoes = _player.GetCells();
    e.level = _player.GetLevel();
    e.maxHealth = _player.GetMaxHealthValue();
    CharacterCheckpoint build;
    _player.CaptureRunState(build);
    for (const CharacterCheckpoint::Slot& slot : build.abilities)
        if (slot.ability != AbilityType::None) e.abilities.push_back({ slot.ability, slot.level });
    e.relics = build.relics;
    e.ending = ending.substr(0, SaveFiles::kMaxTextLength);
    e.hasDeath = outcome == RunOutcome::Death;
    if (e.hasDeath) e.death = _runDeath;

    // A damaged log is set aside (.rejected) and a fresh one started, so a
    // bad file never stops new runs from being recorded.
    SaveFile file = RunLogFile();
    RunLog log;
    if (file.Load([&](const JsonValue& document, std::string& error) { return RunLog::FromJson(document, log, error); }) ==
        SaveLoadResult::Rejected)
        TraceLog(LOG_WARNING, "Run log: %s; starting a new log", file.LastError().c_str());
    log.Add(e);
    const auto verify = [](const JsonValue& document, std::string& error) {
        RunLog check;
        return RunLog::FromJson(document, check, error);
    };
    if (!file.Save(log.ToJson(), verify))
        TraceLog(LOG_WARNING, "Run log could not be written (%s)", file.LastError().c_str());
#else
    (void)outcome;
    (void)ending;
#endif
}
