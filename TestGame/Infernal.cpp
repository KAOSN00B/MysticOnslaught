#include "Infernal.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "AttackTuning.h"
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
    // The SHARED reset owns every pooled-lifetime field (statuses, pit fall,
    // revive, elite events, guard link, phase latch, telemetry, nav, flicker).
    // A reused instance can never carry a previous life's data.
    Enemy::ResetForSpawn(pos);

    // ── Infernal profile on top of the shared defaults ───────────────────────
    SetIdleAnimation(true);
    _scale = 5.6f;                 // its body art is narrow, so a higher scale
                                   // is needed to match the Ogre's on-screen mass

    _health      = 12.f;
    _maxHealth   = 12.f;
    _attackPower = 3.f;
    _speed       = 150.f;          // heavy, but no longer a crawl
    _expValue    = 8;

    _attackRange     = 120.f;
    _attackDelay     = 1.4f;       // ponderous swing cadence
    _attackCooldown  = 0.f;

    _signatureState        = SignatureState::None;
    _signatureTimer        = 0.f;
    _signatureCooldown     = Balance::Elite::kInfernalSignatureCooldown * 0.5f;   // first one comes sooner
    _signatureDirection    = Vector2{ 1.f, 0.f };
    _marchPatchAccumulator = 0.f;
    _marchPatchesDropped   = 0;
    _secondWavePending     = false;
    _secondWaveTimer       = 0.f;

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
// Elite signature: The Living Furnace.
void Infernal::SetSignatureSheet(const Texture2D& sheet)
{
    if (_texture.id == sheet.id)
        return;
    _texture    = sheet;
    _width      = kInfFrameWidth;
    _height     = (float)sheet.height;
    _updateTime = kInfFrameTime;
    _maxFrames  = kInfFrameCount;
    _frame      = 0;
    _runningTime = 0.f;
}

void Infernal::EmitFurnaceFissures(float spreadRadians, int fissureCount)
{
    for (int fissureIndex = 0; fissureIndex < fissureCount; ++fissureIndex)
    {
        Vector2 fissureDirection = EliteSpreadDirection(
            _signatureDirection, fissureIndex, fissureCount, spreadRadians);
        EmitEliteEvent({ EliteEventKind::Execute, EliteArchetype::Infernal,
                         EliteMove::InfernalFurnaceBurst, 0, _worldPos, {},
                         fissureDirection });
    }
}

