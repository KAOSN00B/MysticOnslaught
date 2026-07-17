#include "Stormclub.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "Character.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

// ---- Static member definitions ----------------------------------------------
Texture2D Stormclub::_sharedIdleAnim[Stormclub::kVariantCount]{};
Texture2D Stormclub::_sharedWalkAnim[Stormclub::kVariantCount]{};
Texture2D Stormclub::_sharedAttackAnim[Stormclub::kVariantCount]{};
Texture2D Stormclub::_sharedTakeDamageAnim[Stormclub::kVariantCount]{};
Texture2D Stormclub::_sharedDeathAnim[Stormclub::kVariantCount]{};
Sound     Stormclub::_sharedAttackSound{};
Sound     Stormclub::_sharedHurtSound{};
Sound     Stormclub::_sharedDeathSound{};
bool      Stormclub::_sharedResourcesLoaded = false;

namespace
{
    const char* kStormclubVariantSuffixes[4] = { "", "_B", "_C", "_D" };

    // All Stormclub sheets are stitched 6-frame strips of 48x48 pixels.
    constexpr int   kStormclubFrameCount = 6;
    constexpr float kStormclubFrameWidth = 48.f;
    constexpr float kStormclubFrameTime  = 1.f / 8.f;   // big wind-up club swings
}

// =============================================================================
Stormclub::Stormclub(Vector2 pos)
    : Enemy(pos)
{
}

Stormclub::~Stormclub() {}

// =============================================================================
void Stormclub::Init()
{
    EnsureSharedResourcesLoaded();

    _attackSound = _sharedAttackSound;
    _hurtSound   = _sharedHurtSound;
    _deathSound  = _sharedDeathSound;
    SetVariantTier(_variantTier);

    ResetForSpawn(_worldPos);
}

void Stormclub::SetVariantTier(int tier)
{
    EnsureSharedResourcesLoaded();

    _variantTier = (tier < 0) ? 0 : (tier >= kVariantCount) ? kVariantCount - 1 : tier;

    _idleAnim       = _sharedIdleAnim[_variantTier];
    _walkAnim       = _sharedWalkAnim[_variantTier];
    _attackAnim     = _sharedAttackAnim[_variantTier];
    _takeDamageAnim = _sharedTakeDamageAnim[_variantTier];
    _deathAnim      = _sharedDeathAnim[_variantTier];

    SetIdleAnimation(false);
}

// =============================================================================
void Stormclub::ResetForSpawn(Vector2 pos)
{
    _worldPos          = pos;
    _worldPosLastFrame = pos;
    _homePos           = pos;
    _velocity          = Vector2Zero();
    _isActive          = true;

    SetIdleAnimation(false);
    _scale = 4.8f;                 // elite-Ogre sized

    _health      = 12.f;
    _maxHealth   = 12.f;
    _attackPower = 3.f;            // the club hits hard AND displaces you
    _speed       = 160.f;
    _expValue    = 8;

    _attackRange     = 125.f;
    _attackDelay     = 1.6f;       // long recovery between smashes
    _attackCooldown  = 0.f;

    _frame       = 0;
    _runningTime = 0.f;

    _hitTimer                 = 0.f;
    _deathTimer               = 0.4f;
    _freezeTimer              = 0.f;
    _isCharged                = false;
    _chargeNextStunTime       = 0.f;
    _electricChargeTotalTimer = 0.f;
    _isEliteMiniboss          = false;
    _isInvulnerable           = false;
    _leapInvulnerable         = false;
    _takingDamage = false;
    _attacking    = false;
    _dying        = false;

    _forcedPushActive    = false;
    _forcedPushDirection = Vector2Zero();
    _forcedPushSpeed     = 0.f;

    _pendingBurns.clear();
    _waypoints.clear();
    _waypointIndex = 0;

    ResetTuningState();
    ApplyStoredTuning();
}

void Stormclub::SetIdleAnimation(bool resetFrame)
{
    _texture    = _idleAnim;
    _width      = kStormclubFrameWidth;
    _height     = (float)_idleAnim.height;
    _updateTime = (_editorAnimFrameTimes[0] > 0.f) ? _editorAnimFrameTimes[0] : kStormclubFrameTime;
    _maxFrames  = kStormclubFrameCount;
    if (resetFrame) { _frame = 0; _runningTime = 0.f; }
}

// =============================================================================
void Stormclub::SetWaveScale(int /*wave*/)
{
    _health      = 12.f;
    _maxHealth   = 12.f;
    _attackPower = 3.f;
    _speed       = 160.f;
    _expValue    = 8;
}

