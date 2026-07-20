#include "PumpkinJack.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "Character.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

// ---- Static member definitions ----------------------------------------------
Texture2D PumpkinJack::_sharedIdleAnim{};
Texture2D PumpkinJack::_sharedWalkAnim{};
Texture2D PumpkinJack::_sharedMeleeAnim{};
Texture2D PumpkinJack::_sharedMagicAnim{};
Texture2D PumpkinJack::_sharedDefendAnim{};
Texture2D PumpkinJack::_sharedHurtAnim{};
Texture2D PumpkinJack::_sharedDeathAnim{};
Sound     PumpkinJack::_sharedAttackSound{};
Sound     PumpkinJack::_sharedHurtSound{};
Sound     PumpkinJack::_sharedDeathSound{};
Sound     PumpkinJack::_sharedCastSound{};
bool      PumpkinJack::_sharedResourcesLoaded = false;

// =============================================================================
PumpkinJack::PumpkinJack(Vector2 pos)
    : Enemy(pos)
{
}

PumpkinJack::~PumpkinJack() {}

// =============================================================================
void PumpkinJack::Init()
{
    EnsureSharedResourcesLoaded();
    _healthBarHeight  = 8.f;
    _healthBarYFrac   = 0.62f;
    _healthBarYOffset = 14.f;

    _idleAnim       = _sharedIdleAnim;
    _walkAnim       = _sharedWalkAnim;
    _attackAnim     = _sharedMeleeAnim;
    _takeDamageAnim = _sharedHurtAnim;
    _deathAnim      = _sharedDeathAnim;
    _attackSound    = _sharedAttackSound;
    _hurtSound      = _sharedHurtSound;
    _deathSound     = _sharedDeathSound;

    ResetForSpawn(_worldPos);
}

// =============================================================================
void PumpkinJack::ResetForSpawn(Vector2 pos)
{
    _worldPos          = pos;
    _worldPosLastFrame = pos;
    _homePos           = pos;
    _velocity          = Vector2Zero();
    _isActive          = true;

    _stableFrameW = (float)_sharedIdleAnim.width / (float)_sheetFrameCount;
    _stableFrameH = (float)_sharedIdleAnim.height;

    SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
    _scale = _bossScale;

    _health      = 55.f;
    _maxHealth   = 55.f;
    _attackPower = 1.f;
    _speed       = _moveSpeed;
    _expValue    = _bossBaseExpValue;

    _hitTimer                 = 0.f;
    _deathTimer               = 0.7f;
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

    _state          = State::Chasing;
    _stateTimer     = 0.f;
    _meleeCooldown  = 1.2f;
    _contactCooldown = 1.0f;
    _volleyCooldown = 3.0f;
    _summonCooldown = 7.0f;
    _defendCooldown = 5.0f;
    _pendingVolley  = false;
    _volleyDirection = Vector2Zero();
    _volleyBoltCount = 3;
    _pendingSummonCount = 0;
    _teleportCooldown = 2.0f;
    _teleportTarget   = pos;
    _teleportAmbush   = false;
    _pendingPhaseSummon = false;
    ClearEliteEvents();

    // 3 phases: 66% unlocks the behind-the-player ambush + 5-bolt volleys; 33% blinks
    // more and reappears straight into a volley (blink-barrage) with bigger grave calls.
    SetPhaseThresholds({ 0.66f, 0.33f });

    _forcedPushActive    = false;
    _forcedPushDirection = Vector2Zero();
    _forcedPushSpeed     = 0.f;

    _pendingBurns.clear();
    _waypoints.clear();
    _waypointIndex = 0;

    // Character Animator overrides (scale, hitboxes, anim speeds) win last.
    ResetTuningState();
    ApplyStoredTuning();
}

int PumpkinJack::GetCurrentAnimSlot() const
{
    if (_texture.id == _sharedIdleAnim.id)   return 0;
    if (_texture.id == _sharedWalkAnim.id)   return 1;
    if (_texture.id == _sharedMeleeAnim.id)  return 2;
    if (_texture.id == _sharedMagicAnim.id)  return 3;
    if (_texture.id == _sharedDefendAnim.id) return 4;
    if (_texture.id == _sharedHurtAnim.id)   return 5;
    if (_texture.id == _sharedDeathAnim.id)  return 6;
    return 0;
}

