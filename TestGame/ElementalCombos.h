#pragma once
#include "AbilityType.h"
#include "Enemy.h"
#include "VFXManager.h"

// ─────────────────────────────────────────────────────────────────────────────
// MAGE CLASS IDENTITY — ELEMENTAL COMBOS
//
// The Mage's "brain mode" is spell SEQUENCING: elements react with the status
// already on the target, so cast order matters more than cooldown spam.
//
//   SHATTER!  fire     on a FROZEN  target → bonus burst damage
//   CONDUCT!  electric on a FROZEN  target → bonus damage + freeze refreshed
//   STEAM     ice      on a BURNING target → scalding cloud: strong slow
//   OVERLOAD! electric on a BURNING target → bonus burst damage
//
// Call ResolveElementalCombo BEFORE the hit's own element status is applied so
// it reads the status that was set up by the PREVIOUS cast. It applies any
// side effects (slow / freeze refresh) itself, announces the combo with a
// floating word, and returns bonus damage for the caller to add to the hit.
// Only the Mage's elemental pipeline produces these AbilityTypes, so no class
// check is needed. Balance is deliberately conservative — small bonuses that
// reward sequencing, not damage inflation.
// ─────────────────────────────────────────────────────────────────────────────
inline int ResolveElementalCombo(Enemy& enemy, AbilityType element, VFXManager* vfx)
{
    const bool isFire     = (element == AbilityType::FireSpread     || element == AbilityType::FireBolt     || element == AbilityType::FireUltimate);
    const bool isIce      = (element == AbilityType::IceSpread      || element == AbilityType::IceBolt      || element == AbilityType::IceUltimate);
    const bool isElectric = (element == AbilityType::ElectricSpread || element == AbilityType::ElectricBolt || element == AbilityType::ElectricUltimate);

    int         bonusDamage = 0;
    const char* comboWord   = nullptr;
    Color       comboColor  = WHITE;

    if (isFire && enemy.IsFrozen())
    {
        // Fire shatters the ice — the frozen setup pays off as a burst.
        bonusDamage = 3;
        comboWord   = "SHATTER!";
        comboColor  = Color{ 170, 230, 255, 255 };   // ice-white burst
    }
    else if (isElectric && enemy.IsFrozen())
    {
        // Current conducts through the ice — damage plus the hold is refreshed.
        bonusDamage = 2;
        enemy.ApplyFreeze(2.f);
        comboWord   = "CONDUCT!";
        comboColor  = Color{ 255, 235, 90, 255 };    // electric yellow
    }
    else if (isIce && enemy.IsBurning())
    {
        // Ice on burning flesh flashes to scalding steam — a strong lasting slow.
        bonusDamage = 1;
        enemy.ApplySlow(0.5f, 3.5f);
        comboWord   = "STEAM";
        comboColor  = Color{ 200, 220, 230, 255 };   // pale vapour grey
    }
    else if (isElectric && enemy.IsBurning())
    {
        // Charge ignites the flames into an overload burst.
        bonusDamage = 3;
        comboWord   = "OVERLOAD!";
        comboColor  = Color{ 255, 170, 60, 255 };    // burning orange
    }

    if (comboWord != nullptr && vfx != nullptr)
        vfx->SpawnFloatingLabel(enemy.GetWorldPos(), comboWord, comboColor, 1.3f);

    return bonusDamage;
}
