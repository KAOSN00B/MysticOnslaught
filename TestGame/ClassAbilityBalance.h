#pragma once

#include "AbilityType.h"

// ── Class ability direct-hit damage ───────────────────────────────────────────
// Every class ability's main hit is  base + attackPower * perAtk,  then scaled
// by ability level (+25%/level) and active buffs in Engine::HandleClassAbilityCast.
// Bleeds, burns, poison and ground-zone ticks are tuned at their call sites.
//
// OPENING RULE (see GameBalance.h): at level 1 with the class's starting attack
// and no Rage/Faith/buffs, a non-ultimate ability's plain hit deals at most
// kOpeningAbilityHitCap, so it never kills a standard enemy (60+ HP) in one cast.
// Earned payoffs can exceed it: Backstab from behind (x2.2), a combo-spending
// Eviscerate, Rage/Faith, and ability level-ups. Ultimates are exempt.
// OpeningBalanceTests.cpp enforces this.
namespace Balance::ClassAbility
{
    inline constexpr int kOpeningAbilityHitCap = 50;

    struct HitSpec
    {
        AbilityType type;
        float       base;
        float       perAtk;
    };

    inline constexpr HitSpec kHits[] = {
        // Warrior (starting attack 40)
        { AbilityType::WarCleave,       18.0f, 0.8f },   // 50
        { AbilityType::Whirlwind,       26.0f, 0.6f },   // 50, + bleed
        { AbilityType::ThrowingAxe,     10.0f, 1.0f },   // 50, + bleed
        { AbilityType::Rend,            26.0f, 0.6f },   // 50, + bleed
        { AbilityType::ShieldBash,      24.0f, 0.4f },   // 40, + stun
        { AbilityType::GroundSlam,      70.0f, 1.5f },   // ultimate
        { AbilityType::Earthshatter,   100.0f, 2.0f },   // ultimate
        // Rogue (starting attack 30)
        { AbilityType::FanOfKnives,     29.0f, 0.7f },   // 50, + poison
        { AbilityType::Shadowstep,      32.0f, 0.6f },   // 50, + bleed
        { AbilityType::Backstab,        23.0f, 0.9f },   // 50 front, 110 from behind
        { AbilityType::Eviscerate,      17.0f, 1.1f },   // 50 at 0 combo, up to +75%
        { AbilityType::DeathMark,       90.0f, 1.8f },   // ultimate
        { AbilityType::BladeDance,      60.0f, 1.2f },   // ultimate
        { AbilityType::RainOfBlades,    90.0f, 1.7f },   // ultimate
        // Hunter (starting attack 30)
        { AbilityType::PiercingShot,    17.0f, 1.1f },   // 50, + bleed
        { AbilityType::Multishot,       32.0f, 0.6f },   // 50
        { AbilityType::FrostTrap,       21.0f, 0.3f },   // 30, + freeze
        { AbilityType::ExplosiveArrow,  11.0f, 1.3f },   // 50 AoE
        { AbilityType::Volley,           8.0f, 1.4f },   // 50, pierces shields
        { AbilityType::ArrowStorm,      80.0f, 1.6f },   // ultimate
        { AbilityType::PiercingBarrage,100.0f, 1.9f },   // ultimate
        // Paladin (starting attack 30)
        { AbilityType::Smite,           23.0f, 0.9f },   // 50
        { AbilityType::HolyBolt,        20.0f, 1.0f },   // 50, + mark
        { AbilityType::HammerThrow,     20.0f, 1.0f },   // 50, + stun
        { AbilityType::DivineStorm,     70.0f, 1.4f },   // ultimate
        { AbilityType::HammerOfJustice,100.0f, 1.9f },   // ultimate
        // Warlock (starting attack 30)
        { AbilityType::ShadowBolt,      20.0f, 1.0f },   // 50, + curse / vulnerability
        { AbilityType::DrainLife,       23.0f, 0.9f },   // 50, heals
        { AbilityType::Curse,           18.0f, 0.4f },   // 30, + poison / curse
        { AbilityType::Hellfire,        17.0f, 1.1f },   // 50 AoE, + burn
        { AbilityType::SoulSiphon,      23.0f, 0.9f },   // 50 AoE, + slow
        { AbilityType::Cataclysm,       72.0f, 1.8f },   // ultimate
        { AbilityType::ShadowNova,      73.0f, 1.7f },   // ultimate
    };

    // Unknown abilities fall back to a zero hit (rounded to 1 by the caller).
    // Table-only balance checks do not establish completeness of this lookup.
    inline constexpr HitSpec Hit(AbilityType type)
    {
        for (const HitSpec& spec : kHits)
            if (spec.type == type) return spec;
        return HitSpec{ type, 0.f, 0.f };
    }
}