const char* PumpkinJack::GetEditorAnimName(int index) const
{
    static const char* kAnimNames[7] = { "Idle", "Walk", "Melee", "Magic", "Defend", "Hurt", "Death" };
    return (index >= 0 && index < 7) ? kAnimNames[index] : "";
}

void PumpkinJack::PlayEditorAnim(int index)
{
    const Texture2D* sheets[7] = {
        &_sharedIdleAnim, &_sharedWalkAnim, &_sharedMeleeAnim, &_sharedMagicAnim,
        &_sharedDefendAnim, &_sharedHurtAnim, &_sharedDeathAnim
    };
    if (index < 0 || index > 6)
        return;

    float frameTimeOverride = _editorAnimFrameTimes[index];
    SetAnimation(*sheets[index], (frameTimeOverride > 0.f) ? frameTimeOverride : 1.f / 8.f, true);
}

void PumpkinJack::SetAnimation(const Texture2D& sheet, float frameTime, bool resetFrame)
{
    SetSpriteSheet(sheet, _sheetFrameCount, frameTime, resetFrame);
}

// =============================================================================
void PumpkinJack::Update(float dt, Vector2 heroWorldPos, Vector2 /*navigationTarget*/, bool /*hasNavigationTarget*/,
    const std::vector<std::unique_ptr<Enemy>>& /*enemies*/,
    const std::vector<Vector2>& /*propCenters*/)
{
    if (!_isActive)
        return;

    _worldPosLastFrame = _worldPos;

    UpdateHit(dt);
    UpdateBurns(dt);
    UpdateElectricCharge(dt);

    if (_hitTimer > 0.f)
        _hitTimer -= dt;

    if (_freezeTimer > 0.f)
        _freezeTimer -= dt;
    if (_meleeCooldown > 0.f)
        _meleeCooldown -= dt;
    if (_contactCooldown > 0.f)
        _contactCooldown -= dt;
    if (_state == State::Chasing)
    {
        if (_volleyCooldown > 0.f)   _volleyCooldown -= dt;
        if (_summonCooldown > 0.f)   _summonCooldown -= dt;
        if (_defendCooldown > 0.f)   _defendCooldown -= dt;
        if (_teleportCooldown > 0.f) _teleportCooldown -= dt;
    }

    if (!_dying && _target != nullptr)
    {
        bool controlled = IsFrozen() || IsElectroStunned();

        // Each phase change opens with a Grave Call summon (fired when he's chasing).
        int newPhase = ConsumePhaseChange();
        if (newPhase >= 0)
        {
            EmitEliteEvent({ EliteEventKind::PhaseChange, EliteArchetype::Ogre,
                             EliteMove::None, 0, _worldPos });
            _pendingPhaseSummon = true;
            RequestBossCallout(newPhase >= 2 ? "HARVEST RITUAL" : "GRAVE CALL");
        }
        if (_pendingPhaseSummon && !controlled && _state == State::Chasing)
        {
            _pendingPhaseSummon = false;
            _summonCooldown = 0.f;   // HandleChasing will roll straight into Summoning
        }

        switch (_state)
        {
        case State::Chasing:        if (!controlled) HandleChasing(dt, heroWorldPos); break;
        case State::MeleeAttacking: HandleMelee();          break;
        case State::VolleyCasting:  HandleVolleyCast(dt);   break;
        case State::Summoning:      HandleSummoning(dt);    break;
        case State::Defending:      HandleDefending(dt);    break;
        case State::TeleportOut:    HandleTeleportOut(dt);  break;
        case State::TeleportIn:     HandleTeleportIn(dt);   break;
        case State::Recovery:       HandleRecovery(dt);     break;
        }

        if (_state != State::TeleportOut && _state != State::TeleportIn)
            TryDealContactDamage();

        _worldPos.x = std::clamp(_worldPos.x, 190.f, (float)kVirtualWidth  - 190.f);
        _worldPos.y = std::clamp(_worldPos.y, 190.f, (float)kVirtualHeight - 190.f);
    }

    HandleAnimation(dt);
}

