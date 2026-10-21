Mystic Onslaught

Mystic Onslaught is a top-down action roguelite built in C++ with raylib. The project combines real-time combat, class-based abilities, procedural dungeon rooms, enemy encounters, boss fights, pickups, relics, meta progression, a village hub, and custom content tools. Development targets native Windows builds; web support has been discontinued.

This is the largest project in this portfolio set and represents ongoing work toward a playable roguelite demo.

## Project Overview

Mystic Onslaught is designed around a dungeon-run loop. The player chooses a class, enters combat rooms, fights enemies, collects rewards, upgrades abilities, and pushes deeper into the dungeon. Between runs, the village acts as a home base where long-term progression, services, NPCs, and future customization systems can grow.

The current design direction is combat-first: the dungeon should test player skill, movement, ability use, and decision-making. Meta progression is intended to expand options rather than simply make the player overpowered.

## Core Design Pillars

- Combat should stay skillful and readable.
- Player classes should feel different mechanically, not only through damage numbers.
- Enemy pressure should come from roles, aggression, and room composition.
- Runs should feed long-term village growth.
- Meta progression should unlock choices and identity more than raw stat power.
- The village should feel like a compact home base, not a giant empty map.

## Features

- Top-down real-time combat
- Multiple player classes
- Class-specific starter abilities
- Ability slots and ability selection
- Enemy waves and encounter planning
- Boss encounters and support enemy behavior
- Procedural / authored room systems
- Room types such as standard, elite, rest, treasure, shop, and boss rooms
- Enemy roles, modifiers, and elite signatures
- Pickups for health, gold, and cells
- Relic and upgrade systems
- Damage numbers and combat feedback
- Cutscenes and dialogue
- Zeph shop / NPC interaction flow
- Poe / death and revival theming
- Meta progression through persistent cells and unlocks
- Village map and village asset metadata pipeline
- Settings and keybinding configuration
- Gamepad and touch-control support
- Virtual 1920x1080 canvas with letterboxing
- Audio, music, and SFX routing
- Debug tools and in-game editors
- Windows build support through Visual Studio

## Tech Stack

- C++
- raylib 5.5
- Visual Studio 2022 / MSVC v143
- PowerShell build, test and packaging scripts in `tools/`
- Custom text/metadata content files
- Custom editor tools built inside the game

## Gameplay

The player enters dungeon runs and clears rooms filled with enemies, hazards, pickups, and rewards. The game includes multiple enemy types and boss-style threats, including named classes such as:

- Skeleton Archer
- Slime enemies
- Flame Wisp
- Abyss Slime
- Pumpkin Jack
- Minotaur
- Werewolf
- Chomp Bug
- Osiris
- Titan Guard
- Toxic Vermin
- Ancient Bear
- Infernal
- Bonechill
- Stormclub
- Venomfang

Combat is supported by projectile systems, VFX, ability tuning files, character tuning files, collision, navigation, and room hazard systems.

## Player Classes

The project includes class identity work for:

- Warrior
- Rogue
- Paladin
- Hunter
- Mage
- Warlock

Each class is intended to feel distinct through its abilities and mechanics. Starter abilities are assigned by class, such as Warrior starting with a cleave-style attack and Mage starting with a fire bolt.

## Controls

Default keyboard bindings are stored in `TestGame/keybindings.cfg`.

| Action | Default |
|---|---|
| Move Up | W |
| Move Down | S |
| Move Left | A |
| Move Right | D |
| Dash | Space |
| Attack | Mouse button |
| Ability 1 | 1 |
| Ability 2 | 2 |
| Ability 3 | 3 |
| Ability 4 | 4 |
| Interact / Advance dialogue | E / Enter / Space depending on screen |
| Pause / Back | Escape |

The project also includes gamepad and touch-control support, including customizable touch layout behavior for web/mobile-style builds.

## Main Systems

### Engine and Game Loop

The application starts through `main.cpp`, creates an `Engine` instance, and runs the desktop loop.

The engine handles:

- Window setup
- Settings loading
- Audio initialization
- Texture and asset loading
- State transitions
- Virtual canvas rendering
- Input routing
- Gameplay updates
- Resource cleanup

### Combat System

Combat is built from several connected systems:

