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

        // Imported library (see Sounds/MysticExpansion/manifest.json for provenance).
        { SfxId::FootGrassA,      "Sounds/MysticExpansion/Footsteps/Footsteps_Grass_01.wav" },
        { SfxId::FootGrassB,      "Sounds/MysticExpansion/Footsteps/Footsteps_Grass_02.wav" },
        { SfxId::FootStoneA,      "Sounds/MysticExpansion/Footsteps/Footsteps_Stone_Tiles_01.wav" },
        { SfxId::FootStoneB,      "Sounds/MysticExpansion/Footsteps/Footsteps_Stone_Tiles_02.wav" },
        { SfxId::FootDirtA,       "Sounds/MysticExpansion/Footsteps/Footsteps_Dirt_01.wav" },
        { SfxId::FootDirtB,       "Sounds/MysticExpansion/Footsteps/Footsteps_Dirt_02.wav" },
        { SfxId::CoinA,           "Sounds/MysticExpansion/Licensed/Coin_Accessory_A.wav" },
        { SfxId::CoinB,           "Sounds/MysticExpansion/Licensed/Coin_Accessory_B.wav" },
        { SfxId::ChestOpen,       "Sounds/MysticExpansion/Interactions/GS2_Treasure_Chest_Open.wav" },
        { SfxId::DoorOpen,        "Sounds/MysticExpansion/Interactions/GS2_Door_Open_2.wav" },
        { SfxId::DashA,           "Sounds/MysticExpansion/Licensed/Dash_Whoosh_A.wav" },
        { SfxId::DashB,           "Sounds/MysticExpansion/Licensed/Dash_Whoosh_B.wav" },
        { SfxId::ArmorLight,      "Sounds/MysticExpansion/Armor/Armor_Hit_Light_A.wav" },
        { SfxId::ArmorHeavy,      "Sounds/MysticExpansion/Armor/Armor_Hit_Heavy_A.wav" },
        { SfxId::RelicReveal,     "Sounds/MysticExpansion/Stingers/Mysterious_Box_001.wav" },
        { SfxId::RelicGained,     "Sounds/MysticExpansion/Item Tiers/High_Tier_Item_A.wav" },
        { SfxId::UpgradeBought,   "Sounds/MysticExpansion/Upgrades/Upgrade_High_Tier_Item_A.wav" },
        { SfxId::ConsumableA,     "Sounds/MysticExpansion/Item Types/Item_Consumable_A.wav" },
        { SfxId::ConsumableB,     "Sounds/MysticExpansion/Item Types/Item_Consumable_B.wav" },
        { SfxId::GuiCancel,       "Sounds/MysticExpansion/Licensed/Denied_Cue_A.wav" },
        { SfxId::HealA,           "Sounds/MysticExpansion/Licensed/Heal_Upgrade_A.wav" },
        { SfxId::HealB,           "Sounds/MysticExpansion/Licensed/Heal_Upgrade_B.wav" },
        { SfxId::ReviveDust,      "Sounds/MysticExpansion/Spells/Angel_Dust_001.wav" },
        { SfxId::BossArrival,     "Sounds/MysticExpansion/Stingers/Drums_Of_Fate_001.wav" },
        { SfxId::SpellSmite,      "Sounds/MysticExpansion/Spells/Smite_002.wav" },
        { SfxId::SpellFog,        "Sounds/MysticExpansion/Spells/Fog_Disappear_001.wav" },
        { SfxId::SpellHexproof,   "Sounds/MysticExpansion/Spells/Hexproof_001.wav" },
        { SfxId::SpellIntoDarkness,"Sounds/MysticExpansion/Spells/Into_The_Darkness_001.wav" },
        { SfxId::ShotFate,        "Sounds/MysticExpansion/Combat/Fate_Shot_002.wav" },
        { SfxId::StrikeDestiny,   "Sounds/MysticExpansion/Combat/Blade_Of_Destiny_002.wav" },
        { SfxId::KillStompA,      "Sounds/MysticExpansion/Licensed/Kill_Stomp_A.wav" },
        { SfxId::KillStompB,      "Sounds/MysticExpansion/Licensed/Kill_Stomp_B.wav" },
        { SfxId::KillStompC,      "Sounds/MysticExpansion/Licensed/Kill_Stomp_C.wav" },
        { SfxId::KillExecution,   "Sounds/MysticExpansion/Licensed/Kill_Execution_A.wav" },
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

void SfxBank::PlayEither(SfxId a, SfxId b, float volume, SfxId fallback)
{
    Sound* first = Find(a);
    Sound* second = Find(b);
    if (first && second) { Play(GetRandomValue(0, 1) == 0 ? a : b, volume); return; }
    if (first)           { Play(a, volume); return; }
    if (second)          { Play(b, volume); return; }
    if (fallback != SfxId::None) Play(fallback, volume);
}

