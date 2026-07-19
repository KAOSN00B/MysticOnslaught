# Mystic Onslaught Elite Review and Boss Encounter Pattern Design

Date: 2026-07-18
Status: Approved design direction and Claude implementation handoff
Approved encounter model: Hybrid authored phases

## Purpose

This document reviews the mini-boss elite implementation currently in the
codebase and defines how its strongest ideas should be extended into every full
boss. The intended result is not simply more projectiles or faster attacks.
Bosses should become readable, learnable encounters with deliberate survival
sequences, positioning tests, safe gaps, counterplay, and punish windows.

The approved structure is hybrid. Each boss phase contains one memorable,
authored survival set piece and a small deck of ordinary attacks. The set piece
is predictable enough to learn and master. The ordinary deck adds controlled
variation without allowing immediate repetitions or nonsensical combinations.
This preserves replayability while giving every boss a recognizable identity.

This is a design and implementation handoff. It does not authorize unrelated
changes to village, map editor, class balance, procedural room generation, or
other systems. Existing user and Claude work must be preserved.

## Current Verification

The following verification was completed before writing this document:

- The elite implementation commits were reviewed from `8c045e4` through
  `a68ac96`.
- `EliteSignatureTests.exe` completed successfully.
- A clean `Debug|x64` rebuild completed successfully.
- The final executable was produced at `x64/Debug/TestGame.exe`.
- The build currently contains pre-existing numeric conversion warnings and one
  runtime-library linker warning, but no build errors.
- No full manual gameplay pass has been completed. Every elite and boss still
  requires in-game testing.

## Executive Assessment

Claude chose the right broad architecture for the elites. Elite classes own
their movement and decision states, emit small value events, and leave damage,
status, sound, VFX, and screen shake to `CombatDirector`. The event queues and
live zone storage are bounded. Modifier compatibility is centralized. Guard
Links reduces damage instead of granting unexplained immunity. These are all
strong foundations for larger boss encounters.

The elite pass is not ready to be copied directly into bosses, however. Several
requirements in the elite specification are either incomplete or disconnected
from runtime behavior. Pool resets are unsafe, locked movement does not validate
room geometry, an old HUD table mislabels modifiers, the editor is only partly
authoritative, and the shared action clock is unused and cannot correctly cross
multiple stages during one large frame. These problems should be corrected
before building ten larger encounter scripts on top of them.

The full bosses are further along than they may appear. All ten already have
individual states, phase thresholds, attack timers, and class-specific behavior.
They do not need to be deleted and rewritten. Their existing moves should become
execution tools beneath a new encounter-pattern layer that decides when a
survival sequence starts, which safe geometry it uses, how it escalates, and
when the player receives a punish window.

## Elite Code Review Findings

### 1. Pooled Elite Reset Is Incomplete

`Infernal`, `Bonechill`, `Stormclub`, `Venomfang`, and `Ogre` each implement a
custom `ResetForSpawn()` and manually reset selected fields. They do not call the
shared `Enemy::ResetForSpawn()` path. The shared path resets considerably more
runtime data than the derived implementations, including statuses, pit-fall
state, revive state, elite event queues, sequence counters, dropped-event
telemetry, Guard Link state, signature counters, phase state, and other pooled
lifetime data.

Because these enemies are reused through `SpawnPooledType`, a dead or inactive
instance can carry data from its previous life. Room cleanup masks some of this
for the active elite pointer, but it does not make each pooled object
self-contained and safe. Debug spawning, ordinary non-elite uses of these enemy
types, interrupted room flows, and future encounter reuse can expose stale
statuses or telemetry.

Required correction:

- Introduce one protected shared reset routine for all `Enemy` subclasses, or
  call `Enemy::ResetForSpawn(pos)` first and then apply the subclass profile.
- Do not duplicate the shared field list in five classes.
- Add a pool-reuse test that dirties statuses, elite queues, phase, pit state,
  revive state, and telemetry before calling each derived reset.
