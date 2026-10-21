# Combat and dungeon polish playtest — 2026-09-10

Status: first-pass candidate, awaiting player feedback. No commits or pushes.

## Launch

Double-click `PlayPolishPreview.bat` in the project root. It launches the separate
`x64/PolishPreview-2026-09-10` package from its correct content directory.
Its assets and settings are copies; your normal executable and settings are not replaced.
This preview keeps the current Debug x64 demo configuration and its existing run flow.
It is not a Steam release candidate or a completed full-game build.

## Changes to assess

- Brief directional white contact accents at landed hits; stronger local rings on kills/criticals.
- Physical/holy impact sound reinforcement for Warrior, Rogue, Hunter, and Paladin.
- Normal hits use 12–35 ms hit-stop; kills use 45 ms. Direct feedback is rate-limited
  to 75 ms and critical/kill accents to 120 ms, preventing multi-target stacking.
- Normal critical hits no longer trigger whole-fight slow motion. Boss victory keeps its cinematic beat.
- Camera shake fades quadratically instead of stopping abruptly; weaker requests cannot
  overwrite a stronger remaining shake. Shake still respects the existing juice multiplier.
- Recurring mage field ticks retain damage, combos, and status application, with quieter
  local feedback and no repeated impact recoil/hit-stop. Initial impacts still get direct feedback.
  Ordinary poison/bleed already bypass this Engine feedback path and were not changed.
- Existing encounter profiles now differ in reinforcement spacing: Skirmish 2.0 s,
  Assault 1.15 s, Swarm 0.65 s, Holdout 1.5 s. An empty arena can refill after a 0.45 s
  breathing interval. Existing spawn telegraphs still follow; these values are not total spawn latency.
- Existing simultaneous body caps, total enemy budgets, room geometry, and enemy health are retained.
- Repeated central reinforcement banners are removed; ground warning circles and smoke remain.
- Room clear gets a brief local gold cue and confirmation sound. Doors/rewards are not delayed.
- Transient sparks/contact accents are cleared with the room effects rather than following room resets.

## Suggested 10–15 minute playtest

1. Use your usual melee class in a familiar Forest room. Compare a single hit, critical,
   and killing blow. Does contact feel clearer without losing movement responsiveness?
2. Hit a group with an area attack. Check that effects stay readable and the game does
   not keep freezing as individual enemies take hits or die.
3. Try a mage persistent field (fire wall, meteor aftermath, blizzard, or lightning storm).
   Check that ongoing damage/status effects still work and targets are not repeatedly
   shoved by presentation recoil. Compare the first spell impact with subsequent ticks.
4. Play several rooms with different encounter profiles. Check the space between batches,
   warning positions, and whether the last enemy actually leads to an unlocked exit.
5. Clear a room, collect rewards, revisit it, and transition again. Look for leftover
   particles or repeated clear cues. Test a boss with adds as well.
6. Tell me which feels best/worst: contact, camera movement, spell fields, room pacing,
   and completion. Especially flag idle waiting, weak hits, visual clutter, or missed feedback.

The next iteration should be based on this playtest. Room composition/discovery variation,
ambient environmental polish, and a wider progression/content pass remain future work.
No designed room needs to be remade for this pass.

## Verification

- New combat presentation tests: 19 checks pass. The baseline scaffold failed 11 checks
  before implementation, including cooldowns, periodic-hit suppression, rhythm, and shake decay.
- Existing CombatRhythmTests and EncounterPlannerTests compile and pass.
- Full Debug x64 build succeeds using the installed B:/VisualStudio toolchain.
- Independent review found no important regressions, including the spell-field call-site follow-up.
- Packaged executable stayed running during a six-second hidden startup smoke test and
  responded to the close request. This does not verify gameplay feel, controller use, or sound quality.
- All 55 authored room files match their pre-change SHA-256 hashes.
- Packaged executable hash matches the latest successful build.

## Existing limitations observed

The compiler reports conversion warnings and the initial clean build reported a CRT library
conflict warning. Startup reports missing legacy files also absent from the source project:
ForestLevel/{Tree,SmallTree,Rock,BigRock}.png; Hero/Mage_{Staff,Push,Jump}.png;
TileSet/{Pillar,Torch,PillarTorch,Map}.png. Shutdown also reports a redundant audio-device close.
These were not silently replaced with new assets in this polish pass.

The first build attempts exposed local toolchain/environment issues, resolved for this build
by using B:/VisualStudio MSBuild with a normalized child-process environment. Project toolchain
settings were not altered. Release/web builds were not revalidated in this pass.

## Files

Modified: TestGame/Engine.cpp, Engine.h, VFXManager.cpp, VFXManager.h.
Added: TestGame/CombatPresentation.h, CombatPresentationTests.cpp, PlayPolishPreview.bat,
and this playtest note. Existing unrelated uncommitted work is retained.

The preview's `Verification` folder contains the build/startup logs and session-only diff.
