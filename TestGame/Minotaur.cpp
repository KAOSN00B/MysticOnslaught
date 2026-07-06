#include "Minotaur.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "Character.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

// ---- Static member definitions ----------------------------------------------
Texture2D Minotaur::_sharedIdleAnim{};
Texture2D Minotaur::_sharedWalkAnim{};
Texture2D Minotaur::_sharedMeleeAnim{};
Texture2D Minotaur::_sharedStompAnim{};
Texture2D Minotaur::_sharedHurtAnim{};
Texture2D Minotaur::_sharedDeathAnim{};
Sound     Minotaur::_sharedAttackSound{};
Sound     Minotaur::_sharedHurtSound{};
Sound     Minotaur::_sharedDeathSound{};
Sound     Minotaur::_sharedRushSound{};
Sound     Minotaur::_sharedStompSound{};
bool      Minotaur::_sharedResourcesLoaded = false;

// =============================================================================
Minotaur::Minotaur(Vector2 pos)
    : Enemy(pos)
{
}

Minotaur::~Minotaur() {}

// =============================================================================
void Minotaur::Init()
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
void Minotaur::ResetForSpawn(Vector2 pos)
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

    _health      = 70.f;
    _maxHealth   = 70.f;
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

    _state           = State::Chasing;
    _stateTimer      = 0.f;
    _meleeCooldown   = 1.2f;
    _contactCooldown = 1.0f;
    _rushCooldown    = 3.0f;
    _stompCooldown   = 4.0f;
    _rushDirection   = Vector2{ 1.f, 0.f };
    _rushTravelled   = 0.f;
    _rushHitPlayer   = false;
    _rushChainRemaining = 0;
    _stompRingRadius = 0.f;
    _stompDamageApplied   = false;
    _impactShakeRequested = false;

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

int Minotaur::GetCurrentAnimSlot() const
{
    if (_texture.id == _sharedIdleAnim.id)  return 0;
    if (_texture.id == _sharedWalkAnim.id)  return 1;
    if (_texture.id == _sharedMeleeAnim.id) return 2;
    if (_texture.id == _sharedStompAnim.id) return 3;
    if (_texture.id == _sharedHurtAnim.id)  return 4;
    if (_texture.id == _sharedDeathAnim.id) return 5;
    return 0;
}

const char* Minotaur::GetEditorAnimName(int index) const
{
    static const char* kAnimNames[6] = { "Idle", "Walk", "Melee", "Stomp", "Hurt", "Death" };
    return (index >= 0 && index < 6) ? kAnimNames[index] : "";
}

void Minotaur::PlayEditorAnim(int index)
{
    const Texture2D* sheets[6] = {
        &_sharedIdleAnim, &_sharedWalkAnim, &_sharedMeleeAnim,
        &_sharedStompAnim, &_sharedHurtAnim, &_sharedDeathAnim
    };
    if (index < 0 || index > 5)
        return;

    float frameTimeOverride = _editorAnimFrameTimes[index];
    SetAnimation(*sheets[index], (frameTimeOverride > 0.f) ? frameTimeOverride : 1.f / 8.f, true);
}

void Minotaur::SetAnimation(const Texture2D& sheet, float frameTime, bool resetFrame)
{
    SetSpriteSheet(sheet, _sheetFrameCount, frameTime, resetFrame);
}

