# Mystic Onslaught Visual VFX and Dungeon Border Design

## Purpose

This project completes two visual-foundation tasks before the remaining UX backlog:

1. Replace active gameplay prototype shapes with animated VFX while preserving readable targeting telegraphs.
2. Replace the incomplete dungeon-variant experiment with an editor-driven variant, border-brush, and room-bounds workflow.

The work must preserve existing gameplay balance, user-authored tuning, village/editor changes, save identifiers, and unrelated systems.

## Project Order

### Project A: Active Gameplay VFX

Complete and verify the active-attack VFX pass first. This gives every class, enemy, boss, and environmental hazard a consistent rendering path before dungeon presentation is expanded.

### Project B: Dungeon Variants and Borders

After Project A builds and passes visual checks, implement editor-managed dungeon variants, semantic border brushes, and independent room collision bounds. Remove the Ancient Castle/Dream Realm shortcut and author genuine same-biome alternatives where suitable art exists.

### Later UX Work

Combat HUD and status UX, boss presentation, dungeon-flow polish, village/builder improvements, menus/onboarding, and audio remain subsequent phases. They are not bundled into these first two projects.

## Primitive Shape Policy

Raylib circles, rectangles, lines, ellipses, and triangles remain allowed for:

- UI panels, meters, icons, cooldown masks, and minimaps.
- Debug overlays, hitboxes, colliders, handles, and editor guides.
- Shadows beneath characters and enemies.
- Pre-attack targeting telegraphs, including AOE warnings, cones, charge lines, landing markers, and player aiming reticles.
- Invisible collision and damage calculations.

Primitive shapes must not be the final visible effect for:

- Active projectiles, beams, slashes, strikes, explosions, or impacts.
- Persistent attack zones, pools, storms, walls, traps, or auras.
- Applied status visuals such as poison, bleed, burning, freeze, shock, stun, curses, or armour break.
- Environmental hazards after their warning phase becomes active.
- Ability activation, travel, contact, and expiration effects.

A telegraph disappears or transitions when the attack activates. The active attack then uses animated sprite VFX. Gameplay geometry remains independent from artwork so changing VFX scale cannot silently change damage or collision.

## VFX Asset Policy

Use existing project assets first. When a move lacks a suitable animation, search the user's previously supplied owned VFX libraries and copy only the selected effect into the project. Preserve pixel-art filtering and transparent backgrounds.

If no exact animation exists, use the closest semantically appropriate effect with tinting, layering, rotation, or compositing. Do not leave an active move as a prototype shape merely because the source library lacks a perfectly named effect.

Every imported effect must have a clear project filename, ownership purpose, expected frame dimensions, frame count, playback rate, and loop mode. Avoid loading duplicate textures for effects that can share one resource.

## VFX Runtime Architecture

Extend the existing VFXManager and attack-tuning path rather than adding isolated per-ability drawing code. A gameplay effect definition needs:

- Texture asset and animation cell dimensions.
- Frame count and frames per second.
- One-shot or looping playback.
- World-space offset from caster, target, projectile, or impact.
- Scale, rotation, tint, and draw layer.
- Direction and optional travel behavior.
- Lifetime and fade-out behavior.
- Follow-caster, follow-target, or fixed-world anchoring.

Abilities with multiple stages may define cast, travel, impact, lingering, and expiration effects independently. Missing assets must fail safely without crashing, while debug mode reports the missing effect clearly.

## Animator Integration

The combined character/attack animator is the tuning authority for player abilities. Where applicable, it must expose:

- Effect origin relative to the selected character appearance.
- Projectile fire point and travel path.
- Scale and collision visualization as separate controls.
- Rotation, tint, frame rate, frame count, loop mode, and lifetime.
- Cast, projectile, impact, and persistent-zone stages.
- A real-time cooldown playback test.
- Class, hero appearance, and ability selection.

