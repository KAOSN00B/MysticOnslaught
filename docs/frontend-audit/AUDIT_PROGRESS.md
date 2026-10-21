# Frontend audit: progress notes (interrupted 2026-10-17)

Read-only audit of the player-facing UI. **No source code was changed.** The six requested reports (FRONTEND_AUDIT, SCREEN_BY_SCREEN_REVIEW, UI_DESIGN_SYSTEM, UI_BUG_REPORT, UI_IMPROVEMENT_ROADMAP, UI_PLAYTEST_CHECKLIST) are **not written yet**; this file preserves everything found so far so the next session can continue.

Evidence screenshots (1920×1080 virtual canvas, current build): `x64/FlowTest/20261017-105912/<profile>/shots/`.

## Pass 1: discovery (done)

- **Implementation:** immediate-mode raylib primitives inside `Engine` (Draw*/Update* pairs per `GameState`), plus helper classes `MainMenu`, `PauseAndGameOver`, `ShopManager`, `WorldMapManager`, `HUDRenderer`, `OverlayRenderer`, `DialogueBox`, `CutsceneManager`, `DamageNumberManager`, `VFXManager`, `TouchControls`. No raygui. Some 9-slice textures (`_shopBorderTex`, `_settingsBorderTex`).
- **Rendering:** everything draws to a fixed 1920×1080 render texture (`Engine.cpp` ~428, `LoadRenderTexture(kVirtualWidth, kVirtualHeight)`), letterboxed to the window in `RunFrame`. No `SetTextureFilter` on the canvas, so the default (nearest) filter applies when scaling. LIKELY: thin text breaks up at 1280×720 and shimmers at non-integer scales (2560×1440). Verify by downscaling a screenshot with nearest-neighbour.
- **Fonts:** zero `LoadFont` calls. All 744 `DrawText` calls use raylib's built-in 10 px font scaled to arbitrary sizes (13–52 px), giving uneven pixel scaling.
- **Colours:** about 1,415 `Color{...}` literals and 1,164 named-colour uses; no palette or theme module.
- **Motion:** 8 easing references in UI code; almost no eased animation.
- Large UI functions: `DrawLevelUpChoice` 850 lines (Engine.cpp:6142), `ShopManager::Draw` 799 (ShopManager.cpp:1325), `DrawDungeonRun` 721, `UpdateSettings` 544, `DrawHowToPlay` 434, `MainMenu::Draw` 283, `DrawHUD` 249, `DrawAbilityBar` 219, `WorldMapManager::Draw` 222. A full function list is reproducible with the inventory script approach (Draw*/Update* by class, with line counts).
- `GameState::CosmeticShop` exists in the enum but has no Draw case: check whether it's dead.

## Confirmed findings so far (from screenshots)

