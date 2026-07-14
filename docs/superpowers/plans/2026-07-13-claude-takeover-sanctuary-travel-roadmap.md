# Mystic Onslaught Sanctuary Travel Takeover Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Continue from the completed onboarding work and replace the old post-boss village-return flow with a reliable forward journey through destination selection, handcrafted sanctuaries, Poe's stackable Double Down wager, and later forward-only quests.

**Architecture:** Preserve `Engine` as the screen/state coordinator, but put durable travel rules in `RunSession` so the map, village, dungeon, boss choice, death flow, and future physical exits all consume the same state. The existing world map is a temporary destination chooser. The existing fixed village is a temporary sanctuary layout. Do not build unique settlements, NPC schedules, or physical route exits until the core travel state passes automated and manual tests.

**Tech Stack:** C++17, raylib 5.5, Visual Studio/MSBuild `Debug|x64`, standalone assertion tests compiled with `cl.exe`.

## Global Constraints

- Preserve all current user, Claude, and Codex changes. The working tree contains unrelated enemy edits; never reset, revert, overwrite, or broadly reformat them.
- Do not redo the prologue unless a manual playthrough exposes a specific defect.
- Do not revive the player-facing village builder. Real sanctuaries are handcrafted and deterministic.
- Keep the village playground/editor available as developer tooling until a separate cleanup is approved.
- Zeph is a travelling merchant present in every sanctuary. Real progression must not require the permanent Zeph shop building.
- Poe is visible only to the player. Villagers do not know Poe exists and do not react to him.
- The first real dungeon is always the Forest. Caverns joins the later destination pool.
- Player-facing text may name a location directly (`Forest`, `Caverns`, `Ancient Castle`) or use `region` generically. Never show `biome` to the player.
- Sanctuary restores health and mana. Double Down restores neither.
- Double Down does not immediately grant gold. It doubles all carried gold exactly once after the next destination boss is defeated.
- Consecutive successful Double Downs stack the displayed streak (`x2`, `x4`, `x8`) and each success doubles the current carried gold once.
- Entering a sanctuary keeps already-earned doubled gold but ends the wager streak.
- Death clears carried run gold under the existing death rule and resets all wager state.
- New destination entry rooms remain quiet: no Zeph, Poe, wager orb, shop, enemies, or introductory gift.
- Build `Debug|x64` and run focused tests after every phase.

---

## Read First

These files define the approved direction and current implementation:

- `docs/superpowers/specs/2026-07-13-region-sanctuary-traversal-design.md`: authoritative travel design.
- `docs/superpowers/plans/2026-07-13-prologue-village-run-flow.md`: earlier implementation plan; many tasks are already complete.
- `docs/superpowers/specs/2026-07-13-prologue-dialogue-and-last-stand-polish-design.md`: completed tutorial polish.
- `TestGame/RunSession.h` and `TestGame/RunSession.cpp`: current minimal run-state module to extend.
- `TestGame/Engine.cpp`: current orchestration for onboarding, village, boss choice, map, death, and dungeons.
- `TestGame/WorldMapManager.h` and `TestGame/WorldMapManager.cpp`: temporary destination chooser.
- `TestGame/VillageLayoutData.h`, `TestGame/VillageLayoutData.cpp`, and `VillageAssets/VillageLayout.vlayout`: deterministic placeholder sanctuary.
- `TestGame/PrologueController.h` and `TestGame/PrologueController.cpp`: implemented tutorial state machine.

`MYSTIC_ONSLAUGHT_CURRENT_VERSION.txt` is stale where it describes a permanent player-built home village. Do not use that village section as authority. The region-and-sanctuary specification supersedes it.

## Current Implementation Snapshot

### Already Implemented - Preserve It

