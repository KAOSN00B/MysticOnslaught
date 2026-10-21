// =============================================================================
// EngineFlowSelfTest.cpp — automated end-to-end flow test (dev builds only).
//
// Drives the REAL Engine through the player journey with the same functions
// the UI calls (no input simulation, no mocks), then a second launch checks
// what persisted. It exists because formula-level tests passed while purchase,
// reward and save integration bugs slipped through.
//
// Run (see tools\run_flow_selftest.ps1, which does all of this):
//   set MO_PROFILE_DIR=<empty sandbox folder>
//   set MO_FLOW_SELFTEST=1   -> new profile play-through, writes expectations
//   set MO_FLOW_SELFTEST=2   -> relaunch: verifies the persisted results
//   set MO_FLOW_SELFTEST=3   -> biome tour: enters every domain through the
//                               real world-map path and screenshots its rooms
//                               (each entrance at noon, evening and night)
//   set MO_FLOW_SELFTEST=4/5 -> mid-run save: Save & Main Menu, then relaunch
//                               and Continue back into the identical run
//   set MO_FLOW_SELFTEST=6   -> room events: generation rules per domain and
//                               every event's card screen and effects
//   set MO_FLOW_SELFTEST=7   -> combat stress (on demand, not in the gate; see
//                               tools\run_combat_stress.ps1): frame timing with
//                               10, 25 and 50 enemies, as FRAMETIME log lines
//   set MO_FLOW_SELFTEST=8   -> Character Animator: every character opens, the
//                               size readout, size + undo, the unsaved prompt
//                               and the size lineup (never saves a file)
// Exit code 0 = every check passed. A report is written to the sandbox.
// =============================================================================

#if defined(_MSC_VER)
#pragma warning(disable: 4996)   // std::getenv: read-only test switches
#endif
#include "Engine.h"
#include "AssetPaths.h"
#include "AncientBear.h"
#include "DayNight.h"
#include "RunCheckpoint.h"
#include "RoomEvents.h"
#include "SaveFiles.h"
#include "SaveIds.h"
#include "SaveStore.h"
#include "SfxBank.h"
#include "Shieldbearer.h"
#include "ShopPricing.h"
#include "UserDataPath.h"
#include "VfxGlow.h"

#include <climits>
#include <cmath>
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <algorithm>

int Engine::s_processExitCode = 0;

#if MO_DEV_TOOLS

namespace
{
    constexpr float kStepTimeout = 45.f;
    // Cause-of-death probes: a lethal burn credited to an elite Infernal, and
    // a light enemy shot.
    constexpr float kFlowBurnDelay     = 0.1f;
    constexpr int   kFlowBurnTicks     = 2;
    constexpr float kFlowLethalBurn    = 99999.f;
    constexpr int   kFlowProbeDamage   = 10;
    constexpr float kDefenseEpsilon    = 0.01f;   // Defense comparisons
    constexpr int   kCharacterSheetPauseAction = 8;   // PauseAndGameOver::UpdatePause code
    // Time of day: an hour deep in the night, and how much darker an outdoor
    // place must be then (share of its brightness before nightfall).
    constexpr float kFlowNightHour     = 23.f;
    constexpr float kFlowMaxNightShare = 0.80f;
    constexpr int   kFlowLookFrames    = 2;       // frames for a new hour to be drawn

    std::map<std::string, std::string> ReadKeyValues(const std::string& path)
    {
        std::map<std::string, std::string> kv;
        std::ifstream in(path);
        std::string line;
        while (std::getline(in, line))
        {
            const size_t eq = line.find('=');
            if (eq != std::string::npos) kv[line.substr(0, eq)] = line.substr(eq + 1);
        }
        return kv;
    }

    // A signed save as the game would read it (signature, size cap, JSON);
    // null when it is missing or does not verify.
    JsonValue ReadSaveDocument(const std::string& path, SaveKind kind, std::size_t maxBytes)
    {
        JsonValue document;
        std::string error;
        return SaveFile::ReadDocument(path, kind, maxBytes, document, error) ? document : JsonValue();
    }

    JsonValue ReadRunCheckpointDocument()
    {
        return ReadSaveDocument(SaveFiles::RunCheckpointPath(), SaveKind::RunCheckpoint, SaveFiles::kMaxRunCheckpointBytes);
    }

    // The value at a dotted path ("grimoire.relics", "run.modifiers.0.curse"),
    // or null. Numeric parts index arrays.
    const JsonValue* FindPath(const JsonValue& document, const std::string& path)
    {
        const JsonValue* at = &document;
        std::size_t start = 0;
        while (at && start <= path.size())
        {
            const std::size_t dot = path.find('.', start);
            const std::string key = path.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
            if (at->IsArray() && !key.empty() && key.find_first_not_of("0123456789") == std::string::npos)
                at = std::stoul(key) < at->Size() ? &at->At(std::stoul(key)) : nullptr;
            else
                at = at->IsObject() ? at->Find(key) : nullptr;
            if (dot == std::string::npos) break;
            start = dot + 1;
        }
        return at;
    }

    bool PathIs(const JsonValue& document, const std::string& path, const std::string& expectedJson)
    {
        const JsonValue* value = FindPath(document, path);
        std::string text;
        return value && WriteJson(*value, text) && text == expectedJson;
    }

