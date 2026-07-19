#include "Stormclub.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "AttackTuning.h"
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
    // The SHARED reset owns every pooled-lifetime field (statuses, pit fall,
    // revive, elite events, guard link, phase latch, telemetry, nav, flicker).
    Enemy::ResetForSpawn(pos);

    // ── Stormclub profile on top of the shared defaults ──────────────────────
    SetIdleAnimation(true);
    _scale = 4.8f;                 // elite-Ogre sized

    _health      = 12.f;
    _maxHealth   = 12.f;
    _attackPower = 3.f;            // the club hits hard AND displaces you
    _speed       = 160.f;
    _expValue    = 8;

    _attackRange     = 125.f;
    _attackDelay     = 1.6f;       // long recovery between smashes
    _attackCooldown  = 0.f;

    _signatureState    = SignatureState::None;
    _signatureTimer    = 0.f;
    _signatureCooldown = Balance::Elite::kStormclubSignatureCooldown * 0.5f;   // first leap comes sooner
    _leapTarget        = Vector2Zero();
    _activeLeapRange   = Balance::Elite::kStormclubLeapRange;
    _leapsRemaining    = 0;
    _landedOnPlayer    = false;

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
// Elite signature: The Thunder Breaker.
void Stormclub::SetSignatureSheet(const Texture2D& sheet)
{
    if (_texture.id == sheet.id)
        return;
    _texture    = sheet;
    _width      = kStormclubFrameWidth;
    _height     = (float)sheet.height;
    _updateTime = kStormclubFrameTime;
    _maxFrames  = kStormclubFrameCount;
    _frame      = 0;
    _runningTime = 0.f;
}

void Stormclub::BeginLeapTelegraph(float telegraphSeconds, float leapRange)
{
    _signatureState  = SignatureState::LeapTelegraph;
    _signatureTimer  = telegraphSeconds;
    _activeLeapRange = leapRange;
    _eliteSignatureCasts++;
    EmitEliteEvent({ EliteEventKind::Telegraph, EliteArchetype::Stormclub,
                     EliteMove::StormclubThunderLeap, 0, _worldPos,
                     _target ? _target->GetFeetWorldPos() : _worldPos });
}

