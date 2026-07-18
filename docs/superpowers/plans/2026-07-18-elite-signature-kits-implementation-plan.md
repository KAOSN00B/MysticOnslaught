# Elite Signature Kits Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the five homogenized elite bruisers with readable signature attacks, fair 50%-health escalations, compatibility-aware room modifiers, authored VFX/audio, and reusable encounter primitives for later Diablo-style boss survival phases.

**Architecture:** Add a small, allocation-free elite action clock and bounded signature-event queue, then let each elite own its behavior while `CombatDirector` owns shared attack zones, damage/status resolution, VFX, and feedback. Centralize elite-room initialization in `Engine` so main runs, handcrafted dungeon runs, room re-entry, snapshots, and debug restarts all apply the same modifier rules.

**Tech Stack:** C++17, raylib 5.5, Visual Studio/MSBuild `Debug|x64`, assertion-based standalone tests, existing `AttackTuningStore`, `AttackEditor`, `VFXManager`, `SfxBank`, and pooled enemy storage.

## Global Constraints

- Preserve all existing user and Claude changes; do not touch village or room-editor behavior except for build-file registration required by new elite files.
- No elite or room modifier may grant complete invulnerability. Guard Links applies exactly `60%` damage reduction, so linked elites receive `40%` of resolved incoming damage with a minimum of `1` damage for a positive hit.
- Remove the generic elite lunge and the generic Leap room modifier. Stormclub alone owns the new leap behavior.
- Every dangerous action follows Telegraph -> Lock -> Execution -> Recovery and remains damageable throughout.
- Every elite crosses its escalation threshold once at `50%` maximum health; recovery windows remain present in phase two.
- Warnings may use simple Raylib targeting geometry. Active attacks and persistent hazards must use animated sprite VFX.
- Signature runtime queues and attack-zone pools are bounded and cleared on spawn reset, death, room exit, snapshot restore, and pool reuse.
- Missing attack tuning, VFX, or audio must fall back safely without crashing or suppressing gameplay.
- Build `Debug|x64` and test all five elites with every compatible modifier in both open and constrained handcrafted rooms.

---

## File Structure

- Create `TestGame/EliteSignature.h`: shared enums, `EliteActionClock`, bounded event queue, modifier compatibility, attack-zone data, and pure geometry/damage helpers.
- Create `TestGame/EliteSignature.cpp`: implementation of pure elite primitives and modifier selection.
- Create `TestGame/EliteSignatureTests.cpp`: deterministic assertion tests for clocks, event capacity, phase latching, guard reduction, compatibility, and line/zone geometry.
- Modify `TestGame/GameBalance.h`: one source of truth for elite health, attack timing, phase multipliers, status values, and modifier values.
- Modify `TestGame/Enemy.h` / `TestGame/Enemy.cpp`: signature hooks/events, phase reset, incoming-damage scale, generic-lunge removal, debug/telemetry access, and shared telegraph drawing hook.
- Modify `TestGame/Ogre.h` / `TestGame/Ogre.cpp`: wall-stun punish window and phase-two double charge.
- Modify `TestGame/Infernal.h` / `TestGame/Infernal.cpp`: Cinder March, Furnace Burst, Overheated phase.
- Modify `TestGame/Bonechill.h` / `TestGame/Bonechill.cpp`: frontal frost armour, Permafrost Slam, armour-shatter phase.
- Modify `TestGame/Stormclub.h` / `TestGame/Stormclub.cpp`: locked-target leap, lightning branches, miss recovery, Tempest double leap.
- Modify `TestGame/Venomfang.h` / `TestGame/Venomfang.cpp`: off-angle pounce, real poison, retreat trail, Predator's Mark, Blood Scent double pounce.
- Modify `TestGame/Character.h` / `TestGame/Character.cpp`: player poison status distinct from burn, capped refresh/stack rules, green tint, reset, and debug getters.
- Modify `TestGame/CombatDirector.h` / `TestGame/CombatDirector.cpp`: consume signature events, own bounded attack zones, apply status/damage, draw warnings, and replace legacy elite mechanics.
- Modify `TestGame/Engine.h` / `TestGame/Engine.cpp`: centralized elite-room setup, VFX callbacks, draw/update integration, snapshot cleanup, debug telemetry, and room modifier banners.
- Modify `TestGame/AttackTuning.h` / `TestGame/AttackTuning.cpp`: signature timing and phase-two tuning fields.
- Modify `TestGame/AttackEditor.h` / `TestGame/AttackEditor.cpp`: Elite category and previews for all signature moves.
- Modify `TestGame/VFXManager.h` / `TestGame/VFXManager.cpp`: bounded reusable animated-effect slots and complete room-clear cleanup.
- Modify `TestGame/DebugPanel.h` / `TestGame/DebugPanel.cpp`: force elite type/modifier/phase/move and expose signature telemetry.
- Modify `TestGame/TestGame.vcxproj`, `TestGame/TestGame.vcxproj.filters`, and `build_web.bat`: register the new source, test, and web-build files.

---

### Task 1: Shared Elite Primitives And Deterministic Tests

**Files:**
- Create: `TestGame/EliteSignature.h`
- Create: `TestGame/EliteSignature.cpp`
- Create: `TestGame/EliteSignatureTests.cpp`
- Modify: `TestGame/GameBalance.h`
- Modify: `TestGame/TestGame.vcxproj`
- Modify: `TestGame/TestGame.vcxproj.filters`
- Modify: `build_web.bat`

