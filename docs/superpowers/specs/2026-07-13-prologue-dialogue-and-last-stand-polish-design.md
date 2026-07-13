# Prologue Dialogue and Last-Stand Polish

**Date:** 2026-07-13  
**Status:** Approved direction

## Scope

Polish only the existing one-time Forest prologue. Do not change ordinary Skeleton Archer balance, later dungeon encounters, the region-traversal work, village layouts, shop wares, or unrelated combat systems.

## Natural Tutorial Prompts

Tutorial prompts remain neutral contextual guidance. Poe does not speak before the scripted death, and no unseen narrator is introduced.

The prompts use the player's active bindings and actual move names:

- Basic room: `They're coming. [ATTACK INPUT] - [BASIC ATTACK NAME].`
- Ability room: `Use what you know. [ABILITY INPUT] - [ABILITY NAME].`
- Dash step: `Move! [DASH INPUT] to dash through danger.`
- Last stand: `There are too many. Keep moving.` followed by the existing three-point story-health display.

Basic attack names are class-aware:

- Mage: `Arcane Staff Bolt`
- Warrior: `Sword Slash`
- Hunter: `Bow Shot`
- Rogue: `Dagger Strike`
- Paladin: `Shielded Slash`
- Warlock: `Shadow Staff Bolt`

The implementation should expose these names through one helper rather than embedding separate prompt strings throughout the draw code.

## First Poe Conversation

The first resurrection conversation grows from three lines to five. It remains quick and preserves Poe's established motive.

1. `Hm. You're stronger than you look. Could've been stronger, with proper guidance.`
2. `Guess you'll do.`
3. `These monsters have kept me very busy. Every body they leave behind becomes my problem.`
4. `I can bring you back. In exchange, you're going to thin their numbers for me.`
5. `Do try to last longer next time.`

The player advances each line using the existing keyboard, mouse, or controller input. The debug onboarding skip continues to bypass the entire conversation safely.

## Dialogue Layering

Dialogue is a foreground overlay and must render after gameplay HUD elements.

- Zeph's first-village dialogue draws after the village HUD, prompts, build/debug UI, player, and overhead world layer.
- Dungeon cutscene dialogue draws after ordinary HUD, banners, minimap, modifiers, and debug gameplay panels.
- Transition fades may still cover dialogue when intentionally leaving a state.
- Poe's death conversation remains a dedicated full-screen state and therefore already satisfies the foreground requirement.

Dialogue panels retain their dark opaque background, border, wrapped text, and input hint so text remains readable over every scene.

## Relentless Last-Stand Archers

Only Skeleton Archers spawned for the prologue's final room receive a relentless-fire profile.

- Opening shots remain slightly staggered so every projectile does not occupy the same frame.
- Draw duration is shortened enough to maintain constant pressure while preserving a readable bow animation.
- Recovery between shots is nearly immediate.
- Tutorial archers may begin drawing whenever the player is in range, even when navigation would normally delay their line-of-sight decision.
- They remain stationary while drawing and releasing, preserving the surrounding firing-ring composition.
- Existing enemy-projectile caps remain active so the encounter cannot create an unbounded projectile collection.
- Normal Skeleton Archers retain their current draw duration, cooldown, kiting, and encounter balance.

This behavior should be an explicit per-instance mode set immediately after tutorial spawn. It must reset to normal whenever an archer instance is initialized or reused.

## Verification

- Each starter class produces the expected basic-attack name.
- Tutorial prompts use current input bindings and do not contain the old robotic wording.
- The five Poe lines advance and complete the existing resurrection transition.
- Zeph dialogue visibly covers the village HUD instead of being covered by it.
- Cutscene dialogue renders above gameplay UI.
- All final-room tutorial archers repeatedly fire with staggered openings.
- Ordinary Skeleton Archers retain their current cadence.
- The projectile cap remains enforced during the last stand.
- Debug onboarding skip still reaches the ready village.
- `Debug|x64` builds successfully.