// =============================================================================
void PumpkinJack::HandleChasing(float dt, Vector2 heroWorldPos)
{
    Vector2 toPlayer = Vector2Subtract(heroWorldPos, _worldPos);
    float dist = Vector2Length(toPlayer);

    if (toPlayer.x < -20.f) _rightLeft = -1.f;
    if (toPlayer.x >  20.f) _rightLeft =  1.f;

    // Specials, priority order: summon (rare), defend (when pressured), volley.
    if (_summonCooldown <= 0.f)
    {
        _state = State::Summoning;
        _stateTimer = 0.f;
        SetAnimation(_sharedMagicAnim, _summonCastDuration / (float)_sheetFrameCount, true);
        PlaySound(_sharedCastSound);
        return;
    }
    if (_defendCooldown <= 0.f && dist < 260.f)
    {
        _state = State::Defending;
        _stateTimer = 0.f;
        SetAnimation(_sharedDefendAnim, 1.f / 8.f, true);
        return;
    }
    if (_volleyCooldown <= 0.f && dist > 220.f)
    {
        _state = State::VolleyCasting;
        _stateTimer = 0.f;
        SetAnimation(_sharedMagicAnim, _volleyCastDuration / (float)_sheetFrameCount, true);
        PlaySound(_sharedCastSound);
        return;
    }

    // Melee when close.
    if (dist < _meleeRange && _meleeCooldown <= 0.f)
    {
        _state = State::MeleeAttacking;
        _damageApplied = false;
        SetAnimation(_sharedMeleeAnim, 1.f / 11.f, true);
        PlaySound(_attackSound);
        return;
    }

    // Trick Step — Jack never closes distance on foot. At range he vanishes
    // and reappears somewhere new instead of walking.
    if (_teleportCooldown <= 0.f && dist > 300.f)
    {
        BeginTeleport();
        return;
    }

    // Short-range shuffle only (footsies inside melee threat range).
    if (dist > 140.f && dist < 300.f)
    {
        float speed = IsEnraged() ? _speed * 1.25f : _speed;
        _worldPos = Vector2Add(_worldPos, Vector2Scale(Vector2Scale(toPlayer, 1.f / dist), speed * dt));

        if (_texture.id != _walkAnim.id && !_takingDamage)
            SetAnimation(_sharedWalkAnim, 1.f / 9.f, true);
    }
    else if (_texture.id != _idleAnim.id && !_takingDamage)
    {
        SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
    }
}

// =============================================================================
// Trick Step — the trickster's signature. He melts into purple flame and
// reappears elsewhere; enraged, he ambushes directly behind the player.
// =============================================================================
void PumpkinJack::BeginTeleport()
{
    _state = State::TeleportOut;
    _stateTimer = 0.f;
    // Phase 1+: teleports become behind-the-player ambushes (more often at phase 2).
    int ambushChance = (GetPhase() >= 2) ? 75 : (GetPhase() >= 1 ? 55 : 0);
    _teleportAmbush  = GetRandomValue(1, 100) <= ambushChance;
    _teleportTarget  = PickTeleportSpot();
    SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
    PlaySound(_sharedCastSound);
}