- Confirm a reused elite begins with no poison, bleed, slow, vulnerability,
  mark, curse, pending event, dropped-event count, or active Guard Link.

Relevant code:

- `TestGame/Enemy.cpp`, `Enemy::ResetForSpawn`
- `TestGame/Ogre.cpp`, `Ogre::ResetForSpawn`
- `TestGame/Infernal.cpp`, `Infernal::ResetForSpawn`
- `TestGame/Bonechill.cpp`, `Bonechill::ResetForSpawn`
- `TestGame/Stormclub.cpp`, `Stormclub::ResetForSpawn`
- `TestGame/Venomfang.cpp`, `Venomfang::ResetForSpawn`

### 2. Locked Movement Does Not Respect Authored Room Geometry

Stormclub and Venomfang lock a world-space target and then repeatedly move
toward it. The target is not validated against walls, prop collision, fall
rectangles, door safety zones, or navigable ground. The engine collision pass
may push the elite sideways or backward while the signature state continues
toward the original target. The resulting movement can bend away from the
telegraph, stall along a collider, or resolve an impact somewhere different
from the warned path.

This violates the core fairness rule: what was warned must be what hits. It is
also the same family of failure that previously caused leap and teleport
problems for Abyss Slime. Bosses will magnify this issue because they use longer
travel distances and more complex handcrafted rooms.

Required correction:

- Add a shared locked-path query supplied by the runtime context.
- Clamp requested endpoints to the last valid point along the warned segment.
- Reject points inside walls, solid props, fall rectangles, closed door zones,
  and outside room bounds.
- Preserve the locked direction after commitment.
- If collision occurs during committed travel, end at the validated contact
  point and enter the authored recovery rather than steering around the wall.
- Add tests for open floor, wall interception, prop interception, fall terrain,
  room edge, and a target that becomes invalid after the telegraph starts.

Relevant code:

- `TestGame/Stormclub.cpp`, `LeapTelegraph` and `Leaping`
- `TestGame/Venomfang.cpp`, `PounceTelegraph` and `Pouncing`
- `TestGame/Engine.cpp`, dungeon enemy collision resolution
- `TestGame/RoomCollision.cpp`

### 3. Old Elite Modifier Labels Remain In The Dungeon HUD

The current room intro and room badge still contain an old five-entry mechanic
table. It displays Guard Links as `Bodyguard Shield` and displays Arena Pressure
as `Leaping Brute`. The new modifier enum only contains Cage, Guard Links,
Enrage, and Arena Pressure. The stale table teaches rules that no longer exist.

Required correction:

- Remove the local five-entry string arrays from `Engine.cpp`.
- Use one shared label and description function keyed by `EliteModifier`.
- Make the entrance banner, room badge, debug panel, telemetry, and any future
  codex entry use the same source.
- Never index the modifier array with a hardcoded count of five.

Relevant code:

- `TestGame/Engine.cpp`, `kEliteMechanicNames` and
  `kEliteMechanicShort`
- `TestGame/DebugPanel.cpp`
- `TestGame/EliteSignature.h`

### 4. Elite Attack Editor Values Are Only Partly Authoritative

The Attack Editor saves elite timing, hitbox, FX, and position information into
the shared attack-tuning record. Runtime elite code reads selected timing fields,
but most damage geometry, status payloads, movement speed, VFX scale, lane
length, lane width, patch radius, and impact radius remain hardcoded in
`CombatDirector` or the elite class. This can make the editor appear to work
while gameplay remains unchanged.

Required correction:

- Define exactly which tuning fields are supported by each signature move.
- Load one resolved tuning snapshot when a signature begins.
- Use that snapshot for both the warning preview and the damaging zone.
- The runtime and editor must share the same default geometry functions.
- Hide or disable controls that a move does not support.
- Add a debug overlay comparing the warned geometry, resolved hit geometry,
  and current tuning key.

Relevant code:

