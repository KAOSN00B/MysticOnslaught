// =============================================================================
// EngineRunSave.cpp — mid-run save (suspend & resume).
//
// Checkpoints are written at calm moments: entering a dungeon room, clearing
// one, reaching the world map, and Save & Quit (pause menu or closing the
// window). The checkpoint (Saves/run.mosave, RunCheckpoint.h) holds the full
// character build, run progress (domain, shrines, contracts, ascension) and
// the current domain's exact room graph with per-room progress. Resuming
// re-enters the saved room from the door the player came in by: a room that
// was mid-fight restarts with fresh enemies, while the player keeps the HP
// and build they had at the checkpoint.
//
// Every main run has an id in the profile. A run that ends (death, victory,
// abandon, New Game) is marked ended in the profile, then an inactive
// checkpoint is written, so Continue can never revive a finished run, not
// even from a backup file. A checkpoint write that fails returns false and
// changes nothing in memory: the caller keeps the run and tells the player.
// =============================================================================

#include "Engine.h"
#include "RunCheckpoint.h"
#include "SaveFiles.h"
#include "SaveStore.h"

#include <string>
#include <vector>

namespace
{
    constexpr int kRedShift = 24, kGreenShift = 16, kBlueShift = 8;
    constexpr unsigned kChannelMask = 0xFF;

    SaveFile RunCheckpointFile()
    {
        return SaveFile(SaveFiles::RunCheckpointPath(), SaveKind::RunCheckpoint, SaveFiles::kMaxRunCheckpointBytes);
    }

    bool VerifyCheckpoint(const JsonValue& document, std::string& error)
    {
        RunCheckpoint checkpoint;
        return RunCheckpointFromJson(document, checkpoint, error);
    }

    unsigned PackRgba(Color c)
    {
        return ((unsigned)c.r << kRedShift) | ((unsigned)c.g << kGreenShift) | ((unsigned)c.b << kBlueShift) | (unsigned)c.a;
    }

    Color UnpackRgba(unsigned rgba)
    {
        return Color{ (unsigned char)((rgba >> kRedShift) & kChannelMask), (unsigned char)((rgba >> kGreenShift) & kChannelMask),
                      (unsigned char)((rgba >> kBlueShift) & kChannelMask), (unsigned char)(rgba & kChannelMask) };
    }
}

bool Engine::IsRunSaveable() const
{
#if MO_ITCH_DEMO
    return false;   // the demo is stateless
#else
    const GameState where = (_gameState == GameState::Pause) ? _stateBeforePause : _gameState;
    return _isMainGameRun && _runSessionData.IsActive() && !_runCheckpointEnded && _runId != 0 &&
           !_prologueActive && !_editorPlaytestActive && !_playerDying &&
           _player.GetHealthValue() > 0.f &&
           (where == GameState::DungeonRun || where == GameState::WorldMap);
#endif
}