- A one-time, three-room Forest prologue.
- Starter ability assignment and class-aware tutorial prompts.
- Tutorial prompts showing actual basic-attack and ability names.
- Tutorial-only Skeleton Archer relentless-fire mode with staggered opening shots.
- Scripted three-hit last stand and first Poe resurrection.
- Five-line first Poe conversation.
- Dialogue drawn above gameplay and village HUD elements.
- Debug onboarding skip.
- Starter class lock rules and onboarding persistence.
- Forest-first main run.
- Quiet dungeon entrance rooms.
- Fixed deterministic village layout using the graveyard and Zeph assets.
- Poe and Zeph marker-based placement and existing service interactions.
- Basic `RunSession` pause/resume data and map reopening support.

### Current Gaps and Contradictions

- `Engine::UpdateBossChoice()` still enters the village immediately for the left choice instead of selecting the next destination first.
- The right choice still grants an immediate bravery bounty. That contradicts the approved wager.
- `Engine::UpdateWorldMap()` immediately loads the selected dungeon. It cannot route the player through a sanctuary first.
- There is no `TravelMode`, committed `pendingRegion`, active wager, payout guard, or wager streak.
- The fixed village gate resumes/reopens the map instead of entering an already-selected destination.
- Boss-choice comments and copy still describe the retired village-builder/wager-shrine behavior.
- The legacy cursed wager shrine/orb is still represented in `Engine`. It must not participate in the new travel wager.
- The world map visibly says `Select a biome to venture into next`.
- The old current-version document still describes the retired home-village-builder loop.

---

### Task 1: Protect the Baseline and Validate the Completed Prologue

**Files:**
- Inspect: `TestGame/PrologueController.cpp`
- Inspect: `TestGame/SkeletonArcher.cpp`
- Inspect: `TestGame/Engine.cpp`
- Test: `TestGame/PrologueControllerTests.cpp`

**Interfaces:**
- Consumes the current prologue implementation without changing its public API.
- Produces a written defect list only if the manual playthrough exposes a reproducible tutorial problem.

- [ ] **Step 1: Record the dirty working tree before editing**

Run:

```powershell
git status --short
git diff --name-only
```

Expected: several existing modified enemy and tutorial files may appear. Treat all of them as user work. Do not clean the tree.

- [ ] **Step 2: Run the focused prologue test**

Use a Visual Studio x64 developer prompt:

```bat
cl /nologo /std:c++17 /EHsc /DPROLOGUE_CONTROLLER_TEST_MAIN /I TestGame TestGame\PrologueController.cpp TestGame\PrologueControllerTests.cpp /Fe:%TEMP%\PrologueControllerTests.exe
%TEMP%\PrologueControllerTests.exe
```

Expected: exit code `0`.

- [ ] **Step 3: Manually play the complete onboarding once**

Verify all of the following in one uninterrupted run:

```text
Room 1: correct bound attack control and class-specific attack name
Room 2: correct first ability name and dash prompt
Early lethal damage: tutorial restores the player instead of ending the run
Final room: archers form pressure around the player and repeatedly fire
Third final-room hit: scripted death begins
Poe: all five lines advance and remain above other UI
Village: Zeph introduction appears above the village HUD
Skip: debug onboarding skip reaches the ready village
```

- [ ] **Step 4: Fix only reproducible onboarding defects**

Do not rewrite tutorial wording, enemy behavior, or state flow based on preference. If a defect appears, add a focused assertion to `PrologueControllerTests.cpp` when it is a pure rule; otherwise document the exact reproduction and make the narrowest integration fix.

- [ ] **Step 5: Build before beginning travel work**

Run:

```bat
MSBuild.exe TestGame.sln /m /p:Configuration=Debug /p:Platform=x64
```

Expected: zero errors. Existing warnings may remain, but record any new warning introduced by later phases.

---

### Task 2: Extend RunSession With Explicit Travel State

**Files:**
- Modify: `TestGame/RunSession.h`
- Modify: `TestGame/RunSession.cpp`
- Modify: `TestGame/RunSessionTests.cpp`

**Interfaces:**
- Produces `enum class TravelMode { None, Sanctuary, DoubleDown };`.
- Produces `RunSession::ChooseTravelMode(TravelMode)`.
- Produces `RunSession::CommitDestination(Biome, int)`.
- Produces `RunSession::HasPendingRegion()`, `GetPendingRegion()`, and `ConsumePendingRegion()`.
- Produces `RunSession::IsWagerActive()`, `GetWagerStreak()`, and `GetWagerDisplayMultiplier()`.
- Produces `WagerResolution RunSession::ResolveBossWager(int carriedGold)`.
- Produces `RunSession::ClearWagerOnDeath()`.

