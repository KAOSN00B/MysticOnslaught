#include "Venomfang.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "AttackTuning.h"
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

    _signatureState        = SignatureState::None;
    _signatureTimer        = 0.f;
    _signatureCooldown     = Balance::Elite::kVenomfangSignatureCooldown * 0.5f;   // first pounce comes sooner
    _pounceTarget          = Vector2Zero();
    _retreatDirection      = Vector2{ 1.f, 0.f };
    _biteLanded            = false;
    _pouncesRemaining      = 0;
    _predatorMarks         = 0;
    _predatorMarkTimer     = 0.f;
    _trailPatchAccumulator = 0.f;

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
// Venomous bite: applies the REAL stacking poison status (green tint, capped
// stacks, stable ticks) instead of routing through the burn presentation.
// Behavior identity: hit-and-run — after every landed bite it darts away from
// the player (rides the hit-knockback channel on ITSELF, which also suppresses
// its pursuit briefly), so it never stands still trading blows.
void Venomfang::OnMeleeHitPlayer(Character* target)
{
    if (target == nullptr)
        return;
    target->ApplyPoison(Balance::Elite::kPoisonDuration,
                        Balance::Elite::kPoisonTickInterval,
                        Balance::Elite::kPoisonDamagePerTick);

    Vector2 away = Vector2Subtract(_worldPos, target->GetWorldPos());
    float len = Vector2Length(away);
    if (len < 0.001f) away = Vector2{ -GetFacingSign(), 0.f };
    else              away = Vector2Scale(away, 1.f / len);
    _hitKnockbackVel   = Vector2Scale(away, kDisengageSpeed);
    _hitKnockbackTimer = kDisengageDuration;
}

// =============================================================================
// Elite signature: The Ambush Predator.
void Venomfang::SetSignatureSheet(const Texture2D& sheet)
{
    if (_texture.id == sheet.id)
        return;
    _texture    = sheet;
    _width      = kVenomfangFrameWidth;
    _height     = (float)sheet.height;
    _updateTime = kVenomfangFrameTime;
    _maxFrames  = kVenomfangFrameCount;
    _frame      = 0;
    _runningTime = 0.f;
}

void Venomfang::BeginPounceTelegraph(float telegraphSeconds)
{
    _signatureState = SignatureState::PounceTelegraph;
    _signatureTimer = telegraphSeconds;
    _eliteSignatureCasts++;
    EmitEliteEvent({ EliteEventKind::Telegraph, EliteArchetype::Venomfang,
                     EliteMove::VenomfangPounce, 0, _worldPos,
                     _target ? _target->GetFeetWorldPos() : _worldPos });
}

