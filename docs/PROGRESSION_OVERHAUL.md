# Mystic Onslaught: progression overhaul (design)

Agreed in discussion with Robert, 2026-10-17. Nothing here is implemented yet. Numbers are placeholders: balance and values are Robert's call. Wherever possible, content and tuning live in data tables so they can be changed without code.

## Ground rules

- Native Windows only. The web build is gone. A new **downloadable demo** is built at the very end; ignore the demo until then.
- **Saves: clean slate, no migration.** The save system is reworked as part of this overhaul (see "Saves"), which lifts the earlier "saving deferred" rule for this work.
- Things the player should understand are shown with a progress bar and requirement text (classes, abilities, Poe items). Not everything is told: death lessons, some relics and some cosmetics stay hidden or teased as "???".

## 1. After every run (win or lose)

- Two reward tracks:
  - **Echoes** (exists): spent at Poe on permanent unlocks the player chooses.
  - **Renown** (new): fills automatically after every run, scored by domains cleared, bosses killed, time survived and feats. Milestones unlock passives and content on their own.
- Unlocks open further unlocks (tiers), so progress keeps branching.
- A **post-run reveal screen** shows what just unlocked and how close the next unlock is.
- A browsable **unlock map** (in the village) shows the tracks.

## 2. Abilities

- Every run starts with **1 ability**. Ability slots are **4 maximum, always**. *Open: does the ultimate count toward the 4? (Today the kit is 3 abilities + 1 ultimate.)*
- Starting with **2 abilities** is a purchase (Poe's existing "Ability Mastery" unlock can carry it).
- **Starter choice per class** before a run: the existing Armory, with options opening as you play that class.
- **Ultimates and the stronger abilities unlock only by playing that class** (class mastery is already tracked per class).
- Every class gets a **locked kit**: today only the Mage's Bolts/Ultimates are gated. Locked abilities appear as "???" in shops and on cards.
- The level-up screen's ultimate row exists but is never switched on: decide its role.

## 3. Merchants and the village

- **Zeph**: gear. A merchant first, light on story.
- **Trainer** (new): abilities and per-class starter choices.
- **Apothecary** (new): consumables and wares.
- **Blacksmith** (new, unlocked later): upgrades or rerolls gear bonuses.
- **Wardrobe**: skins.
- **Poe**: Echoes, permanent unlocks, and **the main story**.
- Each merchant arrives as a village building once unlocked; their stock deepens in tiers. *Open: are merchants in the village between runs, at camps during runs (Zeph's camp opens each domain today), or both?*
- **Living village** (it should feel alive and safe): villagers with routines (walking between buildings, sitting, chatting, going home at night), advice that reacts to how you died, new residents moving in as you progress, calm music and warm lighting.
- **Story**: Poe's dialogue advances after each death, with bigger beats on milestones. Scripts are written together later, near the end.

## 4. Time of day

- **Built 2026-10-19 (Robert's call, replacing the hybrid clock):** the clock always runs while you play, in the village and in the dungeon, indoors or out, Breath of the Wild style, and never shows on screen. One day is 20 minutes of play (`Balance::DayNight::kDayLengthSeconds`). It stops with the game (menus, pause, card picks) and is saved in the profile (`village.clock`).
- The look of the hour (morning gold, plain day, evening orange, dusk purple, blue night) recolours the existing art; there is no night art. Only outdoor places take it: the village, Forest, Graveyard, Jungle, Lost City and Wastelands (`DayNight::IsOutdoorBiome`). Ancient Castle, Caverns, Demons Insides, Dream Realm and The Sanctuary keep their own lighting.
- At night everything that glows lights the ground around the spot it is on (a fireball carries its light, with no lit trail), in stepped pixel bands, and the player has a soft light of their own. Code: `DayNight.h` (clock and looks), `VfxNight.h` (the shader pass), `EngineDayNight.cpp`; tuning in `Balance::DayNight`. Debug: the juice panel (J in debug mode) sets the hour and fast-forwards it.
- Still to come: lanterns and lit windows in the village, villagers indoors at night, maybe a night-only merchant.

## 5. Numbers, crits and gear

- **Big numbers**: HP, damage and healing are rescaled ×10 (heroes 60–100 HP, opening hits 20–50, late hits into the high hundreds). Robert chose ×10 over ×100 on 2026-10-18: it is a short RPG, so numbers should reach the high hundreds, not the thousands. Multipliers, chances, durations, XP and prices did not change. No large-number formatting is needed at this scale.
- **Real stats** (in the game since 2026-10-18; values are placeholders in `GameBalance.h` `Balance::Stats`): Attack, Defense, Speed (cards add only a little), Crit Chance (5% base), Crit Damage (+50% base), Ability Power, Max HP and Mana. Every player hit rolls the crit; Deadeye now adds +20% crit chance. New common cards: Crit Chance +3% and Crit Damage +15%. The Character page (second pause screen) lists them for now.
- **Defense formula**: the placeholder is `damage × K / (K + Defense)` with K = 100, on hits only (burn, poison and other ticks ignore it), capped at a 60% reduction (reached at 150 Defense), and a landed hit always deals at least 1. The armour pips are gone; their sources now grant Defense (Warrior and Paladin start at 10; War Cry and Shield of Faith give +20 for 6 s; Ironclad gives +3 per cleared room up to +15). Still open: K growing per domain so early gear slowly matters less. Rejected: plain `attack − defense` (immunity at the top, irrelevance at the bottom).
- **Slots**: helmet, gloves/gauntlets, chest, weapon (more TBD). Weapons are class-based (sword, mace, bow, daggers, staff, tome) and carry attack.
- **Weights** (WoW-style):
  - Mage, Warlock: light only.
  - Hunter, Rogue: light and medium.
  - Warrior, Paladin: light, medium and heavy.
- **Primary stats** decide who benefits: Strength (Warrior, Paladin), Agility (Hunter, Rogue), Intellect (Mage, Warlock).
- **Item model**: hand-made base pieces per slot and weight, plus 1–2 rolled bonuses (e.g. +attack, +crit). Class-only legendaries with unique effects. Gear sets with set bonuses as a long-term chase.
- The armour pips (each blocked a whole hit) were replaced by Defense on 2026-10-18; a heavy-armour block feature could still come back with gear.

## 6. Relics

- More relics (59 today). The pool starts smaller and grows through unlocks, feats and death lessons.

## 7. Death lessons

Dying to something teaches a lesson that unlocks a counter. **Requires death-cause tracking, which does not exist yet.** Tags: [Perk] passive loadout, [Relic] joins the pool, [Ware] sold by a merchant, [Lore] Grimoire page.

**Rank and file**
1. Grunt → [Perk] Battle-Hardened: a little extra max HP, capped.
2. Cyclops → [Relic] Mirror Shard: lasers deal half damage.
3. Skeleton Archer → [Ware] Arrowcatcher Cloak: blocks the first arrow each room.
4. Flame Wisp → [Relic] Ember Ward: burns end twice as fast.
5. Slime → [Perk] Split Decision: the last piece of a split slime drops a small heal.
6. Sporeling → [Ware] Herbal Mask: poison clouds can't hurt you for one room.
7. Shieldbearer → [Relic] Flanker's Edge: bonus damage from behind.
8. Phantom → [Perk] Spirit Sight: phased enemies glow just before turning solid.
9. Bomber Imp → [Relic] Blast Padding: less explosion damage.
10. Warchief → [Ware] Banner Breaker: killing a commander stuns enemies nearby.
11. Living Blade → [Perk] Riposte: dashing through an attack refunds the dash.

**Elite bruisers**

12. Ogre → [Relic] Matador's Cape: a charge that misses you stuns the charger.
13. Infernal Brute → [Ware] Fireproof Tonic.
14. Bonechill → [Relic] Warm Blood: slows wear off faster.
15. Stormclub → [Perk] Anchor: much less knockback.
16. Venomfang → [Relic] Antivenom: venom damage reduced; kills cleanse you.

**Domain guardians**

17. Werewolf → [Relic] Silver Edge: bonus damage to beasts.
18. Chomp Bug → [Relic] Swatter: bonus damage to flyers.
19. Pumpkin Jack → [Perk] Eyes in the Back: a warning when something teleports behind you.
20. Minotaur → [Relic] Red Flag: enemies that hit walls stay stunned longer.
21. Osiris → [Perk] Ring Reader: gaps in bullet rings are highlighted.
22. Titan Guard → [Relic] Backstabber's Oath: rear hits always crit.
23. Toxic Vermin → [Ware] Stilt Boots: ground pools can't hurt you briefly.
24. Ancient Bear → [Perk] Rooted: resist pull effects.
25. Abyss Slime → [Relic] Etched Soles: your acid trail hurts enemies.
26. Molarbeast → [Lore] the final guardian's story, plus the Molten Crown cosmetic.

**Hazards and statuses**

27. Lava pool → [Perk] Fire Walker: lava hurts less.
28. Fire totem → [Ware] Totem Breaker: totems die in one hit.
29. Fireball torch → [Relic] Torchbearer: melee hits knock fireballs away.
30. Burned to death → [Perk] Stop, Drop: dashing puts out burn.
31. Poisoned to death → [Perk] Tolerance: poison can't take your last HP.
32. Died while slowed → [Ware] Quicksilver Draught.
33. A corpse blast (Volatile rooms) → [Perk] Volatile Instinct: corpses flash before they burst.
34. Died inside an elite's cage → [Relic] Cage Key: cages close slower.

**Elites and room events**

35. Enraged elite → [Perk] Calm Before: enrage starts later.
36. Guard-linked elite → [Perk] Linkbreaker: hitting a link anchor deals bonus damage.
37. Arena Pressure room → [Ware] Pressure Valve.
38. During a Cursed Cache curse → [Relic] Curse Ward: shorter curses.
39. With a Risk Shrine contract running → [Perk] Hedged Bet: failed contracts refund part of the stake.
40. In a domain's first room → [Perk] Warm-Up: a brief shield on domain entry.

**How you died**

41. With full mana → [Ware] Mana Shield: full mana absorbs one lethal hit per room.
42. Without dashing for 30 s → [Perk] Second Step: +1 dash charge.
43. Carrying 500+ gold → [Ware] Lockbox: bank gold mid-run.
44. With an empty ability slot → [Perk] Quick Study: your first ability pick each run shows 4 cards.
45. With your class resource full (Rage, Combo, Faith…) → [Perk] Release Valve: it fires automatically at low HP.
46. Within 10 s of entering a room → [Perk] Grace Period: brief invulnerability when you enter.
47. With the boss under 10% HP → [Perk] Grudge: that boss starts weaker against you next time.
48. Three times in the same domain → [Perk] Local Guide: that domain's map is revealed.
49. On an Ascension run → [Cosmetic] Ascendant's Scar.
50. After 45+ minutes → [Perk] Marathoner: a start-of-run bonus that grows with your longest run.

## 8. Skins and sprites

A sprite is either a player skin or an NPC, never both. 29 selectable looks exist (Hero01–Hero29); Hero30 is Poe; Zeph has his own art. The five class-named sheets (Hero, Hunter, Mage, Rogue, Warrior) duplicate numbered looks.

| Group | Sprites | Use |
|---|---|---|
| Start set | 03 (default), 02, 04, 05 | Player, any class, unlocked from the start |
| Bunny onesies | 24, 27, 28, 09 | Player, any class, unlockable |
| Heavy armour | 13, 16, 17, 23 | Player, Warrior/Paladin only, unlockable |
| Hooded robes | 10, 11, 19 | Player, Mage/Warlock only, unlockable |
| Eyepatch, dark gear | 06, 07, 08 | Player, Rogue/Hunter only, unlockable |
| Villagers | 01, 12, 18, 20, 25 | NPCs. Five is thin: add recoloured variants or new art |
| Poe | 30 | NPC |
| Shelved | 14, 15, 21, 22, 26, 29 | Unused (near-copies of the chosen onesies) |

Skins unlock through Renown milestones, feats, death lessons, class mastery (class-themed looks) and a few secrets with no hint.

## 9. More unlockable content

- Merchant wares (yes, definitely), crossroads boons, village buildings.
- Heirloom choice: pick 1 of 3 starting relics instead of a random one.
- A village jukebox that collects domain music.
- Challenge runs (one HP, no shops…), each with its own reward.
- New Poe stock tiers as Renown grows.
- Grimoire lore pages and villager dialogue that advance after deaths.
- Event rooms: undecided.

## 10. Reasons to keep playing after mastery

- Expanded Ascension tiers with per-tier cosmetics.
- Prestige mastery ranks (cosmetic rewards, not power).
- Class trials: hand-made challenge runs per class.
- Daily seeded runs (local only).
- Gear sets with set bonuses.
- Grimoire completion goals.
- Dropped: online leaderboards (no backend) and separate harder boss versions (difficulty already scales with depth).

## 11. Difficulty curve (needs more thought)

- **Today**: power level 1 (Forest) to 6 (final domain). Each level adds +12% enemy HP, +8% damage, +4% speed and 5% faster attacks (capped). Bosses share it. Dungeons grow each domain. Ascension adds cumulative rules.
- **Model**: a run is enemy pressure racing player power; the run ends where pressure overtakes power, and meta unlocks move that point deeper over many runs.
- **Direction**: accelerating pressure (each domain multiplies the last instead of adding a flat step), pressure beyond stats (density, nastier enemy mixes, earlier and stacked elite modifiers, scarcer healing, pricier shops), no hidden difficulty adjustment, an optional visible Poe boon after a losing streak.
- **Targets**: early runs die in domains 1–2; runs 5–10 reach domains 3–4; first win around 5–8 runs for a skilled player, 10–15 for an average one.
- **Measure before tuning**: a run log (domain, room, time, cause of death, build) plus the per-room telemetry that already exists.

## 12. Menus and UI polish

Animated menu backgrounds; buttons with eased hover/press animations and sounds; smooth screen transitions; reward cards that flip and glow; unlock reveals with particles and a short flash; progress bars that fill with a shimmer; number count-ups. Existing screens (title, class select, pause, Poe's altar) can go first; new screens (post-run reveal, unlock map, gear loadout) need it most.

## 13. Saves (rework)

- **Clean slate, no migration.**
- **Structured, versioned format** (JSON), one profile folder (Steam Cloud friendly).
- **Tamper resistance**: each file is signed (HMAC-SHA256 with a key built into the game) and optionally obfuscated so it isn't plain text. On load: signature check, then validation (types, ranges, references); a failed file is rejected and the last good backup restored.
- **Honest limit**: an offline single-player game cannot be made tamper-proof; anything stored on the player's machine can eventually be edited by someone determined. This stops casual editing, which is the realistic goal.
- Folds in the old deferred saving backlog: failed checkpoint writes, numeric and semantic validation, resource bounds (see the engineering review).
- **Status (2026-10-17): built.** The details are in `docs/SAVE_FORMAT.md`.
  - Signed JSON in `<user data>/Saves/`, with stable names and schema 1.
  - Strict loading with backup restore.
  - Failed writes never lose the run.
  - A run log with cause of death.
  - Lifetime death tallies in the profile.

## 14. Release path

Robert's plan: proper marketing (beyond TikTok and Threads) → build interest → optional Kickstarter → Steam page with a demo → polish → release. For the build order this means a polished, showable slice (UI pizazz plus the new progression loop) is needed early enough for the Steam page, screenshots and trailer.

## Open questions

1. Does the ultimate count toward the 4 ability slots?
2. Merchants: village, in-run camps, or both?
3. Economy: currencies (gold, Echoes, Renown, a gear material?) and what persists between runs.
4. Final defense formula and stat values (a placeholder formula and stat list are in; see section 5).
5. Difficulty numbers (after the run log exists).
6. Event-room unlocks: yes or no.
7. Villager crowd: recolours or new art.

## Suggested build order

1. **Save rework** (foundation for everything new) together with the **run log and death-cause tracking**. *Done 2026-10-17; see `docs/SAVE_FORMAT.md`.*
2. **Number rescale** (×10), stats (Defense, crits, Ability Power, Speed) and damage numbers: every later number depends on it. *Done 2026-10-18 with placeholder values; the Character page is a plain layout for the UI overhaul to restyle.*
3. **Progression core**: Echoes + Renown after every run, the unlock framework (requirements, visibility, tiers), the post-run reveal screen.
4. **Ability rules**: 1 to start, 4 maximum, per-class locked kits, class-earned ultimates, the Armory.
5. **Skins** roster and unlocks.
6. **Gear** (once the economy and formula are settled).
7. **Merchants, village growth, living villagers, and the night village (lanterns, lit windows; the clock and lighting landed early, see section 4).**
8. **Death lessons** and relic pool growth.
9. **Difficulty tuning** with real run data.
10. **UI polish** throughout, starting early on existing screens for Steam material.
11. **Story scripts** with Robert.
12. **Downloadable demo build** and Steam.
