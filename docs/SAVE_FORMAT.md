# Mystic Onslaught: save format

Clean slate from 2026-10-17 (progression overhaul, build step 1). Nothing from the old `*.cfg` files is read or migrated, and those files are left where they are.

## Where

One folder holds every save: `<user data>/Saves/`.

| Build | Folder |
|---|---|
| Shipping (full game) | `%LOCALAPPDATA%\MysticOnslaught\Saves\` |
| Tests and flow self-test | `%MO_PROFILE_DIR%\Saves\` |
| Developer builds without `MO_PROFILE_DIR` | `Saves\` under the working directory |

| File | Contents | Form |
|---|---|---|
| `profile.mosave` | Permanent progress (`MetaProgression`) | Signed |
| `run.mosave` | The suspended run (`RunCheckpoint`) | Signed |
| `runlog.mosave` | Finished runs and how they ended (`RunLog`) | Signed |
| `settings.json` | Display, audio, controls, comfort | Plain JSON |
| `keybindings.json` | Keyboard and gamepad bindings (raylib codes) | Plain JSON |

Each file can have `.bak` (the previous good generation), `.tmp` (a write in progress) and `.rejected` (the last file that failed to load, kept for inspection and never read) beside it. Paths and limits live in `TestGame/SaveFiles.h`. Packaging excludes the whole `Saves` folder.

## Signed files

`SaveStore.cpp` writes this envelope:

| Bytes | Field |
|---|---|
| 0–3 | `MOSV` |
| 4 | Envelope version (1) |
| 5 | File kind: 1 profile, 2 run checkpoint, 3 run log |
| 6–7 | Reserved (zero) |
| 8–23 | Random nonce, new for every write |
| 24–55 | HMAC-SHA256 over bytes 0–23 and the payload |
| 56– | Payload: the JSON, XOR'd with HMAC-SHA256(scramble key, nonce ‖ counter) blocks |

- The signing and scrambling keys are derived from one built-in key, `kMasterKey` in `SaveStore.cpp`. Changing that key invalidates every signed save, which is another clean slate.
- SHA-256 and HMAC are implemented in `Sha256.cpp` and checked against the NIST and RFC 4231 test vectors (suite `Sha256`).
- This stops casual editing. An offline game can't stop a determined player who extracts the key, or who restores a whole copied `Saves` folder (save-scumming).

## Loading

1. **Size cap:** checked from the file's metadata before anything is read (`ReadBounded`).
2. **Envelope:** magic, version, kind and reserved bytes, then a constant-time signature comparison.
3. **JSON:** strict RFC 8259 (`Json.cpp`), with no comments, trailing commas, NaN/Infinity, duplicate keys or `\u0000`. UTF-8 must be valid, and there are limits on nesting, string length, container size and total values.
4. **Schema:** every field is checked for type, range and finiteness. Lists are bounded and names must resolve. In the run checkpoint, the room graph must have one room per grid cell, every door must lead to a neighbour that opens back, and every room must be reachable from the entrance.

If any step fails, the file is renamed to `.rejected` and the `.bak` goes through the same checks. A good backup is copied back as the primary. A file that merely can't be opened (locked by another program) is left alone and the backup is used. With no usable file, the game starts fresh.

**Unknown names are not tampering.** Content removed in a later build (a relic, ability, class or domain) is skipped. Only the run checkpoint's route is strict: an unknown domain there rejects the file, because the run can't continue on it.

## Saving

1. The document must be writable: finite numbers and valid text.
2. **Write-verify:** the exact text is parsed and validated as a load would, so the game never writes a file it would later reject. A rejected document fails the save and leaves the disk untouched.
3. The temp file is written, flushed and committed.
4. The current primary is copied to `.bak`, but only if it still verifies, so a damaged primary never overwrites the last good backup.
5. The temp file atomically replaces the primary.

Every save returns success or failure:

- **Profile:** stays dirty and is retried at the next calm save.
- **Run checkpoint:** the run is untouched in memory.
  - *Save & Quit* and *Quit* stay on the pause menu with a "Couldn't save" notice.
  - Closing the window: the first close after a failed write is refused with the notice; a second close quits.
- **Run log:** a failed write is logged.

## Stable names

Saves never store enum numbers for unlocks, relics, abilities, feats, classes, domains or Bestiary creatures. They store the names in `SaveIds.cpp` (for example `"UnlockFireBolt"`, `"LastStand"`, `"Hunter"`, `"Caverns"`, `"Ogre"`). Each table is `static_assert`ed against its enum's size, so a new value needs a name before the game compiles. **Never rename a shipped name.**

Run-only enums (room types, shop wares and so on) are stored as numbers in the run checkpoint. Their ranges are listed in `RunCheckpointLimits` and `static_assert`ed in `EngineRunSave.cpp`.

## Versions

- Each document has `"schema": 1` (`SaveFiles.h`). A file with another value is rejected; when a later change needs it, an upgrade step goes in the loader.
- Optional sections take their defaults when missing, and unknown keys are ignored, so adding a section doesn't need a version bump.
- Numbers are 64-bit integers or doubles. Floats are written in their shortest exact form.

## Room for the progression overhaul

These profile sections are reserved and not written yet:

- `renown`
- `unlockTracks`
- `skins`
- `gear`
- `lessons`
- `merchants`

Add each as an optional object when its feature lands. The bounds in `SaveFiles.h` and `RunCheckpointLimits` (for example the character stat limit of 1e9) leave ample room for the ×10 HP/damage scale adopted on 2026-10-18.

## Profile (`profile.mosave`)

- `runs`: `nextId`, `active`. Every main run gets an id, and the run checkpoint names it. Continue is offered only while `active` matches the checkpoint's id, so a backup of a run that has since ended can't revive it. Ids keep counting through New Game.
- `echoes`: `banked`, `lifetime`.
- `villageGold`.
- `poeUnlocks`: a list of names.
- `classes`: per class, `rooms`, `bosses`, `masteryRank` and an optional `starter`.
- `unlockedClasses`.
- `onboardingComplete`, `gameCompleted`.
- `ascension`: `selected`, `maxUnlocked`.
- `domainsCleared`.
- `deaths`: `total`, `byCause` (keys like `"enemy/Ogre"`, `"status/Burn"`, `"hazard/LavaPool"`), `byDomain`.
- `grimoire`: `relics`, `creatures`, `feats`.
- `village`: `announced`, an optional `layout` of `{asset, x, y}`, and an optional `clock` with `hours` (0–24, the time of day; absent on a new profile, which starts at `Balance::DayNight::kStartHour`). The clock never saves on its own: it is written with the next calm save, in quarter-hour steps.

## Run checkpoint (`run.mosave`)

`schema`, `runId`, `active`. An ended run stores only those three fields. A live run adds:

- `character`: `class`, `appearance`, `stats` (each `Character::VisitRunFields` field by name), `abilities`, `relics`. A stat missing from the file keeps the class default, so the armour-to-Defense change (2026-10-18) needed no schema bump: old `armour`/`maxArmour` values are ignored and `defense`, `ironcladDefense`, `critChance` and `critDamage` are range-checked.
- `run`: route, multipliers, counters, timers, seed, `modifiers`, plus `domainLog` and `domainStart` for the run log.
- `location`: `atWorldMap`, `atCrossroads`, and three `crossroads` roads.
- `dungeon`: the room graph and per-room progress (only when in a dungeon).
- `shop`: Zeph's stock (only when stocked).

## Run log (`runlog.mosave`)

- `totals`: `runs`, `wins`. These count every run ever logged.
- `runs`: the newest 100 records, newest first. Each record has:
  - `runId`, `endedAt` (UTC seconds), `outcome` (`victory`, `death` or `abandoned`), `class`, `skin`, `ascension`.
  - `domains` in order, each with `roomsCleared`, `seconds` and `bossDefeated`.
  - `domainReached`, `seconds`, the kill counters, `gold` and `echoes` carried.
  - `build`: `level`, `maxHealth`, `abilities`, `relics`.
  - `ending`.
  - For a death, `death`:
    - `cause`: `kind`, `id`, `elite`, `boss`, and `by` for statuses.
    - `statuses`: `burning`, `poisoned`, `chilled`.
    - `context`: elite cage, mana full, empty ability slot, class resource full, curse and active modifiers, room type/depth/event/affix, seconds in room, seconds since the last dash, boss HP fraction, gold carried.

### Cause of death

`DamageSource.h` names whatever hurt the player:

- **Enemies** by their Bestiary type (`"SkeletonArcher"`, `"Ogre"`, `"Slime"`), with elite and boss flags.
- **Hazards** by fixed names: `LavaPool`, `FireTotem`, `FireballTorch`, `Pit`, `LavaPit`, `WastelandBlast`, `LostCityBeam`, `EliteCage`, `VolatileCorpse`.
- **Statuses** (`Burn`, `Poison`), together with whoever applied them.

How each source gets named:

- `CombatDirector` holds a source scope around every enemy's turn and credits the projectiles that enemy spawns (arrows, bolts, spit, lasers, lava balls).
- Elite signature zones name their elite.
- Engine hazard sites name themselves.
- Poison clouds remember who left them.

The player records the source of the last hit that actually took HP. A death with no named source is logged as `unknown`, with a warning in the log.