- `TestGame/AttackEditor.cpp`
- `TestGame/AttackTuning.h`
- `TestGame/AttackTuning.cpp`
- `TestGame/CombatDirector.cpp`, `SpawnEliteZonesForEvent`

### 5. EliteActionClock Is Unused And Mishandles Very Large Frames

`EliteActionClock` describes Telegraph, Active, Recovery, and Ready stages, but
none of the five elite implementations instantiate it. Each elite currently
uses its own enum and timer. That is acceptable for bespoke movement, but the
header comments currently imply the clock drives the live elite encounters.

The clock also advances at most one stage per call. It carries overshoot into a
negative remaining value but does not continue advancing if that overshoot also
crosses the following stage. A severe frame hitch can therefore leave the clock
inside an already-expired intermediate stage until another update. A boss
pattern system cannot rely on this behavior because a missed transition can
desynchronize warning, damage, VFX, and recovery.

Required correction:

- Either remove the unused clock and document that patterns own explicit state,
  or upgrade it into the shared sequence primitive used by the boss system.
- If retained, process all crossed boundaries in a bounded loop.
- Return every crossed stage transition, not only a boolean.
- Prevent zero-duration stages from creating infinite loops.
- Add tests where one update crosses two stages and the entire action.

Relevant code:

- `TestGame/EliteSignature.cpp`, `EliteActionClock::Update`
- `TestGame/EliteSignatureTests.cpp`

### 6. Arena Pressure Is Generic And Ignores The Environmental Budget

Arena Pressure currently fires `LavaBallProjectile` volleys regardless of elite
archetype. It does not produce the promised fire, ice, lightning, poison, or
physical identities. It also does not use the environmental projectile cap
already present in `GameBalance.h`.

Required correction:

- Route Arena Pressure through the same bounded encounter-pattern projectile
  service intended for bosses.
- Select visuals and statuses by elite archetype.
- Count active environmental shots separately from enemy-owned projectiles.
- Delay or reduce a volley when the environmental pressure budget is full.
- Keep entry space and door lanes clear.

Relevant code:

- `TestGame/CombatDirector.cpp`, `UpdateEliteMechanics`
- `TestGame/GameBalance.h`, environmental projectile limits

### 7. Guard Link Reduction Feedback Is Not Fully Connected

Damage reduction correctly guarantees at least one point of damage, but the
`_eliteGuardReducedHit` flag is set and never consumed by the runtime. The
specification calls for visible links and a clear reduced-hit response so the
player understands that attacks are working but inefficient.

Required correction:

- Consume the reduced-hit flag at the centralized damage-number call site.
- Show the real post-reduction value with a restrained `GUARDED` or broken-chain
  indicator, not `IMMUNE`.
- Draw visible link beams from living guards to the elite.
- Break links immediately when the last valid guard dies.

Relevant code:

- `TestGame/Enemy.h`, `ConsumeGuardReducedFlag`
- `TestGame/Enemy.cpp`, Guard Links damage path
- `TestGame/Ogre.cpp`, custom Guard Links damage path

### 8. Tests Cover Helpers, Not The Five Live Signatures

The current standalone suite validates queue order, modifier compatibility,
damage-reduction helpers, spread geometry, the phase helper, and selected clock
behavior. It does not instantiate the five elite classes or test their actual
state transitions, target locking, blocked movement, interrupt behavior,
recovery, room exit, or pooled reuse.

Required correction:

- Keep the fast pure-helper tests.
- Add deterministic state-machine tests with fake targets and collision queries.
- Test every elite from idle through telegraph, lock, execution, and recovery.
- Test phase two exactly once.
- Test death and room cleanup during every active signature state.
- Test an interrupted telegraph and an uninterrupted committed movement.

## Boss Foundation Findings

### Boss Detection Must Use IsBoss

