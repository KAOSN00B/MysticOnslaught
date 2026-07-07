#pragma once

#include "AbilityType.h"
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// AttackTuning — the runtime reader for the Attack Editor's saved files
// (attacktuning_<key>.txt). Gameplay queries this so a resized hitbox or a
// swapped FX sprite actually takes effect.
//
// SAFETY: Get() returns nullptr when no file exists for a key, and consumers
// fall back to their hardcoded defaults — so the game plays exactly as before
// until the designer deliberately saves a tuning.
//
// The key must match how AttackEditor builds it: sanitised "<owner>_<name>".
// Both sides use the helpers below so the keys never drift apart.
// ─────────────────────────────────────────────────────────────────────────────

struct AttackTuning
{
    bool        hasBox = false;
    float       x = 0.f, y = 0.f, w = 0.f, h = 0.f;   // centre-relative, facing right
    bool        hasFx  = false;
    std::string fxStem;                                // "" = cleared (no FX)
};

const char* AttackOwnerForAbility(int abilityIdx);     // Mage/Warrior/... by enum range
std::string AttackSanitise(const std::string& s);
std::string AttackTuningKey(const std::string& owner, const std::string& name);
std::string AttackTuningKeyForAbility(AbilityType ability);

namespace AttackTuningStore
{
    // nullptr = no saved file (use defaults). Results are cached.
    const AttackTuning* Get(const std::string& key);
    void Reload(const std::string& key);   // drop the cache entry after a save
}