bool Venomfang::UpdateEliteSignature(float deltaTime, Vector2 /*navigationTarget*/,
    bool /*hasNavigationTarget*/, const std::vector<std::unique_ptr<Enemy>>& /*enemies*/,
    const std::vector<Vector2>& /*propCenters*/)
{
    if (!_isEliteMiniboss || _target == nullptr || _dying || !IsAlive())
        return false;

    // Predator's Mark expires after several seconds without another bite.
    if (_predatorMarkTimer > 0.f)
    {
        _predatorMarkTimer -= deltaTime;
        if (_predatorMarkTimer <= 0.f)
            _predatorMarks = 0;
    }

    // BLOOD SCENT: one-time 50% escalation — faster circling, and future
    // signature cycles permit a second, independently telegraphed pounce.
    if (ConsumePhaseChange() >= 1)
    {
        RequestBossCallout("BLOOD SCENT");
        EmitEliteEvent({ EliteEventKind::PhaseChange, EliteArchetype::Venomfang,
                         EliteMove::VenomfangPounce, 0, _worldPos });
        _speed *= Balance::Elite::kVenomfangPhaseCircleSpeed;
        if (_signatureState == SignatureState::PounceTelegraph)
        {
            _signatureState = SignatureState::None;
            _signatureCooldown = 1.0f;
        }
    }

    // Interrupting the windup cancels the pounce — that IS the counterplay.
    // A miss or interruption never builds Predator's Mark.
    if (IsFrozen() || _takingDamage)
    {
        if (_signatureState == SignatureState::PounceTelegraph)
        {
            _signatureState = SignatureState::None;
            _signatureCooldown = Balance::Elite::kVenomfangSignatureCooldown * 0.6f;
            _pouncesRemaining = 0;
        }
        if (_signatureState != SignatureState::Pouncing)
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
        if (distanceToPlayer < 200.f || distanceToPlayer > Balance::Elite::kVenomfangPounceRange + 160.f)
            return false;   // keep circling until an off-angle window opens

        _pouncesRemaining = IsElitePhaseTwo() ? 2 : 1;
        // Attack Library tuning (attacktuning_Venomfang_Venom_Pounce.txt).
        BeginPounceTelegraph(EliteSignatureValueOr(AttackTuningStore::Get("Venomfang_Venom_Pounce"),
                                                   &AttackTuning::telegraphTime,
                                                   Balance::Elite::kVenomfangPounceTelegraph));
        return true;
    }

    case SignatureState::PounceTelegraph:
    {
        _velocity = Vector2Zero();
        SetSignatureSheet(_idleAnim);
        // The narrow path tracks the player during the windup; the LOCK happens
        // when the timer ends and the endpoint never moves afterward.
        Vector2 desired = _target->GetFeetWorldPos();
        Vector2 toDesired = Vector2Subtract(desired, _worldPos);
        const float desiredDistance = Vector2Length(toDesired);
        const float pounceRange = EliteSignatureValueOr(AttackTuningStore::Get("Venomfang_Venom_Pounce"),
                                                        &AttackTuning::travelDistance,
                                                        Balance::Elite::kVenomfangPounceRange);
        _pounceTarget = (desiredDistance > pounceRange && desiredDistance > 0.01f)
            ? Vector2Add(_worldPos, Vector2Scale(Vector2Normalize(toDesired), pounceRange))
            : desired;
        if (_pounceTarget.x < _worldPos.x - 1.f) _rightLeft = -1;
        if (_pounceTarget.x > _worldPos.x + 1.f) _rightLeft =  1;

        _signatureTimer -= deltaTime;
        if (_signatureTimer <= 0.f)
        {
            EmitEliteEvent({ EliteEventKind::Lock, EliteArchetype::Venomfang,
                             EliteMove::VenomfangPounce, 0, _worldPos, _pounceTarget });
            EmitEliteEvent({ EliteEventKind::Execute, EliteArchetype::Venomfang,
                             EliteMove::VenomfangPounce, 0, _worldPos, _pounceTarget });
            _signatureState = SignatureState::Pouncing;
            _biteLanded = false;
            SetSignatureSheet(_attackAnim);
            PlayAttackSound();
        }
        return true;
    }

    case SignatureState::Pouncing:
    {
        // Dart along the locked path; a body-contact bite ends it early.
        Vector2 toTarget = Vector2Subtract(_pounceTarget, _worldPos);
        const float remainingDistance = Vector2Length(toTarget);
        const float stepDistance = Balance::Elite::kVenomfangPounceSpeed * deltaTime;

        if (!_biteLanded && _target->IsAlive() &&
            CheckCollisionRecs(GetCollisionRec(), _target->GetCollisionRec()))
        {
            // BITE: base damage scaled by Predator's Mark (consecutive bites).
            const float markMultiplier = 1.f + 0.25f * (float)_predatorMarks;
            _target->TakeDamage((int)std::round(_attackPower * markMultiplier), _worldPos);
            OnMeleeHitPlayer(_target);   // real poison + self-disengage impulse
            _biteLanded = true;
            _eliteSignatureHits++;
            _predatorMarks = std::min(Balance::Elite::kPredatorMarkCap, _predatorMarks + 1);
            _predatorMarkTimer = Balance::Elite::kPredatorMarkDuration;
        }

        if (_biteLanded || remainingDistance <= stepDistance || remainingDistance < 0.01f)
        {
            if (!_biteLanded)
                _worldPos = _pounceTarget;
            // Disengage away from the player, painting a short poison trail.
            Vector2 away = Vector2Subtract(_worldPos, _target->GetFeetWorldPos());
            _retreatDirection = (Vector2Length(away) > 0.01f)
                ? Vector2Normalize(away) : Vector2{ -(float)_rightLeft, 0.f };
            _signatureState = SignatureState::Retreating;
            _signatureTimer = 0.45f;
            _trailPatchAccumulator = 70.f;   // drop the first patch immediately
        }
        else
        {
            _worldPos = Vector2Add(_worldPos, Vector2Scale(Vector2Normalize(toTarget), stepDistance));
        }
        return true;
    }

    case SignatureState::Retreating:
    {
        SetSignatureSheet(_walkAnim);
        const float retreatSpeed = _speed * 1.8f;
        _worldPos = Vector2Add(_worldPos, Vector2Scale(_retreatDirection, retreatSpeed * deltaTime));

        // Short animated poison trail (resolved as zones by CombatDirector).
        _trailPatchAccumulator += retreatSpeed * deltaTime;
        if (_trailPatchAccumulator >= 70.f)
        {
            _trailPatchAccumulator = 0.f;
            EmitEliteEvent({ EliteEventKind::TrailPatch, EliteArchetype::Venomfang,
                             EliteMove::VenomfangPounce, 0, _worldPos });
        }

        _signatureTimer -= deltaTime;
        if (_signatureTimer <= 0.f)
        {
            _pouncesRemaining = std::max(0, _pouncesRemaining - 1);
            if (_biteLanded && _pouncesRemaining > 0)
            {
                // BLOOD SCENT second pounce: an independently visible delay,
                // then a fresh, fully telegraphed path.
                BeginPounceTelegraph(Balance::Elite::kVenomfangSecondPounceDelay);
            }
            else
            {
                _signatureState = SignatureState::Recovery;
                _signatureTimer = EliteSignatureValueOr(AttackTuningStore::Get("Venomfang_Venom_Pounce"),
                                                        &AttackTuning::recoveryTime,
                                                        Balance::Elite::kVenomfangPounceRecovery);
                EmitEliteEvent({ EliteEventKind::Recover, EliteArchetype::Venomfang,
                                 EliteMove::VenomfangPounce, 0, _worldPos });
                SetIdleAnimation(true);
            }
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
            _signatureCooldown = EliteSignatureValueOr(AttackTuningStore::Get("Venomfang_Venom_Pounce"),
                                                       &AttackTuning::signatureCooldown,
                                                       Balance::Elite::kVenomfangSignatureCooldown);
            SetIdleAnimation(true);
        }
        return true;
    }

    return false;
}

