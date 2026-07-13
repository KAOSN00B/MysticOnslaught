# Mystic Onslaught Enemy Density and Damage Numbers Design

## Purpose

Increase combat population and spectacle without turning rooms into damage sponges or unreadable projectile fields. Replace the existing floating-text vector with a centralized, pooled damage-number system that always reports the actual result of player attacks.

This project precedes the active-VFX and dungeon-border projects because high-density combat needs stable feedback, predictable display limits, and measurable performance before more animated effects are added.

## Global Constraints

- Preserve existing class kits, attack damage, cooldowns, hitboxes, enemy behavior, saves, and unrelated village/editor work unless a change is explicitly required by this design.
- Continue using `GameBalance.h` as the central location for encounter and feedback tuning constants.
- Normal player-dealt damage is red and critical player-dealt damage is gold everywhere.
- Enemy damage against the player uses a clearly different presentation.
- Display actual post-defence damage, never theoretical pre-defence damage.
- Do not display an unexplained zero.
- Keep encounter population separate from simultaneous danger.

## Encounter Population Targets

Standard combat-room population targets are:

- Early rooms: 8-12 total enemies.
- Middle rooms: 12-18 total enemies.
- Late rooms: 18-26 total enemies.
- Swarm encounters: temporary peaks of 28-32 fragile enemies.
- Boss encounters: one boss plus 4-10 adds depending on boss complexity.

These are encounter totals, not requirements to spawn every enemy simultaneously. Reinforcement waves carry population that would make the opening unreadable or exceed the active-body cap.

## Separate Encounter Budgets

The encounter director must track four independent limits.

### Population Budget

Population counts enemy bodies. Most enemies cost one population slot. Summoned disposable creatures may use a fractional or grouped population classification internally, but debug output reports actual bodies alive, queued, and spawned.

Population selects the encounter's scale. It does not imply equal danger: twenty fragile melee enemies are not equivalent to twenty archers.

### Danger Pressure

Danger pressure measures simultaneous tactical load. Each enemy role receives a pressure cost based on range, durability, control, mobility, protection, summoning, or support behavior. Hazards also consume pressure.

The active room may continue releasing low-pressure bodies while population remains, but it cannot exceed the current danger-pressure cap. Pressure is recalculated from live enemies and active hazards rather than treated only as a spawn-time counter.

### Specialist Caps

Every tier and encounter template defines simultaneous caps for ranged, tank/shield, elite, support/summoner, assassin, and zoner roles. A large target population must never be reached by filling the room with expensive specialists.

Templates may reserve specialist slots, but all remaining population is filled with biome-appropriate fragile melee, charger, or swarm roles. Boss encounters use stricter caps selected per boss complexity.

### Projectile Caps

Enemy projectiles and environmental projectiles use separate caps. Reaching a projectile cap delays or suppresses new projectile attacks without blocking ordinary melee behavior. Boss-critical projectiles may reserve capacity so ambient hazards cannot starve a boss pattern.

## Enemy Cost Profiles

Each enemy exposes or resolves:

- Population cost.
- Danger-pressure cost.
- Specialist category.
- Projectile contribution or firing class.
- Whether it qualifies as fragile swarm filler.
- Maximum simultaneous count when needed for unique threats.

Existing `EnemyRole` and `GetSpawnCost()` behavior should be evolved rather than replaced with disconnected tables. The resolved profile must be visible in debug mode.

## Swarm Health and Rewards

Swarm encounters use enemies that die quickly to basic attacks and especially to area abilities. Their health is reduced through an explicit swarm profile rather than by globally weakening the enemy type.

Swarm enemies cannot be used to multiply XP, gold, Echoes, healing, or proc effects beyond intended room rewards. Room-clear progression remains the primary XP source. Per-enemy rewards use a swarm-aware budget so high population creates spectacle, not runaway economy.

## Encounter Construction

Encounter generation proceeds in this order:

1. Select depth tier and encounter template.
2. Roll target population inside the tier range.
3. Reserve any required specialist roles within specialist and pressure limits.
4. Fill remaining population with suitable low-pressure bodies.
5. Build an opening group that fits active-body, pressure, specialist, and projectile limits.
6. Queue the remaining authored enemy entries as mixed reinforcement waves.
7. Release waves when live pressure and body count have room, subject to minimum spacing and spawn protection.

Queued reinforcements store complete spawn entries, not only grunt type identifiers. This permits controlled mixed waves without rerolling composition during combat.

## Reinforcement Behavior

Reinforcements arrive from valid readable locations away from the player. A brief warning identifies spawn regions before enemies become active. Newly spawned enemies retain the existing short spawn protection against immediate overlap, but they must not damage the player before they are visually established.

Wave release is driven by both active population and current danger pressure. A timer can prevent a fight from stalling, but it cannot force a wave that violates hard safety caps. Swarm encounters may use shorter wave intervals with smaller low-pressure groups.

## Boss Adds

Bosses receive a complexity profile containing add-population range, active-add cap, permitted roles, reinforcement timing, and projectile reservation. Simple melee bosses may support more adds. Bosses with dense bullet patterns, summons, invulnerable phases, or arena hazards receive fewer adds.

Adds must complement a boss instead of obscuring its core lesson. Shield, ranged, support, and elite adds remain tightly capped. Boss phase transitions can release or replace add groups when authored, but should not accumulate every group simultaneously.

## DamageNumberManager

Create a focused `DamageNumberManager` responsible for player-dealt damage numbers and combat outcome labels. It owns a fixed-capacity pool allocated during initialization and performs no per-hit heap allocation during ordinary combat.

The manager consumes a resolved event containing:

- Stable target identifier.
- Fixed world-space impact position.
- Final damage dealt.
- Outcome type: normal, critical, blocked, immune, dodge, armour, or informational.
- Flags for backstab, killing blow, elite, boss, damage over time, and multi-hit source.
- Element/status color metadata only when the outcome is not normal or critical direct damage.
- Spawn timestamp and optional source attack identifier.

Numbers capture their world position when spawned and never continue following the enemy. Rendering applies the current camera transform each frame.

## Pool and Display Limits

The storage pool and visible display cap are separate values. The pool must have enough entries for merge candidates and prioritized queued events, while the visible cap protects readability.

When display capacity is reached, priority is:

1. Killing blow on a boss or elite.
2. Critical killing blow.
3. Backstab or critical hit.
4. Ordinary killing blow or large hit.
5. Blocked, immune, or dodge explanation.
6. Ordinary direct damage.
7. Damage-over-time accumulation.

Lower-priority entries may be merged, replaced, or suppressed. A critical, backstab, kill, blocked, immune, or dodge event may not be silently discarded when a lower-priority entry can be replaced.

## Merge Rules

Rapid events against the same target merge over a configurable 0.15-0.25 second window.

- Ordinary direct hits merge only when they share compatible outcome and source grouping.
- Critical hits never merge into ordinary damage.
- Backstab labels remain attached to the qualifying event.
- Killing blows immediately finalize and display the accumulated value.
- Damage-over-time ticks accumulate into one short-lived number per target and status source.
- Repeated multi-hit area attacks may show one accumulated number per enemy rather than one per frame.
- Blocked, immune, dodge, and armour outcomes do not merge into damage values.

Merge candidates preserve the final fixed impact position or use a bounded average of contributing impact positions. They do not chase a moving target.

## Normal Damage Presentation

Normal player damage uses strong red text with a dark outline or shadow. It spawns at the impact point, receives a small deterministic horizontal drift, rises, starts slightly enlarged, settles quickly, and fades smoothly.

Font size scales modestly using final damage relative to a configurable reference value. Strict minimum and maximum sizes prevent tiny unreadable ticks or screen-filling numbers.

## Critical Presentation

Critical damage uses bright gold, approximately 25% larger than an equivalent normal hit. It receives a stronger initial punch and upward velocity. A small `CRIT` label may accompany the value but never compete with it.