bool Engine::SaveRunCheckpoint()
{
    // The validator checks numbers stored for these enums against
    // RunCheckpointLimits; keep the two in step.
    static_assert((int)RoomType::Boss + 1 == RunCheckpointLimits::kRoomTypes, "update RunCheckpointLimits::kRoomTypes");
    static_assert((int)EncounterProfile::Holdout + 1 == RunCheckpointLimits::kEncounterProfiles, "update RunCheckpointLimits");
    static_assert((int)RoomSpecialType::Count == RunCheckpointLimits::kRoomSpecials, "update RunCheckpointLimits::kRoomSpecials");
    static_assert((int)DungeonDoorSide::East + 1 == RunCheckpointLimits::kDoorSides, "update RunCheckpointLimits::kDoorSides");
    static_assert((int)PathBoon::Count == RunCheckpointLimits::kPathBoons, "update RunCheckpointLimits::kPathBoons");
    static_assert((int)ShopWareType::Count == RunCheckpointLimits::kShopWares, "update RunCheckpointLimits::kShopWares");
    static_assert((int)ShopContractType::Count == RunCheckpointLimits::kShopContracts, "update RunCheckpointLimits");
    static_assert((int)UpgradeType::Count == RunCheckpointLimits::kUpgrades, "update RunCheckpointLimits::kUpgrades");
    static_assert(DungeonGen::kGridSize == RunCheckpointLimits::kGridSize, "update RunCheckpointLimits::kGridSize");

    if (!IsRunSaveable()) return true;   // nothing to save here
    const GameState where = (_gameState == GameState::Pause) ? _stateBeforePause : _gameState;

    RunCheckpoint c;
    c.runId = _runId;
    c.active = true;
    _player.CaptureRunState(c.character);

    // Run progress.
    c.biome = _currentBiome;
    c.worldZone = _worldZone;
    c.completedBiomes = _worldCompletedBiomes;
    c.chosenNodes = _worldChosenNodeIndices;
    c.worldMapGenerated = _runSessionData.HasGeneratedWorldMap();
    c.sessionCompleted = _runSessionData.GetCompletedBiomes();
    c.sessionChosen = _runSessionData.GetChosenBiomes();
    c.sessionNodes = _runSessionData.GetChosenNodeIndices();
    c.ascensionTier = _ascensionTier;
    c.ascensionRecorded = _ascensionRecorded;
    c.playerDamageMult = _runPlayerDamageMult;
    c.enemyHealthMult = _runEnemyHealthMult;
    c.enemyDamageMult = _runEnemyDamageMult;
    c.wagerTier = _wagerTier;
    c.curseShrineUsed = _curseShrineUsed;
    c.wagerAccessGranted = _wagerAccessGranted;
    c.secondWind = _secondWindAvailable;
    c.pendingRelicChoices = _pendingRelicChoices;
    c.enemiesKilled = _enemiesKilled;
    c.bossesDefeated = _bossesDefeated;
    c.roomsCleared = _runRoomsCleared;
    c.elitesSlain = _runElitesSlain;
    c.timer = _runTimer;
    c.gameTimer = _gameTimer;
    c.seed = _runSeed;
    c.domainLog = _runDomainLog;
    c.domainStartRooms = _domainStartRooms;
    c.domainStartSeconds = _domainStartSeconds;
    for (const RunModifier& m : _runModifiers)
    {
        ModifierCheckpoint saved;
        saved.label = m.label;
        saved.enemyHpMult = m.enemyHpMult;
        saved.enemyDmgMult = m.enemyDmgMult;
        saved.goldMult = m.goldMult;
        saved.xpMult = m.xpMult;
        saved.tintRgba = PackRgba(m.tint);
        saved.active = m.active;
        saved.roomsRemaining = m.roomsRemaining;
        saved.shopContract = (int)m.shopContract;
        saved.curse = m.curse;
        c.modifiers.push_back(saved);
    }

    // Where the player is.
    c.atWorldMap = where == GameState::WorldMap;
    c.atCrossroads = _atCrossroads;
    for (int i = 0; i < RunCheckpointLimits::kCrossroadsDoors; ++i)
        c.crossroads[i] = { _crossroadsDoors[i].open, _crossroadsDoors[i].biome, (int)_crossroadsDoors[i].boon };
    if (!c.atWorldMap && !c.atCrossroads)
    {
        c.hasDungeon = true;
        DungeonCheckpoint& d = c.dungeon;
        const auto& rooms = _dungeonGen.GetRooms();
        for (std::size_t i = 0; i < rooms.size(); ++i)
        {
            const DungeonRoom& r = rooms[i];
            RoomCheckpoint room;
            room.col = r.col;
            room.row = r.row;
            room.type = (int)r.type;
            room.profile = (int)r.encounterProfile;
            room.depth = r.depth;
            room.north = r.hasNorth;
            room.south = r.hasSouth;
            room.east = r.hasEast;
            room.west = r.hasWest;
            room.startsEmpty = r.startsEmpty;
            if (i < _handcraftedDungeonRoomIds.size()) room.layout = _handcraftedDungeonRoomIds[i];
            auto state = _dungeonRoomStates.find((int)i);
            if (state != _dungeonRoomStates.end())
            {
                const DungeonRoomState& s = state->second;
                room.hasProgress = true;
                room.cleared = s.cleared;
                room.visited = s.visited;
                room.eliteMechanic = s.eliteMechanic;
                room.holdout = s.holdoutTimeRemaining;
                room.special = (int)s.special;
                room.specialClaimed = s.specialClaimed;
                room.specialChoice = s.specialChoice;
                for (int o = 0; o < RunCheckpointLimits::kSpecialOptions; ++o) room.specialOptions[o] = s.specialOptions[o];
            }
            d.rooms.push_back(room);
        }
        d.start = _dungeonGen.GetStartIndex();
        d.boss = _dungeonGen.GetBossIndex();
        d.key = _dungeonGen.GetKeyIndex();
        d.current = _dungeonRoomIdx;
        d.entrySide = (int)_dungeonEntryDoorSide;
        d.entryX = _dungeonRoomEntrySpawnPos.x;
        d.entryY = _dungeonRoomEntrySpawnPos.y;
        d.hasMagicGem = _hasMagicGem;
        d.magicGemSpawned = _magicGemSpawned;
        d.magicGemCollected = _magicGemCollected;
        d.bossBarrierUnlocked = _bossBarrierUnlocked;
        d.chestSpawned = _treasureChestSpawned;
        d.chestBroken = _treasureChestBroken;
        // Zeph's stock, when this domain's camp has stocked it.
        if (_shopStockZone == _worldZone)
        {
            c.hasShop = true;
            c.shop.zone = _shopStockZone;
            _shop.CaptureStock(c.shop);
        }
    }

    SaveFile file = RunCheckpointFile();
    if (file.Save(RunCheckpointToJson(c), VerifyCheckpoint))
    {
        _closeRefusedForFailedSave = false;
        return true;
    }
    TraceLog(LOG_WARNING, "Run checkpoint could not be written (%s); the run stays in memory", file.LastError().c_str());
    ++_runCheckpointWriteFailures;
    return false;
}

