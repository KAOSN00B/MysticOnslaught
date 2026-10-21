# Mystic Onslaught: engineering review and implementation handoff

Reviewed 2026-10-17 against the current working tree, including uncommitted files.

## Current scope

Updated per user direction: native development only; web support is discontinued. Do not repair, build, test, or maintain the web pipeline. Saving improvements are deferred, including checkpoint handling, save parsing, restore validation, and persistence redesign. Preserve current saving behavior during cleanup and refactoring. Existing save tests may run as regression checks. Historical web artifacts are not a deletion target in this pass.

## Scope and confidence

This was a focused architecture, persistence, build, and dead-code review, not a line-by-line audit of every enemy, asset, or editor. No gameplay code was changed or deleted. The working tree already contains extensive changes; preserve them. Historical planning and release documents are context, not proof of the current implementation.

Fresh verification: `tools/run_tests.ps1 -Suite RunSave,MetaProgression,JourneyTransition,RunProgression,DamagePolicy,RoomSelection` passed all six suites. Logs: `x64/TestRuns/20261017-084700-680`. The first attempt could not create its output folder; retry with the required filesystem access succeeded. No full native/web build or interactive playthrough was performed for this review.

## Implementation pass 2026-10-17: verification outcomes

Native-only. No saving code, save format, checkpoint handling or web artifact was changed. Every file edited in this pass was first copied to `x64/CleanupBackup-20261017/`. Nothing was committed.

**Baseline (before any edit).** `tools/run_tests.ps1`: all 36 default suites passed (`x64/TestRuns/20261017-090129-687`). `tools/build.ps1` Shipping and Release: both succeeded. Both builds were already up to date, so MSBuild did not recompile.

**Stage 1: test gate.**
- `RoomEditorTests.cpp` is registered as suite `RoomEditor` (with `RoomEditor.cpp` and the room sources). It passes.
- Discovery check: every `*Tests.cpp` under `TestGame/` and `Tests/` must be registered, or the runner throws before building. Verified with a temporary unregistered probe file, which was rejected (exit 1).
- Profile isolation: an inherited `MO_PROFILE_DIR` is removed for the run and restored afterwards. `TEMP`/`TMP` are now isolated per suite, like `LOCALAPPDATA`. Verified by running with `MO_PROFILE_DIR` pointing at a sentinel folder: the folder stayed empty and the variable was restored.
- `/UNDEBUG` is passed, so `assert` stays live even if the `CL` environment variable adds `/DNDEBUG`.
- Result: 37/37 default suites pass (`GeneratedVariants` still needs `-IncludeContent`).

**Stage 2: dead-code removal.** Evidence for each group came from a reference map (comment-stripped, name-based, conservative), checked by hand with grep. Declarations were removed with the definitions, so any surviving caller would have failed compilation. All four native configurations compiled.
- `TestGame/Game.cpp`, `Game.h`: removed from `TestGame.vcxproj` and `.filters`. No consumer existed. The files are still on disk because deleting them was blocked by the session permission policy, and the owner should delete them. Historical web definitions were left alone.
- `Engine::UpdateGamePlay` (old act-map/wave loop, no caller), plus everything only it reached: `HandleCollisions`, `HandleRoomContinueAction`, `CompleteCurrentMapNode`, `SpawnTimedPickup`, `UpdateBiomeTransition`, and the old debug-command path `DebugRestartRoomAs` / `DebugSetEliteMechanic`. The live dungeon equivalent `DebugRestartDungeonRoomAs` remains.
- Act map: `DrawMap` (no caller), `EnterMapRoom`, and 22 layout/editor fields used only by them (`_mapEditorActive`, `_mapEditorSelIdx`, `_mapJourneyX` … `_mapHintFs`, `_mapPos`). `_mapEditorActive` could never become true, so it was also dropped from the cursor check.
- EXP tally: `UpdateExpTally`, `Engine::DrawExpTally`, `OverlayRenderer::DrawExpTally`, `ExpTallyRenderContext`.
- Boss choice / game over: `OpenBossChoice` (no caller), `UpdateBossChoice`, `DrawBossChoice`, `BossChoiceCardRects`, `_bossChoiceCursor`, `_bossChoiceOpenTimer`, `PauseAndGameOver::DrawGameOver`, `_gpGameOverSelected`, `_gpGameOverStickCooldown`. The `GameOver`/`BossChoice` cases were also removed from the Update and Draw switches: the retired-state guard returns before the Update switch, and after this removal no code assigns either state, directly or through `_stateBeforePause`, `_levelUpReturnState`, `_stateBeforeSettings`, `_metaShopReturnState`, `_classSelectReturnState` or `_grimoireReturnState`.
- Methods with no caller at all: `DrawWaveIntro`, `DrawRoomAffixBanner`, `SpawnSpreadBurst`, `SpawnBolt`, `TriggerUltimateSequence`, `RespawnOutOfBoundsEnemies`, `GetBiomeTransitionAlpha`, `TryGetPooledEnemySpawn`, `TryGetPooledCyclopsSpawn`, `TryGetPooledOgreSpawn`, `GetCyclopsSpawnCountForWave`, `GetOgreSpawnCountForWave`, `VillageObjectHasSolidAt`.
- Size: `Engine.cpp` went from 25,037 to 22,984 lines, including the new `ApplyPauseAction`.
- Kept deliberately: the `GameState` enum values, which `AudioManager` and its tests still use; `IsRetiredGameState` / `RecoverFromRetiredState`; `AudioManager`'s Play/Map/ExpTally/GameOver handling; and all save/compatibility readers.
- Re-running the reference map after removal finds no further unreferenced Engine methods and no newly orphaned fields.

