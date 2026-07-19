#include "Bonechill.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "AttackTuning.h"
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

    _signatureState    = SignatureState::None;
    _signatureTimer    = 0.f;
    _signatureCooldownDuration = EliteSignatureValueOr(
        AttackTuningStore::Get("Bonechill_Permafrost_Slam"),
        &AttackTuning::signatureCooldown, Balance::Elite::kBonechillSignatureCooldown);
    _signatureCooldown = _signatureCooldownDuration * 0.5f;   // first slam comes sooner
    _signatureDirection = Vector2{ 1.f, 0.f };
    _frostArmourBroken  = false;

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
// Elite signature: The Frozen Wall.
bool Bonechill::IsFrostArmourActive() const
{
    // Armour holds only while deliberately advancing in phase one. The slam's
    // windup and recovery drop it entirely — attacking the opening is always
    // fully rewarded — and ARMOUR SHATTERED removes it for good.
    return _isEliteMiniboss && !_frostArmourBroken && !_dying && IsAlive() &&
           _signatureState == SignatureState::None;
}

void Bonechill::TakeDamage(int damage, Vector2 attackerPos)
{
    // Frontal frost armour: reduce (ceil, min 1) then hand the result to the
    // COMMON damage path so Guard Links, revives, statuses and damage numbers
    // stay authoritative. Rear hits pass through untouched. Never IMMUNE.
    if (IsFrostArmourActive() && damage > 0)
    {
        Vector2 toAttacker = Vector2Subtract(attackerPos, _worldPos);
        if (Vector2Length(toAttacker) > 0.01f)
        {
            toAttacker = Vector2Normalize(toAttacker);
            const Vector2 facing{ (float)_rightLeft, 0.f };
            const float frontalDot = toAttacker.x * facing.x + toAttacker.y * facing.y;
            if (frontalDot >= 0.35f)
                damage = ApplyBonechillFrontReduction(damage);
        }
    }
    Enemy::TakeDamage(damage, attackerPos);
}

void Bonechill::SetSignatureSheet(const Texture2D& sheet)
{
    if (_texture.id == sheet.id)
        return;
    _texture    = sheet;
    _width      = kBonechillFrameWidth;
    _height     = (float)sheet.height;
    _updateTime = kBonechillFrameTime;
    _maxFrames  = kBonechillFrameCount;
    _frame      = 0;
    _runningTime = 0.f;
}

bool Bonechill::UpdateEliteSignature(float deltaTime, Vector2 /*navigationTarget*/,
    bool /*hasNavigationTarget*/, const std::vector<std::unique_ptr<Enemy>>& /*enemies*/,
    const std::vector<Vector2>& /*propCenters*/)
{
    if (!_isEliteMiniboss || _target == nullptr || _dying || !IsAlive())
        return false;

    // ARMOUR SHATTERED: one-time 50% escalation. The wall cracks — no more
    // frontal reduction, but it hunts faster and slams more often. Counterplay
    // shifts from flanking to dodge timing.
    if (ConsumePhaseChange() >= 1)
    {
        RequestBossCallout("ARMOUR SHATTERED");
        EmitEliteEvent({ EliteEventKind::PhaseChange, EliteArchetype::Bonechill,
                         EliteMove::BonechillPermafrostSlam, 0, _worldPos });
        _frostArmourBroken = true;
        const AttackTuning* signatureTuning = AttackTuningStore::Get("Bonechill_Permafrost_Slam");
        _speed *= EliteSignatureValueOr(signatureTuning, &AttackTuning::phaseSpeedMult,
                                        Balance::Elite::kBonechillPhaseSpeed);
        _signatureCooldownDuration *= EliteSignatureValueOr(signatureTuning,
                                        &AttackTuning::phaseCooldownMult,
                                        Balance::Elite::kBonechillPhaseCooldownMult);
        if (_signatureState != SignatureState::None)
        {
            _signatureState = SignatureState::None;
            _signatureCooldown = 1.0f;
        }
    }

    if (IsFrozen() || _takingDamage)
    {
        if (_signatureState != SignatureState::None)
        {
            _signatureState = SignatureState::None;
            _signatureCooldown = _signatureCooldownDuration * 0.6f;
        }
        return false;
    }

    switch (_signatureState)
    {
    case SignatureState::None:
    {
        _signatureCooldown -= deltaTime;
        if (_signatureCooldown > 0.f || _attacking)
            return false;

        const float distanceToPlayer = Vector2Distance(_target->GetFeetWorldPos(), _worldPos);
        if (distanceToPlayer > 520.f)
            return false;   // keep advancing; slam only at threatening range

        Vector2 toPlayer = Vector2Subtract(_target->GetFeetWorldPos(), _worldPos);
        _signatureDirection = (Vector2Length(toPlayer) > 0.01f)
            ? Vector2Normalize(toPlayer) : Vector2{ (float)_rightLeft, 0.f };
        if (_signatureDirection.x < -0.01f) _rightLeft = -1;
        if (_signatureDirection.x >  0.01f) _rightLeft =  1;

        _signatureState = SignatureState::SlamTelegraph;
        _signatureTimer = EliteSignatureValueOr(AttackTuningStore::Get("Bonechill_Permafrost_Slam"),
                                                &AttackTuning::telegraphTime,
                                                Balance::Elite::kBonechillSlamTelegraph);
        _eliteSignatureCasts++;
        EmitEliteEvent({ EliteEventKind::Telegraph, EliteArchetype::Bonechill,
                         EliteMove::BonechillPermafrostSlam, 0, _worldPos, {}, _signatureDirection });
        return true;
    }

    case SignatureState::SlamTelegraph:
        _velocity = Vector2Zero();
        SetSignatureSheet(_idleAnim);
        _signatureTimer -= deltaTime;
        if (_signatureTimer <= 0.f)
        {
            EmitEliteEvent({ EliteEventKind::Lock, EliteArchetype::Bonechill,
                             EliteMove::BonechillPermafrostSlam, 0, _worldPos, {}, _signatureDirection });
            // Direct slam cone in front, then three ice lanes with walkable
            // gaps between them. Zones resolve centrally in CombatDirector.
            EmitEliteEvent({ EliteEventKind::Execute, EliteArchetype::Bonechill,
                             EliteMove::BonechillPermafrostSlam, 0, _worldPos, {}, _signatureDirection });
            for (int laneIndex = 0; laneIndex < 3; ++laneIndex)
            {
                Vector2 laneDirection = EliteSpreadDirection(
                    _signatureDirection, laneIndex, 3, 56.f * DEG2RAD);
                EmitEliteEvent({ EliteEventKind::TrailPatch, EliteArchetype::Bonechill,
                                 EliteMove::BonechillPermafrostSlam, 0, _worldPos, {}, laneDirection });
            }
            PlayAttackSound();
            _signatureState = SignatureState::SlamRecovery;
            _signatureTimer = EliteSignatureValueOr(AttackTuningStore::Get("Bonechill_Permafrost_Slam"),
                                                    &AttackTuning::recoveryTime,
                                                    Balance::Elite::kBonechillSlamRecovery);
        }
        return true;

    case SignatureState::SlamRecovery:
        // Fully vulnerable (no frost armour) — the authored punish window.
        _velocity = Vector2Zero();
        SetSignatureSheet(_idleAnim);
        _signatureTimer -= deltaTime;
        if (_signatureTimer <= 0.f)
        {
            _signatureState = SignatureState::None;
            _signatureCooldown = _signatureCooldownDuration;
            EmitEliteEvent({ EliteEventKind::Recover, EliteArchetype::Bonechill,
                             EliteMove::BonechillPermafrostSlam, 0, _worldPos });
            SetIdleAnimation(true);
        }
        return true;
    }

    return false;
}