**Interfaces:**
- Produces: `EliteArchetype`, `EliteMove`, `EliteModifier`, `EliteActionStage`, `EliteActionClock`, `EliteSignatureEvent`, `EliteEventQueue`, `EliteAttackZone`, `IsEliteModifierCompatible`, `ChooseEliteModifier`, `ApplyGuardLinkReduction`, `ShouldEnterElitePhaseTwo`, and `DistancePointToSegment`.
- Consumes: raylib `Vector2` and constants from `Balance::Elite`.

- [ ] **Step 1: Write the failing pure-logic tests**

Add tests that state the required API and behavior:

```cpp
static void TestActionClockTraversesReadableStages()
{
    EliteActionClock clock;
    clock.Start({ 0.40f, 0.20f, 0.35f });
    assert(clock.GetStage() == EliteActionStage::Telegraph);
    assert(!clock.Update(0.39f));
    assert(clock.GetStage() == EliteActionStage::Telegraph);
    assert(clock.Update(0.02f));
    assert(clock.GetStage() == EliteActionStage::Active);
    assert(clock.Update(0.20f));
    assert(clock.GetStage() == EliteActionStage::Recovery);
    assert(clock.Update(0.35f));
    assert(clock.GetStage() == EliteActionStage::Ready);
}

static void TestGuardLinksReduceButNeverNullify()
{
    assert(ApplyGuardLinkReduction(10) == 4);
    assert(ApplyGuardLinkReduction(2) == 1);
    assert(ApplyGuardLinkReduction(1) == 1);
    assert(ApplyGuardLinkReduction(0) == 0);
}

static void TestModifierCompatibilityMatchesDesign()
{
    assert(IsEliteModifierCompatible(EliteArchetype::Ogre, EliteModifier::Cage));
    assert(!IsEliteModifierCompatible(EliteArchetype::Ogre, EliteModifier::ArenaPressure));
    assert(!IsEliteModifierCompatible(EliteArchetype::Stormclub, EliteModifier::Cage));
    assert(IsEliteModifierCompatible(EliteArchetype::Venomfang, EliteModifier::Enrage));
    assert(!IsEliteModifierCompatible(EliteArchetype::Venomfang, EliteModifier::GuardLinks));
}

static void TestEventQueueIsBoundedAndFIFO()
{
    EliteEventQueue queue;
    for (int i = 0; i < EliteEventQueue::kCapacity; ++i)
        assert(queue.Push({ EliteEventKind::Telegraph, EliteArchetype::Ogre,
                            EliteMove::OgreCharge, (std::uint32_t)i }));
    assert(!queue.Push({}));
    for (int i = 0; i < EliteEventQueue::kCapacity; ++i)
    {
        EliteSignatureEvent event{};
        assert(queue.Pop(event));
        assert(event.sequence == (std::uint32_t)i);
    }
}
```

Also test `ShouldEnterElitePhaseTwo(false, 6, 12)` returns true, a latched phase returns false, zero max health returns false, and `DistancePointToSegment` handles horizontal, vertical, and zero-length segments.

- [ ] **Step 2: Compile and run the test to verify RED**

Run from a Visual Studio developer shell:

```powershell
cl /nologo /std:c++17 /EHsc /DELITE_SIGNATURE_TEST_MAIN /I"C:\CLibraries\raylib-5.5_win64_msvc16\include" TestGame\EliteSignatureTests.cpp /link /OUT:x64\Debug\EliteSignatureTests.exe
```

Expected: compilation fails because `EliteSignature.h` and its types do not exist.

- [ ] **Step 3: Add the shared contracts and bounded storage**

Use explicit stable enums and fixed arrays:

```cpp
enum class EliteArchetype : std::uint8_t { Ogre, Infernal, Bonechill, Stormclub, Venomfang, Count };
enum class EliteModifier : std::int8_t { Random = -1, Cage, GuardLinks, Enrage, ArenaPressure };
enum class EliteActionStage : std::uint8_t { Ready, Telegraph, Active, Recovery };
enum class EliteEventKind : std::uint8_t { Telegraph, Lock, Execute, Recover, PhaseChange, TrailPatch };
enum class EliteMove : std::uint8_t {
    None, OgreCharge, InfernalCinderMarch, InfernalFurnaceBurst,
    BonechillPermafrostSlam, StormclubThunderLeap, VenomfangPounce
};
enum class EliteZoneShape : std::uint8_t { Disc, Lane, Cone };
enum class EliteStatusPayload : std::uint8_t { None, Burn, Chill, Shock, Poison, Knockback };

struct EliteActionTiming { float telegraph = 0.f, active = 0.f, recovery = 0.f; };

class EliteActionClock
{
public:
    void Start(EliteActionTiming timing);
    bool Update(float dt);
    void Cancel();
    EliteActionStage GetStage() const { return _stage; }
    float GetStageProgress() const;
private:
    EliteActionTiming _timing{};
    EliteActionStage _stage = EliteActionStage::Ready;
    float _remaining = 0.f;
};

struct EliteSignatureEvent
{
    EliteEventKind kind = EliteEventKind::Telegraph;
    EliteArchetype archetype = EliteArchetype::Ogre;
    EliteMove move = EliteMove::None;
    std::uint32_t sequence = 0;
    Vector2 origin{};
    Vector2 target{};
    Vector2 direction{ 1.f, 0.f };
    int phase = 0;
};

struct EliteAttackZone
{
    bool active = false;
    std::uint32_t sequence = 0;
    EliteArchetype owner = EliteArchetype::Ogre;
    EliteMove move = EliteMove::None;
    EliteZoneShape shape = EliteZoneShape::Disc;
    EliteStatusPayload status = EliteStatusPayload::None;
    Vector2 start{};
    Vector2 end{};
    float radius = 0.f;
    float halfAngleRadians = 0.f;
    float telegraphRemaining = 0.f;
    float activeRemaining = 0.f;
    float tickInterval = 0.f;
    float tickRemaining = 0.f;
    float damage = 0.f;
    bool hitPlayer = false;
};

struct EliteSignatureTelemetry
{
    EliteActionStage stage = EliteActionStage::Ready;
    float cooldown = 0.f;
    Vector2 lockedTarget{};
    int phase = 0;
    int casts = 0;
    int hits = 0;
    int droppedEvents = 0;
};

class EliteEventQueue
{
public:
    static constexpr int kCapacity = 12;
    bool Push(const EliteSignatureEvent& event);
    bool Pop(EliteSignatureEvent& event);
    void Clear();
private:
    std::array<EliteSignatureEvent, kCapacity> _items{};
    int _head = 0, _count = 0;
};

bool IsEliteModifierCompatible(EliteArchetype archetype, EliteModifier modifier);
EliteModifier ChooseEliteModifier(EliteArchetype archetype, std::uint32_t seed,
                                  int forcedModifier = -1);
int ApplyGuardLinkReduction(int damage);
bool ShouldEnterElitePhaseTwo(bool alreadyLatched, float health, float maxHealth);
float DistancePointToSegment(Vector2 point, Vector2 start, Vector2 end);
```

Implement modifier compatibility with static bitmasks, selection from only compatible values, integer guard reduction using `ceil(damage * 0.4f)`, and no random fallback outside the compatible set.

- [ ] **Step 4: Centralize elite values in `GameBalance.h`**

Add named constants under `Balance::Elite`, including:

```cpp
inline constexpr float kPhaseThreshold = 0.50f;
inline constexpr float kGuardLinksDamageTaken = 0.40f;
inline constexpr int   kSignatureZoneCapacity = 64;
inline constexpr float kOgreHealth = 10.f;
inline constexpr float kInfernalHealth = 12.f;
inline constexpr float kBonechillHealth = 14.f;
inline constexpr float kStormclubHealth = 12.f;
inline constexpr float kVenomfangHealth = 8.f;
inline constexpr float kBonechillFrontDamageTaken = 0.55f;
inline constexpr float kBonechillPhaseSpeed = 1.25f;
inline constexpr int   kPredatorMarkCap = 3;
```

Include named default timings for every move rather than scattering literals through class implementations.

- [ ] **Step 5: Run the pure tests to verify GREEN**

Compile with both sources and run:

```powershell
cl /nologo /std:c++17 /EHsc /DELITE_SIGNATURE_TEST_MAIN /I"C:\CLibraries\raylib-5.5_win64_msvc16\include" TestGame\EliteSignature.cpp TestGame\EliteSignatureTests.cpp /link /OUT:x64\Debug\EliteSignatureTests.exe
x64\Debug\EliteSignatureTests.exe
```

Expected: `Elite signature tests passed`.

- [ ] **Step 6: Register the files and commit**

Register `.cpp` files in the Visual Studio project and web build source list; register headers/tests in filters without adding the test `main` macro to the game build.

```powershell
git add TestGame/EliteSignature.h TestGame/EliteSignature.cpp TestGame/EliteSignatureTests.cpp TestGame/GameBalance.h TestGame/TestGame.vcxproj TestGame/TestGame.vcxproj.filters build_web.bat
git commit -m "feat: add elite signature combat primitives"
```

---

### Task 2: Enemy Signature Hook, Phase Latch, And Damage Reduction

**Files:**
- Modify: `TestGame/Enemy.h`
- Modify: `TestGame/Enemy.cpp`
- Modify: `TestGame/EliteSignatureTests.cpp`

**Interfaces:**
- Consumes: Task 1 `EliteActionClock`, `EliteEventQueue`, and `EliteArchetype`.
- Produces: `GetEliteArchetype()`, `UpdateEliteSignature(...)`, `DrawEliteTelegraph(...)`, `EmitEliteEvent(...)`, `ConsumeEliteEvent(...)`, `SetEliteGuardLinked(bool)`, `GetEliteSignatureTelemetry()`, and debug force/reset hooks.

- [ ] **Step 1: Add failing tests for incoming damage and reset semantics**

Extend pure tests around a production helper used by `Enemy::TakeDamage`: linked positive damage is reduced, unlinked damage is unchanged, and resetting an event queue removes every pending event. Run the Task 1 test command and confirm the new expectations fail before changing production code.

- [ ] **Step 2: Add the small virtual signature interface**

Add defaults that do nothing for ordinary enemies:

