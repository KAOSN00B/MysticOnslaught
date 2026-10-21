# Game flow and arrival polish

Status: proposed revised first milestone for user review.

## Priority and intent

The user reports that the game feels clunky, unfun and as though everything loads at once, especially around dungeon transitions. Make the experience responsive, coherent and paced from village departure through dungeon rooms and back home. This takes priority over expanding biome content. The approved requirement for every authored layout in all ten biomes remains intact.

This is an architectural change spanning resource preparation, state transitions, encounter activation and presentation. Deliver and playtest one complete journey before broadening into a whole-project polish pass.

## Evidence and remaining uncertainty

Source inspection of the current working tree establishes:

- TriggerVillageDungeonGate calls StartMainRun directly. StartMainRun resets player state, prepares the dungeon, loads its tileset and enters its first room before assigning its final fade-in timer.
- UpdateWorldMap similarly loads the chosen biome and enters its dungeon before starting the dungeon fade-in.
- LoadDungeonVisualVariantAssets initializes a renderer and calls LoadRoomAssetCatalog. TileRenderer::Init unloads/reloads its base sheets; LoadRoomAssetCatalog unloads its sources and loads every catalog sheet.
- Room scrolling prepares the next layout, and sometimes its renderer, synchronously before activating the scroll. EnterDungeonRoom runs when the scroll completes.
- The dungeon fade invokes its pending action during Update as soon as alpha reaches black. It does not explicitly wait for that fully black frame to be presented. Its fade-in branch permits normal gameplay updates.
- EnterDungeonRoom sets up navigation, enemies, modifiers, banners and cutscene triggers in one call.
- AudioManager::StartMusicCue stops the old cue immediately before starting the new one. A new loop fades in, but this is not a crossfade.
- A prior August loading-overlay design describes a presentation barrier; the inspected transition paths still contain the behavior above.

These findings identify concrete sequencing and resource-management risks. No interactive reproduction or frame-time capture has yet established their individual contribution to the reported feel. The implementation begins with baseline traces rather than claiming a measured speedup.

## Approaches

1. Add fades around current calls: small scope, but leaves synchronous work and overlapping arrival events largely intact.
2. Recommended: coordinate transitions, stage preparation, reuse required assets and pace arrival. Addresses both responsiveness and how the journey feels, while keeping the existing game and authored rooms.
3. Replace the engine with a general streaming scene framework: much larger scope and regression risk than this milestone requires.

## Player experience

Village departure gives immediate feedback at the gate, carries the player into a short departure transition and reveals the destination once it is ready. The camera and player are already correctly placed when the room becomes visible. A restrained biome title accompanies first arrival; HUD, dialogue and encounter messaging do not compete for attention.

Ordinary room doors retain the existing directional travel idea with a consistent eased motion and an initial target of roughly 0.3–0.4 seconds. Revisited safe rooms need no ceremonial pause. Preparation must finish before the visual move starts; no mid-scroll layout swap or blocking resource load is intended.

Biome travel and returning to the village use a short cover/reveal sequence. Start with approximately 0.2 seconds to cover and 0.3 seconds to reveal, then tune in the playable build. Longer actual preparation may extend the covered phase with a subtle activity indicator. No invented percentage and no forced minimum loading delay.

On arrival, allow orientation before combat pressure. Combat, damaging hazards and relevant gameplay timers remain paused until the destination is visible and control is handed back. Then use the existing enemy telegraphs. Do not make every ordinary room wait through a new title or cutscene.

Music transitions blend appropriate looping cues while keeping both streams serviced. Existing victory, boss and dialogue cues retain their special rules. A reduced-motion presentation avoids sweeping camera travel while preserving readiness and input guarantees.

## Implementation boundaries

- A small transition coordinator owns departure, presented cover, preparation, destination commit, reveal and handoff. It runs above individual game states so switching state cannot drop the transition. Requests execute once; duplicate activation cannot start another run or apply progression twice.
- Preparation uses bounded stages between rendered frames. Keep GPU resource creation on the rendering thread. Time each stage, split large batches and retain indivisible work in the covered phase. Do not claim asynchronous loading by moving one unchanged blocking callback behind a spinner.
- Collect required sheets from the actual room layouts, definitions, props and animation references. Reuse loaded textures with explicit ownership across active and incoming renderers. Retain only the needed working set; invalidation for editor changes and unload/shutdown behavior remain defined. Avoid whole-catalog reloads for a door crossing.
- Stage destination state before committing progression and discarding the source. Missing content or failed preparation returns to a valid source/menu state with a useful message. It must not spend rewards, advance the world path or strand the player behind black.
- Separate room preparation from encounter activation. A single handoff condition controls combat, hazards, input and announcement scheduling. Flush the triggering action so a menu confirm cannot become an attack or another travel request; refresh held movement deliberately when control resumes.
- Keep the existing simulation rules, rewards, authored geometry and full-run progression. Target only refactoring needed to implement these boundaries safely.

## Delivery and verification

1. Capture baseline transition stage timings, frame times and texture-load counts for new run, world-map travel, ordinary door, visual-variant boundary, boss exit and village return. Distinguish cold and warm paths and native/web behavior.
2. Implement and test transition sequencing, presentation acknowledgement, exact-once execution, failure recovery and input/encounter handoff.
3. Implement required-asset preparation/reuse and verify lifetime safety. Warm traversal with all required assets resident must issue no redundant texture loads.
4. Integrate village departure and biome selection first; extend the same rules to room traversal, boss exits and returns without altering run economy.
5. Add arrival pacing and audio blending, then package a separate playable candidate that preserves the existing working build and user saves.
6. Compare baseline and candidate on the same routes. Report maximum and percentile frame times, stage stalls, resource loads and remaining long operations. Do not mark the loading problem resolved solely because black hides it.

Acceptance includes: no destination input or damage before handoff; no source-frame freeze before departure feedback; no unprepared destination shown; no repeated progression changes; correct music and state recovery after failure; stable repeated travel without texture lifetime errors; and no new mandatory waits on familiar paths. Automated tests cover state sequencing and resource ownership; native and web smoke/playtests cover rendering, audio and actual input behavior.

The user must assess whether arrival feels smoother and repeated traversal stays snappy. Technical tests cannot certify fun. Record outstanding perceptual issues and iterate on the candidate before declaring this milestone complete.

## Following work

After this journey is coherent, apply the same pacing principles to combat/rewards, menus, death/retry and onboarding. Then expand replayability and complete the approved biome variants. Preserve the original full-title objective throughout; this milestone is the first slice, not a claim that transitions alone make the game shippable.
