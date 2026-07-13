# Enemy Density and Damage Numbers Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver readable 8-32 enemy encounters and allocation-free, prioritized, merge-aware player damage numbers showing actual post-defence results.

**Architecture:** Add a fixed-capacity `DamageNumberManager` that owns combat-result text and a deterministic `EncounterPlanner` that separates total population from live danger pressure. `Engine` remains the integration owner, `CombatDirector` retains combat orchestration, `VFXManager` retains animated sprite effects and rare narrative labels, and `GameBalance.h` remains the tuning authority.

**Tech Stack:** C++17, raylib 5.5, Visual Studio 2022/MSBuild, deterministic assertion-based C++ test executable.

## Global Constraints

- Preserve all unrelated user and Claude changes in the dirty worktree.
- Do not alter class damage, ability cooldowns, hitboxes, village systems, dungeon art, or save identifiers.
- Normal outgoing damage is strong red; critical outgoing damage is gold.
- Display actual health removed after defence and blocking.
- Never display an unexplained zero.
- Do not allocate heap memory for ordinary damage-number submissions.
- Keep population, danger pressure, specialists, enemy projectiles, and environmental projectiles independently capped.
- Build `Debug|x64` after each integration milestone.

---

## File Structure

### New Files

- `TestGame/DamageNumberManager.h`: event types, fixed pool, settings, statistics, update/draw API.
- `TestGame/DamageNumberManager.cpp`: allocation-free submission, merge, priority replacement, animation, and rendering.
- `TestGame/EncounterPlanner.h`: spawn kinds, specialist classes, candidates, requests, plans, and debug totals.
- `TestGame/EncounterPlanner.cpp`: deterministic population roll, specialist reservation, pressure validation, opening/wave partitioning.
- `TestGame/CombatSystemsTests.cpp`: standalone assertion-based tests for both new systems.

### Modified Files

- `TestGame/GameBalance.h`: population targets, live caps, pressure caps, specialist caps, projectile caps, swarm and damage-number defaults.
- `TestGame/Enemy.h`, `TestGame/Enemy.cpp`: stable combat identifier and explicit swarm profile.
- `TestGame/Engine.h`, `TestGame/Engine.cpp`: manager ownership, resolved-damage routing, lifecycle clearing, debug controls, encounter-plan integration, reinforcement release, telemetry.
- `TestGame/VFXManager.h`, `TestGame/VFXManager.cpp`: remove numeric damage ownership while preserving animated VFX, sparks, and non-damage narrative labels.
- `TestGame/CombatDirector.h`, `TestGame/CombatDirector.cpp`: encounter candidate/profile helpers and role-pressure queries.
- `TestGame/DebugPanel.h`, `TestGame/DebugPanel.cpp`: expose encounter and damage-number telemetry toggles where appropriate.
- `TestGame/TestGame.vcxproj`, `TestGame/TestGame.vcxproj.filters`: compile and display new production files; tests remain outside the game target.
- `build_web.bat`: compile the two new production `.cpp` files for web.

---

### Task 1: Fixed-Capacity Damage Number Core

**Files:**
- Create: `TestGame/DamageNumberManager.h`
- Create: `TestGame/DamageNumberManager.cpp`
- Create: `TestGame/CombatSystemsTests.cpp`

**Interfaces:**
- Produces: `DamageNumberEvent`, `DamageNumberSettings`, `DamageNumberStats`, and `DamageNumberManager`.
- Consumes: raylib `Vector2`, `Color`, `Font`, and camera/world offset at draw time.

- [ ] **Step 1: Define the public types and fixed pool**

Use this public contract:

```cpp
enum class DamageNumberOutcome : unsigned char
{
    Normal, Critical, Blocked, Immune, Invulnerable,
    Dodge, Armour, Airborne, Underground, Incoming
};

struct DamageNumberEvent
{
    std::uint64_t targetId = 0;
    std::uint32_t attackId = 0;
    Vector2 worldPos{};
    int finalDamage = 0;
    DamageNumberOutcome outcome = DamageNumberOutcome::Normal;
    bool backstab = false;
    bool killingBlow = false;
    bool elite = false;
    bool boss = false;
    bool damageOverTime = false;
};

struct DamageNumberSettings
{
    bool enabled = true;
    int visibleCap = 32;
    int minFontSize = 20;
    int maxFontSize = 42;
    float damageReference = 250.f;
    float riseSpeed = 92.f;
    float horizontalDrift = 22.f;
    float lifetime = 0.90f;
    float outlineOffset = 2.f;
    float mergeWindow = 0.20f;
};

struct DamageNumberStats
{
    int capacity = 96;
    int active = 0;
    int visible = 0;
    int mergeCandidates = 0;
    std::uint64_t submitted = 0;
    std::uint64_t merged = 0;
    std::uint64_t suppressed = 0;
    std::uint64_t replaced = 0;
    int highWater = 0;
};

class DamageNumberManager
{
public:
    static constexpr int kCapacity = 96;
    void Init(Font font = GetFontDefault());
    void Submit(const DamageNumberEvent& event);
    void Update(float dt);
    void Draw(Vector2 worldOffset) const;
    void Clear();
    DamageNumberSettings& Settings();
    const DamageNumberStats& Stats() const;
};
```

Internally use `std::array<Slot, kCapacity>` and fixed-size label storage such as `char label[16]`. Do not use `std::vector` or `std::string` in a submitted slot.

- [ ] **Step 2: Write failing pool, merge, and priority tests**

Add a standalone `main()` under `#ifdef COMBAT_SYSTEMS_TEST_MAIN` with assertions covering:

```cpp
DamageNumberManager manager;
manager.Init(GetFontDefault());
manager.Submit({ 7, 11, {100,100}, 4, DamageNumberOutcome::Normal });
manager.Submit({ 7, 11, {104,100}, 6, DamageNumberOutcome::Normal });
assert(manager.Stats().active == 1);
assert(manager.DebugValueForTarget(7) == 10);

manager.Submit({ 7, 11, {104,100}, 8, DamageNumberOutcome::Critical });
assert(manager.Stats().active == 2);

for (int i = 0; i < 140; ++i)
    manager.Submit({ (std::uint64_t)(100+i), 1, {(float)i,0}, 1, DamageNumberOutcome::Normal });
assert(manager.Stats().active <= DamageNumberManager::kCapacity);
```

Expose `DebugValueForTarget` and `DebugOutcomeCount` only under `_DEBUG` or the test define.

- [ ] **Step 3: Compile tests and verify failure**

Run:

```powershell
cl /nologo /std:c++17 /EHsc /DCOMBAT_SYSTEMS_TEST_MAIN /I"C:\CLibraries\raylib-5.5_win64_msvc16\include" TestGame\DamageNumberManager.cpp TestGame\CombatSystemsTests.cpp /link /LIBPATH:"C:\CLibraries\raylib-5.5_win64_msvc16\lib" raylib.lib winmm.lib gdi32.lib opengl32.lib /OUT:x64\Debug\CombatSystemsTests.exe
```

Expected: compilation fails until the manager implementation exists.

- [ ] **Step 4: Implement deterministic merge and replacement logic**

Implement these rules:

```cpp
bool compatible = slot.active
    && slot.targetId == event.targetId
    && slot.attackId == event.attackId
    && slot.outcome == event.outcome
    && slot.damageOverTime == event.damageOverTime
    && slot.age <= _settings.mergeWindow
    && event.outcome != DamageNumberOutcome::Critical
    && !event.backstab;
```

Priority returns the highest value for boss/elite kills, then critical/backstab, ordinary kills, denial labels, normal hits, and DoT. When the pool is full, replace the lowest-priority active slot only if the incoming event has higher priority; otherwise increment `suppressed`.