```cpp
virtual EliteArchetype GetEliteArchetype() const { return EliteArchetype::Count; }
virtual bool UpdateEliteSignature(float dt, Vector2 navigationTarget,
    bool hasNavigationTarget, const std::vector<std::unique_ptr<Enemy>>& enemies,
    const std::vector<Vector2>& propCenters) { return false; }
virtual void DrawEliteTelegraph(Vector2 cameraRef) const {}
virtual void DebugForceEliteSignature() {}
virtual void DebugForceElitePhaseTwo() {}
virtual const char* GetEliteSignatureStateName() const { return "None"; }

bool EmitEliteEvent(EliteSignatureEvent event);
bool ConsumeEliteEvent(EliteSignatureEvent& event);
void SetEliteGuardLinked(bool linked) { _eliteGuardLinked = linked; }
bool IsEliteGuardLinked() const { return _eliteGuardLinked; }
```

Call `UpdateEliteSignature` in `Enemy::Update` after common status/timer updates and before normal movement/attack. When it returns true, update animation and skip ordinary chase/melee for that frame.

- [ ] **Step 3: Remove automatic generic lunging**

Delete the `_isEliteMiniboss` and `UsesPersonalLunge` trigger path from `Enemy::Update`; stop calling `UpdateEliteLunge` entirely. Remove `UsesPersonalLunge` from the public contract and delete the generic lunge state/constants after Stormclub no longer depends on them. Preserve ordinary attack animation lean through `UsesAttackLunge` because it is visual-only.

- [ ] **Step 4: Make phase and event reset safe**

`ResetForSpawn`, `SetActive(false)`, death completion, and room reset must clear `_eliteEvents`, `_elitePhaseTwo`, guard-link state, pending phase callout, and stale signature telemetry. `SetIsEliteMiniboss(true)` arms a one-time 50% phase latch but does not change invulnerability or movement.

- [ ] **Step 5: Apply visible damage reduction instead of immunity**

In `Enemy::TakeDamage`, replace bodyguard/leap immunity handling with:

```cpp
if (_eliteGuardLinked)
    damage = ApplyGuardLinkReduction(damage);
```

Keep actual immunity only for unrelated, explicit systems such as pit fall and grave revive. Record a `reducedByGuardLinks` flag for damage-number/debug feedback, but do not report `SHIELDED` or zero damage. Mirror the same reduction in `Ogre::TakeDamage` until Task 4 routes Ogre through the shared helper.

- [ ] **Step 6: Verify and commit**

Run `EliteSignatureTests.exe`, then build `Debug|x64`. Expected: no generic elite lunge symbols remain and all ordinary enemy behavior still builds.

```powershell
git add TestGame/Enemy.h TestGame/Enemy.cpp TestGame/EliteSignatureTests.cpp
git commit -m "refactor: add elite signature hooks and fair guard reduction"
```

---

### Task 3: One Elite-Room Setup Path And Compatible Modifiers

**Files:**
- Modify: `TestGame/CombatDirector.h`
- Modify: `TestGame/CombatDirector.cpp`
- Modify: `TestGame/Engine.h`
- Modify: `TestGame/Engine.cpp`
- Modify: `TestGame/DebugPanel.h`
- Modify: `TestGame/DebugPanel.cpp`
- Modify: `TestGame/EliteSignatureTests.cpp`

**Interfaces:**
- Consumes: Task 1 compatibility helpers and Task 2 elite archetype/guard APIs.
- Produces: `Engine::InitializeEliteRoomRuntime`, `Engine::ResetEliteRoomRuntime`, `Engine::GetCompatibleEliteMechanicForRoom`, modifier ids `0..3`, and debug-forced elite type/modifier controls.

- [ ] **Step 1: Add failing deterministic modifier-selection tests**

For every archetype, call `ChooseEliteModifier(archetype, seed)` for at least 1,000 seeds and assert that every result is compatible. Assert the forced modifier is accepted only when compatible and otherwise falls back to a compatible deterministic choice. Verify RED before implementing selection.

- [ ] **Step 2: Replace five legacy mechanics with four explicit modifiers**

Use stable runtime ids:

```cpp
0 = Cage
1 = GuardLinks
2 = Enrage
3 = ArenaPressure
```

Remove legacy Leap state fields (`_eliteIsLeaping`, target/start/timer/cooldown) from Engine and both combat contexts. Rename legacy Hazards UI/banner text to `Arena Pressure`. Update every array bound, forced-debug clamp, snapshot field, and room-intro string from five entries to four.

- [ ] **Step 3: Centralize initialization**

Add one Engine function used by debug restart, main dungeon entry, handcrafted dungeon entry, snapshot restore, and room re-entry:

```cpp
void Engine::InitializeEliteRoomRuntime(Enemy* elite, int roomIndex,
                                        float worldWidth, float worldHeight)
{
    ResetEliteRoomRuntime();
    _eliteMinibossPtr = elite;
    if (!elite) return;
    const EliteArchetype type = elite->GetEliteArchetype();
    _eliteMechanic = GetCompatibleEliteMechanicForRoom(roomIndex, type);
    _eliteEnrageWarningTimer = kEliteEnrageWarningDuration;
    // Configure Cage, GuardLinks, Enrage, or ArenaPressure once here.
}
```

