# Character Animator (dev tool)

Main menu, the Character Animator button (or H / K). Everything it saves is a
plain text file next to the exe (the `TestGame` folder in dev builds) that the
game reads when a character spawns. Hover any control in the tool for help.

## What it edits

| Where in the tool | What the game does with it | File and keys |
|---|---|---|
| Size (x hero) | draws the character at exactly that size | `charactertuning_<Name>.txt`: `scale`, `size_set=1` |
| Speed | frame time per animation | `anim_frametime_<i>` |
| Sprite mode | draw offset per animation (picture only) | `anim_<i>_draw_x/_y` |
| Body mode, cyan circle | blocks movement; its square is what gets hit | `anim_<i>_body_x/_y/_r` |
| Body mode, yellow dashed box | what the player's attacks hit: the body square plus padding | `hurt_pad_x/_y` |
| Melee mode, red box | the melee swing's box, per attacking animation | `anim_<i>_melee_x/_y/_w/_h` |
| HIT FRAMES lane | the frames that box is live (first to last) | `anim_<i>_hit_start/_end` |
| EVENTS lane / EVENTS tab | an FX, the attack sound or a shake on a frame | `anim_<i>_event_<n>=kind:frame:x:y:size:fx` |
| MOVES tab | a boss/elite special move's hit area | `attacktuning_<Owner>_<Move>.txt`: `hit_area=1`, `hit_*` |
| MOVES tab, TIMING rows | elite signature timing | same file: `sig_*` |
| ATTACK tab (Player) | fire point, FX origin, projectile, ability lab | `attacktuning_<Class>_<Ability>.txt` |

All boxes are authored facing right, relative to the character's position, and
mirror with it in game. `i` is the animation's slot in the tool's list.

## Rules worth knowing

- **Nothing changes until you save.** A character without a file plays with its
  code defaults; a move without `hit_area=1` keeps its coded size (the Attack
  Library's older `box_*` values are not used for boss moves).
- **Size.** Standard enemies are sized to the hero's height automatically
  (`Balance::Visual`). Saving a size different from that writes `size_set=1`;
  *Auto size* goes back. Hitboxes, offsets and events scale with the Size control.
- **Hit windows.** Unset, each class keeps its coded frames: the grunt strikes on
  its third frame, the hero on frames 3-5, bosses from the fourth frame to the end.
  A set window stays live every frame until the swing connects or the window
  ends (a coded boss swing tests once).
- **Melee boxes** exist only on animations the game reads one from
  (`Enemy::GetDefaultHitWindow`). An attacking animation without its own box
  uses the attack animation's.
- **Events** fire as the animation reaches their frame (a long frame step never
  skips one). FX sheets are `PowerUps/FX_<name>.png` in 64 px cells.
- **Special moves** draw their warnings from the same value the damage uses, so
  what is warned is what hits.
- **The hero** is everyone's size reference, so its size is fixed. In game its
  hurtbox is the Idle body circle's square (else Walk's), with no padding.

## Code map

- Tool: `CharacterAnimator.cpp/.h`. Formats: `CharacterTuning.cpp/.h`,
  `AttackTuning.cpp/.h`, `TuningFiles.h` (folder), `EliteMoveDefs.h` (elite
  timing table and zone sizes).
- Game: `Enemy::ApplyStoredTuning` (spawn), `Enemy::InHitWindow`,
  `MeleeWindowLive`/`MeleeCheckDone` (boss swings), `Enemy::CollectAnimEvents`
  (fired in `CombatDirector`'s enemy loop, played by `Engine::PlayAnimEvent`),
  `Enemy::TunedMoveBox` and each boss's `ApplyMoveTuning`/`GetEditorMove`.
  The hero: `Character::CanApplyMeleeDamage`, `MeleeSwingSpent`, `CollectAnimEvents`.
- Tests: `CharacterTuningTests.cpp` (formats, event timing) and flow phase 8
  (every character opens; size, undo, the unsaved prompt, the lineup; hit
  windows and events; an AncientBear saved in a sandbox folder and respawned).