// =============================================================================
void Minotaur::Update(float dt, Vector2 heroWorldPos, Vector2 /*navigationTarget*/, bool /*hasNavigationTarget*/,
    const std::vector<std::unique_ptr<Enemy>>& /*enemies*/,
    const std::vector<Vector2>& /*propCenters*/)
{
    if (!_isActive)
        return;

    _worldPosLastFrame = _worldPos;

    UpdateHit(dt);
    UpdateBurns(dt);
    UpdateElectricCharge(dt);

    if (_freezeTimer > 0.f)
        _freezeTimer -= dt;
    if (_meleeCooldown > 0.f)
        _meleeCooldown -= dt;
    if (_contactCooldown > 0.f)
        _contactCooldown -= dt;
    if (_state == State::Chasing)
    {
        if (_rushCooldown > 0.f)  _rushCooldown -= dt;
        if (_stompCooldown > 0.f) _stompCooldown -= dt;
    }

    if (!_dying && _target != nullptr)
    {
        bool controlled = IsFrozen() || IsElectroStunned();

        switch (_state)
        {
        case State::Chasing:        if (!controlled) HandleChasing(dt, heroWorldPos); break;
        case State::MeleeAttacking: HandleMelee();          break;
        case State::RushWindup:     HandleRushWindup(dt);   break;
        case State::Rushing:        HandleRushing(dt);      break;
        case State::Stunned:        HandleStunned(dt);      break;
        case State::StompWindup:    HandleStompWindup(dt);  break;
        case State::Stomping:       HandleStomping(dt);     break;
        case State::Recovery:       HandleRecovery(dt);     break;
        }

        if (_state != State::Rushing)
            TryDealContactDamage();

        _worldPos.x = std::clamp(_worldPos.x, 200.f, (float)kVirtualWidth  - 200.f);
        _worldPos.y = std::clamp(_worldPos.y, 200.f, (float)kVirtualHeight - 200.f);
    }

    HandleAnimation(dt);
}

// =============================================================================
void Minotaur::HandleChasing(float dt, Vector2 heroWorldPos)
{
    Vector2 toPlayer = Vector2Subtract(heroWorldPos, _worldPos);
    float dist = Vector2Length(toPlayer);

    if (toPlayer.x < -20.f) _rightLeft = -1.f;
    if (toPlayer.x >  20.f) _rightLeft =  1.f;

    // Stomp when the player hugs him; rush when they keep distance.
    if (_stompCooldown <= 0.f && dist < 240.f)
    {
        _state = State::StompWindup;
        _stateTimer = 0.f;
        SetAnimation(_sharedStompAnim, _stompWindupDuration / (float)_sheetFrameCount, true);
        return;
    }
    if (_rushCooldown <= 0.f && dist > 320.f)
    {
        _state = State::RushWindup;
        _stateTimer = 0.f;
        // Enraged bulls charge twice back-to-back.
        _rushChainRemaining = IsEnraged() ? 1 : 0;
        SetAnimation(_sharedWalkAnim, 1.f / 16.f, true);   // fast paw-the-ground shuffle
        return;
    }

    // Axe cleave when close.
    if (dist < _meleeRange && _meleeCooldown <= 0.f)
    {
        _state = State::MeleeAttacking;
        _damageApplied = false;
        SetAnimation(_sharedMeleeAnim, 1.f / 11.f, true);
        PlaySound(_attackSound);
        return;
    }

    float speed = IsEnraged() ? _speed * 1.3f : _speed;
    if (dist > 0.01f)
        _worldPos = Vector2Add(_worldPos, Vector2Scale(Vector2Scale(toPlayer, 1.f / dist), speed * dt));

    if (_texture.id != _walkAnim.id && !_takingDamage)
        SetAnimation(_sharedWalkAnim, 1.f / 9.f, true);
}

