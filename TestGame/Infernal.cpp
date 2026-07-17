#include "Infernal.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "Character.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

// ---- Static member definitions ----------------------------------------------
Texture2D Infernal::_sharedIdleAnim[Infernal::kVariantCount]{};
Texture2D Infernal::_sharedWalkAnim[Infernal::kVariantCount]{};
Texture2D Infernal::_sharedAttackAnim[Infernal::kVariantCount]{};
Texture2D Infernal::_sharedTakeDamageAnim[Infernal::kVariantCount]{};
Texture2D Infernal::_sharedDeathAnim[Infernal::kVariantCount]{};
Sound     Infernal::_sharedAttackSound{};
Sound     Infernal::_sharedHurtSound{};
Sound     Infernal::_sharedDeathSound{};
bool      Infernal::_sharedResourcesLoaded = false;

namespace
{
    // Colour-variant progression by zone (pack letters A/B/C/D -> tiers 0..3).
    const char* kInfVariantSuffixes[4] = { "", "_B", "_C", "_D" };

    // All Infernal sheets are stitched 6-frame strips of 48x48 pixels.
    constexpr int   kInfFrameCount = 6;
    constexpr float kInfFrameWidth = 48.f;
    constexpr float kInfFrameTime  = 1.f / 8.f;   // heavier/slower than a grunt
}

// =============================================================================
Infernal::Infernal(Vector2 pos)
    : Enemy(pos)
{
}

Infernal::~Infernal() {}

// =============================================================================
void Infernal::Init()
{
    EnsureSharedResourcesLoaded();

    _attackSound = _sharedAttackSound;
    _hurtSound   = _sharedHurtSound;
    _deathSound  = _sharedDeathSound;
    SetVariantTier(_variantTier);

    ResetForSpawn(_worldPos);
}

void Infernal::SetVariantTier(int tier)
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
void Infernal::ResetForSpawn(Vector2 pos)
{
    _worldPos          = pos;
    _worldPosLastFrame = pos;
    _homePos           = pos;
    _velocity          = Vector2Zero();
    _isActive          = true;

    SetIdleAnimation(false);
    _scale = 4.8f;                 // hulking — on par with the elite Ogre

    _health      = 12.f;
    _maxHealth   = 12.f;
    _attackPower = 3.f;
    _speed       = 150.f;          // heavy, but no longer a crawl
    _expValue    = 8;

    _attackRange     = 120.f;
    _attackDelay     = 1.4f;       // ponderous swing cadence
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

void Infernal::SetIdleAnimation(bool resetFrame)
{
    _texture    = _idleAnim;
    _width      = kInfFrameWidth;
    _height     = (float)_idleAnim.height;
    _updateTime = (_editorAnimFrameTimes[0] > 0.f) ? _editorAnimFrameTimes[0] : kInfFrameTime;
    _maxFrames  = kInfFrameCount;
    if (resetFrame) { _frame = 0; _runningTime = 0.f; }
}

// =============================================================================
void Infernal::SetWaveScale(int /*wave*/)
{
    _health      = 12.f;
    _maxHealth   = 12.f;
    _attackPower = 3.f;
    _speed       = 150.f;
    _expValue    = 8;
}

// =============================================================================
// Fire strike: every landed melee hit sets the player alight.
void Infernal::OnMeleeHitPlayer(Character* target)
{
    if (target == nullptr)
        return;
    target->ApplyBurnTicks(kBurnTickDelay, kBurnTickCount, kBurnDamagePerTick, _worldPos);
}

// =============================================================================
Rectangle Infernal::GetCollisionRec() const
{
    Rectangle animBodyRect;
    if (GetAnimBodyRectWorld(animBodyRect))
        return animBodyRect;
    if (_hasTunedCollision)
        return GetTunedCollisionRec();

    // Computed live from the current draw scale (not cached) so the body grows
    // with the sprite — the elite buff / a bigger scale gets a bigger hitbox.
    float halfW = kInfFrameWidth * _scale * 0.5f;
    float halfH = (_idleAnim.id > 0 ? (float)_idleAnim.height : _height) * _scale * 0.5f;
    float boxW  = halfW * 1.05f;                 // body ~52% of the sprite width
    float boxH  = halfH * 1.20f;
    return Rectangle{
        _worldPos.x - boxW * 0.5f,
        _worldPos.y - boxH * 0.5f + halfH * 0.12f,   // sit on the body, not the head
        boxW, boxH
    };
}

Capsule2D Infernal::GetCapsule() const
{
    Capsule2D animBodyCapsule;
    if (GetAnimBodyCapsuleWorld(animBodyCapsule))
        return animBodyCapsule;

    // Live from scale so separation matches the current size.
    float radius = kInfFrameWidth * _scale * 0.22f;
    return Capsule2D{
        { _worldPos.x, _worldPos.y + radius * 0.25f },
        18.f,
        radius
    };
}

// =============================================================================
void Infernal::PlayAttackSound()
{
    float pitch = GetRandomValue(70, 95) / 100.f;   // low, heavy
    SetSoundPitch(_attackSound, pitch);
    SetSoundVolume(_attackSound, 0.7f);
    PlaySound(_attackSound);
}

void Infernal::PlayDeathSound()
{
    SfxBank::Get().Play(SfxId::DeathSmall, 0.75f, true);
}

// =============================================================================
void Infernal::EnsureSharedResourcesLoaded()
{
    if (_sharedResourcesLoaded)
        return;

    for (int variant = 0; variant < kVariantCount; variant++)
    {
        const char* suffix = kInfVariantSuffixes[variant];
        _sharedIdleAnim[variant]       = LoadTexture(AssetPath(TextFormat("Enemy/InfernalIdle%s.png",   suffix)).c_str());
        _sharedWalkAnim[variant]       = LoadTexture(AssetPath(TextFormat("Enemy/InfernalWalk%s.png",   suffix)).c_str());
        _sharedAttackAnim[variant]     = LoadTexture(AssetPath(TextFormat("Enemy/InfernalAttack%s.png", suffix)).c_str());
        _sharedTakeDamageAnim[variant] = LoadTexture(AssetPath(TextFormat("Enemy/InfernalHurt%s.png",   suffix)).c_str());
        _sharedDeathAnim[variant]      = LoadTexture(AssetPath(TextFormat("Enemy/InfernalDeath%s.png",  suffix)).c_str());
    }
    _sharedAttackSound = LoadSound(AssetPath("Sounds/GS1_Spell_Fire.ogg").c_str());
    _sharedHurtSound   = LoadSound(AssetPath("Sounds/SmallMonsterDamage.ogg").c_str());
    _sharedDeathSound  = LoadSound(AssetPath("Sounds/PlayerDeath.ogg").c_str());
    _sharedResourcesLoaded = true;
}

void Infernal::UnloadSharedResources()
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
