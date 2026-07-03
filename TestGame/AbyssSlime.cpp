#include "AbyssSlime.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "Character.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

// ---- Static member definitions ----------------------------------------------
Texture2D AbyssSlime::_sharedIdleAnim{};
Texture2D AbyssSlime::_sharedWalkAnim{};
Texture2D AbyssSlime::_sharedMeleeAnim{};
Texture2D AbyssSlime::_sharedMagicAnim{};
Texture2D AbyssSlime::_sharedJumpAnim{};
Texture2D AbyssSlime::_sharedFallAnim{};
Texture2D AbyssSlime::_sharedHurtAnim{};
Texture2D AbyssSlime::_sharedDeathAnim{};
Sound     AbyssSlime::_sharedAttackSound{};
Sound     AbyssSlime::_sharedHurtSound{};
Sound     AbyssSlime::_sharedDeathSound{};
Sound     AbyssSlime::_sharedLandSound{};
bool      AbyssSlime::_sharedResourcesLoaded = false;

// =============================================================================
AbyssSlime::AbyssSlime(Vector2 pos)
    : Enemy(pos)
{
}

AbyssSlime::~AbyssSlime() {}

// =============================================================================
void AbyssSlime::Init()
{
    EnsureSharedResourcesLoaded();

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
void AbyssSlime::ResetForSpawn(Vector2 pos)
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

    _health      = 60.f;
    _maxHealth   = 60.f;
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

    _state       = State::Chasing;
    _stateTimer  = 0.f;
    _meleeCooldown   = 1.2f;
    _contactCooldown = 1.0f;
    _jumpCooldown    = 2.6f;   // first leap comes fairly quickly
    _airborneTimer   = 0.f;
    _landingDamageApplied = false;
    _summonedAt66    = false;
    _summonedAt33    = false;
    _pendingSummonCount   = 0;
    _impactShakeRequested = false;

    _isHopping       = false;
    _hopTimer        = 0.f;
    _hopPauseTimer   = 0.6f;
    _hopDirection    = Vector2{ 1.f, 0.f };
    _hopsSincePuddle = 0;
    _puddles.clear();
    _puddleDamageCooldown = 0.f;

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

int AbyssSlime::GetCurrentAnimSlot() const
{
    if (_texture.id == _sharedIdleAnim.id)  return 0;
    if (_texture.id == _sharedWalkAnim.id)  return 1;
    if (_texture.id == _sharedMeleeAnim.id) return 2;
    if (_texture.id == _sharedMagicAnim.id) return 3;
    if (_texture.id == _sharedJumpAnim.id)  return 4;
    if (_texture.id == _sharedFallAnim.id)  return 5;
    if (_texture.id == _sharedHurtAnim.id)  return 6;
    if (_texture.id == _sharedDeathAnim.id) return 7;
    return 0;
}

const char* AbyssSlime::GetEditorAnimName(int index) const
{
    static const char* kAnimNames[8] = { "Idle", "Hop", "Melee", "Magic", "Jump", "Fall", "Hurt", "Death" };
    return (index >= 0 && index < 8) ? kAnimNames[index] : "";
}

void AbyssSlime::PlayEditorAnim(int index)
{
    const Texture2D* sheets[8] = {
        &_sharedIdleAnim, &_sharedWalkAnim, &_sharedMeleeAnim, &_sharedMagicAnim,
        &_sharedJumpAnim, &_sharedFallAnim, &_sharedHurtAnim,  &_sharedDeathAnim
    };
    if (index < 0 || index > 7)
        return;

    float frameTimeOverride = _editorAnimFrameTimes[index];
    SetAnimation(*sheets[index], (frameTimeOverride > 0.f) ? frameTimeOverride : 1.f / 8.f, true);
}

void AbyssSlime::SetAnimation(const Texture2D& sheet, float frameTime, bool resetFrame)
{
    _texture    = sheet;
    _width      = (float)sheet.width / (float)_sheetFrameCount;
    _height     = (float)sheet.height;
    _updateTime = frameTime;
    _maxFrames  = _sheetFrameCount;
    if (resetFrame) { _frame = 0; _runningTime = 0.f; }
}

// =============================================================================
void AbyssSlime::Update(float dt, Vector2 heroWorldPos, Vector2 /*navigationTarget*/, bool /*hasNavigationTarget*/,
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
    if (_jumpCooldown > 0.f && _state == State::Chasing)
        _jumpCooldown -= dt;

    // Acid puddles keep ticking through every state, including boss death,
    // so leftover pools stay dangerous until they evaporate.
    UpdatePuddles(dt);

    if (!_dying && _target != nullptr)
    {
        // HP-threshold summons interrupt everything except an active leap.
        bool canStartSummon = (_state == State::Chasing || _state == State::MeleeAttacking);
        if (canStartSummon && !_summonedAt66 && _health <= _maxHealth * 0.66f)
        {
            _summonedAt66 = true;
            _state = State::Summoning;
            _stateTimer = 0.f;
            SetAnimation(_sharedMagicAnim, _summonDuration / (float)_sheetFrameCount, true);
        }
        else if (canStartSummon && !_summonedAt33 && _health <= _maxHealth * 0.33f)
        {
            _summonedAt33 = true;
            _state = State::Summoning;
            _stateTimer = 0.f;
            SetAnimation(_sharedMagicAnim, _summonDuration / (float)_sheetFrameCount, true);
        }

        bool controlled = IsFrozen() || IsElectroStunned();

        switch (_state)
        {
        case State::Chasing:        if (!controlled) HandleChasing(dt, heroWorldPos); break;
        case State::MeleeAttacking: HandleMelee();            break;
        case State::JumpCharging:   HandleJumpCharge(dt);     break;
        case State::Airborne:       HandleAirborne(dt);       break;
        case State::Landing:        HandleLanding(dt);        break;
        case State::Summoning:      HandleSummoning(dt);      break;
        case State::Recovery:       HandleRecovery(dt);       break;
        }

        if (_state != State::Airborne)
            TryDealContactDamage();

        // Keep the boss inside the one-screen arena at all times.
        _worldPos.x = std::clamp(_worldPos.x, 190.f, (float)kVirtualWidth  - 190.f);
        _worldPos.y = std::clamp(_worldPos.y, 190.f, (float)kVirtualHeight - 190.f);
    }

    HandleAnimation(dt);
}

// =============================================================================
// Hop locomotion — the slime never walks. Short bouncy hops close the gap;
// the grounded pauses between hops are the player's punish windows.
// =============================================================================
void AbyssSlime::HandleChasing(float dt, Vector2 heroWorldPos)
{
    Vector2 toPlayer = Vector2Subtract(heroWorldPos, _worldPos);
    float dist = Vector2Length(toPlayer);

    if (toPlayer.x < -20.f) _rightLeft = -1.f;
    if (toPlayer.x >  20.f) _rightLeft =  1.f;

    if (_isHopping)
    {
        _hopTimer += dt;
        float hopSpeed = IsEnraged() ? _hopSpeed * 1.3f : _hopSpeed;
        _worldPos = Vector2Add(_worldPos, Vector2Scale(_hopDirection, hopSpeed * dt));

        if (_hopTimer >= _hopDuration)
        {
            // Touch down — every second landing leaves an acid puddle.
            _isHopping     = false;
            _hopPauseTimer = IsEnraged() ? _hopPauseBase * 0.6f : _hopPauseBase;
            _hopsSincePuddle++;
            if (_hopsSincePuddle >= 2)
            {
                _hopsSincePuddle = 0;
                SpawnPuddle(_worldPos, 85.f);
            }
            SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
        }
        return;   // committed to the hop — no attacks mid-bounce
    }

    // ── Grounded: decide the next action ────────────────────────────────────
    // Crushing Leap whenever it's off cooldown and the player isn't point blank.
    if (_jumpCooldown <= 0.f && dist > 240.f)
    {
        BeginJump();
        return;
    }

    // Melee slam when close.
    if (dist < _meleeRange && _meleeCooldown <= 0.f)
    {
        _state = State::MeleeAttacking;
        _damageApplied = false;
        SetAnimation(_sharedMeleeAnim, 1.f / 10.f, true);
        PlaySound(_attackSound);
        return;
    }

    // Otherwise wait out the grounded pause, then hop toward the player with a
    // little angle jitter so the approach path wobbles.
    _hopPauseTimer -= dt;
    if (_hopPauseTimer <= 0.f && dist > 120.f)
    {
        float jitter = (float)GetRandomValue(-35, 35) / 100.f;   // ±0.35 rad
        float baseAngle = atan2f(toPlayer.y, toPlayer.x) + jitter;
        _hopDirection = Vector2{ cosf(baseAngle), sinf(baseAngle) };
        _isHopping = true;
        _hopTimer  = 0.f;
        SetAnimation(_sharedWalkAnim, _hopDuration / (float)_sheetFrameCount, true);
    }
}

void AbyssSlime::HandleMelee()
{
    _velocity = Vector2Zero();

    // Swing becomes live on frame 3 for a readable telegraph.
    if (!_damageApplied && _frame >= 3 && _target != nullptr)
    {
        // Per-animation melee box (Character Animator, slot 2) wins.
        Rectangle attackRec;
        if (!GetAnimMeleeRectWorld(2, attackRec))
        {
            attackRec = GetBodyContactRec();
            attackRec.x -= 40.f; attackRec.y -= 30.f;
            attackRec.width += 80.f; attackRec.height += 60.f;
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

void AbyssSlime::BeginJump()
{
    _state = State::JumpCharging;
    _stateTimer = 0.f;
    SetAnimation(_sharedJumpAnim, _jumpChargeDuration / (float)_sheetFrameCount, true);
}

void AbyssSlime::HandleJumpCharge(float dt)
{
    _stateTimer += dt;

    // Track the player's live position during the crouch so the landing spot
    // is honest — the telegraph ring in DrawEnemy shows exactly where.
    if (_target != nullptr)
        _jumpTargetPos = _target->GetFeetWorldPos();

    if (_stateTimer >= _jumpChargeDuration)
    {
        _state = State::Airborne;
        _jumpStartPos  = _worldPos;
        _airborneTimer = 0.f;
        _airborneDuration = IsEnraged() ? 0.55f : 0.7f;
        _landingDamageApplied = false;
        SetAnimation(_sharedFallAnim, _airborneDuration / (float)_sheetFrameCount, true);
    }
}

void AbyssSlime::HandleAirborne(float dt)
{
    _airborneTimer += dt;
    float t = std::min(1.f, _airborneTimer / _airborneDuration);

    // Straight lerp across the floor — the arc illusion comes from the draw
    // offset in DrawEnemy lifting the sprite by a parabola.
    _worldPos = Vector2Lerp(_jumpStartPos, _jumpTargetPos, t);

    if (t >= 1.f)
    {
        _state = State::Landing;
        _stateTimer = 0.f;
        _impactShakeRequested = true;
        // The crushing leap always leaves a large acid pool at the crater.
        SpawnPuddle(_worldPos, 130.f);
        PlaySound(_sharedLandSound);
        SetAnimation(_sharedMeleeAnim, 1.f / 14.f, true);
    }
}

// =============================================================================
// Acid puddles — the slime's area-denial signature. Standing in one chips the
// player's health on a slow tick until it evaporates.
// =============================================================================
void AbyssSlime::SpawnPuddle(Vector2 pos, float radius)
{
    // Cap the pool count so long fights don't flood the arena completely;
    // the oldest puddle is recycled first.
    if ((int)_puddles.size() >= _maxPuddles)
        _puddles.erase(_puddles.begin());

    SlimePuddle puddle;
    puddle.pos    = pos;
    puddle.timer  = _puddleLifetime;
    puddle.radius = radius;
    _puddles.push_back(puddle);
}

void AbyssSlime::UpdatePuddles(float dt)
{
    if (_puddleDamageCooldown > 0.f)
        _puddleDamageCooldown -= dt;

    for (int i = (int)_puddles.size() - 1; i >= 0; --i)
    {
        _puddles[i].timer -= dt;
        if (_puddles[i].timer <= 0.f)
        {
            _puddles.erase(_puddles.begin() + i);
            continue;
        }

        if (_target != nullptr && _target->IsAlive() && _puddleDamageCooldown <= 0.f)
        {
            float distToPlayer = Vector2Distance(_puddles[i].pos, _target->GetFeetWorldPos());
            if (distToPlayer < _puddles[i].radius)
            {
                _target->TakeFractionalDamage(_puddleTickDamage, _puddles[i].pos);
                _puddleDamageCooldown = _puddleTickInterval;
            }
        }
    }
}

void AbyssSlime::DrawPuddles(Vector2 cameraRef) const
{
    for (const SlimePuddle& puddle : _puddles)
    {
        Vector2 screenPos = Vector2Subtract(puddle.pos, cameraRef);
        screenPos.x += kVirtualWidth  / 2.f;
        screenPos.y += kVirtualHeight / 2.f;

        // Fade out over the last 1.5 seconds of life.
        float alpha = (puddle.timer < 1.5f) ? (puddle.timer / 1.5f) : 1.f;
        float wobble = sinf((float)GetTime() * 3.f + puddle.pos.x * 0.01f) * 4.f;

        DrawEllipse((int)screenPos.x, (int)screenPos.y,
            puddle.radius + wobble, (puddle.radius + wobble) * 0.55f,
            Fade(Color{ 100, 50, 190, 255 }, 0.30f * alpha));
        DrawEllipse((int)screenPos.x, (int)screenPos.y,
            (puddle.radius + wobble) * 0.72f, (puddle.radius + wobble) * 0.40f,
            Fade(Color{ 165, 110, 245, 255 }, 0.28f * alpha));
    }
}

void AbyssSlime::HandleLanding(float dt)
{
    _stateTimer += dt;

    if (!_landingDamageApplied && _target != nullptr)
    {
        float distToPlayer = Vector2Distance(_worldPos, _target->GetFeetWorldPos());
        if (distToPlayer < _landingRadius)
        {
            _target->TakeFractionalDamage(1.0f, _worldPos);
            _target->StartForcedPush(GetPushDirectionToPlayer(), _bossPushSpeed);
        }
        _landingDamageApplied = true;
    }

    if (_stateTimer >= 0.4f)
    {
        _state = State::Recovery;
        _stateTimer = 0.f;
        _jumpCooldown = IsEnraged() ? _jumpCooldownBase * 0.6f : _jumpCooldownBase;
        SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
    }
}

void AbyssSlime::HandleSummoning(float dt)
{
    _velocity = Vector2Zero();
    _stateTimer += dt;

    if (_stateTimer >= _summonDuration)
    {
        // CombatDirector reads this request and spawns the small slimes.
        _pendingSummonCount = IsEnraged() ? 4 : 3;
        _state = State::Recovery;
        _stateTimer = 0.f;
        SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
    }
}

void AbyssSlime::HandleRecovery(float dt)
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
void AbyssSlime::TryDealContactDamage()
{
    if (_target == nullptr || !_target->IsAlive())
        return;
    if (_target->IsBeingForcedPushed())
        return;
    if (_state == State::MeleeAttacking || _state == State::Landing)
        return;
    if (_contactCooldown > 0.f)
        return;

    if (!CheckCollisionRecs(GetBodyContactRec(), _target->GetCollisionRec()))
        return;

    _target->TakeFractionalDamage(_bossDamagePerHit, _worldPos);
    _target->StartForcedPush(GetPushDirectionToPlayer(), _bossPushSpeed);
    _contactCooldown = _contactCooldownBase;
}

Rectangle AbyssSlime::GetBodyContactRec() const
{
    Rectangle bodyRec = GetCollisionRec();
    bodyRec.x += 30.f;
    bodyRec.y += 24.f;
    bodyRec.width  = std::max(1.f, bodyRec.width  - 60.f);
    bodyRec.height = std::max(1.f, bodyRec.height - 48.f);
    return bodyRec;
}

Vector2 AbyssSlime::GetPushDirectionToPlayer() const
{
    if (_target == nullptr)
        return Vector2{ 1.f, 0.f };
    Vector2 away = Vector2Subtract(_target->GetWorldPos(), _worldPos);
    if (Vector2Length(away) <= 0.01f)
        return Vector2{ 1.f, 0.f };
    return Vector2Normalize(away);
}

// =============================================================================
int AbyssSlime::ConsumeSummonRequest()
{
    int count = _pendingSummonCount;
    _pendingSummonCount = 0;
    return count;
}

bool AbyssSlime::ConsumeImpactShakeRequest()
{
    bool requested = _impactShakeRequested;
    _impactShakeRequested = false;
    return requested;
}

// =============================================================================
void AbyssSlime::HandleAnimation(float dt)
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
            _frame = 0;
        }
    }
}

// =============================================================================
void AbyssSlime::DrawEnemy(Vector2 cameraRef)
{
    if (!_isActive)
        return;

    float drawWidth  = _stableFrameW * _scale;
    float drawHeight = _stableFrameH * _scale;

    Vector2 screenPos = Vector2Subtract(_worldPos, cameraRef);
    screenPos.x += kVirtualWidth  / 2.f;
    screenPos.y += kVirtualHeight / 2.f;

    // Acid pools render under everything else.
    DrawPuddles(cameraRef);

    // Landing-spot telegraph while charging / airborne.
    if (_state == State::JumpCharging || _state == State::Airborne)
    {
        Vector2 targetScreen = Vector2Subtract(_jumpTargetPos, cameraRef);
        targetScreen.x += kVirtualWidth  / 2.f;
        targetScreen.y += kVirtualHeight / 2.f;
        float pulse = sinf((float)GetTime() * 12.f) * 0.5f + 0.5f;
        DrawCircleLines((int)targetScreen.x, (int)targetScreen.y, _landingRadius, Fade(Color{ 160, 90, 245, 255 }, 0.5f + 0.3f * pulse));
        DrawCircleV(targetScreen, _landingRadius * (0.35f + 0.25f * pulse), Fade(Color{ 160, 90, 245, 255 }, 0.14f));
    }

    // Parabolic lift while airborne sells the jump without real Z movement.
    // Short hops get a smaller arc of their own.
    float airLift = 0.f;
    if (_state == State::Airborne)
    {
        float t = std::min(1.f, _airborneTimer / _airborneDuration);
        airLift = sinf(t * PI) * 300.f;
    }
    else if (_isHopping && _state == State::Chasing)
    {
        float t = std::min(1.f, _hopTimer / _hopDuration);
        airLift = sinf(t * PI) * 90.f;
    }

    bool burning        = !_pendingBurns.empty();
    bool frozen         = IsFrozen();
    bool electroStunned = IsElectroStunned();

    Color tint = electroStunned ? Color{ 255, 255,  60, 255 } :
                 frozen         ? Color{ 140, 200, 255, 255 } :
                 burning        ? Color{ 255, 180, 180, 255 } :
                 IsEnraged()    ? Color{ 255, 190, 235, 255 } :
                                  WHITE;

    // Shadow stays on the floor while the body lifts.
    DrawEllipse((int)screenPos.x, (int)(screenPos.y + drawHeight * 0.34f),
        drawWidth * 0.34f, drawHeight * 0.12f, Fade(BLACK, 0.35f));

    // Summon cast glow.
    if (_state == State::Summoning)
    {
        float pulse = sinf((float)GetTime() * 14.f) * 0.5f + 0.5f;
        DrawCircleV(screenPos, drawWidth * (0.45f + 0.1f * pulse), Fade(Color{ 90, 60, 220, 255 }, 0.22f));
    }

    Vector2 animDrawOffset = GetCurrentAnimDrawOffset();
    Rectangle source{ _frame * _width, 0.f, _rightLeft * _width, _height };
    Rectangle dest{ screenPos.x - drawWidth / 2.f + animDrawOffset.x,
                    screenPos.y - drawHeight / 2.f - airLift + animDrawOffset.y, drawWidth, drawHeight };
    DrawTexturePro(_texture, source, dest, Vector2{}, 0.f, tint);

    DrawHealthBar(screenPos, drawWidth, drawHeight);
}

Rectangle AbyssSlime::GetCollisionRec() const
{
    Rectangle animBodyRect;
    if (GetAnimBodyRectWorld(animBodyRect))
        return animBodyRect;
    if (_hasTunedCollision)
        return GetTunedCollisionRec();

    float halfW = _stableFrameW * _scale * 0.5f;
    float halfH = _stableFrameH * _scale * 0.5f;
    // The slime body fills most of the frame width but sits low.
    return Rectangle{
        _worldPos.x - halfW * 0.62f,
        _worldPos.y - halfH * 0.40f,
        halfW * 1.24f,
        halfH * 1.15f
    };
}

Capsule2D AbyssSlime::GetCapsule() const
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
        { _worldPos.x, _worldPos.y + 16.f },
        0.f,
        _stableFrameW * _scale * 0.30f
    };
}

