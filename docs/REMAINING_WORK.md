# Mystic Onslaught: what's left (updated 2026-10-19)

One page of everything still open, rewritten after the 2026-10-19 bugs and performance pass. Details live in `PROGRESSION_OVERHAUL.md`, `FEEL_AND_CONTROLS_PLAN.md`, `UI_SCREENS_PERF_PROMPT.md`, `frontend-audit/AUDIT_PROGRESS.md`, `ENGINEERING_REVIEW_AND_AGENT_PROMPT.md` and `RELEASE_PLAN.md`.

## Where things stand

**Done and working:**
- Cleanup; Draw no longer changes game state; legacy code deleted.
- Overhaul step 1: signed JSON saves, run log, cause-of-death tracking.
- Overhaul step 2: ×10 numbers, Defense, crits, damage numbers, a plain Character page.
- Every old `RELEASE_PLAN.md` P0 item, and every P1 except P1-16 (re-check table in that doc).
- **2026-10-19:** the known bugs and leftovers and the performance items (section 5 below).
- Tests (2026-10-19, final source): all three builds, Debug, packaging and the packaged smoke test (now with zero startup warnings) pass. Flow phases 3–6 pass with 0 failures. The only failures are the village ones below: the `VillageLayoutData` suite and 4 village checks in phases 1–2.

**Needs you first:**
1. **The village is empty.** On 2026-10-18 every building line was removed from `VillageAssets/VillageLayout.vlayout` during the unfinished village art change. The game now loads the village with no homes, Trophy Hall, Armory or Zeph's shop. One test suite and 4 flow checks fail because of it. Restore the old lines or place the buildings for the new art.
2. **158 uncommitted changes** on `feature/enemy-combat-rhythm`, and the push is still pending.

## 1. Plans already written, waiting to start

| Plan | What it covers | Status |
|---|---|---|
| `FEEL_AND_CONTROLS_PLAN.md` | Speed, dash and enemy sizes in "body heights", the aim reticle, a controls check, menu backgrounds, villager navigation | Decisions given; steps 1–6 not started. The player hitbox and the hold/release ability rule are known unfinished. |
| `UI_SCREENS_PERF_PROMPT.md` | Stage 1: 9-slice fix. Stage 2: UI layer (theme, font, buttons). Stage 3: move screens out of `Engine` | Not started (its Stage 4 performance work is done; see section 5). Needs your font and How To Play layout choices. |
| `frontend-audit/AUDIT_PROGRESS.md` | 17 confirmed UI findings | Reports not written; some screens not yet screenshotted. |
| `PROGRESSION_OVERHAUL.md` | Build-order steps 3–12 | Steps 1–2 done; step 3 next. |

## 2. Game systems still to build (overhaul steps 3–9)

1. **Progression core:** Renown alongside Echoes, the unlock framework (requirements, visibility, tiers), the post-run reveal screen, the unlock map.
2. **Ability rules:** 1 at start, 4 maximum, a locked kit per class, ultimates earned by playing the class, the Armory reshaped.
3. **Skins:** the agreed roster (4 starters, 4 onesies, 13 class looks), unlock sources, a wardrobe.
4. **Gear:** slots, weights, primary stats, hand-made bases plus rolled bonuses, sets; the defense K per domain.
5. **Merchants and village:** trainer, apothecary, blacksmith and wardrobe buildings; living villagers; the hybrid day clock and lighting; building doors.
6. **Death lessons:** 50 lessons and relic pool growth (check lessons 41–50 against what the death record stores).
7. **Difficulty tuning** with real run-log data.

## 3. Content and art

- **Paladin and Warlock** still reuse the Warrior and Mage sprites.
- **The village:** the empty layout (above); only 5 villager sprites; placeholder clothing colours; windows where doors should be; a placeholder interior.
- **Sound placeholders:** staffs use the sword-swing sound, enemies share the player-death sound, no level-up chime.
- **New art for new systems:** gear icons, unlock map, merchant buildings, relic and lesson icons, a unique icon per ability.
- **Writing:** Poe's story scripts, the 50 lesson texts, villager advice, tutorial text.

## 4. UI and presentation (confirmed in screenshots)