`CombatDirector::IsBossFightActive()` currently recognizes only Molarbeast by
checking `AsMolarbeast()`. All ten boss classes already override `IsBoss()`.
This helper can therefore mis-handle support respawning, ordinary enemy reward
suppression, and other boss-fight rules for every other boss.

Required correction:

- Return true when any active, living, non-dying enemy reports `IsBoss()`.
- Add a test for every boss class or at minimum a fake boss implementation.
- Audit every call to `IsBossFightActive()` after correcting it.

### Boss Damage Denials Must Be Centralized And Explained

Several bosses own custom `TakeDamage()` implementations. Werewolf and Abyss
Slime silently return while airborne. Osiris silently returns during teleport.
Other bosses apply their own reductions. These paths bypass portions of
`Enemy::TakeDamage`, including standardized block reasons and shared protection
logic. This is a likely contributor to the historical feeling that bosses
randomly become invincible.

Required correction:

- Add a shared boss damage-resolution hook that returns an explicit outcome:
  Applied, Reduced, Guarded, Dodged, Untargetable, or Immune.
- Keep class-specific reduction calculations in the boss class, but route the
  final result through the common damage pipeline.
- If a boss is genuinely untargetable, its sprite and telegraph must make that
  state unmistakable and damage numbers must say why.
- Prefer reduced damage or positional counterplay over immunity.
- Do not use unexplained health-phase invulnerability.

## Approved Hybrid Encounter Model

Every boss has three health phases using the existing thresholds near 66% and
33%. Each phase contains four layers:

1. An entry beat that announces the phase without stopping combat for long.
2. One authored survival set piece that defines the phase.
3. A small weighted deck of ordinary attack cards.
4. A deliberate recovery or punish window after the set piece.

The survival set piece is deterministic enough to learn. It may rotate, mirror,
or choose one of a few safe-lane variants, but it must not become arbitrary.
The ordinary deck provides replay variation. The deck should prevent immediate
repeats, enforce cooldown groups, consider player distance, and avoid combining
attacks that remove all safe movement.

The boss should remain damageable unless the fiction and presentation clearly
require a brief untargetable movement state. Survival phases do not need a
hidden invulnerability switch. The boss may move to a protected position,
receive modest damage reduction, or become difficult to reach, but successful
attacks should normally still advance the fight.

## Recommended Architecture

### EncounterPatternDirector

Create a generalized encounter-pattern layer rather than expanding
`EliteArchetype` until it contains every boss. Suggested ownership:

- Boss classes own their animation, locomotion, collision body, and existing
  move execution.
- `EncounterPatternDirector` owns authored phase scripts and attack-card order.
- `CombatDirector` continues to own attack-zone resolution, projectiles, status,
  VFX, audio, hit feedback, and pressure budgets.
- `AttackTuningStore` supplies resolved move and pattern values.
- `Engine` supplies room collision queries, safe spawn queries, and rendering
  callbacks without containing boss-specific scripts.

The existing `EliteSignatureEvent` concepts should be generalized into neutral
encounter events. Do not make full bosses pretend to be elites. A gradual path
is acceptable: keep the elite names temporarily behind aliases while moving
shared shapes and event payloads into `EncounterPattern.h`.

### Pattern Events

The event vocabulary should support:

- Telegraph zone
- Lock point or direction
- Execute zone or projectile wave
- Move actor along a validated path
- Spawn temporary hazard
- Spawn or command adds
- Change arena rule
- Begin punish window
- End punish window
- Phase callout
- Pattern complete

Each event is a value object. Live zones, projectiles, and visual effects remain
bounded or pooled. Pattern scripts reference stable tuning keys and asset IDs,
not raw pointers to temporary data.

### Attack Cards

Each ordinary boss move becomes an attack card with:

- Move identifier
- Allowed phases
- Minimum and maximum range
- Weight
- Cooldown group
- Pressure cost
- Required free arena space
- Whether it can follow the previous card
- Recovery duration
- Counterplay tag

