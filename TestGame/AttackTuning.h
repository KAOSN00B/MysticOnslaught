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

    // ── Elite signature timing (Attack Library "Elite" category) ────────────
    // Telegraph → Active → Recovery seconds, the cooldown between signatures,
    // travel distance for movement moves (leap/pounce), and the phase-two
    // multipliers. Unset = the coded defaults in Balance::Elite.
    bool  hasSignature = false;
    float telegraphTime = 0.f;
    float activeTime = 0.f;
    float recoveryTime = 0.f;
    float signatureCooldown = 0.f;
    float travelDistance = 0.f;
    float phaseSpeedMult = 1.f;
    float phaseCooldownMult = 1.f;

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

    // ── Hit area of a boss or elite special move (Character Animator) ───────
    // Unset = the move's coded size. Circle: hitRadius around the attacker,
    // hitForward ahead of it; ring: hitInner to hitRadius; lane: hitLength long,
    // hitWidth to each side; box: hitX/hitY/hitW/hitH (centre-relative, facing
    // right). Separate from the Attack Library's box_* so neither tool
    // overwrites the other.
    bool  hasHitArea = false;
    float hitRadius  = 0.f;
    float hitInner   = 0.f;
    float hitForward = 0.f;
    float hitLength  = 0.f;
    float hitWidth   = 0.f;
    float hitX = 0.f, hitY = 0.f, hitW = 0.f, hitH = 0.f;
};

// Basic (non-ability) attack tuning key for a class, e.g. "Mage_Basic".
std::string AttackTuningKeyForBasic(int playerClass);

const char* AttackOwnerForAbility(int abilityIdx);     // Mage/Warrior/... by enum range
std::string AttackSanitise(const std::string& s);
std::string AttackTuningKey(const std::string& owner, const std::string& name);
std::string AttackTuningKeyForAbility(AbilityType ability);

// Resolve one elite signature value: the saved tuning wins when its group is
// present and the value is meaningful (> 0), otherwise the coded default.
// Used by every elite so tuned and untuned moves flow through one rule.
inline float EliteSignatureValueOr(const AttackTuning* tuning,
                                   float AttackTuning::*field, float fallback)
{
    if (tuning != nullptr && tuning->hasSignature && tuning->*field > 0.f)
        return tuning->*field;
    return fallback;
}

// A special move's saved hit area (Character Animator), or null while the move
// keeps its coded size. The helpers return the saved value or `fallback`; the
// move's damage and its warning both read them, so what is warned is what hits.
const AttackTuning* TunedHitArea(const std::string& key);
float TunedHitRadius(const std::string& key, float fallback);
float TunedHitInner(const std::string& key, float fallback);
float TunedHitForward(const std::string& key, float fallback);
float TunedHitLength(const std::string& key, float fallback);
float TunedHitWidth(const std::string& key, float fallback);

namespace AttackTuningStore
{
    // nullptr = no saved file (use defaults). Results are cached.
    const AttackTuning* Get(const std::string& key);
    void Reload(const std::string& key);   // drop the cache entry after a save
    void ReloadAll();                      // after TuningFiles::Directory() changes
    void Parse(const std::string& text, AttackTuning& out);   // a file's text
    // Write a full attacktuning_<key>.txt (only the groups whose has* flag is set),
    // then refresh the cache so the change is live immediately.
    bool Save(const std::string& key, const AttackTuning& t);
    // The file text Save writes; matching text means the same tuning.
    std::string Serialize(const AttackTuning& t);
}