    std::string ReadFileBytes(const std::string& path)
    {
        std::ifstream in(path, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }

    void WriteFileBytes(const std::string& path, const std::string& bytes)
    {
        std::ofstream(path, std::ios::binary | std::ios::trunc) << bytes;
    }

    bool ListHas(const JsonValue* list, const char* name)
    {
        for (std::size_t i = 0; list && list->IsArray() && i < list->Size(); ++i)
            if (list->At(i).GetString() == name) return true;
        return false;
    }

    // Every room's authored layout id, in graph order, as one comparable line.
    std::string FlowJoinLayouts(const std::vector<std::string>& ids)
    {
        std::string joined;
        for (const std::string& id : ids) joined += (joined.empty() ? "" : ",") + id;
        return joined;
    }
}

bool Engine::FlowSelfTestRequested()
{
    const char* mode = std::getenv("MO_FLOW_SELFTEST");
    return mode != nullptr && mode[0] >= '1' && mode[0] <= '8' && mode[1] == 0;
}

void Engine::InitFlowSelfTest()
{
    const char* mode = std::getenv("MO_FLOW_SELFTEST");
    _flowTest = FlowSelfTest{};
    _flowTest.phase = (mode && mode[0] >= '2' && mode[0] <= '8') ? mode[0] - '0' : 1;
    FlowLog(TextFormat("flow self-test phase %d started", _flowTest.phase));
}

void Engine::FlowLog(const std::string& line)
{
    _flowTest.report += line + "\n";
    TraceLog(LOG_INFO, "FLOWTEST %s", line.c_str());
}

void Engine::FlowCheck(bool ok, const std::string& what)
{
    ++_flowTest.checks;
    if (!ok) ++_flowTest.failures;
    FlowLog(std::string(ok ? "PASS  " : "FAIL  ") + what);
}

void Engine::FlowNextStep()
{
    ++_flowTest.step;
    _flowTest.stepTime = 0.f;
    _flowTest.subStep = 0;
}

void Engine::FinishFlowSelfTest(bool aborted)
{
    // A checkpoint the game could not write back in a loadable form is a save
    // bug even when nothing else noticed (in play it is only logged).
    FlowCheck(_runCheckpointWriteFailures == _flowTest.injectedSaveFailures,
              TextFormat("saves: every run checkpoint this phase was written (%d failed, %d on purpose)",
                         _runCheckpointWriteFailures, _flowTest.injectedSaveFailures));
    if (aborted) ++_flowTest.failures;
    FlowLog(TextFormat("flow self-test phase %d finished: %d checks, %d failures%s",
                       _flowTest.phase, _flowTest.checks, _flowTest.failures, aborted ? " (aborted)" : ""));
    std::ofstream(UserDataPath(TextFormat("flowtest_report_phase%d.txt", _flowTest.phase))) << _flowTest.report;
    s_processExitCode = (_flowTest.failures == 0) ? 0 : 1;
    _flowTest.phase = 0;
    _shouldClose = true;
}

// Kills every live enemy through the base damage path (same as a pit fall),
// so the real room-clear, reward, milestone and relic code runs.
void Engine::FlowKillAllEnemies()
{
    for (auto& e : _enemies)
        if (e && e->IsActive() && e->IsAlive())
            e->BaseCharacter::TakeDamage(INT_MAX / 2, e->GetWorldPos());
}

// Saves the last fully drawn virtual-canvas frame (what the player sees) to
// <sandbox>/shots/<name>.png. Works with the hidden test window.
void Engine::FlowScreenshot(const std::string& name)
{
    Image img = LoadImageFromTexture(_virtualCanvas.texture);
    ImageFlipVertical(&img);   // render textures are stored upside down
    const std::string dir = UserDataPath("shots");
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const std::string path = (std::filesystem::path(dir) / (name + ".png")).string();
    ExportImage(img, path.c_str());
    UnloadImage(img);
    FlowLog("shot  " + path);
}

// Average brightness (0-1) of the last fully drawn frame, sampled on a grid.
float Engine::FlowCanvasBrightness() const
{
    constexpr int kSampleStep = 8;   // every 8th pixel each way is plenty for an average
    Image img = LoadImageFromTexture(_virtualCanvas.texture);
    double sum = 0.0;
    long long samples = 0;
    for (int y = 0; y < img.height; y += kSampleStep)
        for (int x = 0; x < img.width; x += kSampleStep)
        {
            const Color c = GetImageColor(img, x, y);
            sum += (0.299 * c.r + 0.587 * c.g + 0.114 * c.b) / 255.0;
            ++samples;
        }
    UnloadImage(img);
    return samples > 0 ? (float)(sum / samples) : 0.f;
}

bool Engine::FlowTravelSettled() const
{
    return !_journey.Active() && !_journeyExecuting && _dungeonFadeState == DungeonFadeState::None;
}

void Engine::UpdateFlowSelfTest(float dt)
{
    FlowSelfTest& t = _flowTest;
    t.stepTime += dt;
    // Answer reward screens the way a player would (first card), so every step
    // runs through real level-ups and elite rewards instead of stalling.
    if (_gameState == GameState::LevelUpChoice && _levelUpOpenTimer <= 0.f)
    {
        if (_showUltimateRow && !_ultimateRowPicked) ConfirmLevelUpCard(true, 0);
        if (!_regularRowPicked) ConfirmLevelUpCard(false, 0);
        ++t.levelUpsAnswered;
        return;
    }
    if (t.stepTime > kStepTimeout)
    {
        FlowCheck(false, TextFormat("step %d did not complete within %.0f s (state %d)",
                                    t.step, kStepTimeout, (int)_gameState));
        if (_dungeonRoomIdx >= 0)
        {
            const Rectangle exit = GetDungeonBossExitTrigger();
            const int bossIdx = _dungeonGen.GetBossIndex();
            const std::string roomId = (_dungeonRoomIdx < (int)_handcraftedDungeonRoomIds.size())
                ? _handcraftedDungeonRoomIds[_dungeonRoomIdx] : std::string("(procedural)");
            FlowLog(TextFormat("diag  room %d (boss %d, cleared %d) id %s; exit rect %.0f,%.0f %.0fx%.0f; player %.0f,%.0f; fade %d; enemies %d",
                               _dungeonRoomIdx, bossIdx, (int)_dungeonRoomStates[bossIdx].cleared, roomId.c_str(),
                               exit.x, exit.y, exit.width, exit.height,
                               _player.GetWorldPos().x, _player.GetWorldPos().y,
                               (int)_dungeonFadeState, GetActiveEnemyCount()));
        }
        FinishFlowSelfTest(true);
        return;
    }
    if (t.phase == 1)      UpdateFlowSelfTestPhase1();
    else if (t.phase == 2) UpdateFlowSelfTestPhase2();
    else if (t.phase == 3) UpdateFlowSelfTestPhase3();
    else if (t.phase == 4) UpdateFlowSelfTestPhase4();
    else if (t.phase == 5) UpdateFlowSelfTestPhase5();
    else if (t.phase == 7) UpdateFlowSelfTestPhase7();
    else if (t.phase == 8) UpdateFlowSelfTestPhase8();
    else                   UpdateFlowSelfTestPhase6();
}

void Engine::UpdateFlowSelfTestPhase1()
{
    FlowSelfTest& t = _flowTest;
    const int baseHp = GetPlayerClassInfo(PlayerClass::Warrior).baseHealth;
    switch (t.step)
    {
    case 0:   // fresh profile -> New Game -> class select -> village
        if (t.subStep < 1000)   // menu extras first: the Credits overlay and Settings > Comfort
        {
            if (t.subStep == 0)
            {
                if (_gameState != GameState::Menu || !FlowTravelSettled()) return;
                _menu.OpenCredits();
                FlowCheck(_menu.IsShowingCredits(), "menu: Credits opens");
            }
            else if (t.subStep == 30)
            {
                FlowScreenshot("00_credits");
                _menu.ScrollCredits(100000.f);   // jump to the end of the list
            }
            else if (t.subStep == 36)
            {
                FlowScreenshot("00_credits_end");
                _menu.CloseCredits();
                _stateBeforeSettings = GameState::Menu;
                _settingsTab         = 3;
                _settingsDragSlider  = -1;
                _keybindingsEdit     = _player.GetBindings();
                _gameState           = GameState::Settings;
            }
            else if (t.subStep == 60)
            {
                FlowScreenshot("00_settings_comfort");
                _gameState   = GameState::Menu;
                _settingsTab = 0;
                GameSettings& s = _settingsMgr.Get();
                const GameSettings saved = s;
                _hitStopTimer = 0.f;
                s.hitStopLevel = 2;
                RequestHitStop(0.1f);
                FlowCheck(_hitStopTimer == 0.f, "comfort: Hit-Stop Off skips the freeze");
                s.hitStopLevel = 1;
                RequestHitStop(0.1f);
                FlowCheck(fabsf(_hitStopTimer - 0.05f) < 1e-4f, "comfort: Hit-Stop Reduced halves the freeze");
                _hitStopTimer = 0.f;
                _shakeTimer = _shakeDuration = _shakeStrength = 0.f;
                s.screenShake = 0.f;
                TriggerScreenShake(10.f, 0.5f);
                FlowCheck(_shakeStrength == 0.f, "comfort: Screen Shake 0% means no shake");
                s.screenShake = 0.5f;
                TriggerScreenShake(10.f, 0.5f);
                FlowCheck(fabsf(_shakeStrength - 5.f * _juiceShakeMult) < 1e-3f, "comfort: Screen Shake scales every shake");
                _shakeTimer = _shakeDuration = _shakeStrength = 0.f;
                s = saved;
                t.subStep = 1000;
                return;
            }
            ++t.subStep;
            return;
        }
        if (_gameState != GameState::Menu || !FlowTravelSettled()) return;
        FlowCheck(!_meta.HasAnyProgress(), "fresh sandbox profile has no progress");
        _prologueEntryMode = PrologueEntryMode::NewGame;
        _classSelectReturnState = GameState::Menu;
        _gameState = GameState::ClassSelect;
        FlowCheck(_meta.IsRogueUnlocked() == false, "Rogue starts locked");
        FlowCheck(ConfirmClassSelection(PlayerClass::Warrior), "Warrior is selectable on a fresh profile");
        FlowNextStep();
        return;

    case 1:   // village arrival: the next run is prepared, Zeph's welcome
        if (_gameState != GameState::Village || !FlowTravelSettled()) return;
        if (t.subStep < 20) { if (++t.subStep == 20) FlowScreenshot("01_village_first_visit"); return; }
        // The village is outdoors: at night it darkens; then the hour is put back.
        {
            constexpr int kNightAt = 20, kNightShot = kNightAt + kFlowLookFrames + 1;
            if (t.subStep == kNightAt)
            {
                t.noonBrightness = FlowCanvasBrightness();
                t.clockBefore = _clockHours;
                _clockHours = kFlowNightHour;
            }
            if (t.subStep < kNightShot) { ++t.subStep; return; }
            if (t.subStep == kNightShot)
            {
                FlowScreenshot("01_village_night");
                const float night = FlowCanvasBrightness();
                FlowCheck(night < t.noonBrightness * kFlowMaxNightShare,
                          TextFormat("village: night falls (brightness %.2f on arrival, %.2f at night)",
                                     t.noonBrightness, night));
                _clockHours = t.clockBefore;
                ++t.subStep;
            }
        }
        FlowCheck(_nextRunPrepared, "village arrival prepares the next run");
        FlowCheck(_player.GetClass() == PlayerClass::Warrior, "prepared character uses the chosen class");
        FlowCheck(_player.GetLearnedCount() >= 1, "starter ability learned before the run");
        FlowCheck(_player.GetGold() == 0 && _meta.GetVillageWallet() == 0, "fresh wallet is empty");
        FlowCheck(_firstVillageVisit, "first village visit asks the player to meet Zeph");
        FlowCheck(VillageHasPlacedObject("VillageCottageA") && VillageHasPlacedObject("VillageCottageB") &&
                  VillageHasPlacedObject("VillageTownhouseA") && VillageHasPlacedObject("VillageTownhouseB"),
                  "the town's homes stand from the first visit");
        FlowCheck(!VillageHasPlacedObject("VillageTrophyHall") && !VillageHasPlacedObject("VillageArmory"),
                  "Trophy Hall and Armory wait for their unlocks");
        {
            bool townInProfile = false;
            for (const auto& p : _meta.GetVillagePlacements()) townInProfile = townInProfile || IsVillageCoreBuilding(p.asset);
            FlowCheck(!townInProfile, "core town buildings are not stored as player buildings");
            const int cottage = FindVillageObjectDefIndex("VillageCottageA");
            FlowCheck(cottage >= 0 && !VillagePlaygroundCanPlace(cottage, 20, 12), "core town buildings cannot be player-placed");
        }
        FlowCheck(GetRunStarterAbility(PlayerClass::Warrior) == GetArmoryOptions(PlayerClass::Warrior)[0] &&
                  _player.HasLearnedAbility(GetArmoryOptions(PlayerClass::Warrior)[0]),
                  "fresh profile starts with the class default ability");
        FlowCheck(!ConfirmArmoryChoice(1), "a locked Armory choice is refused");
        FinishVillageIntro();
        FlowCheck(_player.GetGold() == Balance::Economy::kNewProfileWelcomeGold, "Zeph's welcome purse paid once");
        FlowCheck(_meta.GetVillageWallet() == _player.GetGold(), "welcome purse saved as the village wallet");
        FlowNextStep();
        return;

    case 2:   // village shop: real purchase, stale-card refusal, contract rules
    {
        std::vector<ShopManager::OfferView> offers = _shop.GetOffers();
        FlowCheck(offers.size() == 6, "Zeph stocks a full six-card page in the village");
        int wareIdx = -1;
        for (int i = 0; i < (int)offers.size(); ++i)
            if (!offers[i].isAbility && !offers[i].isContract && offers[i].price <= _player.GetGold() &&
                offers[i].ware == ShopWareType::IronrootTonic)
                wareIdx = i;
        for (int i = 0; wareIdx < 0 && i < (int)offers.size(); ++i)
            if (!offers[i].isAbility && !offers[i].isContract && offers[i].price <= _player.GetGold() &&
                offers[i].ware != ShopWareType::FieldDressing && offers[i].ware != ShopWareType::ManaPrism)
                wareIdx = i;
        if (wareIdx >= 0)
        {
            const int before = _player.GetGold(), price = offers[wareIdx].price;
            t.boughtWare = (int)offers[wareIdx].ware;
            t.defenseBefore = _player.GetDefense();
            FlowCheck(_shop.TryPurchase(wareIdx, _player) == ShopManager::PurchaseResult::Purchased, "village ware purchase succeeds");
            FlowCheck(_player.GetGold() == before - price, "ware purchase charges exactly its price");
        }
        else FlowLog("note  no affordable applicable ware in this stock (random); purchase check skipped");

        // Finding 3: a card whose effect became impossible must not take gold.
        offers = _shop.GetOffers();
        for (int i = 0; i < (int)offers.size(); ++i)
        {
            if (!offers[i].isAbility || offers[i].upgradesAbility || offers[i].purchased) continue;
            _player.LearnAbility(offers[i].ability);          // owned through another path
            _player.AddGold(1000);
            const int before = _player.GetGold();
            const ShopManager::PurchaseResult r = _shop.TryPurchase(i, _player);
            FlowCheck(r == ShopManager::PurchaseResult::NoEffect && _player.GetGold() == before,
                      "stale ability card refuses and charges nothing");
            _player.AddGold(-1000);
            break;
        }

        // Finding 5: one shop contract per fight, judged on run state.
        offers = _shop.GetOffers();
        for (int i = 0; i < (int)offers.size(); ++i)
        {
            if (!offers[i].isContract || offers[i].purchased) continue;
            FlowCheck(_shop.TryPurchase(i, _player) == ShopManager::PurchaseResult::Purchased, "first contract accepted");
            CommitShopContract();                            // leaving the shop
            _shop.SetRunContractPending(true);               // as Engine reports next frame
            _player.AddGold(500);
            FlowCheck(_shop.TryReroll(_player), "reroll after accepting a contract");
            std::vector<ShopManager::OfferView> fresh = _shop.GetOffers();
            int contracts = 0;
            for (int k = 0; k < (int)fresh.size(); ++k)
                if (fresh[k].isContract)
                {
                    ++contracts;
                    FlowCheck(_shop.TryPurchase(k, _player) == ShopManager::PurchaseResult::ContractPending,
                              "second contract refused while one waits for the next fight");
                }
            FlowCheck(contracts == 1, "reroll keeps exactly one contract card");
            // Reserve a ware and reroll: the contract slot must survive.
            for (int k = 0; k < (int)fresh.size(); ++k)
                if (!fresh[k].isAbility && !fresh[k].isContract) { _shop.TryReserve(k, _player); break; }
            _shop.TryReroll(_player);
            int contractsAfter = 0;
            for (const auto& o : _shop.GetOffers()) if (o.isContract) ++contractsAfter;
            FlowCheck(contractsAfter == 1, "a reserved ware never displaces the contract card");
            _runModifiers.clear();                           // keep the run simple from here
            _shop.SetRunContractPending(false);
            break;
        }
        _player.SetGold(0);
        _meta.SetVillageWallet(0);
        FlowNextStep();
        return;
    }

    case 3:   // Poe's altar purchases while the next run is already prepared
        _meta.BankCells(600);
        FlowCheck(_meta.Purchase(MetaUnlockType::Vitality1), "Vitality I purchasable at the altar");
        FlowCheck(_meta.Purchase(MetaUnlockType::SecondWind), "Second Wind purchasable at the altar");
        FlowCheck(_meta.Purchase(MetaUnlockType::StartingGold1), "Zeph's Stipend I purchasable at the altar");
        FlowCheck((int)_player.GetMaxHealthValue() == baseHp, "prepared character not yet buffed (applied at the gate)");
        t.bankedBeforeRun = _meta.GetBankedCells();
        FlowNextStep();
        return;

    case 4:   // gate -> Forest
        if (t.subStep == 0) { TriggerVillageDungeonGate(); t.subStep = 1; return; }
        if (_gameState != GameState::DungeonRun || !FlowTravelSettled()) return;
        FlowCheck((int)_player.GetMaxHealthValue() == baseHp + _meta.GetVitalityBonus(baseHp),
                  "Vitality bought in the village applies to the very next run");
        FlowCheck(_player.GetHealthValue() == _player.GetMaxHealthValue(), "run starts at full health");
        FlowCheck(_player.GetGold() == _meta.GetStartingGoldBonus(), "Stipend paid once at run start");
        FlowCheck(_meta.GetVillageWallet() == 0, "wallet leaves the village with the run");
        FlowCheck(_secondWindAvailable, "Second Wind armed for this run");
        FlowCheck(_worldZone == 0 && GetRunPowerLevel() == 1, "Forest is domain 0 / power level 1");
        if (t.boughtWare == (int)ShopWareType::IronrootTonic)
            FlowCheck(_player.GetDefense() >= t.defenseBefore + Balance::Stats::kDefenseMajor - kDefenseEpsilon,
                      "village ware carried into the run");
        _player.SetInvulnerableLock(true);
        FlowNextStep();
        return;

    case 5:   // clear the boss room through the real clear path
    {
        const int bossIdx = _dungeonGen.GetBossIndex();
        if (t.subStep == 0)
        {
            // Room centre: well away from the exit wall, as a player entering
            // through a door would be.
            EnterDungeonRoom(bossIdx, DungeonDoorSide::None,
                             Vector2{ (float)kVirtualWidth * 0.5f, (float)kVirtualHeight * 0.55f }, false);
            t.subStep = 1;
            return;
        }
        if (t.subStep == 1)
        {
            if (_gameState == GameState::RelicChoice) { t.subStep = 2; return; }
            if (_gameState != GameState::DungeonRun) return;
            if (_dungeonRoomStates[bossIdx].cleared && _pendingRelicChoices == 0 && t.stepTime > 8.f)
            {
                FlowCheck(false, "boss clear owed a relic pick but none opened");
                t.subStep = 3;
                return;
            }
            FlowKillAllEnemies();
            _dungeonReinforcements.clear();
            return;
        }
        if (t.subStep == 2)
        {
            const int before = _player.GetRelicCount();
            ConfirmRelicChoice(0);
            FlowCheck(_player.GetRelicCount() == before + 1, "boss relic pick opens in the dungeon and grants a relic");
            FlowCheck(_meta.IsRogueUnlocked(), "first boss unlocks the Rogue");
            t.subStep = 3;
            return;
        }
        if (_gameState != GameState::DungeonRun) return;
        _pickups.clear();   // boss gold drops would blur the bounty check
        _player.AddCellsFromDrop(7);
        t.carriedCells = _player.GetCells();
        t.goldBeforeExit = _player.GetGold();
        t.hpBeforeExit = _player.GetHealthValue();
        const Rectangle exit = GetDungeonBossExitTrigger();
        _player.SetWorldPos({ exit.x + exit.width * 0.5f, exit.y + exit.height * 0.5f });
        FlowNextStep();
        return;
    }

    case 6:   // boss exit -> domain complete -> the crossroads
        if (_gameState == GameState::DungeonRun && !_atCrossroads &&
            _dungeonFadeState == DungeonFadeState::None && !_journey.Active())
        {
            // Keep walking into the exit (collision can nudge a teleported player
            // back out of a doorway, exactly as it would stop a real player).
            const Rectangle exit = GetDungeonBossExitTrigger();
            _player.SetWorldPos({ exit.x + exit.width * 0.5f, exit.y + exit.height * 0.5f });
            return;
        }
        if (!_atCrossroads || _gameState != GameState::DungeonRun || !FlowTravelSettled()) return;
        if (t.subStep < 30) { if (++t.subStep == 30) FlowScreenshot("02_crossroads"); return; }
        FlowCheck(_player.GetGold() == t.goldBeforeExit + Balance::Economy::kDomainBountyBase,
                  "domain bounty paid on boss exit");
        FlowCheck(_player.GetHealthValue() >= std::min(_player.GetMaxHealthValue(), t.hpBeforeExit), "breather heal never reduces HP");
        FlowCheck(_player.GetCells() == t.carriedCells, "Echoes stay carried until Zeph's camp");
        {
            // Three doors, three different unplayed domains, three different rewards.
            int open = 0;
            bool fresh = true, distinct = true, rewards = true;
            for (int i = 0; i < 3; ++i)
            {
                const CrossroadsDoor& d = _crossroadsDoors[i];
                if (!d.open) continue;
                ++open;
                fresh = fresh && d.biome != Biome::Forest && d.biome != Biome::DemonsInsides;
                for (int j = i + 1; j < 3; ++j)
                {
                    distinct = distinct && _crossroadsDoors[j].biome != d.biome;
                    rewards = rewards && _crossroadsDoors[j].boon != d.boon;
                }
            }
            FlowCheck(open == 3 && fresh && distinct, "crossroads: three doors to three unplayed domains");
            FlowCheck(rewards, "crossroads: each road has its own arrival reward");
            const int start = _dungeonGen.GetStartIndex();
            FlowCheck(_crossroadsGateway[0] == _dungeonGen.GetNeighborIndex(start, 0, -1) &&
                      _crossroadsGateway[1] == _dungeonGen.GetNeighborIndex(start, -1, 0) &&
                      _crossroadsGateway[2] == _dungeonGen.GetNeighborIndex(start, 0, 1) &&
                      _dungeonGen.GetBossIndex() < 0, "crossroads: west, north and east doors each open a road");
            FlowCheck(_dungeonRoomStates[start].cleared && GetActiveEnemyCount() == 0, "crossroads: a quiet room");
            const JsonValue checkpoint = ReadRunCheckpointDocument();
            FlowCheck(PathIs(checkpoint, "location.atCrossroads", "true") &&
                      PathIs(checkpoint, "location.crossroads.1.biome", std::string("\"") + SaveIdOf(_crossroadsDoors[1].biome) + "\""),
                      "crossroads: the checkpoint remembers the roads");
            // Take a road paying gold or a heal: the camp checks below count Echoes exactly.
            int door = 0;
            for (int i = 0; i < 3; ++i)
                if (_crossroadsDoors[i].boon == PathBoon::Gold || _crossroadsDoors[i].boon == PathBoon::Vigor) door = i;
            t.pathBoon = (int)_crossroadsDoors[door].boon;
            t.goldAtCrossroads = _player.GetGold();
            t.bankedBeforeCamp = _meta.GetBankedCells();
            ChooseCrossroadsDoor(door);
            ChooseCrossroadsDoor((door + 1) % 3);   // a second door can't also be taken
        }
        FlowNextStep();
        return;

    case 7:   // next domain opens at Zeph's camp
    {
        if (_gameState != GameState::DungeonRun || !FlowTravelSettled()) return;
        if (t.subStep < 20) { if (++t.subStep == 20) FlowScreenshot("02_zeph_camp"); return; }
        const int start = _dungeonGen.GetStartIndex();
        FlowCheck(!_atCrossroads && _worldCompletedBiomes.size() == 1, "crossroads: one road taken");
        if (t.pathBoon == (int)PathBoon::Gold)
            FlowCheck(_player.GetGold() == t.goldAtCrossroads + PathBoonAmount(PathBoon::Gold, 1), "crossroads: the gold road pays on arrival");
        else
            FlowCheck(_player.GetHealthValue() >= _player.GetMaxHealthValue() - 0.01f, "crossroads: the healing road heals on arrival");
        FlowCheck(_worldZone == 1 && GetRunPowerLevel() == 2, "second domain is power level 2");
        FlowCheck(_dungeonGen.GetRooms()[start].type == RoomType::Store, "domain entrance is Zeph's camp");
        FlowCheck(_player.GetCells() == 0 && _meta.GetBankedCells() == t.bankedBeforeCamp + t.carriedCells,
                  "carried Echoes banked at the camp");
        FlowCheck(GetShopTier() == 2, "camp shop prices for domain 1 (tier 2)");
        // Re-entering the camp must not restock, reheal or rebank.
        std::vector<ShopManager::OfferView> before = _shop.GetOffers();
        int neighbour = -1;
        for (int d = 0; d < 4 && neighbour < 0; ++d)
            neighbour = _dungeonGen.GetNeighborIndex(start, d == 0 ? -1 : d == 1 ? 1 : 0, d == 2 ? -1 : d == 3 ? 1 : 0);
        if (neighbour >= 0)
        {
            _player.SetInvulnerableLock(true);
            EnterDungeonRoom(neighbour, DungeonDoorSide::None, GetDungeonEntranceSpawnPos(), false);
            EnterDungeonRoom(start, DungeonDoorSide::None, GetDungeonEntranceSpawnPos(), false);
            std::vector<ShopManager::OfferView> after = _shop.GetOffers();
            bool same = before.size() == after.size();
            for (size_t i = 0; same && i < before.size(); ++i)
                same = before[i].price == after[i].price && before[i].isContract == after[i].isContract &&
                       before[i].ability == after[i].ability && before[i].ware == after[i].ware;
            FlowCheck(same, "re-entering Zeph's camp does not restock");
        }
        FlowNextStep();
        return;
    }

    case 8:   // Second Wind, then a real death
        if (t.subStep == 0)
        {
            _player.SetInvulnerableLock(false);
            _player.AddGold(100);
            _player.AddCellsFromDrop(9);
            t.carriedCells = _player.GetCells();
            t.bankedBeforeDeath = _meta.GetBankedCells();
            _player.BaseCharacter::TakeDamage(99999, _player.GetWorldPos());
            t.subStep = 1;
            return;
        }
        if (t.subStep == 1)
        {
            if (_gameState != GameState::DungeonRun) return;
            if (_player.GetHealthValue() <= 0.f) return;   // revive happens on the next dungeon update
            FlowCheck(!_secondWindAvailable && !_playerDying, "Second Wind revives once in the dungeon");
            FlowCheck((int)_player.GetHealthValue() == (int)std::ceil(_player.GetMaxHealthValue() * 0.4f), "Second Wind restores 40% HP");
            {
                // The killing blow is a burn credited to an elite Infernal: the
                // run log must name the status and who applied it.
                const DamageSourceScope infernal(_player, DamageSource::FromEnemy("Infernal", true, false));
                _player.ApplyBurnTicks(kFlowBurnDelay, kFlowBurnTicks, kFlowLethalBurn, _player.GetWorldPos());
            }
            t.subStep = 2;
            return;
        }
        if (_gameState == GameState::RunSummary)
        {
            if (t.subStep == 2)
            {
                FlowCheck(!_runSummary.victory && _runSummary.domainReached == 2 &&
                          _runSummary.echoes == t.carriedCells && _runSummary.ending.rfind("Fell", 0) == 0,
                          TextFormat("run summary: a death in domain 2 (%s)", _runSummary.ending.c_str()));
                FlowCheck(_runSummary.roomsCleared >= 1 && _runSummary.bossesDefeated == 1 &&
                          _runSummary.domains.size() == 2 && _runSummary.domains[0] == Biome::Forest,
                          TextFormat("run summary: %d rooms, %d boss, road of %d domains", _runSummary.roomsCleared,
                                     _runSummary.bossesDefeated, (int)_runSummary.domains.size()));
                const JsonValue runLog = ReadSaveDocument(SaveFiles::RunLogPath(), SaveKind::RunLog, SaveFiles::kMaxRunLogBytes);
                const JsonValue* ending = FindPath(runLog, "runs.0.ending");
                FlowCheck(PathIs(runLog, "totals.runs", "1") && PathIs(runLog, "runs.0.outcome", "\"death\"") &&
                          ending && ending->GetString() == _runSummary.ending, "run log records the run");
                FlowCheck(PathIs(runLog, "runs.0.death.cause.kind", "\"status\"") && PathIs(runLog, "runs.0.death.cause.id", "\"Burn\"") &&
                          PathIs(runLog, "runs.0.death.cause.by", "\"Infernal\"") && PathIs(runLog, "runs.0.death.cause.elite", "true"),
                          "run log: the cause of death is the Infernal's burn");
                const JsonValue* domains = FindPath(runLog, "runs.0.domains");
                FlowCheck(PathIs(runLog, "runs.0.domainReached", "2") && domains && domains->Size() == 2 &&
                          PathIs(runLog, "runs.0.domains.0.biome", "\"Forest\"") && PathIs(runLog, "runs.0.domains.0.bossDefeated", "true") &&
                          PathIs(runLog, "runs.0.domains.1.bossDefeated", "false"),
                          "run log: the domains in order, the first one cleared");
                const JsonValue* abilities = FindPath(runLog, "runs.0.build.abilities");
                FlowCheck(abilities && abilities->Size() >= 1 && PathIs(runLog, "runs.0.class", std::string("\"") + SaveIdOf(_player.GetClass()) + "\""),
                          "run log: the class and the build it ended with");
                FlowCheck(_meta.GetDeathCount() == 1 && _meta.GetDeathsByCause("status/Burn") == 1,
                          "profile: the death is counted by cause");
                t.subStep = 3;
            }
            if (++t.subStep == 20) FlowScreenshot("run_summary_death");
            if (t.subStep < 70) return;   // past the input lock
            LeaveRunSummary();
            return;
        }
        if (_gameState != GameState::DeathRevive) return;
        FlowCheck(_player.GetGold() == 0, "death loses carried gold (no Deep Pockets)");
        FlowCheck(_meta.GetBankedCells() == t.bankedBeforeDeath + MetaProgressionManager::GetEchoesKeptOnDeath(t.carriedCells),
                  "Poe salvages half the carried Echoes");
        FlowCheck(_meta.GetVillageWallet() == 0, "death leaves an empty wallet at home");
        FlowNextStep();
        return;

    case 9:   // Poe -> class select -> village with a newly unlocked class
        if (t.subStep == 0)
        {
            _deathRevivePhase = 1;
            ConfirmDeathRevive();
            FlowCheck(_gameState == GameState::ClassSelect, "Poe leads into class select");
            FlowCheck(ConfirmClassSelection(PlayerClass::Rogue), "Rogue now selectable");
            t.subStep = 1;
            return;
        }
        if (_gameState != GameState::Village || !FlowTravelSettled()) return;
        FlowCheck(_player.GetClass() == PlayerClass::Rogue && _nextRunPrepared, "next run prepared as the Rogue");
        FlowCheck(_player.GetRelicCount() == 0 && _player.GetLevel() == 1, "death wiped run-scoped power");
        FlowCheck(VillageHasPlacedObject("VillageTrophyHall"), "Trophy Hall opens after the first domain boss");
        FlowCheck(_meta.GetClassMasteryTier(PlayerClass::Warrior) >= 1 && VillageHasPlacedObject("VillageArmory"),
                  "Armory opens at the first class mastery rank");
        FlowCheck(_meta.HasSeenVillageUnlock(MetaProgressionManager::kSeenTrophyHall),
                  "the Trophy Hall opening is announced on arrival");
        {
            const std::vector<AbilityType> options = GetArmoryOptions(PlayerClass::Rogue);
            FlowCheck(options.size() == 6, "Armory offers the class starter plus five class abilities");
            FlowCheck(!ConfirmArmoryChoice(1), "Rogue's second choice is locked at mastery rank 0");
            for (int i = 0; i < 10 && _meta.GetClassMasteryTier(PlayerClass::Rogue) < 1; ++i)
                _meta.RecordRoomCleared(PlayerClass::Rogue);
            FlowCheck(ConfirmArmoryChoice(1), "Armory choice accepted at mastery rank 1");
            FlowCheck(_player.HasLearnedAbility(options[1]) && !_player.HasLearnedAbility(options[0]),
                      "the prepared Rogue's starter is swapped in place");
            FlowCheck(GetRunStarterAbility(PlayerClass::Rogue) == options[1], "the run starter follows the Armory choice");
            t.armoryStarter = (int)options[1];
            // The Trophy Hall opens the journal-only page and returns home.
            _metaShopReturnState = GameState::Village;
            _metaShopJournal = true;
            _metaShopTrophyHall = true;
            _gameState = GameState::MetaShop;
        }
        FlowNextStep();
        return;

    case 10:  // village building transaction (finding 1)
    {
        // The previous frame drew the Trophy Hall page; then one Armory frame.
        if (t.subStep == 0)
        {
            FlowCheck(_gameState == GameState::MetaShop && _metaShopTrophyHall, "Trophy Hall opens its journal page");
            FlowScreenshot("04_trophy_hall");
            _metaShopTrophyHall = false;
            _metaShopJournal = false;
            _gameState = GameState::Armory;
            _armoryOpenTimer = 0.25f;
            t.subStep = 1;
            return;
        }
        if (t.subStep == 1)
        {
            FlowScreenshot("05_armory");
            _gameState = GameState::Village;
            t.subStep = 2;
            return;
        }
        _player.AddGold(400);
        int defIdx = -1;
        for (int i = 0; i < (int)_villageObjectCatalog.size() && defIdx < 0; ++i)
        {
            const VillageRuntimeObjectDef& d = _villageObjectCatalog[i];
            if (d.costGold > 0 && d.costGold <= 400 && d.removable && !VillageObjectIsPermanentService(d))
                defIdx = i;
        }
        if (defIdx < 0) { FlowLog("note  no purchasable village building in the catalog; skipped"); FlowNextStep(); return; }
        int col = -1, row = -1;
        for (int r = 0; r < 30 && col < 0; ++r)
            for (int c = 0; c < 40 && col < 0; ++c)
                if (VillagePlaygroundCanPlace(defIdx, c, r)) { col = c; row = r; }
        if (col < 0) { FlowLog("note  no free village cell for the building; skipped"); FlowNextStep(); return; }
        const int cost = _villageObjectCatalog[defIdx].costGold;
        const std::string name = _villageObjectCatalog[defIdx].name;
        auto diskHas = [&](const std::string& asset, int& wallet) {
            MetaProgressionManager disk; disk.Load();
            wallet = disk.GetVillageWallet();
            int n = 0;
            for (const auto& p : disk.GetVillagePlacements()) if (p.asset == asset) ++n;
            return n;
        };
        int wallet = -1;
        const int countBefore = diskHas(name, wallet);
        const int goldBefore = _player.GetGold();
        FlowCheck(BuyVillageObject(defIdx, col, row), "village building purchase");
        int afterCount = diskHas(name, wallet);
        FlowCheck(afterCount == countBefore + 1 && wallet == goldBefore - cost, "building and its cost saved in one profile write");
        RemoveVillageObject((int)_villagePlacedObjects.size() - 1);
        afterCount = diskHas(name, wallet);
        FlowCheck(afterCount == countBefore && wallet == goldBefore, "same-visit removal refunds fully, saved together");
        FlowCheck(BuyVillageObject(defIdx, col, row), "rebuild the building for the relaunch check");
        afterCount = diskHas(name, wallet);
        t.expectBuilding = name;
        t.expectBuildingCount = afterCount;
        FlowNextStep();
        return;
    }

    case 11:  // a (shortened) winning run: bank on victory, unlock the Paladin, go home
        if (t.subStep == 0) { TriggerVillageDungeonGate(); t.subStep = 1; return; }
        if (t.subStep == 1)
        {
            if (_gameState != GameState::DungeonRun || !FlowTravelSettled()) return;
            FlowCheck(t.armoryStarter < 0 || _player.HasLearnedAbility((AbilityType)t.armoryStarter),
                      "the run starts with the Armory starter");
            _player.SetInvulnerableLock(true);
            _player.AddCellsFromDrop(11);
            t.carriedCells = _player.GetCells();
            t.bankedBeforeDeath = _meta.GetBankedCells();
            _worldZone = RunProgression::kFinalZone;   // skip to the final domain's boss exit
            CompleteDomain();
            FlowCheck(_gameState == GameState::RunSummary && _runSummary.victory &&
                      _runSummary.domainReached == RunProgression::kFinalZone + 1,
                      "final boss exit opens the run summary (victory)");
            {
                const JsonValue runLog = ReadSaveDocument(SaveFiles::RunLogPath(), SaveKind::RunLog, SaveFiles::kMaxRunLogBytes);
                FlowCheck(PathIs(runLog, "totals.runs", "2") && PathIs(runLog, "totals.wins", "1") &&
                          PathIs(runLog, "runs.0.outcome", "\"victory\"") && PathIs(runLog, "runs.1.outcome", "\"death\"") &&
                          !FindPath(runLog, "runs.0.death"), "run log: newest first, wins counted");
            }
            FlowCheck(_meta.HasFeat((int)Feat::FirstBlood) && _meta.HasFeat((int)Feat::Conqueror),
                      "feats: First Blood (a guardian) and Conqueror (a won run)");
            LeaveRunSummary();
            FlowCheck(_gameState == GameState::DemoEnd, "final boss exit reaches the victory screen");
            FlowCheck(_meta.HasCompletedGame() && _meta.IsPaladinUnlocked(), "victory unlocks the Paladin");
            FlowCheck(_meta.GetBankedCells() == t.bankedBeforeDeath + t.carriedCells, "victory banks every carried Echo");
            FlowCheck(_meta.GetVillageWallet() == _player.GetGold(), "victory carries the gold home");
            t.expectWallet = _player.GetGold();
            LeaveVictoryScreen();
            t.subStep = 2;
            return;
        }
        if (_gameState != GameState::Village || !FlowTravelSettled()) return;
        FlowCheck(_nextRunPrepared && !_isMainGameRun, "victory returns home with the next run prepared");
        {
            std::ofstream out(UserDataPath("flowtest_expect.txt"));
            out << "banked=" << _meta.GetBankedCells() << "\n"
                << "wallet=" << _meta.GetVillageWallet() << "\n"
                << "building=" << t.expectBuilding << "\n"
                << "building_count=" << t.expectBuildingCount << "\n"
                << "rogue_starter=" << t.armoryStarter << "\n";
        }
        _meta.SaveIfDirty();
        FinishFlowSelfTest(false);
        return;
    }
}

void Engine::UpdateFlowSelfTestPhase2()
{
    FlowSelfTest& t = _flowTest;
    switch (t.step)
    {
    case 0:
    {
        if (_gameState != GameState::Menu || !FlowTravelSettled()) return;
        std::map<std::string, std::string> kv = ReadKeyValues(UserDataPath("flowtest_expect.txt"));
        FlowCheck(!kv.empty(), "phase-1 expectations present");
        FlowCheck(_meta.HasAnyProgress(), "relaunch finds the saved profile (Continue available)");
        FlowCheck(std::to_string(_meta.GetBankedCells()) == kv["banked"], "banked Echoes survived the relaunch");
        FlowCheck(std::to_string(_meta.GetVillageWallet()) == kv["wallet"], "village wallet survived the relaunch");
        FlowCheck(_meta.IsUnlocked(MetaUnlockType::Vitality1) && _meta.IsUnlocked(MetaUnlockType::SecondWind),
                  "altar unlocks survived the relaunch");
        FlowCheck(_meta.IsRogueUnlocked() && _meta.IsPaladinUnlocked() && _meta.HasCompletedGame(),
                  "class unlocks and completion survived the relaunch");
        FlowCheck(std::to_string((int)_meta.GetStarterChoice(PlayerClass::Rogue)) == kv["rogue_starter"],
                  "Armory starter choice survived the relaunch");
        t.expectBuilding = kv["building"];
        t.expectBuildingCount = std::atoi(kv["building_count"].c_str());
        t.expectWallet = std::atoi(kv["wallet"].c_str());
        _prologueEntryMode = PrologueEntryMode::Continue;
        _classSelectReturnState = GameState::Menu;
        _gameState = GameState::ClassSelect;
        FlowCheck(ConfirmClassSelection(PlayerClass::Paladin), "Paladin selectable after the relaunch");
        FlowNextStep();
        return;
    }
    case 1:
    {
        if (_gameState != GameState::Village || !FlowTravelSettled()) return;
        if (t.subStep < 20) { if (++t.subStep == 20) FlowScreenshot("03_village_after_progress"); return; }
        FlowCheck(_player.GetGold() == t.expectWallet, "Continue restores the village wallet");
        if (!t.expectBuilding.empty())
        {
            int n = 0;
            for (const auto& p : _villagePlacedObjects) if (p.defName == t.expectBuilding) ++n;
            FlowCheck(n == t.expectBuildingCount, "village buildings restored from the profile");
        }
        FlowCheck(VillageHasPlacedObject("VillageTrophyHall") && VillageHasPlacedObject("VillageArmory") &&
                  VillageHasPlacedObject("VillageCottageA"), "the core town is rebuilt after the relaunch");
        FinishFlowSelfTest(false);
        return;
    }
    }
}

void Engine::UpdateFlowSelfTestPhase3()
{
    FlowSelfTest& t = _flowTest;
    static const Biome kTour[] = {
        Biome::Forest, Biome::AncientCastle, Biome::Caverns, Biome::DemonsInsides, Biome::DreamRealm,
        Biome::Graveyard, Biome::Jungle, Biome::LostCity, Biome::TheSanctuary, Biome::Wastelands };
    constexpr int kTourCount = (int)(sizeof(kTour) / sizeof(kTour[0]));
    constexpr int kSettleFrames = 20;
    // Time of day: the tour holds noon (the art as drawn) so its screenshots
    // compare across runs; each entrance is also shot at evening and at night.
    constexpr float kTourNoon = 12.f, kTourEvening = 19.f;
    constexpr int   kNightBolts = 5;                 // fire bolts in flight for the night shot
    constexpr float kIndoorBrightnessTolerance = 0.03f;   // indoors, night stays this close to noon

    if (t.step == 0)   // fresh profile -> Warrior -> village -> gate (the Forest)
    {
        if (t.subStep == 0)
        {
            if (_gameState != GameState::Menu || !FlowTravelSettled()) return;
            // Every wired library clip and biome theme decodes in the real build.
            const SfxId clips[] = { SfxId::FootGrassA, SfxId::FootGrassB, SfxId::FootStoneA, SfxId::FootStoneB,
                SfxId::FootDirtA, SfxId::FootDirtB, SfxId::CoinA, SfxId::CoinB, SfxId::ChestOpen, SfxId::DoorOpen,
                SfxId::DashA, SfxId::DashB, SfxId::ArmorLight, SfxId::ArmorHeavy, SfxId::RelicReveal,
                SfxId::RelicGained, SfxId::UpgradeBought, SfxId::ConsumableA, SfxId::ConsumableB,
                SfxId::GuiCancel, SfxId::HealA, SfxId::HealB, SfxId::ReviveDust, SfxId::BossArrival,
                SfxId::SpellSmite, SfxId::SpellFog, SfxId::SpellHexproof, SfxId::SpellIntoDarkness,
                SfxId::ShotFate, SfxId::StrikeDestiny };
            int loaded = 0;
            for (SfxId id : clips) loaded += SfxBank::Get().Has(id) ? 1 : 0;
            FlowCheck(loaded == (int)(sizeof(clips) / sizeof(clips[0])),
                      TextFormat("audio: %d of %d library clips load", loaded, (int)(sizeof(clips) / sizeof(clips[0]))));
            const char* themes[] = { "Forest_MystwoodReverie", "Jungle_FeysongFields", "LostCity_TombOfKings",
                "Wastelands_EnemyGrounds", "Graveyard_BlackDoves", "Sanctuary_Above", "Village_Denmark",
                "WorldMap_StormbornDestiny", "FinalBoss_ElderAwakening" };
            for (const char* theme : themes)
            {
                Music music = LoadMusicStream(AssetPath(TextFormat("Music/%s.ogg", theme)).c_str());
                FlowCheck(music.ctxData != nullptr && GetMusicTimeLength(music) > 60.f,
                          std::string("audio: ") + theme + " streams (" +
                          std::to_string((int)GetMusicTimeLength(music)) + " s)");
                if (music.ctxData != nullptr) UnloadMusicStream(music);
            }
            _clockHours = kTourNoon;
            _dayClockSpeed = 0.f;   // the clock holds still for the whole tour
            _prologueEntryMode = PrologueEntryMode::NewGame;
            _classSelectReturnState = GameState::Menu;
            _gameState = GameState::ClassSelect;
            FlowCheck(ConfirmClassSelection(PlayerClass::Warrior), "tour: Warrior selected");
            t.subStep = 1;
            return;
        }
        if (t.subStep == 1)
        {
            if (_gameState != GameState::Village || !FlowTravelSettled()) return;
            FinishVillageIntro();
            TriggerVillageDungeonGate();
            t.subStep = 2;
            return;
        }
        if (_gameState != GameState::DungeonRun || !FlowTravelSettled()) return;
        _player.SetInvulnerableLock(true);
        FlowNextStep();
        return;
    }

    const int index = t.step - 1;
    if (index >= kTourCount) { FinishFlowSelfTest(false); return; }
    const Biome biome = kTour[index];
    std::string name = GetBiomeName(biome);
    for (char& c : name) if (c == ' ') c = '_';
    const int bossIdx = _dungeonGen.GetBossIndex();

    // Forest only: an elite room under Arena Pressure. The volley's embers must
    // light on the walls (near an edge, where the player can see them) before
    // any shot flies, and the shots must be fire bolt sized. Screenshots of
    // both, then on to the boss.
    constexpr int kVolleyStep        = 4000;   // enter an elite room and settle
    constexpr int kVolleyEmbersStep  = 5000;   // armed: wait for the embers
    constexpr int kVolleyEmberShot   = 6000;   // let them grow, then a screenshot
    constexpr int kVolleyFlightStep  = 7000;   // wait for the shots
    constexpr int kVolleyFlightShot  = 8000;   // let them leave the wall, then a screenshot
    constexpr int kVolleyWaitFrames  = 120;    // most frames to wait for embers or shots
    constexpr int kVolleyGrowFrames  = 20;
    constexpr int kVolleyLeaveFrames = 12;
    if (t.subStep >= kVolleyStep)
    {
        const Vector2 roomCentre{ (float)kVirtualWidth * 0.5f, (float)kVirtualHeight * 0.55f };
        auto finish = [&]()
        {
            _eliteMechanic = -1;
            _volleyWindups.clear();
            _lavaBalls.clear();
            EnterDungeonRoom(bossIdx, DungeonDoorSide::None, roomCentre, false);
            t.subStep = 3 + kSettleFrames;
        };
        if (t.subStep == kVolleyStep)
        {
            int eliteIdx = -1;
            const auto& rooms = _dungeonGen.GetRooms();
            for (int i = 0; i < (int)rooms.size() && eliteIdx < 0; ++i)
                if (rooms[i].type == RoomType::Elite) eliteIdx = i;
            FlowCheck(eliteIdx >= 0, name + ": has an elite room for the volley check");
            if (eliteIdx < 0) { finish(); return; }
            EnterDungeonRoom(eliteIdx, DungeonDoorSide::None, roomCentre, false);
            ++t.subStep;
            return;
        }
        if (t.subStep < kVolleyStep + kSettleFrames) { ++t.subStep; return; }
        if (t.subStep < kVolleyEmbersStep)
        {
            _eliteEnrageWarningTimer = 0.f;   // past the ELITE ENCOUNTER banner's hold
            _eliteMechanic = 3;               // Arena Pressure, whatever the room rolled
            _eliteHazardSpawnTimer = 0.f;
            _volleyWindups.clear();
            _lavaBalls.clear();
            t.subStep = kVolleyEmbersStep;
            return;
        }
        if (t.subStep < kVolleyEmberShot)
        {
            if (_volleyWindups.empty())
            {
                if (++t.subStep - kVolleyEmbersStep > kVolleyWaitFrames)
                {
                    FlowCheck(false, name + ": Arena Pressure lights embers on the walls");
                    finish();
                }
                return;
            }
            const Rectangle floor = GetArenaFloorRect();
            const float nearWall = Balance::Hazards::kVolleyCornerInset +
                                   Balance::Hazards::kVolleyLaunchSlideStep * Balance::Hazards::kVolleyLaunchSlideMax;
            bool allNearWall = true, allVisible = true;
            for (const VolleyWindup& w : _volleyWindups)
            {
                const float edge = std::min({ w.from.x - floor.x, floor.x + floor.width - w.from.x,
                                              w.from.y - floor.y, floor.y + floor.height - w.from.y });
                allNearWall = allNearWall && edge <= nearWall;
                allVisible = allVisible && IsVolleyLaunchPointClear(w.from);
            }
            int flying = 0;
            for (const LavaBallProjectile& shot : _lavaBalls) flying += shot.IsActive() ? 1 : 0;
            FlowCheck(allNearWall, TextFormat("%s: Arena Pressure launches from the walls (%d embers)",
                                              name.c_str(), (int)_volleyWindups.size()));
            FlowCheck(allVisible, name + ": no volley ember hides under the HUD");
            FlowCheck(flying == 0, name + ": the embers glow before any volley shot flies");
            t.subStep = kVolleyEmberShot;
            return;
        }
        if (t.subStep < kVolleyFlightStep)
        {
            if (++t.subStep - kVolleyEmberShot == kVolleyGrowFrames)
            {
                FlowScreenshot("tour_" + std::to_string(index) + "_" + name + "_volley_embers");
                t.subStep = kVolleyFlightStep;
            }
            return;
        }
        if (t.subStep < kVolleyFlightShot)
        {
            int flying = 0;
            for (const LavaBallProjectile& shot : _lavaBalls) flying += shot.IsActive() ? 1 : 0;
            if (flying == 0)
            {
                if (++t.subStep - kVolleyFlightStep > kVolleyWaitFrames)
                {
                    FlowCheck(false, name + ": the volley embers launch their shots");
                    finish();
                }
                return;
            }
            t.subStep = kVolleyFlightShot;
            return;
        }
        if (++t.subStep - kVolleyFlightShot < kVolleyLeaveFrames) return;
        FlowScreenshot("tour_" + std::to_string(index) + "_" + name + "_volley_flight");
        bool fireBoltSized = true;
        int flying = 0;
        for (const LavaBallProjectile& shot : _lavaBalls)
        {
            if (!shot.IsActive()) continue;
            ++flying;
            fireBoltSized = fireBoltSized && shot.GetCollisionRec().width == Balance::Hazards::kVolleyHitSize;
        }
        FlowCheck(flying > 0 && fireBoltSized,
                  TextFormat("%s: volley shots are fire bolt sized (%d in flight, hitbox %.0f px)",
                             name.c_str(), flying, Balance::Hazards::kVolleyHitSize));
        finish();
        return;
    }

    // The entrance again at evening and at night, with fire bolts in flight so
    // the night shot shows the light they carry. Outdoor domains darken;
    // indoor ones keep their own lighting. Then back to noon and on to the boss.
    // Noon is measured once the room has settled (arrival fades finished), so
    // only the hour differs between the two measurements.
    constexpr int   kTimeOfDayStep     = 2000;   // settling at noon
    constexpr int   kLookStep          = 3000;   // evening, then night
    constexpr int   kSettleCheckFrames = 10;     // re-measure noon this often...
    constexpr float kSettledDelta      = 0.005f; // ...until it changes less than this
    constexpr int   kMaxSettleFrames   = 300;
    if (t.subStep >= kTimeOfDayStep && t.subStep < kLookStep)
    {
        const int frame = t.subStep - kTimeOfDayStep;
        if (frame % kSettleCheckFrames == 0)
        {
            const float now = FlowCanvasBrightness();
            const bool settled = frame > 0 && std::fabs(now - t.noonBrightness) < kSettledDelta;
            t.noonBrightness = now;
            if (settled || frame >= kMaxSettleFrames)
            {
                _clockHours = kTourEvening;
                t.subStep = kLookStep;
                return;
            }
        }
        ++t.subStep;
        return;
    }
    if (t.subStep >= kLookStep)
    {
        const int frame = t.subStep - kLookStep;
        if (frame == kFlowLookFrames)
        {
            FlowScreenshot("tour_" + std::to_string(index) + "_" + name + "_evening");
            _clockHours = kFlowNightHour;
            for (int i = 0; i < kNightBolts; ++i)
            {
                SpreadProjectile bolt;
                const Vector2 from{ kVirtualWidth * (i + 1.f) / (kNightBolts + 1.f), kVirtualHeight * 0.3f };
                bolt.InitBasic(from, Vector2{ 0.f, 1.f }, AbilityType::FireSpread, Vfx::kFireGlow);
                _spreadProjectiles.push_back(bolt);
            }
        }
        else if (frame == 2 * kFlowLookFrames)
        {
            FlowScreenshot("tour_" + std::to_string(index) + "_" + name + "_night");
            const float night = FlowCanvasBrightness();
            if (DayNight::IsOutdoorBiome(biome))
                FlowCheck(night < t.noonBrightness * kFlowMaxNightShare,
                          TextFormat("%s: night falls outdoors (brightness %.2f at noon, %.2f at night)",
                                     name.c_str(), t.noonBrightness, night));
            else
                FlowCheck(std::fabs(night - t.noonBrightness) < kIndoorBrightnessTolerance,
                          TextFormat("%s: indoors keeps its own lighting at night (brightness %.2f at noon, %.2f at night)",
                                     name.c_str(), t.noonBrightness, night));
            _spreadProjectiles.clear();
            _clockHours = kTourNoon;
            if (index == 0)
            {
                t.subStep = kVolleyStep;   // the Forest also shows an Arena Pressure elite room
                return;
            }
            EnterDungeonRoom(bossIdx, DungeonDoorSide::None,
                             Vector2{ (float)kVirtualWidth * 0.5f, (float)kVirtualHeight * 0.55f }, false);
            t.subStep = 3 + kSettleFrames;
            return;
        }
        ++t.subStep;
        return;
    }

    // The biome-specific room (tools/biome_content/biome_rooms.py), entered for
    // real on a graph room its doors fit: it builds, seals the doors the graph
    // does not use, and runs an encounter.
    constexpr int kBiomeRoomStep = 1000;
    if (t.subStep >= kBiomeRoomStep)
    {
        if (t.subStep < kBiomeRoomStep + kSettleFrames) { ++t.subStep; return; }
        const DungeonRoom& room = _dungeonGen.GetRooms()[(size_t)_dungeonRoomIdx];
        const std::string id = _handcraftedDungeonRoomIds[(size_t)_dungeonRoomIdx];
        FlowCheck(_dungeonRoomLayout.handcrafted && _dungeonRoomLayout.sourceRoomId == id,
                  name + ": biome room " + id + " builds in the game");
        FlowCheck(_dungeonRoomLayout.doorZones[(int)RoomWallSide::Top].enabled == room.hasNorth &&
                  _dungeonRoomLayout.doorZones[(int)RoomWallSide::Bottom].enabled == room.hasSouth &&
                  _dungeonRoomLayout.doorZones[(int)RoomWallSide::Left].enabled == room.hasWest &&
                  _dungeonRoomLayout.doorZones[(int)RoomWallSide::Right].enabled == room.hasEast,
                  name + ": biome room opens only the graph's doors");
        int active = 0, falling = 0;
        for (auto& e : _enemies)
            if (e && e->IsActive() && e->IsAlive()) { ++active; falling += e->IsPitFalling() ? 1 : 0; }
        FlowCheck(active + (int)_pendingEnemySpawns.size() > 0 && falling == 0,
                  TextFormat("%s: biome room encounter spawns (%d enemies, %d pending, %d in a pit)", name.c_str(),
                             active, (int)_pendingEnemySpawns.size(), falling));
        FlowScreenshot("tour_" + std::to_string(index) + "_" + name + "_biome_room");
        FlowKillAllEnemies();
        _dungeonReinforcements.clear();
        FlowNextStep();
        return;
    }

    if (t.subStep == 0)
    {
        if (biome != Biome::Forest) CommitWorldMapChoice(biome, 0);   // the real domain-entry path
        t.subStep = 1;
        return;
    }
    if (t.subStep == 1)
    {
        if (_gameState != GameState::DungeonRun || !FlowTravelSettled()) return;
        FlowCheck(_currentBiome == biome, name + ": entered");
        FlowCheck(_useHandcraftedDungeonRooms && _dungeonRoomLayout.handcrafted, name + ": entrance room is handcrafted");
        if (biome != Biome::Forest)
            FlowCheck(_dungeonGen.GetRooms()[_dungeonGen.GetStartIndex()].type == RoomType::Store,
                      name + ": opens at Zeph's camp");
        bool allAuthored = (int)_handcraftedDungeonRoomIds.size() == (int)_dungeonGen.GetRooms().size();
        for (const std::string& id : _handcraftedDungeonRoomIds) allAuthored = allAuthored && _roomLibrary.FindById(id) != nullptr;
        FlowCheck(allAuthored, name + ": every room of the domain is an authored room");
        {
            const DungeonGen::Plan plan = DungeonGen::PlanForDomain(_worldZone);
            int elites = 0;
            for (const DungeonRoom& r : _dungeonGen.GetRooms()) elites += r.type == RoomType::Elite;
            FlowCheck((int)_dungeonGen.GetRooms().size() == plan.rooms && elites == plan.elites,
                      TextFormat("%s: domain %d is sized for its depth (%d rooms, %d elites)", name.c_str(),
                                 _worldZone + 1, (int)_dungeonGen.GetRooms().size(), elites));
        }
        t.subStep = 2;
        return;
    }
    if (t.subStep < 2 + kSettleFrames) { ++t.subStep; return; }
    if (t.subStep == 2 + kSettleFrames)
    {
        FlowScreenshot("tour_" + std::to_string(index) + "_" + name + "_entrance");
        t.subStep = kTimeOfDayStep;   // noon settled, evening, night, then the boss room
        return;
    }
    if (t.subStep < 3 + 2 * kSettleFrames) { ++t.subStep; return; }
    const int bossShot = 3 + 2 * kSettleFrames;
    if (t.subStep == bossShot)
    {
        FlowCheck(_dungeonRoomIdx == bossIdx && _dungeonRoomLayout.handcrafted, name + ": boss room is handcrafted");
        if (biome == Biome::DemonsInsides || biome == Biome::Caverns)
        {
            bool finalMolarbeast = false, abyssSlime = false, anyMolarbeast = false;
            for (auto& e : _enemies)
            {
                if (!e || !e->IsActive() || !e->IsBoss()) continue;
                if (Molarbeast* m = e->AsMolarbeast()) { anyMolarbeast = true; finalMolarbeast = finalMolarbeast || m->IsFinalForm(); }
                if (e->AsAbyssSlime()) abyssSlime = true;
            }
            if (biome == Biome::DemonsInsides)
                FlowCheck(finalMolarbeast && !abyssSlime, name + ": the final boss is the Molarbeast, in its final form");
            else
                FlowCheck(abyssSlime && !anyMolarbeast, name + ": the Caverns boss is now the Abyss Slime");
        }
        FlowScreenshot("tour_" + std::to_string(index) + "_" + name + "_boss");
        if (index != 0)
        {
            FlowKillAllEnemies();
            _dungeonReinforcements.clear();
            const RoomBlueprint* biomeRoom = nullptr;
            for (const RoomBlueprint& r : _roomLibrary.Rooms())
                if (r.biome == biome && r.id.rfind("biome_", 0) == 0) { biomeRoom = &r; break; }
            if (biomeRoom == nullptr) { FlowNextStep(); return; }
            // A Standard combat room whose doors the biome room fits; re-roll the
            // domain graph (the real generator) until one has such a room.
            auto fits = [&](int i)
            {
                const DungeonRoom& r = _dungeonGen.GetRooms()[(size_t)i];
                const unsigned char mask = RoomDoorMask(r.hasNorth, r.hasSouth, r.hasEast, r.hasWest);
                return r.type == RoomType::Standard && !r.startsEmpty && i != _dungeonGen.GetStartIndex() &&
                       i != _dungeonGen.GetBossIndex() && (mask == biomeRoom->DoorMask() || biomeRoom->DoorMask() == 15);
            };
            int slot = -1;
            for (int attempt = 0; attempt < 200 && slot < 0; ++attempt)
            {
                if (attempt > 0 && !GenerateHandcraftedDungeon(_currentBiome, _handcraftedDungeonRoomIds, 600, true))
                    continue;
                for (int i = 0; i < (int)_dungeonGen.GetRooms().size() && slot < 0; ++i)
                    if (fits(i)) slot = i;
            }
            FlowCheck(slot >= 0, name + ": a domain room fits biome room " + biomeRoom->id);
            if (slot < 0) { FlowNextStep(); return; }
            _handcraftedDungeonRoomIds[(size_t)slot] = biomeRoom->id;
            _dungeonRoomStates.clear();   // a plain combat room: no decision-room tag
            EnterDungeonRoom(slot, DungeonDoorSide::None,
                             Vector2{ (float)kVirtualWidth * 0.5f, (float)kVirtualHeight * 0.55f }, false);
            t.subStep = kBiomeRoomStep;
            return;
        }
        // Forest only: kill the adds through the real hit-feedback path (the
        // same RegisterHitFx a player hit uses) so the death payoff renders.
        int payoffKills = 0;
        for (auto& e : _enemies)
        {
            if (!e || !e->IsActive() || !e->IsAlive() || e->IsBoss()) continue;
            const float before = e->GetHealthValue();
            e->BaseCharacter::TakeDamage(INT_MAX / 2, _player.GetWorldPos());
            RegisterHitFx(*e, before, payoffKills % 2 == 1, false, false, 0);
            ++payoffKills;
        }
        FlowCheck(payoffKills > 0, TextFormat("death payoff: %d adds killed through the hit-feedback path", payoffKills));
        t.subStep = bossShot + 1;
        return;
    }
    if (t.subStep < bossShot + 8) { ++t.subStep; return; }
    if (t.subStep == bossShot + 8) { FlowScreenshot("death_payoff_1_mid"); ++t.subStep; return; }
    if (t.subStep < bossShot + 50) { ++t.subStep; return; }
    FlowScreenshot("death_payoff_2_after");
    FlowKillAllEnemies();
    _dungeonReinforcements.clear();
    FlowNextStep();
}

namespace
{
    // A character capture as a plain document, and its fields as
    // "name -> JSON text" for diffing.
    JsonValue CharacterDocument(const CharacterCheckpoint& state)
    {
        RunCheckpoint holder;
        holder.runId = 1;
        holder.active = true;
        holder.character = state;
        return *RunCheckpointToJson(holder).Find("character");
    }

