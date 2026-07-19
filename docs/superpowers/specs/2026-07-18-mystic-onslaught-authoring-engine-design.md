# Mystic Onslaught Authoring Engine Design

Date: 2026-07-18
Status: Approved architecture, implementation specification pending review
Audience: Robert, Claude, Codex, and any future contributor

## Purpose

Build a purpose-made 2D action-roguelite authoring engine directly into Mystic
Onslaught. The tools must enhance the existing game, not create a separate game,
replace working handcrafted rooms, or require Robert to edit C++ for routine
content tuning.

The completed system allows Robert to:

- create reusable obstacles, traps, totems, moving hazards, switches, animated
  objects, projectiles, and other room objects;
- attach collision, damage, movement, targeting, projectile, trigger, timeline,
  VFX, sound, destructible, and interaction behavior through editor controls;
- place those assets into handcrafted room blueprints through a Made Assets
  shelf;
- tune enemy base values, attacks, aggression, status behavior, hitboxes, phases,
  and run-depth scaling without changing hardcoded class constants;
- playtest rooms and assets immediately, including pause and frame-step;
- produce a Public Demo or Release build in which no editor, debug panel, cheat,
  hidden shortcut, or UI-adjustment tool can be opened.

This is a developer-facing layer of the existing Mystic Onslaught project. The
content it creates is loaded by the real game and shipped in player builds.

## Non-Goals

This system is not intended to reproduce all of Unity. It will not add arbitrary
user scripts, a general visual-programming language, 3D import, a general ECS,
custom shader authoring, package management, or an extension marketplace.

The editor does not replace the existing Room Editor, Attack Editor, Character
Animator, asset metadata, or handcrafted room library. Those systems are reused,
adapted, and gradually presented through one consistent shell.

The first implementation does not make every existing enemy, boss, hazard, or
projectile data-driven at once. It proves the complete path with representative
content, then migrates existing content incrementally.

## Same-Project Build Model

The current Visual Studio solution remains the only project repository. It gains
three explicit x64 configurations:

### Developer

The full game plus the unified editor, room playtesting, debug panel, tuning
tools, gizmos, validation console, live reload, and all developer shortcuts.

### Public Demo

The real game and all authored runtime content, but compiled with
`MO_DEV_TOOLS=0`. Editor game states, editor source files, debug commands, cheats,
hidden main-menu keys, border/UI adjusters, forced room controls, and direct test
entry points are unavailable. They are not merely hidden.

### Release

The same runtime content system as Public Demo, with final optimization and
packaging settings. It does not contain developer entry points.

`MO_DEV_TOOLS` is the single build-level authority. Scattered `_DEBUG` checks are
not sufficient because a developer may need an optimized build with tools, and
a public demo may still include symbols for crash diagnosis.

## Architectural Layers

Dependencies flow downward only:

```text
Editor Shell
    -> Content Database and Definition APIs
    -> Room Blueprint APIs

Game Runtime
    -> Content Database and Definition APIs
    -> Runtime Directors and Factories

Content Database
    -> Versioned Content Parser and Validators

Core Definitions
    -> small value types only
```

The Editor Shell never mutates live game objects directly. The Game Runtime never
depends on editor panels, selection state, gizmos, or undo history. Public builds
compile the runtime layers while excluding editor layers.

## Core Content Types

### Gameplay Asset Definition

A reusable room object saved as `.gasset`. Examples include Ice Totem, Moving
Blade, Sliding Wall, Lava Vent, Fireball Torch, Pressure Plate, Destructible
Barricade, rotating hazard, or an animated environmental mechanism.

```cpp
struct GameplayAssetDefinition
{
    ContentId id;
    std::string displayName;
    std::string category;
    std::vector<std::string> tags;
    int schemaVersion;

    VisualComponent visual;
    std::optional<AnimationComponent> animation;
    std::optional<CollisionComponent> collision;
    std::optional<DamageComponent> damage;
    std::optional<MovementComponent> movement;
    std::optional<TargetingComponent> targeting;
    std::optional<EmitterComponent> emitter;
    std::optional<TriggerComponent> trigger;
    std::optional<DestructibleComponent> destructible;
    std::optional<InteractionComponent> interaction;
    std::optional<AudioVfxComponent> feedback;
    BehaviorTimeline timeline;
};
```

This is explicit composition, not a general ECS. Components are known value
types with documented fields and deterministic runtime behavior.

### Projectile Definition

