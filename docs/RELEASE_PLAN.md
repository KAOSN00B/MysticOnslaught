# Mystic Onslaught — release-candidate plan and status

Started 2026-10-16. Source of truth is the code; every item below was verified
against the current working tree (file:line references are to that state).
Status keys: **DONE** (implemented + automated check), **DONE-UNTESTED**
(implemented, compiles, needs interactive check), **OPEN**, **DEFERRED**
(documented, intentionally not in this candidate).

Baseline backup of the pre-change working tree (tracked diff, untracked files,
status and HEAD) is outside the repo in `C:\Users\rober\MysticOnslaught-baselines\`.
Nothing has been committed.

## Release target

| Item | Decision |
|---|---|
| Product | Windows x64 full game, `Shipping` configuration (`MO_DEV_TOOLS=0`, `MO_ITCH_DEMO=0`). |
| Web demo | Kept as the separate `PublicDemo`/Emscripten product (Forest -> Caverns). Not expanded into a full web release. |
| Run structure | Forest (domain 0) -> four domains picked on the world map from eight -> Demon's Insides (domain 5, final boss) -> victory. ~6 bosses per run; 10 boss types exist. |
| Classes | Warrior, Hunter, Mage from the start; Rogue and Warlock by milestone; Paladin after a completed run. |
| Content policy | Forest + Jungle use authored `.mroom` libraries (validated). All other domains use the procedural room generator. Caverns/Graveyard authored rooms remain editor content until validated as full libraries (see F). |
| Controls | Keyboard + mouse and XInput controller are release paths. Touch is a web-demo path. |
| Saves | Per-user profile (`%LOCALAPPDATA%\MysticOnslaught`) for progression, settings, bindings, village layout and the village wallet. A run in progress is kept only for the current session (village pause); quitting ends it. |
| Performance | Measured only on the development PC available here; other hardware is an open acceptance item. |

## Confirmed defects and work items

### Re-check 2026-10-19 (supersedes the Status column below)

The tables below were written on 2026-10-16 and many rows were fixed in later sessions without the Status column being updated. Each item was re-checked against the code on 2026-10-19; details are in `ENGINEERING_REVIEW_AND_AGENT_PROMPT.md`, "Bugs and performance pass 2026-10-19".

| Item | Status 2026-10-19 |
|---|---|
| P0-1 | Done (power level per domain). |
| P0-2 | Done (`RunProgression::ShopTierForZone`). |
| P0-3 | Done (`EvaluateClassMilestones`). |
| P0-4 | Done (Echoes banked on return). |
| P0-5 | Done (stipend paid once at run start; flow check). |
| P0-6 | Done (`GetGoldKeptOnDeath`). |
| P0-7 | Done (save keys repurposed; 4 slots kept). |
| P0-8 | Done (Second Wind on the dungeon death path; flow check). |
| P0-9 | Done (next run prepared on village arrival). |
| P0-10 | Done (village layout stored in the profile). |
| P0-11 | Done (debug hint only in developer builds). |
| P1-1 | Done (`DamagePolicy.h`: one rounding per hit). |
| P1-2 | Done (Keen Edge / Bloodlust / Swift Boots / Momentum redesigned with distinct roles). |
| P1-3 | Done (×10 numbers: the attack card is +5). |
| P1-4 | **Fixed 2026-10-19** (authored swing box follows reach upgrades). |
| P1-5 | Done (`GetUltimateManaRequired`). |
| P1-6 | Done (War Cry / Shield of Faith now give temporary Defense, no heal). |
| P1-7 | Retuned (`GameBalance.h`); balance is Robert's call. |
| P1-8 | **Fixed 2026-10-19** (lifesteal overlap). |
| P1-9 | Done (Zeph restocks once per village arrival). |
| P1-10 | Done (one pending contract). |
| P1-11 | Done (capped wares refused and refunded; full-slot learn cards offer a swap). |
| P1-12 | Done (room-clear XP boosted once at clear). |
| P1-13 | **Fixed 2026-10-19** (only first room visits count). |
| P1-14 | **Fixed 2026-10-19** (affix kept per room). |
| P1-15 | Done (mid-run save, Continue). |
| P1-16 | Can't re-check: the FEATURE_INVENTORY list no longer exists. Part of the frontend/text pass. |
| P1-17 | Done (Comfort settings). |
| P1-18 | **Fixed 2026-10-19** (LB/RB and a key hint; the victory input lock already existed). |
| P1-19 | Done (signed run log). |
| P1-20 | Done (demo-specific victory text). |
| P2-1, P2-2 | Done (`tools/`). |
| P2-3 | Open (optional hosted CI). |
| P2-4 | **Fixed 2026-10-19** (legacy prop loads removed; the smoke test no longer whitelists them). |
| P2-5 | Done (`CREDITS.md`, `AUDIO_PROVENANCE.md`). |
| P2-6 | Done (`source.sha256` + `version.json`). |
| P2-7 | Done (typo gone). |
| P2-8 | Done (every biome has a validated authored library; `ForestVariants` builds all 269 rooms). |

### P0 — broken progression, data loss, invalid packaging

| # | Finding (verified) | Fix | Status |
|---|---|---|---|
| P0-1 | Enemy stats never scale: `_wave` stays 0 on the real route, so every domain is power level 1 (Engine.cpp `ConfigureSpawnedEnemy`, `GetEnemyPowerLevelForWave`). | `RunProgression.h`: one power level per world-map domain; behaviour tiers fed the matching wave. | DONE (see tests) |
| P0-2 | Shop tier never advances: `_currentAct` stays 1 on the real route. | Shop tier from domain depth. | OPEN |
| P0-3 | Rogue and Warlock can never be unlocked (setters never called; milestones never designed). | Milestones from existing profile data: Rogue = defeat any domain boss; Warlock = clear 3 different domains. Retroactive on load. | OPEN |
| P0-4 | Carried Echoes are never banked in the full game (only a dungeon Store room banks, and the full game generates none). Kill Echoes, Soul Siphon, Echo Surge are worthless. | Bank on safe return to the village after a boss and on victory. | OPEN |
| P0-5 | Zeph's Stipend I-III (140 Echoes) never applies: `StartMainRun` overwrites the granted gold. | Explicit wallet flow. | OPEN |
| P0-6 | Deep Pockets I-III (245 Echoes) never applies: death always zeroes gold. | Apply retention share on death. | OPEN |
| P0-7 | Arcane Attunement / Ability Mastery (300 Echoes) never add a usable slot; `GetAbilityKey` would read past the 4-entry binding array. | Repurpose to working, option-expanding effects (same save keys); keep 4 slots. | OPEN |
| P0-8 | Second Wind (160 Echoes) only works in the legacy wave mode; the dungeon death path ignores it. | Port to the dungeon death path. | OPEN |
| P0-9 | Zeph purchases made in the village before a run are wiped by `StartMainRun -> ResetRunState -> Init`. | Prepare the next run when arriving in the village; the gate no longer re-inits the player. | OPEN |
| P0-10 | Village layout saved into the install folder (not per-user) in Shipping; write failure ignored; refunds of buildings let gold dodge death and can duplicate gold if the save fails. | Per-user layout path; refunds only for buildings placed this visit... see P1-economy. | OPEN |
| P0-11 | Victory screen in Shipping tells players to use a Debug button and F1/F10 that do not exist. | Gate the hint to dev builds. | OPEN |

### P1 — major balance, misleading UI, incomplete core flow

| # | Finding | Fix | Status |
|---|---|---|---|
| P1-1 | Damage rounding: `ceil` makes +15% on a 2-damage hit +50%; several rounding stages compound. | One documented policy: compute the full multiplier in float, round once; fractional remainder carried per player so the average equals the nominal bonus. | OPEN |
| P1-2 | Dominated relics (Keen Edge < Bloodlust, Swift Boots < Momentum, both Common). | Give each a distinct role/condition. | OPEN |
| P1-3 | `+0.5 attack` cards do nothing (attack truncated to int). | Float attack through the damage policy. | OPEN |
| P1-4 | Reach upgrades do nothing (authored melee box ignores range multiplier). | Scale authored box reach, or stop offering. | OPEN |
| P1-5 | Ultimates fire at full power with 1 MP while draining "all" mana. | Require the documented 70% threshold. | OPEN |
| P1-6 | War Cry / Aegis farm +1 armour and heals in cleared rooms outside sustain caps. | Armour/heal from these only while enemies are active. | OPEN |
| P1-7 | Flame Wall ~16 dmg/MP (doubled burn); ground zones 3-4x peers. | Targeted retune with documented numbers. | OPEN |
| P1-8 | Lifesteal overwrite keeps the longer timer with the larger fraction. | Keep the larger fraction only for its own duration. | OPEN |
| P1-9 | Free village restock (any building edit re-rolls Zeph's stock and resets reroll cost). | Stock persists per visit/domain. | OPEN |
| P1-10 | Shop contracts stack without limit. | One pending shop contract. | OPEN |
| P1-11 | Wares bought at their cap consume gold for nothing; treasure "Learn" cards lost with full slots. | Don't offer / refuse purchase. | OPEN |
| P1-12 | Risk Shrine XP bonus never applies (multiplier reset before the XP drains). | Apply at clear. | OPEN |
| P1-13 | Elite reward rarity rises with backtracking (`_currentRoom` increments every entry). | Use domain depth. | OPEN |
| P1-14 | Uncleared room re-rolls its affix on re-entry. | Persist per-room affix. | OPEN |
| P1-15 | Continue does not resume anything; New Game resets nothing. | Honest menu semantics. | OPEN |
| P1-16 | Ability/relic/meta descriptions contradict code (list in FEATURE_INVENTORY). | Fix text or behaviour. | OPEN |
| P1-17 | No persistent shake / flash / hit-stop comfort settings; window size unvalidated. | Add to Settings, clamp. | OPEN |
| P1-18 | Ascension selector has no controller input or on-screen hint; victory screen has no input delay. | Add both. | OPEN |
| P1-19 | Run telemetry keeps 8 rooms in memory only. | Bounded local run log (JSONL). | OPEN |
| P1-20 | Demo victory text claims six domains after two. | Product-specific text. | OPEN |

### P2 — polish, maintainability, pipeline

| # | Item | Status |
|---|---|---|
| P2-1 | Build/test scripts hard-code one machine's drive layout. | `tools/MoToolchain.ps1` + `tools/run_tests.ps1` DONE; build/package scripts OPEN |
| P2-2 | 22 of 29 C++ test suites had no runner. | DONE (`tools/run_tests.ps1`, 31 suites) |
| P2-3 | No CI workflow. | OPEN (local script only; CI needs a raylib source decision) |
| P2-4 | Seven legacy prop images missing; smoke test whitelists them. | OPEN |
| P2-5 | Asset provenance/credits inventory missing. | OPEN |
| P2-6 | Build/version stamp and manifest. | OPEN |
| P2-7 | Typo "a a magic gem". | OPEN |
| P2-8 | Caverns/Graveyard authored room validation. | OPEN |

## Session status — 2026-10-16 (stopped at usage limit)

Verified: `tools\build.ps1` (Shipping) compiles; `tools\run_tests.ps1` = 30/30 implemented suites PASS
(`RunTelemetry` is listed but not written yet, so the runner reports it as MISSING).
Nothing below has been play-tested interactively.

DONE (implemented + automated tests): P0-1 domain-based enemy scaling (`RunProgression.h`, RunProgression suite);
P0-2 shop tier from domain (`ShopPricing.h`, unified upgrade price, kit-first stock rule);
P0-3 Rogue/Warlock milestones (retroactive on load); P0-6 Deep Pockets; repurposed unlock keys 16/17
(free reroll / bonus starter ability); village wallet persisted (MetaProgression suite);
P1-1 single-rounding damage policy with carry (`DamagePolicy.h`), P1-3 float attack power, ranged scale 2/3;
P1-2 relic redesign (Keen Edge opener, Bloodlust kill-chain, Swift Boots dash, Momentum while moving) + class-fit offers (RelicBalance suite).

DONE-UNTESTED (compiles; needs interactive check): P0-4 Echo banking on safe return/victory; P0-5 Stipend at run start;
P0-7 no phantom slots; P0-8 Second Wind in dungeon; **relic picks after elite/boss kills now open (were only in the dead
`UpdateGamePlay` loop)**; heal VFX in dungeon; P0-9 next run prepared on village arrival; death -> Poe -> class select -> village;
P0-10 per-user village layout, half-value salvage for older buildings, load-once layout; P1-9 per-visit Zeph stock;
P0-11 victory text per product + dev-only debug hint + input lock + unlock line; P1-6 War Cry/Aegis persistent grants combat-only;
P1-11 ware caps refuse purchase, ultimate-aware slot check; survivor HP restored on room re-entry; magic-gem typo.

FALSE POSITIVE: P1-5 (ultimates already require 70% max mana via `CanCastAbility`).

STILL OPEN: P1-4, P1-7, P1-8, P1-10, P1-12..P1-20 (ability description fixes for Whirlwind/Deadeye/Vengeful Ward/Roll
were not applied), telemetry, comfort settings, Continue semantics, package script (`VillageLayoutSave.vlayout` exclusion,
use `tools\build.ps1` output), credits inventory, feature inventory / balance rationale / test-results docs,
packaged smoke test of this build, all interactive acceptance.

## Session 2 — 2026-10-16 (review fixes, Hades-style flow, start balance)

Direction (decided with Robert): Hades-style run (village = home after death/victory only; Zeph's camp opens
every domain after the Forest); village = core town + unlocks; story overhaul last; Debug/Release build the full game.

External review findings — all fixed and covered by the end-to-end flow self-test (`tools\run_flow_selftest.ps1`):
1. Village buildings + wallet now saved in ONE atomic profile write (layout lives in metaprogress.cfg; legacy file migrated once).
2. All Poe's altar effects apply at the gate (GrantRunStartBonuses), so village purchases count for the next run.
3. Shop cards revalidate effect and live price at purchase (`ShopManager::TryPurchase`, `RefreshOffers`); refunds on no effect.
4. Timed contracts use active combat seconds; relic picks wait for room-clear resolution; Risk Shrine XP applied at clear.
5. Reserve+reroll never displaces the contract card; one shop contract per fight judged on run state.
6. Fractional damage carry is staged and only committed when the target actually loses HP.
7. Momentum uses a 0.5 s recent-movement window ("on the move").
8. Packaging builds/packages x64\Build-Shipping only, refuses stale exes, discovers the MSVC runtime, version.json.

Flow / oversights:
- Boss exit -> CompleteDomain (40% breather heal + 50/100/.../250 bounty) -> world map -> Zeph's camp (bank, shop,
  Poe's altar, wager shrine; first-visit-only stock/heal/bank). Victory -> home. Death -> Poe -> class select -> home.
- Retired states (Play, act Map, ExpTally, legacy GameOver, BossChoice) recover into the run/village instead of freezing.
- Shop has its own return state. Main menu: Continue first when a profile exists; New Game asks before erasing.
- **Softlock found by the flow test and fixed:** the Forest "Basic Room" boss layout put the exit against the south
  wall where the player's centre point could never reach it; exits now trigger on body overlap.

Start balance: Zeph's 60g welcome purse; Poe salvages 50% of carried Echoes on death; Paladin also unlocks after
5 different domains; Vitality is +20%/+35% of class base HP.

Verification: Shipping + Release build; 30/30 C++ suites; flow self-test 8/8 consecutive passes (58 + 9 checks).
Still not verified by a human: feel, visuals of camps in authored rooms, menus with controller.

### Village core town (session 2, continued)
- Fixed town plots from `VillageAssets/VillageLayout.vlayout` (48px grid, 2x art scale like Zeph's shop):
  four homes from the first visit; **Trophy Hall** opens after the first domain boss (mastery journal page);
  **Armory** opens at the first class mastery rank (choose each class's starting ability; option N opens at
  mastery rank N; Mage bolts also need Poe's unlock). Plots stay reserved before they open; town buildings are never
  stored as player buildings and cannot be bought, moved or removed.
- Armory choice saved per class (`starter_<cls>` in metaprogress.cfg); applied to the prepared character in place
  (unless already upgraded at Zeph's, then from the next run). One-time "has opened" notices (`village_seen`).
- Residents use real hero sprites (idle/walk), greet the player with neutral lines (story pass rewrites them),
  count = 3 + classes brought home + player buildings (cap 10), and no longer walk through .vasset buildings.
- Village draw is depth-sorted (buildings, residents, Zeph, player) so nobody stands on a roof.
- HUD: top-centre toasts stack instead of overlapping; relic strip sits below the Echoes counter; the dungeon
  tutorial prompt no longer covers the class resource bar.
- Verification: Release/Shipping/PublicDemo build; 30/30 suites; flow self-test passes (73-74 + 11 checks, incl.
  town gating, Armory swap + persistence, Trophy Hall/Armory screenshots).
- Noticed, not yet fixed: the camp/entrance spawn at the bottom door sits under the ability bar; two thin vertical
  lines in the procedural Caverns camp room (to check during the handcrafted-room pass).

### Handcrafted rooms in every biome (session 2, continued)
- All 10 biomes now use authored rooms. Before this, only Forest and Jungle did and
  the other 8 were procedural.
- Each biome got the 23 Forest layout families re-drawn in its own art, 184 rooms
  in total. Caverns and Graveyard keep their originals too.
- Gameplay geometry is identical to the Forest sources. See
  `docs/biome-content-audit/forest-variants/README.md`.
- New `ForestVariants` unit suite. Flow self-test phase 3 tours every domain via
  the real world-map path and saves screenshots.
- Fixed the "two vertical lines" seen in rooms: a 1px floor seam at exact-pixel
  column edges (`SnapTileDest` float error), present in every biome.
- Verification:
  - 31/31 suites pass.
  - The flow self-test passes all three phases (73 + 11 + 50 checks).
  - Release, Shipping and PublicDemo build.
- Needs human review: the art choices per biome (contact sheets and in-engine
  shots are in that folder).

### Audio (session 2, continued)
- **Music**
  - Every biome has its own theme. Before this, all dungeon runs played one track,
    and the village and world map were silent.
  - Forest = Mystwood Reverie; Caverns keeps the original dungeon theme. New
    village, world-map and final-boss themes.
  - Built as seamless OGG loops from Ovani Fantasy Vol. 7 (`tools/audio/make_music_loops.py`;
    reverb tail folded over the loop start). About 2 MB each, versus 27 MB WAVs.
- **SFX wired from the imported library**
  - Surface footsteps by biome (grass, stone, dirt).
  - Coins, heals, chest, doors opening after a clear, dash, armour absorb (light
    and heavy), relic reveal and gain, boss-arrival stinger, Second Wind.
  - Shop purchase feedback by item kind, plus a refusal cue.
  - Werewolf voice, and signature sounds for Smite, Smoke Bomb, Aegis, Vengeful
    Ward, Curse, Piercing Shot and Backstab.
- **Enemy voices**
  - Every enemy hurt and attack sound now uses footstep-like ±10% pitch variation
    around that creature's own base pitch (`SoundVariation.h`). That's 40+ call
    sites; about 25 had no variation at all.
- **Licensing**
  - Game SFX Treasury v1.1 has no license file, so its clips were replaced with
    Ovani equivalents and none ship.
  - The package now ships only referenced library clips.
  - See `docs/CREDITS.md` and `docs/AUDIO_PROVENANCE.md`.
- **Verification**
  - The flow test checks that all 30 library clips load and all 9 new themes
    stream (82–96 s).
  - AudioMusicRouting tests cover unique biome themes and the final boss.
  - 31/31 suites pass.
- **Not verified**
  - No listening was possible, so track choice, loudness, loop seams and SFX
    fit all need Robert's ears.

### Combat feel (session 2, continued)
- **Knockback:** per-hit recoil reduced about 38% (210, crit 320; was 340 and 520).
  Weight now comes from freezes and shake.
- **Shake:** about 15% more everywhere (`_juiceShakeMult` 1.15). Kill shake 4.5,
  elite 6, boss 10.
- **Freezes:**
  - Hits: 0.016–0.042 s.
  - Kills: 0.065 s (elites 1.4×).
- **Enemy deaths:**
  - A 2–3 frame white-hot flash.
  - The corpse is thrown along the killing blow, arcing and spinning while it
    deflates. This is visual only; collision is untouched.
  - A shockwave ring.
  - Creature-coloured gore chunks that arc, bounce and settle.
  - A ground splat that lingers 4.5–6 s.
  - A "crunch" stomp layered under the death cry, plus an Execution flourish on
    elite and crit kills.
  - A short slow-mo on the last kill of an ordinary combat room.
- **Fix:** south-door entrances now spawn above the HUD, not under the ability bar.
- **Verification:**
  - The flow self-test kills Forest boss-room adds through the real `RegisterHitFx`
    path and saves `death_payoff_1_mid.png` and `death_payoff_2_after.png`.
  - 31/31 suites pass.
  - Release, Shipping and PublicDemo build.
- **Needs hands-on playtest:** how it feels at speed, the white-flash length, gore
  amount, and whether the slow-mo ever annoys.

## Session 3 — 2026-10-16 (full roguelite feature list)
Robert approved a 13-item list of what a full roguelite still needs. Agreed order:
mid-run save, comfort settings and credits, room events, more relics, final boss
and ending (with the story), run summary/codex/achievements/history,
biome-exclusive rooms, then daily/seeded runs, Steam, licence checks and playtests.

### Mid-run save (done)
- **What it does:** a run survives quitting. Pause offers "Save & Main Menu" and
  "Save & Quit Game" during a run, and the main menu's Continue drops straight
  back into it, with a caption naming the run ("Run in progress: Hunter, Forest
  domain 1").
- **Checkpoints:** written on entering a room, when a room's doors open, and on
  opening the world map. The file is `run_save.cfg` in the user data folder:
  key=value lines with a checksum, written to a temp file and swapped in, so a
  crash mid-write keeps the previous checkpoint.
- **What is saved:** the character (81 scalar fields, abilities and levels, relics
  in order, class, appearance), the run session and world-map route, the dungeon
  graph, each room's authored layout and cleared state, and the domain and biome.
- **Ending a run:** death, victory, abandoning, or New Game marks the save
  inactive. The file is never deleted.
- **Known limits:**
  - Shop stock is not saved; the camp restocks once after a resume (a minor
    reroll exploit).
  - Resuming on the world map regenerates the map's layout.
  - Quitting mid-fight restarts that room's enemies; the player keeps current HP.
- **Verification:**
  - RunSave suite: round trip, exact floats, escaping, checksum, version and
    truncation rejection, atomic replace.
  - Flow phases 4–5: build a run, Save & Main Menu, relaunch, Continue. Checks
    the same room, layout, biome, domain and cleared rooms, and that the
    character is identical (81 fields, 0 differ). Dying then ends the run.

### Comfort settings (done)
- **New Settings tab, COMFORT:**
  - Screen Shake slider (0–100%) scales every camera shake.
  - Screen Flashes (Full / Reduced / Off) covers the full-screen flashes:
    level-ups, relics, ultimates (including the white-out), boss attacks and
    Second Wind. Reduced keeps 30%.
  - Hit-Stop (Full / Reduced / Off) scales the impact freezes; Reduced halves them.
- Saved in `settings.cfg` (`screen_shake`, `screen_flashes`, `hit_stop`); values
  from a hand-edited file are clamped. Mouse and gamepad both work.
- **Not covered:** the small white flash on a dying enemy's sprite and the death
  shockwave disc are local effects and stay on.
- **Verification:**
  - SettingsDefaults suite: defaults, scale levels, save/load round trip, clamping.
  - Flow phase 1: Hit-Stop Off/Reduced and Shake 0%/50% change the real
    `RequestHitStop` and `TriggerScreenShake` results. Screenshot:
    `00_settings_comfort.png`.

### Credits screen (done)
- Main menu has a Credits button. It opens a scrolling overlay (wheel, arrows,
  stick or auto-roll; Esc, B or Back closes it).
- Content lives in `TestGame/Credits.h` and lists only verified attributions:
  Ovani Sound (music and SFX), ElvGames (SFX and village art, credit required),
  and raylib.
- The creator line reads "A game by Robert Blandon". **Robert to confirm or
  change** (for example to a studio name).
- With five buttons the menu column tightens and the frame grows, leaving room
  for the run caption.
- **Verification:**
  - Credits suite fails if the ElvGames or Ovani entries go missing, or if a line
    uses characters the default font can't draw.
  - Flow screenshots: `00_credits.png`, `00_credits_end.png`, `resume_menu.png`.
  - 33/33 suites pass. Release, Shipping and PublicDemo build.
- **Still owed:** credits for the character, enemy, boss, tileset and UI art, and
  the older audio, once their licences are confirmed (see `docs/CREDITS.md`).

### Room events (done)
- **Built on the Risk Shrine pipeline.** Each event is a Standard room tagged
  when the domain is generated, with a seeded offer, so re-entering or resuming
  shows the same cards. Walk to the shrine, press E, take one card or walk away
  and come back later. A spent room is checkpointed straight away.
- **The events** (code in `TestGame/EngineRoomEvents.cpp`):
  - **Cursed Cache:** one of Blood Gold (gold now), Forbidden Relic (a relic
    pick now), Hollow Vigor (full HP and mana) or Stolen Echoes (Echoes now).
    Each brings a curse on the next 2–3 combat rooms: tougher or deadlier
    enemies, or no gold drops. Curses show as HUD pills with rooms left, and
    lift with a "CURSE LIFTED" toast.
  - **Omen Stone:** three of five two-sided omens for the next combat room. Some
    are easier but poorer (e.g. Frailty: enemies have 30% less HP, half XP).
    Gold and XP multipliers below 1× now work.
  - **Locked Armory:** pay for one of three cases. Iron (45): 40% heal and +1
    armour. Silver (90): upgrade your weakest ability. Gilded (150): a relic
    pick. Prices rise 25% per domain.
  - **Mutation Altar** (domain 2+): three of four permanent trades.
    - Glass Marrow: −20% max HP, +20% damage.
    - Quickened Blood: −10% max HP, +12% speed, faster dash.
    - Arcane Graft: −1 max armour, +6 mana, +20% regen.
    - Iron Hide: −10% speed, +1 max armour, +15% max HP.
  - **Relic Forge** (domain 2+):
    - Salvage melts your newest relic for gold and a 30% heal.
    - Reforge swaps it for a relic pick.
    - Temper costs 15% max HP for +5% damage per relic carried. (This card was
      first called Overcharge, renamed to avoid clashing with the Overcharge relic.)
    - Removing a relic reverses its passive exactly (new `Character::RemoveRelic`).
  - **Rest Site** (one per domain from domain 2, in the deepest free room,
    drawn as a campfire): Rest (heal 40%), Train (upgrade your weakest ability)
    or Fortify (+1 max armour, refilled).
- **Generation rules:**
  - A domain gets a Rest Site (domain 2+), plus one or two distinct events.
  - Events never go on the entrance, boss, key, elite or treasure room.
  - Holdout and Swarm rooms are used only as a last resort.
  - The modifier cap (2) still applies to curses, omens and contracts.
- **Fixes on the way:**
  - Decision rooms could roll a combat affix (e.g. a "Gilded" pill in an empty
    shrine room), because the affix rolled before the room was marked peaceful.
    They no longer do.
  - Card screens skip an empty downside block.
  - The weakest-ability search is now shared by the Untouched contract, Train
    and the Silver Case.
- **Debug:** F6 in debug mode now cycles the current room through every event.
- **Verification:**
  - Flow phase 6 (30 checks):
    - 180 generation rolls across domains 1, 2 and 4 against the rules above.
    - Every event's card screen screenshotted, and each choice's effects checked
      in the real game.
    - An unaffordable case leaves the armory open.
    - Exact relic-removal reversal.
    - A curse is written to the checkpoint.
    - 25 fresh entries into a rolled event room: no affix, peaceful, named banner.
  - The resume test now compares 83 character fields (adds the two new
    run-scoped damage fields).
  - 33/33 suites pass. Release, Shipping and PublicDemo build.
- **Balance is first-pass and needs playtesting.** Event values were set by
  judgement, not play. From domain 2 a Rest Site replaces one combat room, so a
  domain gives slightly less XP; this should be revisited with the levelling
  redesign.
- **Web build:** `build_web.ps1` has a hand-kept source list that was already
  missing many files. It also lacks `EngineRunSave.cpp` and
  `EngineRoomEvents.cpp`. It needs updating before any web build.

### Relic expansion, first pass (done): 24 → 60 relics
- **Kept from the old design:** relics stay additive with each other, and
  elemental or class relics are only offered to classes that can use them. The
  7 archetype icons are reused.
- **Class mechanics (18):** each relic pushes one class mechanic further:
  - Warrior Rage: drains slower; bigger full-Rage bonus.
  - Mage: +20% fire, ice or lightning spell damage; reactions +50%.
  - Hunter: marked prey takes more damage.
  - Rogue: +1 combo point; finishers scale harder.
  - Paladin: Aegis reflects more; Retribution stacks hit harder.
  - Warlock: curses hit harder and last longer.
  - Shared:
    - Poison +50% (Rogue, Hunter, Warlock).
    - Life drain +50% (Warrior, Rogue, Warlock).
    - Class resource builds faster (Warrior, Paladin, Rogue).
    - +15% ability damage (all classes but the Mage).
    - Heal 2 on room clear (all classes).
- **On kill (5):**
  - Mana Siphon: kills restore 1 mana.
  - Pyre Heart: burning kills heal.
  - Glacial Battery: frozen kills restore mana.
  - Chain Reaction: a 25% chance a kill blasts nearby foes, and can chain.
  - Soul Harvest: elite and boss kills grant Echoes.
- **Situational damage (7):**
  - Giant Slayer: vs elites and bosses.
  - Ambush: first 4 s of a fight.
  - Duelist: last enemy.
  - Gilded Gauntlet: per 100 gold carried.
  - Bully: below half health.
  - Afterimage: 1 s after a dash.
  - Retaliation: 3 s after being hit.
- **Defense, utility and economy (6):**
  - Spiked Pauldrons: hurt nearby enemies when hit.
  - Last Stand: once per room a lethal hit leaves 1 HP; it goes before Second Wind.
  - Ironclad: +1 armour on room clear.
  - Hourglass: cooldowns 25% faster.
  - Coin Purse: +15 gold on room clear.
  - Stoneheart: +40% max HP, −10% speed.
- **Hooks:** one choke point each.
  - The hit context gained target and fight facts: elite, fight timing, last
    enemy, gold, dash, recent hurt.
  - The engine's single HP-drop detector drives Retaliation and Spiked Pauldrons.
  - Both death paths try Last Stand.
  - The room-clear block pays Ironclad and Coin Purse.
  - `Character::RemoveRelic` reverses every new passive. Two rounding edges:
    - Shadow Reserve at the 7-point cap.
    - Stoneheart on a fractional max HP.
- **HUD:** the relic strip wraps into a second column after 12.
- **RelicBalance suite (2229 checks):**
  - No single relic beats +40% in its best moment.
  - The strongest 8 with every condition true: +275%. All 60 at once: +445%.
  - Every common still wins some situation.
  - Descriptions match the numbers and fit two 26-character lines.
  - Names are unique.
  - Every class has at least two relics of its own.
- **Flow phase 6 adds:** a relic pick showing new relics, Last Stand vs Second
  Wind, Retaliation after a real hit, and Ironclad and Coin Purse on a real
  room clear. 37 checks.
- **To revisit with the levelling redesign:**
  - Many class relics raise the same dials the level-up class cards do (Rage
    bonus, element damage, marked/cursed bonus, poison, reflect), so the two
    stack.
  - Values are first-pass. The plan's 60–150+ target is met at its low end;
    more relics can follow once the levelling budget is known.

### Final boss and growing domains (done, Robert's call 2026-10-16)
- **The final boss is the Molarbeast.** Robert: "Abyss slime should not be the
  final boss. Maybe molar beast is better suited."
  - In Demon's Insides it spawns in a **final form**: 18% bigger, a molten glow
    under it, a hotter tint, +2 shots in every lava fan, and specials about 15%
    more often.
  - Its health moved to the final-boss budget (about 71 base; about 115 at final
    depth). The old first-boss caps on it (ultimates max 3 damage, Mage bolts a
    flat 2) are gone.
  - The banner reads "FINAL BOSS" in the final domain.
  - The Caverns, which had the Molarbeast, now get the **Abyss Slime** on the
    standard mid-boss budget (about 46).
- **Dungeons grow as the run goes deeper:**

  | Domain | 1 | 2 | 3 | 4 | 5 | Final |
  |---|---|---|---|---|---|---|
  | Rooms | 10 | 11 | 12 | 14 | 15 | 16 |
  | Elites | 1 | 1 | 2 | 2 | 2 | 2 |
  | Treasure rooms | 1 | 1 | 1 | 2 | 2 | 2 |
  | Events (+ Rest Site from domain 2) | 1–2 | 2 | 2 | 2–3 | 2–3 | 2–3 |

  - Was 12 rooms with one elite and one treasure in every domain.
  - The grid stays 8×8, and the corner minimap always draws that whole grid, so
    a dungeon can never outgrow it.
  - 16 rooms (a quarter of the grid) is the hard cap.
  - Dev tools and editor playtests keep the old default of 12.
- **Verification:**
  - DungeonGenEntrance: every domain plan over 120 rolls. Exact room, elite and
    treasure counts; everything inside the grid; every room reachable; sizes
    never shrink.
  - ForestVariants: every biome's room library forms every domain size, 260
    dungeons in all.
  - Flow phase 3, through the real world-map path: each domain is sized for its
    depth; Demon's Insides spawns the final-form Molarbeast; the Caverns spawn
    the Abyss Slime.
  - Phase 6 checks the new event counts.
  - 33/33 suites and flow phases 1–6 pass. Release, Shipping and PublicDemo build.
- **Effects to watch in playtests:**
  - The first domain is a little shorter (10 rooms, was 12). The last three are
    longer, so a full run has about 78 rooms instead of 72, though only the key
    and boss rooms are required.
  - Bigger later domains mean more XP and more relics (second elite), which
    feeds the levelling redesign.
  - The Abyss Slime was designed as a final boss and may be a hard mid boss.

### Crossroads instead of the world map (done, Robert's design)
- **How it works now:** after a domain's boss you walk into a quiet room drawn
  in the domain you just cleared. Its west, north and east doors each lead to a
  different unplayed domain. Walking through a door is the choice; there is no
  map screen.
  - **Signs:** each door has a sign with the domain, its guardian (boss), and
    an arrival reward. The rewards are a relic pick, gold, Echoes or a full
    heal; all three differ and gold/Echoes scale with depth.
  - **Look and sound:** the doorway glows in the domain's colour, and the
    journey theme plays.
  - **Banner:** "DOMAIN CLEARED" with the bounty, plus how to choose. This
    replaces a toast that covered it.
  - **Last domain:** after the fifth domain only the north door opens: "The
    final descent" into Demon's Insides, arriving fully healed.
- **Saving:** the checkpoint holds the three roads, so Continue returns to the
  same crossroads. This also fixes the old "world map regenerates on resume" gap.
- **Bug fix:** "doors open" bursts from the previous room no longer carry over
  into the next one.
- The world map code stays (debug editor only); nothing was deleted.
- Code: `TestGame/EngineCrossroads.cpp`, `DungeonGen::GenerateCrossroads`.

### Run summary and run history (done)
- **Run summary:** every main run now ends on a summary screen before Poe
  (death) or the victory screen. It shows:
  - how the run ended ("Fell to the Minotaur in Ancient Castle");
  - class, level, domains reached, rooms cleared, enemies, elites, bosses, time;
  - gold and Echoes, and what happens to them;
  - the road of domains taken, and every relic, coloured by rarity.
- **Run history:** each summary is also saved to `run_history.cfg` (the newest
  30 runs, plus total runs and wins).
- **Stat fixes:** boss kills were already counted; rooms and elites now are too,
  and dungeon time is tracked (the old timer only ran in the retired mode).
  All are saved with the run.

### Grimoire (done; Robert asked for a better name than "codex")
- Opened from a new pause-menu button, in the village or mid-run. Four tabs:
  - **Relics:** each relic shows its name, rarity, archetype, effect and classes
    once you have carried it; until then it shows "???".
  - **Bestiary:** each of the 26 creatures shows its role, domain and an honest
    one-line note on how it fights, once you have slain one.
  - **Chronicle:** the run history.
  - **Feats:** 16 in-game achievements: First Blood, Pathfinder, The Final
    Descent, Conqueror, Ascendant, Swift Victory, Untouched, Elite Hunter,
    Exterminator, Collector, Hoarder, Curse Breaker, Reshaped, Echo Keeper,
    Beast Scholar, Relic Sage. Earning one shows a "FEAT EARNED" toast.
- Finds and feats are saved in the profile and only count in real runs (not
  debug, the prologue or editor tests). New Game clears them with the rest of
  the profile.
- Steam achievements are sidelined per Robert. The feats are written so each
  could map to one later.

### Mid-run save: Zeph's stock (done)
- The checkpoint now holds the exact shop (cards, prices, bought and reserved
  flags, reroll price), so Continue finds the same wares. This closes the
  "camp restocks after a resume" reroll exploit.
- Older saves without stock still get the one-time restock.

### Verification for this round
- 34/34 suites pass. New ones: Grimoire, plus Grimoire flags in MetaProgression
  and the crossroads music route in AudioMusicRouting.
- Flow phases 1–6 pass: 92 + 11 + 73 + 8 + 13 + 56 checks. In-game checks cover:
  - the crossroads: doors, roads, rewards, save and resume, the final door;
  - the death and victory summaries and history order;
  - feats and discoveries saved to the profile, and every Grimoire tab;
  - the seven-button pause menu;
  - Zeph's stock after Continue.
- Release, Shipping and PublicDemo build.
- Screenshots: `02_crossroads`, `crossroads_final_descent`, `run_summary_death`,
  `grimoire_*`, `pause_menu`.
- **One intermittent failure, once:** MetaProgression's backup-recovery check
  failed in one run and passed on every re-run. The test sandboxes sit inside
  the OneDrive-synced Desktop, and a brief OneDrive file lock during the save's
  rename would explain it. Worth moving the project out of OneDrive, or
  excluding `x64/`.

### Dungeon room variety (done)
**What was there:** 262 authored rooms. Forest's 23 rooms had already been
reskinned into the other nine biomes (207 variants in
`Rooms/Generated/<Biome>/forest_variants`). Only Caverns (23) and Graveyard (9)
had layouts of their own; the other seven biomes played Forest geometry only.

**Fewer repeats within a dungeon.** Each graph room used to pick its layout at
random, with no memory of the rest of the dungeon. `RoomLibrary::AssignLayouts`
now picks per room, in this order:
1. an exact-door layout not yet used in this dungeon;
2. an unused four-door layout of the same role (unused exits are sealed, as
   they already were for missing door shapes);
3. otherwise, the least-used compatible layout, exact shapes first.

Biome, role and door rules are unchanged. Elite and Store rooms borrow Standard
layouts only when their own role has none; Treasure and Boss never borrow.
A small pool repeats layouts and the dungeon still forms. Generation only
retries a new graph when a room has no compatible layout at all, as before.
Saves store each room's layout id, so Continue never rerolls a room.
- 1,200 sample dungeons (15,600 rooms): 2,558 repeated layouts with the old
  pick vs 1,462 now. Every remaining repeat is forced: all of that slot's
  compatible layouts were already in the dungeon.
- Not changed:
  - the crossroads hub's single room;
  - the room editor's playtest;
  - the per-room safety pick, used only if a room has no assigned layout;
  - the procedural fallback, for when a biome's library cannot form a dungeon.

**Seven biome-specific rooms**, one per biome that had only reskins. They are
new layouts (each test-checked against every existing room), built only from
art the biome already ships: its walls, doors and floor, its prop records with
their real collision boxes, and the existing water/lava pool tiles.

| Biome | Room | Doors | What it changes |
|---|---|---|---|
| Lost City | Colonnade | N+S | Two rows of pillars: an aisle and two flanks, cover from ranged foes |
| Ancient Castle | Pillar Hall | E+W | Long hall between double columns |
| The Sanctuary | Basin Court | all | Basins and braziers ring an open court |
| Wastelands | Rubble Field | all | Scattered, uneven rubble and a dead tree |
| Dream Realm | Statue Circle | all | Six gargoyles in a ring, lamps at the corners |
| Demon's Insides | Lava Pools | all | Four lava pits leave a cross of floor |
| Jungle | Sunken Pools | E+W | Two long water pits squeeze the fight onto a path |

- Made by `tools/biome_content/biome_rooms.py`. It refuses to write a room
  unless:
  - every prop is inside the walls, with no collision overlap;
  - the door lanes and the event-shrine centre are open;
  - every door can reach every other.

  Previews are in `docs/biome-content-audit/biome-rooms/`.
- All seven are Standard rooms. Caverns and Graveyard already had their own
  rooms, and Forest is the source layout set, so they got none.

### Verification for the room work
- New suites:
  - **RoomSelection:** assignment rules and repeat statistics.
  - **BiomeRooms:** for each new room, it loads and builds with every asset
    resolved, and has its declared biome, role, doors and pits. Its door lanes
    and centre are open, and its floor is one connected region. For every door
    shape it can fill, only those doors pass, the rest are sealed. It is also
    checked to be a new layout and to be picked by real dungeon assignment.
- Flow phase 3 enters each new room on a real domain graph. The checks pass
  that it builds, seals the right doors and spawns an encounter with no enemy
  in a pit. Screenshots: `tour_*_biome_room`.
- Phase 5 now checks that every room keeps its layout after Continue, not just
  the current one.
- Results:
  - 36/36 suites pass;
  - flow phases 1–6 pass, with 92 + 11 + 101 + 8 + 14 + 56 checks;
  - Release, Debug, Shipping and PublicDemo build with 0 errors.
- **Still for Robert's eyes:**
  - how the seven rooms look and play;
  - four-door rooms standing in for missing shapes, with their exits sealed;
  - fairness of the lava and water pits;
  - readability with a full encounter.
