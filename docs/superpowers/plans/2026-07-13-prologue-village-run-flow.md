# Prologue, Fixed Village, and Run Continuity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the one-time Forest combat prologue, first Poe resurrection, compact fixed village, locked starter roster, Forest-first progression, empty biome entrances, and village visits that preserve the active world-map run.

**Architecture:** Keep orchestration in `Engine`, but move durable rules into small engine-independent modules: `RunSession` owns run/map pause state, `ClassUnlockRules` owns roster gates, `VillageLayoutData` loads a fixed layout, and `PrologueController` owns the tutorial state machine. Existing `VillageAssetData`, `DungeonGen`, `WorldMapManager`, `CutsceneManager`, `DialogueBox`, and combat entities remain the rendering/gameplay adapters. The debug playground remains separate and retains its editor behavior.

**Tech Stack:** C++17, raylib 5.5, Visual Studio/MSBuild `Debug|x64`, standalone `cl.exe` assertion tests.

## Global Constraints

- Preserve every unrelated user and Claude change in the dirty worktree.
- Do not change combat balance, sound assignments, village asset authoring, or shop wares.
- The real village is fixed and never randomized; sandbox/playground placement remains available for testing.
- Starter classes are Warrior, Hunter, and Mage. Rogue and Warlock are locked milestones. Paladin unlocks after a completed run.
- The prologue occurs once per save, has exactly three horizontal Forest rooms, grants no rewards, and cannot fail before the scripted third-room death.
- Returning after a boss fully restores health and mana and pauses the run. Continuing restores neither and retains the existing double-or-nothing reward bonus without a wager orb.
- Entering a biome begins in an empty entrance room with no Poe, Zeph, shop, dialogue, enemies, or wager object.
- New source files must be added to `TestGame/TestGame.vcxproj` and `TestGame/TestGame.vcxproj.filters`.

---

### Task 1: Persistent Run Session Rules

**Files:**
- Create: `TestGame/RunSession.h`
- Create: `TestGame/RunSession.cpp`
- Create: `TestGame/RunSessionTests.cpp`
- Modify: `TestGame/TestGame.vcxproj`
- Modify: `TestGame/TestGame.vcxproj.filters`

**Interfaces:**
- Produces `RunSession::Begin()`, `PauseInVillage()`, `Resume()`, `RecordBiomeClear(Biome)`, `RecordMapChoice(Biome,int)`, `HasGeneratedWorldMap()`, `MarkWorldMapGenerated()`, `NeedsFirstForest()`, and read-only completed-biome/chosen-node accessors.
- Produces `BossExitPolicy GetBossExitPolicy(BossExitChoice)` where village return restores health/mana and continue restores neither.

- [ ] **Step 1: Write the failing rules test**

```cpp
#include "RunSession.h"
#include <cassert>
int main() {
    RunSession run;
    run.Begin();
    assert(run.IsActive());
    assert(run.NeedsFirstForest());
    assert(!run.HasGeneratedWorldMap());
    run.RecordBiomeClear(Biome::Forest);
    run.RecordMapChoice(Biome::Caverns, 2);
    run.MarkWorldMapGenerated();
    run.PauseInVillage();
    assert(run.IsPausedInVillage());
    assert(run.GetCompletedBiomes().front() == Biome::Forest);
    assert(run.GetChosenNodeIndices().front() == 2);
    assert(GetBossExitPolicy(BossExitChoice::ReturnToVillage).restoreHealth);
    assert(!GetBossExitPolicy(BossExitChoice::Continue).restoreMana);
}
```

- [ ] **Step 2: Compile to verify the test fails**

Run: `cl /std:c++17 /EHsc /I TestGame /I C:\CLibraries\raylib-5.5_win64_msvc16\include TestGame\RunSessionTests.cpp TestGame\RunSession.cpp /Fe:RunSessionTests.exe`
Expected: FAIL because `RunSession.h` does not exist.

- [ ] **Step 3: Implement the minimal run state**