**Stage 3: pause actions moved from Draw to Update.**
- `PauseAndGameOver::DrawPause` was split into `UpdatePause()` (input, editor drags, gamepad navigation, clicks; returns the action) and `DrawPause(promptMode, runInProgress) const`. Because the draw is const, the compiler now guarantees drawing cannot change pause-menu state.
- `Engine::ApplyPauseAction` holds the unchanged action dispatch, including the original `SaveRunCheckpoint()` / `SaveAndQuitRunToMenu()` / `AbandonRunToMenu()` calls. It runs from the Pause case of `Update`, after the existing Esc handling. The Draw case now only renders.
- Small timing differences, both stricter than before: (a) a click on the very frame the pause menu opens is no longer applied immediately, which removes a possible click-through; (b) pause input is not read during the journey reveal, because `Update` already returns while a journey is active.
- Appearance: the flow self-test's `pause_menu` screenshots before and after show an identical panel, labels and colors. Only the randomly chosen room behind the menu differs.
- Regression checks added to flow phase 6: "pause: no choice keeps the pause menu open" and "pause: Resume returns to the paused run".

**Builds and flows after stages 2–3.**
- `tools/build.ps1` Shipping, Release, PublicDemo and Debug: all succeed with 0 errors. Each configuration shows 6 C4244 warnings, all on unchanged existing lines.
- `tools/run_tests.ps1`: 37/37 pass (`x64/TestRuns/20261017-093139-841`).
- `tools/run_flow_selftest.ps1` (Release): phases 1–6 pass, 283 checks (`x64/FlowTest/20261017-093427`). These cover menu → village → dungeon → room clear → boss/relic → crossroads → death → Poe → class select → victory, persistence across relaunch, a tour of every domain, save/resume (phases 4–5), and room events.
- Known flake: on the stage-2 build, one phase-6 check failed once: "relics: Retaliation after a hit (2.03 -> 2.03, HP 5.8)". The same exe then passed phase 6 three times in a row, and the stage-3 run passed it. The check depends on frame timing (heal one frame, hit the next, then read the hurt detector in `Update`). It is unrelated to the removed code, since the `NotifyHurt` site is unchanged. Worth making deterministic.

**Stage 4: release hygiene.**
- `tools/verify_native.ps1` is a documented, reproducible native gate: it builds Shipping, PublicDemo and Release, runs all suites, and with `-Flow` also runs the flow self-test. No GitHub workflow was added: `tools/` and many sources are still untracked, and the flow test needs a real window, so a hosted workflow could not be verified from here.
- `build.ps1` writes `source.sha256` beside the exe (`Get-MoSourceFingerprint` in `MoToolchain.ps1`). `package_shipping.ps1` refuses a build whose stamp does not match the current source, and falls back to the old timestamp check for unstamped builds. It also records `sourceFingerprint` in `version.json`.
- Package exclusions now cover `run_save.*`, `run_history.*`, `flowtest_*`, the `shots/` folder, and the existing profile, settings, bindings, village, `.tmp` and `.bak` files. Packaging stops if any `UserDataPath("<name>")` literal in the source is not covered, so a future profile file cannot ship unnoticed.
- README build/test/package instructions now describe the `tools/` workflow. Web support is labelled discontinued, and the `Makefile` is marked obsolete there and in its own header.
- Verified:
  - `package_shipping.ps1 -Build` produced `x64/Packages/MysticOnslaught-20261017-093700` with 0 personal-data files, and `version.json` carries the fingerprint.
  - `smoke_shipping.ps1` passed on that package (packaged startup and clean shutdown; 7 known legacy-prop warnings).
  - A build folder with a mismatched `source.sha256` was refused ("Stale build"), and no package was created.

**Stage 5: measurement only, no performance changes.**
- Measured from the existing `JOURNEY [...]: preparation stage` log line across the stage-2 flow run: 68 stages, mean 187 ms, maximum 673 ms. 43 stages took under 50 ms, 6 took 50–200 ms, and 19 took 500 ms or more.
- 14 of those 19 slow stages uploaded 0 tile sheets (for example Lost City at 626 ms, Jungle at 614 ms). Texture upload is not the main cost; something else inside those synchronous stages is. Profile those stages first.
- Worst-case combat frame time was **not measured**: there is no frame-time instrumentation yet.

## Continuation pass 2026-10-17 (Codex, then Claude): outcomes

Native-only. No saving code, save format, checkpoint handling, restore rule or progression semantics changed; no web artifact touched; nothing committed. Claude copied every file it edited in this pass to `x64/CleanupBackup-20261017-session2/` first.

**Codex's extraction, verified before further edits.** Codex moved Keybindings redirection, help-tab/back input (`UpdateHowToPlayMouse`), ability-bar clicks (`UpdateAbilityBarInput`), level-up, starter and ability-choice selection (`UpdateLevelUpChoiceInput`, `UpdateStartingAbilityChoiceInput`, `UpdateAbilityChoiceInput`/`FinishAbilityChoice`) out of Draw, and deleted `Game.cpp`/`Game.h`. Its handoff had no final verification, so `tools/verify_native.ps1 -Flow` was run on its tree before anything else: Shipping, PublicDemo, Release, 37/37 suites, flow phases 1–6 all passed (phase 6: 65 checks, `x64/FlowTest/20261017-102926`). The exes postdated the last source edit.