void Minotaur::HandleMelee()
{
    _velocity = Vector2Zero();

    if (!_damageApplied && _frame >= 3 && _target != nullptr)
    {
        // Per-animation melee box (Character Animator, slot 2) wins.
        Rectangle attackRec;
        if (!GetAnimMeleeRectWorld(2, attackRec))
        {
            attackRec = GetBodyContactRec();
            attackRec.width += 110.f;
            if (_rightLeft < 0.f)
                attackRec.x -= 110.f;
            attackRec.y -= 24.f;
            attackRec.height += 48.f;
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
        _meleeCooldown = IsEnraged() ? _meleeCooldownBase * 0.6f : _meleeCooldownBase;
        SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
    }
}

void Minotaur::HandleRushWindup(float dt)
{
    _velocity = Vector2Zero();
    _stateTimer += dt;

    // Keep re-aiming during the windup; the charge line locks on release.
    if (_target != nullptr)
    {
        Vector2 toPlayer = Vector2Subtract(_target->GetFeetWorldPos(), _worldPos);
        if (toPlayer.x < 0.f) _rightLeft = -1.f;
        if (toPlayer.x > 0.f) _rightLeft =  1.f;
        if (Vector2LengthSqr(toPlayer) > 0.0001f)
            _rushDirection = Vector2Normalize(toPlayer);
    }

    if (_stateTimer >= _rushWindupDuration)
    {
        _state = State::Rushing;
        _rushTravelled = 0.f;
        _rushHitPlayer = false;
        PlaySound(_sharedRushSound);
        SetAnimation(_sharedWalkAnim, 1.f / 18.f, true);
    }
}

void Minotaur::HandleRushing(float dt)
{
    float step = _rushSpeed * dt;
    _worldPos = Vector2Add(_worldPos, Vector2Scale(_rushDirection, step));
    _rushTravelled += step;

    // Trample the player once per rush.
    if (!_rushHitPlayer && _target != nullptr &&
        CheckCollisionRecs(GetBodyContactRec(), _target->GetCollisionRec()))
    {
        _rushHitPlayer = true;
        _target->TakeFractionalDamage(1.0f, _worldPos);
        _target->StartForcedPush(GetPushDirectionToPlayer(), _bossPushSpeed);
    }

    // The rush ends on a wall (arena edge) or after the max distance.
    bool hitWall =
        _worldPos.x <= 200.f || _worldPos.x >= (float)kVirtualWidth  - 200.f ||
        _worldPos.y <= 200.f || _worldPos.y >= (float)kVirtualHeight - 200.f;

    if (hitWall || _rushTravelled >= _rushMaxDistance)
        EndRush(hitWall);
}

void Minotaur::EndRush(bool crashedIntoWall)
{
    _impactShakeRequested = true;
    PlaySound(_sharedStompSound);

    if (crashedIntoWall)
    {
        // Head-first into stone — long stun and full damage taken. This is the
        // fight's core punish window: bait the rush, sidestep, unload.
        _state = State::Stunned;
        _stateTimer = 0.f;
        _rushChainRemaining = 0;   // the crash knocks the second charge out of him
        _rushCooldown = _rushCooldownBase;
        SetAnimation(_sharedHurtAnim, _stunDuration / (float)_sheetFrameCount, true);
        return;
    }

    // Ran out of steam mid-arena: enraged bulls immediately wheel around for
    // the chained second charge; otherwise a brief recovery.
    if (_rushChainRemaining > 0)
    {
        _rushChainRemaining--;
        _state = State::RushWindup;
        _stateTimer = _rushWindupDuration - _rushChainWindup;   // short re-aim only
        SetAnimation(_sharedWalkAnim, 1.f / 16.f, true);
        return;
    }

    _state = State::Recovery;
    _stateTimer = IsEnraged() ? -0.1f : -0.35f;   // longer stagger than normal recovery
    _rushCooldown = IsEnraged() ? _rushCooldownBase * 0.55f : _rushCooldownBase;
    SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
}

void Minotaur::HandleStunned(float dt)
{
    _velocity = Vector2Zero();
    _stateTimer += dt;
    if (_stateTimer >= _stunDuration)
    {
        _state = State::Recovery;
        _stateTimer = 0.f;
        SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
    }
}

void Minotaur::HandleStompWindup(float dt)
{
    _velocity = Vector2Zero();
    _stateTimer += dt;

    if (_stateTimer >= _stompWindupDuration)
    {
        _state = State::Stomping;
        _stateTimer = 0.f;
        _stompRingRadius = 0.f;
        _stompDamageApplied = false;
        _impactShakeRequested = true;
        PlaySound(_sharedStompSound);
    }
}

void Minotaur::HandleStomping(float dt)
{
    _velocity = Vector2Zero();
    _stateTimer += dt;

    // Expanding ring over 0.35 seconds; damage applies when it reaches the player.
    _stompRingRadius = std::min(_stompRadius, (_stateTimer / 0.35f) * _stompRadius);

    if (!_stompDamageApplied && _target != nullptr)
    {
        float distToPlayer = Vector2Distance(_worldPos, _target->GetFeetWorldPos());
        if (distToPlayer <= _stompRingRadius && distToPlayer <= _stompRadius)
        {
            _stompDamageApplied = true;
            _target->TakeFractionalDamage(1.0f, _worldPos);
            _target->StartForcedPush(GetPushDirectionToPlayer(), _bossPushSpeed);
        }
    }

    if (_stateTimer >= 0.45f)
    {
        _state = State::Recovery;
        _stateTimer = 0.f;
        _stompCooldown = IsEnraged() ? _stompCooldownBase * 0.6f : _stompCooldownBase;
        SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
    }
}

void Minotaur::HandleRecovery(float dt)
{
    _stateTimer += dt;
    float duration = IsEnraged() ? _recoveryDuration * 0.6f : _recoveryDuration;
    if (_stateTimer >= duration)
    {
        _state = State::Chasing;
        _stateTimer = 0.f;
    }
}

// =============================================================================
void Minotaur::TryDealContactDamage()
{
    if (_target == nullptr || !_target->IsAlive())
        return;
    if (_target->IsBeingForcedPushed())
        return;
    // No contact damage while dazed — the player is supposed to move in close
    // and punish the wall crash.
    if (_state == State::MeleeAttacking || _state == State::Stomping || _state == State::Stunned)
        return;
    if (_contactCooldown > 0.f)
        return;

    if (!CheckCollisionRecs(GetBodyContactRec(), _target->GetCollisionRec()))
        return;

    _target->TakeFractionalDamage(_bossDamagePerHit, _worldPos);
    _target->StartForcedPush(GetPushDirectionToPlayer(), _bossPushSpeed);
    _contactCooldown = _contactCooldownBase;
}

Rectangle Minotaur::GetBodyContactRec() const
{
    Rectangle bodyRec = GetCollisionRec();
    bodyRec.x += 30.f;
    bodyRec.y += 24.f;
    bodyRec.width  = std::max(1.f, bodyRec.width  - 60.f);
    bodyRec.height = std::max(1.f, bodyRec.height - 48.f);
    return bodyRec;
}

Vector2 Minotaur::GetPushDirectionToPlayer() const
{
    if (_target == nullptr)
        return Vector2{ 1.f, 0.f };
    Vector2 away = Vector2Subtract(_target->GetWorldPos(), _worldPos);
    if (Vector2Length(away) <= 0.01f)
        return Vector2{ 1.f, 0.f };
    return Vector2Normalize(away);
}

bool Minotaur::ConsumeImpactShakeRequest()
{
    bool requested = _impactShakeRequested;
    _impactShakeRequested = false;
    return requested;
}

// =============================================================================
void Minotaur::HandleAnimation(float dt)
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
            // Stomp windup holds its final raised-leg frame; the wall-crash
            // stun holds its final dazed frame for the full duration.
            if (_state == State::StompWindup || _state == State::Stomping || _state == State::Stunned)
            {
                _frame = _maxFrames - 1;
                return;
            }
            _frame = 0;
        }
    }
}