```cpp
enum class BossExitChoice { ReturnToVillage, Continue };
struct BossExitPolicy { bool restoreHealth; bool restoreMana; bool pauseRun; };

class RunSession {
public:
    void Begin();
    void Reset();
    void PauseInVillage();
    void Resume();
    void RecordBiomeClear(Biome biome);
    void RecordMapChoice(Biome biome, int tierIndex);
    void MarkWorldMapGenerated();
    bool IsActive() const;
    bool IsPausedInVillage() const;
    bool HasGeneratedWorldMap() const;
    bool NeedsFirstForest() const;
    int GetWorldZone() const;
    const std::vector<Biome>& GetCompletedBiomes() const;
    const std::vector<int>& GetChosenNodeIndices() const;
};
BossExitPolicy GetBossExitPolicy(BossExitChoice choice);
```

- [ ] **Step 4: Compile and run the test**

Run: `RunSessionTests.exe`
Expected: exit code `0`.

- [ ] **Step 5: Add production files to the Visual Studio project and commit**

```powershell
git add TestGame/RunSession.h TestGame/RunSession.cpp TestGame/RunSessionTests.cpp TestGame/TestGame.vcxproj TestGame/TestGame.vcxproj.filters
git commit -m "feat: add persistent run session rules"
```

### Task 2: Class Unlock Rules and Profile Flags

**Files:**
- Create: `TestGame/ClassUnlockRules.h`
- Create: `TestGame/ClassUnlockRules.cpp`
- Create: `TestGame/ClassUnlockRulesTests.cpp`
- Modify: `TestGame/MetaProgression.h`
- Modify: `TestGame/MetaProgression.cpp`
- Modify: `TestGame/Engine.cpp` (`UpdateClassSelect`, `DrawClassSelect`)
- Modify: `TestGame/TestGame.vcxproj`
- Modify: `TestGame/TestGame.vcxproj.filters`

**Interfaces:**
- Produces `ClassUnlockStatus GetClassUnlockStatus(PlayerClass, const ClassUnlockProfile&)`.
- Adds persisted profile flags `onboarding_complete`, `rogue_unlocked`, `warlock_unlocked`, and `game_completed` with getters/setters on `MetaProgressionManager`.

- [ ] **Step 1: Write a failing class matrix test**

```cpp
ClassUnlockProfile fresh{};
assert(GetClassUnlockStatus(PlayerClass::Warrior, fresh).unlocked);
assert(GetClassUnlockStatus(PlayerClass::Hunter, fresh).unlocked);
assert(GetClassUnlockStatus(PlayerClass::Mage, fresh).unlocked);
assert(!GetClassUnlockStatus(PlayerClass::Rogue, fresh).unlocked);
assert(!GetClassUnlockStatus(PlayerClass::Warlock, fresh).unlocked);
assert(!GetClassUnlockStatus(PlayerClass::Paladin, fresh).unlocked);
fresh.gameCompleted = true;
assert(GetClassUnlockStatus(PlayerClass::Paladin, fresh).unlocked);
```

- [ ] **Step 2: Compile and verify failure**

Run: `cl /std:c++17 /EHsc /I TestGame TestGame\ClassUnlockRulesTests.cpp TestGame\ClassUnlockRules.cpp /Fe:ClassUnlockRulesTests.exe`
Expected: FAIL because the module does not exist.

- [ ] **Step 3: Implement exact lock copy and persistent fields**

```cpp
struct ClassUnlockProfile { bool rogueUnlocked{}; bool warlockUnlocked{}; bool gameCompleted{}; };
struct ClassUnlockStatus { bool unlocked{}; const char* reason{}; };
```

Use `"Complete Rogue milestone"`, `"Complete Warlock milestone"`, and `"Complete a run"` as the three locked-card messages. Parse absent config keys as `false` so old profiles remain valid.

- [ ] **Step 4: Gate class confirmation and draw a locked overlay**