A reusable projectile saved as `.projectile`. It can be referenced by gameplay
assets, enemies, bosses, or player abilities without duplicating its travel and
impact configuration.

Required editable fields include:

- visual sprite strip, frame size, frame count, frame time, scale, tint, and
  orientation;
- collision shape and radius or rectangle;
- speed, acceleration, maximum turn rate, lifetime, range, pierce count, and
  environmental collision policy;
- targeting mode: fixed direction, player snapshot, tracked player, predicted
  position, radial, spread, sweep, or authored path;
- final damage, knockback, and status payload;
- cast, travel, impact, expiration, and blocked VFX;
- cast, travel-loop, impact, and blocked sounds;
- whether it can hit the player, enemies, destructible objects, or other
  supported collision layers;
- pressure cost and maximum simultaneous instances.

An Ice Bolt definition therefore behaves identically whether fired by an Ice
Totem, enemy, boss, or test object unless the emitter applies an explicit
instance override.

### Enemy Definition

An enemy definition saved as `.enemy`. It contains editable base configuration,
references to attacks/projectiles, and run-scaling behavior. Existing C++ enemy
classes remain responsible for unique algorithms until a behavior has been
safely moved into reusable data.

Enemy difficulty is resolved in this order:

```text
base enemy definition
x current-run threat tier
+ optional region behavior profile
+ optional postgame modifier
+ explicit debug override in Developer builds only
```

Starting a new run resets threat-tier effects. Region profiles primarily add
thematic behavior, projectile, status, or cadence differences; they should not
duplicate entire enemy stat sheets. Postgame modifiers remain named, inspectable
layers rather than permanently editing base values.

The Enemy Inspector always shows the final resolved value and its sources. For
example:

```text
Attack Cooldown: 1.12 s
  Base Skeleton Archer: 1.40 s
  Run Threat Tier 3: x0.90
  Ancient Castle Veteran: x0.89
```

### Room Asset Instance

`RoomBlueprint` gains a versioned collection of gameplay-object instances:

```cpp
struct GameplayAssetInstance
{
    ContentId assetId;
    InstanceId instanceId;
    Vector2 position;
    float rotationDegrees;
    bool mirrorX;
    RoomDrawBand drawBand;
    std::vector<Vector2> pathPoints;
    PropertyOverrideSet overrides;
};
```

The room stores a reference plus deliberate overrides, not a full copied prefab.
Updating a source asset updates all non-overridden instances. Every overridden
field is visibly marked in the Inspector and supports Revert or Apply To Asset.

## Stable Identity And References

Display names are editable, but references use stable `ContentId` strings created
once. Renaming an asset cannot break rooms. Duplicating creates a new ID. Deleting
an asset first queries a dependency index and lists every room, gameplay asset,
projectile, or enemy that still references it.

The Content Database is the only authority allowed to resolve IDs:

```cpp
const GameplayAssetDefinition* FindGameplayAsset(ContentId id) const;
const ProjectileDefinition* FindProjectile(ContentId id) const;
const EnemyDefinition* FindEnemy(ContentId id) const;
ContentValidationReport ValidateAll() const;
std::vector<ContentReference> FindReferencesTo(ContentId id) const;
bool Reload(ContentId id, ContentError& error);
```

Runtime objects store stable IDs or short-lived handles supplied by the database,
not owning raw pointers into containers that may reallocate during hot reload.

## File Format And Safe Persistence

Definitions use a shared section-based format consistent with the project's
existing text-authored workflow. Nested lists use numbered sections:

```ini
version=1
id=ice_totem
name=Ice Totem
category=Hazard

[visual]
sprite=PowerUps/Hazard_IceTotem.png
scale=4.0

[trigger]
type=CombatStart

[emitter]
projectile=ice_bolt
count=1

[timeline.0]
action=Wait
duration=1.5

[timeline.1]
action=Telegraph
duration=0.6

[timeline.2]
action=LockTarget

[timeline.3]
action=FireProjectile

[timeline.4]
action=Recover
duration=1.2

[timeline.5]
action=Repeat
target=0
```

A shared `ContentDocument` parser owns section parsing, typed conversion, source
line numbers, unknown fields, and error messages. Type serializers do not perform
ad hoc `sscanf` loops independently.

Every save follows a transaction:

1. Serialize to a temporary file.
2. Load the temporary file through the production parser.
3. Validate schema, references, ranges, and timelines.
4. Preserve the previous valid file as a recovery backup.
5. Replace the destination.
6. Reload the Content Database.
7. Restore the backup if replacement or reload fails.