- `Character`
- `Enemy`
- `ProjectileSystem`
- `CombatDirector`
- `EncounterPlanner`
- `DamageNumberManager`
- `VFXManager`
- `AttackTuning`
- enemy-specific classes

Combat tuning is data-driven through text files such as `attacktuning_*.txt` and `charactertuning_*.txt`, making it easier to adjust enemy attacks, player abilities, and balance values without hardcoding every value directly into gameplay code.

### Dungeon and Rooms

The dungeon uses authored room files and room systems such as:

- `DungeonGen`
- `RoomLayout`
- `RoomBlueprint`
- `RoomLibrary`
- `RoomEditor`
- `RoomCollision`
- `RoomDirector`
- `RoomHazardDirector`
- `TileMapper`
- `TileRenderer`

Rooms are stored as `.mroom` files and grouped by biome folders such as Forest, Caverns, and Graveyard. This allows room content to be authored, saved, loaded, and reused by the run system.

### Village and Meta Progression

The village is planned as a compact hub where the player returns between runs. It is meant to support long-term identity, services, NPCs, and decoration.

Current village concepts include:

- Graveyard / death and respawn space
- Zeph's shop
- Bestiary or sanctuary space
- Villager houses
- NPC landmarks
- Player decoration space
- Village growth through run rewards

Technical support includes village map data, village layout data, `.vasset` metadata, colliders, markers, and runtime loading.

### Save-Like Persistent Data

The project uses configuration and progression files such as:

- `settings.cfg`
- `keybindings.cfg`
- `metaprogress.cfg`
- `run_save.cfg` (suspended-run checkpoint) and `run_history.cfg`
- `VillageLayoutSave.vlayout`

These files support settings, input customization, meta progression, suspended runs, and the village. The full game (Shipping) stores them in `%LOCALAPPDATA%\MysticOnslaught`; developer builds keep them in the working folder. Setting `MO_PROFILE_DIR` redirects every one of them to a sandbox folder, which the automated tests and the flow self-test use so they never touch a real profile.

### Tools and Editors

Mystic Onslaught includes several internal tools used to build and tune content:

- Room editor
- Tile mapper
- Attack editor
- Map editor
- Nine-slice editor
- Dialogue box designer
- Debug panel
- HUD/editor tuning screens
- Village asset adjustment tools

These tools are important because they allow gameplay content, room layouts, UI, and tuning values to be adjusted faster while developing.

## Project Structure

```text
TestGame/
├── TestGame.sln
├── TestGame/
│   ├── main.cpp
│   ├── Engine.cpp / Engine.h
│   ├── Character.cpp / Character.h
│   ├── Enemy.cpp / Enemy.h
│   ├── PlayerClass.cpp / PlayerClass.h
│   ├── DungeonGen.cpp / DungeonGen.h
│   ├── Room*.cpp / Room*.h
│   ├── Tile*.cpp / Tile*.h
│   ├── VFXManager.cpp / VFXManager.h
│   ├── DamageNumberManager.cpp / DamageNumberManager.h
│   ├── EncounterPlanner.cpp / EncounterPlanner.h
│   ├── MetaProgression.cpp / MetaProgression.h
│   ├── ShopManager.cpp / ShopManager.h
│   ├── Village*.cpp / Village*.h
│   ├── attacktuning_*.txt
│   ├── charactertuning_*.txt
│   ├── tilemapper_*.txt
│   └── Rooms/
├── Hero/
├── Enemy/
├── Bosses/
├── TileSet/
├── MapTilesets/
├── UI/
├── Sounds/
├── Music/
├── PowerUps/
├── VillageAssets/
└── tools/                 (supported native build / test / package scripts)
```

## How to Build and Run

Native Windows is the only supported platform.

### Requirements

- Visual Studio 2022 with the x64 C++ tools (MSVC v143)
- raylib 5.5, MSVC build (`include\raylib.h` and `lib\raylib.lib`)

The scripts find both automatically. Visual Studio is located with `vswhere`; raylib defaults to `C:\CLibraries\raylib-5.5_win64_msvc16`. To use other locations, set `MO_VS_INSTALL` / `MO_RAYLIB_DIR` or pass `-VsInstall` / `-RaylibDir`.

### Configurations