Vector2 PumpkinJack::PickTeleportSpot() const
{
    Vector2 playerPos = (_target != nullptr) ? _target->GetFeetWorldPos() : _worldPos;
    Vector2 spot;

    if (_teleportAmbush)
    {
        // Land on the far side of the player from where Jack currently stands
        // — reads as "he appeared behind me".
        Vector2 throughPlayer = Vector2Subtract(playerPos, _worldPos);
        Vector2 dir = (Vector2LengthSqr(throughPlayer) > 0.0001f)
            ? Vector2Normalize(throughPlayer)
            : Vector2{ 1.f, 0.f };
        spot = Vector2Add(playerPos, Vector2Scale(dir, 165.f));
    }
    else
    {
        // Random ring position at comfortable casting range.
        float angle  = (float)GetRandomValue(0, 628) / 100.f;
        float radius = (float)GetRandomValue(300, 430);
        spot = Vector2{ playerPos.x + cosf(angle) * radius,
                        playerPos.y + sinf(angle) * radius };
    }

    spot.x = std::clamp(spot.x, 200.f, (float)kVirtualWidth  - 200.f);
    spot.y = std::clamp(spot.y, 200.f, (float)kVirtualHeight - 200.f);

    // Validated arrival: clamp the spot to navigable ground (walked from the
    // player's position, which is walkable by definition) so Jack can never
    // reappear inside walls, props, or fall terrain.
    static const std::vector<Vector2> kNoProps;
    return ClampElitePathToNavigable(playerPos, spot, kNoProps);
}

void PumpkinJack::DrawEliteTelegraph() const
{
    // Destination cue: while Jack is vanished, a pumpkin-orange marker shows
    // exactly where he will reappear — the ambush "behind you" is a read, not
    // a surprise hit.
    if (_state != State::TeleportOut && _state != State::TeleportIn)
        return;
    const Vector2 markerPosition = (_state == State::TeleportOut) ? _teleportTarget : _worldPos;
    const float pulse = 0.7f + 0.3f * sinf((float)GetTime() * 10.f);
    const Color markerColor = _teleportAmbush ? Color{ 255, 110, 40, 255 }
                                              : Color{ 255, 180, 80, 255 };
    DrawCircleLines((int)markerPosition.x, (int)markerPosition.y, 70.f * pulse,
                    Fade(markerColor, 0.7f));
    DrawCircleLines((int)markerPosition.x, (int)markerPosition.y, 70.f,
                    Fade(markerColor, 0.35f));
    DrawCircleV(markerPosition, 9.f, Fade(markerColor, 0.55f));
}

void PumpkinJack::DebugForceEliteSignature()
{
    if (_dying || !IsAlive() || _state != State::Chasing)
        return;
    _teleportCooldown = 0.f;   // the signature IS the trick-step ambush loop
}

void PumpkinJack::DebugForceElitePhaseTwo()
{
    const float nextThreshold = (GetPhase() == 0) ? 0.65f : 0.32f;
    _health = std::min(_health, std::max(1.f, std::floor(_maxHealth * nextThreshold)));
}

const char* PumpkinJack::GetEliteSignatureStateName() const
{
    switch (_state)
    {
    case State::TeleportOut:   return _teleportAmbush ? "AmbushVanish" : "TrickStepOut";
    case State::TeleportIn:    return "TrickStepIn";
    case State::VolleyCasting: return "VolleyCasting";
    case State::Summoning:     return "GraveCall";
    case State::Defending:     return "Defending";
    default:                   return "Stalking";
    }
}

void PumpkinJack::HandleTeleportOut(float dt)
{
    _velocity = Vector2Zero();
    _stateTimer += dt;
    if (_stateTimer >= _teleportOutDuration)
    {
        _worldPos   = _teleportTarget;
        _state      = State::TeleportIn;
        _stateTimer = 0.f;
    }
}

void PumpkinJack::HandleTeleportIn(float dt)
{
    _velocity = Vector2Zero();
    _stateTimer += dt;
    if (_stateTimer >= _teleportInDuration)
    {
        _teleportCooldown = IsEnraged() ? _teleportCooldownBase * 0.65f : _teleportCooldownBase;
        _stateTimer = 0.f;

        // Immediate follow-up so the teleport always threatens something.
        float dist = (_target != nullptr)
            ? Vector2Distance(_worldPos, _target->GetFeetWorldPos())
            : 9999.f;

        if (dist < _meleeRange * 1.25f)
        {
            _state = State::MeleeAttacking;
            _damageApplied = false;
            _meleeCooldown = IsEnraged() ? _meleeCooldownBase * 0.65f : _meleeCooldownBase;
            SetAnimation(_sharedMeleeAnim, 1.f / 11.f, true);
            PlaySound(_attackSound);
        }
        else if (_volleyCooldown <= 1.2f)   // close-enough cooldown counts as ready
        {
            _state = State::VolleyCasting;
            SetAnimation(_sharedMagicAnim, _volleyCastDuration / (float)_sheetFrameCount, true);
            PlaySound(_sharedCastSound);
        }
        else
        {
            _state = State::Chasing;
        }
    }
}

