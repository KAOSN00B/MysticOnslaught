# Mystic Onslaught audio additions — 2026-10-15

Assets only. No source code, configuration, project files, existing audio, or builds were changed by this import.

## Integration status
These clips are NOT connected to gameplay. SfxBank.cpp and AudioManager.cpp currently use explicit paths. Claude can integrate selected clips after the concurrent gameplay work, respecting the existing mixer, cooldowns, and voice limits.

Selection used pack descriptions and filenames, not listening. WAV headers and complete nonzero PCM payloads were checked, and every copied file was SHA-256 verified against its source. Audition in context before final assignment. Loop-labelled files still need seam checks. Music suggestions are provisional.

## Contents
- Armor: 4 files
- Blacksmith: 1 files
- Combat: 6 files
- Creatures: 12 files
- Environment: 7 files
- Footsteps: 8 files
- Interactions: 8 files
- Item Tiers: 6 files
- Item Types: 2 files
- Props: 2 files
- Spells: 16 files
- Stingers: 4 files
- Themes: 4 files
- Upgrades: 2 files
- Utility: 14 files
- Weapons: 2 files

## Handoff to Claude
- See manifest.json for exact source, destination, suggested use, duration, sample format and hash of every clip.
- Keep only useful variants active; do not preload every candidate into the sound bank.
- Prioritize spell identity, enemy warnings, armour blocks, loot feedback and surface footsteps.
- Use ambient loops quietly and stream music. Keep critical warning cues audible.
- Preserve source WAVs; make compressed shipping copies of accepted music/ambience before expanding browser downloads. Existing packaging may recursively include these folders even though the game does not play them yet.
- These are additions, not replacements for existing sounds. No automatic replacement or activation is intended.
- SourceDocuments preserves the supplied license-link PDFs; it is provenance, not a new licensing determination. The original packs remain in the user's library.
