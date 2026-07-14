#include "SfxBank.h"
#include "AssetPaths.h"
#include <cstdlib>

SfxBank& SfxBank::Get()
{
    static SfxBank instance;
    return instance;
}

namespace
{
    struct SfxEntry { SfxId id; const char* file; };

    // SfxId -> file under Sounds/. Imported clips are .wav (raylib decodes both
    // wav and ogg natively on desktop and web); BasicSword reuses the existing
    // ogg. A missing file just leaves that id unloaded (silent no-op).
    const SfxEntry kSfxTable[] = {
        { SfxId::BasicFireBolt,   "Sounds/GS1_Spell_Fire.ogg" },
        { SfxId::BasicStaff,      "Sounds/Basic_Staff.wav" },
        { SfxId::BasicBow,        "Sounds/Basic_Bow.wav" },
        { SfxId::BasicDagger,     "Sounds/Basic_Dagger.wav" },
        { SfxId::BasicHoly,       "Sounds/Basic_Holy.wav" },
        { SfxId::BasicShadow,     "Sounds/Basic_Shadow.wav" },
        { SfxId::BasicSword,      "Sounds/SwordSwipe.ogg" },

        { SfxId::CastFire,        "Sounds/Cast_Fire.wav" },
        { SfxId::CastIce,         "Sounds/Cast_Ice.wav" },
        { SfxId::CastElectric,    "Sounds/Cast_Electric.wav" },
        { SfxId::CastHoly,        "Sounds/Cast_Holy.wav" },
        { SfxId::CastShadow,      "Sounds/Cast_Shadow.wav" },
        { SfxId::CastEarth,       "Sounds/Cast_Earth.wav" },
        { SfxId::CastArcane,      "Sounds/Basic_Staff.wav" },   // arcane basic doubles as generic cast

        { SfxId::ImpactFire,      "Sounds/Impact_Fire.wav" },
        { SfxId::ImpactIce,       "Sounds/Impact_Ice.wav" },
        { SfxId::ImpactElectric,  "Sounds/Impact_Electric.wav" },
        { SfxId::ImpactHoly,      "Sounds/Impact_Holy.wav" },
        { SfxId::ImpactShadow,    "Sounds/Impact_Shadow.wav" },
        { SfxId::ImpactPhysical,  "Sounds/Impact_Physical.wav" },
        { SfxId::ImpactPoison,    "Sounds/Impact_Poison.wav" },

        { SfxId::UltFire,         "Sounds/Ult_Fire.wav" },
        { SfxId::UltIce,          "Sounds/Ult_Ice.wav" },
        { SfxId::UltElectric,     "Sounds/Ult_Electric.wav" },

        { SfxId::SkillAxeThrow,   "Sounds/Skill_AxeThrow.wav" },
        { SfxId::SkillKnifeThrow, "Sounds/Skill_KnifeThrow.wav" },
        { SfxId::SkillArrowRain,  "Sounds/Skill_ArrowRain.wav" },
        { SfxId::SkillBladeRain,  "Sounds/Skill_BladeRain.wav" },
        { SfxId::BuffCast,        "Sounds/Buff_Cast.wav" },
        { SfxId::WarCryRoar,      "Sounds/WarCry_Roar.wav" },
        { SfxId::SkillGroundSlam, "Sounds/Skill_GroundSlam.wav" },
        { SfxId::SkillEarthshatter,"Sounds/Skill_Earthshatter.wav" },
        { SfxId::SkillTeleport,   "Sounds/Skill_Teleport.wav" },
        { SfxId::SkillSmoke,      "Sounds/Skill_Smoke.wav" },
        { SfxId::SkillVialThrow,  "Sounds/Skill_VialThrow.wav" },
        { SfxId::TrapWood,        "Sounds/Trap_Wood.wav" },
        { SfxId::TrapMagic,       "Sounds/Trap_Magic.wav" },
        { SfxId::TrapArm,         "Sounds/Trap_Arm.wav" },
        { SfxId::SkillHellfire,   "Sounds/Skill_Hellfire.wav" },
        { SfxId::SkillCurse,      "Sounds/Skill_Curse.wav" },
        { SfxId::ZonePoison,      "Sounds/Zone_Poison.wav" },

        { SfxId::DeathFlesh,      "Sounds/Death_Flesh.wav" },
        { SfxId::DeathSmall,      "Sounds/Death_Small.wav" },
        { SfxId::DeathSpectral,   "Sounds/Death_Spectral.wav" },
        { SfxId::DeathPlant,      "Sounds/Death_Plant.wav" },
        { SfxId::HitMetal,        "Sounds/Hit_Metal.wav" },
        { SfxId::HitSlime,        "Sounds/Hit_Slime.wav" },
        { SfxId::EnemyAggro,      "Sounds/Enemy_Aggro.wav" },

        { SfxId::BossImpact,      "Sounds/Boss_Impact.wav" },
        { SfxId::BossRoar,        "Sounds/Boss_Roar.wav" },
        { SfxId::BossSlam,        "Sounds/Boss_Slam.wav" },

        { SfxId::PickupGold,      "Sounds/Pickup_Gold.wav" },
        { SfxId::PickupMagic,     "Sounds/Pickup_Magic.wav" },
        { SfxId::PickupCell,      "Sounds/Pickup_Cell.wav" },
        { SfxId::LevelUp,         "Sounds/LevelUp.wav" },
        { SfxId::AbilityLearn,    "Sounds/AbilityLearn.wav" },
        { SfxId::UIConfirm,       "Sounds/UI_Confirm.wav" },
        { SfxId::UISelect,        "Sounds/UI_Select.wav" },
    };
}

