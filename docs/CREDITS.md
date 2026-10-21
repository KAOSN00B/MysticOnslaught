# Credits and third-party assets

This page lists only attributions verified from license files or the publishers'
published terms. Anything not yet verified is in "Still to confirm"; nothing here
is a guessed license.

## Music
- **Ovani Sound**, Fantasy Music Pack Vol. 7. These tracks are tail-folded into
  seamless loops; see `Music/music_manifest.json`.

  | Track | Used for |
  |---|---|
  | Mystwood Reverie | Forest |
  | Feysong Fields | Jungle |
  | Tomb of Kings | Lost City |
  | Enemy Grounds | Wastelands |
  | Black Doves | Graveyard |
  | Above | The Sanctuary |
  | Denmark | Village |
  | Stormborn Destiny | World map |
  | Elder Awakening | Final boss |

  The license is royalty-free, with commercial use allowed. Credit is
  "highly appreciated", not required. Standalone redistribution and AI training
  are prohibited (https://ovanisound.com/policies/terms-of-service).

## Sound effects
- **Ovani Sound** packs (same license as the music):
  - Gothic Fantasy Sound FX Pack Vol. 1
  - Inventory Sound FX Pack Vol. 2
  - Medieval Fantasy Sound FX Pack Vol. 4
- **ElvGames** packs:
  - Footsteps Sound FX Asset Pack
  - Game Sounds FX Pack I
  - Game Sounds FX Pack II

  The license (License.txt in each pack) allows commercial use and editing, and
  **requires credit to ElvGames**. It forbids AI training, crypto/NFT use,
  reselling the packs and claiming them as your own (https://elvgames.itch.io/terms).

## Art
- **ElvGames**, "Rogue Adventure" (village tileset `MapTilesets/Village.png` and
  the village buildings composed from it). Same ElvGames license: credit required.

## Still to confirm before release
- **Game SFX Treasury v1.1** has no license file on this machine. None of its clips
  ship: they are unreferenced and pruned from the package.
- The older sounds in `Sounds/` and the original music (Title, Pause, Dungeon,
  Forest (old), Boss, Shop, Victory, Game Over, and the three Biome*.wav themes)
  do not byte-match any pack on this machine. Their source and license need
  confirming; see `docs/AUDIO_PROVENANCE.md`.
- Character, enemy, boss, tileset and UI art other than the ElvGames village art.
  Their packs and licenses still need listing here.
- Each newly confirmed attribution must also be added to the in-game credits
  (`TestGame/Credits.h`).

## In-game credits
Main Menu > Credits shows the verified entries above (Ovani Sound, ElvGames,
raylib), which satisfies the ElvGames credit requirement. The list lives in
`TestGame/Credits.h`, and the Credits test suite fails if the ElvGames or Ovani
entries are removed.