1. **No unified visual language (P1).** Main menu = parchment + ornate chain frame + grey-blue ribbon buttons (`profile_resume/shots/resume_menu.png`). Pause = different dark gold frame with seven differently coloured buttons (`profile_events/shots/pause_menu.png`). Settings = navy and cyan modern panel on a blue checkerboard (`profile/shots/00_settings_comfort.png`). Cards (relic, events, Armory, Grimoire, Trophy Hall, run summary) = flat dark rounded panels. The checkerboard background appears in at least five colours.
2. **Toasts and banners overlap (P1 bug).** `profile/shots/02_crossroads.png`: "DOMAIN CLEARED" overlaps the "New Relic" toast, and "FEAT EARNED" covers the north door sign (Graveyard / Pumpkin Jack). `profile/shots/02_zeph_camp.png`: Echoes banked, Road's Reward, Feat and New Relic toasts stack on top of each other, with the "Zeph" label showing through. Cause to check: `DrawHUD` stacks three toasts, but `DrawFeatToast` and the room banners position themselves independently.
3. **The boss intro covers the boss (P1).** `profile_tour/shots/tour_3_Demons_Insides_boss.png`: the "BOSS ROOM" banner and the Feat toast are drawn over the boss and its health bar.
4. **No prominent boss health bar (P1).** Bosses only get a thin green bar over the sprite, with no name and no top-of-screen bar (`profile_tour/shots/death_payoff_2_after.png`). Verify in `DrawDungeonRun`.
5. **Pause title and footer unreadable (P1).** Black "PAUSED" on dark brown; the footer hint "Click: Select  Esc: Resume" is nearly invisible (`pause_menu.png`, `PauseAndGameOver::DrawPause` at PauseAndGameOver.cpp:330).
6. **Pause shows "Button Mapping" on desktop.** It likely opens the touch-layout editor (`TouchButtonMapping`), which is meaningless with mouse and keyboard. Verify in `PauseAndGameOver` labels and actions.
7. **HUD colour collisions (P2).** Gold, MP orb and objective text are all yellow; Echoes are pink. The red HP orb and pink text lose contrast on red biomes because HUD panels are translucent (`tour_3_Demons_Insides_boss.png`).
8. **HUD footprint (P2).** The bottom HUD spans about y 845–1065 × x 440–1480, about 20% of the playfield, including an opaque black rage bar (`death_payoff_1_mid.png`). Three large counters are stacked top-left (Gold / Enemies Left / Echoes at 45–48 px). "Enemies Left" shows in boss rooms and at the crossroads (0).
9. **Ability names overflow slots (P2).** "Cleave Wave" and "Shoulder Charge" extend past the slot boxes (`02_zeph_camp.png`; `DrawAbilityBar`, Engine.cpp:5048).
10. **Settings layout (P2).** The "100%" slider value runs into the panel frame; uneven row spacing; a large empty lower area; Back sits outside the panel (`00_settings_comfort.png`; `DrawSettings`, Engine.cpp:17883).
11. **Terminology inconsistent (P2).** The Trophy Hall says "cells" / "Milestone cells earned" while the HUD and run summary say "Echoes" (`profile/shots/04_trophy_hall.png`).
12. **Trophy Hall bars (LIKELY, P2).** "5 / 15" is shown next to an empty bar. The bar seems to show progress within the current rank while the text is cumulative.
13. **Icon reuse (P2).** The red sword icon is used for the Cleave Wave ability and the Ambush relic; Rogue abilities share one sword icon (`05_armory.png`, `relics_new_choice.png`).
14. **Duplicate NPC labels (P3).** A small "Poe" plus "[E] Poe" (`01_village_first_visit.png`).
15. **Run summary (P2).** No visible "continue" prompt, no hero or relic icons, the right panel mostly empty (`profile/shots/run_summary_death.png`).
16. **Grimoire relics tab (P3).** A text-only wall of "???"; relic icons exist but aren't shown (`grimoire_relics.png`).
17. **Main menu first impression (P2).** A static checkerboard background with no game art or hero; the title is default-font text on a ribbon, not a logo; selection is shown by fill colour only.

**Strong screens:** relic and event cards (clear hierarchy; rarity shown as a label plus colour; good footer hints), the Armory (locked cards state their requirement), the Trophy Hall and the credits.

## Remaining work (next session)

1. **Capture the missing screens** without touching the repo. Copy `TestGame/*.cpp|h|vcxproj*` into a scratch folder, add a temporary flow "phase 7" UI gallery there:
   - Edit the copied `FlowSelfTestRequested`/`InitFlowSelfTest` to accept '7'.
   - Skip the auto level-up answer for phase 7.
   - Open each screen and call `FlowScreenshot`: ClassSelect (fresh, plus a locked class), Settings Display/Audio/Keybindings, HowToPlay tabs 0–3, Poe's altar (`MetaShop`), Shop (see Engine.cpp:19755 Store-room flow), LevelUpChoice (+ starting-ability variant), AbilityChoice, WorldMap (`OpenWorldMap`), CurseShrine (`OpenCurseShrine`), DeathRevive, DemoEnd / victory summary, village intro dialogue, TouchButtonMapping.
   - Use `SetMousePosition` with the letterbox transform for hover states.
   - Build with MSBuild (see `tools/build.ps1` for the arguments) into scratch, and run it from the real `TestGame` working directory with a sandbox `MO_PROFILE_DIR`.
2. **Simulate resolutions:** nearest-neighbour downscale screenshots to 1280×720 and check 1440p/4K scaling factors.
3. **Code passes:** input consistency (Esc/back, gamepad focus per screen), shop affordability and feedback, level-up card hierarchy, UI audio (`SfxBank` UI cues and volume), onboarding (`DungeonTutorial`, HowToPlay), modal layering.
4. **Write the six reports** in this folder with the evidence format the brief asks for (screen, file, function, line, current behaviour, issue, consequence, recommendation, priority, effort).