Autosave writes to a separate recovery location and never silently overwrites the
last deliberate save.

## Gameplay Asset Components

### Visual

Source texture or authored tile composition, scale, tint, pivot, facing, draw
band, visibility, and preview thumbnail.

### Animation

Named clips, source frames, frame times, loop policy, default clip, transitions,
and frame events for sound, VFX, damage activation, projectile emission, or
collider changes.

### Collision

Multiple editable rectangles or supported simple shapes. Each collider declares
whether it blocks player, enemies, ordinary projectiles, environmental
projectiles, or dashes. Debug visualization uses distinct colors per mask.

### Damage

One or more damage shapes, active windows, damage amount, tick cadence, knockback,
status payload, hit cooldown, and whether the object can damage once or
repeatedly. Damage is inactive during telegraph unless the definition explicitly
uses a persistent active field.

### Movement

Static, point-to-point, loop, ping-pong, one-shot, orbit, or authored path. Fields
include speed, easing, waits at path points, initial offset, activation trigger,
collision response, and whether the path restarts on room re-entry.

### Targeting

Fixed direction, player snapshot, tracked player, predicted position, radial,
spread, sweep, or authored point. Target tracking must define a visible lock time
so attacks cannot follow the player after their warning has committed.

### Projectile Emitter

Projectile ID, spawn offset, count, spread, burst delay, cooldown, maximum live
children, and optional overrides. Emitters do not own projectile collision; they
request a projectile from the existing centralized runtime system.

### Trigger

Supported activation sources:

- Always Active
- Combat Start
- Player Enter
- Switch Activated
- Object Destroyed
- Room Clear

Triggers may be one-shot or resettable and may include a delay. References to
switches or other objects use stable instance IDs.

### Destructible

Health, armour or resistance if needed, hit cooldown, disabled state, recovery
time, destroyed animation, destroyed collision policy, rewards, and events.

### Interaction

Prompt, interaction radius, input policy, required state, and emitted event. This
supports switches and mechanisms without turning gameplay assets into NPCs.

### Audio And VFX

Named events for idle, telegraph, lock, activation, movement, fire, impact,
disabled, recovered, and destroyed. VFX are visual only; gameplay geometry remains
owned by collision/damage/projectile data.

## Controlled Behavior Timeline

The timeline coordinates components through a bounded list of known actions:

```text
Wait
Telegraph
LockTarget
Move
FireProjectile
EnableDamage
DisableDamage
PlayAnimation
PlayVfx
PlaySound
SetColliderEnabled
EmitEvent
Repeat
Stop
```

There is no arbitrary script execution. Each step has a maximum duration or
bounded repeat rule. Validation rejects missing targets, out-of-range repeat
indices, unbounded zero-duration loops, missing projectiles, and timelines that
can activate damage without a valid shape.

Runtime evaluation is deterministic and uses explicit state:

```text
Triggers -> Timeline -> Targeting -> Movement -> Emission -> Damage -> Animation
```

Room exit, object destruction, playtest stop, and runtime reset clear all pending
events, emitted children, loops, VFX handles, and collision state.

## Runtime Ownership

`GameplayObjectDirector` owns the current room's `GameplayObjectRuntime` pool.
Definitions remain immutable while a room is running. Runtime instances own only
timers, current timeline step, animation state, path progress, health, target
snapshot, and bounded child handles.

The Director communicates with existing systems through narrow callbacks or
service interfaces:

- projectile spawning remains centralized;
- player/enemy damage remains centralized;
- VFX and audio remain owned by their managers;
- navigation blocking is rebuilt only when a collider actually changes;
- room pressure accounting reads declared hazard/projectile costs;
- room transitions clear the complete gameplay-object runtime before loading the
  next blueprint.

No gameplay asset creates a separate unbounded vector of projectiles, particles,
or child objects. Definitions specify caps, and global runtime systems enforce a
second hard cap.

## Unified Editor Shell

The Developer build presents one consistent workspace:

```text
Top Toolbar
  Select | Move | Collider | Path | Trigger | Play | Pause | Step | Save

Left
  Hierarchy: room objects and selection

Center
  Scene View: room, assets, paths, colliders, ranges, and previews

Right
  Inspector: collapsible components and selected-object overrides

Bottom
  Asset Browser / Made Assets / Projectiles / Enemies / VFX / Console
```