**Stage A: render/update separation finished.**
- `DrawHUD` and the helpers it calls (`DrawAbilityBar`, `DrawTouchAbilityArc`, `DrawDebugToggleTab`) are now `const`, so the compiler enforces that drawing the HUD cannot change engine state. `GetAbilityIcon` gained a `const` overload.
- Gold/Echoes counter smoothing, HP/MP orb smoothing, the three HUD toast countdowns (Echoes banked, contract, relic) and HUD-editor input (9 toggle, select, nudge, S export) moved to `UpdateHudPresentation(frameDt)`. `RunFrame` calls it once per presented frame, just before `Draw` and under the same journey condition, but only when `IsHudDrawnThisFrame()`, which mirrors every `DrawHUD` call site: village outside interiors (and pause over the village), dungeon run when not scrolling, level-up, ability choice, touch layout. Toasts and smoothing keep their old per-drawn-frame timing, including during the journey reveal that `Update` skips.
- The HUD editor's 54 rows are one table, `kHudEditorFields` (label + `HUDConfig` member pointer), shared by the input and the panel. It replaces two parallel 54-entry arrays.
- Touch layout: `UpdateTouchButtonMapping()` (drag + Back/Save/Default hit test) runs from the TouchButtonMapping case of `Update`; `ApplyTouchButtonMappingAction()` holds the dispatch that used to sit in `Engine::Draw`, unchanged; `DrawTouchButtonMapping()` is `const`. One helper, `TouchMappingButtonRect`, gives both the button geometry. A press that starts a drag still never clicks a centre button.
- Removed: the unreachable legacy HUD after `return;` in `DrawHUD` (124 lines), and `OverlayRenderer::DrawHowToPlay` with `HowToPlayRenderContext` (no caller anywhere; `Engine::DrawHowToPlay` renders inline). The latter was the last Draw path still applying clicks, through callbacks.
- Transitive audit: every `Draw*` body was scanned for input reads, frame-time use and member writes (the scan reproduces the known inventory on the backups). No Draw path now applies input or changes gameplay state. Presentation-only state still advances per draw call: the room intro banner timer (`DrawRoomIntroBanner`; read only by `DrawRoomBadge`), the cell pickup bob, the Shieldbearer block flash and `TileRenderer`'s animation-start cache. `MainMenu::Draw` logs its dev border-editor export on S. `DrawSettings` and `WorldMapManager::Draw` read touch/mouse state for display only and are `const`. The scan only covers functions named `Draw*`, so it is a lead list, not a proof. The journey-cover guard in `RunFrame` stays; its comment now gives these reasons.
- Phase-6 checks added: repeated HUD draw leaves toasts and counters alone / the update advances them; touch layout draw neither moves buttons nor navigates / Default restores the captured layout / Back returns to pause; toasts count down on real dungeon frames (exercises the `RunFrame` wiring).

**Stage B: dead code removed.** Whole-word reference counts across `TestGame/` and `Tests/`, then builds.
- `Engine::StartNextRoom` and `Engine::GenerateActMap` had no callers; their namesakes in `RoomDirector`/`MapDirector` had confused the earlier name-based map. Also removed: the `SpawnWave` declaration (never defined) and the nested `Engine::MapNode`.
- Write-only fields (only declarations, resets or assignments remained): `_actMap`, `_currentMapNodeIdx`, `_mapKeySelectedIdx`, `_mapOpenTimer` (plus its write when a level-up returned to the retired Map state), `_biomeTransitionActive/Swapped/Timer`, `kBiomeFadeOutDuration`/`kBiomeFadeInDuration`, `_waveIntroTimer`, `_waveIntroDuration`, `_expTallyDone`, `_tallyStartLevel`, `_tallyChoiceChaining`. Kept because they are still read: `_waveStarting`, `_currentRoom`, `_pendingBiome`, `_expTallyAccum`, `_tallyLevelUpsRemaining`, `_eliteCageRadius`.
- `MapDirector.cpp/.h` and `RoomDirector.cpp/.h` are referenced only by each other (not by Engine, Tests or tools), so they were removed from `TestGame.vcxproj` and `.filters`. The files stay on disk because the historical `build_web.bat` lists them. `GameTypes.h`'s `MapNode` is used only by them; delete it together with those files.
- `Engine.cpp`: 23,044 → 22,807 lines (this pass, including the additions).

**Stage C: phase-6 Retaliation flake diagnosed and fixed.**
- Cause: Retaliation's window is 3 s (`kRetaliationDuration`), but step 12 waited only 2.5 s after step 11's Last Stand hit. When an earlier random relic pick in phase 6 had already granted Retaliation, that lethal hit opened a window still running at the baseline, so baseline and after were both buffed (the recorded 2.03 → 2.03). `BaseCharacter::TakeDamage` ignores i-frames, so the test hit always lands; the run log does not record random picks, so the original run cannot be attributed directly.
- Reproduced deterministically by granting Retaliation before step 11 (temporary line): `FAIL relics: Retaliation after a hit (2.11 -> 2.11, HP 5.8)` (`x64/FlowTestPhase/20261017-103937-red-old-check`).
- Fix: step 12 waits until `Character::IsRetaliationActive()` (new `const` getter) is false. The check asserts the window is open after the hit, still via the real hurt detector in `Engine::Update` (no direct `NotifyHurt`), and compares crit-free multiplier samples because Deadeye rolls random crits. With the forced grant: `PASS ... (window open, 1.62 -> 2.11, HP 5.8)`, 71 checks (`x64/FlowTestPhase/20261017-104051-green-forced`). The temporary line was removed.