`GetCompatibleEliteMechanicForRoom` first returns the room snapshot's stored
modifier when it remains compatible. For a new room it rolls one seed, calls
`ChooseEliteModifier(type, seed, forcedDebugValue)`, stores the result in that
room's runtime state, and returns it. Re-entry and snapshot restore therefore
never reroll a different challenge.

Do not leave any duplicate `if (_eliteMechanic == ...)` initialization blocks elsewhere in `Engine.cpp` or `CombatDirector::SpawnEnemies`.

- [ ] **Step 4: Implement Guard Links as reduction**

While any active non-elite guard remains, call `SetEliteGuardLinked(true)`. On the first frame no guards remain, clear it and request `GUARD BROKEN`. Draw visible beams from guards to elite using warning/UI geometry. Never call `SetInvulnerable(true)` for this modifier.

- [ ] **Step 5: Add force controls for QA**

Debug panel buttons:

```text
Elite Type: Random | Ogre | Infernal | Bonechill | Stormclub | Venomfang
Modifier: Random | Cage | Guard Links | Enrage | Arena Pressure
Actions: Force Signature | Force Phase Two | Reset Signature Cooldown
```

`SpawnEliteMiniboss` accepts a forced type; production passes `-1`. Forced phase/signature actions apply only to the current active elite.

- [ ] **Step 6: Verify all entry paths and commit**

Run pure tests and `Debug|x64`. Search for stale legacy behavior:

```powershell
rg -n "eliteIsLeaping|eliteLeapTarget|SetInvulnerable\(true\)|GetRandomValue\(0, 4\)" TestGame
```

Expected: no elite-room setup uses those legacy paths.

```powershell
git add TestGame/CombatDirector.h TestGame/CombatDirector.cpp TestGame/Engine.h TestGame/Engine.cpp TestGame/DebugPanel.h TestGame/DebugPanel.cpp TestGame/EliteSignatureTests.cpp
git commit -m "refactor: centralize compatible elite room modifiers"
```

---

### Task 4: Ogre Battering-Ram Phase

**Files:**
- Modify: `TestGame/Ogre.h`
- Modify: `TestGame/Ogre.cpp`
- Modify: `TestGame/Engine.cpp`
- Modify: `TestGame/EliteSignatureTests.cpp`

**Interfaces:**
- Consumes: elite phase latch, events, Task 1 timings, existing Engine collision calls to `Ogre::OnRushBlocked`.
- Produces: committed charge lock, wall stun, `SECOND WIND`, and double-charge sequence.

- [ ] **Step 1: Add failing sequence-helper tests**

Test a small production helper `NextOgreChargeCount(phaseTwo)` returning `1` in phase one and `2` in phase two, and `ShouldEndOgreChargeSequence(remainingCharges, hitWall)` ending immediately on wall impact. Verify RED.

- [ ] **Step 2: Make the charge obey Telegraph -> Lock -> Execution -> Recovery**

Keep Ogre's custom update, animation, enemy scattering, and collision-owned wall detection. At the end of telegraph, snapshot `_rushDirection`; never rotate during execution. Emit events for telegraph, lock, rush start, and wall impact. Draw a lane warning only during telegraph.

- [ ] **Step 3: Preserve the wall punish window**

`OnRushBlocked` calls `FinishRush(true)`, emits impact/recovery events, and uses the existing wall sound. Increase the authored fallback stun to a full meaningful-ability window; tuning may override it, but it must not fall below `0.70s`.

- [ ] **Step 4: Add one-time phase-two double charge**

On crossing 50% health:

```cpp
_phaseTwo = true;
_chargesRemaining = 2;
RequestBossCallout("SECOND WIND");
EmitEliteEvent(phaseEvent);
```

After the first charge ends without a wall collision, enter a visible `Retargeting` state, lock a new direction after its warning, then perform the second charge. A wall collision ends the complete sequence and enters stun. Reset all counters on pool reuse.

- [ ] **Step 5: Verify and commit**

Test normal charge, double charge, wall collision after first charge, wall collision after second, freeze interruption during telegraph, and death during recovery. Build `Debug|x64`.

```powershell
git add TestGame/Ogre.h TestGame/Ogre.cpp TestGame/Engine.cpp TestGame/EliteSignatureTests.cpp
git commit -m "feat: give ogre a counterable double-charge phase"
```

---

### Task 5: Infernal And Bonechill Signature Kits

**Files:**
- Modify: `TestGame/Infernal.h`
- Modify: `TestGame/Infernal.cpp`
- Modify: `TestGame/Bonechill.h`
- Modify: `TestGame/Bonechill.cpp`
- Modify: `TestGame/Enemy.h`
- Modify: `TestGame/Enemy.cpp`
- Modify: `TestGame/EliteSignatureTests.cpp`

**Interfaces:**
- Consumes: base signature hook/events and `AttackTuningStore` values.
- Produces: Cinder March, Furnace Burst, Overheated, frontal frost armour, Permafrost Slam, and Armour Shattered.

- [ ] **Step 1: Add failing geometry and damage tests**

Test that a frontal attacker is identified with the existing facing dot helper's intended threshold, Bonechill front damage uses `ceil(damage * 0.55f)`, rear damage is unchanged, and armour is disabled during slam windup/recovery and after phase two. Test three fissure/ice-lane centerlines leave non-overlapping safe gaps.