    std::map<std::string, std::string> CaptureFields(const JsonValue& character)
    {
        std::map<std::string, std::string> fields;
        std::function<void(const JsonValue&, const std::string&)> walk = [&](const JsonValue& value, const std::string& path)
        {
            if (value.IsObject())
                for (std::size_t i = 0; i < value.Size(); ++i) walk(value.At(i), path + "." + value.KeyAt(i));
            else if (value.IsArray())
                for (std::size_t i = 0; i < value.Size(); ++i) walk(value.At(i), path + "." + std::to_string(i));
            else
                WriteJson(value, fields[path]);
        };
        walk(character, "character");
        return fields;
    }
}

void Engine::UpdateFlowSelfTestPhase4()
{
    FlowSelfTest& t = _flowTest;
    switch (t.step)
    {
    case 0:   // fresh profile -> Hunter -> village -> gate
        if (t.subStep == 0)
        {
            if (_gameState != GameState::Menu || !FlowTravelSettled()) return;
            FlowCheck(!HasResumableRun(), "resume: no suspended run on a fresh profile");
            _prologueEntryMode = PrologueEntryMode::NewGame;
            _classSelectReturnState = GameState::Menu;
            _gameState = GameState::ClassSelect;
            FlowCheck(ConfirmClassSelection(PlayerClass::Hunter), "resume: Hunter selected");
            t.subStep = 1;
            return;
        }
        if (t.subStep == 1)
        {
            if (_gameState != GameState::Village || !FlowTravelSettled()) return;
            FinishVillageIntro();
            TriggerVillageDungeonGate();
            t.subStep = 2;
            return;
        }
        if (_gameState != GameState::DungeonRun || !FlowTravelSettled()) return;
        FlowCheck(HasResumableRun(), "resume: entering the first room writes a checkpoint");
        FlowNextStep();
        return;

    case 1:   // build up a distinctive run state, then clear a combat room
    {
        if (t.subStep == 0)
        {
            GrantRelic(RollRandomRelic());
            GrantRelic(RollRandomRelic());
            _player.AddGold(137);
            _player.AddCellsFromDrop(9);
            _player.BaseCharacter::TakeDamage(20, _player.GetWorldPos());
            _secondWindAvailable = true;
            const int start = _dungeonGen.GetStartIndex();
            int neighbour = -1;
            for (int d = 0; d < 4 && neighbour < 0; ++d)
                neighbour = _dungeonGen.GetNeighborIndex(start, d == 0 ? -1 : d == 1 ? 1 : 0, d == 2 ? -1 : d == 3 ? 1 : 0);
            FlowCheck(neighbour >= 0, "resume: the entrance has a neighbouring room");
            if (neighbour < 0) { FinishFlowSelfTest(true); return; }
            _player.SetInvulnerableLock(true);
            EnterDungeonRoom(neighbour, DungeonDoorSide::None,
                             Vector2{ (float)kVirtualWidth * 0.5f, (float)kVirtualHeight * 0.55f }, false);
            t.subStep = 1;
            return;
        }
        if (_gameState == GameState::RelicChoice) { ConfirmRelicChoice(0); return; }
        if (_gameState != GameState::DungeonRun) return;
        if (!_dungeonRoomStates[_dungeonRoomIdx].cleared)
        {
            FlowKillAllEnemies();
            _dungeonReinforcements.clear();
            return;
        }
        if (++t.subStep < 30) return;   // let the clear resolve (doors, rewards, checkpoint)
        _player.SetInvulnerableLock(false);
        _player.RestoreMana(_player.GetMaxMana());   // full mana: regen can't drift while settling

        // Expectations for the relaunch.
        CharacterCheckpoint capture;
        _player.CaptureRunState(capture);
        std::string captureText;
        WriteJson(CharacterDocument(capture), captureText);
        std::ofstream(UserDataPath("flowtest_resume_char.txt"), std::ios::binary) << captureText;
        std::ofstream(UserDataPath("flowtest_resume_expect.txt"))
            << "room=" << _dungeonRoomIdx << '\n'
            << "biome=" << (int)_currentBiome << '\n'
            << "zone=" << _worldZone << '\n'
            << "rooms=" << _dungeonGen.GetRooms().size() << '\n'
            << "cleared_count=" << std::count_if(_dungeonRoomStates.begin(), _dungeonRoomStates.end(),
                   [](const auto& kv) { return kv.second.cleared; }) << '\n'
            << "layout=" << (_dungeonRoomIdx < (int)_handcraftedDungeonRoomIds.size()
                   ? _handcraftedDungeonRoomIds[_dungeonRoomIdx] : std::string()) << '\n'
            << "layouts=" << FlowJoinLayouts(_handcraftedDungeonRoomIds) << '\n';
        FlowCheck(IsRunSaveable(), "resume: a cleared room is a saveable place");
        FlowNextStep();
        return;
    }

    case 2:   // pause -> Save & Main Menu (not a death)
    {
        const int banked = _meta.GetBankedCells();
        _stateBeforePause = GameState::DungeonRun;
        _gameState = GameState::Pause;

        // A checkpoint write that fails keeps everything: the run, the pause
        // menu, the file on disk. Quit doesn't close the game either.
        const std::string onDisk = ReadFileBytes(SaveFiles::RunCheckpointPath());
        const float hpBefore = _player.GetHealthValue();
        const int goldBefore = _player.GetGold();
        SaveFile::SetFailWritesForTesting(true);
        ApplyPauseAction(5);   // Save & Main Menu
        FlowCheck(_gameState == GameState::Pause && IsRunSaveable() && _runSaveFailedTimer > 0.f,
                  "save failure: Save & Main Menu keeps the run and says so");
        FlowCheck(_player.GetHealthValue() == hpBefore && _player.GetGold() == goldBefore && _runSessionData.IsActive(),
                  "save failure: nothing in memory is lost");
        ApplyPauseAction(3);   // Quit
        FlowCheck(!_shouldClose && _gameState == GameState::Pause, "save failure: Quit keeps the game open");
        SaveFile::SetFailWritesForTesting(false);
        t.injectedSaveFailures = 2;
        FlowCheck(ReadFileBytes(SaveFiles::RunCheckpointPath()) == onDisk, "save failure: the checkpoint on disk is untouched");
        _runSaveFailedTimer = 0.f;

        SaveAndQuitRunToMenu();
        FlowCheck(_gameState == GameState::Menu, "resume: Save & Main Menu returns to the menu");
        FlowCheck(HasResumableRun(), "resume: the suspended run is on disk");
        FlowCheck(_meta.GetBankedCells() == banked, "resume: Save & Quit applies no death penalty");
        FinishFlowSelfTest(false);
        return;
    }
    }
}

void Engine::UpdateFlowSelfTestPhase5()
{
    FlowSelfTest& t = _flowTest;
    switch (t.step)
    {
    case 0:
        if (_gameState != GameState::Menu || !FlowTravelSettled()) return;
        if (++t.subStep < 15) return;   // let the menu draw its caption
        FlowCheck(HasResumableRun(), "resume: relaunch finds the suspended run");
        {
            // An edited checkpoint is set aside and its backup (an earlier
            // checkpoint of the same run) is used. Then put the original back.
            const std::string path = SaveFiles::RunCheckpointPath();
            const std::string good = ReadFileBytes(path), goodBackup = ReadFileBytes(path + ".bak");
            std::string edited = good;
            edited[edited.size() / 2] ^= 0x20;
            WriteFileBytes(path, edited);
            RunCheckpoint fromBackup;
            FlowCheck(!goodBackup.empty() && ReadSuspendedRun(fromBackup) && fromBackup.runId == _meta.GetActiveRunId() &&
                      ReadFileBytes(path + ".rejected") == edited && ReadFileBytes(path) == goodBackup,
                      "tamper: an edited checkpoint is rejected and the backup restored");
            WriteFileBytes(path, good);
        }
        FlowCheck(_menuRunCaption.find("Hunter") != std::string::npos,
                  "resume: the menu names the suspended run (" + _menuRunCaption + ")");
        FlowScreenshot("resume_menu");
        FlowCheck(ResumeSavedRun(), "resume: Continue starts the resume");
        FlowNextStep();
        return;

    case 1:
    {
        if (_gameState != GameState::DungeonRun || !FlowTravelSettled()) return;
        std::map<std::string, std::string> kv = ReadKeyValues(UserDataPath("flowtest_resume_expect.txt"));
        FlowCheck(std::to_string(_dungeonRoomIdx) == kv["room"], "resume: back in the same room");
        FlowCheck(std::to_string((int)_currentBiome) == kv["biome"] && std::to_string(_worldZone) == kv["zone"],
                  "resume: same biome and domain");
        FlowCheck(std::to_string(_dungeonGen.GetRooms().size()) == kv["rooms"], "resume: same dungeon graph");
        FlowCheck(_dungeonRoomIdx < (int)_handcraftedDungeonRoomIds.size() &&
                  _handcraftedDungeonRoomIds[_dungeonRoomIdx] == kv["layout"], "resume: same authored room layout");
        FlowCheck(!kv["layouts"].empty() && FlowJoinLayouts(_handcraftedDungeonRoomIds) == kv["layouts"],
                  "resume: every room keeps its assigned layout (none rerolled)");
        const long long cleared = std::count_if(_dungeonRoomStates.begin(), _dungeonRoomStates.end(),
                                                [](const auto& e) { return e.second.cleared; });
        FlowCheck(std::to_string(cleared) == kv["cleared_count"], "resume: cleared rooms stay cleared");
        FlowCheck(_isMainGameRun && _runSessionData.IsActive(), "resume: it is a live main run again");

        // The character must match field for field.
        JsonValue before;
        std::string parseError;
        FlowCheck(ParseJson(ReadFileBytes(UserDataPath("flowtest_resume_char.txt")), before, parseError),
                  "resume: expected character capture readable");
        CharacterCheckpoint now;
        _player.CaptureRunState(now);
        const auto expected = CaptureFields(before), actual = CaptureFields(CharacterDocument(now));
        int mismatches = 0;
        for (const auto& [key, value] : expected)
        {
            auto it = actual.find(key);
            if (it == actual.end() || it->second != value)
            {
                ++mismatches;
                FlowLog("diff  " + key + ": saved " + value + " / resumed " + (it == actual.end() ? "(missing)" : it->second));
            }
        }
        FlowCheck(mismatches == 0 && expected.size() == actual.size(),
                  TextFormat("resume: character identical (%d fields compared, %d differ)", (int)expected.size(), mismatches));
        FlowCheck(_player.GetRelicCount() >= 2, "resume: relics restored");
        FlowScreenshot("resume_back_in_room");

        // Keep the last live checkpoint's bytes for the anti-revive check.
        SaveRunCheckpoint();
        t.savedCheckpoint = ReadFileBytes(SaveFiles::RunCheckpointPath());
        _player.SetInvulnerableLock(false);
        _player.ClearLastDamageSource();
        t.hpBeforeExit = _player.GetHealthValue();
        FlowNextStep();
        return;
    }

    case 2:   // a shot that lands is credited to its shooter (run log cause of death)
    {
        if (_player.GetHealthValue() < t.hpBeforeExit)
        {
            const DamageSource& hit = _player.GetLastDamageSource();
            FlowCheck(hit.kind == DamageSourceKind::Enemy && hit.id == "SkeletonArcher",
                      "cause: an enemy's shot is credited to the shooter (" + hit.id + ")");
            // Dying ends the suspended run for good.
            _secondWindAvailable = false;
            _player.BaseCharacter::TakeDamage(99999, _player.GetWorldPos());
            FlowNextStep();
            return;
        }
        // Room-entry protection, armour or a ward may stop a shot; keep firing
        // until one takes HP.
        if (_player.HasActiveIFrames()) return;
        constexpr float kHalf = 0.5f;
        const Rectangle body = _player.GetCollisionRec();
        EnemyProjectile shot;
        shot.Init(Vector2{ body.x + body.width * kHalf, body.y + body.height * kHalf }, Vector2{ 1.f, 0.f },
                  EnemyProjectileKind::Arrow, kFlowProbeDamage);
        shot.SetSource(DamageSource::FromEnemy("SkeletonArcher", false, false));
        _enemyProjectiles.push_back(shot);
        UpdateEnemyProjectiles(0.f);
        return;
    }

    case 3:
        if (!_playerDying && _gameState != GameState::DeathRevive) return;
        FlowCheck(!HasResumableRun(), "resume: death ends the suspended run (Continue can't undo it)");
        // Putting the last live checkpoint back (as a restored backup would)
        // still can't revive it: the profile records that the run ended.
        WriteFileBytes(SaveFiles::RunCheckpointPath(), t.savedCheckpoint);
        {
            JsonValue revived;
            std::string why;
            const bool live = !t.savedCheckpoint.empty() &&
                SaveFile::ReadDocument(SaveFiles::RunCheckpointPath(), SaveKind::RunCheckpoint,
                                       SaveFiles::kMaxRunCheckpointBytes, revived, why) &&
                PathIs(revived, "active", "true");
            FlowCheck(live && !HasResumableRun() && _meta.GetActiveRunId() == 0,
                      "anti-revive: a live checkpoint of an ended run can't be continued");
        }
        FinishFlowSelfTest(false);
        return;
    }
}

// Phase 7, on demand (not part of the gate): combat stress. An invulnerable
// Warrior who never attacks stands in a combat room while mixed groups of 10,
// 25 and 50 enemies (melee, archers, casters, a cyclops) fight for a fixed
// time each. The FRAMETIME summary written on exit buckets the frames by live
// enemy count, so each group gets its own update/draw/interval numbers.
void Engine::UpdateFlowSelfTestPhase7()
{
    FlowSelfTest& t = _flowTest;
    constexpr int   kStressGroups[]        = { 10, 25, 50 };
    constexpr int   kStressGroupCount      = (int)(sizeof(kStressGroups) / sizeof(kStressGroups[0]));
    constexpr float kStressSecondsPerGroup = 12.f;
    constexpr float kStressRingRadius      = 300.f;   // around the room centre, which every room keeps open
    constexpr int   kStressRingJitter      = 90;      // +/- px, so enemies don't stand in one perfect circle

    if (t.step == 0)   // fresh profile -> Warrior -> village -> gate -> first room
    {
        if (t.subStep == 0)
        {
            if (_gameState != GameState::Menu || !FlowTravelSettled()) return;
            _prologueEntryMode = PrologueEntryMode::NewGame;
            _classSelectReturnState = GameState::Menu;
            _gameState = GameState::ClassSelect;
            FlowCheck(ConfirmClassSelection(PlayerClass::Warrior), "stress: Warrior selected");
            t.subStep = 1;
            return;
        }
        if (t.subStep == 1)
        {
            if (_gameState != GameState::Village || !FlowTravelSettled()) return;
            FinishVillageIntro();
            TriggerVillageDungeonGate();
            t.subStep = 2;
            return;
        }
        if (_gameState != GameState::DungeonRun || !FlowTravelSettled()) return;
        _player.SetInvulnerableLock(true);
        FlowNextStep();
        return;
    }

    const int group = t.step - 1;   // one step per group
    if (group >= kStressGroupCount) { FinishFlowSelfTest(false); return; }
    if (_gameState == GameState::RelicChoice) { ConfirmRelicChoice(0); return; }
    if (_gameState != GameState::DungeonRun || !FlowTravelSettled()) return;

    if (t.subStep == 0)
    {
        FlowKillAllEnemies();
        _dungeonReinforcements.clear();
        _pendingEnemySpawns.clear();
        using SpawnFn = Enemy* (Engine::*)(Vector2);
        const SpawnFn mix[] = { &Engine::SpawnBasicEnemy, &Engine::SpawnSkeletonArcher, &Engine::SpawnFlameWisp,
                                &Engine::SpawnShieldbearer, &Engine::SpawnLivingBlade, &Engine::SpawnSporeling,
                                &Engine::SpawnPhantom, &Engine::SpawnWarchief, &Engine::SpawnCyclops };
        constexpr int kMixCount = (int)(sizeof(mix) / sizeof(mix[0]));
        const Vector2 centre{ kVirtualWidth * 0.5f, kVirtualHeight * 0.5f };
        const int count = kStressGroups[group];
        for (int i = 0; i < count; ++i)
        {
            const float angle  = 2.f * PI * (float)i / (float)count;
            const float radius = kStressRingRadius + (float)GetRandomValue(-kStressRingJitter, kStressRingJitter);
            (this->*mix[i % kMixCount])(Vector2{ centre.x + cosf(angle) * radius, centre.y + sinf(angle) * radius });
        }
        FlowCheck(GetActiveEnemyCount() >= count, TextFormat("stress: %d enemies spawned (%d active)", count, GetActiveEnemyCount()));
        t.peakEnemyProjectiles = 0;
        t.stepTime = 0.f;
        t.subStep = 1;
        return;
    }

    t.peakEnemyProjectiles = std::max(t.peakEnemyProjectiles, (int)_enemyProjectiles.size());
    if (t.subStep == 1 && t.stepTime >= kStressSecondsPerGroup * 0.5f)
    {
        FlowScreenshot(TextFormat("stress_%d_enemies", kStressGroups[group]));   // evidence the fight is live
        t.subStep = 2;
    }
    if (t.stepTime < kStressSecondsPerGroup) return;
    FlowLog(TextFormat("STRESS group %d: %d enemies spawned, %d still active after %.0f s, peak %d enemy projectiles",
                       group + 1, kStressGroups[group], GetActiveEnemyCount(), kStressSecondsPerGroup, t.peakEnemyProjectiles));
    FlowNextStep();
}

void Engine::UpdateFlowSelfTestPhase6()
{
    FlowSelfTest& t = _flowTest;
    if (t.step == 0 && t.subStep == 0 && _gameState == GameState::Menu && FlowTravelSettled())
    {
        // Rendering twice without an update must not navigate or advance UI state.
        _gameState = GameState::Keybindings;
        BeginTextureMode(_virtualCanvas);
        Draw();
        Draw();
        EndTextureMode();
        FlowCheck(_gameState == GameState::Keybindings, "render: keybindings draw does not navigate");
        _htpSlideOffset = 100.f;
        BeginTextureMode(_virtualCanvas);
        DrawHowToPlay();
        DrawHowToPlay();
        EndTextureMode();
        FlowCheck(_htpSlideOffset == 100.f, "render: help animation advances only in Update");
        UpdateHowToPlayMouse(0.02f);
        FlowCheck(_htpSlideOffset < 100.f, "update: help animation advances");
        _htpSlideOffset = 0.f;

        // HUD counters and toasts advance only in UpdateHudPresentation.
        constexpr float kHudToastProbe = 2.f;
        constexpr float kHudGoldOffset = 50.f;
        constexpr float kHudProbeDt    = 0.1f;   // long enough for the gold counter to land
        const float goldNow = (float)_player.GetGold();
        _relicToastTimer = kHudToastProbe;
        _displayGold = goldNow + kHudGoldOffset;
        BeginTextureMode(_virtualCanvas);
        DrawHUD();
        DrawHUD();
        EndTextureMode();
        FlowCheck(_relicToastTimer == kHudToastProbe && _displayGold == goldNow + kHudGoldOffset,
                  "render: repeated HUD draw leaves toasts and counters alone");
        UpdateHudPresentation(kHudProbeDt);
        FlowCheck(_relicToastTimer < kHudToastProbe && _displayGold == goldNow,
                  "update: HUD toasts and counters advance");
        _relicToastTimer = 0.f;

        // Touch layout screen: drawing never moves buttons or navigates; its
        // Default/Back actions apply from Update.
        constexpr float kTouchProbeShift = 40.f;
        EnterTouchButtonMapping();
        _gameState = GameState::TouchButtonMapping;
        const float defaultAtkX = _touchMappingPos[0].x;
        _touchMappingPos[0].x -= kTouchProbeShift;   // as if dragged
        BeginTextureMode(_virtualCanvas);
        Draw();
        Draw();
        EndTextureMode();
        FlowCheck(_gameState == GameState::TouchButtonMapping && _touchMappingPos[0].x == defaultAtkX - kTouchProbeShift,
                  "render: touch layout draw neither moves buttons nor navigates");
        ApplyTouchButtonMappingAction(3);   // Default
        FlowCheck(_touchMappingPos[0].x == defaultAtkX && _gameState == GameState::TouchButtonMapping,
                  "update: touch layout Default restores the captured layout");
        ApplyTouchButtonMappingAction(2);   // Back
        FlowCheck(_gameState == GameState::Pause, "update: touch layout Back returns to the pause menu");

        // A pending controller confirm must never be consumed by rendering.
        _gamepad.isActive = true;
        _gamepad.menuConfirmPressed = true;
        _levelUpOpenTimer = 0.f;
        _awaitingStartingAbility = false;
        _showUltimateRow = false;
        _regularRowPicked = false;
        _levelUpGpRow = 1;
        _levelUpGpCursor = 0;
        _levelUpOptions[0] = UpgradeType::AttackPower;
        _levelUpReturnState = GameState::Menu;
        _gameState = GameState::LevelUpChoice;
        BeginTextureMode(_virtualCanvas);
        DrawLevelUpChoice();
        DrawLevelUpChoice();
        EndTextureMode();
        FlowCheck(!_regularRowPicked && _gameState == GameState::LevelUpChoice,
                  "render: repeated level-up draw does not select a reward");
        UpdateLevelUpChoiceInput();
        FlowCheck(_regularRowPicked && _gameState == GameState::Menu,
                  "update: controller confirm selects the level-up reward");

        _abilityChoiceOpenTimer = 0.f;
        _abilityChoiceOptionCount = 1;
        _abilityChoiceOptions[0] = UpgradeType::AttackPower;
        _abilityChoiceGpCursor = 0;
        _abilityChoiceSwapPending = false;
        _pendingRoomChoice = false;
        _gameState = GameState::AbilityChoice;
        BeginTextureMode(_virtualCanvas);
        DrawAbilityChoice();
        DrawAbilityChoice();
        EndTextureMode();
        FlowCheck(_gameState == GameState::AbilityChoice, "render: ability choice does not consume confirm");
        UpdateAbilityChoiceInput();
        FlowCheck(_gameState == GameState::Play, "update: ability choice preserves its existing return path");

        // Starter pick on a controller: the first A only picks the card, and a
        // second A on the picked card confirms it (the mouse has its own button).
        _awaitingStartingAbility   = true;
        _starterAbilityGiftClaimed = false;
        _startingAbilityPickCount  = 0;
        for (int i = 0; i < 3; ++i)
        {
            _startingAbilitySelected[i] = false;
            _startingAbilityOptions[i]  = UpgradeType::AttackPower;
        }
        _levelUpGpCursor    = 0;
        _levelUpOpenTimer   = 0.f;
        _levelUpReturnState = GameState::Menu;
        _gameState          = GameState::LevelUpChoice;
        UpdateLevelUpChoiceInput();
        FlowCheck(_startingAbilitySelected[0] && _awaitingStartingAbility && _gameState == GameState::LevelUpChoice,
                  "update: the first controller press only picks the starter");
        UpdateLevelUpChoiceInput();
        FlowCheck(!_awaitingStartingAbility && _gameState == GameState::Menu,
                  "update: a second press on the picked starter confirms it");
        _starterAbilityGiftClaimed = false;

        // Overlapping lifesteal: a short strong grant keeps its own length, and
        // a longer weak grant never overwrites a strike-heal.
        {
            constexpr float kLingerFraction = 0.15f, kLingerSeconds = 7.f;
            constexpr float kStrikeFraction = 0.6f,  kStrikeSeconds = 0.3f;
            const float potency = _player.GetLifestealPotency();
            _player.GrantLifesteal(kLingerFraction, kLingerSeconds);
            _player.GrantLifesteal(kStrikeFraction, kStrikeSeconds);
            FlowCheck(_player.GetLifestealTimeLeft() <= kStrikeSeconds,
                      TextFormat("lifesteal: a strike-heal does not inherit a longer timer (%.1f s)", _player.GetLifestealTimeLeft()));
            _player.GrantLifesteal(kLingerFraction, kLingerSeconds);
            FlowCheck(fabsf(_player.GetLifestealFraction() - std::min(0.9f, kStrikeFraction * potency)) < 1e-4f,
                      "lifesteal: a weaker grant does not cut a strike-heal short");
        }

        // Reach cards lengthen the swing, including the authored swing box.
        {
            const float before = _player.GetSideAttackRec(1.f).width;
            _player.ApplyUpgrade(UpgradeType::AttackRange);
            const float after = _player.GetSideAttackRec(1.f).width;
            FlowCheck(after > before, TextFormat("reach: a range card lengthens the melee swing (%.0f -> %.0f px)", before, after));
        }

        // The room intro banner counts down per presented frame, never in Draw.
        _roomIntroTimer = kRoomIntroDuration;
        _gameState = GameState::DungeonRun;
        BeginTextureMode(_virtualCanvas);
        DrawRoomIntroBanner();
        DrawRoomIntroBanner();
        EndTextureMode();
        FlowCheck(_roomIntroTimer == kRoomIntroDuration, "render: the room intro banner does not count down while drawing");
        UpdateHudPresentation(kHudProbeDt);
        FlowCheck(_roomIntroTimer < kRoomIntroDuration, "update: the room intro banner counts down per presented frame");
        _roomIntroTimer = 0.f;

        // The HUD panel fades while the hero is under it, and only in Update.
        {
            constexpr int kCoverProbeFrames = 10;   // x kHudProbeDt: long enough for either fade to settle
            const Vector2 heroWas = _player.GetWorldPos();
            const Vector2 cameraWas = _cameraPos;
            _cameraPos = Vector2{ kVirtualWidth * 0.5f, kVirtualHeight * 0.5f };   // a dungeon room: world = screen
            const Rectangle panel = GetHudPanelLayout().panel;
            _player.SetWorldPos(Vector2{ panel.x + panel.width * 0.5f, panel.y + panel.height * 0.5f });
            _hudCoverAlpha = 1.f;
            BeginTextureMode(_virtualCanvas);
            DrawHUD();
            EndTextureMode();
            FlowCheck(_hudCoverAlpha == 1.f, "render: drawing the HUD never fades it");
            for (int i = 0; i < kCoverProbeFrames; ++i) UpdateHudPresentation(kHudProbeDt);
            const float covered = _hudCoverAlpha;
            _player.SetWorldPos(Vector2{ kVirtualWidth * 0.5f, kVirtualHeight * 0.5f });
            for (int i = 0; i < kCoverProbeFrames; ++i) UpdateHudPresentation(kHudProbeDt);
            FlowCheck(covered < 1.f && _hudCoverAlpha == 1.f,
                      TextFormat("hud: the panel fades while the hero is under it (%.2f) and returns after (%.2f)",
                                 covered, _hudCoverAlpha));
            _player.SetWorldPos(heroWas);
            _cameraPos = cameraWas;
        }

        // Shieldbearer guard: a front hit is blocked and fills the meter; a shot
        // arriving from behind and poison get through; a swing drops the shield;
        // a full meter breaks the guard and that hit lands.
        {
            constexpr float kProbeOffset        = 200.f;   // px from the shieldbearer
            constexpr int   kProbeHit           = 1;
            constexpr float kPoisonSeconds      = 2.f;
            constexpr float kStatusProbeSeconds = 1.f;     // long enough for one poison tick
            constexpr int   kGuardChunks        = 4;       // the meter fills over this many hits
            constexpr int   kMaxGuardHits       = 8;
            Shieldbearer sb(Vector2{});
            sb.Init();
            sb.SetEditorFacing(1.f);   // facing right
            const Vector2 at = sb.GetWorldPos();
            const Vector2 front{ at.x + kProbeOffset, at.y };
            const Vector2 back{ at.x - kProbeOffset, at.y };
            const float full = sb.GetHealthValue();

            sb.TakeDamage(kProbeHit, front);
            FlowCheck(sb.ConsumeHitBlock() == Enemy::HitBlockReason::Blocked && sb.GetHealthValue() == full &&
                      sb.GetGuardFraction() > 0.f,
                      "shield: a front hit is blocked and fills the guard meter");
            sb.TakeDamageArriving(kProbeHit, front, back);
            float hp = sb.GetHealthValue();
            FlowCheck(sb.ConsumeHitBlock() == Enemy::HitBlockReason::None && hp < full,
                      "shield: a shot arriving from behind lands though the hero stands in front");
            sb.ApplyPoison(kProbeHit, kPoisonSeconds);
            sb.UpdateStatuses(kStatusProbeSeconds);
            FlowCheck(sb.GetHealthValue() < hp, "shield: poison ticks get through the shield");
            hp = sb.GetHealthValue();
            sb.TestSetSwinging(true);
            sb.TakeDamage(kProbeHit, front);
            FlowCheck(!sb.IsShieldRaised() && sb.GetHealthValue() < hp, "shield: a front hit lands while it swings");
            sb.TestSetSwinging(false);

            const int chunk = (int)std::ceil(full * Balance::ShieldGuard::kBreakAtMaxHpFraction / kGuardChunks);
            for (int i = 0; i < kMaxGuardHits && !sb.IsGuardBroken(); ++i)
            {
                hp = sb.GetHealthValue();
                sb.TakeDamage(chunk, front);
            }
            FlowCheck(sb.IsGuardBroken() && !sb.IsShieldRaised() && sb.GetHealthValue() < hp &&
                      sb.ConsumeHitBlock() == Enemy::HitBlockReason::None && sb.IsAlive(),
                      "shield: a full guard meter breaks the guard and that hit lands");
        }

        _gamepad.menuConfirmPressed = false;
        _gamepad.isActive = false;
        _gameState = GameState::Menu;
    }
    // Each event step: tag the (cleared) current room, open its cards, take a
    // screenshot, then apply one card and check what it did.
    auto prepareRoom = [&](RoomSpecialType type) -> DungeonRoomState&
    {
        DungeonRoomState& st = _dungeonRoomStates[_dungeonRoomIdx];
        TagRoomSpecial(st, type);
        st.specialOptions[0] = 0; st.specialOptions[1] = 1; st.specialOptions[2] = 2;   // fixed for the checks
        st.cleared = true;
        ClearDungeonEnemies();
        _dungeonReinforcements.clear();
        _pendingEnemySpawns.clear();
        _dungeonEnemiesSpawned = false;
        _currentRoomAffix = RoomAffix::None;   // as if entered fresh: decision rooms roll none
        _screenFlashTimer = 0.f;                // no relic-grant flash or hurt edge over the cards
        _playerHurtTimer  = 0.f;
        return st;
    };
    auto modifierCount = [&]() { return (int)_runModifiers.size(); };

    // Steps 2+ wait out any relic pick an event left behind.
    if (t.step >= 2 && t.subStep == 0)
    {
        if (_gameState == GameState::RelicChoice) { ConfirmRelicChoice(0); return; }
        if (_gameState != GameState::DungeonRun || !FlowTravelSettled() || _pendingRelicChoices > 0) return;
    }

    switch (t.step)
    {
    case 0:   // fresh profile -> Warrior -> village -> gate
        if (t.subStep == 0)
        {
            if (_gameState != GameState::Menu || !FlowTravelSettled()) return;
            _prologueEntryMode = PrologueEntryMode::NewGame;
            _classSelectReturnState = GameState::Menu;
            _gameState = GameState::ClassSelect;
            FlowCheck(ConfirmClassSelection(PlayerClass::Warrior), "events: Warrior selected");
            t.subStep = 1;
            return;
        }
        if (t.subStep == 1)
        {
            if (_gameState != GameState::Village || !FlowTravelSettled()) return;
            FinishVillageIntro();
            TriggerVillageDungeonGate();
            t.subStep = 2;
            return;
        }
        if (_gameState != GameState::DungeonRun || !FlowTravelSettled()) return;
        _player.SetInvulnerableLock(true);
        FlowNextStep();
        return;

    case 1:   // generation rules, over many rolls in several domains
    {
        const auto saved = _dungeonRoomStates;
        const int savedZone = _worldZone;
        const int startIdx = _dungeonGen.GetStartIndex(), bossIdx = _dungeonGen.GetBossIndex(), keyIdx = _dungeonGen.GetKeyIndex();
        bool restRule = true, buildRule = true, distinct = true, structural = true, standardOnly = true, someEvent = true;
        int seen[(int)RoomSpecialType::Count] = {};
        for (int zone : { 0, 1, 3 })
            for (int roll = 0; roll < 60; ++roll)
            {
                _worldZone = zone;
                for (auto& [idx, st] : _dungeonRoomStates) { st.special = RoomSpecialType::None; st.specialClaimed = false; }
                RollDungeonRoomSpecials();
                int rests = 0, events = 0, perType[(int)RoomSpecialType::Count] = {};
                for (const auto& [idx, st] : _dungeonRoomStates)
                {
                    if (st.special == RoomSpecialType::None) continue;
                    ++perType[(int)st.special];
                    ++seen[(int)st.special];
                    if (st.special == RoomSpecialType::RestSite) ++rests; else ++events;
                    if (idx == startIdx || idx == bossIdx || idx == keyIdx) structural = false;
                    if (_dungeonGen.GetRooms()[idx].type != RoomType::Standard) standardOnly = false;
                    if (zone == 0 && (st.special == RoomSpecialType::MutationAltar || st.special == RoomSpecialType::RelicForge))
                        buildRule = false;
                }
                if (rests != (zone >= 1 ? 1 : 0)) restRule = false;
                for (int n : perType) if (n > 1) distinct = false;
                const int fewest = zone == 0 ? 1 : 2, most = zone == 0 ? 2 : zone <= 2 ? 2 : 3;
                if (events < fewest || events > most) someEvent = false;
            }
        _dungeonRoomStates = saved;
        _worldZone = savedZone;
        FlowCheck(restRule, "events: one Rest Site per domain from the second on, none in the first");
        FlowCheck(buildRule, "events: Mutation Altar and Relic Forge wait for the second domain");
        FlowCheck(someEvent, "events: 1-2 events in the first domain, 2 next, 2-3 from the fourth");
        FlowCheck(distinct, "events: an event never repeats within a domain");
        FlowCheck(structural && standardOnly, "events: never on the entrance, boss, key, elite or treasure room");
        bool everySeen = true;
        for (int i = 1; i < (int)RoomSpecialType::Count; ++i) everySeen = everySeen && seen[i] > 0;
        FlowCheck(everySeen, "events: all seven decision rooms come up in rolls");
        FlowCheck(RoomEventGold(60) == 60, "events: first-domain gold is the base amount");
        FlowNextStep();
        return;
    }

    case 2:   // Cursed Cache: Blood Gold now, a two-room curse later
        if (t.subStep == 0) { prepareRoom(RoomSpecialType::CursedReward); OpenDecisionRoom(); }
        if (++t.subStep == 3) FlowScreenshot("event_cursed_cache");
        if (t.subStep < 4) return;
        {
            const int gold = _player.GetGold(), mods = modifierCount();
            ApplyDecisionChoice(0);
            FlowCheck(_player.GetGold() == gold + RoomEventGold(60), "Cursed Cache: Blood Gold pays out now");
            FlowCheck(modifierCount() == mods + 1 && _runModifiers.back().curse &&
                      _runModifiers.back().roomsRemaining == 2 && _runModifiers.back().enemyDmgMult > 1.29f,
                      "Cursed Cache: a two-room damage curse is armed");
            FlowCheck(_dungeonRoomStates[_dungeonRoomIdx].specialClaimed, "Cursed Cache: the room is spent");
            FlowCheck(PathIs(ReadRunCheckpointDocument(), "run.modifiers." + std::to_string(mods) + ".curse", "true"),
                      "Cursed Cache: the curse is in the checkpoint");
            ClearRunModifiers();
        }
        FlowNextStep();
        return;

    case 3:   // Omen Stone: Frailty (weaker enemies, half XP) and a below-1 multiplier
        if (t.subStep == 0) { prepareRoom(RoomSpecialType::OmenProphecy); OpenDecisionRoom(); }
        if (++t.subStep == 3) FlowScreenshot("event_omen_stone");
        if (t.subStep < 4) return;
        {
            ApplyDecisionChoice(1);
            FlowCheck(modifierCount() == 1 && !_runModifiers[0].curse && _runModifiers[0].enemyHpMult < 0.71f &&
                      _runModifiers[0].xpMult < 0.51f && _runModifiers[0].roomsRemaining == 1,
                      "Omen Stone: Frailty arms a one-room omen");
            const int gold = _player.GetGold();
            _player.SetContractGoldMult(0.5f);
            _player.AddGoldFromDrop(10);
            _player.SetContractGoldMult(1.f);
            _player.TakeContractBonusGold();
            FlowCheck(_player.GetGold() == gold + 5, "Omen Stone: a half-gold omen really halves drops");
            ClearRunModifiers();
            ActivatePendingModifiers();   // levers back to 1
        }
        FlowNextStep();
        return;

    case 4:   // Locked Armory: an unaffordable case is refused, the Gilded Case pays for a relic
        if (t.subStep == 0) { prepareRoom(RoomSpecialType::LockedArmory); _player.SetGold(0); OpenDecisionRoom(); }
        if (++t.subStep == 3) FlowScreenshot("event_locked_armory");
        if (t.subStep < 4) return;
        {
            ApplyDecisionChoice(2);
            FlowCheck(!_dungeonRoomStates[_dungeonRoomIdx].specialClaimed, "Locked Armory: no gold, no case, the armory stays open");
            _player.SetGold(400);
            OpenDecisionRoom();
            const int price = _decisionScreen.cards[2].costGold, pending = _pendingRelicChoices;
            ApplyDecisionChoice(2);
            FlowCheck(_player.GetGold() == 400 - price && _pendingRelicChoices == pending + 1,
                      TextFormat("Locked Armory: the Gilded Case costs %d gold and owes a relic", price));
            FlowCheck(_dungeonRoomStates[_dungeonRoomIdx].specialClaimed, "Locked Armory: one case only");
        }
        FlowNextStep();
        return;

    case 5:   // Mutation Altar: Glass Marrow trades max HP for damage
        if (t.subStep == 0) { prepareRoom(RoomSpecialType::MutationAltar); OpenDecisionRoom(); }
        if (++t.subStep == 3) FlowScreenshot("event_mutation_altar");
        if (t.subStep < 4) return;
        {
            const float maxHp = _player.GetMaxHealthValue();
            ApplyDecisionChoice(0);
            FlowCheck(fabsf(_player.GetMaxHealthValue() - maxHp * 0.8f) < 0.01f && fabsf(_player.GetEventDamageMult() - 1.2f) < 1e-4f,
                      "Mutation Altar: Glass Marrow is -20% max HP for +20% damage");
        }
        FlowNextStep();
        return;

    case 6:   // Relic Forge: relic removal is exact, then Salvage and Overcharge
        if (t.subStep == 0)
        {
            // RemoveRelic must undo each passive exactly.
            const float defense = _player.GetDefense();
            const float maxHp = _player.GetMaxHealthValue();
            if (!_player.HasRelic(RelicType::StoneSkin))
            {
                _player.AddRelic(RelicType::StoneSkin);
                _player.RemoveRelic(RelicType::StoneSkin);
                FlowCheck(fabsf(_player.GetDefense() - defense) < kDefenseEpsilon && !_player.HasRelic(RelicType::StoneSkin),
                          "Relic Forge: removing Stone Skin returns its Defense");
            }
            if (!_player.HasRelic(RelicType::ThornmailRunes))
            {
                _player.AddRelic(RelicType::ThornmailRunes);
                _player.RemoveRelic(RelicType::ThornmailRunes);
                FlowCheck(fabsf(_player.GetMaxHealthValue() - maxHp) < 0.01f, "Relic Forge: removing Thornmail Runes returns its max HP");
            }
            GrantRelic(RelicType::KeenEdge);
            prepareRoom(RoomSpecialType::RelicForge);
            OpenDecisionRoom();
        }
        if (++t.subStep == 3) FlowScreenshot("event_relic_forge");
        if (t.subStep < 4) return;
        {
            const int relics = _player.GetRelicCount(), gold = _player.GetGold();
            const RelicType newest = _player.GetRelicAt(relics - 1);
            ApplyDecisionChoice(0);
            FlowCheck(_player.GetRelicCount() == relics - 1 && !_player.HasRelic(newest) &&
                      _player.GetGold() == gold + RoomEventGold(50), "Relic Forge: Salvage melts the newest relic for gold");
            GrantRelic(RelicType::KeenEdge);
            GrantRelic(RelicType::Executioner);
            prepareRoom(RoomSpecialType::RelicForge);
            OpenDecisionRoom();
            const float maxHp = _player.GetMaxHealthValue();
            ApplyDecisionChoice(2);
            FlowCheck(fabsf(_player.GetRelicOverchargePerRelic() - 0.05f) < 1e-4f &&
                      fabsf(_player.GetMaxHealthValue() - maxHp * 0.85f) < 0.01f,
                      "Relic Forge: Overcharge is -15% max HP for +5% damage per relic");
        }
        FlowNextStep();
        return;

    case 7:   // Rest Site: rest heals 40%, train upgrades, fortify raises armour
        if (t.subStep == 0)
        {
            _player.SetInvulnerableLock(false);
            _player.BaseCharacter::TakeDamage(50, _player.GetWorldPos());
            _player.SetInvulnerableLock(true);
            prepareRoom(RoomSpecialType::RestSite);
            OpenDecisionRoom();
        }
        if (++t.subStep == 3) FlowScreenshot("event_rest_site");
        if (t.subStep < 4) return;
        {
            const float hp = _player.GetHealthValue(), maxHp = _player.GetMaxHealthValue();
            ApplyDecisionChoice(0);
            const float expected = std::min(maxHp, hp + std::ceil(maxHp * 0.4f));
            FlowCheck(fabsf(_player.GetHealthValue() - expected) < 0.01f && hp < maxHp,
                      TextFormat("Rest Site: Rest heals 40%% (%.1f -> %.1f of %.1f)", hp, _player.GetHealthValue(), maxHp));
            const AbilityType weakest = FindWeakestUpgradableAbility();
            const int level = weakest != AbilityType::None ? _player.GetAbilityLevel(weakest) : 0;
            prepareRoom(RoomSpecialType::RestSite);
            OpenDecisionRoom();
            ApplyDecisionChoice(1);
            FlowCheck(weakest != AbilityType::None && _player.GetAbilityLevel(weakest) == level + 1,
                      "Rest Site: Train upgrades the weakest ability");
            const float defense = _player.GetDefense();
            prepareRoom(RoomSpecialType::RestSite);
            OpenDecisionRoom();
            ApplyDecisionChoice(2);
            FlowCheck(fabsf(_player.GetDefense() - (defense + Balance::Stats::kDefenseMajor)) < kDefenseEpsilon,
                      "Rest Site: Fortify raises Defense");
        }
        FlowNextStep();
        return;

    case 8:   // the campfire in the room, with its prompt
        if (_gameState == GameState::RelicChoice) { ConfirmRelicChoice(0); return; }
        if (t.subStep == 0)
        {
            prepareRoom(RoomSpecialType::RestSite);
            const Vector2 fire = GetDecisionShrinePos();
            _player.SetWorldPos(Vector2{ fire.x + 40.f, fire.y + 120.f });
        }
        if (++t.subStep < 20) return;
        FlowCheck(_nearDecisionShrine, "Rest Site: standing by the fire offers it");
        FlowScreenshot("event_rest_site_room");
        FlowNextStep();
        return;

    case 9:   // a real first entry into this domain's rolled event room
    {
        int eventRoom = -1;
        for (const auto& [idx, st] : _dungeonRoomStates)
            if (st.special != RoomSpecialType::None && idx != _dungeonRoomIdx) { eventRoom = idx; break; }
        FlowCheck(eventRoom >= 0, "events: the first domain rolled an event room");
        if (eventRoom < 0) { FinishFlowSelfTest(false); return; }
        bool noAffix = true, peaceful = true, banner = true;
        for (int i = 0; i < 25; ++i)   // affixes are a dice roll: enter fresh many times
        {
            _dungeonRoomStates[eventRoom].cleared = false;
            _dungeonRoomStates[eventRoom].enemiesInitialized = false;
            EnterDungeonRoom(eventRoom, DungeonDoorSide::None,
                             Vector2{ (float)kVirtualWidth * 0.5f, (float)kVirtualHeight * 0.7f }, false);
            noAffix  = noAffix && _currentRoomAffix == RoomAffix::None;
            peaceful = peaceful && _dungeonRoomStates[eventRoom].cleared;
            banner   = banner && _roomIntroTitle == GetRoomEventPresentation(_dungeonRoomStates[eventRoom].special).title;
        }
        FlowCheck(noAffix, "events: an event room never rolls a combat affix");
        FlowCheck(peaceful, "events: an event room is peaceful on first entry");
        FlowCheck(banner, "events: an event room announces itself by name");
        FlowNextStep();
        return;
    }

    case 10:   // relic expansion: a pick offering new relics
        if (t.subStep == 0)
        {
            _relicChoices[0] = RelicType::Ambush;
            _relicChoices[1] = RelicType::LastStand;
            _relicChoices[2] = RelicType::Hourglass;
            _relicChoiceCursor = 1;
            _relicChoiceOpenTimer = 0.f;
            _pendingRelicChoices = 1;
            _gameState = GameState::RelicChoice;
        }
        if (++t.subStep == 3) FlowScreenshot("relics_new_choice");
        if (t.subStep < 4) return;
        ConfirmRelicChoice(1);
        FlowCheck(_player.HasRelic(RelicType::LastStand) && _pendingRelicChoices == 0, "relics: Last Stand taken from a pick");
        FlowNextStep();
        return;

    case 11:   // Last Stand beats Second Wind to a lethal hit, once per room
        if (t.subStep == 0)
        {
            // The event rooms above leave an intro banner playing, and the room
            // holds still under it: a fresh shot must not move while it plays.
            // Clear it so the lethal hit below is handled on the next frames.
            {
                constexpr float kFreezeProbeDt = 1.f / 60.f;
                SpreadProjectile probe;
                probe.InitBasic(_player.GetWorldPos(), Vector2{ 0.f, -1.f }, AbilityType::FireSpread, WHITE);
                _spreadProjectiles.push_back(probe);
                const std::size_t probeIdx = _spreadProjectiles.size() - 1;
                const Vector2 start = _spreadProjectiles[probeIdx].GetWorldPos();
                _roomIntroTimer = kRoomIntroDuration;
                UpdateDungeonRun(kFreezeProbeDt);
                const bool held = probeIdx < _spreadProjectiles.size() &&
                                  _spreadProjectiles[probeIdx].GetWorldPos().x == start.x &&
                                  _spreadProjectiles[probeIdx].GetWorldPos().y == start.y;
                FlowCheck(held, "banner: the room holds still while the intro banner plays");
                if (probeIdx < _spreadProjectiles.size())
                    _spreadProjectiles.erase(_spreadProjectiles.begin() + (std::ptrdiff_t)probeIdx);
                _roomIntroTimer = 0.f;
            }

            _lastStandUsedThisRoom = false;
            _secondWindAvailable = true;
            _player.SetInvulnerableLock(false);
            _player.BaseCharacter::TakeDamage(9999, _player.GetWorldPos());
            t.subStep = 1;
            return;
        }
        if (++t.subStep < 6) return;
        FlowCheck(!_playerDying && fabsf(_player.GetHealthValue() - Character::kReviveMinHealth) < 0.01f && _lastStandUsedThisRoom,
                  TextFormat("relics: Last Stand leaves %.0f HP (HP %.1f)", Character::kReviveMinHealth, _player.GetHealthValue()));
        FlowCheck(_secondWindAvailable, "relics: Last Stand goes first, Second Wind is kept");
        _player.SetInvulnerableLock(true);
        FlowNextStep();
        return;

    case 12:   // Retaliation after a hit; Ironclad and Coin Purse on a real room clear
    {
        constexpr float kToastProbeSeconds = 4.f;   // a fresh relic toast, as GrantRelic shows
        // Deadeye rolls a random crit per call; compare crit-free samples so the
        // two multipliers differ only by game state.
        auto critFreeMultiplier = [&]()
        {
            constexpr int kMaxCritRerolls = 32;   // 20% crit chance: never exhausted in practice
            RelicHitContext neutral; neutral.targetHpFraction = 0.6f;
            bool crit = true;
            float mult = 0.f;
            for (int attempt = 0; attempt < kMaxCritRerolls && crit; ++attempt)
                mult = _player.OutgoingDamageMultiplier(neutral, crit);
            return mult;
        };
        if (t.subStep == 0)
        {
            if (t.stepTime < 2.5f) return;   // Last Stand's revive i-frames must run out first
            // A Retaliation from an earlier random pick also opened its 3 s window
            // on Last Stand's hit; let it close, or the baseline is already buffed.
            if (_player.IsRetaliationActive()) return;
            GrantRelic(RelicType::Retaliation);
            GrantRelic(RelicType::Ironclad);
            GrantRelic(RelicType::CoinPurse);
            _player.Heal(999);   // healed this frame, hit next frame: the detector sees the drop
            _relicToastTimer = kToastProbeSeconds;
            t.subStep = 1;
            return;
        }
        if (t.subStep == 1)
        {
            t.hpBeforeExit = critFreeMultiplier();   // reused as scratch
            _player.SetInvulnerableLock(false);
            _player.BaseCharacter::TakeDamage(10, _player.GetWorldPos());
            t.subStep = 2;
            return;
        }
        if (t.subStep < 5)
        {
            if (++t.subStep < 5) return;   // a few frames for the hurt detector to see the HP drop
            _player.SetInvulnerableLock(true);
            const bool windowOpen = _player.IsRetaliationActive();
            const float after = critFreeMultiplier();
            FlowCheck(windowOpen && after > t.hpBeforeExit + 0.2f,
                      TextFormat("relics: Retaliation after a hit (window %s, %.2f -> %.2f, HP %.1f)",
                                 windowOpen ? "open" : "closed", t.hpBeforeExit, after, _player.GetHealthValue()));
            FlowCheck(_relicToastTimer > 0.f && _relicToastTimer < kToastProbeSeconds,
                      TextFormat("hud: toasts count down on real dungeon frames (%.2f s left)", _relicToastTimer));

            // A fresh combat room, cleared for real.
            int combatRoom = -1;
            const auto& rooms = _dungeonGen.GetRooms();
            for (int i = 0; i < (int)rooms.size() && combatRoom < 0; ++i)
            {
                auto st = _dungeonRoomStates.find(i);
                const bool spent = st != _dungeonRoomStates.end() && (st->second.cleared || st->second.special != RoomSpecialType::None);
                if (!spent && rooms[i].type == RoomType::Standard && i != _dungeonGen.GetBossIndex() &&
                    i != _dungeonGen.GetKeyIndex() && i != _dungeonGen.GetStartIndex())
                    combatRoom = i;
            }
            FlowCheck(combatRoom >= 0, "relics: a combat room to clear");
            if (combatRoom < 0) { FinishFlowSelfTest(false); return; }
            EnterDungeonRoom(combatRoom, DungeonDoorSide::None,
                             Vector2{ (float)kVirtualWidth * 0.5f, (float)kVirtualHeight * 0.55f }, false);
            t.defenseBefore = _player.GetDefense();
            t.goldBeforeExit = _player.GetGold();
            t.subStep = 6;
            return;
        }
        if (_gameState == GameState::RelicChoice) { ConfirmRelicChoice(0); return; }
        if (_gameState != GameState::DungeonRun) return;
        if (!_dungeonRoomStates[_dungeonRoomIdx].cleared)
        {
            FlowKillAllEnemies();
            _dungeonReinforcements.clear();
            return;
        }
        FlowCheck(_player.GetDefense() >= t.defenseBefore + Balance::Stats::kIroncladDefensePerRoom - kDefenseEpsilon,
                  "relics: Ironclad adds Defense on a room clear");
        FlowCheck(_player.GetGold() >= t.goldBeforeExit + 15, TextFormat("relics: Coin Purse pays on a room clear (%d -> %d)",
                                                                         t.goldBeforeExit, _player.GetGold()));
        FlowNextStep();
        return;
    }

    case 13:   // the crossroads survives a save and Continue
        if (t.subStep == 0)
        {
            if (_gameState == GameState::RelicChoice) { ConfirmRelicChoice(0); return; }
            if (_gameState != GameState::DungeonRun) return;
            _pickups.clear();
            CompleteDomain();   // as the boss exit does
            FlowCheck(_atCrossroads, "crossroads: a cleared domain leads to the crossroads");
            for (int i = 0; i < 3; ++i) t.crossroadsBiomes[i] = _crossroadsDoors[i].open ? (int)_crossroadsDoors[i].biome : -1;
            for (CrossroadsDoor& d : _crossroadsDoors) d.biome = Biome::Forest;   // scramble: resume must restore
            FlowCheck(ResumeSavedRun(), "crossroads: Continue starts from the checkpoint");
            t.subStep = 1;
            return;
        }
        if (!_atCrossroads || _gameState != GameState::DungeonRun || !FlowTravelSettled()) return;
        {
            bool same = true;
            for (int i = 0; i < 3; ++i)
                same = same && (_crossroadsDoors[i].open ? (int)_crossroadsDoors[i].biome : -1) == t.crossroadsBiomes[i];
            FlowCheck(same, "crossroads: resuming restores the same three roads");
        }
        FlowNextStep();
        return;

    case 14:   // after the fifth domain only the final descent is left, and it leads there
        if (t.subStep == 0)
        {
            _worldZone = RunProgression::kFinalZone - 1;
            RollCrossroadsDoors();
            OpenCrossroads();
            FlowCheck(!_crossroadsDoors[0].open && !_crossroadsDoors[2].open && _crossroadsDoors[1].open &&
                      _crossroadsDoors[1].biome == Biome::DemonsInsides, "crossroads: one door, to Demon's Insides");
            t.subStep = 1;
            return;
        }
        if (t.subStep < 30) { if (++t.subStep == 30) FlowScreenshot("crossroads_final_descent"); return; }
        if (t.subStep == 30)
        {
            _player.SetInvulnerableLock(false);
            _player.BaseCharacter::TakeDamage(20, _player.GetWorldPos());
            _player.SetInvulnerableLock(true);
            ChooseCrossroadsDoor(1);
            t.subStep = 31;
            return;
        }
        if (_atCrossroads || _gameState != GameState::DungeonRun || !FlowTravelSettled()) return;
        FlowCheck(_worldZone == RunProgression::kFinalZone && _currentBiome == Biome::DemonsInsides,
                  "crossroads: the last door is the final descent");
        FlowCheck((int)_dungeonGen.GetRooms().size() == DungeonGen::PlanForDomain(RunProgression::kFinalZone).rooms &&
                  _dungeonGen.GetRooms()[_dungeonGen.GetStartIndex()].type == RoomType::Store,
                  "final domain: full size, opening at Zeph's last camp");
        FlowCheck(_player.GetHealthValue() >= _player.GetMaxHealthValue() - 0.01f, "final descent: arrive fully healed");
        FlowNextStep();
        return;

    case 15:   // Zeph's stock survives a save and Continue (no free restock)
    {
        static std::vector<ShopManager::OfferView> s_saved;
        auto sameOffers = [](const std::vector<ShopManager::OfferView>& a, const std::vector<ShopManager::OfferView>& b)
        {
            if (a.size() != b.size()) return false;
            for (size_t i = 0; i < a.size(); ++i)
                if (a[i].price != b[i].price || a[i].isContract != b[i].isContract || a[i].isAbility != b[i].isAbility ||
                    a[i].ability != b[i].ability || a[i].ware != b[i].ware || a[i].purchased != b[i].purchased)
                    return false;
            return true;
        };
        if (t.subStep == 0)
        {
            FlowCheck(_shopStockZone == _worldZone, "shop: the final camp stocked Zeph");
            SaveRunCheckpoint();
            s_saved = _shop.GetOffers();
            const JsonValue checkpoint = ReadRunCheckpointDocument();
            const JsonValue* items = FindPath(checkpoint, "shop.items");
            FlowCheck(PathIs(checkpoint, "shop.zone", std::to_string(_worldZone)) && items && items->Size() == s_saved.size(),
                      "shop: the checkpoint holds Zeph's stock");
            _player.AddGold(1000);
            _shop.TryReroll(_player);   // what a player could do before quitting
            FlowCheck(ResumeSavedRun(), "shop: Continue from the camp");
            t.subStep = 1;
            return;
        }
        if (_gameState != GameState::DungeonRun || !FlowTravelSettled()) return;
        FlowCheck(sameOffers(_shop.GetOffers(), s_saved), "shop: Continue finds the same wares at the same prices");
        FlowCheck(!_campNeedsRestock, "shop: no restock owed after Continue");
        FlowNextStep();
        return;
    }

    case 16:   // the Grimoire: this run's finds and feats, saved, and every tab
        if (t.subStep == 0)
        {
            FlowCheck(_meta.HasFeat((int)Feat::Reshaped) && _meta.HasFeat((int)Feat::Pathfinder) &&
                      _meta.HasFeat((int)Feat::FinalDescent), "grimoire: feats earned on the way (Reshaped, Pathfinder, Final Descent)");
            FlowCheck(_meta.HasFoundRelic((int)RelicType::LastStand) && _meta.FoundRelicCount() >= 5,
                      TextFormat("grimoire: relics carried are recorded (%d)", _meta.FoundRelicCount()));
            FlowCheck(_meta.FoundCreatureCount() >= 1, TextFormat("grimoire: creatures slain are recorded (%d)", _meta.FoundCreatureCount()));
            FlowCheck(!_featToastText.empty(), "grimoire: earning a feat announces it");
            _meta.SaveIfDirty();
            const JsonValue profile = ReadSaveDocument(SaveFiles::ProfilePath(), SaveKind::Profile, SaveFiles::kMaxProfileBytes);
            FlowCheck(ListHas(FindPath(profile, "grimoire.relics"), "LastStand") &&
                      ListHas(FindPath(profile, "grimoire.feats"), "Reshaped"), "grimoire: finds and feats are saved with the profile");
            _stateBeforePause = GameState::DungeonRun;
            _gameState = GameState::Pause;
            t.subStep = 1;
            return;
        }
        ++t.subStep;
        if (t.subStep == 8)
        {
            FlowScreenshot("pause_menu");   // eight buttons, the Grimoire and Character among them
            OpenGrimoire(GameState::Pause);
            _grimoireTab = 0;
            _grimoireCursor = (int)RelicType::LastStand;
        }
        if (t.subStep == 15) FlowScreenshot("grimoire_relics");
        if (t.subStep == 16) { _grimoireTab = 1; _grimoireCursor = 0; }
        if (t.subStep == 30) FlowScreenshot("grimoire_bestiary");
        if (t.subStep == 31) { _grimoireTab = 2; _grimoireCursor = 0; }
        if (t.subStep == 45) FlowScreenshot("grimoire_chronicle");
        if (t.subStep == 46) { _grimoireTab = 3; _grimoireCursor = (int)Feat::Reshaped; }
        if (t.subStep == 60) FlowScreenshot("grimoire_feats");
        if (t.subStep < 61) return;
        constexpr int kCharacterShotStep = 75;
        if (t.subStep == 61)
        {
            FlowCheck(_gameState == GameState::Grimoire, "grimoire: open from the pause menu");
            _gameState = _grimoireReturnState;
            FlowCheck(_gameState == GameState::Pause, "grimoire: Back returns to the pause menu");
            // The Character page is the second pause screen.
            ApplyPauseAction(kCharacterSheetPauseAction);
            FlowCheck(_gameState == GameState::CharacterSheet, "character: open from the pause menu");
        }
        if (t.subStep == kCharacterShotStep) FlowScreenshot("character_sheet");
        if (t.subStep <= kCharacterShotStep) return;
        FlowCheck(_gameState == GameState::CharacterSheet, "character: stays open until Back");
        _gameState = _characterSheetReturnState;
        FlowCheck(_gameState == GameState::Pause, "character: Back returns to the pause menu");
        // Pause choices are applied from Update (never while drawing).
        ApplyPauseAction(0);
        FlowCheck(_gameState == GameState::Pause, "pause: no choice keeps the pause menu open");
        ApplyPauseAction(1);
        FlowCheck(_gameState == GameState::DungeonRun, "pause: Resume returns to the paused run");
        FinishFlowSelfTest(false);
        return;
    }
}

// Phase 8: the Character Animator (dev tool). Every character opens and plays,
// the size readout matches how the game sizes them, a size change scales the
// hitboxes and can be undone, leaving with unsaved edits asks first, and the
// lineup shows everyone. It never saves: dev builds read tuning files from the
// source folder, and the last check proves none of them changed.
namespace
{
    constexpr float kFlowSizeTolerance  = 0.02f;   // multiples of the hero's height
    constexpr float kFlowSizeChange     = 1.5f;    // the size the probe sets, x its start
    constexpr float kFlowScaleEpsilon   = 0.001f;
    constexpr float kFlowRadiusEpsilon  = 0.01f;   // world px
    constexpr int   kFlowAnimatorFrames = 3;       // frames for a change to be measured and drawn
    constexpr int   kFlowLineupHeroes   = 2;       // the hero opens the elite and boss rows
    // Hit windows, events and moves, saved into a sandbox folder and read back
    // by a freshly spawned AncientBear.
    constexpr int   kFlowBearMoves      = 3;       // disc, ring and wedge slams
    constexpr int   kFlowWedgeMove      = 2;
    constexpr float kFlowWedgeReach     = 400.f;
    constexpr int   kFlowHitStart       = 2;
    constexpr int   kFlowHitEnd         = 4;
    constexpr int   kFlowEventFrame     = 3;
    constexpr float kFlowPadX           = 10.f;
    constexpr float kFlowPadY           = 6.f;
    constexpr int   kFlowWerewolfSwipe2 = 3;