On confirm, query the selected class. Locked cards play the existing menu-denied sound and remain on class select. Unlocked cards apply appearance/class and call the onboarding entry function introduced in Task 7.

- [ ] **Step 5: Run class tests and commit**

Run: `ClassUnlockRulesTests.exe`
Expected: exit code `0`.

```powershell
git add TestGame/ClassUnlockRules.* TestGame/ClassUnlockRulesTests.cpp TestGame/MetaProgression.* TestGame/Engine.cpp TestGame/TestGame.vcxproj TestGame/TestGame.vcxproj.filters
git commit -m "feat: gate classes by profile progression"
```

### Task 3: Fixed Village Layout Data

**Files:**
- Create: `TestGame/VillageLayoutData.h`
- Create: `TestGame/VillageLayoutData.cpp`
- Create: `TestGame/VillageLayoutDataTests.cpp`
- Create: `VillageAssets/VillageLayout.vlayout`
- Modify: `TestGame/TestGame.vcxproj`
- Modify: `TestGame/TestGame.vcxproj.filters`

**Interfaces:**
- Produces `VillageLayoutLoader::Load(path)` and `VillageLayoutLoader::Fallback()`.
- Layout entries expose `assetName`, `worldOrigin`, `scale`, and `permanent`.

- [ ] **Step 1: Write a failing parser/fallback test**

```cpp
VillageLayout layout = VillageLayoutLoader::Load("VillageAssets/VillageLayout.vlayout");
assert(layout.objects.size() == 2);
assert(layout.objects[0].assetName == "VillageGraveyard");
assert(layout.objects[1].assetName == "ZephsShop");
assert(layout.objects[0].permanent && layout.objects[1].permanent);
VillageLayout fallback = VillageLayoutLoader::Load("missing.vlayout");
assert(fallback.objects.size() == 2);
```

- [ ] **Step 2: Verify the test fails, then implement the parser**

The format is deliberately small and stable:

```text
size 1920 1080
spawn VillageGraveyard Respawn
exit 960 118 180 90
object VillageGraveyard 310 190 2.0 permanent
object ZephsShop 1190 210 2.0 permanent
```

Reject malformed lines individually, clamp scale to `0.25..8.0`, and return the same two-object fallback when no valid objects load.

- [ ] **Step 3: Compile and run the parser test**

Run: `cl /std:c++17 /EHsc /I TestGame /I C:\CLibraries\raylib-5.5_win64_msvc16\include TestGame\VillageLayoutDataTests.cpp TestGame\VillageLayoutData.cpp /Fe:VillageLayoutDataTests.exe && VillageLayoutDataTests.exe`
Expected: exit code `0`.

- [ ] **Step 4: Add project files and commit**

```powershell
git add TestGame/VillageLayoutData.* TestGame/VillageLayoutDataTests.cpp VillageAssets/VillageLayout.vlayout TestGame/TestGame.vcxproj TestGame/TestGame.vcxproj.filters
git commit -m "feat: define fixed village layout data"
```

### Task 4: Real Village Runtime

**Files:**
- Modify: `TestGame/Engine.h`
- Modify: `TestGame/Engine.cpp` (`EnterVillageShared`, `UpdateVillagePlayground`, `DrawVillagePlayground`, village collision helpers)

**Interfaces:**
- Consumes `VillageLayoutData` and existing `VillageAssetLoader` data.
- Produces `LoadFixedVillage()`, `VillageLayoutObjectToWorld(...)`, `GetFixedVillageColliders()`, and `TryInteractWithVillageService()` private engine helpers.

- [ ] **Step 1: Add a fixed-village geometry test beside `VillageLayoutDataTests.cpp`**

Assert marker conversion uses `worldOrigin + localMarker * scale`, authored collider conversion uses the same transform, and the Respawn/Poe/Zeph marker positions remain inside the 1920x1080 village bounds.

- [ ] **Step 2: Integrate the fixed layout without changing sandbox behavior**