| Configuration | What it is |
|---|---|
| `Shipping` | Full game; no developer tools; saves to `%LOCALAPPDATA%\MysticOnslaught` |
| `PublicDemo` | itch.io demo build |
| `Release` | Developer build (debug panel, editors, flow self-test) |
| `Debug` | Developer build with debug symbols, no optimisation |

### Build

```text
powershell -ExecutionPolicy Bypass -File tools\build.ps1                          # Shipping
powershell -ExecutionPolicy Bypass -File tools\build.ps1 -Configuration Release   # developer build
```

Output goes to `x64\Build-<Configuration>\TestGame.exe`. Developer builds load assets from the `TestGame` folder, so run them with `TestGame` as the working directory. You can also open `TestGame.sln` and build any `x64` configuration in Visual Studio.

### Test

```text
powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1                       # every C++ suite
powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1 -Suite RunSave,RoomEditor
powershell -ExecutionPolicy Bypass -File tools\run_flow_selftest.ps1               # drives the real game end to end
```

### Release gate

```text
powershell -ExecutionPolicy Bypass -File tools\verify_native.ps1 -Flow
```

This builds Shipping, PublicDemo and Release, runs every suite, then runs the flow self-test. It exits non-zero if any step fails. Run it before packaging.

### Package

```text
powershell -ExecutionPolicy Bypass -File tools\package_shipping.ps1 -Build
powershell -ExecutionPolicy Bypass -File tools\smoke_shipping.ps1 -PackageDirectory x64\Packages\<folder>
```

The package goes into a new `x64\Packages\MysticOnslaught-<timestamp>` folder. Packaging refuses a build whose recorded source fingerprint no longer matches the source. It removes every player-data file (profile, run checkpoint and history, village, settings, bindings). `version.json` records the commit, whether there were uncommitted changes, and the source fingerprint.

### Discontinued builds

The web (Emscripten) build and the old `Makefile` were removed on 2026-10-18; web support is discontinued. Build with the `tools/` scripts or `TestGame.sln`.

## Testing and Validation

The project includes several C++ test files for core systems, including:

- Combat systems
- Dungeon generation
- Encounter planning
- Room blueprints
- Room capacity
- Room collision
- Room library
- Room editor
- Tile asset metadata
- Village layout data
- Class unlock rules
- Settings defaults
- Prologue controller
- Elite signatures

This reflects a practical QA mindset: as systems became more complex, tests were added around procedural content, room rules, balance systems, and progression logic.

`tools\run_tests.ps1` builds each suite into its own folder under `x64\TestRuns\`. Suites run with `assert` enabled, an isolated `LOCALAPPDATA` and `TEMP`, and any inherited `MO_PROFILE_DIR` removed, so no test can touch real progress. Every `*Tests.cpp` file must be registered in the runner, or the run fails before building. `tools\run_flow_selftest.ps1` complements the unit suites. It plays the real developer build through menu, village, dungeon, boss, death and run summary, victory, and save/resume, using a fresh sandbox profile each time.

## What I Learned

- Building a larger C++ game architecture with many interconnected systems
- Using raylib for real-time rendering, input, audio, and game loop development
- Managing custom game states and UI screens
- Creating gameplay systems around combat, enemies, abilities, pickups, and bosses
- Designing data-driven tuning files for attacks, characters, rooms, and tiles
- Building internal tools to speed up content creation
- Supporting desktop and web builds from one codebase
- Handling input across keyboard, mouse, gamepad, and touch
- Thinking about long-term roguelite progression and player motivation

## Known Limitations

- The project is still in active development.
- Some systems are experimental and may change as the core loop improves.
- The engine file is very large and could benefit from further refactoring.
- Some debug/editor tools are development-only and may not belong in public demo builds.
- Some content pipelines still rely on specific local folders and tool paths.

## Future Improvements

- Continue refactoring large systems into smaller focused modules
- Improve public demo build flow
- Add clearer player onboarding and tutorialization
- Expand village progression and services
- Add more enemy compositions and room variety
- Improve balancing so progression expands options without flattening combat difficulty
- Add more automated tests around combat, room generation, and progression
- Improve save/progression robustness
- Polish audio, visual feedback, and UI transitions
- Prepare a stable Steam or itch.io demo build