- [ ] **Step 1: Add failing travel-state assertions**

Add these cases to `RunSessionTests.cpp` before implementation:

```cpp
RunSession travel;
travel.Begin();
travel.ChooseTravelMode(TravelMode::Sanctuary);
travel.CommitDestination(Biome::Caverns, 1);
assert(travel.GetTravelMode() == TravelMode::Sanctuary);
assert(travel.HasPendingRegion());
assert(travel.GetPendingRegion() == Biome::Caverns);
assert(!travel.IsWagerActive());
assert(travel.ConsumePendingRegion() == Biome::Caverns);
assert(!travel.HasPendingRegion());

travel.ChooseTravelMode(TravelMode::DoubleDown);
travel.CommitDestination(Biome::AncientCastle, 2);
assert(travel.IsWagerActive());
WagerResolution first = travel.ResolveBossWager(170);
assert(first.paid);
assert(first.goldAfter == 340);
assert(first.streak == 1);
assert(first.displayMultiplier == 2);

travel.ChooseTravelMode(TravelMode::DoubleDown);
travel.CommitDestination(Biome::DreamRealm, 0);
WagerResolution second = travel.ResolveBossWager(400);
assert(second.paid);
assert(second.goldAfter == 800);
assert(second.streak == 2);
assert(second.displayMultiplier == 4);

WagerResolution duplicate = travel.ResolveBossWager(800);
assert(!duplicate.paid);
assert(duplicate.goldAfter == 800);

travel.ChooseTravelMode(TravelMode::Sanctuary);
assert(travel.GetWagerStreak() == 0);
travel.ClearWagerOnDeath();
assert(!travel.IsWagerActive());
```

- [ ] **Step 2: Compile and confirm the test fails**

Run:

```bat
cl /nologo /std:c++17 /EHsc /I TestGame TestGame\RunSession.cpp TestGame\RunSessionTests.cpp /Fe:%TEMP%\RunSessionTests.exe
```

Expected: compilation fails because travel types and methods do not exist.

- [ ] **Step 3: Add the exact state model**

Add the following durable state to `RunSession`:

```cpp
enum class TravelMode { None, Sanctuary, DoubleDown };

struct WagerResolution
{
    bool paid = false;
    int goldAfter = 0;
    int streak = 0;
    int displayMultiplier = 1;
};

TravelMode _travelMode = TravelMode::None;
bool _hasPendingRegion = false;
Biome _pendingRegion = Biome::Forest;
int _pendingTierIndex = 0;
bool _wagerActive = false;
bool _wagerPaidForCurrentBoss = false;
int _wagerStreak = 0;
```

Rules:

```text
Choose Sanctuary: reset the wager streak and active wager; keep previously earned gold untouched.
Choose DoubleDown: do not arm the wager until CommitDestination succeeds.
CommitDestination: store the destination and tier; DoubleDown arms the next-boss wager.
ConsumePendingRegion: return the committed destination and clear only the pending flag.
ResolveBossWager: double once when active and unpaid; then disarm it and guard duplicate resolution.
Gold doubling: calculate through int64_t and clamp to INT_MAX before returning int.
Begin/Reset/death: clear pending travel and wager state.
```

- [ ] **Step 4: Run the focused test**

Run `%TEMP%\RunSessionTests.exe`.

Expected: exit code `0`.

- [ ] **Step 5: Review the module boundary**

`RunSession` must not call raylib drawing functions, manipulate `Character`, switch `GameState`, or access `Engine`. It owns decisions and durable values only.

---

### Task 3: Rework Poe's Post-Boss Choice

**Files:**
- Modify: `TestGame/Engine.h`
- Modify: `TestGame/Engine.cpp` (`OpenBossChoice`, `UpdateBossChoice`, `DrawBossChoice`)