- Three different visual styles across the menu, pause and settings; raylib's built-in font everywhere; about 1,400 hard-coded colours.
- Reward toasts and banners overlap at the camp and crossroads; the "BOSS ROOM" banner covers the boss; no proper boss health bar.
- Pause title and footer hint nearly invisible; "Button Mapping" on desktop opens the touch layout editor.
- HUD: gold, MP and objective text all yellow; the bottom HUD covers about 20% of the playfield; ability names overflow slots; low contrast on red domains.
- "Echoes" vs "cells" naming; Trophy Hall bars look empty next to "5 / 15".
- Text that may not match behaviour (old P1-16): needs a fresh pass.
- Not yet screenshot-checked: class select, level-up and ability cards, Zeph's shop, Poe's altar, world map, How to Play, victory.

## 5. Bugs and performance: done on 2026-10-19

- **Fixed:** the controller starter pick (one press both picked and confirmed); ability-bar clicks also swinging melee; three timers that ran inside Draw; reach upgrades doing nothing for melee classes; lifesteal overlap (a strike-heal could last 8 s); backtracking raising reward rarity and difficulty; room affixes re-rolling on re-entry; no controller input for the Ascension picker; the seven missing legacy art files.
- **Checked:** no room falls back to random generation (all 269 rooms build).
- **Faster:** domain entry loading ~600 ms → ~15 ms after the first domain of a session.
- **Measured:** 50 enemies in a fight cost about 1 ms of CPU per frame (p99), so combat needs no optimization on this PC. Re-run any time with `tools/run_combat_stress.ps1`.
- **Small leftovers:** load the room library at startup (optional); dense-combat test on a low-end PC or Steam Deck; dead-code candidates (`_maxActiveEnemies`, the legacy arena map load, empty `_props` loops).

**2026-10-21, Shieldbearer:**
- **Done:**
  - **Guard meter:** blocked hits fill it; a full meter breaks the guard, that hit lands, and it staggers with the shield down.
  - **Shield drops while it swings.**
  - **Damage over time always gets through.** Poison and bleed ticks were fully blocked before.
  - **Shots, splash, chain lightning and blasts** are judged by where they arrive from, not where the hero stands.
- **Your call:** placeholder values in `GameBalance.h` `Balance::ShieldGuard`.
- **Checks:** 5 new phase-6 checks pass.
- **Still open:**
  - **Cyclops:** it never appears in normal rooms because it's missing from `EncounterPlanner.cpp`'s list. It was meant to be a regular enemy and needs your weights, a per-room cap and a size decision.
  - **Phase 6 has 2 failures unrelated to the shield:**
    - "banner: the room holds still while the intro banner plays": the room-intro freeze was later removed on purpose (only the elite banner freezes now), but the check still expects it. Decide which one is right.
    - "events: 1-2 events in the first domain…"

## 6. Your decisions (open questions)

1. **The village layout** (above).
2. Does the ultimate count toward the 4 ability slots?
3. Merchants in the village, at in-run camps, or both?
4. Economy: which currencies, what persists, a gear material?
5. Final defense formula values and K growing per domain.
6. Difficulty numbers (once the run log has data).
7. Event-room unlocks: yes or no.
8. Villager crowd: recolours or new art.
9. Pixel font for the UI, and the How To Play book layout.
10. Mouse aim line: keep or remove (recommendation: remove).

## 7. Release and business

- **Steam:** store page (capsule, screenshots, trailer, description, tags); Steam Cloud via Auto-Cloud on `%LOCALAPPDATA%/MysticOnslaught/Saves` (decide whether `settings.json` syncs).
- **Demo:** a downloadable demo build at the end; Steam Next Fest June 2027 (register by April 25, 2027).
- **Quality:** a real Shipping playthrough; Steam Deck and low-end PC tests; accessibility (UI scale, colourblind-safe colours, text size); bug reporting; a playtest process.
- **Legal/admin:** a licence folder for every asset pack and font; a name/trademark check; the business setup (the guide on your Desktop).
- **Marketing:** trailer beat sheet, pitch, regular clips, a Discord.
- **Housekeeping:** commit and push; delete the old OneDrive copy and `MysticOnslaught-master-OLD-partial` when sure; optional CI.

## 8. Recommended next steps

1. **Fix the village layout**, then **commit and push**.
2. **Feel and controls, steps 1–4.** The sprite-size change left speed, dash, hitbox and enemy scale out of step, so the game currently feels off.
3. **UI quick wins plus a font choice** (toast overlap, the boss banner, pause contrast, HUD colours, slot overflow), then UI Stages 1–2. Presentable screens for Steam.
4. **Overhaul step 3 (progression core)**, then **step 4 (ability rules)** and **step 5 (skins)**.
5. **Steam page** once the screens look consistent, so wishlists build while the rest is made.