**Stage D: performance measurement (no optimization made).**
- New dev-only `JOURNEY detail` lines split the slow domain-entry stage. `RoomLibrary::Refresh` re-parses all 269 `.mroom` files (7.5 MB) on every domain entry: 616–649 ms each time across the flow run, about 95% of the stage. Dungeon layout generation takes 0–6 ms (1–217 attempts), the asset catalog about 7 ms, and the renderer catalog about 50 ms on first load, 1 ms after. This is the demonstrated bottleneck. Caching the parsed library and re-reading only when a `.mroom` changes (or the room editor saves) would remove most of it, but the editors rely on the refresh to pick up edits, so the invalidation rule is the owner's call.
- New dev-only `FRAMETIME` summary (logged on shutdown): Update CPU, Draw CPU (submission only; GPU time not measured) and frame interval for live dungeon frames, bucketed by active enemies. The flow run only reaches light combat: about 130 frames with 1–9 enemies, CPU p99 ≤ 2 ms, draw p99 ≈ 0.45 ms, interval p50 16.67 ms (vsync). The 150–190 ms interval spikes fall on 0-enemy frames (room entry and flow screenshots). **Dense combat (10+ enemies) was not measured**: the flow test kills enemies quickly. Run a Release build from a console through a busy elite/boss room and read the `FRAMETIME` lines on exit.

**Stage E: packaging audio inventory and guard.**
- 115 files sit under `Sounds|Music/MysticExpansion`. Source names 38 of them; 77 are pruned. No source path is built at runtime.
- Neither manifest is read at runtime. `Sounds/MysticExpansion/manifest.json` is the import/provenance record (109 entries). `Music/music_manifest.json` is build provenance for the 9 converted biome loops (`Music/*.ogg`, shipped with the whole `Music` folder). Neither can replace the source scan.
- Change: the scan runs before the package folder is created, counts only quoted literals (the same 38 files as before), and stops packaging on a built path (format specifier or folder prefix) or on a named clip missing from disk. Tested on throwaway sandbox sources (valid, format, prefix, missing).

**Builds and flows after stages A–E.** `tools/verify_native.ps1 -Flow`: Shipping, PublicDemo and Release build with the same 6 pre-existing C4244 warnings; 37/37 suites (`x64/TestRuns/20261017-105055-415`); flow phases 1–6 pass, 297 checks (92 + 11 + 101 + 8 + 14 + 71, `x64/FlowTest/20261017-105343`). Final run on the final source (only a `RunFrame` comment changed since): the gate passed again (37/37 suites `x64/TestRuns/20261017-105616-586`; flow 297 checks, 0 failures, `x64/FlowTest/20261017-105912`); `tools/build.ps1 -Configuration Debug` succeeded (its full rebuild shows C4244 warnings in untouched enemy files, plus the same 6 in `Engine.cpp`); `package_shipping.ps1` produced `x64/Packages/MysticOnslaught-20261017-110126` with the 38 referenced MysticExpansion clips, 0 personal-data files and the source fingerprint; `smoke_shipping.ps1` passed on it (7 known legacy-prop warnings, `x64/ShippingSmoke/20261017-110204-815`).

### Continuation list

1. Delete from disk: `TestGame/MapDirector.cpp/.h`, `TestGame/RoomDirector.cpp/.h` (already out of the native build) and `GameTypes.h`'s `MapNode`. `Game.cpp`/`Game.h` are already gone.
2. Decide the `RoomLibrary` refresh policy (cache + invalidate on change/editor save) to remove the ~620 ms per domain entry measured above.
3. Measure dense combat with the `FRAMETIME` summary in a manual Release session before any combat optimization.
4. Optional: move the remaining presentation timers (room intro banner, pickup bob, shield flash) out of Draw if the journey cover should ever redraw the scene.
5. Pre-existing, deliberately unchanged: controller starter selection can select and confirm on the same A press; `Character.cpp` blocks melee clicks with hardcoded ability-bar bounds while the HUD uses configurable geometry.
6. Optional hosted CI once `tools/` and the sources are committed (download raylib 5.5 MSVC, set `MO_RAYLIB_DIR`, run `tools/verify_native.ps1` without `-Flow`).
7. Deferred saving backlog (below): done by the save rework on 2026-10-17. See "Save rework pass" above and `docs/SAVE_FORMAT.md`.
8. Next: the progression overhaul, `docs/PROGRESSION_OVERHAUL.md` (decisions, open questions, build order).

## Save rework pass 2026-10-17 (Claude): outcomes

Overhaul build step 1: the save rework, the run log and cause-of-death tracking. The format is described in `docs/SAVE_FORMAT.md`. Every edited file was first copied to `x64/CleanupBackup-20261017-saves/`. Nothing was committed.

**Robert's decisions:** in-house JSON, in-house SHA-256/HMAC (checked against the NIST and RFC 4231 vectors), stable string ids, and settings/keybindings as plain JSON starting from defaults.

**Changes, in five patches, each through the gate:**

1. `Sha256`, `Json` (strict parser, deterministic writer, `JsonReader` validation) and `SaveStore` (signed envelope, size cap before reading, write-verify, atomic write with backup, rejected files renamed to `.rejected`, backup restore, a short retry on copy/rename, and a test hook that forces write failures). Suites: `Sha256`, `Json`, `SaveStore`.
2. The profile becomes `Saves/profile.mosave`, using stable names from `SaveIds.cpp`. `MetaProgressionTests` was rewritten (clean slate, hostile signed documents, tampering, write failures). The web localStorage branch was removed.
3. The run checkpoint becomes `Saves/run.mosave` (`RunCheckpoint`, with full validation including the room-graph rules).
   - `SaveRunCheckpoint` returns success or failure. Save & Quit and Quit stay paused with a notice when it fails.
   - The first window close after a failed write is refused.
   - Run ids in the profile stop an ended run, or a backup of it, from being continued.
   - Suite: `RunCheckpoint`.