The initial shell reuses the current Room Editor scene rendering and input model.
Existing editor screens may remain accessible during migration, but the shell is
the destination for shared selection, property controls, play mode, and content
navigation.

### Required Editor Behavior

- searchable hierarchy with select, rename, hide, lock, duplicate, reorder, and
  delete;
- explicit Inspector adapters for each component; no fragile C++ reflection;
- move, collider, damage-shape, path-point, targeting, and direction gizmos;
- grid, half-grid, configurable increment, and free-movement snapping;
- multi-selection for movement, duplication, deletion, alignment, and shared
  component values;
- transactional undo/redo for every scene and Inspector mutation;
- asset browser with thumbnails, search, category, tags, favorites, recent assets,
  and biome/source filtering;
- stable placement silhouette before clicking;
- copy/paste component and complete-object values;
- independent visibility toggles for solid collision, damage areas, paths,
  triggers, targeting, projectile previews, navigation blocking, fall zones, door
  safety, and spawn restrictions;
- validation console entries that select the offending object when clicked;
- save-state indicator, explicit save, autosave recovery, and unsaved-exit prompt.

## Made Assets Workflow

The bottom of the Room Editor gains a Made Assets shelf early, before the full
shell is complete.

```text
Create Asset | Edit | Duplicate | Rename | Delete | Search | Filter
```

Creating opens the Gameplay Asset Editor. Robert chooses an empty asset or a
template such as Fire Totem, Projectile Totem, Moving Blade, Sliding Wall,
Damage Vent, Switch, or Destructible Obstacle. Templates enable components and
seed conservative defaults; they are not hardcoded runtime subclasses.

The asset editor uses a familiar canvas:

- construct or select the visual;
- set pivot and draw band;
- add and resize solid/damage/trigger colliders;
- add path points and preview motion;
- select animations and frame events;
- attach VFX and sounds from searchable libraries;
- select or create a projectile;
- configure trigger and timeline;
- run a local player test;
- validate and save under a unique name and stable ID.

Selecting an asset in the room shows its silhouette. Click places it; dragging
supports repeated placement where appropriate. Selecting a placed instance shows
source values and overrides separately.

## Projectile Authoring Workflow

The Projectile Editor previews cast, travel, collision, impact, expiration, and
status timing on one timeline. It includes a test player and target dummy, supports
left/right and arbitrary direction, and can simulate several projectiles without
saving them into a room.

An emitter's projectile picker supports New, Edit Referenced, Duplicate, and
Select Existing. Editing a shared projectile warns that every referencing asset,
enemy, boss, or ability will receive the change.

## Enemy Inspector Workflow

The Enemy Inspector discovers supported enemies from the Content Database and
shows:

- identity, role, spawn cost, and tags;
- health, damage, movement, attack cadence, preferred range, aggression, turning,
  navigation, and knockback;
- animation clips and frame timing;
- solid, hurt, melee, projectile, and signature hitboxes;
- attack/projectile references and status payloads;
- phase thresholds and phase-specific modifiers;
- run threat-tier curves;
- optional region behavior profiles;
- optional postgame modifier previews;
- final resolved values with contribution breakdown;
- real-time test arena with player class selection, pause, step, reset, forced
  attack, forced phase, and telemetry.

The first migration moves common base values out of hardcoded reset functions.
Unique algorithms stay in C++ and read parameters from the definition. A later
migration may move suitable behavior into controlled timelines only after the
common system is proven.

## Play Mode And Live Tuning

Entering Play Mode clones the current unsaved room blueprint and referenced
definitions into a temporary runtime session. The editor document, undo history,
selection, and camera remain unchanged.

Play controls include Run, Pause, Single Frame, Restart, and Stop. Safe scalar
properties may be changed while paused. On Stop, runtime changes are discarded by
default. `Apply Play Changes` opens a review listing exactly which supported
authoring values changed; transient health, timers, targets, positions produced by
gameplay, and spawned children can never be applied.

Hot reload is transactional: parse and validate a replacement definition first,
then swap it only while paused or after restarting the preview object. A failed
reload leaves the last valid definition active.

## Prefabs And Overrides

The source asset is the prefab. Every room instance inherits it. The Inspector
uses three field states:

- inherited;
- overridden in this room;
- invalid because the source field/component was removed.

Per-field actions are Revert and Copy Value. Per-instance actions are Revert All,
Apply Selected Overrides To Asset, and Unpack. Unpack is supported only when a
fully independent copy has a valid reason; it creates a new asset ID rather than
embedding anonymous behavior inside a room.

