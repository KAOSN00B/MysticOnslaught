# Release candidate status — September 24, 2026

This is a development candidate, not a completed release certification. Nothing was committed.

## Implemented and checked

- Full native Shipping build with development controls disabled.
- Standalone packaging of supplied assets, tuning, room data and MSVC runtime; no personal profiles copied.
- Shipping saves/settings/keybindings use a per-user directory. Failed progression saves retain pending writes; recovered backups schedule primary repair.
- Travel staging, texture sharing and music crossfades have automated coverage.
- Seven suites in `tools/test_shipping.cmd`: journey, texture cache, audio, opening balance, progression, user paths and actual renderer resource ownership.
- Mage spell constants now use the same definitions as the balance checks. The test includes Lightning Blink's overlapping direct hits. It does not prove every combat scenario is balanced.
- Default appearances missing a Jump sheet use the supplied dash fallback.
- Final-boss exit goes to victory and records completion/ascension immediately; repeated progression callbacks are tested through reload.
- Missing legacy arena paths fall back to the supplied root Map.png. A prior packaged startup hung with a zero-size missing Forest map; the corrected package passed startup and normal shutdown from an unrelated working directory with an isolated profile.

## Biome content

All 55 original authored rooms retain their baseline hashes. Twenty-three Forest-layout Jungle variants have been added under `TestGame/Rooms/Generated/Jungle/forest_variants`, using Jungle artwork and explicitly shared animated water. An additional atlas fits selected Jungle props to the original collision and occlusion footprints.

Native validation checks all 78 installed room loads, all 23 converted layouts, terrain/collision/fall data, prop hitboxes, visual source bounds and door clearing. Each pair receives 14,336 sampled body/fall comparisons. One thousand seeded dungeons found compatible room graphs, requiring at most 29 attempts of the existing 600-attempt limit. The original treasure layouts restrict allowed graph directions; arbitrary graphs are not all supported.

Native-rendered comparison previews are in `docs/biome-content-audit/jungle-pool/*-native.png`. Representative prop-heavy, boundary-tree, multi-door and special-room previews were inspected. The full game now selects the authored Jungle pool during biome travel. The web packaging list includes its metadata, but the web build has not been rerun.

The original audit JSON files are historical baseline inventory: they still report zero conversions. `jungle-pool/promotion.json` is the current verified installation receipt. Of 550 requested original-layout/biome pairs, 55 originals plus 23 generated pairs are installed; 472 remain unconverted. This is not all-biome completion.

## Remaining release blockers

- Complete the remaining biome conversions and visual acceptance, including Caverns/Graveyard source layouts.
- Full interactive runs across classes/biomes, final-boss UI, death/return flow, controller controls and save recovery on a separate machine.
- Broader combat balance evidence: full casts, upgrades, enemy mix, room-clear times and resource economy, beyond opening formula checks.
- Seven absent legacy prop images still produce known startup warnings. The smoke script lists those explicitly; unexpected diagnostics fail it.
- Browser build/runtime validation and target-hardware performance measurements.

## Reproduce

Run `tools\test_shipping.cmd`, `tools\build_shipping.cmd`, then `powershell -File tools\package_shipping.ps1`.
Run `powershell -File tools\smoke_shipping.ps1 -PackageDirectory <printed-package-path>` for isolated native startup/shutdown checks.
Run `python -m unittest discover -s tools\biome_content -p test_pipeline.py` and `tools\biome_content\check_runtime.cmd` for content checks. Add `--render` to regenerate native preview images.