4. `settings.json` and `keybindings.json` are plain JSON: types are checked, out-of-range values clamped, and the backup is used when a file is broken. Suite: `SettingsDefaults`.
5. `DamageSource`:
   - Enemy turns are scoped in `CombatDirector`, and spawned projectiles are credited to the enemy that fired them.
   - Elite zones, hazards, pits and poison clouds name themselves.
   - Burn and poison credit whoever applied them.

   `RunLog` (`Saves/runlog.mosave`, newest 100 runs plus totals): per-domain rooms and time, the build, and for a death its cause, statuses and situation. The Grimoire chronicle reads it. The profile gets lifetime death tallies. Suite: `RunLog`.

**Backlog items closed:**
- A failed checkpoint write no longer discards the run.
- A failed `EndRunCheckpoint` can no longer leave a resumable run: the profile records the end.
- Numbers must be finite and are range-checked.
- Every count and list is bounded.
- The size is checked before reading.
- References are checked, including room-graph connectivity and symmetric doors.

**Bugs the new checks found during the work:**
- Before the first crossroads, the roads hold `PathBoon::Count` ("no reward yet"). The validator rejected that, so checkpoints weren't written. Write-verify caught it.
- My new close loop could hang a programmatic exit. Now only a real window close is refused.
- Every flow phase now asserts that no checkpoint write failed except the ones it injects on purpose.

**Verified:**
- `tools/verify_native.ps1 -Flow` on the final source: Shipping, PublicDemo and Release built; every suite passed (`x64/TestRuns/20261017-185130-773`).
- Flow phases 1–6: 313 checks, 0 failures (`x64/FlowTest/20261017-185449`). New flow checks:
  - Save failure keeps the run.
  - A tampered checkpoint falls back to its backup.
  - An ended run can't be revived.
  - The run log records the run, the domains and the build.
  - A death by burn is credited to the Infernal that applied it.
  - An enemy shot is credited to the shooter.
  - The profile tallies deaths.
- `tools/build.ps1 -Configuration Debug` succeeded.
- `package_shipping.ps1` produced `x64/Packages/MysticOnslaught-20261017-185730` (source fingerprint stamped, 0 save files), and `smoke_shipping.ps1` passed on it (7 known legacy-prop warnings).

**Not verified:**
- A real playthrough in the Shipping profile.
- A real window close while a write is failing. raylib's close flag was not checked; if it is sticky, the second close happens at once, which is the previous behaviour.
- A death from an enemy's own attack inside the `CombatDirector` scope. Only the projectile and status paths are exercised by the flow test.
- Steam Cloud configuration.

### Continuation list (save rework)

1. Delete from disk: `TestGame/RunSave.cpp`, `RunSave.h`, `RunSaveTests.cpp`, and their `RunSave` line in `tools/run_tests.ps1`. They are out of the game build but still pass as a suite.
2. `TestGame/ProjectileSystem.cpp/.h` are compiled but nothing references them. A dead-code candidate, left untouched.
3. Steam Cloud: point Auto-Cloud at `%LOCALAPPDATA%/MysticOnslaught/Saves`. Decide whether `settings.json` should sync (window size is per machine).
4. Lessons 41–50 may need values the death record doesn't have yet (for example "no dash for 30 s" uses `secondsSinceDash`; check each lesson against `RunLogDeath`).
5. Add the reserved profile sections (renown, unlockTracks, skins, gear, lessons, merchants, village.clock) as optional objects when their features land.

## Bugs and performance pass 2026-10-19 (Claude): outcomes

Robert asked for items 5 (known bugs and leftovers) and 6 (performance) of `docs/REMAINING_WORK.md`. Every edited file was first copied to `x64/CleanupBackup-20261019-bugsperf/` (`RoomAffix.h` as its last-committed copy, `RoomAffix.h.HEAD`, which matched the working file). Nothing was committed.

