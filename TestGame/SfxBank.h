#pragma once

#include "raylib.h"
#include "AbilityType.h"
#include "PlayerClass.h"
#include <unordered_map>

// ─────────────────────────────────────────────────────────────────────────────
// SfxBank — one shared, categorized sound library so gameplay actions stop
// sharing placeholders (sword-swing for staffs, PlayerDeath for enemies...).
//
// Design (per the sound-design audit): sounds are grouped by MEANING, not by
// call site. Callers ask for an intent — "play the cast for this element",
// "play this ability", "this creature family just died" — and the bank resolves
// it to a clip with graceful fallbacks (missing ability → element → generic).
// Every clip is optional: if its file is missing the call is a silent no-op, so
// partial sound coverage never crashes or blocks a build.
//
// Global singleton because the play sites are scattered across Character, every
// Enemy/boss, and Engine — matching how raylib Sounds are already used.
// ─────────────────────────────────────────────────────────────────────────────

enum class SfxElement { Physical, Fire, Ice, Electric, Holy, Shadow, Poison, Earth, Arcane };

// Coarse "what is this thing made of" bucket for hurt/death sounds.
enum class CreatureFamily { Flesh, Small, Slime, Spectral, Metal, Plant, Beast, Boss };

// Stable ids for every clip the bank can hold. Add an entry here + a row in
// SfxBank::Load() to introduce a new sound.
enum class SfxId
{
    None = 0,
    // Player basics
    BasicStaff, BasicBow, BasicDagger, BasicHoly, BasicShadow, BasicSword,
    // Element cast / impact / ultimate
    CastFire, CastIce, CastElectric, CastHoly, CastShadow, CastEarth, CastArcane,
    ImpactFire, ImpactIce, ImpactElectric, ImpactHoly, ImpactShadow, ImpactPhysical, ImpactPoison,
    UltFire, UltIce, UltElectric,
    // Ability signatures
    SkillAxeThrow, SkillKnifeThrow, SkillArrowRain, SkillBladeRain, BuffCast,
    WarCryRoar, SkillGroundSlam, SkillEarthshatter, SkillTeleport, SkillSmoke,
    SkillVialThrow, TrapWood, TrapMagic, TrapArm, SkillHellfire, SkillCurse, ZonePoison,
    // Creature families
    DeathFlesh, DeathSmall, DeathSpectral, DeathPlant, HitMetal, HitSlime, EnemyAggro,
    // Boss
    BossImpact, BossRoar, BossSlam,
    // UI / pickup / progression
    PickupGold, PickupMagic, PickupCell, LevelUp, AbilityLearn, UIConfirm, UISelect,
    Count
};

class SfxBank
{
public:
    static SfxBank& Get();

    void Load();     // called once from Engine::Init
    void Unload();   // called from Engine shutdown

    // Master SFX volume (0..1) — multiplied into every play. Wire to the
    // existing SFX slider alongside AudioManager::SetSfxVolumeScale.
    void  SetVolumeScale(float v) { _volumeScale = (v < 0.f) ? 0.f : (v > 1.f) ? 1.f : v; }
    float GetVolumeScale() const  { return _volumeScale; }

    // Low-level: play a specific clip. Safe no-op if the clip is absent.
    void Play(SfxId id, float volume = 1.f, bool pitchJitter = true);

    // ── Intent-based helpers (what callers actually use) ──────────────────────
    void PlayBasicAttack(PlayerClass cls);       // per-class weapon basic
    void PlayAbilityCast(AbilityType ability);   // any ability → its signature/element
    void PlayElementCast(SfxElement e, float volume = 1.f);
    void PlayElementImpact(SfxElement e, float volume = 1.f);
    // Maps a mage element ability (Fire/Ice/Electric*) to its impact clip.
    void PlayProjectileImpact(AbilityType element, float volume = 0.5f);
    void PlayCreatureDeath(CreatureFamily family);
    void PlayCreatureHurt(CreatureFamily family);

private:
    SfxBank() = default;
    Sound* Find(SfxId id);                       // nullptr if not loaded/missing

    std::unordered_map<int, Sound> _sounds;      // SfxId -> Sound (only loaded ones)
    bool   _loaded        = false;
    float  _volumeScale   = 1.f;
    double _lastImpactTime = 0.0;                 // throttles element-impact spam
};