The deck selector filters invalid cards first, then chooses from the remaining
weighted set. It must remember recent cards and prevent immediate repetition.
If no card is valid, the boss repositions or uses a conservative fallback. It
must never select an attack merely because a random roll happened every frame.

### Survival Set Pieces

A survival set piece is a short sequence, generally six to fifteen seconds. It
contains multiple readable beats and at least one stable safe solution. It can
be entered at a health threshold or after a phase-specific cooldown. It should
end with a punish window long enough for every class to use one meaningful
ability.

Set pieces use a pressure budget. The budget counts projectile density, hazard
coverage, forced movement, active adds, and boss body pressure separately.
Adding more bullets must reduce some other source of danger. Safe space should
be measured against the actual player collision body and dash behavior.

### Room Geometry Contract

Patterns must work in handcrafted rooms without assuming a completely open
rectangle. Every pattern queries:

- Navigable floor
- Solid walls
- Prop collision
- Fall rectangles and fall type
- Closed and open door zones
- Entry safety region
- Valid boss movement lanes
- Valid player safe regions

A pattern that cannot fit must use a compatible variant or a safe fallback.
Boss logic must never silently place an impact inside unreachable terrain.

## Boss Encounter Designs

### Molarbeast: Lava Stampede

Molarbeast should teach the hybrid language. Its ordinary deck uses melee,
committed dash, and lava volley. Dash cannot immediately follow dash unless a
phase pattern explicitly requests a chain. Lava volleys create movement but
leave a readable side or diagonal escape.

Phase-one set piece: Molarbeast marks one long charge lane, locks it, charges,
then fires a short lava fan during recovery. A wall collision creates the best
punish window. Phase two mirrors the lane once and adds spaced lava impacts
behind the first charge. Phase three becomes Lava Stampede: two validated,
separately telegraphed charges cross the arena while lava shots occupy the
previous lane. The player alternates perpendicular dodges and punish attacks.

Do not fill the room with permanent lava. The challenge is reading the next
lane while remembering the temporary danger left by the last one.

### Werewolf: Blood Moon Hunt

Werewolf's ordinary deck uses swipe combo, pounce, circling, and howl. Combo
length escalates by phase, but the boss must visibly commit its facing during
the strike chain. Pounce uses a narrow locked path and a landing marker.

Phase-one set piece: howl, circle briefly, then perform one locked pounce and a
two-hit swipe. Phase two creates three sequential claw lanes with one safe gap,
then Werewolf pounces through the final lane. Phase three is Blood Moon Hunt:
the boss performs two individually telegraphed pounces from alternating sides,
followed by a long exhausted recovery if both miss.

Airborne movement should be visibly untargetable only if necessary. Otherwise,
allow reduced damage rather than silently rejecting hits.

### Chomp Bug: Acid Crossfire

Chomp Bug's ordinary deck uses orbit pressure, dive, and acid spit fan. Orbit is
repositioning, not constant unavoidable body damage. Dive direction locks and
does not curve after commitment.

Phase-one set piece: one dive lane followed by a three-shot acid fan. Phase two
adds a mirrored second dive, but the second warning appears only after the first
resolves. Phase three is Acid Crossfire: two fan volleys create alternating
angular gaps while the boss crosses one clearly marked dive lane. The player
must choose the safe wedge that also avoids the body path.

Acid projectiles and the diving body share one pressure budget so the player is
never denied every route simultaneously.

### Osiris: Judgement Of The Sun

Osiris's ordinary deck uses judgement nova, wrath volley, sand step, and melee.
Sand Step is repositioning with a clear vanish and destination cue. It should
not be an unexplained immunity window.

Phase-one set piece: a radial judgement ring leaves one broad safe sector.
Phase two adds a second delayed ring whose gap is rotated but still reachable.
Phase three is Judgement Of The Sun: Osiris relocates to a validated point,
fires rotating projectile spokes, and alternates two safe sectors before a
final nova. The pattern finishes with a clear recovery at center.