void SfxBank::Load()
{
    if (_loaded) return;
    for (const SfxEntry& e : kSfxTable)
    {
        Sound s = LoadSound(AssetPath(e.file).c_str());
        if (s.frameCount > 0)               // only keep clips that actually loaded
            _sounds[(int)e.id] = s;
    }
    _loaded = true;
}

void SfxBank::Unload()
{
    if (!_loaded) return;
    for (auto& [id, snd] : _sounds)
        UnloadSound(snd);
    _sounds.clear();
    _loaded = false;
}

Sound* SfxBank::Find(SfxId id)
{
    auto it = _sounds.find((int)id);
    return (it != _sounds.end()) ? &it->second : nullptr;
}

void SfxBank::Play(SfxId id, float volume, bool pitchJitter)
{
    Sound* s = Find(id);
    if (s == nullptr) return;               // clip absent → silent, never crashes
    SetSoundVolume(*s, volume * _volumeScale);
    // A little pitch variation stops rapid repeats (basic attacks, swarms of
    // deaths) from sounding like a machine gun.
    float pitch = pitchJitter ? (0.94f + (float)(rand() % 130) / 1000.f) : 1.f;
    SetSoundPitch(*s, pitch);
    PlaySound(*s);
}

// ── Intent helpers ────────────────────────────────────────────────────────────

void SfxBank::PlayBasicAttack(PlayerClass cls)
{
    switch (cls)
    {
    case PlayerClass::Mage:    Play(SfxId::BasicFireBolt, 0.5f); break;
    case PlayerClass::Warlock: Play(SfxId::BasicFireBolt, 0.5f); break;
    case PlayerClass::Hunter:  Play(SfxId::BasicBow,    0.6f); break;
    case PlayerClass::Rogue:   Play(SfxId::BasicDagger, 0.55f); break;
    case PlayerClass::Paladin: Play(SfxId::BasicHoly,   0.5f); break;
    case PlayerClass::Warrior:
    default:                   Play(SfxId::BasicSword,  0.5f); break;
    }
}

void SfxBank::PlayElementCast(SfxElement e, float volume)
{
    switch (e)
    {
    case SfxElement::Fire:     Play(SfxId::CastFire,     volume); break;
    case SfxElement::Ice:      Play(SfxId::CastIce,      volume); break;
    case SfxElement::Electric: Play(SfxId::CastElectric, volume); break;
    case SfxElement::Holy:     Play(SfxId::CastHoly,     volume); break;
    case SfxElement::Shadow:   Play(SfxId::CastShadow,   volume); break;
    case SfxElement::Earth:    Play(SfxId::CastEarth,    volume); break;
    case SfxElement::Poison:   Play(SfxId::ImpactPoison, volume); break;
    case SfxElement::Physical: Play(SfxId::BasicSword,   volume); break;
    case SfxElement::Arcane:
    default:                   Play(SfxId::CastArcane,   volume); break;
    }
}