void PumpkinJack::HandleMelee()
{
    _velocity = Vector2Zero();

    if (!_damageApplied && _frame >= 3 && _target != nullptr)
    {
        // Per-animation melee box (Character Animator, slot 2) wins.
        Rectangle attackRec;
        if (!GetAnimMeleeRectWorld(2, attackRec))
        {
            attackRec = GetBodyContactRec();
            // Extend the swing toward the facing direction.
            attackRec.width += 90.f;
            if (_rightLeft < 0.f)
                attackRec.x -= 90.f;
            attackRec.y -= 20.f;
            attackRec.height += 40.f;
        }

        if (CheckCollisionRecs(attackRec, _target->GetCollisionRec()))
        {
            _target->TakeFractionalDamage(_bossDamagePerHit, _worldPos);
            _target->StartForcedPush(GetPushDirectionToPlayer(), _bossPushSpeed);
        }
        _damageApplied = true;
    }

    if (_frame >= _maxFrames - 1)
    {
        _state = State::Recovery;
        _stateTimer = 0.f;
        _meleeCooldown = IsEnraged() ? _meleeCooldownBase * 0.65f : _meleeCooldownBase;
        SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
    }
}

void PumpkinJack::HandleVolleyCast(float dt)
{
    _velocity = Vector2Zero();
    _stateTimer += dt;

    // Face the player during the cast.
    if (_target != nullptr)
    {
        float dx = _target->GetFeetWorldPos().x - _worldPos.x;
        if (dx < -20.f) _rightLeft = -1.f;
        if (dx >  20.f) _rightLeft =  1.f;
    }

    if (_stateTimer >= _volleyCastDuration)
    {
        Vector2 toPlayer = (_target != nullptr)
            ? Vector2Subtract(_target->GetFeetWorldPos(), _worldPos)
            : Vector2{ _rightLeft, 0.f };
        _pendingVolley   = true;
        _volleyDirection = (Vector2LengthSqr(toPlayer) > 0.0001f)
            ? Vector2Normalize(toPlayer)
            : Vector2{ _rightLeft, 0.f };
        _volleyBoltCount = (GetPhase() >= 1) ? 5 : 3;

        // Blink-Barrage (phase 2): instead of recovering, vanish again for another
        // strike so the volleys come from all sides.
        if (GetPhase() >= 2 && _teleportCooldown <= 1.0f)
        {
            BeginTeleport();
        }
        else
        {
            _state = State::Recovery;
            _stateTimer = 0.f;
            _volleyCooldown = IsEnraged() ? _volleyCooldownBase * 0.7f : _volleyCooldownBase;
            SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
        }
    }
}

void PumpkinJack::HandleSummoning(float dt)
{
    _velocity = Vector2Zero();
    _stateTimer += dt;

    if (_stateTimer >= _summonCastDuration)
    {
        _pendingSummonCount = (GetPhase() >= 2) ? 4 : (GetPhase() >= 1 ? 3 : 2);
        _state = State::Recovery;
        _stateTimer = 0.f;
        _summonCooldown = _summonCooldownBase;
        SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
    }
}

void PumpkinJack::HandleDefending(float dt)
{
    _velocity = Vector2Zero();
    _stateTimer += dt;

    if (_stateTimer >= _defendDuration)
    {
        _state = State::Recovery;
        _stateTimer = 0.f;
        _defendCooldown = _defendCooldownBase;
        SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
    }
}

void PumpkinJack::HandleRecovery(float dt)
{
    _stateTimer += dt;
    float duration = IsEnraged() ? _recoveryDuration * 0.65f : _recoveryDuration;
    if (_stateTimer >= duration)
    {
        _state = State::Chasing;
        _stateTimer = 0.f;
    }
}

