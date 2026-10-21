# Persistent progression integration

`MetaProgressionManager` retains the existing altar purchases and class/story flags.
The new milestones award a finite total of 1,390 banked cells: 190 for each of six
classes and 25 for the first boss clear in each of ten biomes. These rewards create
choices in the existing unlock economy; they do not add scaling combat stats.

## Engine hooks

At the first transition from an uncleared combat room to cleared, call exactly one:

```cpp
int reward = bossRoom
    ? meta.RecordBossDefeated(currentClass, currentBiome)
    : meta.RecordRoomCleared(currentClass);
```

Do not invoke for store/rest/treasure rooms, revisiting cleared rooms, opening a
portal, debug room completion, or simply starting a run. A boss contributes five
mastery points and an ordinary combat clear contributes one. Boss and reward-bearing clears save immediately. Ordinary room clears without
a milestone defer the disk write; call `SaveIfDirty()` during village arrival,
shutdown, or other calm moments. Both hooks return the number of milestone cells automatically banked, suitable for a
reward toast. The boss hook records a boss rather than also incrementing the
ordinary-room counter. The manager cannot identify duplicate room events: the
Engine must use its existing room-cleared edge guard.

Class rank thresholds are cumulative 5, 15, 35, 65, 100 points. Rank rewards are
10, 20, 35, 50, 75. Biome first clears are global across all classes and ascensions.

## Journal APIs

- `GetClassMasteryTier(cls)`: earned ranks 0 through 5.
- `GetClassMasteryProgress(cls)`: cumulative mastery points, capped at 100.
- `GetClassMasteryTarget(cls)`: next cumulative threshold; 100 at maximum rank.
- `GetClassRooms(cls)` and `GetClassBosses(cls)`: historical clears, each capped at one million.
- `GetMasteryReward(rank)`: reward for a rank numbered 1 through 5.
- `HasClearedBiome(biome)` and `GetBiomeFirstClearReward(biome)`.
- `DidLastSaveSucceed()`: native/browser save outcome; a failed save leaves the
  in-memory profile intact and pending, so `SaveIfDirty()` or `Save()` retries it.
  Successful writes clear pending state; failed attempts never clear it.

Invalid class/biome values give zero/false and do not mutate progress. Currency
saturates at one billion. Existing ascension progression now rejects victories
at tiers the profile has not unlocked.

## Persistence

Installed full native builds store `metaprogress.cfg`, `.bak`, and `.tmp` together
under `%LOCALAPPDATA%/MysticOnslaught` through `UserDataPath` (`APPDATA` fallback).
They do not silently import a development-directory profile. Authoring builds keep
the existing relative profile, and public demos remain stateless.

Old `metaprogress.cfg` key/value files load with all existing unlock indices
preserved. New files add `format=2`, mastery records, biome records, and an FNV-1a
checksum. Windows saves flush and commit a temporary file, keep the last valid
primary as `.bak`, then replace the primary with `MoveFileEx` using write-through.
A missing, truncated, or checksum-damaged primary falls back to the valid backup
and marks it pending so the next `SaveIfDirty()` repairs the primary.
The checksum detects accidental corruption; it is not anti-cheat protection.

Web builds use the origin's `localStorage` key `mystic-onslaught-profile-v2`, with
storage-denial exceptions handled. Browser runtime remains unverified locally.
Public demo builds retain the original stateless behavior: load/save are no-ops.

## Verification

From the project root:

```bat
Tests\RunMetaProgressionTests.cmd
Tests\RunMetaProgressionTests.cmd /DMO_ITCH_DEMO=1
Tests\RunMetaProgressionTests.cmd /DMO_DEV_TOOLS=0 /DMO_ITCH_DEMO=0
```

The runner compiles the production manager using MSVC and runs inside a dedicated
verification sandbox and overrides `LOCALAPPDATA` inside that sandbox so Shipping
path tests never touch the real player profile. Do not run the test executable from a real player profile
directory: fixtures deliberately replace `metaprogress.cfg` in their working
folder. Native checks cover finite reward accounting, all classes, repeated biome
clears, legacy migration, round trip, truncated/checksum-damaged recovery, invalid
inputs, counter limits, and failed-save retry. The demo variant verifies existing
profiles are neither loaded nor overwritten.

The initial regression run failed at runtime on banked-currency integer overflow;
new progression tests then failed to build against the absent API before the
implementation was added. Both final variants pass. Full Engine/build validation
is performed by the integrating agent.


## Continuation audit

The later deferred-save implementation cleared its pending flag before writing.
A regression using `SaveIfDirty()` after a blocked temporary file reproduced the
lost retry (RED); moving flag clearing to successful completion made it GREEN.
A second regression showed backup recovery did not heal the primary during a
calm-moment flush (RED); recovery now marks the profile pending (GREEN). Native
checks additionally exercise backup-copy failure, replacement failure, deferred
room progress, and no redundant disk write after a successful flush. No commits
were made during this audit. Browser/Emscripten runtime remains unverified.

The Shipping variant first failed because the old relative profile was imported;
with `UserDataPath` integrated it passes no-import/no-overwrite checks, installed
profile round trip, backup recovery, and deferred repair. All three variants pass.
`DidLastSaveSucceed()` remains the UI's save-error query; no API signatures changed.