// =============================================================================
// Thunderous smash: a landed club hit blasts the player away from the ogre.
void Stormclub::OnMeleeHitPlayer(Character* target)
{
    if (target == nullptr)
        return;
    Vector2 away = Vector2Subtract(target->GetWorldPos(), _worldPos);
    target->ApplyKnockbackImpulse(away, kSmashKnockbackSpeed);
}

// =============================================================================
Rectangle Stormclub::GetCollisionRec() const
{
    Rectangle animBodyRect;
    if (GetAnimBodyRectWorld(animBodyRect))
        return animBodyRect;
    if (_hasTunedCollision)
        return GetTunedCollisionRec();

    // Live from the current draw scale so the body grows with the sprite.
    float halfW = kStormclubFrameWidth * _scale * 0.5f;
    float halfH = (_idleAnim.id > 0 ? (float)_idleAnim.height : _height) * _scale * 0.5f;
    float boxW  = halfW * 1.05f;
    float boxH  = halfH * 1.15f;
    return Rectangle{
        _worldPos.x - boxW * 0.5f,
        _worldPos.y - boxH * 0.5f + halfH * 0.12f,
        boxW, boxH
    };
}

Capsule2D Stormclub::GetCapsule() const
{
    Capsule2D animBodyCapsule;
    if (GetAnimBodyCapsuleWorld(animBodyCapsule))
        return animBodyCapsule;

    float radius = kStormclubFrameWidth * _scale * 0.22f;
    return Capsule2D{
        { _worldPos.x, _worldPos.y + radius * 0.25f },
        18.f,
        radius
    };
}

// =============================================================================
void Stormclub::PlayAttackSound()
{
    // Lightning crack on the swing — low pitch so it reads as thunder.
    float pitch = GetRandomValue(70, 90) / 100.f;
    SetSoundPitch(_attackSound, pitch);
    SetSoundVolume(_attackSound, 0.55f);
    PlaySound(_attackSound);
}

void Stormclub::PlayDeathSound()
{
    SfxBank::Get().Play(SfxId::DeathSmall, 0.75f, true);
}

// =============================================================================
void Stormclub::EnsureSharedResourcesLoaded()
{
    if (_sharedResourcesLoaded)
        return;

    for (int variant = 0; variant < kVariantCount; variant++)
    {
        const char* suffix = kStormclubVariantSuffixes[variant];
        _sharedIdleAnim[variant]       = LoadTexture(AssetPath(TextFormat("Enemy/StormclubIdle%s.png",   suffix)).c_str());
        _sharedWalkAnim[variant]       = LoadTexture(AssetPath(TextFormat("Enemy/StormclubWalk%s.png",   suffix)).c_str());
        _sharedAttackAnim[variant]     = LoadTexture(AssetPath(TextFormat("Enemy/StormclubAttack%s.png", suffix)).c_str());
        _sharedTakeDamageAnim[variant] = LoadTexture(AssetPath(TextFormat("Enemy/StormclubHurt%s.png",   suffix)).c_str());
        _sharedDeathAnim[variant]      = LoadTexture(AssetPath(TextFormat("Enemy/StormclubDeath%s.png",  suffix)).c_str());
    }
    _sharedAttackSound = LoadSound(AssetPath("Sounds/LightningSound.ogg").c_str());
    _sharedHurtSound   = LoadSound(AssetPath("Sounds/SmallMonsterDamage.ogg").c_str());
    _sharedDeathSound  = LoadSound(AssetPath("Sounds/MonsterDeath.ogg").c_str());
    _sharedResourcesLoaded = true;
}

void Stormclub::UnloadSharedResources()
{
    if (!_sharedResourcesLoaded)
        return;

    for (int variant = 0; variant < kVariantCount; variant++)
    {
        UnloadTexture(_sharedIdleAnim[variant]);
        UnloadTexture(_sharedWalkAnim[variant]);
        UnloadTexture(_sharedAttackAnim[variant]);
        UnloadTexture(_sharedTakeDamageAnim[variant]);
        UnloadTexture(_sharedDeathAnim[variant]);
        _sharedIdleAnim[variant]       = Texture2D{};
        _sharedWalkAnim[variant]       = Texture2D{};
        _sharedAttackAnim[variant]     = Texture2D{};
        _sharedTakeDamageAnim[variant] = Texture2D{};
        _sharedDeathAnim[variant]      = Texture2D{};
    }
    UnloadSound(_sharedAttackSound);
    UnloadSound(_sharedHurtSound);
    UnloadSound(_sharedDeathSound);
    _sharedAttackSound = Sound{};
    _sharedHurtSound   = Sound{};
    _sharedDeathSound  = Sound{};
    _sharedResourcesLoaded = false;
}