Use a deterministic drift sign derived from `targetId`, `attackId`, and the submit sequence rather than random allocation or global RNG.

- [ ] **Step 5: Implement animation and rendering**

Normal outgoing color is `Color{230,45,50,255}` and critical color is `Color{255,205,45,255}`. Criticals use `1.25f` size, kills use another modest `1.12f`, and incoming damage uses a distinct pale/rose style and downward or lateral motion.

Draw the outline first using four offset draws, then the colored text. Use `MeasureTextEx`/`DrawTextEx` with the stored `Font`. Compute screen position only from captured `worldPos + worldOffset + virtualCanvasHalf`.

- [ ] **Step 6: Run tests and verify pass**

Run the command from Step 3, then:

```powershell
x64\Debug\CombatSystemsTests.exe
```

Expected: exit code `0` and `DamageNumberManager tests passed`.

- [ ] **Step 7: Commit the core manager**

```powershell
git add TestGame/DamageNumberManager.h TestGame/DamageNumberManager.cpp TestGame/CombatSystemsTests.cpp
git commit -m "feat: add pooled damage number manager"
```

---

### Task 2: Engine and VFX Lifecycle Integration

**Files:**
- Modify: `TestGame/Engine.h`
- Modify: `TestGame/Engine.cpp`
- Modify: `TestGame/VFXManager.h`
- Modify: `TestGame/VFXManager.cpp`
- Modify: `TestGame/TestGame.vcxproj`
- Modify: `TestGame/TestGame.vcxproj.filters`
- Modify: `build_web.bat`

**Interfaces:**
- Consumes: `DamageNumberManager` from Task 1.
- Produces: one manager instance updated/drawn/cleared by every combat lifecycle.

- [ ] **Step 1: Add production files to native and web builds**

Add `DamageNumberManager.cpp/.h` to the Visual Studio project/filter and `DamageNumberManager.cpp` to the Emscripten source list.

- [ ] **Step 2: Add manager ownership to Engine**

In `Engine.h` include `DamageNumberManager.h` and add:

```cpp
DamageNumberManager _damageNumbers;
std::uint64_t _nextCombatTargetId = 1;
```

Initialize once after the render context exists:

```cpp
_damageNumbers.Init(GetFontDefault());
```

- [ ] **Step 3: Update and draw from existing combat order**

Call `_damageNumbers.Update(dt)` beside `_vfx.Update(dt)`. Draw after world entities and hit VFX but before the HUD:

```cpp
_damageNumbers.Draw(Vector2{ -shakenCamRef.x, -shakenCamRef.y });
```

Verify the offset matches the existing `VFXManager::DrawFloatingTexts` transform exactly.

- [ ] **Step 4: Clear at every lifecycle boundary**

Add `_damageNumbers.Clear()` beside every combat-relevant `_vfx.Clear()` call: room entry, room reset, run reset, village entry, game over, debug room restart, and shutdown.

- [ ] **Step 5: Retire numeric ownership from VFXManager**

Remove `SpawnFloatingText`, numeric `FloatingText::value`, and `_damageNumberScale`. Keep `SpawnFloatingLabel` for rare non-damage messages such as boss phase names and reinforcement announcements. Route blocked/immune combat labels to the new manager in Task 3.