**Interfaces:**
- Consumes `RunSession::ResolveBossWager()` before presenting the next offer.
- Consumes `RunSession::ChooseTravelMode()` after player confirmation.
- Both choices open the destination map; neither choice enters a sanctuary or dungeon immediately.

- [ ] **Step 1: Resolve an arriving wager exactly once**

At boss completion, before the next choice is interactive:

```cpp
WagerResolution payout = _runSessionData.ResolveBossWager(_player.GetGold());
if (payout.paid)
{
    _player.SetGold(payout.goldAfter);
    // Show a short Poe payout banner and use the existing reward sound/VFX.
}
```

If `Character` does not expose `SetGold`, add the smallest safe setter or apply the positive difference with `AddGold`. Do not call `AddGold` twice on repeated boss-screen updates.

- [ ] **Step 2: Replace the old left-card action**

The left card becomes `Seek Sanctuary`. Confirmation must:

```cpp
_runSessionData.ChooseTravelMode(TravelMode::Sanctuary);
OpenWorldMap();
```

Do not call `EnterVillage()` here. Destination selection happens first.

- [ ] **Step 3: Replace the old right-card action**

The right card becomes `Double Down`. Confirmation must:

```cpp
_runSessionData.ChooseTravelMode(TravelMode::DoubleDown);
OpenWorldMap();
```

Delete the immediate `50 + _worldZone * 50` bravery bounty from this path. Do not restore health or mana. Do not grant access to the old cursed wager shrine.

- [ ] **Step 4: Replace retired copy and comments**

Use concise player-facing meaning:

```text
SEEK SANCTUARY
Choose the road ahead.
Rest before entering the next region.
Restore health and mana.
Visit Zeph and sanctuary services.
Ends Poe's wager streak.

DOUBLE DOWN
Choose the road ahead.
Bypass its sanctuary.
No healing. No merchant. No services.
Defeat its boss to double all carried gold.
Death loses the wager and carried gold.
```

Poe may frame the screen with:

```text
There is a refuge ahead. Beds, merchants, all those little comforts the living cling to.
Reach the next refuge without taking anything from it, and I will double the gold tied to your soul.
```

- [ ] **Step 5: Build and manually inspect both cards**

Verify keyboard, mouse, and controller navigation; legible text; no immediate bounty; and both choices opening the map.

---

### Task 4: Commit the Destination Before Transitioning

**Files:**
- Modify: `TestGame/Engine.h`
- Modify: `TestGame/Engine.cpp` (`UpdateWorldMap`, village gate handling, dungeon-start helper)
- Modify only if necessary: `TestGame/WorldMapManager.h`
- Modify only if necessary: `TestGame/WorldMapManager.cpp`

**Interfaces:**
- Produces private `Engine::EnterCommittedRegion()`.
- Consumes `RunSession::CommitDestination()` and `ConsumePendingRegion()`.
- World-map confirmation no longer assumes every choice goes directly to a dungeon.

- [ ] **Step 1: Extract one dungeon-launch helper**

Create a private helper with this responsibility:

```cpp
void Engine::EnterCommittedRegion()
{
    if (!_runSessionData.HasPendingRegion())
    {
        OpenWorldMap();
        return;
    }

    _currentBiome = _runSessionData.ConsumePendingRegion();
    LoadTilesetForBiome(_currentBiome);
    _dungeonGen.Generate();
    EnterDungeonRoom(_dungeonGen.GetStartIndex(), DungeonDoorSide::None,
                     GetDungeonBottomSpawnPos(), true);
    // Initialize the existing dungeon fade-in state.
}
```

Keep `Biome` as the internal type. The function name uses `Region` because that is the player-facing design concept.

- [ ] **Step 2: Change map confirmation into a committed decision**

After `WorldMapManager::Update()` confirms a selection:

```cpp
_worldZone++;
_worldCompletedBiomes.push_back(selectedBiome);
_worldChosenNodeIndices.push_back(selectedTierIdx);
_runSessionData.RecordMapChoice(selectedBiome, selectedTierIdx);
_runSessionData.CommitDestination(selectedBiome, selectedTierIdx);
_worldMapPreparedZone = -1;
```