void Venomfang::DrawEliteTelegraph() const
{
    if (_signatureState != SignatureState::PounceTelegraph)
        return;

    // Narrow pounce-path warning from the predator to the (tracking) endpoint.
    Vector2 pathDirection = Vector2Subtract(_pounceTarget, _worldPos);
    if (Vector2Length(pathDirection) < 0.01f)
        return;
    pathDirection = Vector2Normalize(pathDirection);
    Vector2 side{ -pathDirection.y, pathDirection.x };
    const float halfWidth = 34.f;
    Vector2 cornerA = Vector2Add(_worldPos, Vector2Scale(side,  halfWidth));
    Vector2 cornerB = Vector2Add(_worldPos, Vector2Scale(side, -halfWidth));
    Vector2 cornerC = Vector2Add(_pounceTarget, Vector2Scale(side, -halfWidth));
    Vector2 cornerD = Vector2Add(_pounceTarget, Vector2Scale(side,  halfWidth));
    Color warn = Fade(Color{ 140, 235, 90, 255 }, 0.20f);
    DrawTriangle(cornerA, cornerB, cornerC, warn);
    DrawTriangle(cornerA, cornerC, cornerD, warn);
    DrawLineEx(cornerB, cornerC, 2.f, Fade(Color{ 170, 250, 120, 255 }, 0.6f));
    DrawLineEx(cornerA, cornerD, 2.f, Fade(Color{ 170, 250, 120, 255 }, 0.6f));
    DrawCircleLines((int)_pounceTarget.x, (int)_pounceTarget.y, 46.f,
                    Fade(Color{ 170, 250, 120, 255 }, 0.6f));
}

void Venomfang::DebugForceEliteSignature()
{
    if (_dying || !IsAlive())
        return;
    _signatureCooldown = 0.f;
}

void Venomfang::DebugForceElitePhaseTwo()
{
    _health = std::min(_health, std::max(1.f, std::floor(_maxHealth * 0.5f)));
}

const char* Venomfang::GetEliteSignatureStateName() const
{
    switch (_signatureState)
    {
    case SignatureState::PounceTelegraph: return "PounceTelegraph";
    case SignatureState::Pouncing:        return "Pouncing";
    case SignatureState::Retreating:      return "Retreating";
    case SignatureState::Recovery:        return "Recovery";
    default:                              return "Circling";
    }
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
