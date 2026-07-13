# Mystic Onslaught Region and Sanctuary Traversal

**Date:** 2026-07-13  
**Status:** Approved direction; first implementation uses the existing map as a temporary destination chooser

## Purpose

Replace the concept of one permanent player-built village with a forward-moving chain of handcrafted sanctuaries. Each major region will eventually have its own settlement, layout, population, atmosphere, and progression state. The first implementation does not build those unique layouts yet. It wires the new journey order using the existing fixed village as a temporary sanctuary placeholder and the existing world map as a temporary destination selector.

The player-facing game must never call a location a `biome`. `Biome` remains a valid internal code term. Specific locations are named directly, such as `Forest`, `Caverns`, or `Ancient Castle`. `Region` is used only when the game needs a general term and no specific location is being named.

## Core Journey

The first real run still begins in the Forest sanctuary and proceeds into the Forest dungeon. Every later destination follows this order:

```text
Defeat current boss
-> Poe offers Sanctuary or Double Down
-> Choose the next destination
-> Sanctuary: enter the destination's settlement, then its dungeon
-> Double Down: bypass the destination's settlement and enter its dungeon
```

For the first implementation, the current world-map screen performs `Choose the next destination`. The future physical-road system will replace that screen without changing the surrounding travel state.

## Poe's Offer

Poe appears after a boss and privately presents the choice. Other NPCs do not perceive Poe or react to the conversation.

### Seek Sanctuary

The player selects the next destination on the existing map and enters that destination's sanctuary before its dungeon.

- Fully restore health and mana.
- Allow access to Zeph's travelling-merchant inventory.
- Allow companion selection once the Beast Post exists.
- Allow quests, passengers, NPC dialogue, and settlement services as those systems are added.
- End the current wager streak after any already-earned wager payout has been applied.
- Leaving through the sanctuary's forward exit enters the already-selected destination dungeon. It does not reopen or regenerate the map.

### Double Down

The player accepts Poe's wager, selects the next destination on the existing map, and enters that destination dungeon directly.

- Completely bypass the destination sanctuary.
- Do not restore health or mana.
- Do not expose Zeph, companion replacement, quest boards, passenger delivery, or other settlement services.
- Preserve the selected class, appearance, abilities, relics, resources, and all other run state.
- Mark the upcoming boss as the completion condition for the wager.

Poe's offer should be concise and dry. The intended meaning is:

> "There is a refuge ahead. Beds, merchants, all those little comforts the living cling to."  
> "Reach the next refuge without taking anything from it, and I will double the gold tied to your soul."

The final dialogue can be polished without changing the gameplay contract.

## Wager Calculation

Accepting Double Down does not immediately create gold. The wager succeeds only when the player defeats the boss of the destination whose sanctuary was bypassed.

On success, Poe doubles all gold currently carried by the player, including gold collected while crossing that destination. The multiplication occurs exactly once for that boss clear and before Poe presents the next travel choice.

Consecutive successful wagers stack naturally:

```text
Start with 100 gold
-> bypass sanctuary
-> collect 70 gold
-> defeat boss
-> 170 becomes 340

Accept again
-> collect 60 gold
-> defeat boss
-> 400 becomes 800
```

The HUD displays the cumulative streak as `POE'S WAGER x2`, then `x4`, `x8`, and so on. This label communicates cumulative risk even though each successful boss performs one new doubling of the current carried total.

Entering a sanctuary after a successful wager keeps the doubled gold but resets the wager streak. Dying before completing the wager uses the existing roguelite death rule: carried gold is lost and the wager streak resets. The final boss can complete an active wager before run-victory processing.

The implementation must guard against duplicate payout if a transition, pause, or state reload updates the boss-clear screen more than once.

## Temporary Destination Selection

The current `WorldMapManager` remains in use for this phase. It is explicitly temporary presentation, but it remains the authority for which destinations are currently offered.

The transition order is:

1. Boss clear records the completed location.
2. Poe's choice records either `Sanctuary` or `DoubleDown` as the travel mode.
3. The existing map opens once and the player selects a destination.
4. The selected destination is stored as `pendingRegion` before leaving the map.
5. `Sanctuary` enters the placeholder settlement and keeps `pendingRegion` intact.
6. The sanctuary's forward exit generates and enters `pendingRegion`.
7. `DoubleDown` generates and enters `pendingRegion` immediately.

Returning from the sanctuary must never generate a second set of choices. The selected destination is already committed.

The current map may continue displaying its route graph, but all visible strings should use actual destination names or `region`; no visible label should say `biome`.

## Temporary Sanctuary

Until region-specific sanctuary layouts are authored, every sanctuary visit uses the current fixed village field as a placeholder. The runtime context still knows the selected destination so future layouts can be selected by region without changing travel flow.

The placeholder sanctuary must:

- Remain deterministic and non-randomized.
- Spawn the player at a safe arrival marker.
- Fully restore health and mana only when reached through `Seek Sanctuary` or ordinary death recovery.
- Present Zeph as a travelling merchant rather than requiring a permanent owned shop.
- Use the existing Zeph shop UI and inventory.
- Launch the stored destination from its forward exit.
- Never reopen the world map from that forward exit.

Zeph is guaranteed in every sanctuary. His physical stall, cart, tent, or market position will eventually differ by layout. The permanent Zeph shop building is no longer part of the long-term game design. Developer sandbox tooling may retain the old asset for testing, but the real sanctuary flow must not depend on the building being player-built.