// =============================================================================
void PumpkinJack::TryDealContactDamage()
{
    if (_target == nullptr || !_target->IsAlive())
        return;
    if (_target->IsBeingForcedPushed())
        return;
    if (_state == State::MeleeAttacking)
        return;
    if (_contactCooldown > 0.f)
        return;

    if (!CheckCollisionRecs(GetBodyContactRec(), _target->GetCollisionRec()))
        return;

    _target->TakeFractionalDamage(_bossDamagePerHit, _worldPos);
    _target->StartForcedPush(GetPushDirectionToPlayer(), _bossPushSpeed);
    _contactCooldown = _contactCooldownBase;
}

Rectangle PumpkinJack::GetBodyContactRec() const
{
    Rectangle bodyRec = GetCollisionRec();
    bodyRec.x += 26.f;
    bodyRec.y += 20.f;
    bodyRec.width  = std::max(1.f, bodyRec.width  - 52.f);
    bodyRec.height = std::max(1.f, bodyRec.height - 40.f);
    return bodyRec;
}

Vector2 PumpkinJack::GetPushDirectionToPlayer() const
{
    if (_target == nullptr)
        return Vector2{ 1.f, 0.f };
    Vector2 away = Vector2Subtract(_target->GetWorldPos(), _worldPos);
    if (Vector2Length(away) <= 0.01f)
        return Vector2{ 1.f, 0.f };
    return Vector2Normalize(away);
}

// =============================================================================
void PumpkinJack::OnVolleyCast()
{
    _pendingVolley   = false;
    _volleyDirection = Vector2Zero();
}

int PumpkinJack::ConsumeSummonRequest()
{
    int count = _pendingSummonCount;
    _pendingSummonCount = 0;
    return count;
}

// =============================================================================
void PumpkinJack::HandleAnimation(float dt)
{
    _runningTime += dt;
    if (_runningTime >= _updateTime)
    {
        _runningTime = 0.f;
        _frame++;

        if (_frame >= _maxFrames)
        {
            if (_dying || IsFrozen())
            {
                _frame = _maxFrames - 1;
                return;
            }
            if (_takingDamage)
            {
                _takingDamage = false;
                SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
                return;
            }
            // Defend holds its last frame for the full duration.
            if (_state == State::Defending)
            {
                _frame = _maxFrames - 1;
                return;
            }
            _frame = 0;
        }
    }
}