For `sandboxMode == false`, load the Forest tiles exactly as one standard dungeon room, load both PNG assets from `VillageAssets`, populate collision rectangles from each `.vasset`, place the player at the graveyard Respawn marker, and disable `B`, catalog selection, placement ghosts, removal, and service-building tutorial prompts. For `sandboxMode == true`, keep every existing editor path unchanged.

- [ ] **Step 3: Restore Poe and Zeph service interactions**

Draw Poe at the graveyard `Poe` marker and Zeph at the shop `Zeph` marker. Interaction distance is measured from those world positions. Poe opens the existing meta screen; Zeph opens the existing shop. Neither NPC receives a hardcoded sprite offset beyond its authored marker.

- [ ] **Step 4: Make the north exit open or resume the world map**

Walking into the exit rectangle triggers without an `E` press after onboarding. First visits show `Head north when you are ready.`; paused runs call `RunSession::Resume()` and open the already-generated map state.

- [ ] **Step 5: Build and commit**

Run: `MSBuild.exe TestGame.sln /m /p:Configuration=Debug /p:Platform=x64`
Expected: build succeeds with zero errors.

```powershell
git add TestGame/Engine.h TestGame/Engine.cpp
git commit -m "feat: run the village from a fixed compact layout"
```

### Task 5: Empty Biome Entrances and Forest-First Run

**Files:**
- Modify: `TestGame/DungeonGen.h`
- Modify: `TestGame/DungeonGen.cpp`
- Create: `TestGame/DungeonGenEntranceTests.cpp`
- Modify: `TestGame/Engine.cpp` (`StartMainRun`, `EnterDungeonRoom`, room-entry cutscene/store branches)
- Modify: `TestGame/WorldMapManager.h`
- Modify: `TestGame/WorldMapManager.cpp`

**Interfaces:**
- Changes dungeon start room from `RoomType::Store` to an empty `RoomType::Standard` entrance identified by `DungeonGen::IsEntranceRoom(int)`.
- Changes world-map tier zero from Caverns to Forest and adds Caverns to the later selectable pool.

- [ ] **Step 1: Write the failing entrance test**

```cpp
DungeonGen gen;
gen.Generate();
int start = gen.GetStartIndex();
assert(gen.IsEntranceRoom(start));
assert(gen.GetRooms()[start].type == RoomType::Standard);
assert(gen.GetRooms()[start].hasNorth);
assert(!gen.GetRooms()[start].hasSouth);
```

- [ ] **Step 2: Implement an explicit entrance identity**

Store `int _entranceIdx` equal to `_startIdx`, expose `IsEntranceRoom`, keep its single north connection, and remove all Store-specific comments/logic. Engine room entry must mark this room cleared, spawn no enemies, skip Zeph/shop/Poe/wager setup, and close the entrance behind the player after moving north.

- [ ] **Step 3: Change first-run routing**

`StartMainRun()` calls `RunSession::Begin()`, sets `_currentBiome = Biome::Forest`, generates a dungeon, and enters its empty entrance. `WorldMapManager` renders Forest as completed tier zero and may offer Caverns in tiers 1-4. Do not allow Forest as the first post-Forest choice when another biome can fill the node.

- [ ] **Step 4: Compile/run the entrance test and build**

Run: `DungeonGenEntranceTests.exe`
Expected: exit code `0`.

Run: `MSBuild.exe TestGame.sln /m /p:Configuration=Debug /p:Platform=x64`
Expected: zero errors.

- [ ] **Step 5: Commit**

```powershell
git add TestGame/DungeonGen.* TestGame/DungeonGenEntranceTests.cpp TestGame/Engine.cpp TestGame/WorldMapManager.*
git commit -m "feat: start runs in forest with quiet biome entrances"
```

### Task 6: Boss Choice and World-Map Continuity

**Files:**
- Modify: `TestGame/Engine.h`
- Modify: `TestGame/Engine.cpp` (`OpenBossChoice`, `UpdateBossChoice`, `DrawBossChoice`, `OpenWorldMap`, `UpdateWorldMap`)