bool SfxBank::PlayFootstep(float volume)
{
    SfxId a = SfxId::None, b = SfxId::None;
    switch (_footSurface)
    {
    case FootSurface::Grass: a = SfxId::FootGrassA; b = SfxId::FootGrassB; break;
    case FootSurface::Stone: a = SfxId::FootStoneA; b = SfxId::FootStoneB; break;
    case FootSurface::Dirt:  a = SfxId::FootDirtA;  b = SfxId::FootDirtB;  break;
    default: return false;
    }
    if (!Find(a) && !Find(b)) return false;
    PlayEither(a, b, volume);
    return true;
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

void SfxBank::PlayKillCrunch(bool heavy)
{
    const double now = GetTime();
    if (now - _lastCrunchTime < 0.05) return;
    _lastCrunchTime = now;
    const SfxId stomps[] = { SfxId::KillStompA, SfxId::KillStompB, SfxId::KillStompC };
    Play(stomps[GetRandomValue(0, 2)], heavy ? 0.75f : 0.55f);
    if (heavy) Play(SfxId::KillExecution, 0.45f);
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

// ── Elite signature intents ──────────────────────────────────────────────────
// Archetype order matches EliteArchetype: Ogre, Infernal, Bonechill, Stormclub,
// Venomfang. Play() itself is a safe no-op for missing clips, so these mappings
// double as the fallback chain (element cast/impact clips are all loaded).
void SfxBank::PlayEliteTelegraph(int eliteArchetype, float volume)
{
    switch (eliteArchetype)
    {
    case 1:  Play(SfxId::CastFire, volume);     break;   // Infernal
    case 2:  Play(SfxId::CastIce, volume);      break;   // Bonechill
    case 3:  Play(SfxId::CastElectric, volume); break;   // Stormclub
    default: Play(SfxId::EnemyAggro, volume);   break;   // Ogre / Venomfang
    }
}

void SfxBank::PlayEliteImpact(int eliteArchetype, float volume)
{
    switch (eliteArchetype)
    {
    case 1:  Play(SfxId::ImpactFire, volume);     break;   // Infernal
    case 2:  Play(SfxId::ImpactIce, volume);      break;   // Bonechill
    case 3:  Play(SfxId::ImpactElectric, volume); break;   // Stormclub
    case 4:  Play(SfxId::ImpactPoison, volume);   break;   // Venomfang
    default: Play(SfxId::BossImpact, volume);     break;   // Ogre
    }
}

void SfxBank::PlayElitePhase(float volume)
{
    Play(SfxId::BossRoar, volume);
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
    case AbilityType::Backstab:    PlayEither(SfxId::StrikeDestiny, SfxId::StrikeDestiny, 0.6f, SfxId::BasicDagger); break;
    case AbilityType::SmokeBomb:   PlayEither(SfxId::SpellFog, SfxId::SpellFog, 0.55f, SfxId::SkillSmoke); break;
    case AbilityType::DeathMark:   PlayElementCast(SfxElement::Shadow, 0.8f); break;
    case AbilityType::BladeDance:  case AbilityType::RainOfBlades: Play(SfxId::SkillBladeRain, 0.7f); break;

    // ── Hunter ─────────────────────────────────────────────────────────────
    case AbilityType::PiercingShot: PlayEither(SfxId::ShotFate, SfxId::ShotFate, 0.6f, SfxId::BasicBow); break;
    case AbilityType::Multishot: case AbilityType::Volley: Play(SfxId::BasicBow, 0.65f); break;
    case AbilityType::FrostTrap:     Play(SfxId::TrapMagic, 0.6f); break;
    case AbilityType::ExplosiveArrow:Play(SfxId::TrapWood,  0.6f); break;
    case AbilityType::Roll:          Play(SfxId::BuffCast,  0.4f); break;
    case AbilityType::ArrowStorm: case AbilityType::PiercingBarrage: Play(SfxId::SkillArrowRain, 0.8f); break;
    case AbilityType::Deadeye:       Play(SfxId::BuffCast,  0.6f); break;

    // ── Paladin (holy) ─────────────────────────────────────────────────────
    case AbilityType::Smite:
        PlayEither(SfxId::SpellSmite, SfxId::SpellSmite, 0.55f, SfxId::CastHoly); break;
    case AbilityType::HolyBolt: case AbilityType::Consecrate:
        PlayElementCast(SfxElement::Holy); break;
    case AbilityType::ShieldOfFaith:
        PlayEither(SfxId::SpellHexproof, SfxId::SpellHexproof, 0.55f, SfxId::BuffCast); break;
    case AbilityType::LayOnHands:
        PlayEither(SfxId::HealA, SfxId::HealB, 0.55f, SfxId::BuffCast); break;
    case AbilityType::AvengingWrath:
        Play(SfxId::BuffCast, 0.6f); break;
    case AbilityType::HammerThrow:   Play(SfxId::SkillAxeThrow, 0.6f); break;
    case AbilityType::DivineStorm: case AbilityType::HammerOfJustice:
        PlayElementCast(SfxElement::Holy, 0.85f); break;

    // ── Warlock (dark) ─────────────────────────────────────────────────────
    case AbilityType::ShadowBolt: case AbilityType::DrainLife: case AbilityType::SoulSiphon:
        PlayElementCast(SfxElement::Shadow); break;
    case AbilityType::Curse:          PlayEither(SfxId::SpellIntoDarkness, SfxId::SpellIntoDarkness, 0.5f, SfxId::SkillCurse); break;
    case AbilityType::CorruptionPool: Play(SfxId::ZonePoison,   0.6f); break;
    case AbilityType::Hellfire:       Play(SfxId::SkillHellfire, 0.7f); break;
    case AbilityType::DemonForm:      Play(SfxId::WarCryRoar,   0.75f); break;
    case AbilityType::Cataclysm: case AbilityType::ShadowNova:
        PlayElementCast(SfxElement::Shadow, 0.85f); break;

    default: PlayElementCast(SfxElement::Arcane, 0.5f); break;
    }
}