Boss and enemy projectile attacks must remain visible in their existing attack editor and use the same saved runtime tuning data. Editor previews must use the same renderer and transforms as gameplay.

## VFX Audit and Acceptance

Every primitive draw site is classified before replacement. The audit records its owning move/system, whether it is allowed by policy, the replacement asset, and its editor/runtime path.

Acceptance requires:

- No active move or active environmental hazard relies on a primitive shape as its final VFX.
- Telegraph timing and gameplay hitboxes remain unchanged unless a separate gameplay change is explicitly approved.
- Persistent effects loop for their complete gameplay duration and terminate cleanly.
- Effects are readable at the game's normal camera scale and do not obscure enemy warnings.
- Debug and editor previews match runtime position, scale, timing, and direction.

## Dungeon Variant Model

A dungeon visual variant belongs to one gameplay biome. It changes presentation only; enemies, rewards, hazards, biome identity, and progression remain unchanged.

Each variant stores:

- Stable identifier and player-facing editor name.
- Source tilesheet and tile-mapper assignment.
- Selection weight.
- Minimum and maximum connected wing size.
- Floor, wall, door, decor, prop, animated-prop, and border-brush definitions.
- Four independent room collision bounds.

The base variant always exists and cannot be deleted. Additional variants can be created, duplicated, renamed, enabled, disabled, reordered, and removed in TileMapper.

## Variant Assignment Rules

Generated rooms are grouped into coherent connected wings. A variant may not change randomly in the middle of one visual wing.

When at least two variants are enabled:

- The Dungeon Run tester guarantees every enabled variant appears when enough rooms exist.
- Adjacent wings avoid repeating the same variant when another enabled choice exists.
- The real run respects weights but still avoids producing an entire dungeon with no visible variety when multiple variants are enabled.
- Special rooms may override a wing only when explicitly authored.
- Missing or malformed variant data falls back to the base variant and emits a debug warning.

Ancient Castle must use genuine grey and purple castle mappings from appropriate castle artwork. Dream Realm must not be used as a substitute for purple castle art. Dream Realm receives an alternate only if its owned tilesheet contains a coherent second visual set.

## TileMapper Variant Workspace

Add a `Variants` workspace to TileMapper. It provides:

- Variant list for the selected biome.
- Create, duplicate, rename, enable/disable, reorder, and delete controls.
- Tilesheet and mapper selection.
- Weight and connected-wing-size controls.
- Immediate room preview using the selected variant.
- Validation messages for missing required mappings or assets.

Switching variants must not discard unsaved changes. Saving writes the complete biome variant definition through the editor; hand-editing a hidden text file is no longer the primary workflow.

## Border Brush System

Add a `Borders` workspace for visual edge authoring. The selected variant defines independent brushes for top, bottom, left, right, and four corners.

A brush may contain:

- Several weighted tiles, static props, or animated props.
- Common, uncommon, and rare visual pieces.
- Minimum spacing and cluster spacing.
- Ground, object, or overhead draw layer.
- Side-specific offset and depth.
- Mandatory authored stamps at selected positions.
- Door-gap exclusion and padding.

The preview generates a complete 28-by-16 room using a selectable seed. The user can regenerate the preview to inspect variety without leaving TileMapper.

Forest borders may use dense tree trunks, roots, rocks, bushes, and overhead canopies. Ancient Castle borders use masonry, columns, broken walls, or castle-appropriate decoration. Other-biome assets may be reused only when they make semantic and visual sense. Border walls and structural corner art never leak between unrelated biomes.

## Layering

Border pieces support three layers:

- Ground: roots, cliff lips, floor transitions, and low debris.
- Objects: trunks, walls, pillars, and collision-level structures.
- Overhead: canopies, arches, tall wall caps, and foreground pieces drawn above actors.

Door openings cut all blocking object pieces and inappropriate overhead pieces from the configured gap. Decorative ground pieces may continue into the doorway only when they do not obscure navigation.

## Room Bounds Workspace