bool Stormclub::UpdateEliteSignature(float deltaTime, Vector2 /*navigationTarget*/,
    bool /*hasNavigationTarget*/, const std::vector<std::unique_ptr<Enemy>>& /*enemies*/,
    const std::vector<Vector2>& propCenters)
{
    if (!_isEliteMiniboss || _target == nullptr || _dying || !IsAlive())
        return false;

    // TEMPEST: one-time 50% escalation — future signature cycles chain two
    // shorter leaps, each individually telegraphed and avoidable.
    if (ConsumePhaseChange() >= 1)
    {
        RequestBossCallout("TEMPEST");
        EmitEliteEvent({ EliteEventKind::PhaseChange, EliteArchetype::Stormclub,
                         EliteMove::StormclubThunderLeap, 0, _worldPos });
        if (_signatureState == SignatureState::LeapTelegraph)
        {
            _signatureState = SignatureState::None;
            _signatureCooldown = 1.0f;
        }
    }

    if (IsFrozen() || _takingDamage)
    {
        // A mid-air leap keeps travelling (it is already committed); windup and
        // recovery states break out normally.
        if (_signatureState == SignatureState::LeapTelegraph)
        {
            _signatureState = SignatureState::None;
            _signatureCooldown = Balance::Elite::kStormclubSignatureCooldown * 0.6f;
            _leapsRemaining = 0;
        }
        if (_signatureState != SignatureState::Leaping)
            return _signatureState != SignatureState::None;
    }

    switch (_signatureState)
    {
    case SignatureState::None:
    {
        _signatureCooldown -= deltaTime;
        if (_signatureCooldown > 0.f || _attacking)
            return false;

        const float distanceToPlayer = Vector2Distance(_target->GetFeetWorldPos(), _worldPos);
        if (distanceToPlayer < 220.f)
            return false;   // too close — the club swing already covers this range

        // Attack Library tuning (attacktuning_Stormclub_Thunder_Leap.txt):
        // telegraph and travel distance override the coded defaults; Tempest
        // leaps stay proportionally shorter.
        const AttackTuning* signatureTuning = AttackTuningStore::Get("Stormclub_Thunder_Leap");
        const float tunedTelegraph = EliteSignatureValueOr(signatureTuning,
            &AttackTuning::telegraphTime, Balance::Elite::kStormclubLeapTelegraph);
        const float tunedRange = EliteSignatureValueOr(signatureTuning,
            &AttackTuning::travelDistance, Balance::Elite::kStormclubLeapRange);
        const float tempestRangeRatio = Balance::Elite::kStormclubLeapRangeP2 /
                                        Balance::Elite::kStormclubLeapRange;
        _leapsRemaining = IsElitePhaseTwo() ? 2 : 1;
        BeginLeapTelegraph(tunedTelegraph,
                           IsElitePhaseTwo() ? tunedRange * tempestRangeRatio : tunedRange);
        return true;
    }

    case SignatureState::LeapTelegraph:
    {
        _velocity = Vector2Zero();
        SetSignatureSheet(_idleAnim);
        // The marker follows the player during the windup — the LOCK happens
        // when the timer ends, and the landing point never moves afterward.
        // The endpoint is clamped to navigable ground BEFORE lock, so the
        // marker the player reads is always the true landing point.
        Vector2 desired = _target->GetFeetWorldPos();
        Vector2 toDesired = Vector2Subtract(desired, _worldPos);
        const float desiredDistance = Vector2Length(toDesired);
        if (desiredDistance > _activeLeapRange && desiredDistance > 0.01f)
            desired = Vector2Add(_worldPos, Vector2Scale(Vector2Normalize(toDesired), _activeLeapRange));
        _leapTarget = ClampElitePathToNavigable(_worldPos, desired, propCenters);

        _signatureTimer -= deltaTime;
        if (_signatureTimer <= 0.f)
        {
            EmitEliteEvent({ EliteEventKind::Lock, EliteArchetype::Stormclub,
                             EliteMove::StormclubThunderLeap, 0, _worldPos, _leapTarget });
            _signatureState = SignatureState::Leaping;
            _landedOnPlayer = false;
            SetSignatureSheet(_attackAnim);
            if (_leapTarget.x < _worldPos.x - 1.f) _rightLeft = -1;
            if (_leapTarget.x > _worldPos.x + 1.f) _rightLeft =  1;
        }
        return true;
    }

    case SignatureState::Leaping:
    {
        // Continuous travel toward the locked point — never a teleport.
        Vector2 toTarget = Vector2Subtract(_leapTarget, _worldPos);
        const float remainingDistance = Vector2Length(toTarget);
        const float stepDistance = Balance::Elite::kStormclubLeapSpeed * deltaTime;
        if (remainingDistance <= stepDistance || remainingDistance < 0.01f)
        {
            _worldPos = _leapTarget;

            // LANDING: central impact plus three lightning branches with even
            // 120-degree gaps, one branch continuing the travel direction.
            Vector2 travelDirection = Vector2Subtract(_leapTarget, _worldPosLastFrame);
            travelDirection = (Vector2Length(travelDirection) > 0.01f)
                ? Vector2Normalize(travelDirection) : Vector2{ (float)_rightLeft, 0.f };
            EmitEliteEvent({ EliteEventKind::Execute, EliteArchetype::Stormclub,
                             EliteMove::StormclubThunderLeap, 0, _worldPos, _leapTarget,
                             travelDirection });
            for (int branchIndex = 0; branchIndex < 3; ++branchIndex)
            {
                Vector2 branchDirection = EliteSpreadDirection(
                    travelDirection, branchIndex, 3, 240.f * DEG2RAD);
                EmitEliteEvent({ EliteEventKind::TrailPatch, EliteArchetype::Stormclub,
                                 EliteMove::StormclubThunderLeap, 0, _worldPos, {},
                                 branchDirection });
            }
            PlayAttackSound();

            _landedOnPlayer = Vector2Distance(_target->GetFeetWorldPos(), _worldPos)
                              <= Balance::Elite::kStormclubImpactRadius;
            if (_landedOnPlayer)
                _eliteSignatureHits++;

            _leapsRemaining = std::max(0, _leapsRemaining - 1);
            if (_leapsRemaining > 0)
            {
                // TEMPEST second leap: the next target is shown and locked only
                // AFTER this landing — never two overlapping destinations.
                BeginLeapTelegraph(0.5f, Balance::Elite::kStormclubLeapRangeP2);
            }
            else
            {
                // Miss = the club stays embedded: the long punish window. A
                // tuned recovery scales the miss window by the same ratio.
                const float tunedHitRecovery = EliteSignatureValueOr(
                    AttackTuningStore::Get("Stormclub_Thunder_Leap"),
                    &AttackTuning::recoveryTime, Balance::Elite::kStormclubHitRecovery);
                const float missRecoveryRatio = Balance::Elite::kStormclubMissRecovery /
                                                Balance::Elite::kStormclubHitRecovery;
                _signatureState = SignatureState::Recovery;
                _signatureTimer = _landedOnPlayer ? tunedHitRecovery
                                                  : tunedHitRecovery * missRecoveryRatio;
                EmitEliteEvent({ EliteEventKind::Recover, EliteArchetype::Stormclub,
                                 EliteMove::StormclubThunderLeap, 0, _worldPos });
                SetSignatureSheet(_idleAnim);
            }
        }
        else
        {
            _worldPos = Vector2Add(_worldPos, Vector2Scale(Vector2Normalize(toTarget), stepDistance));
        }
        return true;
    }

    case SignatureState::Recovery:
        _velocity = Vector2Zero();
        SetSignatureSheet(_idleAnim);
        _signatureTimer -= deltaTime;
        if (_signatureTimer <= 0.f)
        {
            _signatureState = SignatureState::None;
            _signatureCooldown = EliteSignatureValueOr(AttackTuningStore::Get("Stormclub_Thunder_Leap"),
                                                       &AttackTuning::signatureCooldown,
                                                       Balance::Elite::kStormclubSignatureCooldown);
            SetIdleAnimation(true);
        }
        return true;
    }

    return false;
}