Editing a prefab displays affected-room count and runs dependency validation
before saving. Removing a component referenced by instance overrides requires an
explicit confirmation and reports which overrides will be removed.

## Collision Layers And Navigation

Use a small explicit collision-mask set:

```text
Player
Enemy
Player Projectile
Enemy Projectile
Environmental Projectile
Dash
Navigation
Interaction
```

The Inspector uses checkboxes. Runtime collision dispatch remains centralized;
objects declare masks but do not implement independent collision loops.

Moving solid obstacles must either be navigation-neutral, update navigation at a
strict throttled cadence, or declare authored safe movement that does not trap
actors. Validation warns when a moving solid wall crosses a door safety zone or
can close every path through a room.

## Validation And Error Handling

Validation runs at asset save, room save, playtest start, project scan, and public
build packaging.

Errors block save or public packaging when they would crash, corrupt, or make
content unusable. Warnings permit save but remain visible.

Required checks include:

- duplicate or malformed IDs;
- unsupported schema versions;
- missing textures, VFX, sounds, projectiles, enemies, assets, or target instances;
- circular asset references;
- invalid colliders, negative sizes, NaN/inf values, and impossible ranges;
- zero-duration unbounded timeline loops;
- damage activation without geometry;
- projectiles without lifetime/range or exceeding hard caps;
- path points outside room bounds;
- moving solids crossing door safety areas;
- objects inside fall/collision/door areas when disallowed;
- invalid prefab overrides;
- content referenced by a room but omitted from the public content manifest.

The Console groups messages by file and object. Selecting a message focuses the
source asset, room instance, component, or field.

## Undo, Recovery, And Migration

Editor changes use command transactions. Dragging a gizmo creates one undo action
from mouse-down to mouse-up, not one action per frame. Multi-object edits are one
transaction. Save does not erase undo history until the document is closed.

Every content type has a schema version and a pure migration chain. Loading an old
file migrates it in memory and reports the version change; the source file is not
rewritten until Robert saves. Existing room versions continue loading while the
new gameplay-object collection defaults to empty.

Crash recovery stores the last autosave separately. On next editor launch, the
user chooses Restore, Compare, or Discard. Recovery never automatically replaces
the valid source document.

## Existing-System Migration

Migration is incremental:

1. Preserve all current handcrafted room files and existing runtime behavior.
2. Add build-profile separation before exposing more developer entry points.
3. Build and test the shared parser, definitions, database, validation, and
   dependency index.
4. Implement one complete gameplay-asset path using a new test Ice Totem.
5. Add Made Assets placement and room serialization.
6. Adapt existing Fire Totem, Lava Pool, and Fireball Torch definitions to the
   new runtime while retaining fallback behavior during transition.
7. Add projectile authoring and migrate environmental FireBolt/IceBolt examples.
8. Add the unified shell around the proven workflows.
9. Add Enemy Inspector common values and migrate one ordinary enemy first.
10. Migrate additional enemies and hazards in small reviewed groups.
11. Remove legacy hardcoded paths only after saved-content migration, runtime
    parity tests, and public-build verification pass.

No phase deletes a working legacy path before its replacement has loaded old
content and passed gameplay parity checks.

## Testing Strategy

### Pure Automated Tests

- parser line/section handling, typed conversion, and diagnostic line numbers;
- definition round-trip save/load;
- atomic-save failure recovery;
- schema migrations from every supported version;
- stable ID rename, duplicate, delete, and dependency lookup;
- prefab inheritance and override resolution;
- behavior timeline transitions, target locking, bounded repeats, reset, and
  invalid-loop rejection;
- trigger activation and one-shot/resettable behavior;
- projectile targeting, lifetime, collision masks, caps, and status payloads;
- gameplay-object pool limits and room cleanup;
- undo/redo transaction correctness;
- RoomBlueprint compatibility with and without gameplay-object instances;
- enemy resolved-value layering and run reset;
- content-manifest completeness;
- build-time assertion that Public Demo has no registered developer states or
  shortcuts.

### Integration Tests

- create, save, reload, place, and playtest an Ice Totem;
- edit its shared Ice Bolt and confirm every reference updates;
- place two instances with different valid overrides;
- update the prefab and confirm inherited fields update while overrides remain;
- transition rooms while moving objects/projectiles/VFX are active and confirm
  complete cleanup;