bool Infernal::UpdateEliteSignature(float deltaTime, Vector2 /*navigationTarget*/,
    bool /*hasNavigationTarget*/, const std::vector<std::unique_ptr<Enemy>>& /*enemies*/,
    const std::vector<Vector2>& /*propCenters*/)
{
    if (!_isEliteMiniboss || _target == nullptr || _dying || !IsAlive())
        return false;

    // OVERHEATED: one-time 50% escalation. Cancel the current signature safely
    // and answer with an immediate Furnace Burst; melee also quickens slightly
    // for the rest of the fight.
    if (ConsumePhaseChange() >= 1)
    {
        RequestBossCallout("OVERHEATED");
        EmitEliteEvent({ EliteEventKind::PhaseChange, EliteArchetype::Infernal,
                         EliteMove::InfernalFurnaceBurst, 0, _worldPos });
        _attackDelay *= Balance::Elite::kInfernalPhaseMeleeMult;
        _signatureState = SignatureState::BurstTelegraph;
        _signatureTimer = Balance::Elite::kInfernalBurstTelegraph;
        Vector2 toPlayer = Vector2Subtract(_target->GetFeetWorldPos(), _worldPos);
        _signatureDirection = (Vector2Length(toPlayer) > 0.01f)
            ? Vector2Normalize(toPlayer) : Vector2{ (float)_rightLeft, 0.f };
        _eliteSignatureCasts++;
        EmitEliteEvent({ EliteEventKind::Telegraph, EliteArchetype::Infernal,
                         EliteMove::InfernalFurnaceBurst, 0, _worldPos, {}, _signatureDirection });
    }

    // Freeze / a hurt flinch interrupts the signature into a plain reset —
    // interrupting the windup IS the counterplay.
    if (IsFrozen() || _takingDamage)
    {
        if (_signatureState != SignatureState::None)
        {
            _signatureState = SignatureState::None;
            _signatureCooldown = Balance::Elite::kInfernalSignatureCooldown * 0.6f;
            _secondWavePending = false;
        }
        return false;
    }

    // OVERHEATED staggered wave: two extra fissures land shortly after the
    // main three, aimed between the original gaps.
    if (_secondWavePending)
    {
        _secondWaveTimer -= deltaTime;
        if (_secondWaveTimer <= 0.f)
        {
            _secondWavePending = false;
            EmitFurnaceFissures(25.f * DEG2RAD, 2);
        }
    }

    switch (_signatureState)
    {
    case SignatureState::None:
    {
        _signatureCooldown -= deltaTime;
        if (_signatureCooldown > 0.f || _attacking)
            return false;

        const float distanceToPlayer = Vector2Distance(_target->GetFeetWorldPos(), _worldPos);
        Vector2 toPlayer = Vector2Subtract(_target->GetFeetWorldPos(), _worldPos);
        _signatureDirection = (Vector2Length(toPlayer) > 0.01f)
            ? Vector2Normalize(toPlayer) : Vector2{ (float)_rightLeft, 0.f };
        if (_signatureDirection.x < -0.01f) _rightLeft = -1;
        if (_signatureDirection.x >  0.01f) _rightLeft =  1;

        // Far away: march the room and paint the trail. Mid-range: burst.
        // Attack Library tuning (attacktuning_Infernal_*.txt) overrides the
        // coded telegraph lengths per move when authored.
        const bool marchChosen = distanceToPlayer > 430.f;
        _signatureState = marchChosen ? SignatureState::MarchTelegraph
                                      : SignatureState::BurstTelegraph;
        _signatureTimer = marchChosen
            ? EliteSignatureValueOr(AttackTuningStore::Get("Infernal_Cinder_March"),
                                    &AttackTuning::telegraphTime, Balance::Elite::kInfernalMarchTelegraph)
            : EliteSignatureValueOr(AttackTuningStore::Get("Infernal_Furnace_Burst"),
                                    &AttackTuning::telegraphTime, Balance::Elite::kInfernalBurstTelegraph);
        _eliteSignatureCasts++;
        EmitEliteEvent({ EliteEventKind::Telegraph, EliteArchetype::Infernal,
                         marchChosen ? EliteMove::InfernalCinderMarch
                                     : EliteMove::InfernalFurnaceBurst,
                         0, _worldPos, {}, _signatureDirection });
        return true;
    }

    case SignatureState::MarchTelegraph:
        _velocity = Vector2Zero();
        SetSignatureSheet(_idleAnim);
        _signatureTimer -= deltaTime;
        if (_signatureTimer <= 0.f)
        {
            EmitEliteEvent({ EliteEventKind::Lock, EliteArchetype::Infernal,
                             EliteMove::InfernalCinderMarch, 0, _worldPos, {}, _signatureDirection });
            _signatureState = SignatureState::Marching;
            _signatureTimer = EliteSignatureValueOr(AttackTuningStore::Get("Infernal_Cinder_March"),
                                                    &AttackTuning::activeTime,
                                                    Balance::Elite::kInfernalMarchActive);
            _marchPatchAccumulator = Balance::Elite::kInfernalPatchSpacing;   // first patch immediately
            _marchPatchesDropped = 0;
            SetSignatureSheet(_walkAnim);
        }
        return true;

    case SignatureState::Marching:
    {
        // Committed line walk: faster than pursuit, direction never re-aims.
        SetSignatureSheet(_walkAnim);
        const float marchSpeed = _speed * 1.6f;
        _worldPos = Vector2Add(_worldPos, Vector2Scale(_signatureDirection, marchSpeed * deltaTime));

        // Spaced, short-lived flame patches — crossable, and capped so the room
        // never becomes permanently divided.
        _marchPatchAccumulator += marchSpeed * deltaTime;
        if (_marchPatchAccumulator >= Balance::Elite::kInfernalPatchSpacing &&
            _marchPatchesDropped < Balance::Elite::kInfernalPatchCap)
        {
            _marchPatchAccumulator = 0.f;
            _marchPatchesDropped++;
            EmitEliteEvent({ EliteEventKind::TrailPatch, EliteArchetype::Infernal,
                             EliteMove::InfernalCinderMarch, 0, _worldPos });
        }

        _signatureTimer -= deltaTime;
        if (_signatureTimer <= 0.f)
        {
            _signatureState = SignatureState::None;
            _signatureCooldown = EliteSignatureValueOr(AttackTuningStore::Get("Infernal_Cinder_March"),
                                                       &AttackTuning::signatureCooldown,
                                                       Balance::Elite::kInfernalSignatureCooldown);
            EmitEliteEvent({ EliteEventKind::Recover, EliteArchetype::Infernal,
                             EliteMove::InfernalCinderMarch, 0, _worldPos });
            SetIdleAnimation(true);
        }
        return true;
    }

    case SignatureState::BurstTelegraph:
        _velocity = Vector2Zero();
        SetSignatureSheet(_idleAnim);
        _signatureTimer -= deltaTime;
        if (_signatureTimer <= 0.f)
        {
            EmitEliteEvent({ EliteEventKind::Lock, EliteArchetype::Infernal,
                             EliteMove::InfernalFurnaceBurst, 0, _worldPos, {}, _signatureDirection });
            EmitFurnaceFissures(50.f * DEG2RAD, 3);
            if (IsElitePhaseTwo())
            {
                _secondWavePending = true;
                _secondWaveTimer = 0.4f;
            }
            _signatureState = SignatureState::BurstRecovery;
            // Tuned recovery scales the OVERHEATED window by the same ratio the
            // coded defaults use, so phase two always recovers longer.
            const float tunedRecovery = EliteSignatureValueOr(
                AttackTuningStore::Get("Infernal_Furnace_Burst"),
                &AttackTuning::recoveryTime, Balance::Elite::kInfernalBurstRecovery);
            _signatureTimer = IsElitePhaseTwo()
                ? tunedRecovery * (Balance::Elite::kInfernalBurstRecoveryP2 /
                                   Balance::Elite::kInfernalBurstRecovery)
                : tunedRecovery;
        }
        return true;

    case SignatureState::BurstRecovery:
        // Exhausted: stands still and takes it — the authored punish window.
        _velocity = Vector2Zero();
        SetSignatureSheet(_idleAnim);
        _signatureTimer -= deltaTime;
        if (_signatureTimer <= 0.f)
        {
            _signatureState = SignatureState::None;
            _signatureCooldown = EliteSignatureValueOr(AttackTuningStore::Get("Infernal_Furnace_Burst"),
                                                       &AttackTuning::signatureCooldown,
                                                       Balance::Elite::kInfernalSignatureCooldown);
            EmitEliteEvent({ EliteEventKind::Recover, EliteArchetype::Infernal,
                             EliteMove::InfernalFurnaceBurst, 0, _worldPos });
            SetIdleAnimation(true);
        }
        return true;
    }

    return false;
}