// =============================================================================
void Minotaur::DrawEnemy(Vector2 cameraRef)
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

    DrawEllipse((int)screenPos.x, (int)(screenPos.y + drawHeight * 0.36f),
        drawWidth * 0.30f, drawHeight * 0.10f, Fade(BLACK, 0.35f));

    // Charge-line telegraph during the rush windup.
    if (_state == State::RushWindup)
    {
        float pulse = sinf((float)GetTime() * 14.f) * 0.5f + 0.5f;
        Vector2 lineEnd = Vector2Add(screenPos, Vector2Scale(_rushDirection, _rushMaxDistance));
        DrawLineEx(screenPos, lineEnd, 10.f, Fade(Color{ 255, 80, 60, 255 }, 0.16f + 0.08f * pulse));
        DrawLineEx(screenPos, lineEnd, 4.f,  Fade(Color{ 255, 120, 90, 255 }, 0.55f + 0.15f * pulse));
    }

    // Rush dust trail.
    if (_state == State::Rushing)
    {
        for (int i = 0; i < 3; i++)
        {
            float backOffset = (float)GetRandomValue(20, 90);
            Vector2 dust = Vector2Subtract(screenPos, Vector2Scale(_rushDirection, backOffset));
            dust.y += (float)GetRandomValue(-16, 30);
            DrawCircleV(dust, (float)GetRandomValue(4, 11), Fade(Color{ 180, 160, 130, 255 }, 0.4f));
        }
    }

    // Stunned after a wall crash — stars circling the head sell the daze and
    // signal "free hits" loud and clear.
    if (_state == State::Stunned)
    {
        float starSpin = (float)GetTime() * 6.f;
        for (int i = 0; i < 4; i++)
        {
            float angle = starSpin + (float)i * (2.f * PI / 4.f);
            Vector2 starPos{
                screenPos.x + cosf(angle) * drawWidth * 0.34f,
                screenPos.y - drawHeight * 0.42f + sinf(angle) * 14.f
            };
            DrawPoly(starPos, 4, 9.f, starSpin * 40.f + i * 45.f, Fade(YELLOW, 0.9f));
        }
    }

    // Stomp shockwave ring.
    if (_state == State::Stomping)
    {
        DrawCircleLines((int)screenPos.x, (int)screenPos.y, _stompRingRadius, Fade(Color{ 255, 170, 60, 255 }, 0.85f));
        DrawCircleLines((int)screenPos.x, (int)screenPos.y, _stompRingRadius * 0.85f, Fade(Color{ 255, 120, 40, 255 }, 0.5f));
    }
    // Stomp danger radius preview during the windup.
    if (_state == State::StompWindup)
    {
        float pulse = sinf((float)GetTime() * 12.f) * 0.5f + 0.5f;
        DrawCircleLines((int)screenPos.x, (int)screenPos.y, _stompRadius, Fade(Color{ 255, 140, 40, 255 }, 0.35f + 0.3f * pulse));
    }

    Vector2 animDrawOffset = GetCurrentAnimDrawOffset();
    Rectangle source{ _frame * _width, 0.f, _rightLeft * _width, _height };
    Rectangle dest{ screenPos.x - drawWidth / 2.f + animDrawOffset.x,
                    screenPos.y - drawHeight / 2.f + animDrawOffset.y, drawWidth, drawHeight };
    DrawTexturePro(_texture, source, dest, Vector2{}, 0.f, tint);

    DrawHealthBar(screenPos, drawWidth, drawHeight);
}