// =============================================================================
void PumpkinJack::DrawEnemy(Vector2 cameraRef)
{
    if (!_isActive)
        return;

    float drawWidth  = _stableFrameW * _scale;
    float drawHeight = _stableFrameH * _scale;

    Vector2 screenPos = Vector2Subtract(_worldPos, cameraRef);
    screenPos.x += kVirtualWidth  / 2.f;
    screenPos.y += kVirtualHeight / 2.f;

    bool burning        = !_pendingBurns.empty();
    bool frozen         = IsFrozen();
    bool electroStunned = IsElectroStunned();

    Color tint = electroStunned ? Color{ 255, 255,  60, 255 } :
                 frozen         ? Color{ 140, 200, 255, 255 } :
                 burning        ? Color{ 255, 180, 180, 255 } :
                                  WHITE;

    // Teleport: shrink away in a swirl of purple flame, grow back on arrival.
    float teleportScale = 1.f;
    if (_state == State::TeleportOut)
        teleportScale = 1.f - (_stateTimer / _teleportOutDuration) * 0.95f;
    else if (_state == State::TeleportIn)
        teleportScale = 0.05f + (_stateTimer / _teleportInDuration) * 0.95f;

    if (_state == State::TeleportOut || _state == State::TeleportIn)
    {
        float swirl = (float)GetTime() * 9.f;
        for (int i = 0; i < 5; i++)
        {
            float angle  = swirl + (float)i * (2.f * PI / 5.f);
            float radius = 30.f + (1.f - teleportScale) * 55.f;
            DrawCircleV(Vector2{ screenPos.x + cosf(angle) * radius,
                                 screenPos.y + sinf(angle) * radius * 0.6f },
                7.f + (1.f - teleportScale) * 6.f,
                Fade(Color{ 170, 70, 230, 255 }, 0.45f));
        }
    }
    drawWidth  *= teleportScale;
    drawHeight *= teleportScale;

    DrawEllipse((int)screenPos.x, (int)(screenPos.y + drawHeight * 0.50f),
        drawWidth * 0.28f, drawHeight * 0.10f, Fade(BLACK, 0.35f * teleportScale));

    // Shield shimmer while defending — clear "don't attack now" signal.
    if (_state == State::Defending)
    {
        float pulse = sinf((float)GetTime() * 10.f) * 0.5f + 0.5f;
        DrawCircleLines((int)screenPos.x, (int)screenPos.y, drawWidth * 0.42f, Fade(Color{ 120, 220, 255, 255 }, 0.55f + 0.25f * pulse));
        DrawCircleV(screenPos, drawWidth * 0.40f, Fade(Color{ 120, 220, 255, 255 }, 0.10f));
    }

    // Cast glow for volley / summon.
    if (_state == State::VolleyCasting || _state == State::Summoning)
    {
        float pulse = sinf((float)GetTime() * 14.f) * 0.5f + 0.5f;
        Color castColor = (_state == State::VolleyCasting)
            ? Color{ 255, 130, 30, 255 }
            : Color{ 150, 70, 220, 255 };
        DrawCircleV(screenPos, drawWidth * (0.34f + 0.08f * pulse), Fade(castColor, 0.22f));
    }

    Vector2 animDrawOffset = GetCurrentAnimDrawOffset();
    Rectangle source{ _frame * _width, 0.f, _rightLeft * _width, _height };
    Rectangle dest{ screenPos.x - drawWidth / 2.f + animDrawOffset.x,
                    screenPos.y - drawHeight / 2.f + animDrawOffset.y, drawWidth, drawHeight };
    DrawTexturePro(_texture, source, dest, Vector2{}, 0.f, tint);

    DrawHealthBar(screenPos, drawWidth, drawHeight);
}

Rectangle PumpkinJack::GetCollisionRec() const
{
    Rectangle animBodyRect;
    if (GetAnimBodyRectWorld(animBodyRect))
        return animBodyRect;
    if (_hasTunedCollision)
        return GetTunedCollisionRec();

    float halfW = _stableFrameW * _scale * 0.5f;
    float halfH = _stableFrameH * _scale * 0.5f;
    return Rectangle{
        _worldPos.x - halfW * 0.50f,
        _worldPos.y - halfH * 0.55f,
        halfW * 1.00f,
        halfH * 1.20f
    };
}

Capsule2D PumpkinJack::GetCapsule() const
{
    // Per-animation body circle wins, then the whole-character tuned capsule.
    Capsule2D animBodyCapsule;
    if (GetAnimBodyCapsuleWorld(animBodyCapsule))
        return animBodyCapsule;

    if (_capsuleRadius > 0.f)
        return Capsule2D{
            { _worldPos.x + _capsuleOffset.x, _worldPos.y + _capsuleOffset.y },
            _capsuleHalfHeight,
            _capsuleRadius
        };

    return Capsule2D{
        { _worldPos.x, _worldPos.y + 12.f },
        18.f,
        _stableFrameW * _scale * 0.22f
    };
}