Critical feedback reuses an existing critical sound and impact effect when available. Critical screen shake remains restrained; repeated or multi-target criticals aggregate shake rather than triggering full shake per enemy.

## Special Outcomes

- Blocked: display `BLOCKED` and no zero.
- Invulnerable: display `IMMUNE` or `INVULNERABLE` according to the actual state.
- Dodge: display `DODGE` only when collision reached a valid dodge window.
- Backstab: attach `BACKSTAB` to the resolved red or gold damage event.
- Armour: use a distinct cool or neutral treatment that cannot be mistaken for critical gold.
- Airborne or underground denial: display a specific explanatory label when the target state caused the denial.

Outcome reporting must be produced by the damage-resolution path that knows why damage was denied, not inferred later by the renderer.

## Killing Blows and Multi-Kills

Killing blows receive a modest size increase, faster punch, and stronger impact burst. Optional hit-stop is 0.025-0.05 seconds for ordinary kills, with a separately tuned stronger value for elites and bosses.

Multi-kill feedback aggregates screen shake and hit-stop over the frame or attack resolution. It may strengthen once based on the number and importance of kills, but it may not stack one full-strength shake per victim.

## Player Damage Received

Enemy damage against the player remains visually distinct. It must not use the same strong red/gold hierarchy as outgoing damage. A separate color, motion direction, anchor, or label convention identifies incoming damage immediately.

The outgoing `DamageNumberManager` may support incoming events through a separate style, or incoming feedback may remain in a separate manager. In either case, pooling and room-transition clearing rules still apply.

## Debug Tools

Extend the existing debug/juice controls with:

- Force critical hit, preserving the existing behavior.
- Enable or disable damage numbers.
- Display final damage calculation components.
- Display pool capacity, active entries, free entries, suppressed entries, replaced entries, and high-water mark.
- Display active merge candidates and cumulative merged-event count.
- Adjust minimum font size, maximum font size, damage scaling reference, rise speed, horizontal drift, lifetime, outline thickness, merge window, and visible cap.
- Freeze or step damage-number animation for inspection.

Encounter debug output displays target population, spawned population, live bodies, queued bodies, live pressure/cap, specialist counts/caps, enemy projectile usage/cap, environmental projectile usage/cap, and reinforcement timing.

## Font

The current project uses Raylib's default font for combat numbers. Use it initially so implementation does not invent an unrelated visual identity. The manager accepts a `Font` dependency so a later project-wide custom font can replace it without rewriting damage logic.

## Lifecycle and Safety

Initialize the pool once with the engine's combat resources. Clear active and pending entries when entering a new room, returning to the village, resetting a run, opening a non-combat test state, or shutting down.

Stable target identifiers must not dereference destroyed enemies. Merging uses an identifier value only; number instances never retain raw enemy pointers.

## Performance Requirements

- No ordinary per-hit heap allocation in `DamageNumberManager`.
- No unbounded vectors for active damage numbers.
- Encounter generation loops have explicit bounds.
- Active enemy and projectile caps are enforced before spawning.
- Debug counters expose suppressed feedback and cap pressure.
- Stress testing at 32 live fragile enemies includes repeated AOE and damage-over-time effects.

## Testing

Deterministic tests should cover pool reuse, visible-cap priority replacement, normal merging, critical separation, DoT accumulation, fixed world positions, room clearing, outcome labels, and final-damage values.

Encounter tests should cover every tier's population range, guaranteed specialist caps, population-pressure separation, mixed reinforcement serialization, projectile caps, swarm health/reward profiles, and boss complexity profiles.

Manual verification includes:

- Early, middle, late, swarm, and boss-add encounters.
- 32-enemy stress rooms with AOE, multi-hit, DoT, critical, backstab, and multi-kill attacks.
- Normal damage consistently red and critical consistently gold across every class.
- Incoming player damage visibly distinct.
- Camera movement after number spawn.
- Room transitions and village return with no stale numbers.
- Debug tuning and telemetry at native resolution.

Finish with a fresh `Debug|x64` build and record warnings separately from failures.