The spokes should rotate in authored steps, not continuously track the player.
The player's success comes from reading the sector sequence.

### Titan Guard: Siege Protocol

Titan Guard's ordinary deck uses shielded advance, melee, bomb lob, shield
charge, and Bulwark Slam. Frontal defense remains damage reduction with visible
feedback. Attack commitments lower the shield or create flank opportunities.

Phase-one set piece: bombs mark a simple checker or lane pattern, then Bulwark
Slam resolves after the safe lane becomes obvious. Phase two uses two bomb rows
with opposite gaps. Phase three is Siege Protocol: three timed bomb waves build
a safe corridor, Titan Guard charges through the corridor, then becomes
staggered after the final slam.

Bombs must not randomly jitter into the only safe tile. Pattern bombs use
authored positions transformed to valid room space.

### Toxic Vermin: Plague Flood

Toxic Vermin's ordinary deck uses bite, spit fan, burrow, eruption, and poison
pool. Burrow destination locks after a warning and validates against blocked or
fall terrain. Poison pools are short-lived and capped.

Phase-one set piece: two marked eruptions create a wide central safe lane.
Phase two adds a spit fan crossing that lane after a delay. Phase three is
Plague Flood: poison pools fill alternating room bands while Vermin performs
three independently marked eruptions through the remaining clean path.

The clean path must remain visible over every biome. Poison duration should end
soon after the pattern so the arena resets for ordinary combat.

### Ancient Bear: Dream Collapse

Ancient Bear's ordinary deck uses lumbering pressure, melee, Dream Pull, and
crushing slam. Pull strength is capped and cannot drag the player through solid
geometry or directly into unavoidable damage.

Phase-one set piece: Dream Pull announces itself, draws the player inward, then
one outer safe ring remains outside the slam. Phase two alternates an inner and
outer safe region. Phase three is Dream Collapse: a pulsing pull changes the
player's movement problem while three directional slam wedges resolve in a
learnable order.

The pattern tests controlled movement, not raw movement-speed stats. Every pull
tick respects collision and dash protection.

### Abyss Slime: Abyss Rain

Abyss Slime's ordinary deck uses melee, jump slam, summon, and acid burst. The
existing continuous airborne travel should remain; no teleport is allowed.
Landing points must be validated before lock.

Phase-one set piece: one tracked warning locks and the slime leaps, leaving two
small puddles away from the safe exit. Phase two chains a second independently
telegraphed leap. Phase three is Abyss Rain: several landing markers appear in a
forward progression, but only the active marker deals damage. The slime crosses
the room through these points while puddles form a temporary zigzag route.

Adds should be limited during Abyss Rain. The leap sequence itself is the
pressure source. A failed path query ends safely in recovery rather than moving
the slime to a fallback corner.

### Pumpkin Jack: Harvest Ritual

Pumpkin Jack's ordinary deck uses volley, summon, teleport strike, defense, and
melee. Summons are capped and should support the pattern rather than obscure
all projectiles.

Phase-one set piece: a ring of slow pumpkin shots opens one wide gap. Phase two
adds a teleport strike aimed through that gap after the projectiles lock. Phase
three is Harvest Ritual: temporary lantern or pumpkin anchors fire alternating
lanes, creating a moving safe corridor. Pumpkin Jack teleports only to authored,
validated anchor points and becomes vulnerable after the final strike.

If dedicated anchor assets are unavailable, existing animated VFX can represent
them temporarily. Raylib circles remain warnings only.

### Minotaur: Labyrinth Charge

Minotaur's ordinary deck uses melee, locked rush, stomp, and wall stun. It is
the clearest test of bait-and-punish counterplay. Rush direction never turns
after lock.

Phase-one set piece: one long charge with a strong wall-stun reward. Phase two
adds a second charge from the impact point with a fresh telegraph. Phase three
is Labyrinth Charge: authored warning lanes appear one at a time, asking the
player to reposition so Minotaur crashes into the room boundary or a valid
substantial obstacle. The sequence ends immediately on a crash and grants the
full punish window.

