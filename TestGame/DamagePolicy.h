#pragma once

// =============================================================================
// DamagePolicy.h — the one precision/rounding rule for player -> enemy hits,
// and the Defense rule for hits on the player.
//
// Damage and HP stay integers (readable numbers, integer TakeDamage
// everywhere; x10 scale since 2026-10-18). The old pipeline rounded several
// times per hit and rounded relic bonuses UP, so a nominal +15% on a 2-damage
// hit dealt 3 (+50%) and changed hits-to-kill on every 6-HP enemy.
//
// Policy:
//   1. Authored ability values (level / class-buff scaling) are rounded to the
//      nearest integer once, when the ability is cast (unchanged).
//   2. Every per-hit modifier — run/ascension multiplier, relics, crits,
//      status combos, class marks/curses, target vulnerability — is combined
//      in float and converted to an integer exactly ONCE (ResolveHitDamage).
//   3. The fractional remainder is carried per player and paid out on a later
//      hit, so over a fight delivered damage matches the nominal bonus
//      (2 x 1.15 averages 2.3 per hit: 2,2,2,3,2,2,3,...).
//   4. Every landed hit with positive nominal damage deals at least 1.
//   5. Damage-over-time ticks (bleed, poison, burn) snapshot their value when
//      applied and are NOT re-scaled by per-hit modifiers.
// =============================================================================

#include "GameBalance.h"

#include <algorithm>
#include <cmath>

namespace DamagePolicy
{
    // Converts one hit's nominal damage into whole damage, carrying the
    // fraction. `remainder` must start at 0 for a new run.
    inline int ResolveHitDamage(float nominal, float& remainder)
    {
        if (!(nominal > 0.f))
            return 0;
        const float carried = std::clamp(remainder, 0.f, 0.999f);
        const float total = nominal + carried;
        // Tiny epsilon so 2.9999998 (float noise for an exact 3) is still 3.
        int whole = (int)std::floor(total + 1e-4f);
        remainder = std::max(0.f, total - (float)whole);
        if (whole < 1)
        {
            whole = 1;           // a landed hit always registers
            remainder = 0.f;     // ...without creating damage debt
        }
        return whole;
    }

    // Defense (enemy -> player hits only; ticks skip it): the share of a hit it
    // removes, Defense / (K + Defense), capped. None, negative or NaN removes nothing.
    inline float DefenseReduction(float defense)
    {
        if (!(defense > 0.f))
            return 0.f;
        return std::min(Balance::Stats::kMaxDefenseReduction, defense / (Balance::Stats::kDefenseK + defense));
    }

    // A whole-number hit after Defense: rounded to nearest, never below the minimum.
    inline int ApplyDefenseToHit(int damage, float defense)
    {
        if (damage <= 0)
            return 0;
        const long reduced = std::lround((float)damage * (1.f - DefenseReduction(defense)));
        return std::max(Balance::Stats::kMinHitDamage, (int)reduced);
    }

    // Boss contact keeps its fraction.
    inline float ApplyDefenseToFractionalHit(float damage, float defense)
    {
        if (!(damage > 0.f))
            return 0.f;
        return std::max((float)Balance::Stats::kMinHitDamage, damage * (1.f - DefenseReduction(defense)));
    }
}