    // Every tuning file's name, size and write time, sorted, as one line.
    std::string FlowTuningFiles()
    {
        std::vector<std::string> files;
        std::error_code ec;
        for (const auto& file : std::filesystem::directory_iterator(".", ec))
        {
            const std::string name = file.path().filename().string();
            if (name.rfind("charactertuning_", 0) != 0 && name.rfind("attacktuning_", 0) != 0)
                continue;
            files.push_back(name + ":" + std::to_string(file.file_size(ec)) + ":" +
                            std::to_string(file.last_write_time(ec).time_since_epoch().count()));
        }
        std::sort(files.begin(), files.end());
        std::string listing;
        for (const std::string& file : files)
            listing += file + ";";
        return listing;
    }
}

void Engine::UpdateFlowSelfTestPhase8()
{
    FlowSelfTest& t = _flowTest;
    CharacterAnimator& animator = _charAnimator;
    // Let the animator measure and draw what the last step changed.
    auto waiting = [&t]() { return ++t.subStep < kFlowAnimatorFrames; };

    switch (t.step)
    {
    case 0:   // menu -> the animator; the list has the eight characters it used to miss
    {
        if (_gameState != GameState::Menu || !FlowTravelSettled()) return;
        t.tuningFilesBefore = FlowTuningFiles();
        animator.Init();
        _gameState = GameState::CharacterAnimator;
        const char* added[] = { "Grunt", "Cyclops", "Ogre", "Molarbeast", "Bonechill", "Infernal", "Stormclub", "Venomfang" };
        int found = 0;
        for (const char* name : added)
            for (int i = 0; i < animator.TestEntryCount(); i++)
                if (animator.TestEntryName(i) == name) { ++found; break; }
        FlowCheck(found == (int)(sizeof(added) / sizeof(added[0])), "animator: lists the grunt, the six elites and Molarbeast");
        FlowNextStep();
        return;
    }
    case 1:
        if (waiting()) return;
        FlowScreenshot("animator_01_select");
        FlowNextStep();
        return;
    case 2:   // every character opens; every animation has frames and steps both ways
    {
        std::string broken;
        bool steps = true, untouched = true;
        for (int i = 0; i < animator.TestEntryCount(); i++)
        {
            const std::string name = animator.TestEntryName(i);
            if (!animator.TestOpen(name)) { broken += " " + name; continue; }
            Enemy* character = animator.TestCharacter();
            for (int anim = 0; anim < character->GetEditorAnimCount(); anim++)
            {
                animator.TestSelectAnim(anim);
                if (character->GetEditorFrameCount() < 1 || character->GetEditorAnimName(anim)[0] == 0)
                    broken += TextFormat(" %s/%d", name.c_str(), anim);
                const int frame = character->GetEditorFrame();
                animator.TestStepFrame(1);
                animator.TestStepFrame(-1);
                steps = steps && character->GetEditorFrame() == frame;
            }
            untouched = untouched && !animator.TestHasUnsavedChanges();
        }
        FlowCheck(broken.empty(), "animator: every character opens with named, playable animations" + broken);
        FlowCheck(steps, "animator: a frame step forward then back lands on the same frame");
        FlowCheck(untouched, "animator: opening and playing a character changes nothing");
        animator.TestRequestBack();
        FlowCheck(animator.TestOnSelectScreen(), "animator: Back with nothing changed goes straight to the list");
        FlowNextStep();
        return;
    }
    case 3:   // the size readout is the game's: standard enemies at their multiple of the hero
    {
        struct Expect { const char* name; float sizeVsHero; };
        const Expect expects[] = {
            { "Grunt",     1.f },
            { "Cyclops",   Balance::Visual::kEliteSizeMult },
            { "Sporeling", Balance::Visual::kMushroomSizeMult },
        };
        for (const Expect& expect : expects)
        {
            const CharacterTuning* saved = CharacterTuningStore::Get(expect.name);
            if (saved != nullptr && saved->sizeSet)
            {
                FlowLog(std::string("skip  ") + expect.name + " has a saved size");
                continue;
            }
            animator.TestOpen(expect.name);
            FlowCheck(fabsf(animator.TestSizeVsHero() - expect.sizeVsHero) < kFlowSizeTolerance,
                      TextFormat("animator: %s reads %.2fx hero, the size the game gives it (%.2fx)",
                                 expect.name, animator.TestSizeVsHero(), expect.sizeVsHero));
        }
        animator.TestRequestBack();
        FlowNextStep();
        return;
    }
    case 4:   // Size scales the hitboxes with the sprite; undo and redo; unsaved tracking
    {
        animator.TestOpen("Werewolf");
        Enemy* wolf = animator.TestCharacter();
        const float scale  = wolf->GetDrawScale();
        const bool  body   = wolf->GetAnimBodySet(0);
        const float radius = wolf->GetAnimBodyRadius(0);
        const float size   = animator.TestSizeVsHero();
        animator.TestSetSizeVsHero(size * kFlowSizeChange);
        const float grown = wolf->GetDrawScale() / scale;
        FlowCheck(fabsf(animator.TestSizeVsHero() - size * kFlowSizeChange) < kFlowSizeTolerance,
                  TextFormat("animator: Size sets %.2fx hero (was %.2fx)", animator.TestSizeVsHero(), size));
        FlowCheck(body && fabsf(wolf->GetAnimBodyRadius(0) - radius * grown) < kFlowRadiusEpsilon,
                  TextFormat("animator: the body circle grows with the sprite (%.1f -> %.1f px)", radius, wolf->GetAnimBodyRadius(0)));
        FlowCheck(animator.TestHasUnsavedChanges(), "animator: a size change shows as unsaved");
        animator.TestUndo();
        FlowCheck(fabsf(wolf->GetDrawScale() - scale) < kFlowScaleEpsilon && !animator.TestHasUnsavedChanges(),
                  "animator: Undo puts the size back and nothing is left unsaved");
        animator.TestRedo();
        FlowCheck(fabsf(animator.TestSizeVsHero() - size * kFlowSizeChange) < kFlowSizeTolerance,
                  "animator: Redo brings the new size back");
        animator.TestSetHeroShown(true);
        FlowNextStep();
        return;
    }
    case 5:
        if (waiting()) return;
        FlowCheck(animator.TestMeasuredFeetBelowCentre(), "animator: the measured feet line is below the character's centre");
        FlowCheck(animator.TestHeroPlaced(), "animator: the hero stands beside the character, feet on the same ground");
        FlowScreenshot("animator_02_werewolf_bigger");
        animator.TestRequestBack();
        FlowCheck(animator.TestDialogOpen() && !animator.TestOnSelectScreen(), "animator: Back with unsaved changes asks first");
        FlowNextStep();
        return;
    case 6:
        if (waiting()) return;
        FlowScreenshot("animator_03_unsaved_prompt");
        animator.TestDialogCancel();
        FlowCheck(!animator.TestDialogOpen() && !animator.TestOnSelectScreen(), "animator: Cancel stays in the editor");
        animator.TestRequestBack();
        animator.TestDialogDiscard();
        FlowCheck(animator.TestOnSelectScreen(), "animator: Don't save goes back to the list");
        animator.TestOpenLineup();
        FlowCheck(animator.TestLineupCount() == animator.TestEntryCount() + kFlowLineupHeroes,
                  TextFormat("animator: the size lineup shows every character with the hero on each row (%d)", animator.TestLineupCount()));
        FlowNextStep();
        return;
    case 7:
        if (waiting()) return;
        FlowScreenshot("animator_04_lineup");
        animator.TestOpen("Player");
        FlowNextStep();
        return;
    case 8:
        if (waiting()) return;
        FlowScreenshot("animator_05_player");
        animator.TestRequestBack();
        animator.TestOpen("Molarbeast");
        FlowNextStep();
        return;
    case 9:
        if (waiting()) return;
        FlowScreenshot("animator_06_molarbeast");
        animator.TestRequestBack();
        animator.TestOpen("Ogre");
        FlowNextStep();
        return;
    case 10:   // melee boxes only where the game reads them; hit windows; events
    {
        if (waiting()) return;
        FlowScreenshot("animator_07_ogre");
        animator.TestRequestBack();

        animator.TestOpen("Grunt");
        FlowCheck(animator.TestIsMeleeAnim(Enemy::kAnimSlotAttack) && !animator.TestIsMeleeAnim(0),
                  "animator: the grunt's melee box is on its attack animation only");
        animator.TestRequestBack();
        animator.TestOpen("Werewolf");
        FlowCheck(animator.TestIsMeleeAnim(Enemy::kAnimSlotAttack) && animator.TestIsMeleeAnim(kFlowWerewolfSwipe2),
                  "animator: both Werewolf swipes have their own melee box");
        animator.TestRequestBack();
        animator.TestOpen("ChompBug");
        bool chompMelee = false;
        for (int slot = 0; slot < animator.TestCharacter()->GetEditorAnimCount(); slot++)
            chompMelee = chompMelee || animator.TestIsMeleeAnim(slot);
        FlowCheck(!chompMelee && animator.TestMoveCount() == 1,
                  "animator: ChompBug offers no melee box the game ignores; its dive is a move");
        animator.TestRequestBack();
        animator.TestOpen("Player");
        FlowCheck(animator.TestIsMeleeAnim(Character::kSwingAnimSlot) && animator.TestIsMeleeAnim(Character::kStabAnimSlot),
                  "animator: the hero's Swing and Stab each have a melee box");
        animator.TestRequestBack();

        // The HIT lane starts on the code's frames; a window set there takes over.
        animator.TestOpen("Grunt");
        animator.TestSelectAnim(Enemy::kAnimSlotAttack);
        Enemy* grunt = animator.TestCharacter();
        grunt->SetEditorFrame(Enemy::kBaseMeleeHitFrame);
        const bool codeLive = animator.TestMeleeLiveNow();
        grunt->SetEditorFrame(0);
        const bool codeIdle = !animator.TestMeleeLiveNow();
        FlowCheck(codeLive && codeIdle, "animator: the HIT lane starts on the code's frame (the grunt strikes on its third frame)");
        animator.TestSetHitWindow(0, 1);
        FlowCheck(animator.TestMeleeLiveNow(), "animator: a hit window set in the lane takes over");
        animator.TestAddEvent(CharacterTuning::EventKind::Fx, 1);
        FlowCheck(animator.TestEventCount() == 1 && animator.TestHasUnsavedChanges(),
                  "animator: an FX event lands on its frame and shows as unsaved");
        FlowNextStep();
        return;
    }
    case 11:
        if (waiting()) return;
        FlowScreenshot("animator_08_grunt_hit_window_and_event");
        animator.TestUndo();
        animator.TestUndo();
        FlowCheck(animator.TestEventCount() == 0 && !animator.TestCharacter()->GetAnimHitSet(Enemy::kAnimSlotAttack) &&
                  !animator.TestHasUnsavedChanges(), "animator: undo takes the event and the hit window back out");
        animator.TestRequestBack();
        animator.TestOpen("AncientBear");
        animator.TestSelectMove(kFlowWedgeMove);
        FlowNextStep();
        return;
    case 12:   // end to end in a sandbox folder: save from the animator, spawn the boss
    {
        if (waiting()) return;
        FlowScreenshot("animator_09_ancientbear_move");
        animator.TestRequestBack();
        const std::string folder = UserDataPath("tuning");
        std::error_code error;
        std::filesystem::create_directories(folder, error);
        TuningFiles::Directory() = folder;
        CharacterTuningStore::ReloadAll();
        AttackTuningStore::ReloadAll();

        animator.TestOpen("AncientBear");
        FlowCheck(animator.TestMoveCount() == kFlowBearMoves && animator.TestMoveKey(kFlowWedgeMove) == "AncientBear_Slam_Wedge",
                  "animator: AncientBear lists its three slam shapes");
        animator.TestSelectMove(kFlowWedgeMove);
        animator.TestSetMoveRadius(kFlowWedgeReach);
        animator.TestSelectAnim(Enemy::kAnimSlotAttack);
        animator.TestSetHitWindow(kFlowHitStart, kFlowHitEnd);
        animator.TestAddEvent(CharacterTuning::EventKind::Shake, kFlowEventFrame);
        animator.TestSetHurtPad(kFlowPadX, kFlowPadY);
        animator.TestSaveAll();
        FlowCheck(!animator.TestHasUnsavedChanges(), "animator: Save writes the character and its move");
        animator.TestRequestBack();
        {
            AncientBear bear(Vector2{ 0.f, 0.f });
            bear.Init();
            FlowCheck(fabsf(bear.GetSlamWedgeReach() - kFlowWedgeReach) < kFlowScaleEpsilon,
                      TextFormat("game: a spawned AncientBear uses the saved wedge reach (%.0f)", bear.GetSlamWedgeReach()));
            FlowCheck(bear.GetAnimHitSet(Enemy::kAnimSlotAttack) && bear.GetAnimHitStart(Enemy::kAnimSlotAttack) == kFlowHitStart &&
                      bear.GetAnimHitEnd(Enemy::kAnimSlotAttack) == kFlowHitEnd, "game: it uses the saved hit window");
            FlowCheck(fabsf(bear.GetHurtPadX() - kFlowPadX) < kFlowScaleEpsilon && fabsf(bear.GetHurtPadY() - kFlowPadY) < kFlowScaleEpsilon,
                      "game: it uses the saved hurtbox padding");
            std::vector<FiredAnimEvent> fired;
            bear.PlayEditorAnim(Enemy::kAnimSlotAttack);
            bear.SetEditorFrame(0);
            bear.CollectAnimEvents(fired);
            const size_t beforeFrame = fired.size();
            bear.SetEditorFrame(kFlowEventFrame);
            bear.CollectAnimEvents(fired);
            FlowCheck(beforeFrame == 0 && fired.size() == 1 && fired[0].kind == CharacterTuning::EventKind::Shake,
                      "game: the saved shake event fires when the slam reaches its frame");
        }
        TuningFiles::Directory().clear();
        CharacterTuningStore::ReloadAll();
        AttackTuningStore::ReloadAll();

        animator.TestOpen("Werewolf");
        animator.TestSelectAnim(kFlowWerewolfSwipe2);
        FlowNextStep();
        return;
    }
    default:
        if (waiting()) return;
        FlowScreenshot("animator_10_werewolf_swipe2");
        animator.TestRequestBack();
        FlowCheck(FlowTuningFiles() == t.tuningFilesBefore, "animator: the phase wrote no tuning file");
        animator.Unload();
        _menu.Init();
        _gameState = GameState::Menu;
        FinishFlowSelfTest(false);
        return;
    }
}

#endif // MO_DEV_TOOLS