Then branch by the chosen mode:

```text
Sanctuary: fully restore health and mana once, then EnterVillage().
DoubleDown: EnterCommittedRegion() immediately with no restoration.
None/invalid: return safely to BossChoice or WorldMap; never silently load Forest.
```

- [ ] **Step 3: Make the sanctuary gate consume the destination**

In the real, non-sandbox village gate path:

```text
If RunSession has a pending region: call EnterCommittedRegion().
Else if this is the first post-onboarding village: start the mandatory Forest run.
Else: show a safe message and open destination selection; do not generate a random dungeon.
```

The gate must never reopen the map after a destination has already been selected.

- [ ] **Step 4: Preserve the first-run exception**

The first village visit still exits into the Forest. It does not ask for a destination, does not activate a wager, and does not increment the later world-map route twice.

- [ ] **Step 5: Add integration assertions where feasible**

At minimum, expand `RunSessionTests.cpp` to prove that a committed destination survives a sanctuary pause/resume and can only be consumed once.

- [ ] **Step 6: Manually test both routes**

```text
Forest boss -> Seek Sanctuary -> choose Caverns -> placeholder sanctuary -> north gate -> Caverns
Forest boss -> Double Down -> choose Caverns -> Caverns immediately
```

Verify that the map appears once in each route and the chosen destination never changes during the sanctuary visit.

---

### Task 5: Make the Placeholder Village a Real Sanctuary

**Files:**
- Modify: `TestGame/Engine.cpp` (`EnterVillageShared`, village update/draw, Zeph setup)
- Modify if needed: `VillageAssets/VillageLayout.vlayout`
- Preserve: `TestGame/VillageLayoutData.*`
- Preserve: `TestGame/VillageAssetData.*`

**Interfaces:**
- Consumes the existing fixed layout and authored `.vasset` markers/colliders.
- Provides a deterministic safe stop between the destination choice and the committed dungeon.

- [ ] **Step 1: Separate developer playground from real sanctuary rules**

`EnterVillagePlayground()` remains a debug sandbox. `EnterVillage()` becomes the real sanctuary. Build controls, placement ghosts, deletion, and catalog UI must never be active in the real sanctuary.

- [ ] **Step 2: Restore resources only for valid sanctuary arrival**

The existing `EnterVillageShared()` currently restores resources broadly. Gate restoration so it occurs for:

```text
first onboarding arrival
ordinary death recovery
Seek Sanctuary travel
```

Do not let a debug transition or accidental repeated entry create unintended run benefits. Use `_player.Heal(ceil(maxHealth - currentHealth))` and `_player.RestoreMana(_player.GetMaxMana())` through existing APIs.

- [ ] **Step 3: Treat Zeph as travelling, not owned infrastructure**

Always place Zeph at a reachable authored/fallback sanctuary marker and open the existing shop through normal interaction. The sanctuary must not check whether a player-built shop exists. The old `ZephsShop` PNG can remain as the temporary stall art and in sandbox tools.

- [ ] **Step 4: Preserve Poe's special presentation rule**

Poe remains at the graveyard marker for the player's screen. Future villagers must not target Poe, greet Poe, path around Poe as an NPC, or mention Poe in ordinary dialogue. It is acceptable for later dialogue to make villagers think the player is speaking to empty space.

- [ ] **Step 5: Clarify the exit prompt**

When a destination is committed, show `Continue to <Destination>` near the north exit. For the first visit, show `Enter the Forest`. Do not say `Return to map`.

---

### Task 6: Complete the Wager Economy, HUD, and Failure Rules

**Files:**
- Modify: `TestGame/Engine.h`
- Modify: `TestGame/Engine.cpp` (HUD, boss payout feedback, player death cleanup)
- Test: `TestGame/RunSessionTests.cpp`

**Interfaces:**
- Consumes `RunSession` wager state.
- Produces no second wager state inside `Engine`; presentation reads from the rule module.

- [ ] **Step 1: Add the active-wager HUD label**