- [ ] **Step 2: Implement Infernal's action selection**

Override `GetEliteArchetype` and `UpdateEliteSignature`. When signature cooldown expires, choose Cinder March at movement range or Furnace Burst at mid-range. Cinder March locks a direction and emits evenly spaced `TrailPatch` events while moving; cap the number of active patches. Furnace Burst emits one telegraph and then three forward fissure zones with fixed gaps.

- [ ] **Step 3: Add Infernal's phase escalation**

At 50%, cancel the current signature safely, request `OVERHEATED`, and begin a Furnace Burst. Phase two adds one delayed fissure wave, multiplies ordinary melee cadence modestly, and lengthens the post-burst exhausted recovery. It does not enlarge every hazard or fill the whole room.

- [ ] **Step 4: Implement Bonechill frontal armour correctly**

Override `TakeDamage` only to compute whether `attackerPos` lies in the visible frontal arc. Pass the scaled damage into the common Enemy damage path so Guard Links, revive, damage numbers, and statuses remain authoritative. Draw the frost-armour overlay while active; never set a block reason or display `IMMUNE`.

- [ ] **Step 5: Implement Permafrost Slam and armour shatter**

Slam locks its forward cone and emits one moderate direct zone plus several lighter ice lanes with gaps. Direct and lane hits apply chill through shared event resolution. At 50%, request `ARMOUR SHATTERED`, permanently disable frontal reduction, emit fragment VFX, multiply speed by `1.25`, and modestly shorten slam cooldown.

- [ ] **Step 6: Verify and commit**

Check front/rear damage, Guard Links plus frost-armour ordering, both Infernal moves, safe gaps, hazard expiry, one-time phase changes, and reset/reuse. Run tests and `Debug|x64`.

```powershell
git add TestGame/Infernal.h TestGame/Infernal.cpp TestGame/Bonechill.h TestGame/Bonechill.cpp TestGame/Enemy.h TestGame/Enemy.cpp TestGame/EliteSignatureTests.cpp
git commit -m "feat: add infernal and bonechill survival patterns"
```

---

### Task 6: Stormclub, Venomfang, And Real Player Poison

**Files:**
- Modify: `TestGame/Stormclub.h`
- Modify: `TestGame/Stormclub.cpp`
- Modify: `TestGame/Venomfang.h`
- Modify: `TestGame/Venomfang.cpp`
- Modify: `TestGame/Character.h`
- Modify: `TestGame/Character.cpp`
- Modify: `TestGame/EliteSignatureTests.cpp`

**Interfaces:**
- Consumes: signature action/event system and Character damage APIs.
- Produces: Thunder Leap, branch zones, embedded-club recovery, Tempest, Venom Pounce, poison trail, Predator's Mark, Blood Scent, and `Character::ApplyPoison`.

- [ ] **Step 1: Write failing poison and target-lock tests**

Test pure poison refresh rules: stack cap three, reapplication refreshes duration without duplicating a per-frame container, tick cadence is stable under a large `dt`, and reset clears it. Test a locked Stormclub/Venomfang target remains unchanged after the player moves.

- [ ] **Step 2: Add a real Character poison status**

Use scalar state rather than a vector of delayed burn ticks:

```cpp
void ApplyPoison(float duration, float tickInterval, float damagePerTick, int stacks = 1);
bool IsPoisoned() const { return _poisonTimer > 0.f; }
int GetPoisonStacks() const { return _poisonStacks; }
```

Cap stacks at three, refresh duration, tick fractional damage through one dedicated status function, tint the player green while active, and clear poison in `Init`, `Revive`, `RefreshForRoomEntry`, and death reset. Burn remains a separate red/orange status.

- [ ] **Step 3: Implement Stormclub's owned leap**

Remove `UsesPersonalLunge`. During telegraph, show a landing marker; at lock, snapshot the valid target and never retarget. Move continuously toward the target rather than teleporting. Landing emits a central impact plus three lightning branch zones with angular gaps. A missed landing enters a longer embedded-club recovery.

- [ ] **Step 4: Implement Tempest double leap**

At 50%, request `TEMPEST`. Future signature cycles perform two shorter leaps. Telegraph and lock the second target only after the first landing so the player is never asked to read two overlapping destinations. Every landing remains avoidable and every sequence ends in recovery.

- [ ] **Step 5: Implement Venomfang's pounce loop**

Circle to an off-angle point, telegraph a narrow path, lock, and move to the endpoint. A successful bite applies real poison and increments Predator's Mark to a cap of three; the mark expires after several seconds without another hit. After a bite, retreat and emit short poison-trail events. An interrupted or missed pounce does not increment the mark.

- [ ] **Step 6: Implement Blood Scent**

At 50%, request `BLOOD SCENT`, increase circling speed, and permit a second pounce after an independently visible delay. Do not hide, teleport, or make Venomfang untargetable.

- [ ] **Step 7: Verify and commit**

Test leap movement against constrained-room collision, missed recovery, double-leap target timing, pounce interruption, poison cap/expiry, mark expiry, and complete reset on room re-entry. Run tests and `Debug|x64`.

```powershell
git add TestGame/Stormclub.h TestGame/Stormclub.cpp TestGame/Venomfang.h TestGame/Venomfang.cpp TestGame/Character.h TestGame/Character.cpp TestGame/EliteSignatureTests.cpp
git commit -m "feat: add stormclub and venomfang signature phases"
```