void Engine::NotifyRunSaveFailed()
{
    _runSaveFailedTimer = kRunSaveFailedNoticeSeconds;
}

void Engine::DrawRunSaveFailedNotice() const
{
    if (_runSaveFailedTimer <= 0.f) return;
    constexpr int   kFontSize = 28;
    constexpr int   kSegments = 10;
    constexpr float kTop = 180.f, kHeight = 54.f, kPadding = 30.f, kBorder = 2.f;
    constexpr float kRoundness = 0.5f, kCenter = 0.5f, kFadeSeconds = 0.5f, kBackgroundAlpha = 0.9f;
    constexpr Color kBackground{ 40, 10, 12, 255 };
    constexpr Color kOutline{ 255, 120, 110, 255 };
    constexpr Color kText{ 255, 220, 215, 255 };
    const char* message = "Couldn't save your run. It's still here: try again, or keep playing.";
    const float alpha = std::min(1.f, _runSaveFailedTimer / kFadeSeconds);
    const float width = (float)MeasureText(message, kFontSize) + 2.f * kPadding;
    const Rectangle pill{ (float)kVirtualWidth * kCenter - width * kCenter, kTop, width, kHeight };
    DrawRectangleRounded(pill, kRoundness, kSegments, Fade(kBackground, kBackgroundAlpha * alpha));
    DrawRectangleRoundedLinesEx(pill, kRoundness, kSegments, kBorder, Fade(kOutline, alpha));
    DrawText(message, (int)(pill.x + kPadding), (int)(pill.y + (kHeight - (float)kFontSize) * kCenter), kFontSize,
             Fade(kText, alpha));
}

void Engine::EndRunCheckpoint()
{
#if !MO_ITCH_DEMO
    _runCheckpointEnded = true;
    // The profile is the authority: once it lists no active run, neither the
    // checkpoint nor a backup of it can be continued, even if the write below
    // fails. A failed profile write stays pending for the next calm save.
    const long long runId = _runId != 0 ? _runId : _meta.GetActiveRunId();
    _meta.EndActiveRun();
    if (!_meta.DidLastSaveSucceed())
        TraceLog(LOG_WARNING, "Run end could not be saved to the profile yet (%s); retried at the next save",
                 _meta.GetLastSaveProblem().c_str());
    if (runId == 0) return;
    RunCheckpoint ended;
    ended.runId = runId;
    SaveFile file = RunCheckpointFile();
    if (!file.Save(RunCheckpointToJson(ended), VerifyCheckpoint))
        TraceLog(LOG_WARNING, "Could not mark the run checkpoint as finished (%s); the profile records the end",
                 file.LastError().c_str());
#endif
}

bool Engine::ReadSuspendedRun(RunCheckpoint& out) const
{
#if MO_ITCH_DEMO
    (void)out;
    return false;
#else
    SaveFile file = RunCheckpointFile();
    const SaveLoadResult result = file.Load([&](const JsonValue& document, std::string& error) {
        return RunCheckpointFromJson(document, out, error);
    });
    if (result == SaveLoadResult::RestoredBackup || result == SaveLoadResult::Rejected)
        TraceLog(LOG_WARNING, "Run checkpoint: %s", file.LastError().c_str());
    if (result != SaveLoadResult::Loaded && result != SaveLoadResult::RestoredBackup) return false;
    // Only the run the profile still lists as active can be continued.
    return out.active && out.runId != 0 && out.runId == _meta.GetActiveRunId();
#endif
}