## Future Region Sanctuaries

Each destination will eventually receive a large handcrafted settlement positioned before its dungeon. Settlements grow persistently across runs through authored safety stages rather than player construction.

- **Unstable:** sparse population, closed services, weak perimeter security.
- **Secured:** guards, lighting, additional residents, and opened paths.
- **Thriving:** fuller schedules, market activity, beast companions, advanced services, and ambient events.

The player village-builder system is retired from the intended game loop. Existing authoring and sandbox tools can remain available to developers until a later cleanup confirms they are no longer needed.

## Future Route Selection

The static map will eventually be replaced by three physical exits in each sanctuary or boss-transition space. Roads, gates, boats, lifts, caves, and portals should be geographically appropriate to their destinations.

The internal route generator will still produce a semi-random logical network. It should prevent impossible progression and avoid implausible destination combinations. Players will experience the network through physical exits instead of a detached map screen.

Route displays may eventually show:

- Destination name.
- Route name.
- Danger or hazard emphasis.
- Reward tendency.
- Available side quest.
- Passenger destination.
- Settlement safety state.

This future replacement must consume the same `pendingRegion` and travel-mode interface defined by the temporary map implementation.

## Side Quests and Passengers

These systems are designed now but are outside the first implementation slice.

Settlements can offer forward-only quests tied to currently available destinations. Objectives never require returning to a previous region. Examples include rescuing missing villagers, recovering medicine or tools, defeating optional elites, destroying hazard sources, protecting an evacuation point, and recovering records or relics. Trapped-beast rescue quests are excluded; beast unlocks belong to bestiary research and taming progression.

Some rescued NPCs only want the next safe sanctuary and grant a modest reward on arrival. Others want a specific future settlement and become abstract passengers after their initial rescue encounter. They do not physically follow the player during dense combat.

When a passenger's requested destination is available, it is guaranteed among the route choices. Taking them there grants the delivery reward. Choosing another destination lets the passenger remain safely in the current settlement without rewarding or punishing the player.

## Beast Companions

The Forest sanctuary will eventually contain the primary ranch. Once unlocked, every sanctuary can provide a Beast Post connected to that ranch. The player may choose one available companion before entering a destination.

Defeated companions are not permanently killed. They return to the village, become temporarily unavailable, and leave the player without a companion until another sanctuary is reached. Double Down therefore also risks being unable to replace a defeated companion.

## State Boundaries

Traversal should be represented by focused state rather than inferred from the current screen:

- `TravelMode`: `Sanctuary` or `DoubleDown`.
- `pendingRegion`: selected destination not yet entered.
- `wagerActive`: the next boss can pay the wager.
- `wagerStreak`: consecutive completed Double Down legs.
- `wagerPaidForCurrentBoss`: prevents duplicate multiplication.
- Existing run-session data: completed destinations and map choices.

The current map, sanctuary, dungeon, and boss-choice screens consume these values but do not independently reinterpret them.

## Failure and Recovery

- Invalid or missing destination selection returns to the destination chooser instead of silently loading Forest.
- Missing sanctuary layout loads the current fixed placeholder.
- Missing Zeph placement uses a reachable fallback merchant position.
- Entering a sanctuary with no pending destination provides a safe forward fallback to the next valid route choice rather than beginning a duplicate Forest run.
- Duplicate boss-clear updates cannot multiply wager gold twice.
- Ordinary death clears the wager and returns through the established Poe resurrection flow.
- Debug onboarding skip remains independent from wager and travel state.

## First Implementation Scope

Implement now:

- Poe-themed post-boss Sanctuary versus Double Down choice.
- Existing map retained as the destination chooser.
- Destination stored before leaving the map.
- Sanctuary selection enters the current fixed village placeholder, fully restores resources, and launches the selected destination from its forward exit.
- Double Down bypasses the village, preserves health and mana, and enters the selected destination directly.
- Successful wager doubles all carried gold once and supports consecutive stacks.
- Wager HUD indicator and clear player-facing feedback.
- Zeph treated as a guaranteed travelling merchant in the real sanctuary flow.
- Player-facing terminology cleanup for touched travel screens.

Do not implement yet:

- Three physical route exits.
- Expanded dungeon room graph.
- Unique region settlement layouts.
- Settlement safety-stage visuals.
- Side quests or passenger delivery.
- Beast Post or companion attrition changes.
- NPC schedules and population simulation changes.
- Deletion of legacy builder/editor code.

## Verification

Automated tests should cover:

- Sanctuary selection stores a destination and enters the placeholder village.
- Leaving the placeholder enters the stored destination without reopening the map.
- Double Down enters the stored destination directly.
- Double Down preserves health and mana.
- Sanctuary restores health and mana.
- One successful wager doubles carried gold exactly once.
- Consecutive wagers produce cumulative streak labels.
- Death clears the wager and carried gold under existing rules.
- Map reopening cannot replace an already-selected destination.
- Invalid pending destination returns safely to route selection.

Manual verification should cover:

- Complete Forest, choose Sanctuary, select Caverns, visit the placeholder, and enter Caverns.
- Complete Forest, choose Double Down, select Caverns, and enter Caverns without seeing the placeholder.
- Confirm no recovery occurs during Double Down.
- Complete two consecutive wagers and verify both gold totals and HUD labels.
- Cash out after a successful wager and confirm the doubled gold remains available to Zeph.
- Die during an active wager and confirm the streak and carried gold are lost.
- Confirm the touched UI says destination names or `region`, never `biome`.