void AbyssSlime::DrawHealthBar(Vector2 screenPos, float w, float h)
{
    if (_health <= 0.f)
        return;

    float healthPercent = _health / _maxHealth;
    float barWidth      = w * 0.8f;
    float barHeight     = 8.f;
    float barX          = screenPos.x - barWidth / 2.f;
    float barY          = screenPos.y - h * 0.62f - 14.f;

    DrawRectangle((int)barX, (int)barY, (int)barWidth, (int)barHeight, RED);
    DrawRectangle((int)barX, (int)barY, (int)(barWidth * healthPercent), (int)barHeight, GREEN);
}

// =============================================================================
void AbyssSlime::TakeDamage(int damage, Vector2 attackerPos)
{
    (void)attackerPos;
    if (_dying)
        return;
    if (_hitTimer > 0.f)
        return;

    // Airborne body can't be hit — dodge the landing instead.
    if (_state == State::Airborne)
        return;

    _health -= (float)damage;

    if (_health > 0.f)
        PlayHurtSound();

    if (_health <= 0.f)
    {
        _health = 0.f;
        _dying = true;
        _takingDamage = false;
        _attacking = false;
        _pendingSummonCount = 0;
        _state = State::Recovery;
        _deathTimer = 0.7f;
        SetAnimation(_sharedDeathAnim, 1.f / 6.f, true);
        PlayDeathSound();
        return;
    }

    _velocity = Vector2Zero();
    // Only flinch visually while walking — attacks and casts play through.
    if (_state == State::Chasing)
    {
        _takingDamage = true;
        SetAnimation(_sharedHurtAnim, 1.f / 12.f, true);
    }
    _hitTimer = 0.02f;
}