---

### Task 7: Shared Attack Zones, Animated VFX, Audio, And Feedback

**Files:**
- Modify: `TestGame/CombatDirector.h`
- Modify: `TestGame/CombatDirector.cpp`
- Modify: `TestGame/VFXManager.h`
- Modify: `TestGame/VFXManager.cpp`
- Modify: `TestGame/SfxBank.h`
- Modify: `TestGame/SfxBank.cpp`
- Modify: `TestGame/Engine.h`
- Modify: `TestGame/Engine.cpp`
- Modify: `TestGame/EliteSignatureTests.cpp`

**Interfaces:**
- Consumes: `EliteSignatureEvent` and `EliteAttackZone`.
- Produces: bounded zone acquisition/update/draw/clear, VFX routing, status application, phase feedback, and elite telemetry.

- [ ] **Step 1: Add failing bounded-zone tests**

Test that acquiring 64 zones succeeds, the 65th request is rejected or deterministically replaces the oldest low-priority expired zone, update never damages before `telegraphRemaining <= 0`, one-shot zones hit once, lingering patches obey tick intervals, and `ClearEliteRuntime` deactivates all zones/events.

- [ ] **Step 2: Consume events after each enemy update**

In `CombatDirector::UpdateEnemyRuntime`, drain each enemy's queue immediately after `enemy->Update`. Translate telegraph/execute/trail events into fixed-capacity `EliteAttackZone` slots. Store world positions at spawn time; zones do not follow enemies after locking.

- [ ] **Step 3: Resolve zone damage and statuses centrally**

Each zone declares disc, lane/capsule, or cone geometry; warning duration; active duration; damage; and status payload. During active frames, call Character damage once per one-shot zone or at the authored interval for lingering patches. Apply burn, chill, shock/knockback, or poison after a successful hit. Respect player dash/i-frame rules through existing Character APIs.

- [ ] **Step 4: Route warnings and active art separately**

`CombatDirector::DrawEliteWorld` draws only warning outlines and safe-gap geometry. Execution calls `VFXManager::SpawnSpriteFx` or `SpawnHazardDecal` using existing owned assets:

```text
Ogre impact: BossDashDust / BossHeavyStrike
Infernal trail/fissures: Hellfire or BossToxicEruption recolored fire-orange
Bonechill slam/lanes: FrostTrap or ice impact art tinted ice-blue
Stormclub landing/branches: electric impact art tinted bright gold-white
Venomfang pounce/trail: BossPounceImpact and BossPoisonPool tinted toxic green
```

Fallback VFX must still be animated and may reuse the nearest existing sprite strip; do not replace active attacks with circles.

- [ ] **Step 5: Bound and reuse VFX slots**

Reserve fixed slot counts during `VFXManager::Init`; acquire an inactive slot instead of `push_back` growth for animated effects and sparks. `Clear` must deactivate effects and clear floating text and sparks. Do not unload textures from the manager because Engine remains the owner.

- [ ] **Step 6: Add distinct audio intentions**

Add `PlayEliteTelegraph`, `PlayEliteImpact`, and `PlayElitePhase` mappings with explicit fallbacks: Ogre uses `EnemyAggro` then `BossImpact`, Infernal uses `CastFire` then `ImpactFire`, Bonechill uses `CastIce` then `ImpactIce`, Stormclub uses `CastElectric` then `ImpactElectric`, and Venomfang uses `EnemyAggro` then `ImpactPoison`. Every phase transition uses `BossRoar`. Throttle multi-branch impacts to one sound and one shake per cast. Use stronger feedback for wall impacts and phase transitions, not every hazard tick.

- [ ] **Step 7: Add runtime telemetry**

Display in debug mode: elite type, modifier, phase, current signature state, cooldown, locked target, cast count, hit count, active zone count, event queue use, fight duration, and dropped event/zone count. Never expose this panel in normal play.

- [ ] **Step 8: Verify and commit**

Run pure tests, build, transition between rooms while hazards are active, kill elites during telegraph/active/recovery, and verify no VFX or damage survives the room. Confirm event and zone counts remain within fixed capacity.

```powershell
git add TestGame/CombatDirector.h TestGame/CombatDirector.cpp TestGame/VFXManager.h TestGame/VFXManager.cpp TestGame/SfxBank.h TestGame/SfxBank.cpp TestGame/Engine.h TestGame/Engine.cpp TestGame/EliteSignatureTests.cpp
git commit -m "feat: render and resolve elite signature attacks"
```

---

### Task 8: Attack Editor Support And Designer Workflow

**Files:**
- Modify: `TestGame/AttackTuning.h`
- Modify: `TestGame/AttackTuning.cpp`
- Modify: `TestGame/AttackEditor.h`
- Modify: `TestGame/AttackEditor.cpp`
- Create: `attacktuning_Infernal_Cinder_March.txt`
- Create: `attacktuning_Infernal_Furnace_Burst.txt`
- Create: `attacktuning_Bonechill_Permafrost_Slam.txt`
- Create: `attacktuning_Stormclub_Thunder_Leap.txt`
- Create: `attacktuning_Venomfang_Venom_Pounce.txt`
- Modify: `attacktuning_Ogre_Charge.txt`
- Modify: `TestGame/EliteSignatureTests.cpp`

