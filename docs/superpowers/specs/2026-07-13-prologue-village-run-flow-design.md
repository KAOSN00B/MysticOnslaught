# Mystic Onslaught Prologue, Story, and Village Run Flow

**Date:** 2026-07-13  
**Status:** Approved design

## Purpose

Mystic Onslaught needs a short playable tutorial, a clear story premise, and a compact village that anchors the roguelite loop. The opening should teach combat through play, end with a memorable forced death, introduce Poe naturally, and then hand control back quickly. The village should be a consistent sanctuary rather than a large randomized construction field.

This design also changes how a run interacts with the village. Returning after a boss pauses the current run and preserves the world map. Continuing immediately keeps the player in danger without healing or village services.

## Core Story Premise

Poe is a Grim Reaper. The Onslaught has caused so many monster attacks and civilian deaths that his workload has become unmanageable. He chooses the player opportunistically after seeing that they survived longer than expected. The player is not a prophesied hero and Poe was not waiting for them.

Poe resurrects the player in exchange for employment: the player destroys the monsters causing the deaths, while Poe handles the dead. Poe can be seen, heard, and interacted with only by the player because the player has died and been returned. Zeph and all other living villagers are completely unaware that Poe exists.

Poe's first dialogue should be brief, dry, and practical. The final script may be polished later, but it should preserve this intent:

> "Hm. You're stronger than you look. Could've been stronger, with proper guidance."  
> "Guess you'll do."  
> "I can bring you back. In exchange, you're going to work for me."

Later Poe dialogue can use the fact that villagers see the player speaking toward an apparently empty graveyard. This should remain occasional world texture, not a repeated joke.

## Starting Classes and Unlocks

A new save begins with class selection before the tutorial.

- Warrior, Hunter, and Mage are available immediately.
- Rogue is locked behind a future challenge milestone.
- Warlock is locked behind a future story/challenge milestone involving forbidden magic, curses, or monster research.
- Paladin unlocks after completing the game.

The locked classes should remain visible on class select with readable unlock requirements. This creates meaningful meta progression through new playstyles rather than permanent stat inflation. The exact Rogue and Warlock challenges are outside this implementation phase and will be designed with the broader village progression.

## One-Time Forest Prologue

The prologue runs only once per save. Until persistent saving exists, its completion flag lasts for the current game session. A future save system will persist this flag.

The prologue uses a fixed three-room horizontal Forest layout:

```text
[ Tutorial Room 1 ] - [ Tutorial Room 2 ] - [ Last Stand ]
```

The rooms use Forest art but are authored tutorial rooms, not procedurally generated dungeon rooms. The prologue is separate from the player's first real Forest run.

### Tutorial Health

The player uses a simplified three-HP tutorial profile.

- Every tutorial enemy hit removes exactly one HP.
- Armour, healing pickups, stat progression, gold, cells, XP, and random rewards are disabled.
- Reaching zero HP in rooms one or two immediately restores all three HP and keeps the tutorial running.
- The third hit in the last stand triggers the scripted first-death sequence instead of normal game over.

The first two rooms are instructional and cannot fail. The last room is a story event and cannot be won.

### Room One: Basic Combat

The player spawns with the chosen starter class. Two basic melee grunts appear and the exits close.

A device-aware prompt displays the player's actual bound basic-attack control, such as `Press X to attack`, mouse input, keyboard input, or the active controller equivalent. Pressing the correct input satisfies the instruction. The room opens only after both grunts are defeated.

Movement is learned naturally while approaching and avoiding these enemies. The prompt system must read the existing input bindings rather than hardcode controller or keyboard labels.

### Room Two: Class Ability and Dash

Only basic grunts appear. The prompt introduces the selected class and its equipped first ability using real runtime names and bindings:

```text
You are a skilled [CLASS]. Press [BOUND INPUT] to use [ABILITY NAME].
```

Pressing the correct ability input satisfies that instruction. A later telegraphed grunt attack introduces the bound dash control. After the required inputs have been used and all grunts are defeated, the forward exit opens.

No tutorial card should interrupt the action. Prompts appear contextually, disappear after the correct input, and use the game's current keyboard, mouse, touch, or controller prompt mode.

### Room Three: Last Stand

The room seals all exits and spawns ranged enemies in a broad ring around the player. They continuously fire toward the player from every direction.

- Defeated ranged enemies are immediately replaced at valid ring positions.
- If the player clears the full formation, the formation respawns.
- The encounter allocates from a stable pool or capped collection rather than creating unlimited enemies and projectiles.
- No enemies, shots, kills, or survival time grant rewards.
- The first two hits visibly remove HP.
- The third hit starts the first-death sequence.