While an active Double Down leg is underway, draw:

```text
POE'S WAGER x2
```

After one successful consecutive wager and accepting another, draw `x4`; then `x8`. Place it near run resources without covering class mechanics, armor, XP, abilities, room labels, or damage numbers. Give it a dark outline and readable high-contrast text.

- [ ] **Step 2: Add payout feedback**

On the boss clear that resolves the wager, show the old and new gold totals and the new streak. Use one short sound/VFX response. Do not multiply during drawing, repeated updates, pause reopening, or transition callbacks.

- [ ] **Step 3: Reset the wager through the central death path**

In the same path that applies the existing carried-gold death penalty, call:

```cpp
_runSessionData.ClearWagerOnDeath();
```

Do this once before entering Poe's ordinary resurrection flow.

- [ ] **Step 4: Isolate the legacy cursed wager shrine**

The travel wager must not set `_wagerAccessGranted`, `_wagerTier`, `_curseShrineUsed`, or any old shrine multiplier. Remove the orb/shrine from normal region-entry and sanctuary flow. Do not delete all legacy code in this phase unless it is unreachable and deletion is separately reviewed.

- [ ] **Step 5: Test duplicate payout and overflow**

Add assertions proving:

```text
one boss clear pays once
reopening the boss screen does not pay again
two completed legs produce two doublings, not four on the second boss
Sanctuary resets streak without removing earned gold
death clears active/streak state
large gold totals clamp instead of overflowing negative
```

---

### Task 7: Terminology and Transition Cleanup

**Files:**
- Modify: `TestGame/WorldMapManager.cpp`
- Modify: touched player-facing strings in `TestGame/Engine.cpp`
- Do not rename internal `Biome` types, fields, functions, assets, or serialized values.

**Interfaces:**
- No API changes required.

- [ ] **Step 1: Find visible uses of biome**

Run:

```powershell
rg -n 'biome' TestGame/WorldMapManager.cpp TestGame/Engine.cpp
```

Classify each result as an internal comment/identifier or a quoted player-facing string.

- [ ] **Step 2: Replace only player-facing strings**

Examples:

```text
Select a biome to venture into next -> Choose your next destination
New biome -> New region
Biome complete -> <Destination> cleared
```

Prefer the actual destination name whenever context provides it.

- [ ] **Step 3: Verify quiet region entrances**

Every selected dungeon must begin in the existing empty entrance room. Confirm no Zeph, Poe, wager object, enemies, shop prompt, or old starting-ability gift appears there.

- [ ] **Step 4: Update the current-version document after code is stable**

Replace the stale village-builder sections of `MYSTIC_ONSLAUGHT_CURRENT_VERSION.txt` with the sanctuary-chain direction. Do not delete historical specs that still explain implemented systems.

---

### Task 8: Full Core-Flow Verification

**Files:**
- Test: `TestGame/RunSessionTests.cpp`
- Test: `TestGame/PrologueControllerTests.cpp`
- Test: `TestGame/DungeonGenEntranceTests.cpp`
- Test: `TestGame/VillageLayoutDataTests.cpp`

**Interfaces:**
- No new production interface.

- [ ] **Step 1: Run all focused rule tests**

Compile and run the standalone test executables through the x64 Visual Studio developer environment. Expected: every executable exits `0`.

- [ ] **Step 2: Build the complete game**

Run:

```bat
MSBuild.exe TestGame.sln /m /p:Configuration=Debug /p:Platform=x64
```

Expected: zero errors and no new warnings introduced by these tasks.

- [ ] **Step 3: Execute the sanctuary route matrix**

```text
Fresh save -> prologue -> Poe -> first village -> Forest
Forest clear -> Sanctuary -> choose Caverns -> village -> Caverns
Forest clear -> Double Down -> choose Caverns -> direct Caverns, no recovery
Double Down success -> carried gold doubles once
Two Double Downs -> second boss doubles current gold and HUD shows x4
Successful wager -> Sanctuary -> earned gold remains, streak clears
Active wager -> death -> carried gold and streak clear
Sanctuary gate -> committed destination, never a regenerated map
Invalid pending destination -> safe chooser fallback, never silent Forest
Final region -> active wager resolves before victory processing
```

