# Mystic Onslaught: UI layer, screen extraction and performance (agent prompt)

You are a senior C++17/raylib engineer working in `B:\GamesMadeInEngines\MysticOnslaught-master`. The game is native Windows only. The owner, Robert, is learning from this work: explain what you change and why, show where to look, and leave design decisions to Robert.

## Decisions so far (Robert, 2026-10-18)

- Keep the current art everywhere. Don't swap art styles or button art.
- Menu buttons: Option A. Make them 512x96 and draw the existing PlayButton art 1:1. Don't use QuitButton art.
- Inventory is a planned later screen. Start it as a tab next to the Character page, showing relics, consumables and gold.
- Stage 1 first, then check it in the game before Stage 2.

## Read first

1. `docs/ENGINEERING_REVIEW_AND_AGENT_PROMPT.md`, especially "Continuation pass 2026-10-17". Draw paths no longer change game state, dead code was removed, and dev-only `FRAMETIME` and `JOURNEY detail` timing already exist. Don't redo any of that.
2. `docs/PROGRESSION_OVERHAUL.md`, section 12 (menus and UI polish) and "Suggested build order". This task is groundwork for build-order item 10 and for the overhaul's new screens (post-run reveal, unlock map, gear loadout). Don't start any other overhaul item.
3. The current code. Every line number below was checked on 2026-10-17; read the code again before editing.

## Ground rules

- **Saving is out of scope.** Don't touch checkpoint handling, save formats, save parsing or restore rules, or change when saves happen. Every existing save call keeps its exact behavior and frequency.
- **Back up every file before you edit it.** The working tree has a lot of uncommitted and untracked work, including all of `tools/`. Copy each file to `x64/UiPerfBackup-<yyyyMMdd>/` under its relative path first.
- **Never reset, clean, mass-format or delete files.** Leave deletions to Robert.
- **Preserve encodings.** `TestGame/Engine.h` uses CRLF line endings and contains pre-existing U+FFFD characters. `Engine.cpp` uses LF. Keep bytes and line endings exactly as they are.
- **Don't edit `TestGame/` sources while a build is running.**
- **Never commit or push.**
- **No magic numbers.** Every new literal gets a named constant or a theme/config field.
- **No engine rewrite and no ECS.** Work in small verified steps.
- **Run the gate after every stage:** `tools/verify_native.ps1 -Flow`, from the project root with absolute paths. Report pass or fail honestly, with the output folder. Use `tools/build.ps1 -Configuration Release` for performance measurements.
- **Design decisions belong to Robert:** the font, which art style each screen uses, colors, the How To Play layout, and button art. When you need one, keep the current look as a placeholder and ask one short question with your recommendation.
- **Check visuals with screenshots** taken before and after each visual change (the flow self-test already captures screenshots). If a frontend-design skill is available, follow its process (plan palette, type and layout, check it against generic defaults, build, critique from screenshots) and skip its web-only advice.

## Stage 1: fix the 9-slice math (small change, visible result)

**The problem.** `TestGame/NineSlice.h` (`DrawNineSlice` at line 10, `DrawNineSliceEx` at line 64) uses one `dstCorner` value for every side, whatever the size of each source cut. That scales each side by a different amount. For example, the How To Play top cut goes from 62.8 px to 16 px (0.25×), while the bottom cut goes from 15.3 px to 16 px (about 1×). The cuts are also fractional: `UI/nineslice_*.txt` holds values like `62.7895`. Pixel art needs whole-pixel cuts and one whole-number scale shared by all nine pieces.

**The work.**
- Add a new function, for example `DrawNineSliceScaled(Texture2D, NineSliceCuts{int top, bottom, left, right}, int scale, Rectangle dest, Color tint)`.
  - Each piece's on-screen size is its cut × `scale`.
  - Snap `dest` to whole art pixels (multiples of `scale`).
  - Keep the old functions until every caller has moved over, then ask Robert before removing them.
- Update `NineSliceEditor`: snap cut lines to whole pixels and save `scale` instead of `dstCorner`. The cut-value drag code is around `NineSliceEditor.cpp:385-435`. It must still read the old `.txt` files.
- Apply these cuts. They were measured from where each frame's edges stop changing.

| Asset | Drawn at | Now | New cuts | Scale |
|---|---|---|---|---|
| `UI/PauseBoarder.png` (dialogue and Zeph box) | `DialogueBox.h:20-21`, `DialogueBox.cpp:176` | 16 → 28 | top/bottom 32, left/right 22 | 3 |
| `UI/MainMenuBorder.png` | `MainMenu.cpp:13-14`, `:367` | 1 → 20 | top 41, bottom 42, left 40, right 42 | 5 |
| `UI/SettingsBorder.png` | `Engine.cpp` `DrawNineSlice(_settingsBorderTex, 18.f, 22.f, ...)` (~17886) | 18 → 22 | 16 on all sides | 2 |
| `UI/QuitButton.png` (if chosen for buttons) | — | — | 10 on all sides | 3 |

- **Don't 9-slice these two.** Draw each whole, at a whole-number scale:
  - `UI/HowToPlayBorder.png`, drawn at `Engine.cpp:12315`, is a painted book (spine, page ornaments, cover corners). Trim the 56 transparent rows at the top and draw it at 2×, which gives 1152×842. That's narrower than today's 1766×815 panel, so show Robert both versions and let Robert choose.
  - `UI/TitleBanner.png`, drawn at `MainMenu.cpp:411`, is a curved ribbon. It's drawn at 1.575× today; use 2× or 1×.
