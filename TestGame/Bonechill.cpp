#include "Bonechill.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "Character.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

// ---- Static member definitions ----------------------------------------------
Texture2D Bonechill::_sharedIdleAnim[Bonechill::kVariantCount]{};
Texture2D Bonechill::_sharedWalkAnim[Bonechill::kVariantCount]{};
Texture2D Bonechill::_sharedAttackAnim[Bonechill::kVariantCount]{};
Texture2D Bonechill::_sharedTakeDamageAnim[Bonechill::kVariantCount]{};
Texture2D Bonechill::_sharedDeathAnim[Bonechill::kVariantCount]{};
Sound     Bonechill::_sharedAttackSound{};
Sound     Bonechill::_sharedHurtSound{};
Sound     Bonechill::_sharedDeathSound{};
bool      Bonechill::_sharedResourcesLoaded = false;

namespace
{
    const char* kBonechillVariantSuffixes[4] = { "", "_B", "_C", "_D" };

    // All Bonechill sheets are stitched 6-frame strips of 48x48 pixels.
    constexpr int   kBonechillFrameCount = 6;
    constexpr float kBonechillFrameWidth = 48.f;
    constexpr float kBonechillFrameTime  = 1.f / 8.f;   // deliberate, armoured pace
}

// =============================================================================
Bonechill::Bonechill(Vector2 pos)
    : Enemy(pos)
{
}

Bonechill::~Bonechill() {}

// =============================================================================
void Bonechill::Init()
{
    EnsureSharedResourcesLoaded();

    _attackSound = _sharedAttackSound;
    _hurtSound   = _sharedHurtSound;
    _deathSound  = _sharedDeathSound;
    SetVariantTier(_variantTier);

    ResetForSpawn(_worldPos);
}

void Bonechill::SetVariantTier(int tier)
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
void Bonechill::ResetForSpawn(Vector2 pos)
{
    _worldPos          = pos;
    _worldPosLastFrame = pos;
    _homePos           = pos;
    _velocity          = Vector2Zero();
    _isActive          = true;

    SetIdleAnimation(false);
    _scale = 6.2f;                 // its body art is compact, so a higher scale
                                   // is needed to match the Ogre's on-screen mass

    _health      = 14.f;           // tankiest of the bruisers
    _maxHealth   = 14.f;
    _attackPower = 2.f;            // the threat is the chill, not raw damage
    _speed       = 130.f;
    _expValue    = 8;

    _attackRange     = 120.f;
    _attackDelay     = 1.5f;
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

void Bonechill::SetIdleAnimation(bool resetFrame)
{
    _texture    = _idleAnim;
    _width      = kBonechillFrameWidth;
    _height     = (float)_idleAnim.height;
    _updateTime = (_editorAnimFrameTimes[0] > 0.f) ? _editorAnimFrameTimes[0] : kBonechillFrameTime;
    _maxFrames  = kBonechillFrameCount;
    if (resetFrame) { _frame = 0; _runningTime = 0.f; }
}

// =============================================================================
void Bonechill::SetWaveScale(int /*wave*/)
{
    _health      = 14.f;
    _maxHealth   = 14.f;
    _attackPower = 2.f;
    _speed       = 130.f;
    _expValue    = 8;
}

// =============================================================================
// Grave chill: every landed melee hit slows the player.
void Bonechill::OnMeleeHitPlayer(Character* target)
{
    if (target == nullptr)
        return;
    target->ApplyChill(kChillDuration, kChillSpeedMult);
}

// =============================================================================
Rectangle Bonechill::GetCollisionRec() const
{
    Rectangle animBodyRect;
    if (GetAnimBodyRectWorld(animBodyRect))
        return animBodyRect;
    if (_hasTunedCollision)
        return GetTunedCollisionRec();

    // Live from the current draw scale so the body grows with the sprite.
    float halfW = kBonechillFrameWidth * _scale * 0.5f;
    float halfH = (_idleAnim.id > 0 ? (float)_idleAnim.height : _height) * _scale * 0.5f;
    float boxW  = halfW * 1.00f;
    float boxH  = halfH * 1.20f;
    return Rectangle{
        _worldPos.x - boxW * 0.5f,
        _worldPos.y - boxH * 0.5f + halfH * 0.12f,
        boxW, boxH
    };
}

Capsule2D Bonechill::GetCapsule() const
{
    Capsule2D animBodyCapsule;
    if (GetAnimBodyCapsuleWorld(animBodyCapsule))
        return animBodyCapsule;

    float radius = kBonechillFrameWidth * _scale * 0.22f;
    return Capsule2D{
        { _worldPos.x, _worldPos.y + radius * 0.25f },
        18.f,
        radius
    };
}

// =============================================================================
void Bonechill::PlayAttackSound()
{
    float pitch = GetRandomValue(85, 110) / 100.f;
    SetSoundPitch(_attackSound, pitch);
    SetSoundVolume(_attackSound, 0.6f);
    PlaySound(_attackSound);
}

void Bonechill::PlayDeathSound()
{
    SfxBank::Get().Play(SfxId::DeathSmall, 0.7f, true);
}

// =============================================================================
void Bonechill::EnsureSharedResourcesLoaded()
{
    if (_sharedResourcesLoaded)
        return;

    for (int variant = 0; variant < kVariantCount; variant++)
    {
        const char* suffix = kBonechillVariantSuffixes[variant];
        _sharedIdleAnim[variant]       = LoadTexture(AssetPath(TextFormat("Enemy/BonechillIdle%s.png",   suffix)).c_str());
        _sharedWalkAnim[variant]       = LoadTexture(AssetPath(TextFormat("Enemy/BonechillWalk%s.png",   suffix)).c_str());
        _sharedAttackAnim[variant]     = LoadTexture(AssetPath(TextFormat("Enemy/BonechillAttack%s.png", suffix)).c_str());
        _sharedTakeDamageAnim[variant] = LoadTexture(AssetPath(TextFormat("Enemy/BonechillHurt%s.png",   suffix)).c_str());
        _sharedDeathAnim[variant]      = LoadTexture(AssetPath(TextFormat("Enemy/BonechillDeath%s.png",  suffix)).c_str());
    }
    _sharedAttackSound = LoadSound(AssetPath("Sounds/Impact_Ice.wav").c_str());
    _sharedHurtSound   = LoadSound(AssetPath("Sounds/SmallMonsterDamage.ogg").c_str());
    _sharedDeathSound  = LoadSound(AssetPath("Sounds/MonsterDeath.ogg").c_str());
    _sharedResourcesLoaded = true;
}

void Bonechill::UnloadSharedResources()
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
