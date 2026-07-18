# Dungeon Room Pacing And Capacity Design

## Goal

Replace flat room-to-room repetition with a controlled but randomized dungeon
rhythm. Combat population must respect the actual space available in each
handcrafted room and the player's growing ability toolkit.

## Room Capacity

`RoomCapacityAnalyzer` classifies the active `RoomLayout` as Small, Medium,
Large, or Arena. It measures walkable connected tiles, safe spawn tiles, and
chokepoints after accounting for solid tiles, fall zones, precise collision and
fall rectangles, and blocking props. The result supplies opening-body, total-
population, specialist, and pressure caps.

Rooms default to automatic analysis. `RoomBlueprint` stores an optional capacity
override for unusual handcrafted spaces, and the room editor cycles Auto, Small,
Medium, Large, and Arena. Existing room files load as Auto.

## Dungeon Rhythm

`DungeonGen` assigns special rooms using graph depth and branch role instead of
shuffling every eligible room. The boss remains furthest from the entrance. The
treasure room prefers a meaningful side-branch endpoint, while the elite room
prefers a deep room on the boss approach. Neither may appear beside the entrance.

Standard combat rooms receive an `EncounterProfile`: Skirmish, Assault, Swarm,
or Holdout. Early rooms teach with Skirmishes; middle rooms introduce Assault,
Swarm, and Holdout beats; late rooms emphasize Assault without stacking special
rooms together. Generation remains randomized inside these constraints.

## Combat Counts

Cooldown-friendly targets replace the current 8-26 population curve. Small
rooms permit about 3-4 opening and 5-7 total enemies; Medium rooms 4-6 opening
and 7-10 total; Large rooms 5-8 opening and 9-13 total; Arena/Swarm encounters
may reach 10 opening and 18 fragile enemies through waves.

Learned abilities may add at most two fragile bodies within the room's capacity.
Raw player DPS is never read. Hazards and specialists continue spending pressure,
so constrained rooms cannot become unfair merely by rolling expensive roles.

## Alternate Objective

Holdout rooms last 30 seconds. Enemies arrive in readable waves until time
expires; then surviving attackers withdraw and the room clears normally. A
visible objective timer distinguishes Holdout from elimination rooms. Re-entry
uses the room's existing persistent cleared state.

## Verification

Headless tests cover capacity classification and overrides, pacing placement,
profile variety, lower encounter caps, and ability-count bonuses. The complete
game must build in `Debug|x64`.
