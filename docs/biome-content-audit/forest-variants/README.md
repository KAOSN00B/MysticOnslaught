# Forest layout families in every biome

Every biome now runs on authored rooms. Forest and Jungle already did; the other
eight biomes each received the 23 Forest layout families re-drawn in their own
art. Caverns and Graveyard keep their original rooms as well.

- Generator: `tools/biome_content/forest_variants.py` (palettes per biome, every
  rectangle chosen after inspecting the sheet). Promotion (copy-only, refuses to
  overwrite changed files): `tools/biome_content/promote_forest_variants.py`.
  Preview renderer: `tools/biome_content/render_room.py`.
- Method: every Forest art reference is classified by role:
  - floors, paths and leafy patches;
  - 3x3 fence and cliff ring pieces;
  - overlays and bridges;
  - props and decor.

  Each role maps to the biome's own art. Fence and cliff rings use the biome's
  fence where it has one; otherwise they use its curated wall tiles (tilemapper
  TILE records). Props and decor are drawn into `MapTilesets/<Biome>_ForestVariants.png`
  at the exact Forest footprint with Forest's collision rectangle, so gameplay is
  unchanged. Water becomes lava in Demons Insides.
- Borrowed art (explained): Ancient Castle has no registered props, so its
  barrels, pots, stools and torches come from Lost City, whose catalog shares the
  "Ancient Castle" family header. Its columns are its own art. Graveyard floors
  use `Village_Ground`, like the original Graveyard rooms. Wastelands floors use
  `Ground TIles`, its curated GTILE floor.
- Installed: `TestGame/Rooms/Generated/<Biome>/forest_variants/` (184 rooms).

## Verification

- **Unit suite.** `tools\run_tests.ps1` runs the `ForestVariants` suite in the
  default set. It checks that each of the 184 variants matches its Forest source
  exactly:
  - tile, solid and fall grids, colliders, fall rectangles, prop hitboxes and
    footprints;
  - doors, room role and door clearing;
  - 14,336 sampled body and fall positions per door state.

  It also checks that only permitted sheets are used and every sprite stays in
  bounds. Beyond the variants, it checks that all 262 library rooms build without
  warnings, and that all 10 biomes form complete dungeons, with and without the
  Zeph's-camp entrance.
- **Flow test, phase 3.** `tools\run_flow_selftest.ps1` enters every domain through
  the real world-map path. It checks that every room is authored and saves
  entrance and boss screenshots; copies are in `in-engine/`.
- **Contact sheets.** Each biome folder has a `contact.png` of all 23 rooms.

## Still needs a human eye

Art choices are first-pass and were judged from still images only. The weakest
areas:
- Ancient Castle's furniture props.
- Dream Realm's flat tiling.
- Forest water banks, which keep their brown edge in every water biome.

Swap any rectangle in `PALETTES`, then:
1. Regenerate the variants.
2. Re-promote them. Promotion refuses to overwrite changed files, so remove the
   old copies first, with your OK.
3. Re-run the tests.