The player should die quickly. This is not a survival challenge or a hidden skill check. The ring exists to demonstrate that the Onslaught is dangerous because of relentless numbers, not because the player encountered one invincible villain.

### First Death and Resurrection

On the third hit, gameplay freezes briefly. The camera holds on the player's body while combat audio falls away. Remaining monsters fade, retreat, or are visually suppressed so Poe's entrance is readable.

Poe appears beside the dead player, delivers his short introduction, and offers resurrection in exchange for work. A purple transition carries the player to Poe's village graveyard. This bypasses the normal game-over and ordinary death-respawn path.

Every later death uses the shorter existing Poe resurrection flow with rotating dialogue. It never replays the Forest prologue or Poe's first introduction.

## Debug Skip

A debug-only `Skip Onboarding` action must bypass all tutorial-style content:

- class selection still determines the test character unless a debug default is explicitly chosen;
- skip the three Forest rooms;
- skip the scripted first death;
- skip Poe's first resurrection dialogue;
- skip Zeph's first introduction;
- mark onboarding complete for the current session;
- place the player in the ready-state village with the north world-map exit active.

The final binding must be checked against existing debug controls before assignment. The UI should state the binding visibly during onboarding so repeated testing does not require remembering an undocumented key.

## Fixed Premade Village

The village is a fixed, authored space. It is not randomized or procedurally generated.

The initial village uses the same dimensions, camera framing, Forest ground, and Forest borders as a standard dungeon starting room. Every player sees the same layout. The first implementation contains only the required landmarks and enough open space for later residents and decorations.

- Poe's Graveyard is placed on the western side.
- Zeph's Shop is placed on the eastern side.
- A broad, uncluttered central path separates the landmarks.
- A north-centre path/exit opens the world map.
- Poe and the player respawn at the graveyard's authored markers.
- Zeph stands at his shop's authored marker.
- All asset collision uses the existing `.vasset` collider data.

The placement layout is data-driven only so the developer can reposition authored buildings later without recompiling gameplay logic. Data-driven does not mean randomized. The runtime must load one fixed village layout file with stable asset IDs and world positions.

Service buildings are not player-buildable in this phase. The old requirement to build Zeph's shop is removed. A future focused layout tool may allow the developer to reposition all village buildings. A later player-facing decoration system may allow optional trees, flowers, mushrooms, benches, signs, and fences without moving required services.

## Poe in the Village

Poe remains visibly present at the graveyard for the player. Living NPCs must never target him, greet him, avoid him, path toward him, include him in schedules, or react to his dialogue. He is absent from their simulation and perception even though he is rendered on the player's screen.

Poe does not publicly govern the village. Villagers believe the graveyard is ordinary and that the player appeared there under unusual circumstances.

## Zeph in the Village

Zeph does not know Poe and cannot perceive him. Zeph is an ordinary merchant living in a sanctuary from the monster attacks.

During the first non-skipped village visit, a light objective guides the player from the graveyard to Zeph. Zeph introduces the sanctuary and his role without lengthy exposition. His intended tone is:

> "Oh. You must be new here."  
> "You'll find sanctuary here, at least for now. The monsters tend to keep their distance."  
> "If you've got coin, I can sell you supplies. Might even teach you a few abilities..."  
> "For the right price, of course."

After the introduction, Zeph becomes the normal village shop interaction. Zeph is removed from mandatory biome entrance rooms and should not appear elsewhere in the dungeon. Returning to the village is the player's opportunity to use his services.

## First Village Onboarding

The first village visit is lightly guided but remains playable:

1. Poe completes the resurrection dialogue at the graveyard.
2. Control returns and a small objective points toward Zeph.
3. Approaching or interacting with Zeph starts his first introduction.
4. When the introduction ends, the objective becomes `Head north when you are ready`.
5. Walking through the north exit opens the world map.

The player may walk around freely between these steps. Full-screen tutorial cards are not used.

## World Progression

The first real biome is always Forest, replacing Caverns as the opening biome. Only Forest is available for the player's first world-map selection after onboarding.

The prologue does not count as clearing Forest. The player still completes a normal Forest dungeon with rewards, progression, hazards, and its boss.

After the Forest boss is defeated, subsequent world-map offerings may include any other eligible biome, including Caverns. The world map may vary; the village may not.

## Boss Choice and Persistent Runs

The existing post-boss choice remains:

- **Return to Village:** pause the active run, fully restore health and mana, and allow village services.
- **Continue:** enter the next biome without restoring health or mana and preserve the double-or-nothing reward benefit.

Returning to the village must not reset or abandon the run. The following data remains intact:

- world-map position;
- cleared and available biome nodes;
- selected class and appearance;
- known abilities and ability upgrades;
- relics and run modifiers;
- gold, cells, and carried resources;
- boss streak and double-or-nothing state;
- all other build-defining runtime state.