void Infernal::DrawEliteTelegraph() const
{
    // Warnings only — the fissure/patch art itself is animated VFX owned by
    // CombatDirector once the events execute.
    if (_signatureState == SignatureState::BurstTelegraph)
    {
        for (int fissureIndex = 0; fissureIndex < 3; ++fissureIndex)
        {
            Vector2 fissureDirection = EliteSpreadDirection(
                _signatureDirection, fissureIndex, 3, 50.f * DEG2RAD);
            Vector2 fissureEnd = Vector2Add(_worldPos,
                Vector2Scale(fissureDirection, Balance::Elite::kInfernalFissureLength));
            Vector2 side{ -fissureDirection.y, fissureDirection.x };
            const float halfWidth = Balance::Elite::kInfernalFissureWidth;
            Vector2 cornerA = Vector2Add(_worldPos, Vector2Scale(side,  halfWidth));
            Vector2 cornerB = Vector2Add(_worldPos, Vector2Scale(side, -halfWidth));
            Vector2 cornerC = Vector2Add(fissureEnd, Vector2Scale(side, -halfWidth));
            Vector2 cornerD = Vector2Add(fissureEnd, Vector2Scale(side,  halfWidth));
            Color warn = Fade(Color{ 255, 120, 40, 255 }, 0.22f);
            DrawTriangle(cornerA, cornerB, cornerC, warn);
            DrawTriangle(cornerA, cornerC, cornerD, warn);
            DrawLineEx(cornerB, cornerC, 2.f, Fade(Color{ 255, 160, 70, 255 }, 0.6f));
            DrawLineEx(cornerA, cornerD, 2.f, Fade(Color{ 255, 160, 70, 255 }, 0.6f));
        }
    }
    else if (_signatureState == SignatureState::MarchTelegraph)
    {
        // Thin committed-line warning for the march path.
        Vector2 marchEnd = Vector2Add(_worldPos,
            Vector2Scale(_signatureDirection, _speed * 1.6f * Balance::Elite::kInfernalMarchActive));
        DrawLineEx(_worldPos, marchEnd, 6.f, Fade(Color{ 255, 120, 40, 255 }, 0.35f));
    }
}

void Infernal::DebugForceEliteSignature()
{
    if (_dying || !IsAlive())
        return;
    _signatureCooldown = 0.f;
}

void Infernal::DebugForceElitePhaseTwo()
{
    _health = std::min(_health, std::max(1.f, std::floor(_maxHealth * 0.5f)));
}

const char* Infernal::GetEliteSignatureStateName() const
{
    switch (_signatureState)
    {
    case SignatureState::MarchTelegraph: return "MarchTelegraph";
    case SignatureState::Marching:       return "Marching";
    case SignatureState::BurstTelegraph: return "BurstTelegraph";
    case SignatureState::BurstRecovery:  return "BurstRecovery";
    default:                             return "Pursuit";
    }
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