void AbyssSlime::ApplyFreeze(float duration)
{
    if (_dying || !IsAlive())
        return;
    // Leaps can't be frozen mid-air; everything else can.
    if (_state == State::Airborne)
        return;
    if (duration > _freezeTimer)
        _freezeTimer = duration;
}

// =============================================================================
void AbyssSlime::SetWaveScale(int wave)
{
    (void)wave;
    _expValue    = _bossBaseExpValue;
    _health      = 60.f;
    _maxHealth   = 60.f;
    _speed       = _moveSpeed;
    _attackPower = 1.f;
}

// =============================================================================
void AbyssSlime::EnsureSharedResourcesLoaded()
{
    if (_sharedResourcesLoaded)
        return;

    _sharedIdleAnim  = LoadTexture(AssetPath("Bosses/AbyssSlimeIdle.png").c_str());
    _sharedWalkAnim  = LoadTexture(AssetPath("Bosses/AbyssSlimeWalk.png").c_str());
    _sharedMeleeAnim = LoadTexture(AssetPath("Bosses/AbyssSlimeMeleeAttack.png").c_str());
    _sharedMagicAnim = LoadTexture(AssetPath("Bosses/AbyssSlimeMagicAttack.png").c_str());
    _sharedJumpAnim  = LoadTexture(AssetPath("Bosses/AbyssSlimeJump.png").c_str());
    _sharedFallAnim  = LoadTexture(AssetPath("Bosses/AbyssSlimeFall.png").c_str());
    _sharedHurtAnim  = LoadTexture(AssetPath("Bosses/AbyssSlimeHurt.png").c_str());
    _sharedDeathAnim = LoadTexture(AssetPath("Bosses/AbyssSlimeDeath.png").c_str());
    _sharedAttackSound = LoadSound(AssetPath("Sounds/SwordSwipe2.ogg").c_str());
    _sharedHurtSound   = LoadSound(AssetPath("Sounds/SmallMonsterDamage.ogg").c_str());
    _sharedDeathSound  = LoadSound(AssetPath("Sounds/MonsterDeath.ogg").c_str());
    _sharedLandSound   = LoadSound(AssetPath("Sounds/OgreHitWall.ogg").c_str());
    _sharedResourcesLoaded = true;
}

