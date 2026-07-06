#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// ASCENSION — Mystic Onslaught's difficulty ladder (Slay the Spire / Hades style).
//
// Ascension is CUMULATIVE: selecting Ascension N applies every modifier from
// tiers 1..N. Each tier layers on ONE named rule so climbing the ladder feels
// qualitatively different, not just "bigger numbers". This is pure difficulty
// tuning — it needs no new art or content, only the run-wide multipliers the
// Engine already owns (_runEnemyHealthMult / _runEnemyDamageMult / etc.).
//
// To add a tier: bump MetaProgression::kMaxAscension and add a row below. The
// aggregation in GetAscensionModifiers() folds tiers 1..N into the concrete
// multipliers the Engine applies once, at run start (ResetRunState).
// ─────────────────────────────────────────────────────────────────────────────

// One tier's INCREMENTAL contribution — the single rule this tier adds on top of
// every lower tier. Multipliers are per-tier deltas (1.0 = no change); the
// heal-drop penalty is in percentage points removed from the drop chance.
struct AscensionTierDef
{
    const char* name;                // short flavour name shown in the menu
    const char* effect;              // one-line description of THIS tier's rule
    float       enemyHpMult;         // delta to enemy health
    float       enemyDmgMult;        // delta to enemy damage
    float       playerDmgMult;       // delta to the player's outgoing damage
    float       bossHpMult;          // extra HP for bosses only
    int         healDropPenaltyPct;  // points removed from the bonus heal-drop chance
};

// Indexed by (tier - 1): kAscensionTiers[0] is the rule added at Ascension 1.
// Keep this length in sync with MetaProgression::kMaxAscension.
inline const AscensionTierDef kAscensionTiers[] =
{
    { "Toughened Foes",  "Enemies have +18% health",             1.18f, 1.00f, 1.00f, 1.00f, 0 },
    { "Scarce Mercy",    "Healing drops far less often",         1.00f, 1.00f, 1.00f, 1.00f, 6 },
    { "Savage Blows",    "Enemies deal +20% damage",             1.00f, 1.20f, 1.00f, 1.00f, 0 },
    { "Dwindling Power", "Your own damage is reduced by 12%",    1.00f, 1.00f, 0.88f, 1.00f, 0 },
    { "Relentless",      "Enemies gain +18% HP and +12% damage", 1.18f, 1.12f, 1.00f, 1.00f, 0 },
    { "Apex Predators",  "Bosses gain +40% HP; healing scarcer", 1.00f, 1.00f, 1.00f, 1.40f, 4 },
};

inline int GetAscensionTierCount()
{
    return (int)(sizeof(kAscensionTiers) / sizeof(kAscensionTiers[0]));
}

// The rule ADDED at a given tier (1-based). Returns nullptr for tier 0 / overflow.
inline const AscensionTierDef* GetAscensionTierDef(int tier)
{
    if (tier < 1 || tier > GetAscensionTierCount()) return nullptr;
    return &kAscensionTiers[tier - 1];
}

// Aggregated modifiers for a chosen tier — folds tiers 1..tier together.
struct AscensionModifiers
{
    float enemyHpMult        = 1.f;
    float enemyDmgMult       = 1.f;
    float playerDmgMult      = 1.f;
    float bossHpMult         = 1.f;
    int   healDropPenaltyPct = 0;
};

inline AscensionModifiers GetAscensionModifiers(int tier)
{
    AscensionModifiers m;
    if (tier > GetAscensionTierCount()) tier = GetAscensionTierCount();
    for (int i = 0; i < tier; i++)
    {
        const AscensionTierDef& d = kAscensionTiers[i];
        m.enemyHpMult        *= d.enemyHpMult;
        m.enemyDmgMult       *= d.enemyDmgMult;
        m.playerDmgMult      *= d.playerDmgMult;
        m.bossHpMult         *= d.bossHpMult;
        m.healDropPenaltyPct += d.healDropPenaltyPct;
    }
    return m;
}
