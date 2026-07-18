# Mystic Onslaught Elite Signature Kits

Date: 2026-07-18
Status: Approved direction, implementation specification

## Purpose

The five elite-room minibosses will become compact tests for the same encounter
language later used by full bosses: committed telegraphs, survival sequences,
phase escalation, safe gaps, counterplay, and punish windows. Elites must feel
more substantial than ordinary enemies without becoming long damage sponges or
requiring full multi-minute boss scripts.

The elite pool remains:

- Ogre
- Infernal
- Bonechill
- Stormclub
- Venomfang

Each elite receives a recognisable basic pattern, one signature survival move,
one counterplay weakness, and one escalation below 50% health. The target fight
length is roughly 35-55 seconds for an on-curve build.

## Scope

This pass implements all five elite kits, their VFX/audio feedback, tuning and
editor support, compatible elite-room challenge modifiers, deterministic tests,
and debug controls. It does not convert the existing full bosses to the new
phase system. The shared event and phase hooks are intentionally reusable so a
later boss pass can build longer Diablo-style survival phases on them.

No elite becomes completely invulnerable. A player who creates a safe damage
window can always make progress. Guard-based mechanics use visible damage
reduction rather than silently rejecting hits.

## Encounter Rules

Every dangerous elite action follows the same readable sequence:

1. Telegraph: the target area and direction become clear before commitment.
2. Lock: the elite commits its target or travel direction.
3. Execution: damage and movement occur exactly where the warning indicated.
4. Recovery: the elite provides a deliberate punish window.

Warnings may use simple targeting outlines because they communicate danger;
the attack itself must use animated VFX rather than persistent Raylib circles.
Warnings remain visible over every biome and use consistent danger colours.

Phase escalation occurs once when the elite crosses 50% health. The current
attack is safely cancelled, a short callout and VFX announce the transition,
and the elite begins its signature survival sequence. It remains damageable
during the transition and sequence. Killing it early is allowed and rewarded.

## Shared Architecture

### Signature Hook

`Enemy` gains a small virtual signature interface used by curated elites. The
base update keeps ownership of common statuses, collision, navigation, standard
melee and animation. A signature hook may temporarily own movement and attacks
and returns whether the normal chase/melee logic should be skipped that frame.
Each elite stores its own enum state and timers in its class; there is no large
shared state machine attempting to encode all five behaviours.

The existing generic elite lunge no longer activates merely because
`SetIsEliteMiniboss(true)` was called. Signature movement belongs to the elite
that was designed for it. Stormclub gets its own leap state; Ogre keeps its own
charge; Infernal, Bonechill and Venomfang do not inherit an unrelated dash.

### Signature Events

Elite classes emit small combat events when an authored beat begins or lands.
Events describe the elite type, move id, origin, direction or target, phase and
timing. `CombatDirector` consumes them and coordinates VFX, projectiles, screen
shake, sound and player damage. This keeps renderer/effect ownership outside the
enemy AI while making the same event path reusable for later boss phases.

Events are value objects stored in a fixed or bounded per-enemy queue. They do
not allocate every frame. Stale events are cleared on enemy reset, room change,
death and pool reuse.

### Attack Tuning And Editor

Every signature move receives an `AttackTuning` id and appears in the existing
enemy/attack editor under an Elite category. Editable values include:

- cast and impact offsets relative to the elite
- target or travel distance
- hitbox or impact radius
- telegraph, active and recovery timing
- movement speed and lock time
- cooldown
- VFX offset and scale
- phase-two multipliers

The editor timeline previews warning, movement path, impact VFX and hitbox. It
shows the relevant elite sprite and supports facing left/right. Saving uses the
normal tuning path, and missing tuning safely falls back to conservative coded
defaults.

## Elite Kits

### Ogre: The Battering Ram

The Ogre remains the physical control elite. Its existing rush displays a clear
lane, commits direction after the windup, scatters lesser enemies, and stuns the
Ogre when it hits a wall or substantial obstacle. Wall impact produces debris,
a heavy sound and a strong but brief impact response. The stun is the primary
punish window and must remain long enough for every class to land one meaningful
ability.

Below 50% health, the Ogre announces `SECOND WIND` and performs two charges.
After the first charge it pauses, visibly retargets, then commits the second.
The second charge cannot silently rotate during travel. If either charge hits a
wall, the sequence ends in the normal stun. Counterplay is perpendicular
movement followed by attacking during wall recovery.

### Infernal: The Living Furnace

Infernal melee hits continue to burn. During normal pursuit it periodically
enters Cinder March, walking a committed line while leaving short-lived animated
flame patches behind it. Patches are spaced rather than continuous so crossing
the trail remains possible. They expire quickly enough that the room does not
become permanently divided.

Its signature Furnace Burst stops movement, brightens the body, then sends three
animated fire fissures forward with readable gaps. Fissures use the locked
facing direction and do not home after release. Below 50% health, `OVERHEATED`
adds one staggered fissure wave and slightly accelerates melee, followed by a
longer exhausted recovery. Counterplay is moving through the authored gaps and
punishing the exhaustion, not circling while it passively burns the whole room.

### Bonechill: The Frozen Wall

Bonechill remains immovable under ordinary player knockback. While deliberately
advancing, visible frontal frost armour reduces front-facing damage by 45%.
Attacks from the rear and all damage during its signature windup/recovery remain
fully effective. The armour uses a clear icy overlay and distinct hit response;
it never displays `IMMUNE` and never reduces actual damage to zero.