Add a `Bounds` workspace to TileMapper. It shows four labelled draggable guides over the generated room:

- TOP
- BOTTOM
- LEFT
- RIGHT

Each guide supports direct dragging, numeric values, one-pixel and one-source-tile nudging, and reset to the standard one-tile room boundary. A player-sized collision capsule can be moved around the preview to test the result.

Bounds are collision-only and are stored independently for every biome variant. They do not automatically move the visual border. This allows trees, cliffs, canopies, and tall walls to overlap the playable boundary naturally.

Door openings are computed from the authored door mapping and remain passable through the corresponding boundary. Runtime player clamping, enemy clamping, navigation-grid construction, projectile world limits, prop placement margins, and valid spawn calculations all consume the same resolved room bounds.

## Dungeon Run Tester

The main-menu Dungeon Run tester becomes the integration preview:

- The graph labels or color-codes every room by assigned variant.
- Clicking a room previews its generated tiles, props, borders, bounds, and door gaps.
- `V` cycles the current room through enabled variants in static and playable views.
- `V` is available in the main-menu tester and when general debug mode is active, but not during an ordinary player run.
- Cycling shows a visible temporary `Variant: <name>` message without requiring the debug panel.
- Debug mode can display collision bounds and border-brush placements.
- Regenerating the dungeon guarantees enabled variants appear for inspection.

The tester and real runtime must use the same generation, renderer, bounds, and collision data.

## Data and Compatibility

Existing `tilemapper_<Biome>.txt` files remain valid as base variants. If no variant metadata exists, runtime synthesizes one base variant and preserves current behavior.

The editor may introduce a versioned companion format or extend the existing structured mapper format. Parsing must ignore unknown future fields, validate dimensions and weights, and fall back safely on malformed entries.

Web and Windows packaging must include all variant definitions and selected tilesheets. The current Ancient Castle metadata omission from web preloads must be corrected as part of implementation.

## Error Handling

- Missing variant configuration: synthesize the base variant.
- Missing alternate tilesheet or mapper: disable that variant for generation and display an editor warning.
- Invalid bounds: clamp to a valid ordered rectangle with at least one walkable source tile on each axis.
- Invalid door gap: restore the default centred gap for that side.
- Missing border asset: skip that stamp and continue generating the remaining border.
- Empty brush: use the variant's mapped wall tile.
- Runtime load failure: retain or reload the base variant rather than leaving the renderer empty.

## Testing

Automated or deterministic tests should cover parsing, fallback behavior, weighted assignment, guaranteed tester coverage, non-repeating adjacent wings, bounds validation, door-gap cutting, and seeded border generation.

Manual visual verification must cover:

- Every enabled biome variant in static and playable Dungeon Run views.
- `V` cycling and the visible variant message.
- Forest tree borders with correct ground/object/overhead ordering.
- Ancient Castle grey and purple variants.
- Player, enemy, navigation, projectile, and spawn behavior at custom bounds.
- Door transitions on all four sides.
- VFX cast, travel, impact, lingering, and expiration stages for every active move.
- Desktop and controller interaction where relevant.

Each implementation project ends with a fresh `Debug|x64` build. Web packaging is verified after variant data files are added to preload inputs.

## User Workflow

To adjust a dungeon after implementation:

1. Open TileMapper from the main menu.
2. Select the biome.
3. Open `Variants` and select or create the visual variant.
4. Map floor, wall, door, prop, and decor art for that variant.
5. Open `Borders`, assign side and corner brush pieces, set layers and spacing, and regenerate the preview until it reads correctly.
6. Open `Bounds`, drag TOP, BOTTOM, LEFT, and RIGHT to the desired playable edges, then test with the player capsule.
7. Save the biome variant.
8. Open Dungeon Run, select the biome, inspect room labels, click rooms, and use `V` to cycle variants while testing movement and doors.

This workflow is the acceptance standard: no hidden configuration should be required for ordinary visual authoring.