- **Menu buttons need a decision from Robert.** `UI/PlayButton.png` is 512×96 art squeezed into 384×90 buttons (`MainMenu.cpp:15-16`, `:588`). Option A: make the buttons 512×96 and draw the art 1:1. Option B: use `QuitButton.png` at scale 3. `HowToPlayButton.png` has notches on its edges, so it only looks right at its own size.

**Acceptance:** before and after screenshots of the main menu, dialogue box, settings and How To Play. No behavior change. The gate passes.

## Stage 2: a small UI layer

**The problem.** `Engine.cpp` has about 200 hand-typed screen fractions (like `sw * 0.3703f`). There are about 430 `DrawText`/`MeasureText` calls, all using raylib's built-in 10 px font scaled up.

**The work.**
- **`UiTheme`:** one place for font, text sizes, colors, the UI pixel scale, and frame textures with their cuts.
- **`UiText` / `UiMeasureText`:** wrappers around `DrawTextEx`/`MeasureTextEx`.
  - Keep the default font until Robert picks a pixel font. Load the chosen font at its design size, draw it at whole multiples, and use point filtering.
  - Move call sites over one screen at a time, not all 430 at once.
- **`UiButton`:**
  - Fixed size. Today `MainMenu.cpp:569` grows the button 4% on hover, which re-slices it every frame.
  - Hover brightens it and lifts it 1 art pixel; press pushes it down.
  - Add a keyboard/gamepad selection marker to replace the smooth rounded glow at `MainMenu.cpp:580`. `UI/CursorSword.png` is a candidate.
  - Dim unselected buttons with an opaque darker tint instead of 78% opacity (`MainMenu.cpp:584`).
  - Add hover and confirm sound hooks through `AudioManager`.
- **Layout:** named constants in whole art pixels, kept in the screen's file or in the theme.

**Acceptance:** at least the main menu runs fully on the new layer, with screenshots. The gate passes.

## Stage 3: move screens out of `Engine`

**Where things stand.** `Engine.cpp` is about 22,800 lines, and `Engine.h` declares about 400 fields and 380 methods. Draw already only draws (finished 2026-10-17). The next step is giving each screen ownership of its own state.

**The pattern.**
- Each screen class owns its own state.
- It exposes `ScreenAction Update(const UiInput&)` and `void Draw() const`.
- `Engine` applies the returned actions (resume, open settings, quit to menu, and so on) through its existing functions, so every save call stays exactly where it is.

**The work.**
- Extract in this order: Pause, then Settings, then How To Play, then the main menu (already a `MainMenu` class; move it onto the UI layer), then class select. Extract one screen per step and run the gate after each.
- Move only state that belongs to that screen. If gameplay or saving also uses a field, leave it in `Engine` and pass it in.
- Pause must still draw over the frozen game scene. Use a screen stack, or explicitly draw the world first and then the pause screen.

**Acceptance:**
- Moved code no longer exists in `Engine.cpp`.
- The existing flow phases still pass.
- Repeated draws still don't change state (the existing phase-6 checks).
- New flow checks cover each extracted screen's actions.

## Stage 4: performance (measure first, then fix only what's proven)

**a) Room library reload: the proven bottleneck.** `RoomLibrary::Refresh` (`RoomLibrary.cpp:20`, called from `Engine::RefreshHandcraftedRooms` around `Engine.cpp:989`) re-parses all 269 `.mroom` files (7.5 MB) on every domain entry. That takes about 620–650 ms, roughly 95% of that loading stage.
- **Proposed rule:**
  - Keep the parsed library in memory.
  - On `Refresh`, check each file's last-write time and size. Re-parse only files that changed, and pick up added or removed files.
  - The editors keep calling `Refresh` after a save (`RoomEditor.cpp:232`, `:1653`, `:1952`). Edited files have new timestamps, so the editors get the same result as today.
- **Ask Robert to confirm this rule before you implement it.** Then measure before and after with the `JOURNEY detail` log lines.

**b) Dense combat has never been measured.** Add a dev-only flow phase or debug command that spawns mixed enemies at 10, 25 and 50, with projectiles. Record the existing `FRAMETIME` summary for each group in a Release build. Report p50 and p99 for Update CPU time, Draw CPU time and frame interval.

**c) Optimize only what the measurements show is slowest.** Check these leads, but don't fix any of them without evidence:
- enemy-vs-enemy separation and projectile collision loops (are they comparing every pair?)
- per-frame memory allocations in combat and draw code
- sorting and draw-order work
- texture switches between draw calls
- the amount of damage-number and HUD text drawn each frame

Keep all GPU work on the main thread (raylib requires this).

**d) Re-measure loading after (a).** Loading stages run one after another on the main thread. If one still takes much longer than a frame, report it rather than moving it to another thread.

**Acceptance:** a before/after table of numbers, no behavior change, and a passing gate.

## Reporting

- **In the doc:** append a new dated section to `docs/ENGINEERING_REVIEW_AND_AGENT_PROMPT.md`, the same way earlier passes did. Include:
  - what changed
  - measured numbers
  - gate output folders
  - open questions for Robert
  - a continuation list
- **In chat, after each stage, in plain words:**
  - what you changed and why
  - the files and lines to look at
  - how to see it in the game and what to look for
- **If you run out of room,** stop at a stage boundary with the gate passing and leave a precise continuation list. Don't mark unfinished stages as done.
