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
    bool  hasFxOffset = false;
    float fxForward = 0.f;   // visual FX centre relative to player, mirrored by facing
    float fxHeight  = 0.f;   // visual FX centre relative to player, + = down

    // ── Fire point + projectile tuning (Character Animator attack editor) ───────
    // Each group has a 'has' flag; consumers fall back to their hardcoded default
    // when unset, so untuned attacks play exactly as before.
    bool  hasFirePoint = false;
    float fireForward  = 50.f;   // spawn offset in front of the player (px, facing right)
    float fireHeight   = 0.f;    // spawn offset vertically (px, + = down)
    bool  hasProjectile = false;
    float projScale    = 1.f;    // projectile draw-scale multiplier (visual size)
    float projRadius   = 0.f;    // collision radius (0 = keep the projectile's default)
    float projSpeed    = 0.f;    // travel speed (0 = keep default)
    float projLifetime = 0.f;    // seconds before despawn (0 = keep default)
    bool  hasCooldown  = false;
    float cooldown     = 0.f;    // seconds between shots (basic attack cadence)

    // Generic ability-lab values. These describe gameplay geometry/timing and
    // are intentionally class-agnostic so every class can use the same editor.
    bool  hasAbility = false;
    float aimRange = 600.f;
    float areaRadius = 150.f;
    float effectLength = 400.f;
    float effectWidth = 90.f;
    float effectDuration = 4.f;
    float tickInterval = 0.5f;
    float chainRange = 280.f;
    float maxTargets = 4.f;
    float moveDistance = 400.f;
    float previewAngle = 0.f;
};

// Basic (non-ability) attack tuning key for a class, e.g. "Mage_Basic".
std::string AttackTuningKeyForBasic(int playerClass);

const char* AttackOwnerForAbility(int abilityIdx);     // Mage/Warrior/... by enum range
std::string AttackSanitise(const std::string& s);
std::string AttackTuningKey(const std::string& owner, const std::string& name);
std::string AttackTuningKeyForAbility(AbilityType ability);

namespace AttackTuningStore
{
    // nullptr = no saved file (use defaults). Results are cached.
    const AttackTuning* Get(const std::string& key);
    void Reload(const std::string& key);   // drop the cache entry after a save
    // Write a full attacktuning_<key>.txt (only the groups whose has* flag is set),
    // then refresh the cache so the change is live immediately.
    bool Save(const std::string& key, const AttackTuning& t);
}