void AbyssSlime::UnloadSharedResources()
{
    if (!_sharedResourcesLoaded)
        return;

    UnloadTexture(_sharedIdleAnim);
    UnloadTexture(_sharedWalkAnim);
    UnloadTexture(_sharedMeleeAnim);
    UnloadTexture(_sharedMagicAnim);
    UnloadTexture(_sharedJumpAnim);
    UnloadTexture(_sharedFallAnim);
    UnloadTexture(_sharedHurtAnim);
    UnloadTexture(_sharedDeathAnim);
    UnloadSound(_sharedAttackSound);
    UnloadSound(_sharedHurtSound);
    UnloadSound(_sharedDeathSound);
    UnloadSound(_sharedLandSound);

    _sharedIdleAnim  = Texture2D{};
    _sharedWalkAnim  = Texture2D{};
    _sharedMeleeAnim = Texture2D{};
    _sharedMagicAnim = Texture2D{};
    _sharedJumpAnim  = Texture2D{};
    _sharedFallAnim  = Texture2D{};
    _sharedHurtAnim  = Texture2D{};
    _sharedDeathAnim = Texture2D{};
    _sharedAttackSound = Sound{};
    _sharedHurtSound   = Sound{};
    _sharedDeathSound  = Sound{};
    _sharedLandSound   = Sound{};
    _sharedResourcesLoaded = false;
}
