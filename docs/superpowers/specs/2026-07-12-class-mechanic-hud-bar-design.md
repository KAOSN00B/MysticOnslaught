# Class Mechanic HUD Bar Design

## Goal

Make class-specific combat mechanics immediately readable without adding more
top-screen clutter. Warrior, Rogue, Hunter, and Paladin receive one consistent
mechanic bar above the ability EXP bar, directly to the right of the Armour
icons. Mage and Warlock remain unchanged because they do not currently have a
single accumulating class resource that belongs in a progress bar.

## Placement And Presentation

The bar lives in `Engine::DrawAbilityBar`, sharing the bottom HUD coordinate
system with Armour and EXP. Its left edge begins after the final possible Armour
icon and its right edge is constrained to the EXP/ability-bar width. It must not
overlap Armour, the EXP label, ability slots, touch controls, or the edge of the
virtual canvas.

The bar uses a dark near-black backing, a strong class-coloured border, and a
class-coloured progress fill. Its label is centred white text with a black
four-direction outline so it remains readable over both empty and filled parts.
The minimum font size is large enough to read during combat. A full or primed
bar receives a restrained pulse rather than extra explanatory text.

## Class Data

- Warrior displays `RAGE  <percent>%`, using the existing Rage percentage and an
  orange-red fill. Full Rage pulses.
- Rogue displays `COMBO  <current> / <maximum>`, filling one segment-equivalent
  fraction per combo point with a crimson fill. A full combo bank pulses.
- Hunter displays `MARK  <shots> / <shots required>`, using the existing landed
  basic-shot cadence. It fills after each landed shot, resets when a mark is
  applied, and flashes when the next landed shot will mark its target.
- Paladin displays `FAITH  <percent>%`, using the existing Faith percentage and
  a warm gold fill. Full Faith pulses.

The Hunter value remains an Engine-owned run counter because that is where
landed Hunter shots and mark application are currently resolved. No new Hunter
resource or balance behavior is introduced.

## Cleanup And Scope

Remove the old Warrior Rage, Rogue Combo, and Paladin Faith indicators from
`HUDRenderer.cpp` after the replacement is active. Do not add a Mage or Warlock
bar, change class mechanics, alter resource gain, modify Armour placement, or
change ability aiming behavior in this phase. The separate Press / Press aiming
default remains its own approved control change.

## Verification

- Each supported class shows exactly one mechanic bar beside Armour.
- Hunter progress increments only on landed basic shots and resets on marking.
- Full/primed states pulse without obscuring the label.
- White outlined text remains readable at empty, partial, and full fill.
- Mage and Warlock show no mechanic bar and no empty reserved panel.
- The old top Rage, Combo, and Faith indicators no longer render.
- Armour, EXP, and ability slots remain unobstructed at supported resolutions.
- `Debug|x64` builds successfully.