**Interfaces:**
- Consumes `BossExitPolicy` and `RunSession`.
- Produces `OpenWorldMap(bool regenerate)` and `ResumeWorldMapFromVillage()`.

- [ ] **Step 1: Add run-session assertions for pause/resume**

Snapshot completed biomes, selected tier indices, and world zone; pause and resume; assert all are identical. Assert village policy restores both resources and continue policy restores neither.

- [ ] **Step 2: Rework Return to Village**

Record the cleared biome before showing the choice. Return calls `_runSession.PauseInVillage()`, restores player health and mana to maximum through explicit Character APIs, and enters the fixed village. It must not call `ResetRunState`, `WorldMapManager::Reset`, or `WorldMapManager::Generate`.

- [ ] **Step 3: Rework Continue**

Remove the current 40% health/mana recovery. Keep the existing gold/reward multiplier that represents double-or-nothing, clear `_wagerAccessGranted`, and open the generated world map. Remove copy promising a breath, heal, Zeph room, or wager orb.

- [ ] **Step 4: Generate the map only when progression changes**

After a boss, call `Generate` once with the newly completed path, then mark the run session map generated. A village resume reuses the manager's nodes and selection state; it only resets input/fade presentation via a new `WorldMapManager::Reopen()` method.

- [ ] **Step 5: Build, manually test both choices, and commit**

Verify Return gives full HP/mana and resumes the same node choices; Continue gives no HP/mana. Then commit only the flow files.

### Task 7: Prologue State Machine and Combat Rooms

**Files:**
- Create: `TestGame/PrologueController.h`
- Create: `TestGame/PrologueController.cpp`
- Create: `TestGame/PrologueControllerTests.cpp`
- Modify: `TestGame/GameTypes.h`
- Modify: `TestGame/Engine.h`
- Modify: `TestGame/Engine.cpp` (update/draw switches, class confirmation, death routing)
- Modify: `TestGame/TestGame.vcxproj`
- Modify: `TestGame/TestGame.vcxproj.filters`

**Interfaces:**
- Produces `ProloguePhase`, `PrologueEvent`, and `PrologueController::Update(const PrologueInput&)`.
- Engine adapters spawn enemies, detect correct actions, restore tutorial health, and draw dynamic prompts.

- [ ] **Step 1: Write the failing state-machine test**

```cpp
PrologueController p;
p.Begin();
assert(p.GetPhase() == ProloguePhase::BasicAttack);
p.Update({ .basicAttackPressed = true });
p.Update({ .roomCleared = true });
assert(p.GetPhase() == ProloguePhase::Ability);
p.Update({ .abilityPressed = true, .roomCleared = true });
p.Update({ .dashPressed = true, .roomCleared = true });
assert(p.GetPhase() == ProloguePhase::LastStand);
p.Update({ .playerHit = true });
p.Update({ .playerHit = true });
assert(!p.IsScriptedDeathReady());
p.Update({ .playerHit = true });
assert(p.IsScriptedDeathReady());
```

- [ ] **Step 2: Implement phases and reward suppression**

Use phases `BasicAttack`, `Ability`, `Dash`, `LastStand`, `FirstDeath`, `PoeDialogue`, `Complete`. Expose `ShouldSuppressRewards()` for every phase before Complete and `ShouldAutoRestoreOnLethalHit()` in the first three teaching phases.

- [ ] **Step 3: Build the fixed three-room Forest arena in Engine**

Use standard Forest room dimensions and borders. Room 1 spawns two basic melee grunts. Room 2 spawns only basic melee grunts. The last stand creates a capped ring of ranged enemies around the player; dead members respawn at free ring slots and a full clear immediately rebuilds the ring. Clear projectiles/enemies when leaving the prologue.

- [ ] **Step 4: Add dynamic control prompts**