Walking north from the village reopens the same world map at the same position. It does not generate a replacement map or restart the route.

## Run Session Architecture

Introduce a focused `RunSession` owner for state that must survive gameplay, village, and world-map transitions. Existing systems may continue to own complex runtime objects initially, but `RunSession` should become the clear authority for progression and restoration data.

The village pauses a live `RunSession`; it does not reconstruct a new run. This is the preferred first implementation because it is reliable now and provides a clean path to serialization later.

Future saving should be allowed only in approved safe states:

- the village;
- designated safe rooms;
- cleared rooms with no active combat or pending transition.

Save-and-quit implementation is outside this phase, but new state must avoid designs that make safe-state serialization impossible.

## Empty Biome Entrance Rooms

Every newly entered biome begins in a quiet transition room.

- No Zeph.
- No Poe.
- No wager interaction or red wager orb.
- No shop panel or forced dialogue.
- No combat before the player moves forward.
- The forward exit leads into the first real room.
- After the player advances, the entrance closes behind them.

The wider wager system and red orb can be removed in a separate cleanup once references and save implications are audited. This phase removes their presence from biome entrances and prevents them from blocking progression.

## Technical Components

### Prologue Controller

Owns prologue room progression, contextual prompt state, tutorial health overrides, enemy formations, scripted death, first Poe dialogue, completion state, and debug skipping. It must not overload ordinary dungeon room-clear rules with tutorial-only exceptions.

### Run Session

Owns the active route and state that survives a village visit. It provides explicit `PauseForVillage`, `ResumeFromVillage`, and future serialization boundaries rather than relying on scattered reset flags.

### Fixed Village Layout Data

Stores stable village asset IDs and fixed world placements. Runtime uses each placed asset's `.vasset` data for image, collider, marker, and interaction transforms. Missing or malformed placement data falls back to known safe positions for the graveyard and shop and logs a clear error.

### Village NPC Perception

Future villager routines operate on living NPCs and village landmarks. Poe is not registered as a living NPC, citizen, schedule target, or obstacle. Only the player interaction layer exposes him.

## Safety and Recovery

- Missing tutorial enemies must not permanently lock a door; objective completion has a debug timeout/recovery path.
- Missing input labels fall back to readable action names rather than blank prompts.
- Missing graveyard metadata falls back to the current known graveyard placement, Poe marker, respawn marker, and colliders.
- Missing shop metadata falls back to a safe shop placement and keeps Zeph interaction reachable.
- Missing village layout data loads the fixed fallback layout rather than an empty field.
- Debug skipping is idempotent and safe from any prologue room or first-time dialogue state.
- Returning to the village cannot clear or regenerate the active world map.

## Test Plan

### Automated Tests

- Prologue room order is exactly basic combat, class ability/dash, last stand.
- Rooms one and two restore three HP after lethal damage.
- The last stand triggers first death on the third hit.
- Last-stand kills grant no rewards and replacements remain capped.
- Correct active-device bindings populate tutorial prompts.
- Prologue completion prevents replay in the same session and, later, across saves.
- Debug skip reaches the ready village and marks every first-time step complete.
- Starting class availability is Warrior, Hunter, and Mage only.
- Fixed village layout always returns the same graveyard, shop, and north-exit positions.
- `.vasset` collider and marker transforms match rendered asset scale and position.
- Poe is absent from villager perception and scheduling collections.
- First real biome is Forest.
- Post-Forest biome offerings may include Caverns and all other eligible biomes.
- Returning to village preserves world-map identity and run build.
- Returning restores health and mana.
- Continuing preserves current health and mana.
- Empty biome entrances contain no Poe, Zeph, wager orb, or combat encounter.

### Manual Verification

- Complete onboarding with keyboard/mouse and controller prompts.
- Intentionally die repeatedly in tutorial rooms one and two.
- Clear the last-stand formation with debug damage and confirm it respawns without rewards.
- Use Skip Onboarding from each tutorial room and first dialogue state.
- Confirm the graveyard and shop are visually separated with a clear central route.
- Confirm Poe and Zeph render at authored markers and both interactions open correctly.
- Return to the village after the Forest boss, shop, then resume the unchanged world map.
- Continue after a boss and verify no healing occurs.
- Enter multiple later biomes and confirm every entrance room is empty and closes behind the player.

## Out of Scope

- Final Rogue and Warlock unlock challenge designs.
- Full save-and-quit implementation.
- Player placement of service buildings.
- A general-purpose village map editor.
- Randomized village layouts.
- Full villager schedules and homes.
- Player decoration placement.
- Complete wager-system deletion beyond removing it from biome entrances.
- Dedicated village music and per-biome music tracks.