**Interfaces:**
- Consumes: existing `AttackTuningStore` key format and Attack Editor preview.
- Produces: saved signature timing, distance, hitbox, VFX offset/scale, and phase multipliers for all elite moves.

- [ ] **Step 1: Write failing tuning round-trip tests**

Construct an `AttackTuning` with signature fields, save it, reload it, and assert every value survives within floating-point tolerance. Include old files without signature fields and assert defaults remain unchanged.

- [ ] **Step 2: Extend tuning without breaking existing files**

Add:

```cpp
bool hasSignature = false;
float telegraphTime = 0.f;
float activeTime = 0.f;
float recoveryTime = 0.f;
float signatureCooldown = 0.f;
float travelDistance = 0.f;
float phaseSpeedMult = 1.f;
float phaseCooldownMult = 1.f;
```

Read/write only when present. Missing groups preserve the constants from `GameBalance.h`.

- [ ] **Step 3: Add an Elite category to the editor list**

Add entries:

```text
Ogre: Charge
Infernal: Cinder March, Furnace Burst
Bonechill: Permafrost Slam
Stormclub: Thunder Leap
Venomfang: Venom Pounce
```

Distinguish `Player`, `Boss`, and `Elite` categories rather than labeling elites as bosses. Preserve the current ability list and existing boss entries.

- [ ] **Step 4: Preview the complete authored beat**

For each elite entry, display the correct elite sprite, left/right facing, warning geometry, locked path/target, active VFX, hitbox/lanes, and recovery timing on one looping timeline. Existing click/drag geometry stays on the left; numeric timing fields stay in the compact bottom-right panel. Saving reloads live tuning immediately.

- [ ] **Step 5: Seed conservative defaults and verify**

Create files with the coded defaults so the user can edit immediately. Launch the editor, preview each move in both directions, alter one timing/offset, save, restart, and confirm persistence. Run tests and `Debug|x64`.

```powershell
git add TestGame/AttackTuning.h TestGame/AttackTuning.cpp TestGame/AttackEditor.h TestGame/AttackEditor.cpp attacktuning_Ogre_Charge.txt attacktuning_Infernal_Cinder_March.txt attacktuning_Infernal_Furnace_Burst.txt attacktuning_Bonechill_Permafrost_Slam.txt attacktuning_Stormclub_Thunder_Leap.txt attacktuning_Venomfang_Venom_Pounce.txt TestGame/EliteSignatureTests.cpp
git commit -m "feat: expose elite signatures in the attack editor"
```

---

### Task 9: Full Verification And Balance Pass

**Files:**
- Modify only files required by failures discovered in this task.

**Interfaces:**
- Consumes: all previous tasks.
- Produces: verified `Debug|x64` game and a stable baseline for later boss phases.

- [ ] **Step 1: Run automated tests**

```powershell
x64\Debug\EliteSignatureTests.exe
```

Expected: `Elite signature tests passed` with no assertions.

- [ ] **Step 2: Build the complete game**

```powershell
msbuild TestGame.sln /p:Configuration=Debug /p:Platform=x64 /m
```

Expected: zero compile/link errors. Investigate warnings introduced by this feature; do not clean unrelated legacy warnings.

- [ ] **Step 3: Run the compatibility matrix**

Use debug forcing to play:

```text
Ogre: Cage, Guard Links, Enrage
Infernal: Guard Links, Enrage, Arena Pressure
Bonechill: Cage, Guard Links, Arena Pressure
Stormclub: Guard Links, Enrage, Arena Pressure
Venomfang: Enrage, Arena Pressure
```

For every pairing, test one open room and one constrained handcrafted room. Confirm warnings fit geometry, attacks never land in blocked/fall space, and no modifier removes the intended punish window.

- [ ] **Step 4: Run lifecycle abuse tests**

During every signature stage: kill the elite, leave the room, restart the room, return from a snapshot, pause/resume, and reuse the pooled enemy. Confirm there is no stale damage, warning, phase, target, sound loop, or VFX.

- [ ] **Step 5: Measure fight targets and tune behavior before health**

Record fight duration, signature casts/hits, and player damage. Target `35-55s` for an on-curve elite. If too easy, tighten telegraph/cooldown or improve pattern combination before adding health. If too hard, widen safe gaps or recovery before lowering damage.

- [ ] **Step 6: Final review and commit**

Review the complete diff for unrelated changes, raw-circle active VFX, hidden invulnerability, unbounded vectors, duplicated elite initialization, and stale legacy Leap symbols. Re-run tests and build after final fixes.

If verification requires code fixes, stage only the feature-owned files shown by
`git status --short`, verify the staged names with `git diff --cached --name-only`,
and commit them. If verification requires no fixes, do not create an empty commit.

```powershell
git diff --cached --name-only
git commit -m "test: verify elite signature encounter matrix"
```

---

## Boss-Phase Follow-On Boundary

This plan deliberately stops before rewriting full bosses. The reusable result is the action clock, target lock, bounded event queue, attack-zone pool, tuning fields, telegraph/execute separation, phase telemetry, and debug force controls. A later boss plan can chain several `EliteMove`-style events into longer survival scripts, add arena transformations, and use two or three health/time gates without changing the five elite implementations.
