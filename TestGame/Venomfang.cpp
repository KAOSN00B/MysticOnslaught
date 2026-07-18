#include "Venomfang.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "Character.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

// ---- Static member definitions ----------------------------------------------
Texture2D Venomfang::_sharedIdleAnim[Venomfang::kVariantCount]{};
Texture2D Venomfang::_sharedWalkAnim[Venomfang::kVariantCount]{};
Texture2D Venomfang::_sharedAttackAnim[Venomfang::kVariantCount]{};
Texture2D Venomfang::_sharedTakeDamageAnim[Venomfang::kVariantCount]{};
Texture2D Venomfang::_sharedDeathAnim[Venomfang::kVariantCount]{};
Sound     Venomfang::_sharedAttackSound{};
Sound     Venomfang::_sharedHurtSound{};
Sound     Venomfang::_sharedDeathSound{};
bool      Venomfang::_sharedResourcesLoaded = false;

namespace
{
    const char* kVenomfangVariantSuffixes[4] = { "", "_B", "_C", "_D" };

    // All Venomfang sheets are stitched 6-frame strips of 32x32 pixels.
    constexpr int   kVenomfangFrameCount = 6;
    constexpr float kVenomfangFrameWidth = 32.f;
    constexpr float kVenomfangFrameTime  = 1.f / 12.f;   // quick, darting motion
}

// =============================================================================
Venomfang::Venomfang(Vector2 pos)
    : Enemy(pos)
{
}

Venomfang::~Venomfang() {}

// =============================================================================
void Venomfang::Init()
{
    EnsureSharedResourcesLoaded();

    _attackSound = _sharedAttackSound;
    _hurtSound   = _sharedHurtSound;
    _deathSound  = _sharedDeathSound;
    SetVariantTier(_variantTier);

    ResetForSpawn(_worldPos);
}

void Venomfang::SetVariantTier(int tier)
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
void Venomfang::ResetForSpawn(Vector2 pos)
{
    _worldPos          = pos;
    _worldPosLastFrame = pos;
    _homePos           = pos;
    _velocity          = Vector2Zero();
    _isActive          = true;

    SetIdleAnimation(false);
    _scale = 7.0f;                 // 32px art with a slim body — this high scale
                                   // brings it up to the Ogre's on-screen mass

    _health      = 8.f;            // fragile for a bruiser; speed is its armour
    _maxHealth   = 8.f;
    _attackPower = 2.f;
    _speed       = 260.f;          // fastest of the elite pool
    _expValue    = 7;

    _attackRange     = 100.f;
    _attackDelay     = 0.9f;       // quick bites
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

void Venomfang::SetIdleAnimation(bool resetFrame)
{
    _texture    = _idleAnim;
    _width      = kVenomfangFrameWidth;
    _height     = (float)_idleAnim.height;
    _updateTime = (_editorAnimFrameTimes[0] > 0.f) ? _editorAnimFrameTimes[0] : kVenomfangFrameTime;
    _maxFrames  = kVenomfangFrameCount;
    if (resetFrame) { _frame = 0; _runningTime = 0.f; }
}

// =============================================================================
void Venomfang::SetWaveScale(int /*wave*/)
{
    _health      = 8.f;
    _maxHealth   = 8.f;
    _attackPower = 2.f;
    _speed       = 260.f;
    _expValue    = 7;
}

// =============================================================================
// Venomous bite: a long, weak damage-over-time that outlasts the fight beat.
// Behavior identity: hit-and-run — after every landed bite it darts away from
// the player (rides the hit-knockback channel on ITSELF, which also suppresses
// its pursuit briefly), so it never stands still trading blows.
void Venomfang::OnMeleeHitPlayer(Character* target)
{
    if (target == nullptr)
        return;
    target->ApplyBurnTicks(kVenomTickDelay, kVenomTickCount, kVenomDamagePerTick, _worldPos);

    Vector2 away = Vector2Subtract(_worldPos, target->GetWorldPos());
    float len = Vector2Length(away);
    if (len < 0.001f) away = Vector2{ -GetFacingSign(), 0.f };
    else              away = Vector2Scale(away, 1.f / len);
    _hitKnockbackVel   = Vector2Scale(away, kDisengageSpeed);
    _hitKnockbackTimer = kDisengageDuration;
}

// =============================================================================
Rectangle Venomfang::GetCollisionRec() const
{
    Rectangle animBodyRect;
    if (GetAnimBodyRectWorld(animBodyRect))
        return animBodyRect;
    if (_hasTunedCollision)
        return GetTunedCollisionRec();

    // Live from the current draw scale so the body grows with the sprite.
    float halfW = kVenomfangFrameWidth * _scale * 0.5f;
    float halfH = (_idleAnim.id > 0 ? (float)_idleAnim.height : _height) * _scale * 0.5f;
    float boxW  = halfW * 1.10f;
    float boxH  = halfH * 1.00f;   // low, slinking body
    return Rectangle{
        _worldPos.x - boxW * 0.5f,
        _worldPos.y - boxH * 0.5f + halfH * 0.18f,
        boxW, boxH
    };
}

Capsule2D Venomfang::GetCapsule() const
{
    Capsule2D animBodyCapsule;
    if (GetAnimBodyCapsuleWorld(animBodyCapsule))
        return animBodyCapsule;

    float radius = kVenomfangFrameWidth * _scale * 0.24f;
    return Capsule2D{
        { _worldPos.x, _worldPos.y + radius * 0.20f },
        10.f,
        radius
    };
}

// =============================================================================
void Venomfang::PlayAttackSound()
{
    float pitch = GetRandomValue(110, 140) / 100.f;   // quick venomous snap
    SetSoundPitch(_attackSound, pitch);
    SetSoundVolume(_attackSound, 0.55f);
    PlaySound(_attackSound);
}

void Venomfang::PlayDeathSound()
{
    SfxBank::Get().Play(SfxId::DeathSmall, 0.65f, true);
}

// =============================================================================
void Venomfang::EnsureSharedResourcesLoaded()
{
    if (_sharedResourcesLoaded)
        return;

    for (int variant = 0; variant < kVariantCount; variant++)
    {
        const char* suffix = kVenomfangVariantSuffixes[variant];
        _sharedIdleAnim[variant]       = LoadTexture(AssetPath(TextFormat("Enemy/VenomfangIdle%s.png",   suffix)).c_str());
        _sharedWalkAnim[variant]       = LoadTexture(AssetPath(TextFormat("Enemy/VenomfangWalk%s.png",   suffix)).c_str());
        _sharedAttackAnim[variant]     = LoadTexture(AssetPath(TextFormat("Enemy/VenomfangAttack%s.png", suffix)).c_str());
        _sharedTakeDamageAnim[variant] = LoadTexture(AssetPath(TextFormat("Enemy/VenomfangHurt%s.png",   suffix)).c_str());
        _sharedDeathAnim[variant]      = LoadTexture(AssetPath(TextFormat("Enemy/VenomfangDeath%s.png",  suffix)).c_str());
    }
    _sharedAttackSound = LoadSound(AssetPath("Sounds/Impact_Poison.wav").c_str());
    _sharedHurtSound   = LoadSound(AssetPath("Sounds/SmallMonsterDamage.ogg").c_str());
    _sharedDeathSound  = LoadSound(AssetPath("Sounds/MonsterDeath.ogg").c_str());
    _sharedResourcesLoaded = true;
}

void Venomfang::UnloadSharedResources()
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