void Stormclub::DrawEliteTelegraph() const
{
    if (_signatureState != SignatureState::LeapTelegraph)
        return;

    // Landing marker: a pulsing ring where the leap will come down. During the
    // windup it tracks the player; the lock freezes it.
    const float pulse = 0.75f + 0.25f * sinf((float)GetTime() * 9.f);
    DrawCircleLines((int)_leapTarget.x, (int)_leapTarget.y,
                    Balance::Elite::kStormclubImpactRadius * pulse,
                    Fade(Color{ 255, 220, 90, 255 }, 0.75f));
    DrawCircleLines((int)_leapTarget.x, (int)_leapTarget.y,
                    Balance::Elite::kStormclubImpactRadius,
                    Fade(Color{ 255, 240, 150, 255 }, 0.45f));
    DrawCircleV(_leapTarget, 10.f, Fade(Color{ 255, 220, 90, 255 }, 0.55f));
}

void Stormclub::DebugForceEliteSignature()
{
    if (_dying || !IsAlive())
        return;
    _signatureCooldown = 0.f;
}

void Stormclub::DebugForceElitePhaseTwo()
{
    _health = std::min(_health, std::max(1.f, std::floor(_maxHealth * 0.5f)));
}

const char* Stormclub::GetEliteSignatureStateName() const
{
    switch (_signatureState)
    {
    case SignatureState::LeapTelegraph: return "LeapTelegraph";
    case SignatureState::Leaping:       return "Leaping";
    case SignatureState::Recovery:      return "ClubEmbedded";
    default:                            return "Pursuit";
    }
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