Permafrost Slam has a committed cone and launches several animated ice lanes
with safe spaces between them. A direct slam deals moderate damage and chill;
the lanes deal lighter damage and chill. At 50% health, `ARMOUR SHATTERED`
removes frontal reduction, bursts harmless visual fragments outward, increases
movement speed by approximately 25%, and shortens slam cooldown modestly.
Counterplay changes from flanking/baiting to faster dodge timing after the phase.

### Stormclub: The Thunder Breaker

Stormclub owns a bespoke leap instead of the generic elite lunge. A landing
marker appears, locks after the windup, and the elite travels to that point.
Landing creates an animated central impact and three lightning branches with
visible angular gaps. Direct impact deals the most damage and knockback; branch
contact deals lighter damage and a short shock. The attack never teleports and
cannot change its target after lock.

If Stormclub misses the player, its club remains embedded for a visible recovery
window. Below 50% health, `TEMPEST` chains two shorter leaps; the second target is
shown and locked only after the first landing. Each landing remains individually
avoidable. Counterplay is moving after lock and attacking during the embedded-
club recovery.

### Venomfang: The Ambush Predator

Venomfang circles and seeks an off-angle approach. Its Venom Pounce shows a
narrow path, commits, then bites at the endpoint. A landed bite applies a real
poison status rather than routing through the burn presentation. Poison uses a
green status indicator, separate timing and capped refresh rules so repeated
bites are threatening without stacking unlimited unavoidable damage.

After a bite, Venomfang disengages and leaves a short animated poison trail.
Predator's Mark increases the danger of consecutive successful bites, caps at
three, and expires after several seconds without another bite. Below 50% health,
`BLOOD SCENT` increases circling speed and allows a second clearly telegraphed
pounce after a delay. It does not become invisible or untargetable. Counterplay
is interrupting the windup, avoiding consecutive bites, and choosing whether a
poisoned retreat path is worth chasing through.

## Elite-Room Challenge Modifiers

Random challenge mechanics become compatibility-aware. The generic Leap room
modifier is removed because movement is now part of individual kits. The pool
contains:

- Shrinking Cage
- Guard Links
- Permanent Enrage
- Arena Pressure

Guard Links replaces complete invulnerability with 60% damage reduction while
at least one linked guard lives. Visible beams connect guards to the elite, hits
show reduced real damage, and `GUARD BROKEN` appears when the last link falls.

Compatibility prevents combinations that erase counterplay:

- Ogre: Cage, Guard Links, Enrage
- Infernal: Guard Links, Enrage, Arena Pressure
- Bonechill: Cage, Guard Links, Arena Pressure
- Stormclub: Guard Links, Enrage, Arena Pressure
- Venomfang: Enrage, Arena Pressure

Arena Pressure is themed by elite but uses the shared pressure budget: debris
for Ogre, fire for Infernal, ice lanes for Bonechill, lightning for Stormclub,
and poison patches for Venomfang. It cannot exceed the room's environmental
projectile cap or place danger inside entry/door safety space.

## Feedback And Presentation

The entrance banner shows the elite name and challenge modifier. Health bars
receive a restrained archetype accent: Ogre rust, Infernal red-orange,
Bonechill ice blue, Stormclub electric gold and Venomfang toxic green. Phase
callouts appear once and never cover aiming information.

Signature windup, lock, impact and recovery each have distinct sounds. Large
impacts request centralised shake and hit-stop; multi-branch attacks trigger one
combined response rather than stacking one shake per hit. VFX are pooled and
cleared safely when leaving a room.

## Difficulty And Fairness

- Elite health remains near the existing 2.5x elite conversion and is tuned by
  measured fight duration, not increased automatically to prolong phases.
- A signature hit may be severe, but its telegraph must be reliable.
- No elite combines a hard crowd-control effect with an unavoidable follow-up.
- Recovery windows survive phase two; escalation tightens timing but does not
  remove counterplay.
- Elites respect authored collision/fall geometry and never choose a target or
  landing point inside blocked space.
- Dashes and movement skills can cross warning zones when their existing rules
  allow it, but cannot bypass a committed body collision without the normal
  dash protection.

## Debug And QA

Debug controls allow forcing elite type, room modifier, health phase, signature
move, cooldown reset, and telegraph/hitbox display. Telemetry records elite type,
modifier, phase-two time, signature casts, signature hits, damage taken and
fight duration.

Focused tests cover:

- each state transition and timer
- target lock remaining stable after telegraph
- no damage before the active frame/window
- recovery reached after both hit and miss paths
- one-time 50% phase transition
- modifier compatibility filtering
- Guard Links reducing rather than nullifying damage
- blocked landing/charge target fallback
- reset, pool reuse and room-exit event clearing
- snapshot/re-entry preserving elite type without stale signature events

The final verification is a `Debug|x64` build plus playtests of every elite with
every compatible modifier in one open and one constrained handcrafted room.

## Future Boss Phase Expansion

Full bosses later use the same signature events, target-lock rules, tuning data,
phase callouts and survival-sequence telemetry. Bosses may chain several events
into longer phase scripts, add phase-specific arena changes, and use health or
time gates. This elite pass deliberately proves those primitives before any
existing boss is rewritten.