- load an older room without asset instances;
- enter and exit Play Mode without mutating the editor document;
- pause and frame-step targeting, telegraph, fire, and impact;
- build Developer and Public Demo configurations from a clean checkout;
- verify public main menu and all hidden developer keys are unavailable.

### Manual Acceptance Content

The first vertical slice must support:

- Ice Totem targeting the player, showing VFX, locking aim, firing an animated
  Ice Bolt, applying chill, taking damage, becoming disabled, and recovering;
- Moving Blade following a ping-pong path with a damage collider and telegraph;
- Sliding Wall following an authored path and respecting collision masks;
- a handcrafted room containing all three, saved and reopened without drift;
- a Public Demo build that plays the room but cannot access any authoring tool.

## Performance Requirements

- No per-frame file access or definition parsing during normal gameplay.
- No per-frame heap allocation from behavior timelines, targeting, or component
  dispatch.
- Bounded gameplay-object, projectile, VFX, and emitted-child counts.
- Throttled navigation rebuilds for moving solids.
- Asset thumbnails and editor searches cached outside gameplay.
- Content hot reload available only in Developer builds.
- Public builds may omit editor thumbnails, recovery files, source metadata, and
  developer-only manifests.

## Agent Coordination And Ownership

Robert will not have Claude and Codex edit simultaneously. Work still follows a
handoff-safe process:

1. One committed specification is authoritative.
2. Each implementation phase gets a detailed plan with exact file ownership.
3. The active agent records progress, tests, build result, schema changes, and
   unresolved concerns before handing off.
4. The next agent reads the spec, plan, recent commits, current worktree status,
   and progress record before editing.
5. Central integration files such as `Engine.cpp`, `Engine.h`, project files, and
   `RoomBlueprint` are changed in dedicated integration tasks rather than casually
   during every subsystem task.
6. Existing dirty user changes are never reverted or folded into unrelated
   commits.
7. Every phase ends with focused tests and `Debug|x64`; build profiles additionally
   require Public Demo verification.

## Implementation Projects And Order

The master system is implemented as separate reviewable projects:

### Project 1: Build Profiles And Developer Gating

Create Developer, Public Demo, and Release configurations; define
`MO_DEV_TOOLS`; inventory and gate every editor/debug entry point; verify the demo
contains no hidden access.

### Project 2: Content Core

Build `ContentDocument`, typed definitions, serializers, validators, atomic saves,
Content Database, stable IDs, dependency index, migrations, and tests.

### Project 3: Gameplay Asset Runtime Vertical Slice

Build component definitions, runtime state, deterministic timeline, triggers,
factory/director, collision masks, bounded ownership, and Ice Totem acceptance
content.

### Project 4: Gameplay Asset Editor And Made Assets Shelf

Build asset creation/templates, component Inspector, gizmos, VFX/audio/projectile
selection, validation, save/reload, thumbnails, and placement silhouette.

### Project 5: Room Integration And Play Mode

Extend RoomBlueprint, place instances and paths, support overrides, undo/redo,
temporary play mode, pause/step, cleanup, and migration.

### Project 6: Projectile Authoring

Build projectile definitions/editor/runtime adapters and migrate representative
environmental projectiles without duplicating collision ownership.

### Project 7: Unified Editor Shell

Unify hierarchy, scene, Inspector, asset browser, console, selection, snapping,
multi-select, dependency navigation, autosave, and recovery around proven tools.

### Project 8: Enemy Inspector And Data Migration

Build base definitions, run-tier layering, optional region profiles, postgame
preview, test arena, and staged enemy migration.

### Project 9: Existing Hazard Migration And Legacy Removal

Convert Fire Totem, Lava Pool, Fireball Torch, and suitable future hazards; run
parity/lifecycle tests; remove superseded hardcoded paths only after all content
loads through the new system.

Each project must result in usable, testable software. A later project may depend
on earlier interfaces, but no project may require unfinished editor panels merely
to keep the normal game playable.

## Success Criteria

The design is complete when Robert can create an Ice Totem and Moving Blade in
the Mystic Onslaught Developer build, attach animation/collision/path/targeting/
projectile/VFX/sound/timeline behavior without C++, place them in a handcrafted
room, playtest them with pause and frame-step, save/reopen without data loss, and
ship the same room in a Public Demo that contains no developer access.

The broader system succeeds when common enemy tuning and run-depth scaling are
editable and inspectable, existing hazards have migrated safely, and normal
content iteration no longer requires finding scattered constants in C++.