- [ ] **Step 6: Build Debug x64**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' TestGame.sln /t:Build /p:Configuration=Debug /p:Platform=x64 /m /nologo /v:minimal
```

Expected: exit code `0`.

- [ ] **Step 7: Commit lifecycle integration**

```powershell
git add TestGame/Engine.h TestGame/Engine.cpp TestGame/VFXManager.h TestGame/VFXManager.cpp TestGame/TestGame.vcxproj TestGame/TestGame.vcxproj.filters build_web.bat
git commit -m "refactor: route combat numbers through pooled manager"
```

---

### Task 3: Stable Targets and Actual Post-Defence Damage

**Files:**
- Modify: `TestGame/Enemy.h`
- Modify: `TestGame/Enemy.cpp`
- Modify: `TestGame/Engine.h`
- Modify: `TestGame/Engine.cpp`

**Interfaces:**
- Produces: `Enemy::GetCombatId()`, `Enemy::SetCombatId()`, and one resolved player-hit feedback path.
- Consumes: `DamageNumberEvent`.

- [ ] **Step 1: Add stable combat identifiers**

Add to `Enemy`:

```cpp
std::uint64_t GetCombatId() const { return _combatId; }
void SetCombatId(std::uint64_t id) { _combatId = id; }
```

Store `_combatId = 0`. Assign a monotonically increasing value in the common spawned-enemy configuration function. Never store `Enemy*` in `DamageNumberManager`.

- [ ] **Step 2: Define feedback metadata**

Add an Engine-private structure:

```cpp
struct PlayerHitFeedback
{
    std::uint32_t attackId = 0;
    bool critical = false;
    bool backstab = false;
    bool damageOverTime = false;
};
```

Add helpers:

```cpp
int GetActualDamageDealt(float healthBefore, const Enemy& enemy) const;
void SubmitPlayerHitFeedback(Enemy& enemy, float healthBefore,
                             Enemy::HitBlockReason blocked,
                             const PlayerHitFeedback& feedback);
```

- [ ] **Step 3: Write a failing actual-damage regression test**

Add a pure helper assertion to `CombatSystemsTests.cpp` proving `healthBefore=10`, `healthAfter=7`, and requested damage `8` displays `3`, while unchanged health with `Shielded` displays a label and no zero.

- [ ] **Step 4: Migrate every player damage path**

For basic attacks, projectile hits, class abilities, reflected damage, companion hits, elemental reactions, and DoT:

```cpp
float healthBefore = enemy.GetHealthValue();
enemy.TakeDamage(requestedDamage, sourcePos);
Enemy::HitBlockReason blocked = enemy.ConsumeHitBlock();
SubmitPlayerHitFeedback(enemy, healthBefore, blocked,
    { attackId, critical, backstab, damageOverTime });
```

Remove all direct `RegisterHitFx(... requestedDamage ...)` calls. Preserve status application rules by applying statuses only when actual damage landed, matching current behavior.

- [ ] **Step 5: Map denial reasons without zeroes**

Extend `Enemy::HitBlockReason` only where runtime can genuinely distinguish `Invulnerable`, `Airborne`, `Underground`, or `Dodge`. Do not infer `Dodge` from an ordinary miss. Map each reason to a zero-damage `DamageNumberEvent` outcome.

- [ ] **Step 6: Keep impact VFX separate from text**

`SubmitPlayerHitFeedback` calls `_damageNumbers.Submit(event)` and separately invokes `_vfx.SpawnImpactBurst`. Aggregate kill shake/hit-stop in Task 4; do not put screen effects inside the manager.

- [ ] **Step 7: Run tests and build**

Expected: test executable exits `0`; `Debug|x64` builds with no new errors.

- [ ] **Step 8: Commit resolved feedback routing**

```powershell
git add TestGame/Enemy.h TestGame/Enemy.cpp TestGame/Engine.h TestGame/Engine.cpp TestGame/CombatSystemsTests.cpp
git commit -m "fix: display actual resolved player damage"
```

---

### Task 4: Kill Aggregation and Debug Controls

**Files:**
- Modify: `TestGame/Engine.h`
- Modify: `TestGame/Engine.cpp`
- Modify: `TestGame/DebugPanel.h`
- Modify: `TestGame/DebugPanel.cpp`
- Modify: `TestGame/GameBalance.h`

**Interfaces:**
- Consumes: manager settings/stats and resolved hit events.
- Produces: frame-level multi-kill feedback and live debug tuning.

- [ ] **Step 1: Add centralized tuning defaults**

Create `Balance::DamageNumbers` constants matching the approved defaults and kill hit-stop values:

```cpp
inline constexpr int kPoolCapacity = 96;
inline constexpr int kVisibleCap = 32;
inline constexpr float kMergeWindow = 0.20f;
inline constexpr float kNormalKillStop = 0.035f;
inline constexpr float kEliteKillStop = 0.055f;
inline constexpr float kBossKillStop = 0.085f;
```

- [ ] **Step 2: Aggregate kills per attack/frame**

Store a frame accumulator containing ordinary, elite, and boss kill counts plus strongest impact position. Flush once per update:

```cpp
if (bossKills > 0) TriggerSlowMo(kBossKillStop, 0.25f);
else if (eliteKills > 0) TriggerSlowMo(kEliteKillStop, 0.35f);
else if (ordinaryKills > 0) TriggerSlowMo(kNormalKillStop, 0.45f);