Rectangle Minotaur::GetCollisionRec() const
{
    Rectangle animBodyRect;
    if (GetAnimBodyRectWorld(animBodyRect))
        return animBodyRect;
    if (_hasTunedCollision)
        return GetTunedCollisionRec();

    float halfW = _stableFrameW * _scale * 0.5f;
    float halfH = _stableFrameH * _scale * 0.5f;
    return Rectangle{
        _worldPos.x - halfW * 0.52f,
        _worldPos.y - halfH * 0.55f,
        halfW * 1.04f,
        halfH * 1.20f
    };
}

Capsule2D Minotaur::GetCapsule() const
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
        { _worldPos.x, _worldPos.y + 14.f },
        20.f,
        _stableFrameW * _scale * 0.24f
    };
}

// =============================================================================
void Minotaur::TakeDamage(int damage, Vector2 attackerPos)
{
    (void)attackerPos;
    if (_dying)
        return;
    if (_hitTimer > 0.f)
        return;

    int appliedDamage = damage;
    // Half damage while committed to the rush — dodge and punish the recovery.
    if (_state == State::RushWindup || _state == State::Rushing)
        appliedDamage = std::max(1, (int)std::ceil(damage * 0.5f));

    _health -= (float)appliedDamage;

    if (_health > 0.f)
        PlayHurtSound();

    if (_health <= 0.f)
    {
        _health = 0.f;
        _dying = true;
        _takingDamage = false;
        _attacking = false;
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

void Minotaur::ApplyFreeze(float duration)
{
    if (_dying || !IsAlive())
        return;
    // A committed rush cannot be frozen; everything else can.
    if (_state == State::Rushing)
        return;
    if (duration > _freezeTimer)
        _freezeTimer = duration;
}

// =============================================================================
void Minotaur::SetWaveScale(int wave)
{
    (void)wave;
    _expValue    = _bossBaseExpValue;
    _health      = Balance::Boss::kMinotaurHealth;
    _maxHealth   = Balance::Boss::kMinotaurHealth;
_enrageThreshold = 0.40f;
    _speed       = _moveSpeed;
    _attackPower = 1.f;
}

// =============================================================================
void Minotaur::EnsureSharedResourcesLoaded()
{
    if (_sharedResourcesLoaded)
        return;

    _sharedIdleAnim  = LoadTexture(AssetPath("Bosses/MinotaurIdle.png").c_str());
    _sharedWalkAnim  = LoadTexture(AssetPath("Bosses/MinotaurWalk.png").c_str());
    _sharedMeleeAnim = LoadTexture(AssetPath("Bosses/MinotaurMeleeAttack.png").c_str());
    _sharedStompAnim = LoadTexture(AssetPath("Bosses/MinotaurStomp.png").c_str());
    _sharedHurtAnim  = LoadTexture(AssetPath("Bosses/MinotaurHurt.png").c_str());
    _sharedDeathAnim = LoadTexture(AssetPath("Bosses/MinotaurDeath.png").c_str());
    _sharedAttackSound = LoadSound(AssetPath("Sounds/SwordSwipe2.ogg").c_str());
    _sharedHurtSound   = LoadSound(AssetPath("Sounds/SmallMonsterDamage.ogg").c_str());
    _sharedDeathSound  = LoadSound(AssetPath("Sounds/MonsterDeath.ogg").c_str());
    _sharedRushSound   = LoadSound(AssetPath("Sounds/OgreChargeHit.ogg").c_str());
    _sharedStompSound  = LoadSound(AssetPath("Sounds/OgreHitWall.ogg").c_str());
    _sharedResourcesLoaded = true;
}

void Minotaur::UnloadSharedResources()
{
    if (!_sharedResourcesLoaded)
        return;

    UnloadTexture(_sharedIdleAnim);
    UnloadTexture(_sharedWalkAnim);
    UnloadTexture(_sharedMeleeAnim);
    UnloadTexture(_sharedStompAnim);
    UnloadTexture(_sharedHurtAnim);
    UnloadTexture(_sharedDeathAnim);
    UnloadSound(_sharedAttackSound);
    UnloadSound(_sharedHurtSound);
    UnloadSound(_sharedDeathSound);
    UnloadSound(_sharedRushSound);
    UnloadSound(_sharedStompSound);

    _sharedIdleAnim  = Texture2D{};
    _sharedWalkAnim  = Texture2D{};
    _sharedMeleeAnim = Texture2D{};
    _sharedStompAnim = Texture2D{};
    _sharedHurtAnim  = Texture2D{};
    _sharedDeathAnim = Texture2D{};
    _sharedAttackSound = Sound{};
    _sharedHurtSound   = Sound{};
    _sharedDeathSound  = Sound{};
    _sharedRushSound   = Sound{};
    _sharedStompSound  = Sound{};
    _sharedResourcesLoaded = false;
}
