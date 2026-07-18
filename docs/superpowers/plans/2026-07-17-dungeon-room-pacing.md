# Dungeon Room Pacing And Capacity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce deliberate dungeon pacing and geometry-aware, cooldown-friendly encounters.

**Architecture:** Add a pure room-capacity analyzer and keep graph pacing in `DungeonGen`; pass both results into the existing `EncounterPlanner`. Runtime `Engine` owns only the current profile, capacity result, and Holdout timer.

**Tech Stack:** C++17, Raylib, assert-based headless tests, MSVC `Debug|x64`.

## Global Constraints

- Preserve handcrafted room selection, room persistence, and existing user changes.
- Do not scale from raw player DPS.
- Never replace reduced population with automatic enemy HP inflation.
- Small rooms must cap specialists and active bodies as well as total population.
- Existing `.mroom` files must remain loadable.

---

### Task 1: Room Capacity

**Files:** Create `TestGame/RoomCapacity.h`, `TestGame/RoomCapacity.cpp`, and `TestGame/RoomCapacityTests.cpp`; modify `RoomLayout.h`, `RoomBlueprint.h/.cpp`, and project/build lists.

- [ ] Write tests for open, constrained, and overridden room capacities.
- [ ] Confirm tests fail because the analyzer is absent.
- [ ] Implement connected-space, spawn-space, and chokepoint analysis.
- [ ] Add version-compatible capacity override serialization.
- [ ] Run capacity and blueprint tests.

### Task 2: Dungeon Pacing

**Files:** Modify `GameTypes.h`, `DungeonGen.h/.cpp`, and `DungeonGenEntranceTests.cpp`.

- [ ] Write failing tests for safe special-room placement and profile variety.
- [ ] Add encounter profiles and graph-depth pacing rules.
- [ ] Verify full connectivity and twelve-room generation remain intact.

### Task 3: Encounter Planning

**Files:** Modify `GameBalance.h`, `EncounterPlanner.h/.cpp`; create `EncounterPlannerTests.cpp`.

- [ ] Write failing tests for Small/Medium/Large caps and bounded ability bonuses.
- [ ] Replace the 8-26 curve with capacity-aware targets and opening caps.
- [ ] Preserve role pressure, hazard costs, reinforcements, and swarm fragility.

### Task 4: Runtime And Editor

**Files:** Modify `Engine.h/.cpp` and `RoomEditor.cpp`.

- [ ] Analyze each entered layout and pass capacity/profile/toolkit data to the planner.
- [ ] Implement the Holdout timer, wave continuation, withdrawal, and objective HUD.
- [ ] Add capacity/profile debug output and the room-editor override control.

### Task 5: Verification

- [ ] Run all focused headless tests.
- [ ] Run `git diff --check` on touched source files.
- [ ] Build `Debug|x64` and require exit code 0.