bool Engine::HasResumableRun() const
{
    RunCheckpoint checkpoint;
    return ReadSuspendedRun(checkpoint);
}

std::string Engine::DescribeSuspendedRun() const
{
    RunCheckpoint c;
    if (!ReadSuspendedRun(c)) return std::string();
    return TextFormat("Run in progress: %s, %s domain %d", GetPlayerClassInfo(c.character.cls).name,
                      GetBiomeName(c.biome), c.worldZone + 1);
}

bool Engine::SaveAndQuitRunToMenu()
{
    // Leaving with Save & Quit is not a death: nothing is lost, the run waits.
    // If the checkpoint can't be written the run stays exactly as it is.
    if (!SaveRunCheckpoint())
    {
        NotifyRunSaveFailed();
        return false;
    }
    _nextRunPrepared = false;
    ResetRunState();
    _menu.SetHasProfile(_meta.HasAnyProgress());
    _menu.Init();
    _gameState = GameState::Menu;
    return true;
}

bool Engine::ResumeSavedRun()
{
    RunCheckpoint save;
    if (!ReadSuspendedRun(save)) return false;

    QueueJourney(GetBiomeName(save.biome), {
    [this, save] {
        _debug.Deactivate();
        // A fresh character of the saved class/look, then the saved build.
        _player.SetClass(save.character.cls);
        _player.SetAppearance(save.character.appearance.c_str());
        _nextRunPrepared = false;
        ResetRunState();
        if (!_player.RestoreRunState(save.character))
            throw std::runtime_error("The suspended run's character could not be restored");
        _isMainGameRun = true;
        _runCheckpointEnded = false;
        _runId = save.runId;
        _cutsceneIntroPlayed = true;
        _dungeonTutStep = DungeonTutStep::Done;
        _meta.SetVillageWallet(0);   // the gold rides with the run, as when it started

        _worldZone = save.worldZone;
        _worldCompletedBiomes = save.completedBiomes;
        _worldChosenNodeIndices = save.chosenNodes;
        _runSessionData.RestoreState(save.worldMapGenerated, save.sessionCompleted, save.sessionChosen, save.sessionNodes);
        _ascensionTier = save.ascensionTier;
        _ascensionRecorded = save.ascensionRecorded;
        _ascensionMods = GetAscensionModifiers(_ascensionTier);
        _runPlayerDamageMult = save.playerDamageMult;
        _runEnemyHealthMult = save.enemyHealthMult;
        _runEnemyDamageMult = save.enemyDamageMult;
        _wagerTier = save.wagerTier;
        _curseShrineUsed = save.curseShrineUsed;
        _wagerAccessGranted = save.wagerAccessGranted;
        _secondWindAvailable = save.secondWind;
        _pendingRelicChoices = save.pendingRelicChoices;
        _enemiesKilled = save.enemiesKilled;
        _bossesDefeated = save.bossesDefeated;
        _runRoomsCleared = save.roomsCleared;
        _runElitesSlain = save.elitesSlain;
        _runTimer = save.timer;
        _gameTimer = save.gameTimer;
        _runSeed = save.seed;
        _runDomainLog = save.domainLog;
        _domainStartRooms = save.domainStartRooms;
        _domainStartSeconds = save.domainStartSeconds;
        _runDeath = RunLogDeath();
        _player.ClearLastDamageSource();

        ClearRunModifiers();
        for (const ModifierCheckpoint& saved : save.modifiers)
        {
            RunModifier m;
            m.label = saved.label;
            m.enemyHpMult = saved.enemyHpMult;
            m.enemyDmgMult = saved.enemyDmgMult;
            m.goldMult = saved.goldMult;
            m.xpMult = saved.xpMult;
            m.tint = UnpackRgba(saved.tintRgba);
            m.active = saved.active;
            m.roomsRemaining = saved.roomsRemaining;
            m.shopContract = (ShopContractType)saved.shopContract;
            m.curse = saved.curse;
            _runModifiers.push_back(m);
        }

        _currentBiome = save.biome;
        _pendingBiome = _currentBiome;
        _useHandcraftedDungeonRooms = true;
        _lastHandcraftedRoomId.clear();
        _forcedHandcraftedRoomId.clear();
        RefreshHandcraftedRooms();
    },
    [this] { LoadTilesetForBiome(_currentBiome); },
    [this, save] {
        if (save.atCrossroads)
        {
            // Back at the same crossroads: the same three roads and rewards.
            for (int i = 0; i < RunCheckpointLimits::kCrossroadsDoors; ++i)
            {
                _crossroadsDoors[i].open = save.crossroads[i].open;
                _crossroadsDoors[i].biome = save.crossroads[i].open ? save.crossroads[i].biome : Biome::Caverns;
                _crossroadsDoors[i].boon = save.crossroads[i].boon < (int)PathBoon::Count
                    ? (PathBoon)save.crossroads[i].boon : PathBoon::Gold;
            }
            OpenCrossroads();
            return;
        }
        if (save.atWorldMap)
        {
            _worldMapPreparedZone = -1;
            OpenWorldMap();
            return;
        }

        // The domain exactly as it was: graph, authored rooms, per-room progress.
        // The checkpoint's graph was validated when it was read; a layout that
        // no longer exists in this build makes the domain start fresh instead.
        const DungeonCheckpoint& d = save.dungeon;
        std::vector<DungeonRoom> rooms;
        std::vector<std::string> layouts;
        bool layoutsValid = true;
        for (const RoomCheckpoint& saved : d.rooms)
        {
            DungeonRoom r;
            r.col = saved.col;
            r.row = saved.row;
            r.type = (RoomType)saved.type;
            r.encounterProfile = (EncounterProfile)saved.profile;
            r.depth = saved.depth;
            r.hasNorth = saved.north;
            r.hasSouth = saved.south;
            r.hasEast = saved.east;
            r.hasWest = saved.west;
            r.startsEmpty = saved.startsEmpty;
            rooms.push_back(r);
            layouts.push_back(saved.layout);
            layoutsValid = layoutsValid && _roomLibrary.FindById(saved.layout) != nullptr;
        }
        if (!layoutsValid || !_dungeonGen.Restore(rooms, d.start, d.boss, d.key))
        {
            TraceLog(LOG_WARNING, "Run resume: saved domain unusable, restarting it from the entrance");
            _dungeonGen.SetPlan(DungeonGen::PlanForDomain(_worldZone));
            if (!GenerateHandcraftedDungeon(_currentBiome, _handcraftedDungeonRoomIds, 600, _worldZone > 0))
                throw std::runtime_error("This region's handcrafted room library is incomplete");
            EnterDungeonRoom(_dungeonGen.GetStartIndex(), DungeonDoorSide::None, GetDungeonEntranceSpawnPos(), true);
            return;
        }
        _handcraftedDungeonRoomIds = layouts;
        _dungeonRoomStates.clear();
        for (std::size_t i = 0; i < d.rooms.size(); ++i)
        {
            const RoomCheckpoint& saved = d.rooms[i];
            if (!saved.hasProgress) continue;
            DungeonRoomState& s = _dungeonRoomStates[(int)i];
            s.cleared = saved.cleared;
            s.visited = saved.visited;
            s.eliteMechanic = saved.eliteMechanic;
            s.holdoutTimeRemaining = saved.holdout;
            s.special = (RoomSpecialType)saved.special;
            s.specialClaimed = saved.specialClaimed;
            s.specialChoice = saved.specialChoice;
            for (int o = 0; o < RunCheckpointLimits::kSpecialOptions; ++o) s.specialOptions[o] = saved.specialOptions[o];
            // A fight interrupted by the quit restarts with fresh enemies.
            s.enemiesInitialized = s.cleared;
            s.survivors.clear();
        }
        _hasMagicGem = d.hasMagicGem;
        _magicGemSpawned = d.magicGemSpawned;
        _magicGemCollected = d.magicGemCollected;
        _bossBarrierUnlocked = d.bossBarrierUnlocked;
        _treasureChestSpawned = d.chestSpawned;
        _treasureChestBroken = d.chestBroken;
        // Zeph's stock comes back exactly; a checkpoint without it has the
        // camp restock once instead (still no reheal or rebank).
        if (save.hasShop && save.shop.zone == _worldZone && _shop.RestoreStock(save.shop, GetShopTier()))
        {
            _shopStockZone = _worldZone;
            _campNeedsRestock = false;
        }
        else
            _campNeedsRestock = true;

        EnterDungeonRoom(d.current, (DungeonDoorSide)d.entrySide, Vector2{ d.entryX, d.entryY }, false);
    }},
    [this] {
        // Resume failed under the cover: back to the menu, the save is kept.
        _nextRunPrepared = false;
        ResetRunState();
        _menu.Init();
        _gameState = GameState::Menu;
    });
    return true;
}