void SfxBank::PlayElementImpact(SfxElement e, float volume)
{
    // Throttle: a spread fires 8 pellets that can all hit the same frame — one
    // impact clip per ~70ms keeps that from turning into a machine-gun.
    double now = GetTime();
    if (now - _lastImpactTime < 0.07) return;
    _lastImpactTime = now;

    switch (e)
    {
    case SfxElement::Fire:     Play(SfxId::ImpactFire,     volume); break;
    case SfxElement::Ice:      Play(SfxId::ImpactIce,      volume); break;
    case SfxElement::Electric: Play(SfxId::ImpactElectric, volume); break;
    case SfxElement::Holy:     Play(SfxId::ImpactHoly,     volume); break;
    case SfxElement::Shadow:   Play(SfxId::ImpactShadow,   volume); break;
    case SfxElement::Poison:   Play(SfxId::ImpactPoison,   volume); break;
    case SfxElement::Physical:
    default:                   Play(SfxId::ImpactPhysical, volume); break;
    }
}

void SfxBank::PlayProjectileImpact(AbilityType element, float volume)
{
    switch (element)
    {
    case AbilityType::FireSpread:  case AbilityType::FireBolt:  case AbilityType::FireUltimate:
        PlayElementImpact(SfxElement::Fire, volume); break;
    case AbilityType::IceSpread:   case AbilityType::IceBolt:   case AbilityType::IceUltimate:
        PlayElementImpact(SfxElement::Ice, volume); break;
    case AbilityType::ElectricSpread: case AbilityType::ElectricBolt: case AbilityType::ElectricUltimate:
        PlayElementImpact(SfxElement::Electric, volume); break;
    default: break;   // non-elemental projectiles use their own ability/impact sounds
    }
}

void SfxBank::PlayCreatureDeath(CreatureFamily family)
{
    switch (family)
    {
    case CreatureFamily::Slime:    Play(SfxId::HitSlime,      0.6f); break;
    case CreatureFamily::Spectral: Play(SfxId::DeathSpectral, 0.6f); break;
    case CreatureFamily::Metal:    Play(SfxId::HitMetal,      0.6f); break;
    case CreatureFamily::Plant:    Play(SfxId::DeathPlant,    0.6f); break;
    case CreatureFamily::Beast:    Play(SfxId::BossRoar,      0.55f); break;
    case CreatureFamily::Boss:     Play(SfxId::BossRoar,      0.7f); break;
    case CreatureFamily::Small:    Play(SfxId::DeathSmall,    0.55f); break;
    case CreatureFamily::Flesh:
    default:                       Play(SfxId::DeathFlesh,    0.55f); break;
    }
}

void SfxBank::PlayCreatureHurt(CreatureFamily family)
{
    switch (family)
    {
    case CreatureFamily::Slime:    Play(SfxId::HitSlime, 0.4f); break;
    case CreatureFamily::Metal:    Play(SfxId::HitMetal, 0.4f); break;
    case CreatureFamily::Spectral: Play(SfxId::DeathSpectral, 0.35f); break;
    default:                       Play(SfxId::DeathSmall, 0.35f); break;
    }
}