void Bonechill::DrawEliteTelegraph() const
{
    // Frost-armour indicator: a frontal ice arc while the armour holds, so the
    // player can SEE which side is reduced and flank behind it.
    if (IsFrostArmourActive())
    {
        const float arcRadius = kBonechillFrameWidth * _scale * 0.55f;
        const float facingAngle = (_rightLeft >= 0) ? 0.f : 180.f;
        DrawCircleSectorLines(_worldPos, arcRadius, facingAngle - 55.f, facingAngle + 55.f,
                              12, Fade(Color{ 150, 220, 255, 255 }, 0.55f));
        DrawCircleSectorLines(_worldPos, arcRadius - 6.f, facingAngle - 45.f, facingAngle + 45.f,
                              10, Fade(Color{ 200, 240, 255, 255 }, 0.35f));
    }

    if (_signatureState != SignatureState::SlamTelegraph)
        return;

    // Slam warning: the direct cone plus the three lane outlines — exactly the
    // geometry the Execute events will damage.
    Color warn = Fade(Color{ 120, 200, 255, 255 }, 0.22f);
    for (int laneIndex = 0; laneIndex < 3; ++laneIndex)
    {
        Vector2 laneDirection = EliteSpreadDirection(_signatureDirection, laneIndex, 3, 56.f * DEG2RAD);
        Vector2 laneEnd = Vector2Add(_worldPos,
            Vector2Scale(laneDirection, Balance::Elite::kBonechillLaneLength));
        Vector2 side{ -laneDirection.y, laneDirection.x };
        const float halfWidth = Balance::Elite::kBonechillLaneWidth;
        Vector2 cornerA = Vector2Add(_worldPos, Vector2Scale(side,  halfWidth));
        Vector2 cornerB = Vector2Add(_worldPos, Vector2Scale(side, -halfWidth));
        Vector2 cornerC = Vector2Add(laneEnd, Vector2Scale(side, -halfWidth));
        Vector2 cornerD = Vector2Add(laneEnd, Vector2Scale(side,  halfWidth));
        DrawTriangle(cornerA, cornerB, cornerC, warn);
        DrawTriangle(cornerA, cornerC, cornerD, warn);
        DrawLineEx(cornerB, cornerC, 2.f, Fade(Color{ 170, 225, 255, 255 }, 0.6f));
        DrawLineEx(cornerA, cornerD, 2.f, Fade(Color{ 170, 225, 255, 255 }, 0.6f));
    }
    // Direct-slam impact disc right in front.
    Vector2 slamCentre = Vector2Add(_worldPos, Vector2Scale(_signatureDirection, 150.f));
    DrawCircleLines((int)slamCentre.x, (int)slamCentre.y, 140.f, Fade(Color{ 170, 225, 255, 255 }, 0.6f));
}

void Bonechill::DebugForceEliteSignature()
{
    if (_dying || !IsAlive())
        return;
    _signatureCooldown = 0.f;
}

void Bonechill::DebugForceElitePhaseTwo()
{
    _health = std::min(_health, std::max(1.f, std::floor(_maxHealth * 0.5f)));
}

const char* Bonechill::GetEliteSignatureStateName() const
{
    switch (_signatureState)
    {
    case SignatureState::SlamTelegraph: return "SlamTelegraph";
    case SignatureState::SlamRecovery:  return "SlamRecovery";
    default: return _frostArmourBroken ? "Advance (shattered)" : "Advance (armoured)";
    }
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
