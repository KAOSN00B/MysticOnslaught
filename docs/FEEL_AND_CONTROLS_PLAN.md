# Feel and controls plan (2026-10-18)

Status: proposal for Robert to approve before more code changes. Nothing in this document is implemented yet.

## 1. What is wrong now (with evidence)

**Movement feels too fast.**
- Class move speeds are unchanged (350–435 world px/s, `PlayerClass.cpp` kClassTable), but the player sprite was reduced from 6× to 4× (`Character.cpp` `kPlayerDrawScale`). The same speed now crosses about 1.5× more of the body each second.
- Dash is 1500 px/s for 0.18 s, about 270 px (`Character.h` ~792–793). At the 4× body that is about 2 body heights, which feels long for a short dodge.
- Root cause: speeds and distances are in world pixels, but the body is drawn at a different size. Nothing ties them together.

**Enemies feel too big.**
- Grunts default to 6× (`Enemy.cpp:149`), which is 192 px tall. The player is 128 px. Bosses use 4.8–7.0× (`*.h` `_bossScale`).
- Enemy scale also sets their hitbox (`Enemy.cpp:269`), so changing it changes how hard they are to dodge.

**Aim line.**
- For a mouse, the cursor already shows the target. The line adds noise.
- For a stick, a line or a reticle is needed, because there is no cursor.

**Other problems I introduced and have not finished:**
- The player hitbox was authored for the old 5.02× art and does not follow the draw scale (`charactertuning_Player.txt`).
- Abilities: the hold-and-release rule and the eight-way melee swing are built, but not yet checked in play.

## 2. Design targets (what the game should feel like)

Reference points: Hades (fast, readable, the hero is small and the room is big) and Diablo (the cursor is the aim, abilities show their area only while you are choosing where to cast).

Proposed rules:

1. **Everything is measured in body heights.** The player's body height is the unit. Movement speed, dash distance, attack reach and enemy size are multiples of it. Changing the sprite size then keeps the feel.
2. **The player is the smallest hero on screen; ordinary enemies are about the player's size; bosses are 2–3× the player.** Elites sit between.
3. **Movement: fast enough to cross a room in a few seconds, slow enough to read enemy attacks.** Target numbers are set in the simulator, not guessed.
4. **Dash: short and sharp.** A fixed fraction of a body height, with i-frames that match the dash.
5. **Aim:**
   - Mouse: the cursor is the aim. No line for basics. Abilities show their area only while the button is held (already built).
   - Stick: a small reticle at a fixed distance in the stick direction. No long line.
   - Keyboard only: the walk direction, eight ways. No aim indicator.
6. **Abilities:** a quick tap fires at the aim; holding shows the area; releasing fires there.
7. **Melee basics follow the walk direction in eight directions.** The swing box follows that direction.
8. **Ranged basics follow the aim** (mouse free, stick eight ways, walk direction with no aim input).

## 3. The plan, in order

**Step 1: make the units consistent (no new features).**
- Add one constant: body height of the player, in world px, taken from the draw scale.
- Express move speed, dash distance and enemy draw size as multiples of it.
- Set the current numbers so the feel is unchanged for the Warrior and Mage, then compare the other classes.
- Gate: flow self-test passes; speed in body heights is the same before and after for each class.

**Step 2: set enemy and boss sizes.**
- Regular enemies: about the player's size (scale 4 for grunts). Elites a little larger. Bosses 2–3× the player.
- Re-check hitboxes after the size change (the hitbox follows the enemy scale).
- Gate: screenshots of each enemy next to the player.

**Step 3: fix the aim visuals.**
- Mouse: no aim line for basics. The ability area appears only while held.
- Stick: small reticle at a fixed distance. No long line.
- Remove the arrowhead unless you want it on the stick reticle.
- Gate: screenshots for mouse, stick and keyboard-only.

**Step 4: verify the controls in play.**
- Quick taps fire on release; holds show the area; melee swings hit above, below and to the sides.
- Ranged basics go where the mouse points.
- Keyboard-only players walk and attack in eight directions.
- Gate: a checklist played on a keyboard and on a controller.

**Step 5: menus, village and navigation.**
- One shared scrolling background for every menu with a unique colour per screen. The pause menu keeps its dim overlay on the frozen game.
- Navigation: add clearance (agent radius) to the grid; add random reachable points in a radius; village citizens use the same grid, A* and a 1–5 second think timer.
- Gate: screenshots of each menu; citizens do not get stuck in a test walk.

**Step 6: rooms.**
- Use the new warnings (`ROOM:` and `DUNGEON:`) to find which rooms fall back to random generation, then decide what to do about them.

## 4. Decisions (Robert, 2026-10-18)

1. **Sizes:** everyone similar in size. Bosses stay big and daunting; bosses are good as they are.
2. **Mouse basic-attack line:** still unsure. Recommendation: remove it. The reticle (item 3) shows aim for everyone, so the line is not needed.
3. **Reticle:** yes for mouse and controller. It is a small reticle that circles the player at a short fixed radius, not a line that reaches across the screen.
4. **Class speeds:** yes, classes differ in speed (for example the Warrior heavy, the Rogue light).
5. **Keyboard-only players:** one small reticle around the player for everyone, including keyboard-only. It shows the direction the basic attack will go.

## 4a. Navigation (new)

- **Villagers and enemies share one navigation system.** The grid we already have (`NavigationGrid`) plays the role of a navmesh: walkable cells, with blocked cells built from obstacles.
- **Clearance (the gap):** agents keep a gap from walls and props. Blocked cells are grown by the agent's radius before pathing, so a path never runs along a wall edge. This is the Unity agent-radius and Unreal nav-agent-radius idea.
- **Random reachable points:** pick a random walkable cell within a radius of the current spot, like Unreal's "Get Random Reachable Point In Radius".
- **Think timer:** each villager waits a random 1–5 seconds, then picks a reachable point and walks there with A*.
- **Village grid:** built from the village colliders when the village loads, and rebuilt when buildings move.

## 5. Process change

- One step per gate. No more mixed changes across speed, aim and enemies in one go.
- Each step gets a short feel checklist that Robert plays before the next step.
- Numbers for speed, dash and enemy size live in one table, so tuning is a single edit.

## 6. Village and enemy notes (Robert, 2026-10-18)

- **Enemies always go for the player.** Random points are for villagers only. Enemies keep chasing the player through the navigation grid.
- **Villagers wander to random reachable points** (see 4a). Some also idle, or stand face to face to look like they are talking. The code already has a chat partner and chat cooldown for villagers (`UpdateVillageCitizens`); check that it actually triggers before adding new behaviour.
- **Building doors:** buildings show a window where a door should be. `VILLAGE_METADATA_MODEL.md` already defines a `door` line (position, size, target, prompt) that no gameplay reads yet. Plan: draw a door from that data and keep the building art as it is. The door works even when the building can't be entered yet.
- **Art decision needed:** if the door should be painted into the building art, that is an art edit (Robert or an artist). The data-drawn door needs no art change.