// =============================================================================
void PumpkinJack::TakeDamage(int damage, Vector2 attackerPos)
{
    (void)attackerPos;
    if (_dying)
        return;
    if (_hitTimer > 0.f)
        return;

    // He isn't really there while phased out — the swirl is untouchable.
    if (_state == State::TeleportOut || _state == State::TeleportIn)
        return;

    int appliedDamage = damage;
    if (_state == State::Defending)
        appliedDamage = std::max(1, damage / 4);   // shield soaks most of the hit

    _health -= (float)appliedDamage;

    if (_health > 0.f)
        PlayHurtSound();

    if (_health <= 0.f)
    {
        _health = 0.f;
        _dying = true;
        _takingDamage = false;
        _attacking = false;
        _pendingVolley = false;
        _pendingSummonCount = 0;
        _state = State::Recovery;
        _deathTimer = 0.7f;
        SetAnimation(_sharedDeathAnim, 1.f / 6.f, true);
        PlayDeathSound();
        return;
    }

    _velocity = Vector2Zero();
    if (_state == State::Chasing)
    {
        _takingDamage = true;
        SetAnimation(_sharedHurtAnim, 1.f / 12.f, true);
    }
    _hitTimer = 0.02f;
}

void PumpkinJack::ApplyFreeze(float duration)
{
    if (_dying || !IsAlive())
        return;
    if (duration > _freezeTimer)
        _freezeTimer = duration;
}

// =============================================================================
void PumpkinJack::SetWaveScale(int wave)
{
    (void)wave;
    _expValue    = _bossBaseExpValue;
    _health      = Balance::Boss::kPumpkinJackHealth;
    _maxHealth   = Balance::Boss::kPumpkinJackHealth;
_enrageThreshold = 0.50f;
    _speed       = _moveSpeed;
    _attackPower = 1.f;
}

// =============================================================================
void PumpkinJack::EnsureSharedResourcesLoaded()
{
    if (_sharedResourcesLoaded)
        return;

    _sharedIdleAnim   = LoadTexture(AssetPath("Bosses/PumpkinJackIdle.png").c_str());
    _sharedWalkAnim   = LoadTexture(AssetPath("Bosses/PumpkinJackWalk.png").c_str());
    _sharedMeleeAnim  = LoadTexture(AssetPath("Bosses/PumpkinJackMeleeAttack.png").c_str());
    _sharedMagicAnim  = LoadTexture(AssetPath("Bosses/PumpkinJackMagicAttack.png").c_str());
    _sharedDefendAnim = LoadTexture(AssetPath("Bosses/PumpkinJackDefend.png").c_str());
    _sharedHurtAnim   = LoadTexture(AssetPath("Bosses/PumpkinJackHurt.png").c_str());
    _sharedDeathAnim  = LoadTexture(AssetPath("Bosses/PumpkinJackDeath.png").c_str());
    _sharedAttackSound = LoadSound(AssetPath("Sounds/SwordSwipe2.ogg").c_str());
    _sharedHurtSound   = LoadSound(AssetPath("Sounds/SmallMonsterDamage.ogg").c_str());
    _sharedDeathSound  = LoadSound(AssetPath("Sounds/MonsterDeath.ogg").c_str());
    _sharedCastSound   = LoadSound(AssetPath("Sounds/GS1_Spell_Fire.ogg").c_str());
    _sharedResourcesLoaded = true;
}

void PumpkinJack::UnloadSharedResources()
{
    if (!_sharedResourcesLoaded)
        return;

    UnloadTexture(_sharedIdleAnim);
    UnloadTexture(_sharedWalkAnim);
    UnloadTexture(_sharedMeleeAnim);
    UnloadTexture(_sharedMagicAnim);
    UnloadTexture(_sharedDefendAnim);
    UnloadTexture(_sharedHurtAnim);
    UnloadTexture(_sharedDeathAnim);
    UnloadSound(_sharedAttackSound);
    UnloadSound(_sharedHurtSound);
    UnloadSound(_sharedDeathSound);
    UnloadSound(_sharedCastSound);

    _sharedIdleAnim   = Texture2D{};
    _sharedWalkAnim   = Texture2D{};
    _sharedMeleeAnim  = Texture2D{};
    _sharedMagicAnim  = Texture2D{};
    _sharedDefendAnim = Texture2D{};
    _sharedHurtAnim   = Texture2D{};
    _sharedDeathAnim  = Texture2D{};
    _sharedAttackSound = Sound{};
    _sharedHurtSound   = Sound{};
    _sharedDeathSound  = Sound{};
    _sharedCastSound   = Sound{};
    _sharedResourcesLoaded = false;
}
