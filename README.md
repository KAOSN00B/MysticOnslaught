Mystic Onslaught

Mystic Onslaught is a top-down action roguelite built in C++ with raylib. The project combines real-time combat, class-based abilities, procedural dungeon rooms, enemy encounters, boss fights, pickups, relics, meta progression, a village hub, custom content tools, and a web build pipeline.

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
- Web build support through Emscripten

## Tech Stack

- C++
- raylib 5.5
- Visual Studio 2022 / MSVC v143
- Emscripten for web builds
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

The application starts through `main.cpp`, creates an `Engine` instance, and runs either a desktop loop or an Emscripten web frame loop depending on the platform.

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
- `leaderboard.txt`

These files support settings, input customization, meta progression, and leaderboard-style data.

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
├── build_web.bat
├── build_raylib_web.bat
├── Makefile
└── web_build/
```

## How to Build and Run

### Windows / Visual Studio

Requirements:

- Visual Studio 2022
- MSVC v143 toolset
- raylib 5.5 installed at:

```text
C:\CLibraries\raylib-5.5_win64_msvc16
```

Steps:

1. Open `TestGame.sln`.
2. Select `Debug|x64`, `Release|x64`, or `PublicDemo|x64`.
3. Build the solution.
4. Run the project.

### Web Build

The project includes a web build script:

```text
build_web.bat
```

The script expects:

- Emscripten installed at `C:\Users\rober\emsdk`
- raylib web build available at `C:\CLibraries\raylib-src\src\libraylib.a`

To build for web:

1. Run `build_raylib_web.bat` if raylib has not already been compiled for web.
2. Run `build_web.bat`.
3. The output is generated in `web_build/`.
4. Zip the contents of `web_build/` for itch.io upload.

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