float shake = std::min(8.f, 2.5f + std::sqrt((float)ordinaryKills) * 1.5f);
TriggerScreenShake(shake, 0.12f);
```

Do not trigger full shake once per killed enemy.

- [ ] **Step 3: Extend the juice/debug panel**

Add controls for enabled, min/max font, reference damage, rise speed, drift, lifetime, outline, merge window, visible cap, and animation freeze/step. Keep existing Force Crit.

- [ ] **Step 4: Add damage calculation trace**

Capture the latest base damage, run multiplier, class multiplier, status/relic multiplier, critical multiplier, and final requested/result damage. Draw it only when `Show final damage calculation` is enabled.

When a critical resolves, play the project's existing critical cue if one is already loaded. If no dedicated cue exists, retain the current impact sound rather than introducing an unrelated placeholder sound.

- [ ] **Step 5: Add pool telemetry**

Draw capacity, active, visible, free, merge candidates, submitted, merged, suppressed, replaced, and high-water values. Add a comparison toggle that disables number rendering without disabling combat resolution.

- [ ] **Step 6: Verify debug controls and build**

Manual: force critical, disable numbers, change merge window, and confirm stats react. Build `Debug|x64`.

- [ ] **Step 7: Commit game-feel and debugging**

```powershell
git add TestGame/GameBalance.h TestGame/Engine.h TestGame/Engine.cpp TestGame/DebugPanel.h TestGame/DebugPanel.cpp
git commit -m "feat: add damage feedback tuning and telemetry"
```

---

### Task 5: Deterministic Population and Pressure Planner

**Files:**
- Create: `TestGame/EncounterPlanner.h`
- Create: `TestGame/EncounterPlanner.cpp`
- Modify: `TestGame/GameBalance.h`
- Modify: `TestGame/CombatSystemsTests.cpp`
- Modify: `TestGame/TestGame.vcxproj`
- Modify: `TestGame/TestGame.vcxproj.filters`
- Modify: `build_web.bat`

**Interfaces:**
- Produces: `EnemySpawnKind`, `SpecialistClass`, `EncounterSpawnEntry`, `EncounterRequest`, `EncounterPlan`, and `EncounterPlanner::Build`.

- [ ] **Step 1: Add population and safety constants**

Use:

```cpp
inline constexpr int kPopulationMin[3] = { 8, 12, 18 };
inline constexpr int kPopulationMax[3] = { 12, 18, 26 };
inline constexpr int kOpeningBodyCap[3] = { 10, 14, 18 };
inline constexpr int kDangerCap[3] = { 12, 18, 24 };
inline constexpr int kSwarmPeakMin = 28;
inline constexpr int kSwarmPeakMax = 32;
inline constexpr int kEnemyProjectileCap[3] = { 12, 16, 20 };
inline constexpr int kEnvironmentalProjectileCap[3] = { 8, 10, 12 };
```

Add per-tier specialist caps for ranged `{2,3,4}`, tank `{1,2,2}`, support/summoner `{0,1,2}`, assassin `{1,2,3}`, zoner `{1,2,3}`, and total expensive units `{3,5,7}`.

- [ ] **Step 2: Define complete queued entries**

```cpp
enum class EnemySpawnKind : unsigned char
{
    Shadow, Archer, Slime, FlameWisp, Sporeling,
    Shieldbearer, Phantom, Bomber, Warchief, LivingBlade
};

