# Shipping foundation implementation plan

> Use executing-plans for integrated game-flow work and dispatching-parallel-agents for isolated progression, audio and content tooling. User's instruction to "just make" authorizes execution and routine design decisions without additional approval gates.

**Goal:** Deliver a tested playable full-game foundation with coherent travel, persistent goals, music continuity and an honest biome coverage pipeline.

**Architecture:** Keep the raylib game, existing art and authored rooms. A frame-driven transition coordinator covers expensive actions and gates simulation until arrival; renderer reuse removes repeated catalog uploads. Persistent class/biome achievements feed the existing unlock economy. Isolated modules get behavioral tests before integration.

**Tech stack:** C++17, raylib 5.5, MSVC, Python content tools.

**Specs:** ../specs/2026-09-23-game-flow-and-arrival-design.md and ../specs/2026-09-23-all-biome-room-variants-design.md.

## Constraints and execution record

- Preserve current uncommitted work and all 55 source rooms. No hit-flash changes, purchased assets, publication or monetization transactions.
- Work in the existing feature checkout with per-file backups and a saved baseline diff; a HEAD-only worktree would omit the active uncommitted game. Package separately with its own saves.
- User authorized implementation directly. Proceed without repeated planning handoffs.
- Progression/audio/content agents own disjoint files; root alone edits Engine, rendering and build integration.
- A build is not proof of fun or release readiness. Record remaining full-title work explicitly.

## Review focus

1. Confirm/held input must not leak into destination actions; duplicate travel must not double-award progression.
2. A fully covered frame must actually be presented before expensive work; simulation must stay paused during reveal.
3. Texture reuse must not double-free or conceal edited assets.
4. Older/corrupt saves, repeated boss visits and overflow counters must preserve earned progression without duplicate rewards.
5. Demo flags, runtime content paths and packaging must not silently disable the new progression.

## Task 1: Travel coordinator and integration

- [ ] Add `TestGame/JourneyTransition.h` and `JourneyTransitionTests.cpp`. Interface: Begin(label, action), Update(dt), Presented(), Active(), Alpha(), Label(). Cover -> await presented frame -> execute once -> reveal -> idle. Reject nested requests and non-finite frame deltas. Test no execution without presentation, exactly once, duplicate requests, failure recovery and reveal gating.
- [ ] Run failing standalone MSVC test, implement state machine, run passing test.
- [ ] Integrate above game-state dispatch in Engine Update and after scene rendering; acknowledge presentation after EndDrawing. Queue village/new-run/world-map arrivals; route existing dungeon fade callbacks through coordinator; keep quick directional room travel.
- [ ] Add clear destination title/activity presentation, defer combat/UI timer work until handoff, and clear stale action inputs. Log action durations; do not claim one blocked callback is streaming.

## Task 2: Renderer reuse

- [ ] Add stub-raylib resource tests counting loads/unloads across repeated Init/LoadRoomAssetCatalog calls and invalidation.
- [ ] Keep resident unchanged base sheets and catalog textures. Compare normalized source paths and file modification stamps to reload changed editor content. Retire removed sources and unload each owned texture once.
- [ ] Run new resource lifecycle tests plus existing source-selection tests.

## Task 3: Persistent journeys

- [ ] Progression agent implements tested finite mastery and biome milestones, backwards-compatible saves and getters.
- [ ] Root records earned combat room/boss clears only outside debug/editor runs; presents a journal reachable in village with progress, next rewards and existing assets. Connect banked-cell milestone rewards to existing altar economy.
- [ ] Ensure a full-game candidate configuration enables persistence and keeps demo behavior separately available.

## Task 4: Audio and content

- [ ] Audio agent implements tested loop blending and preserves victory/paused cue rules.
- [ ] Content agent delivers reproducible inventory/generation tools with original hashes, exact room-biome coverage and unresolved mapping diagnostics. Only fully mapped variants are eligible for use.

## Task 5: Candidate and verification

- [ ] Run targeted standalone tests and build native full-game candidate. Compile relevant web-compatible modules where available; never substitute native success for browser playtest.
- [ ] Package existing assets, .mroom and metadata into a separate folder; omit personal progression/settings from the fresh candidate.
- [ ] Review integrated changes independently, fix important findings and run relevant checks.
- [ ] Deliver launcher, validation report and exact remaining release blockers; keep full-title roadmap open.

## Progress (2026-09-23, Claude Code continuation)

- Task 1 done: journey transitions integrated; failures now recover to the source state (world map / village) instead of the main menu. Trace: `JOURNEY [label]: preparation stage N ms, K tile-sheet uploads`.
- Task 2 done: `TextureCache.h` (ref-counted, keyed by path + mod time) backs every TileRenderer sheet and catalog source; TileRenderer is non-copyable. Warm traversal performs zero tile-sheet uploads (TextureCacheTests).
- Task 3 done: room/boss clears record mastery and biome first clears (real runs only, not editor playtest/prologue/debug). Ordinary room clears defer the disk write (`SaveIfDirty` on village arrival and shutdown). Altar has a Journal page (TAB / gamepad Y).
- Shipping|x64 configuration added (MO_ITCH_DEMO=0, MO_DEV_TOOLS=0, Windows subsystem). Debug/Release/PublicDemo unchanged.
- `tools\test_shipping.cmd` runs all four suites (journey, texture cache, audio, meta).
- Task 4 content: NOT started beyond the audit. Sheets differ structurally (e.g. Forest = grass/trees, Ancient Castle = masonry), so per-tile mapping needs an art-direction decision; also only Forest uses handcrafted rooms at runtime.
- Not yet verified: interactive playtest, web build.

## Continuation — September 24

- Preserved all uncommitted authoring work; no commits.
- Seven-suite shipping runner passes. Full native Shipping build succeeds.
- Added separate per-user Shipping profiles, failed-save retry/backup repair checks, final-boss completion routing and duplicate-victory persistence regression.
- Added build/package/smoke scripts; package launches from an unrelated directory with an isolated profile and closes normally. Missing legacy arena maps now fall back to supplied Map.png after a reproducible startup hang.
- Added default Mage dash fallback and centralized Mage damage constants. Opening check covers Lightning Blink overlap; broader gameplay balance still needs playtesting.
- Installed 23 native-validated Forest-layout Jungle variants under Rooms/Generated; enabled authored Jungle rooms in full-game travel. Original 55 room hashes preserved. Validated 1000 seeded authored dungeons (worst 29/600 attempts), all 78 native room loads and native preview rendering.
- Ruling: retain the existing graph retry policy for directional treasure rooms. Requiring every arbitrary graph to fit would reject the original Forest library as well; actual generation succeeds within the existing limit.
- Remaining content: 472 layout/biome pairs. Full release status and unresolved validation are in docs/RELEASE_CANDIDATE_STATUS.md. This is a development candidate, not a claim that the complete title is ready to sell.