- [ ] **Step 4: Verify input and presentation**

Test keyboard, mouse, and controller on boss choice, map, sanctuary interactions, and exit. Confirm all dialogue remains above HUD and the wager indicator does not overlap class-specific bars.

- [ ] **Step 5: Review the final diff by subsystem**

Run:

```powershell
git diff --check
git diff --stat
git diff -- TestGame/RunSession.h TestGame/RunSession.cpp TestGame/RunSessionTests.cpp
git diff -- TestGame/Engine.h TestGame/Engine.cpp TestGame/WorldMapManager.cpp
```

Confirm unrelated enemy, combat, sound, VFX, shop, and editor changes have not been reverted or reformatted.

---

## Deferred Phase A: Replace the Static Map With Physical Route Choices

Do this only after Tasks 1-8 are stable.

- Preserve `TravelMode`, `pendingRegion`, and wager state exactly; replace only how the destination is selected.
- Present up to three geographically appropriate exits after a boss or inside the preceding sanctuary.
- Exit examples include roads, gates, boats, cave mouths, lifts, and portals.
- Each exit shows destination name, danger emphasis, reward tendency, and any passenger request.
- The route generator must avoid impossible progression, duplicates, and implausible choices.
- Selecting an exit calls the same `CommitDestination(Biome, tierIndex)` used by the temporary map.
- Do not delete `WorldMapManager` until physical exits pass the same routing matrix.

## Deferred Phase B: Unique Handcrafted Sanctuaries

Build one sanctuary at a time, starting with Forest.

- Each sanctuary is deterministic and authored, not procedurally assembled.
- Each supports arrival, Zeph, Poe presentation where appropriate, a forward exit, service markers, NPC spawn markers, and collision.
- Settlement stages are `Unstable`, `Secured`, and `Thriving`.
- Growth is persistent authored state, not freeform building.
- Planned content includes a graveyard, travelling Zeph, bestiary/Beast Post, three or four villager houses, five or six visible residents, and later interior NPCs.
- NPC schedules should use simple destinations and time/state blocks, not full simulation.

## Deferred Phase C: Forward-Only Side Quests and Passengers

Implement only after at least two destination sanctuaries exist.

- Quests always point forward; never require returning to the previous region.
- Valid objectives include rescuing villagers, recovering medicine/tools, defeating an optional elite, destroying hazard sources, protecting an evacuation point, or recovering records.
- Do not add trapped-beast rescue quests. Beast acquisition belongs to research/taming progression.
- Rescued passengers become abstract run state rather than physical combat followers.
- A generic rescued NPC can leave at the next sanctuary for a modest reward.
- A destination-specific passenger guarantees their requested destination among future route choices when valid.
- Choosing another route leaves the passenger safely behind and grants no reward or punishment.

## Deferred Phase D: Beast Post and Companion Continuity

- The Forest sanctuary eventually owns the main ranch/bestiary.
- Completing research challenges unlocks a tameable version of that monster.
- Every later sanctuary can provide a Beast Post connected to the ranch.
- The player chooses one available companion before entering a destination.
- A defeated companion returns to the ranch, becomes temporarily unavailable, and leaves the player alone until another sanctuary.
- Double Down therefore also risks losing the ability to replace a defeated companion.
- Companion health bars display `Friend`; they must never be confused with hostile monsters.

## Final Scope Guard

Finish the core travel loop before expanding content. The correct implementation order is:

```text
protect existing work
-> validate tutorial
-> central travel state
-> Poe choice
-> committed destination
-> sanctuary routing
-> wager payout/HUD/death
-> terminology and transition cleanup
-> complete verification
-> physical routes
-> unique sanctuaries
-> quests/passengers
-> beast continuity
```

Do not combine this work with combat balance, enemy behavior, VFX replacement, shop-wares redesign, village-editor cleanup, or asset-pipeline refactors. Those are separate systems and currently have unrelated changes in progress.
