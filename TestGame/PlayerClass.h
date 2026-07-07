#pragma once

#include "AbilityType.h"

// ─────────────────────────────────────────────────────────────────────────────
// PLAYER CLASSES — chosen at the start of a run. Each class has its own sprite
// set (weapon animation), base stats, and ability pool offered in Zeph's shop.
//
// The Mage keeps the original elemental spell kit. The other three classes get
// their own dedicated movesets (built in a later pass); until those land they
// share a themed slice of the elemental abilities so every class is playable.
//
// Sprite files live in Hero/ as <ClassName>_{Idle,Walk,Attack,Death,Hurt}.png.
// ─────────────────────────────────────────────────────────────────────────────

enum class PlayerClass
{
    Mage,       // staff — Fire/Ice/Electric spells (the original kit)
    Warrior,    // sword — heavy melee bruiser (tanky)
    Hunter,     // bow   — mobile ranged skirmisher
    Rogue,      // daggers — fast, fragile, high burst
    Paladin,    // sword+shield — holy tank (kit built in a later pass)
    Warlock,    // dark caster — curses & drain (kit built in a later pass)
    Count
};

struct PlayerClassInfo
{
    const char* name;
    const char* spritePrefix;    // Hero/<prefix>_Idle.png etc.
    const char* description;     // one-liner for the select screen
    const char* playstyle;       // short flavour of base stats

    // Base stats (applied in Character::Init on top of / instead of defaults).
    int   baseHealth;
    int   baseMana;
    float baseAttackPower;
    float baseMoveSpeed;
    int   startArmour;
    float manaRegenMult;
};

const PlayerClassInfo& GetPlayerClassInfo(PlayerClass cls);

// True if this class may be offered the ability in shops / level-up / treasure.
bool ClassAllowsAbility(PlayerClass cls, AbilityType ability);

// Mage/Warlock/Hunter fire a projectile as their basic attack instead of melee.
bool ClassUsesRangedBasic(PlayerClass cls);

// ─────────────────────────────────────────────────────────────────────────────
// APPEARANCE — chosen INDEPENDENTLY of class. The player picks how their hero
// looks; class only decides the moveset. Sprites live in Hero/<prefix>_*.png.
// ─────────────────────────────────────────────────────────────────────────────
int         GetAppearanceCount();
const char* GetAppearancePrefix(int index);   // e.g. "Hero03"
const char* GetAppearanceName(int index);      // e.g. "Hero 03"
const char* GetDefaultAppearancePrefix();      // starting look
const char* GetCellsShopMerchantPrefix();      // NPC reserved for the cells shop