enum class SpecialistClass : unsigned char
{
    None, Ranged, Tank, Support, Assassin, Zoner
};

struct EncounterSpawnEntry
{
    EnemySpawnKind kind{};
    EnemyRole role = EnemyRole::Grunt;
    SpecialistClass specialist = SpecialistClass::None;
    int populationCost = 1;
    int pressureCost = 1;
    bool swarmProfile = false;
};

struct EncounterPlanDebug
{
    int targetPopulation = 0;
    int plannedPopulation = 0;
    int openingPopulation = 0;
    int openingPressure = 0;
    int totalPressure = 0;
    int specialistCounts[6]{};
};

struct EncounterPlan
{
    std::vector<EncounterSpawnEntry> opening;
    std::deque<EncounterSpawnEntry> reinforcements;
    EncounterPlanDebug debug{};
    bool swarm = false;
};
```

`EncounterPlan` owns `std::vector<EncounterSpawnEntry> opening` and `std::deque<EncounterSpawnEntry> reinforcements`, plus target/total debug counts.

- [ ] **Step 3: Write failing deterministic planner tests**

For seeds `1..1000`, assert every standard plan lies inside its tier population range; no plan exceeds specialist caps; opening pressure and body cap are respected; and swarm plans reach `28..32` using only swarm-eligible filler for inflated population.

- [ ] **Step 4: Implement planner**

Build candidates from the current weighted table, reserve template specialists, fill population with cheap biome-appropriate candidates, then partition in authored order. If a candidate violates specialist or total danger constraints, choose eligible filler rather than reducing the target population.

The opening partition accepts an entry only when both body and live danger caps permit it. Remaining entries preserve their full kind/profile in reinforcement storage.

- [ ] **Step 5: Run deterministic tests**

Compile the test executable with both new `.cpp` files. Expected: all 1000 seeded plans per tier pass, plus explicit swarm tests.

- [ ] **Step 6: Add production files to builds and compile**

Add the planner source/header to VS filters/project and web build, then build `Debug|x64`.

- [ ] **Step 7: Commit the planner**

```powershell
git add TestGame/EncounterPlanner.h TestGame/EncounterPlanner.cpp TestGame/GameBalance.h TestGame/CombatSystemsTests.cpp TestGame/TestGame.vcxproj TestGame/TestGame.vcxproj.filters build_web.bat
git commit -m "feat: separate encounter population from pressure"
```

---

### Task 6: Runtime Encounter and Reinforcement Integration

**Files:**
- Modify: `TestGame/Engine.h`
- Modify: `TestGame/Engine.cpp`
- Modify: `TestGame/CombatDirector.h`
- Modify: `TestGame/CombatDirector.cpp`
- Modify: `TestGame/Enemy.h`
- Modify: `TestGame/Enemy.cpp`
- Modify: `TestGame/RoomHazardDirector.h`
- Modify: `TestGame/RoomHazardDirector.cpp`

**Interfaces:**
- Consumes: `EncounterPlan` and `EncounterSpawnEntry`.
- Produces: live-pressure-aware mixed waves and enforced projectile limits.

- [ ] **Step 1: Replace integer reinforcement queue**

Replace `_dungeonReinforcementTypeIds` with:

```cpp
std::deque<EncounterSpawnEntry> _dungeonReinforcements;
EncounterPlanDebug _encounterDebug{};
```

Change `SpawnDungeonGruntByTypeId` to `SpawnDungeonEntry(const EncounterSpawnEntry&, Vector2, float, float)`.

- [ ] **Step 2: Integrate planner into standard rooms**

Remove the inline weighted-roll and pressure-clamp block from `SpawnDungeonRoomEnemies`. Build an `EncounterRequest` from tier, biome, affix, hazards, world zone, and deterministic room seed. Spawn `plan.opening`, then move `plan.reinforcements` into the runtime queue.

Fix Ancient Castle's opening cluster to spawn each planned kind rather than replacing every entry with a basic enemy.

- [ ] **Step 3: Recalculate live danger before every wave**

Add a `CombatDirector` helper that sums active enemy pressure and specialist counts from resolved encounter profiles. Release queued entries one by one only while body, danger, and specialist caps remain valid.

The timer may request a wave but cannot bypass hard caps. If no queued entry currently fits, keep it queued and retry after the battlefield changes.

- [ ] **Step 4: Add readable spawn warnings**

Select valid spawn regions away from the player, show the existing allowed telegraph for at least `0.6s`, then materialize the wave. Enemies cannot deal contact/projectile damage during the warning/materialization window.

- [ ] **Step 5: Enforce projectile caps**

Before enemy or environmental projectile creation, query the corresponding cap. Reserve a small boss-projectile allowance during boss fights. A suppressed ranged attack returns to cooldown without creating a projectile; melee AI remains active.

- [ ] **Step 6: Update room-clear, save, and reset logic**

Room clear requires no active enemies, no pending materialization wave, and an empty reinforcement queue. Clear queues on room exit/reset and preserve survivor behavior without duplicating queued enemies.

- [ ] **Step 7: Add encounter debug telemetry**

Show target/spawned/live/queued population, live/cap pressure, specialist counts/caps, both projectile cap usages, and next-wave timer.

- [ ] **Step 8: Build and manually verify every tier**

Use debug room restart/tier controls. Confirm early `8-12`, mid `12-18`, late `18-26`, and waves respect simultaneous limits.

- [ ] **Step 9: Commit runtime integration**

```powershell
git add TestGame/Engine.h TestGame/Engine.cpp TestGame/CombatDirector.h TestGame/CombatDirector.cpp TestGame/Enemy.h TestGame/Enemy.cpp TestGame/RoomHazardDirector.h TestGame/RoomHazardDirector.cpp
git commit -m "feat: run high-density mixed reinforcement encounters"
```

---

### Task 7: Swarm Profiles and Boss Add Complexity

**Files:**
- Modify: `TestGame/Enemy.h`
- Modify: `TestGame/Enemy.cpp`
- Modify: `TestGame/Engine.h`
- Modify: `TestGame/Engine.cpp`
- Modify: `TestGame/CombatDirector.h`
- Modify: `TestGame/CombatDirector.cpp`
- Modify: `TestGame/GameBalance.h`
- Modify: `TestGame/CombatSystemsTests.cpp`

**Interfaces:**
- Produces: explicit swarm health/reward behavior and per-boss add profiles.

- [ ] **Step 1: Add swarm profile state**

Add `Enemy::ApplySwarmProfile()` that applies a one-time `0.55f` max-health multiplier after depth scaling, marks the enemy as swarm-profiled, and prevents duplicate application.

- [ ] **Step 2: Control swarm rewards and procs**

Swarm enemies retain normal combat interactions but use a reduced drop/reward contribution. Cap room-wide enemy-drop rolls to the existing economy budget and ensure kill-triggered effects cannot recursively create unbounded rewards.

- [ ] **Step 3: Define boss complexity profiles**

Create data for each boss family:

```cpp
struct BossAddProfile
{
    int totalMin;
    int totalMax;
    int activeCap;
    int pressureCap;
    int projectileReserve;
    unsigned allowedSpecialistMask;
    float reinforcementInterval;
};
```

Dense projectile/summon bosses use `4-6` total adds and lower active caps; straightforward melee bosses may use `7-10`. Existing authored boss summons count against the profile.

- [ ] **Step 4: Integrate boss add queues**

Replace unconditional boss Cyclops support with profile-driven entries. Do not spawn adds during introductory grace, invulnerable transitions, or patterns already saturating projectile/hazard caps.

- [ ] **Step 5: Add swarm and boss tests**

Assert swarm health applies once, swarm population remains `28-32`, every boss profile remains `4-10`, and projectile-heavy bosses have lower add pressure than simple melee bosses.

- [ ] **Step 6: Build and manual verification**

Test one melee boss, one projectile boss, one summoner boss, and a 32-body swarm. Build `Debug|x64`.

- [ ] **Step 7: Commit swarm and boss profiles**

```powershell
git add TestGame/Enemy.h TestGame/Enemy.cpp TestGame/Engine.h TestGame/Engine.cpp TestGame/CombatDirector.h TestGame/CombatDirector.cpp TestGame/GameBalance.h TestGame/CombatSystemsTests.cpp
git commit -m "feat: add swarm and boss add profiles"
```

---

### Task 8: Full Stress Verification and Tuning

**Files:**
- Modify only if verification exposes a requirement failure in files owned by Tasks 1-7.

**Interfaces:**
- Verifies the complete approved combat-density and damage-feedback specification.

- [ ] **Step 1: Run deterministic tests**

Expected: `CombatSystemsTests.exe` exits `0` with manager, planner, swarm, and boss-profile tests passing.

- [ ] **Step 2: Run fresh Debug x64 build**

Expected: MSBuild exit code `0`; record existing warnings separately and fix any new warning introduced by this project.

- [ ] **Step 3: Run the damage-number matrix**

Verify normal, critical, blocked, immune/invulnerable, armour, airborne/underground where applicable, backstab, ordinary kill, elite kill, boss kill, DoT merge, rapid multi-hit merge, AOE per-target numbers, multi-kill aggregation, incoming player damage, camera movement, and room clearing.

- [ ] **Step 4: Run the encounter matrix**

Verify ten generated rooms per tier, three swarm encounters, and representative bosses. Record target population, maximum simultaneous bodies, peak danger pressure, specialist peaks, projectile peaks, room-clear time, and frame rate.

- [ ] **Step 5: Verify lifecycle safety**

Change rooms, return to village, restart a run, enter the shop, trigger game over, and use debug room restart. Confirm no stale numbers, queued enemies, raw-pointer access, or phantom room-clear state.

- [ ] **Step 6: Inspect worktree scope**

Run `git diff --check` and review only project-owned files. Do not revert unrelated dirty changes.

- [ ] **Step 7: Commit final tuning if needed**

```powershell
git add TestGame/DamageNumberManager.h TestGame/DamageNumberManager.cpp TestGame/EncounterPlanner.h TestGame/EncounterPlanner.cpp TestGame/CombatSystemsTests.cpp TestGame/GameBalance.h TestGame/Enemy.h TestGame/Enemy.cpp TestGame/Engine.h TestGame/Engine.cpp TestGame/VFXManager.h TestGame/VFXManager.cpp TestGame/CombatDirector.h TestGame/CombatDirector.cpp TestGame/DebugPanel.h TestGame/DebugPanel.cpp TestGame/RoomHazardDirector.h TestGame/RoomHazardDirector.cpp TestGame/TestGame.vcxproj TestGame/TestGame.vcxproj.filters build_web.bat
git commit -m "test: tune dense combat readability"
```

## Completion Gate

This plan is complete only when:

- Every standard encounter falls inside the approved total population target.
- Swarms reach 28-32 fragile bodies without exceeding specialist/projectile safety caps.
- Boss profiles produce 4-10 adds according to complexity.
- Normal outgoing damage is red and critical outgoing damage is gold everywhere.
- Displayed damage equals actual post-defence health removed.
- Pooling, merging, priority, lifecycle clearing, and debug telemetry are verified.
- `CombatSystemsTests.exe` passes.
- `Debug|x64` builds successfully.