Resolve keyboard/gamepad/touch labels through existing prompt helpers. Room 1 requires the bound basic attack and a clear. Room 2 requires the first learned ability, then dash, and a clear. Prompt copy includes the selected class and actual ability name. The first two rooms restore to exactly 3 HP on lethal damage; the last stand counts three one-damage hits and triggers scripted death.

- [ ] **Step 5: Assign one canonical starter ability per starter class**

Use `WarCleave` for Warrior, `PiercingShot` for Hunter, and `FireBolt` for Mage. Learn it before the prologue and retain it into the first real run. Do not open the legacy starting-ability gift in biome entrances.

- [ ] **Step 6: Compile tests, build, and commit**

Run: `PrologueControllerTests.exe`
Expected: exit code `0`.

Run: `MSBuild.exe TestGame.sln /m /p:Configuration=Debug /p:Platform=x64`
Expected: zero errors.

### Task 8: First Poe Resurrection, Zeph Introduction, and Debug Skip

**Files:**
- Modify: `TestGame/Engine.h`
- Modify: `TestGame/Engine.cpp`
- Modify: `TestGame/MetaProgression.h`
- Modify: `TestGame/MetaProgression.cpp`

**Interfaces:**
- Consumes `PrologueController`, fixed village runtime, and `MetaProgressionManager::HasCompletedOnboarding()`.
- Produces `StartOnboardingOrRun()`, `BeginFirstPoeResurrection()`, `BeginFirstVillageVisit()`, and `SkipOnboardingForDebug()`.

- [ ] **Step 1: Route class confirmation through onboarding state**

Fresh profiles start the prologue. Completed profiles start in the fixed village with the selected class. Debug skip is available only while debug mode is active and visibly shows its non-conflicting key in the upper-right; it skips prologue rooms, scripted death, Poe intro, and Zeph intro while preserving class/appearance.

- [ ] **Step 2: Implement the first death cinematic**

Freeze combat, lower music, hold the body briefly, reveal Poe, and use these lines exactly:

```text
Hm. You're stronger than you look. Could've been stronger, with proper guidance.
Guess you'll do.
I can bring you back. In exchange, you're going to work for me.
```

Use a purple full-screen transition, then enter the village at the graveyard Respawn marker. Do not apply the ordinary death penalty or regular Poe death dialogue.

- [ ] **Step 3: Implement Zeph's first-visit dialogue**

After Poe's sequence, guide the player to Zeph and use these lines exactly:

```text
Oh. You must be new here.
You'll find sanctuary here, at least for now. The monsters tend to keep their distance.
If you've got coin, I can sell you supplies. Might even teach you a few abilities...
For the right price, of course.
```

Zeph never references Poe. Poe remains excluded from citizen perception/update data. After dialogue, mark onboarding complete, save the profile, and show `Head north when you are ready.`

- [ ] **Step 4: Preserve ordinary later deaths**

When onboarding is complete, use the existing short Poe death/revival path and death penalty unchanged, returning to the graveyard marker. Never replay tutorial rooms or either introduction.

- [ ] **Step 5: Full verification**

Run all four standalone tests, then build `Debug|x64`. Manually verify: fresh starter roster, locked cards, tutorial recovery, scripted third hit, Poe dialogue, purple transition, fixed village collision/NPC markers, Zeph dialogue, north exit, Forest first, quiet biome entry, boss Return continuity/full restore, Continue/no restore, and one-time onboarding persistence.

- [ ] **Step 6: Final commit**

```powershell
git add TestGame/Engine.* TestGame/MetaProgression.*
git commit -m "feat: add one-time prologue and first village onboarding"
```

## Self-Review Results

- Spec coverage: every approved requirement is assigned to Tasks 1-8; later safe-room save serialization and roaming villagers remain intentionally outside this implementation.
- Placeholder scan: no `TBD`, `TODO`, generic error-handling instruction, or undefined later-task interface remains.
- Type consistency: `RunSession`, `BossExitPolicy`, `VillageLayoutLoader`, `ClassUnlockStatus`, and `PrologueController` names/signatures are consistent across producing and consuming tasks.
