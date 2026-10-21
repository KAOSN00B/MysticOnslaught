#pragma once

// =============================================================================
// RunProgression.h — the single campaign-depth policy (release-candidate pass).
//
// Why this exists: enemy stats used to scale from Engine::_wave and shop prices
// from Engine::_currentAct, but the real run route (dungeon rooms + world map)
// never advanced either counter. Every domain therefore fought power-level-1
// enemies and every shop charged first-act prices, while the debug/legacy act
// map was the only path where the curve ever moved.
//
// Policy: run DEPTH is the world-map domain index (Engine::_worldZone):
//   0 = the opening Forest, 1-4 = the four chosen domains, 5 = the final domain.
// Depth only advances when the player commits to a new domain on the world map,
// so backtracking, revisiting cleared rooms, reopening menus, pausing in the
// village and resuming can never raise difficulty. A new run resets it to 0.
//
// Each difficulty axis has exactly one owner, so nothing double-scales:
//   * Stats (this file): one global HP/speed/cadence/damage curve per domain.
//   * Composition (Engine::SpawnDungeonRoomEnemies): intra-domain tier from
//     cleared rooms, plus one extra body per two domains.
//   * Colour variant tier: cosmetic only, also keyed to the domain.
//   * Ascension / wagers / affixes / contracts: explicit opt-in multipliers.
// =============================================================================

#include "GameBalance.h"

#include <algorithm>
#include <cmath>

namespace RunProgression
{
    inline constexpr int kFirstZone = 0;
    inline constexpr int kFinalZone = 5;

    inline constexpr int ClampZone(int worldZone)
    {
        return worldZone < kFirstZone ? kFirstZone : (worldZone > kFinalZone ? kFinalZone : worldZone);
    }

    // Enemy power level for a domain: 1 in the opening Forest, 6 in the final
    // domain. Bosses share it, so a boss met late is genuinely tougher.
    inline constexpr int EnemyPowerLevelForZone(int worldZone)
    {
        return 1 + ClampZone(worldZone);
    }

    // Cyclops/Ogre/Skeleton Archer/Flame Wisp still tighten their attack timing
    // in bands of kRoomsPerPowerLevel "waves" inside SetWaveScale. Feed them the
    // wave that maps to the same power level so stats and behaviour agree.
    inline constexpr int BehaviourWaveForPowerLevel(int powerLevel)
    {
        return 1 + Balance::Curve::kRoomsPerPowerLevel * ((powerLevel < 1 ? 1 : powerLevel) - 1);
    }

    // Shop tier ("act" argument of ShopManager::Enter) for a domain.
    inline constexpr int ShopTierForZone(int worldZone)
    {
        return 1 + ClampZone(worldZone);
    }

    // HP a room survivor returns with when the player re-enters its room: the
    // same share of its (same-domain, therefore identical) max HP, rounded up,
    // never below 1 and never above full. Leaving a room never heals enemies.
    inline float RestoredSurvivorHealth(float maxHealth, float savedFraction)
    {
        if (!(savedFraction > 0.f) || savedFraction >= 1.f) return maxHealth;
        const float hp = std::ceil(maxHealth * savedFraction);
        return std::max(1.f, std::min(maxHealth, hp));
    }

    // Multipliers applied to an enemy's authored base profile at a power level.
    struct EnemyStatScale
    {
        float health      = 1.f;
        float damage      = 1.f;
        float speed       = 1.f;
        float attackDelay = 1.f;   // < 1 = attacks more often
        int   expBonus    = 0;
    };

    inline EnemyStatScale StatScaleForPowerLevel(int powerLevel)
    {
        EnemyStatScale s{};
        if (powerLevel <= 1)
            return s;
        const float t = (float)(powerLevel - 1);
        s.health      = 1.f + Balance::Curve::kHealthPerLevel * t;
        s.damage      = 1.f + Balance::Curve::kDamagePerLevel * t;
        s.speed       = 1.f + Balance::Curve::kSpeedPerLevel  * t;
        s.attackDelay = std::max(Balance::Curve::kMinAttackDelayFactor,
                                 1.f - Balance::Curve::kAttackDelayPerLevel * t);
        s.expBonus    = powerLevel - 1;
        return s;
    }
}