**Pre-existing regression found by the baseline (not fixed; Robert's call).** On 2026-10-18 at 12:38, during an unfinished village art change (building PNGs redrawn, colliders refit, a new `ZephSpawn` asset), every `object` line was removed from `VillageAssets/VillageLayout.vlayout`. The code still reads the core buildings from that file, so the village loads with no homes, Trophy Hall, Armory or Zeph's shop (only the gate is repaired). The `VillageLayoutData` suite asserts, and flow phases 1 and 2 fail 4 village checks. Baseline before any edit: phase 1 92 checks/3 failures, phase 2 11/1, phases 3–6 pass (`x64/FlowTestAll/20261018-174401-baseline`). This pass was verified against that baseline: the same 4 failures and nothing new.

**Bugs fixed:**
1. **Controller starter pick:** one A press selected and confirmed at once. Now the first A selects and a second A on the selected card confirms; the button reads "A again: start with this" on a controller (`Engine::UpdateStartingAbilityChoiceInput`).
2. **Ability-bar clicks also swung melee:** `Character::HandleAttackInput` blocked clicks with hardcoded 80 px slots, while the HUD draws 103 px slots 108 px apart, so clicks on slots 1 and 4 cast and swung. `Engine::AbilitySlotRect` is now the one geometry for drawing, slot clicks and `ClickLandsOnAbilityBar`, and the Engine tells the player before each update (`Character::SetAttackClickConsumed`).
3. **Timers that ran inside Draw:** the room intro banner counts down in `UpdateHudPresentation` (its draw is now `const`); cell pickups bob from `GetTime()` plus a per-pickup phase; the Shieldbearer's block spark fades by the time since the block.
4. **Stale `RELEASE_PLAN.md` items re-checked** (results table added to that doc). Still broken, fixed now:
   - P1-4: the authored swing box ignored reach upgrades, so reach cards did nothing for melee classes. It now grows outward by `range / kBaseAttackRangeMultiplier` (1.5).
   - P1-8: a lifesteal grant replaced the strength but kept the longer timer. Drain Life (60%, 0.3 s) cast during Demon Form (20%, 8 s) gave 60% for 8 s, and Soul Siphon's 40% strike-heal was overwritten by its own 25% linger. Now the strongest grant applies for its own duration and a longer, weaker one takes over afterwards.
   - P1-13: `_currentRoom` counted every room entry, so walking back through cleared rooms raised level-up/treasure rarity and the encounter tier. Only first visits count now.
   - P1-14: an uncleared room re-rolled its affix on every entry. The affix is now kept per room (not in the run checkpoint: a resumed run rolls it once more).
   - P1-18: the Ascension picker takes LB/RB, and a hint shows the keys.
   - P2-4: the seven legacy arena props (Pillar, Torch, PillarTorch, Tree, SmallTree, Rock, BigRock) were never in the art pack. Their loads, fields and the two legacy helpers that used them (`PopulatePropsForBiome`, `GetRandomPropPosition`) are removed, and `tools/smoke_shipping.ps1` no longer whitelists them: any startup warning now fails the smoke test.
5. **Rooms falling back to random generation:** none. No `ROOM:`/`DUNGEON:` warnings in any flow phase, and the `ForestVariants` suite builds all 269 library rooms into 260 dungeons across 10 biomes.
6. **New phase-6 checks** (82 checks now): starter pick needs two presses; lifesteal overlap both ways; a reach card lengthens the swing; the room banner counts down only in update.

**Performance:**
- **Room library:** `RoomLibrary::Refresh` keeps each file's size and last-write time and re-parses only changed, added or removed files. The editors still see every save, because they call `Refresh` after saving. Domain-entry library load ~600 ms → ~4 ms; the whole domain-entry stage ~600 ms → ~15 ms (phase 3 tour, `x64/FlowTestPhase/20261018-181144-roomcache`). The first load of a session still parses everything (~580 ms, behind the journey cover). `RoomLibraryTests` covers no-change, single-file change and restore. Limitation: a file rewritten at the same size within the same filesystem clock tick (~15 ms) as its previous write would be missed; a person saving in the editor can't do that.
- **Dense combat**, measured with the new on-demand flow phase 7 (`tools/run_combat_stress.ps1`, Release; not in the gate): mixed groups of 10, 25 and 50 live enemies around an invulnerable Warrior, 12 s each; screenshots confirm the swarm is engaging (`x64/CombatStress/20261018-181632`). Update+Draw CPU per frame: 10–24 enemies p50 0.33 / p99 0.81 ms; 25–49 p50 0.41 / p99 0.89 ms; 50+ p50 0.58 / p99 1.12 ms; frame interval p50 16.67 ms (vsync). GPU time isn't measured (draw submission only), and this is the dev PC only. **These numbers show no combat bottleneck, so no combat optimization was made.** FRAMETIME buckets are now 0 / 1–9 / 10–24 / 25–49 / 50+.

**Verification (final source):**
- `tools/verify_native.ps1 -Flow`: Shipping, PublicDemo and Release built. One suite failed: `VillageLayoutData`, the pre-existing village regression (`x64/TestRuns/20261018-182014-549`). The flow step stopped after phase 1's 3 known village failures (`x64/FlowTest/20261018-182138`).
- All six phases run without stopping on failure (`x64/FlowTestAll/20261018-182152-final`): phase 1 92 checks/3 failures and phase 2 11/1 (the same village checks as the baseline); phases 3 (102), 4 (13), 5 (18) and 6 (82, including the 7 new checks) passed with 0 failures.
- The room-source suites, including the slow content validation, passed (13/13, `x64/TestRuns/20261018-180856-063`).
- `tools/build.ps1 -Configuration Debug` succeeded (an LNK4098 MSVCRT default-library warning showed in the Debug link).
- `package_shipping.ps1` produced `x64/Packages/MysticOnslaught-20261018-182402`. The stricter `smoke_shipping.ps1` passed with no startup warnings (`x64/ShippingSmoke/20261018-182415-793`).
- Not verified: a real playthrough by hand; controller input on real hardware (the starter pick and Ascension LB/RB were checked by simulated input and code only).

### Continuation list (bugs and performance pass)

1. **Robert:** restore or redesign `VillageAssets/VillageLayout.vlayout`; if the town changes on purpose, update `VillageLayoutDataTests.cpp` and the flow checks to match.
2. `RELEASE_PLAN.md` P1-16 (descriptions vs behaviour) can't be re-checked: its list (FEATURE_INVENTORY) no longer exists. Fold it into the frontend/text pass.
3. Optional: load the room library once at startup, so even the first domain entry is quick.
4. Optional: run `tools/run_combat_stress.ps1` on a low-end PC and a Steam Deck.
5. Dead-code candidates seen: `_maxActiveEnemies` (never read); the legacy arena map (`ApplyBiome` still loads `Map.png`/`ForestMap.png` at run reset); `_props` loops in projectile code (always empty in dungeons now).

## Project understanding

- C++17/raylib game, with Visual Studio native configurations and a separate Emscripten build. `TestGame/main.cpp` owns `Engine` and closes the graphics/audio devices after engine destruction.
- The current full-game flow centers on menu/class selection, village, dungeon domains, crossroads/world-map routing, death/run summary, and victory. PublicDemo and developer configurations have different behavior. `Engine::IsRetiredGameState` explicitly retires Play, Map, ExpTally, GameOver, and BossChoice, despite substantial older implementations remaining.
- `Engine` coordinates state, player abilities, rooms, progression, UI, assets, and tools. It owns enemies/pickups through `unique_ptr` collections and passes references, callbacks, and observer pointers into combat subsystems. `CombatDirector`, `EncounterPlanner`, engagement/separation/pacing modules, projectiles, enemy subclasses, and navigation implement combat.
- `DungeonGen` creates the room graph; `RoomLibrary` assigns compatible authored layouts. Blueprints, tile definitions/catalogs, collision, and rendering turn `.mroom` data into playable rooms. Generated biome content and the Python pipeline extend authored content.
- `Character`, `PlayerClass`, ability tuning, relics, and balance policies describe the player's build. `RunSession` tracks run progress, while `Engine` also carries overlapping zone/completion state. `MetaProgression` stores permanent progress. `RunSave` and `EngineRunSave` serialize suspended runs and rebuild their rooms/player state.
- `JourneyTransition` stages main-thread preparation behind a presentation barrier. `TextureCache` shares tile textures. Audio routing/transitions, resource ownership, and many gameplay policies already have targeted tests: preserve these improvements.
- Editors are embedded in the application. Native build/test/package scripts exist under `tools/`; the web script and old Makefile are separate build definitions. Historical distributable folders and tracked generated binaries coexist with source.

## Prioritized findings

### Deferred saving backlog: failed checkpoint writes

Recorded for the later saving task; do not implement changes now.

Evidence: `EngineRunSave.cpp:175-187,217-225`; pause quit in `Engine.cpp:5345`; window-loop exit around `Engine.cpp:1952`.

`SaveRunCheckpoint()` returns void and only logs a failed write. `SaveAndQuitRunToMenu()` then resets run state and changes screens regardless. Pause quit likewise sets `_shouldClose` without observing success. A failed save can therefore discard current in-memory progress and leave only an older checkpoint (or none).

`EndRunCheckpoint()` sets only an in-memory ended flag before trying to write an inactive file. If that write fails, an earlier active checkpoint remains resumable on the next launch. This is a confirmed control-flow gap; disk-failure behavior was not exercised in an engine integration test here. Existing RunSave tests prove serializer/atomic replacement behavior, not these engine decisions.

### Out of scope: historical web build drift

The native `.vcxproj` includes these files absent from `build_web.bat`: `EngineFlowSelfTest.cpp`, `EngineRunSave.cpp`, `RunSave.cpp`, `EngineRoomEvents.cpp`, `EngineCrossroads.cpp`, `EngineRunSummary.cpp`, `EngineGrimoire.cpp`, `Grimoire.cpp`.

This is a historical static finding, not an observed Emscripten linker result. Web support has been discontinued, so these omissions require no action.

The old `Makefile` additionally advertises C++14 and discovers C sources rather than the current `TestGame/*.cpp` application. It does not describe the current C++17/filesystem build. Native toolchain discovery already exists; reuse it instead of claiming all builds still require hardcoded paths. Web tooling still hardcodes local SDK paths.

### Deferred saving backlog: semantic and resource bounds

Recorded for the later saving task; do not implement changes now.

Evidence: `RunSave.cpp:GetInt/GetFloat/Deserialize/ReadFrom`, `Character.cpp:RestoreRunState`, `EngineRunSave.cpp:ResumeSavedRun`, `DungeonGen.cpp:Restore`.

- Numeric parsing checks consumption but not overflow or finite floats. `nan`/`inf` and overflow values can pass the numeric accessors. A checksum detects accidental byte changes; it does not validate legal game state.
- Version parsing uses `atoi`, so a checksum-valid malformed version string with a valid numeric prefix can pass.
- `ReadFrom` reads the entire file before enforcing its 1 MiB cap.
- Character relic restoration loops to the saved count while limiting accepted unique relics. A very large count containing invalid/duplicate IDs can cause excessive iteration; bound examined entries independently of accepted entries.
- Some enum/count fields are checked, but zone, modifiers, stats, room types, and restored graph semantics are not uniformly validated. `DungeonGen::Restore` checks indices/coordinates/duplicates, not connectivity or door consistency. Validate against the actual graph rules; intentional one-way entrance behavior must remain legal.

### P2: state ownership and render-time mutations complicate correctness

`Engine.cpp` has 25,036 lines; `Engine.h` has 1,983. Recent `Engine*.cpp` splits help navigation but still share the same large mutable class. `RunSession` and engine fields overlap, and save code serializes parallel representations of progress. This invites divergence; it is an architectural risk, not a proven divergence bug.

The pause Draw branch processes input, saves, closes, and changes state. `RunFrame` explicitly avoids drawing some menus under the loading cover because drawing can handle input. Extract screen actions into Update and make Draw observational, preserving existing save-call behavior. Then extract cohesive non-persistence state owners incrementally, beginning with menu/pause actions. Defer consolidation that requires changing save contracts or persistence architecture. Do not replace the whole engine or introduce an ECS just to reduce file length.

### P2: test/build coverage has identifiable gaps

`tools/run_tests.ps1` claims to run every standalone suite but omits `RoomEditorTests.cpp`. No CI workflow was found under `.github` (only copilot instructions). Add coverage/discovery checks so new suites cannot silently disappear. Keep assertions active in test configurations. Isolate both `LOCALAPPDATA` and `MO_PROFILE_DIR`: `UserDataPath` gives the latter precedence, whereas the runner currently isolates only the former.

Unit tests should be complemented by game-flow checks and native Shipping/developer build checks. Save failure/resume coverage improvements belong to the deferred saving task. Existing flow self-tests are a useful foundation, not a reason to invent a second harness.

### P3: release hygiene and performance measurement

Tracked historical executables/web data and configs exist in `Mystic Onslaught/`, `MysticOnslaught_Windows/`, `web_build/`, and `TestGame/`. Separate authored defaults, personal profiles, release archives, and reproducible output. Do not delete archives or rewrite history without a dedicated decision. Update README build/test instructions to the current scripts.

`package_shipping.ps1` already excludes several personal file names and creates a hash manifest. Expand the exclusion review to run checkpoints/history and future profile files. Its source-regex audio selection and timestamp-based build freshness are weaker than an explicit runtime asset manifest and source/config fingerprint; improve these incrementally.

Profile room preparation and combat before optimizing. Journey stages are synchronous, so a single large stage can still hitch even though the cover is displayed. Measure stage duration, texture uploads, and worst-case enemy/projectile frame time. No performance regression was measured here.

## Dead-code assessment

| Candidate | Evidence | Recommended action |
|---|---|---|
| `TestGame/Game.cpp`, `Game.h` | Tiny prototype window/loop; project source search found no consumer beyond its own definitions. Real entry point uses Engine. Still listed in native/web builds and `.vcxproj.filters`. | Highest-confidence removal candidate. Remove files and active native build/filter references together, then build. Historical web definitions are outside this pass. **2026-10-17:** removed from `.vcxproj`/`.filters`, and all configurations build. The files themselves still need deleting. |
| Old BossChoice update path and old Play/Map/ExpTally/GameOver machinery | `Engine.cpp:2215` recovers retired states before the Update switch; `IsRetiredGameState` is at approximately line 12526. Older handlers/render branches remain. | Trace writers and shared helpers before deleting. Draw can still see states assigned during Update, so the guard alone does not prove every old render branch unreachable. **2026-10-17:** `UpdateGamePlay`, act map, EXP tally, boss choice and game-over groups removed after a writer trace (see outcomes). `GameState` values, the retired-state guard and `AudioManager` handling were kept. |
| Old act-map generation, scripted prologue paths, wave-loop helpers | Retired flow and comments suggest leftovers, but shared combat, developer tooling, and tests may still consume pieces. | Candidates only; build a reference map and remove in small verified groups. **2026-10-17 (continuation):** `StartNextRoom`, `GenerateActMap`, `SpawnWave`, `Engine::MapNode` and the write-only act-map, biome-fade and tally fields removed; `MapDirector`/`RoomDirector` taken out of the native build (files still on disk). Prologue paths not audited. |
| Legacy enums, tuning IDs, metadata readers | Some explicitly preserve save/content compatibility. | Retain or migrate deliberately. A legacy label does not make code dead. |

No deletion was made during this review: source/build files already have extensive user changes, and the selected unit tests do not establish full-build safety for removal.

## Ready-to-use prompt for the next AI agent

You are the senior C++/raylib engineer working in `B:\GamesMadeInEngines\MysticOnslaught-master`. Implement the active native-only improvements in this review. Web support is discontinued: do not repair, build, test, or maintain it. Saving improvements are deferred: do not change checkpoint handling, save parsing, restore validation, save formats, or persistence architecture. Preserve current saving behavior and use existing save tests as regression checks. Leave historical web artifacts in place. Read the actual current code before editing; line numbers and findings are a review baseline, not an instruction to patch blindly. Preserve all pre-existing modified/untracked work. Do not reset, clean, mass-format, rewrite the engine, or remove assets based only on text-search absence. Keep work and tool output focused.

Work in these bounded stages, validating each before expanding scope:

1. Establish the current native build/test baseline using the existing tools scripts. Register RoomEditorTests, check suite discovery, and isolate inherited profile overrides so tests cannot write to real player data. Acceptance: native Shipping and developer builds and relevant suites pass; unavailable checks are reported honestly.

2. Remove the unused Game prototype and corresponding active native project/filter entries after confirming no consumer. Audit retired-state code with a call/reference map, distinguishing dead paths from shared helpers, developer features, and compatibility readers. Delete only proven-dead groups. Leave historical web build definitions alone. Acceptance: native builds, relevant unit tests, and existing flow checks pass after cleanup.

3. Extract pause/menu actions from Draw into Update without changing controls, appearance, or saving behavior. Continue focused extraction into components that own cohesive non-persistence state; avoid merely spreading the same mutable state across more files. Consolidate duplicate runtime views only where persistence contracts remain unchanged; otherwise defer that work. Acceptance: pause/resume/settings actions occur once, repeated drawing cannot mutate gameplay, and transitions/room rewards/death/victory retain behavior. Existing save calls retain their original semantics and frequency.

4. Add CI or an equivalent documented reproducible native build/test gate. Tighten package profile exclusion and provenance without changing how the game saves. Update README to the supported native workflow, label web support as discontinued, and clearly mark the old Makefile as obsolete. Do not delete release archives or rewrite history. Acceptance: native packaging excludes personal progress, and documentation identifies supported build paths.

5. Measure room preparation and worst-case combat before making performance changes. Reuse journey timing and texture-upload diagnostics, preserve main-thread GPU ownership, and optimize only demonstrated bottlenecks. Report measurements and defer speculative optimization.

Update this review with actual verification outcomes and keep saving findings in the deferred backlog.

Reuse existing tests and flow tooling. Add regression tests for behavioral fixes, not tests that merely restate trivial implementation details. Use temporary profiles, never the developer's real progress. Smoke-test menu -> village -> dungeon -> room clear -> death/summary and final victory, plus any still-supported native demo route. Run existing save/resume checks where relevant, but leave saving improvements deferred. Do not commit or publish unless requested. Finish with changed files, removed symbols and evidence, tests/builds actually run, and remaining limitations. If scope must be split, prioritize native verification, proven dead-code cleanup, and render/update separation; leave a concrete continuation list rather than silently marking all stages complete.