void SfxBank::PlayAbilityCast(AbilityType a)
{
    switch (a)
    {
    // ── Mage elemental ─────────────────────────────────────────────────────
    case AbilityType::FireSpread:  case AbilityType::FireBolt:  PlayElementCast(SfxElement::Fire); break;
    case AbilityType::IceSpread:   case AbilityType::IceBolt:   PlayElementCast(SfxElement::Ice); break;
    case AbilityType::ElectricSpread: case AbilityType::ElectricBolt: PlayElementCast(SfxElement::Electric); break;
    case AbilityType::FireUltimate:     Play(SfxId::UltFire,     0.8f); break;
    case AbilityType::IceUltimate:      Play(SfxId::UltIce,      0.8f); break;
    case AbilityType::ElectricUltimate: Play(SfxId::UltElectric, 0.8f); break;

    // ── Warrior ────────────────────────────────────────────────────────────
    case AbilityType::WarCleave: case AbilityType::Whirlwind:
    case AbilityType::Rend:      case AbilityType::ShieldBash: Play(SfxId::BasicSword, 0.6f); break;
    case AbilityType::ThrowingAxe:  Play(SfxId::SkillAxeThrow, 0.6f); break;
    case AbilityType::WarCry:       Play(SfxId::WarCryRoar,    0.6f); break;
    case AbilityType::GroundSlam:   Play(SfxId::SkillGroundSlam, 0.8f); break;
    case AbilityType::Rampage:      Play(SfxId::WarCryRoar,    0.75f); break;
    case AbilityType::Earthshatter: Play(SfxId::SkillEarthshatter, 0.8f); break;

    // ── Rogue ──────────────────────────────────────────────────────────────
    case AbilityType::FanOfKnives: case AbilityType::Eviscerate: Play(SfxId::SkillKnifeThrow, 0.55f); break;
    case AbilityType::Shadowstep:  Play(SfxId::SkillTeleport,  0.6f); break;
    case AbilityType::PoisonVial:  Play(SfxId::SkillVialThrow, 0.6f); break;
    case AbilityType::Backstab:    Play(SfxId::BasicDagger,    0.7f); break;
    case AbilityType::SmokeBomb:   Play(SfxId::SkillSmoke,     0.6f); break;
    case AbilityType::DeathMark:   PlayElementCast(SfxElement::Shadow, 0.8f); break;
    case AbilityType::BladeDance:  case AbilityType::RainOfBlades: Play(SfxId::SkillBladeRain, 0.7f); break;

    // ── Hunter ─────────────────────────────────────────────────────────────
    case AbilityType::PiercingShot: case AbilityType::Multishot: case AbilityType::Volley: Play(SfxId::BasicBow, 0.65f); break;
    case AbilityType::FrostTrap:     Play(SfxId::TrapMagic, 0.6f); break;
    case AbilityType::ExplosiveArrow:Play(SfxId::TrapWood,  0.6f); break;
    case AbilityType::Roll:          Play(SfxId::BuffCast,  0.4f); break;
    case AbilityType::ArrowStorm: case AbilityType::PiercingBarrage: Play(SfxId::SkillArrowRain, 0.8f); break;
    case AbilityType::Deadeye:       Play(SfxId::BuffCast,  0.6f); break;

    // ── Paladin (holy) ─────────────────────────────────────────────────────
    case AbilityType::Smite: case AbilityType::HolyBolt: case AbilityType::Consecrate:
        PlayElementCast(SfxElement::Holy); break;
    case AbilityType::ShieldOfFaith: case AbilityType::LayOnHands: case AbilityType::AvengingWrath:
        Play(SfxId::BuffCast, 0.6f); break;
    case AbilityType::HammerThrow:   Play(SfxId::SkillAxeThrow, 0.6f); break;
    case AbilityType::DivineStorm: case AbilityType::HammerOfJustice:
        PlayElementCast(SfxElement::Holy, 0.85f); break;

    // ── Warlock (dark) ─────────────────────────────────────────────────────
    case AbilityType::ShadowBolt: case AbilityType::DrainLife: case AbilityType::SoulSiphon:
        PlayElementCast(SfxElement::Shadow); break;
    case AbilityType::Curse:          Play(SfxId::SkillCurse,   0.6f); break;
    case AbilityType::CorruptionPool: Play(SfxId::ZonePoison,   0.6f); break;
    case AbilityType::Hellfire:       Play(SfxId::SkillHellfire, 0.7f); break;
    case AbilityType::DemonForm:      Play(SfxId::WarCryRoar,   0.75f); break;
    case AbilityType::Cataclysm: case AbilityType::ShadowNova:
        PlayElementCast(SfxElement::Shadow, 0.85f); break;

    default: PlayElementCast(SfxElement::Arcane, 0.5f); break;
    }
}