The pattern must not require a decorative prop that a particular room lacks.
Every supported room has a boundary-based solution.

## Presentation Rules

- Telegraph shapes may use simple Raylib geometry because they communicate
  targeting and safety.
- Executed attacks use animated VFX, projectiles, sprite animation, decals, or
  other authored visuals.
- Warning color, opacity, and outline remain readable over every region.
- A lock beat must be perceivable through sound, color, animation, or marker
  behavior.
- Phase callouts do not cover the player, aim cursor, or immediate danger.
- Multi-projectile patterns request one combined impact response rather than
  stacking full screen shake per projectile.
- Killing blows may interrupt a pattern safely.
- Room exit and death clear every live pattern event, zone, decal, projectile,
  and pending callback.

## Difficulty Rules

- Boss difficulty comes primarily from pattern comprehension, positioning,
  controlled projectile density, and sustain pressure.
- Do not compensate for weak patterns by inflating health.
- Boss target duration remains roughly 90 to 150 seconds on an on-curve build.
- Every set piece contains at least one reachable safe solution.
- Later phases may tighten timing or combine two previously learned ideas, but
  should not remove the established counterplay.
- Recovery windows remain meaningful in phase three.
- Adds and environmental projectiles have separate caps.
- A boss pattern cannot combine hard crowd control with an unavoidable hit.
- Damage during a phase transition remains possible unless the boss is visibly
  absent from the arena.

## Debug And Editor Requirements

Add a Boss Patterns section to the existing attack/debug tools. It should allow:

- Force boss type
- Force phase one, two, or three
- Force the current phase set piece
- Advance one pattern event
- Pause pattern time
- Restart the current pattern
- Show locked targets and movement segments
- Show damaging geometry
- Show safe-region analysis
- Show active zone and projectile pressure
- Show chosen attack card and why other cards were rejected
- Show pattern state, elapsed time, and recovery time
- Disable boss damage for observation
- Disable player damage for route testing

The editor should not become a completely separate engine project. It remains
part of Mystic Onslaught and edits tuning/pattern data consumed by the game.
The first boss pass may keep scripts in C++ while exposing tuning values. A
later authoring pass can move pattern timelines into validated data files after
the runtime behavior is proven.

## Implementation Phases

### Phase 0: Repair The Shared Foundation

- Fix derived enemy pooled reset behavior.
- Correct elite HUD labels.
- Replace Molarbeast-only boss detection with `IsBoss()`.
- Add explicit boss damage outcomes.
- Validate committed movement targets and paths.
- Connect or hide unsupported elite editor fields.
- Fix or replace `EliteActionClock`.
- Apply environmental projectile caps.
- Connect Guard Link feedback.

Do not begin all boss conversions until this phase passes automated and manual
verification.

### Phase 1: Generalize Encounter Primitives

- Introduce neutral encounter events and live zones.
- Preserve compatibility with current elite events.
- Add pattern cancellation and cleanup.
- Add attack cards and recent-card memory.
- Add pressure-cost calculation.
- Add room-geometry validation callbacks.
- Add deterministic tests for sequence timing and safe fallbacks.

### Phase 2: Prove The System On Two Bosses

Implement Molarbeast first because its charge, volley, and wall punish already
match the new language. Implement Osiris second because it tests projectiles,
safe sectors, and repositioning rather than physical charge collision. This
proves both major pattern families before touching the remaining eight.

Hand-test both bosses in an open room and at least two constrained handcrafted
rooms. Tune player readability before increasing projectile count.

### Phase 3: Physical And Pursuit Bosses

- Werewolf
- Ancient Bear
- Minotaur
- Abyss Slime

These bosses stress locked movement, pull/knockback collision, wall impacts,
and punish windows.

### Phase 4: Projectile And Arena-Control Bosses

- Chomp Bug
- Titan Guard
- Toxic Vermin
- Pumpkin Jack

