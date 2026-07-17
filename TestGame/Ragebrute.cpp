#include "Ragebrute.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

// ---- Static member definitions ----------------------------------------------
Texture2D Ragebrute::_sharedIdleAnim[Ragebrute::kVariantCount]{};
Texture2D Ragebrute::_sharedWalkAnim[Ragebrute::kVariantCount]{};
Texture2D Ragebrute::_sharedAttackAnim[Ragebrute::kVariantCount]{};
Texture2D Ragebrute::_sharedTakeDamageAnim[Ragebrute::kVariantCount]{};
Texture2D Ragebrute::_sharedDeathAnim[Ragebrute::kVariantCount]{};
Sound     Ragebrute::_sharedAttackSound{};
Sound     Ragebrute::_sharedHurtSound{};
Sound     Ragebrute::_sharedDeathSound{};
bool      Ragebrute::_sharedResourcesLoaded = false;

namespace
{
    const char* kRagebruteVariantSuffixes[4] = { "", "_B", "_C", "_D" };

    // All Ragebrute sheets are stitched 6-frame strips of 48x48 pixels.
    constexpr int   kRagebruteFrameCount = 6;
    constexpr float kRagebruteFrameWidth = 48.f;
    constexpr float kRagebruteFrameTime  = 1.f / 9.f;
}

// =============================================================================
Ragebrute::Ragebrute(Vector2 pos)
    : Enemy(pos)
{
}

Ragebrute::~Ragebrute() {}

// =============================================================================
void Ragebrute::Init()
{
    EnsureSharedResourcesLoaded();

    _attackSound = _sharedAttackSound;
    _hurtSound   = _sharedHurtSound;
    _deathSound  = _sharedDeathSound;
    SetVariantTier(_variantTier);

    ResetForSpawn(_worldPos);
}

void Ragebrute::SetVariantTier(int tier)
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
void Ragebrute::ResetForSpawn(Vector2 pos)
{
    _worldPos          = pos;
    _worldPosLastFrame = pos;
    _homePos           = pos;
    _velocity          = Vector2Zero();
    _isActive          = true;

    SetIdleAnimation(false);
    _scale = 4.8f;                 // elite-Ogre sized

    _health      = 16.f;           // meaty — it is MEANT to reach its rage flip
    _maxHealth   = 16.f;
    _attackPower = 2.f;
    _speed       = 140.f;          // pre-rage; ApplyEnrage() multiplies by 1.5
    _expValue    = 9;

    _attackRange     = 115.f;
    _attackDelay     = 1.3f;       // pre-rage; ApplyEnrage() halves this
    _attackCooldown  = 0.f;

    _raging      = false;

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

void Ragebrute::SetIdleAnimation(bool resetFrame)
{
    _texture    = _idleAnim;
    _width      = kRagebruteFrameWidth;
    _height     = (float)_idleAnim.height;
    _updateTime = (_editorAnimFrameTimes[0] > 0.f) ? _editorAnimFrameTimes[0] : kRagebruteFrameTime;
    _maxFrames  = kRagebruteFrameCount;
    if (resetFrame) { _frame = 0; _runningTime = 0.f; }
}

// =============================================================================
void Ragebrute::SetWaveScale(int /*wave*/)
{
    _health      = 16.f;
    _maxHealth   = 16.f;
    _attackPower = 2.f;
    _speed       = 140.f;
    _expValue    = 9;
}

// =============================================================================
// Rage latch: crossing half HP permanently enrages it (until a fresh spawn).
// ApplyEnrage is the shared elite buff (+50% speed, half attack delay); the
// floating "ENRAGED" word rides the boss-callout channel, which the runtime
// polls for every enemy.
void Ragebrute::TakeDamage(int damage, Vector2 attackerPos)
{
    Enemy::TakeDamage(damage, attackerPos);

    if (!_raging && IsAlive() && !_dying &&
        _health <= _maxHealth * kRageHpFraction)
    {
        _raging = true;
        ApplyEnrage();
        RequestBossCallout("ENRAGED");
    }
}

// =============================================================================
Rectangle Ragebrute::GetCollisionRec() const
{
    Rectangle animBodyRect;
    if (GetAnimBodyRectWorld(animBodyRect))
        return animBodyRect;
    if (_hasTunedCollision)
        return GetTunedCollisionRec();

    // Live from the current draw scale so the body grows with the sprite.
    float halfW = kRagebruteFrameWidth * _scale * 0.5f;
    float halfH = (_idleAnim.id > 0 ? (float)_idleAnim.height : _height) * _scale * 0.5f;
    float boxW  = halfW * 1.10f;   // wide, hunched slab
    float boxH  = halfH * 1.05f;
    return Rectangle{
        _worldPos.x - boxW * 0.5f,
        _worldPos.y - boxH * 0.5f + halfH * 0.15f,
        boxW, boxH
    };
}

Capsule2D Ragebrute::GetCapsule() const
{
    Capsule2D animBodyCapsule;
    if (GetAnimBodyCapsuleWorld(animBodyCapsule))
        return animBodyCapsule;

    float radius = kRagebruteFrameWidth * _scale * 0.23f;
    return Capsule2D{
        { _worldPos.x, _worldPos.y + radius * 0.25f },
        16.f,
        radius
    };
}

// =============================================================================
void Ragebrute::PlayAttackSound()
{
    // Faster, angrier grunts once raging.
    float pitch = _raging ? GetRandomValue(95, 120) / 100.f
                          : GetRandomValue(75, 95)  / 100.f;
    SetSoundPitch(_attackSound, pitch);
    SetSoundVolume(_attackSound, 0.65f);
    PlaySound(_attackSound);
}

void Ragebrute::PlayDeathSound()
{
    SfxBank::Get().Play(SfxId::DeathSmall, 0.75f, true);
}

// =============================================================================
void Ragebrute::EnsureSharedResourcesLoaded()
{
    if (_sharedResourcesLoaded)
        return;

    for (int variant = 0; variant < kVariantCount; variant++)
    {
        const char* suffix = kRagebruteVariantSuffixes[variant];
        _sharedIdleAnim[variant]       = LoadTexture(AssetPath(TextFormat("Enemy/RagebruteIdle%s.png",   suffix)).c_str());
        _sharedWalkAnim[variant]       = LoadTexture(AssetPath(TextFormat("Enemy/RagebruteWalk%s.png",   suffix)).c_str());
        _sharedAttackAnim[variant]     = LoadTexture(AssetPath(TextFormat("Enemy/RagebruteAttack%s.png", suffix)).c_str());
        _sharedTakeDamageAnim[variant] = LoadTexture(AssetPath(TextFormat("Enemy/RagebruteHurt%s.png",   suffix)).c_str());
        _sharedDeathAnim[variant]      = LoadTexture(AssetPath(TextFormat("Enemy/RagebruteDeath%s.png",  suffix)).c_str());
    }
    _sharedAttackSound = LoadSound(AssetPath("Sounds/GS1_Spell_Misc_1.ogg").c_str());
    _sharedHurtSound   = LoadSound(AssetPath("Sounds/SmallMonsterDamage.ogg").c_str());
    _sharedDeathSound  = LoadSound(AssetPath("Sounds/MonsterDeath.ogg").c_str());
    _sharedResourcesLoaded = true;
}

void Ragebrute::UnloadSharedResources()
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
