# Press / Press Ability Aiming Design

## Goal

Make aimed abilities use a two-press interaction by default. The first press of
an ability button enters aiming and the second press of that same ability button
confirms and casts it. This behavior must be consistent for keyboard, mouse HUD
buttons, touch controls, and gamepad controls.

## Behavior

- Instant abilities continue casting on their first press.
- Directional and ground-targeted abilities enter aiming on their first press.
- Pressing the same ability slot again confirms the current direction or target.
- Pressing a different ability cancels the previous aim and begins aiming the new
  ability if it can be cast.
- Basic Attack does not confirm an aimed ability and retains its normal action.
- Right mouse button and the existing cancel control continue cancelling aim.
- Mana and cooldown are consumed only when the aimed ability is confirmed.
- The player-facing aiming hint reads `PRESS AGAIN TO CAST` in this mode.

## Settings And Compatibility

`Press / Press` becomes the default for new or missing settings files. The
existing `Hold / Release` option remains available in Controls and existing
players who explicitly saved that option keep their preference. The serialized
`ability_aim_toggle` key remains unchanged, avoiding save or settings migration.

## Implementation Scope

Change the default value in `SettingsManager.h` and verify the existing Engine
state machine already satisfies the interaction above. Add a focused regression
test around the default setting where practical, then build `Debug|x64`. Do not
change ability timing, aim geometry, movement scaling, input bindings, combat
balance, village systems, or editor systems.

## Verification

- With no saved override, directional and ground-targeted abilities use two
  presses of the same ability button.
- Instant abilities still fire immediately.
- Keyboard and gamepad use identical confirmation behavior.
- Hold / Release can still be selected and saved from Controls.
- Basic Attack does not accidentally confirm a pending ability.
- Cancelling aim consumes neither mana nor cooldown.
- `Debug|x64` builds successfully.