These bosses stress projectile budgets, temporary hazards, safe corridors,
summon caps, and authored arena positions.

### Phase 5: Editor, Telemetry, And Final Tuning

- Expose supported pattern tuning.
- Record pattern hits, avoidances, clear time, player damage taken, and punish
  damage dealt.
- Tune with telemetry rather than health inflation.
- Complete controller and keyboard testing.
- Verify every room transition and death cleanup path.

## Automated Test Plan

- Large-delta sequence crossing one, two, and all stages
- Zero-duration stage safety
- Event queue and zone capacity behavior
- Pattern cancellation on death
- Pattern cancellation on room exit
- Pooled elite and boss reset
- Locked target does not move after commitment
- Blocked endpoint clamps to the last valid point
- No target inside wall, prop, fall rect, or closed door
- Attack card cannot immediately repeat
- Invalid cards are filtered before weighting
- Safe fallback when no card is valid
- Environmental projectile cap
- Add cap
- Guarded, reduced, untargetable, and immune damage outcomes
- `IsBossFightActive()` recognizes all boss classes

## Manual Elite QA Matrix

Test every compatible pairing in one open and one constrained handcrafted room:

- Ogre: Cage, Guard Links, Enrage
- Infernal: Guard Links, Enrage, Arena Pressure
- Bonechill: Cage, Guard Links, Arena Pressure
- Stormclub: Guard Links, Enrage, Arena Pressure
- Venomfang: Enrage, Arena Pressure

For every fight:

- Observe the first signature from full health.
- Force phase two and confirm it triggers once.
- Interrupt the telegraph where supported.
- Move after lock and confirm the attack does not track.
- Dash through warning geometry and verify protection rules.
- Fight beside walls, props, and fall areas.
- Leave or restart the room during an active signature.
- Reuse the same pooled enemy and confirm a clean reset.
- Confirm modifier labels and damage feedback are truthful.

## Manual Boss QA Matrix

For each of the ten bosses:

- Play all three phases naturally.
- Force each set piece repeatedly.
- Test keyboard and controller movement.
- Test every class with at least one low-mobility and one high-mobility build.
- Test open, narrow, obstacle-heavy, and fall-terrain rooms where compatible.
- Confirm every warning matches the damaging geometry.
- Confirm at least one reachable safe route exists.
- Confirm attacks do not repeat improperly.
- Confirm phase transitions do not create unexplained immunity.
- Confirm the punish window remains usable.
- Confirm death and room exit clear all pattern state.

## Definition Of Done

The boss encounter pass is complete only when:

- All prerequisite elite and boss foundation defects are resolved.
- Every boss has three hybrid phases.
- Every phase has one authored survival set piece and a valid ordinary deck.
- Every dangerous action follows Telegraph, Lock, Execute, Recovery.
- Every committed movement path respects room geometry.
- No boss silently becomes invulnerable.
- All environmental effects remain inside pressure budgets.
- The editor exposes only values that runtime actually uses.
- Automated tests pass.
- `Debug|x64` builds successfully.
- Every elite compatibility pairing and every boss phase has been hand-tested.

## Claude Implementation Instructions

Read this document together with:

- `docs/superpowers/specs/2026-07-18-elite-signature-kits-design.md`
- `docs/superpowers/plans/2026-07-18-elite-signature-kits-implementation-plan.md`

Begin with Phase 0 only. Do not rewrite every boss at once. Preserve existing
boss animations, current move implementations, user-authored attack tuning,
room systems, and all unrelated work. Produce a focused plan for the foundation
repairs, implement it, run deterministic tests, build `Debug|x64`, and report
which items still require Robert's hand testing.

After Phase 0 is approved, implement the neutral encounter primitives and then
Molarbeast as the first complete hybrid boss. Do not proceed to all remaining
bosses until Molarbeast is readable, fair, stable in constrained rooms, and
approved through gameplay testing.
