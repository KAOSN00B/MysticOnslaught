#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// ROOM AFFIXES — Mystic Onslaught's run-to-run variety layer.
//
// When the player enters a Standard combat room there's a chance it rolls an
// AFFIX: a one-room modifier layered on top of the tiles/enemies that already
// exist, so the two real biomes feel far more varied without any new art. Each
// affix is pure game logic driven off hooks the Engine already owns (enemy
// count, ApplyDifficultyScaling, gold drops, the on-kill callback).
//
// Add an affix: add to the enum (before Count), add a row to kRoomAffixes in the
// SAME order, then make sure its effect fields are honoured at the hook sites in
// Engine.cpp (SpawnDungeonRoomEnemies / ConfigureSpawnedEnemy / SpawnEnemyDrop /
// the onEnemyKilled callback).
// ─────────────────────────────────────────────────────────────────────────────

enum class RoomAffix
{
    None = 0,
    Swarm,       // many weak enemies
    Cursed,      // enemies hit harder, but drop more loot
    Volatile,    // slain enemies erupt in toxic clouds
    Gilded,      // enemies drop far more gold
    Count
};

struct RoomAffixDef
{
    const char*   name;
    const char*   description;
    unsigned char r, g, b;        // banner colour
    float         enemyCountMult; // multiplier on the room's basic-enemy count
    float         enemyHpMult;    // per-enemy health multiplier
    float         enemyDmgMult;   // per-enemy damage multiplier
    float         goldMult;       // gold-drop multiplier (whole extra piles)
    bool          volatileDeath;  // leave a toxic cloud where enemies die
    bool          bonusLoot;      // raise the bonus heal-drop chance
    int           rollWeight;     // relative chance once an affix is chosen
};

// Indexed by (int)RoomAffix — keep in the same order as the enum. None is index 0
// and must stay a pure no-op.
inline const RoomAffixDef kRoomAffixes[] =
{
    { "",         "",                                       0,   0,   0,   1.0f, 1.0f, 1.0f, 1.0f, false, false,  0 },
    { "Swarm",    "Overwhelming numbers - weak but many",   214, 140, 60,  1.6f, 0.7f, 1.0f, 1.0f, false, false, 30 },
    { "Cursed",   "Foes hit harder, but hoard riches",      176,  84, 208, 1.0f, 1.0f, 1.25f,1.5f, false, true,  26 },
    { "Volatile", "Slain foes erupt in toxic clouds",       96,  196, 96,  1.0f, 1.0f, 1.0f, 1.0f, true,  false, 24 },
    { "Gilded",   "Foes glitter - triple gold",             232, 202, 72,  1.0f, 1.0f, 1.0f, 3.0f, false, false, 14 },
};

// Percent chance a Standard room rolls ANY affix (else it stays None).
inline constexpr int kRoomAffixChancePercent = 40;

inline const RoomAffixDef& GetRoomAffixDef(RoomAffix affix)
{
    int idx = (int)affix;
    if (idx < 0 || idx >= (int)RoomAffix::Count) idx = 0;
    return kRoomAffixes[idx];
}
