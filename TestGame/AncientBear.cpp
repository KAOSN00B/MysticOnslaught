#include "AncientBear.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "Character.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

Texture2D AncientBear::_sharedIdleAnim{};
Texture2D AncientBear::_sharedWalkAnim{};
Texture2D AncientBear::_sharedMeleeAnim{};
Texture2D AncientBear::_sharedRoarAnim{};
Texture2D AncientBear::_sharedHurtAnim{};
Texture2D AncientBear::_sharedDeathAnim{};
Sound     AncientBear::_sharedAttackSound{};
Sound     AncientBear::_sharedHurtSound{};
Sound     AncientBear::_sharedDeathSound{};
Sound     AncientBear::_sharedRoarSound{};
bool      AncientBear::_sharedResourcesLoaded = false;

AncientBear::AncientBear(Vector2 pos) : Enemy(pos) {}
AncientBear::~AncientBear() {}

void AncientBear::Init()
{
    EnsureSharedResourcesLoaded();
    _healthBarHeight  = 8.f;
    _healthBarYFrac   = 0.62f;
    _healthBarYOffset = 14.f;
    _attackSound = _sharedAttackSound;
    _hurtSound   = _sharedHurtSound;
    _deathSound  = _sharedDeathSound;
    ResetForSpawn(_worldPos);
}

void AncientBear::ResetForSpawn(Vector2 pos)
{
    _worldPos = pos; _worldPosLastFrame = pos; _homePos = pos;
    _velocity = Vector2Zero();
    _isActive = true;

    _stableFrameW = (float)_sharedIdleAnim.width / (float)_sheetFrameCount;
    _stableFrameH = (float)_sharedIdleAnim.height;

    SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
    _scale = _bossScale;

    _health = 75.f; _maxHealth = 75.f;
    _attackPower = 1.f;
    _speed = _moveSpeed;
    _expValue = _bossBaseExpValue;

    _hitTimer = 0.f; _deathTimer = 0.7f; _freezeTimer = 0.f;
    _isCharged = false; _chargeNextStunTime = 0.f; _electricChargeTotalTimer = 0.f;
    _isEliteMiniboss = false; _isInvulnerable = false; _leapInvulnerable = false;
    _takingDamage = false; _attacking = false; _dying = false;

    _state = State::Lumbering;
    _stateTimer = 0.f;
    _meleeCooldown = 1.4f;
    _contactCooldown = 1.0f;
    _roarCooldown = 4.5f;
    _pullTickTimer = 0.f;
    _slamDamageApplied = false;
    _slamChainRemaining = 0;
    _pendingPhaseRoar = false;
    _impactShakeRequested = false;
    _slamVariant = 0;
    _slamIndexInChain = 0;
    _slamWedgeAngle = 0.f;
    ClearEliteEvents();

    // 3 phases: 66% makes the pull chain into a DOUBLE slam; 33% (runes ignite) a
    // TRIPLE slam with the faster/longer pull. Each phase opens with a Dream Pull.
    SetPhaseThresholds({ 0.66f, 0.33f });

    _forcedPushActive = false; _forcedPushDirection = Vector2Zero(); _forcedPushSpeed = 0.f;
    _pendingBurns.clear();
    _waypoints.clear(); _waypointIndex = 0;

    ResetTuningState();
    ApplyStoredTuning();
}

void AncientBear::SetAnimation(const Texture2D& sheet, float frameTime, bool resetFrame)
{
    SetSpriteSheet(sheet, _sheetFrameCount, frameTime, resetFrame);
}

int AncientBear::GetCurrentAnimSlot() const
{
    if (_texture.id == _sharedIdleAnim.id)  return 0;
    if (_texture.id == _sharedWalkAnim.id)  return 1;
    if (_texture.id == _sharedMeleeAnim.id) return 2;
    if (_texture.id == _sharedRoarAnim.id)  return 3;
    if (_texture.id == _sharedHurtAnim.id)  return 4;
    if (_texture.id == _sharedDeathAnim.id) return 5;
    return 0;
}

const char* AncientBear::GetEditorAnimName(int index) const
{
    static const char* kNames[6] = { "Idle", "Walk", "Slam", "Roar", "Hurt", "Death" };
    return (index >= 0 && index < 6) ? kNames[index] : "";
}

void AncientBear::PlayEditorAnim(int index)
{
    const Texture2D* sheets[6] = {
        &_sharedIdleAnim, &_sharedWalkAnim, &_sharedMeleeAnim,
        &_sharedRoarAnim, &_sharedHurtAnim, &_sharedDeathAnim
    };
    if (index < 0 || index > 5) return;
    float frameTimeOverride = _editorAnimFrameTimes[index];
    SetAnimation(*sheets[index], (frameTimeOverride > 0.f) ? frameTimeOverride : 1.f / 8.f, true);
}

void AncientBear::Update(float dt, Vector2 heroWorldPos, Vector2, bool,
    const std::vector<std::unique_ptr<Enemy>>&, const std::vector<Vector2>&)
{
    if (!_isActive) return;
    _worldPosLastFrame = _worldPos;

    UpdateHit(dt);
    UpdateBurns(dt);
    UpdateElectricCharge(dt);

    if (_hitTimer > 0.f)
        _hitTimer -= dt;

    if (_freezeTimer > 0.f)     _freezeTimer -= dt;
    if (_meleeCooldown > 0.f)   _meleeCooldown -= dt;
    if (_contactCooldown > 0.f) _contactCooldown -= dt;
    if (_state == State::Lumbering && _roarCooldown > 0.f) _roarCooldown -= dt;

    if (!_dying && _target != nullptr)
    {
        bool controlled = IsFrozen() || IsElectroStunned();
        Vector2 toPlayer = Vector2Subtract(heroWorldPos, _worldPos);
        float dist = Vector2Length(toPlayer);

        // Each phase change opens with a Dream Pull -> (multi) slam, fired when neutral.
        int newPhase = ConsumePhaseChange();
        if (newPhase >= 0)
        {
            _pendingPhaseRoar = true; _impactShakeRequested = true;
            RequestBossCallout(newPhase >= 2 ? "DREAM COLLAPSE" : "NIGHTMARE");
        }
        if (_pendingPhaseRoar && !controlled &&
            (_state == State::Lumbering || _state == State::Recovery))
        {
            _pendingPhaseRoar = false;
            _state = State::Roaring; _stateTimer = 0.f; _pullTickTimer = 0.f;
            SetAnimation(_sharedRoarAnim, _roarDuration / (float)_sheetFrameCount, true);
            PlaySound(_sharedRoarSound);
        }

        switch (_state)
        {
        case State::Lumbering:
        {
            if (controlled) break;

            if (toPlayer.x < -20.f) _rightLeft = -1.f;
            if (toPlayer.x >  20.f) _rightLeft =  1.f;

            float pullRange = IsRuneIgnited() ? _pullRange * 1.2f : _pullRange;
            if (_roarCooldown <= 0.f && dist > 280.f && dist < pullRange)
            {
                _state = State::Roaring; _stateTimer = 0.f;
                _pullTickTimer = 0.f;
                SetAnimation(_sharedRoarAnim, _roarDuration / (float)_sheetFrameCount, true);
                PlaySound(_sharedRoarSound);
                break;
            }
            if (dist < _meleeRange && _meleeCooldown <= 0.f)
            {
                _state = State::MeleeAttacking;
                _damageApplied = false;
                SetAnimation(_sharedMeleeAnim, 1.f / 10.f, true);
                PlaySound(_attackSound);
                break;
            }

            Vector2 dir = (dist > 0.01f) ? Vector2Scale(toPlayer, 1.f / dist) : Vector2{ 1.f, 0.f };
            _worldPos = Vector2Add(_worldPos, Vector2Scale(dir, _moveSpeed * dt));
            if (_texture.id != _sharedWalkAnim.id && !_takingDamage)
                SetAnimation(_sharedWalkAnim, 1.f / 8.f, true);
            break;
        }

        case State::MeleeAttacking:
            _velocity = Vector2Zero();
            if (!_damageApplied && _frame >= 3)
            {
                Rectangle attackRec;
                if (!GetAnimMeleeRectWorld(2, attackRec))
                {
                    attackRec = GetBodyContactRec();
                    attackRec.width += 130.f;
                    if (_rightLeft < 0.f) attackRec.x -= 130.f;
                    attackRec.y -= 30.f; attackRec.height += 60.f;
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
                _state = State::Recovery; _stateTimer = 0.f;
                _meleeCooldown = _meleeCooldownBase;
                SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
            }
            break;

        case State::Roaring:
        {
            _velocity = Vector2Zero();
            _stateTimer += dt;

            // Dream Pull: repeated short pushes TOWARD the bear. Dashing away
            // between ticks escapes the drag.
            _pullTickTimer -= dt;
            if (_pullTickTimer <= 0.f && _target->IsAlive() &&
                !_target->IsBeingForcedPushed() && dist > 220.f)
            {
                _pullTickTimer = 0.16f;
                Vector2 towardBear = Vector2Subtract(_worldPos, _target->GetWorldPos());
                if (Vector2Length(towardBear) > 0.01f)
                    _target->StartForcedPush(Vector2Normalize(towardBear), _pullStrength);
            }

            float duration = IsRuneIgnited() ? _roarDuration * 0.8f : _roarDuration;
            if (_stateTimer >= duration)
            {
                // The pull always chains into the slam. Phase scales how many
                // slams follow AND what shape each one takes (Dream Collapse):
                //   phase 0: one disc (safe = outside the circle)
                //   phase 1: disc, then a RING (safe = hug him or stand far)
                //   phase 2: three rotating 120-degree WEDGES in a fixed order
                _slamChainRemaining = (GetPhase() >= 2) ? 2 : (GetPhase() >= 1 ? 1 : 0);
                _slamIndexInChain = 0;
                if (GetPhase() >= 2)
                {
                    _slamVariant = 2;
                    Vector2 toPlayerNow = Vector2Subtract(_target->GetFeetWorldPos(), _worldPos);
                    _slamWedgeAngle = (Vector2Length(toPlayerNow) > 0.01f)
                        ? atan2f(toPlayerNow.y, toPlayerNow.x) : 0.f;
                }
                else
                {
                    _slamVariant = 0;
                }
                _state = State::SlamWindup; _stateTimer = 0.f;
                SetAnimation(_sharedMeleeAnim, _slamWindupDuration / (float)_sheetFrameCount, true);
            }
            break;
        }

        case State::SlamWindup:
            _velocity = Vector2Zero();
            _stateTimer += dt;
            if (_stateTimer >= _slamWindupDuration)
            {
                _state = State::Slamming; _stateTimer = 0.f;
                _slamDamageApplied = false;
                _impactShakeRequested = true;
                PlaySound(_attackSound);
            }
            break;

        case State::Slamming:
            _velocity = Vector2Zero();
            _stateTimer += dt;
            if (!_slamDamageApplied && _stateTimer > 0.05f)
            {
                _slamDamageApplied = true;
                // Hit test matches the warned SHAPE exactly.
                const Vector2 playerFeet = _target->GetFeetWorldPos();
                const float playerDistance = Vector2Distance(_worldPos, playerFeet);
                bool playerInsideShape = false;
                if (_slamVariant == 0)          // disc: safe outside
                {
                    playerInsideShape = playerDistance < _slamRadius;
                }
                else if (_slamVariant == 1)     // ring: safe in close OR far out
                {
                    playerInsideShape = playerDistance >= _slamRingInnerRadius &&
                                        playerDistance <= _slamRingOuterRadius;
                }
                else                            // wedge: safe outside the sector
                {
                    Vector2 toPlayerNow = Vector2Subtract(playerFeet, _worldPos);
                    const float playerAngle = atan2f(toPlayerNow.y, toPlayerNow.x);
                    float angleDifference = fmodf(playerAngle - _slamWedgeAngle + 3.f * PI,
                                                  2.f * PI) - PI;
                    playerInsideShape = playerDistance < _slamWedgeRadius &&
                                        fabsf(angleDifference) < _slamWedgeHalfAngle;
                }
                if (playerInsideShape)
                {
                    _target->TakeFractionalDamage(1.5f, _worldPos);
                    _target->StartForcedPush(GetPushDirectionToPlayer(), _bossPushSpeed);
                }
            }
            if (_stateTimer >= 0.4f)
            {
                if (_slamChainRemaining > 0)
                {
                    // Multi-slam: wind up and crash down again before recovering.
                    // The next slam's SHAPE advances the learnable sequence.
                    _slamChainRemaining--;
                    _slamIndexInChain++;
                    if (GetPhase() >= 2)
                        _slamWedgeAngle += 2.094f;   // fixed 120-degree rotation
                    else
                        _slamVariant = 1;            // second slam is the ring
                    _state = State::SlamWindup; _stateTimer = 0.f;
                    _slamDamageApplied = false;
                    _impactShakeRequested = true;
                    SetAnimation(_sharedMeleeAnim, _slamWindupDuration / (float)_sheetFrameCount, true);
                }
                else
                {
                    _state = State::Recovery; _stateTimer = 0.f;
                    _roarCooldown = IsRuneIgnited() ? _roarCooldownBase * 0.6f : _roarCooldownBase;
                    SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
                }
            }
            break;

        case State::Recovery:
            _stateTimer += dt;
            if (_stateTimer >= _recoveryDuration)
            {
                _state = State::Lumbering; _stateTimer = 0.f;
            }
            break;
        }

        if (_state != State::MeleeAttacking && _state != State::Slamming)
            TryDealContactDamage();

        _worldPos.x = std::clamp(_worldPos.x, 200.f, (float)kVirtualWidth  - 200.f);
        _worldPos.y = std::clamp(_worldPos.y, 200.f, (float)kVirtualHeight - 200.f);
    }

    HandleAnimation(dt);
}

void AncientBear::DrawEliteTelegraph() const
{
    // The current slam's danger SHAPE draws through the pull and the windup,
    // so the dragged player always knows where the safe region is before the
    // paw comes down.
    const bool showSlamWarning = (_state == State::Roaring ||
                                  _state == State::SlamWindup);
    if (!showSlamWarning)
        return;

    const Color dangerFill = Fade(Color{ 190, 110, 255, 255 }, 0.16f);
    const Color dangerEdge = Fade(Color{ 220, 160, 255, 255 }, 0.6f);

    if (_state == State::Roaring)
    {
        // Pull reach indicator: a faint outer circle showing the drag range.
        const float pullRange = IsRuneIgnited() ? _pullRange * 1.2f : _pullRange;
        DrawCircleLines((int)_worldPos.x, (int)_worldPos.y, pullRange,
                        Fade(Color{ 190, 110, 255, 255 }, 0.30f));
    }

    if (_slamVariant == 0)          // disc: everything inside is danger
    {
        DrawCircleV(_worldPos, _slamRadius, dangerFill);
        DrawCircleLines((int)_worldPos.x, (int)_worldPos.y, _slamRadius, dangerEdge);
    }
    else if (_slamVariant == 1)     // ring: safe hugging him or standing far
    {
        DrawRing(_worldPos, _slamRingInnerRadius, _slamRingOuterRadius,
                 0.f, 360.f, 36, dangerFill);
        DrawCircleLines((int)_worldPos.x, (int)_worldPos.y, _slamRingInnerRadius, dangerEdge);
        DrawCircleLines((int)_worldPos.x, (int)_worldPos.y, _slamRingOuterRadius, dangerEdge);
    }
    else                            // wedge: a 120-degree sector, rotating per slam
    {
        const float wedgeStartDegrees = (_slamWedgeAngle - _slamWedgeHalfAngle) * RAD2DEG;
        const float wedgeEndDegrees   = (_slamWedgeAngle + _slamWedgeHalfAngle) * RAD2DEG;
        DrawCircleSector(_worldPos, _slamWedgeRadius, wedgeStartDegrees, wedgeEndDegrees,
                         18, dangerFill);
        DrawCircleSectorLines(_worldPos, _slamWedgeRadius, wedgeStartDegrees, wedgeEndDegrees,
                              18, dangerEdge);
    }
}

void AncientBear::DebugForceEliteSignature()
{
    if (_dying || !IsAlive() || _state != State::Lumbering)
        return;
    _roarCooldown = 0.f;   // the signature IS the pull-into-slam sequence
}

void AncientBear::DebugForceElitePhaseTwo()
{
    const float nextThreshold = (GetPhase() == 0) ? 0.65f : 0.32f;
    _health = std::min(_health, std::max(1.f, std::floor(_maxHealth * nextThreshold)));
}

const char* AncientBear::GetEliteSignatureStateName() const
{
    switch (_state)
    {
    case State::Roaring:    return "DreamPull";
    case State::SlamWindup: return (_slamVariant == 2) ? "WedgeWindup"
                                 : (_slamVariant == 1) ? "RingWindup" : "SlamWindup";
    case State::Slamming:   return "Slamming";
    default:                return "Lumbering";
    }
}

void AncientBear::TryDealContactDamage()
{
    if (_target == nullptr || !_target->IsAlive()) return;
    if (_target->IsBeingForcedPushed()) return;
    if (_contactCooldown > 0.f) return;
    if (!CheckCollisionRecs(GetBodyContactRec(), _target->GetCollisionRec())) return;

    _target->TakeFractionalDamage(0.5f, _worldPos);
    _target->StartForcedPush(GetPushDirectionToPlayer(), _bossPushSpeed);
    _contactCooldown = _contactCooldownBase;
}

Rectangle AncientBear::GetBodyContactRec() const
{
    Rectangle bodyRec = GetCollisionRec();
    bodyRec.x += 30.f; bodyRec.y += 24.f;
    bodyRec.width  = std::max(1.f, bodyRec.width  - 60.f);
    bodyRec.height = std::max(1.f, bodyRec.height - 48.f);
    return bodyRec;
}

Vector2 AncientBear::GetPushDirectionToPlayer() const
{
    if (_target == nullptr) return Vector2{ 1.f, 0.f };
    Vector2 away = Vector2Subtract(_target->GetWorldPos(), _worldPos);
    return (Vector2Length(away) > 0.01f) ? Vector2Normalize(away) : Vector2{ 1.f, 0.f };
}

bool AncientBear::ConsumeImpactShakeRequest()
{
    bool requested = _impactShakeRequested;
    _impactShakeRequested = false;
    return requested;
}

void AncientBear::HandleAnimation(float dt)
{
    _runningTime += dt;
    if (_runningTime >= _updateTime)
    {
        _runningTime = 0.f;
        _frame++;
        if (_frame >= _maxFrames)
        {
            if (_dying || IsFrozen()) { _frame = _maxFrames - 1; return; }
            if (_takingDamage)
            {
                _takingDamage = false;
                SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
                return;
            }
            if (_state == State::SlamWindup || _state == State::Slamming)
            {
                _frame = _maxFrames - 1;
                return;
            }
            _frame = 0;
        }
    }
}

void AncientBear::DrawEnemy(Vector2 cameraRef)
{
    if (!_isActive) return;

    float drawWidth  = _stableFrameW * _scale;
    float drawHeight = _stableFrameH * _scale;

    Vector2 screenPos = Vector2Subtract(_worldPos, cameraRef);
    screenPos.x += kVirtualWidth  / 2.f;
    screenPos.y += kVirtualHeight / 2.f;

    DrawEllipse((int)screenPos.x, (int)(screenPos.y + drawHeight * 0.50f),
        drawWidth * 0.32f, drawHeight * 0.10f, Fade(BLACK, 0.35f));

    // Dream Pull: converging rings show the vacuum.
    if (_state == State::Roaring)
    {
        float pullRange = IsRuneIgnited() ? _pullRange * 1.2f : _pullRange;
        for (int i = 0; i < 3; i++)
        {
            float t = 1.f - fmodf(_stateTimer * 1.3f + i * 0.33f, 1.f);
            DrawCircleLines((int)screenPos.x, (int)screenPos.y, pullRange * t,
                Fade(Color{ 150, 220, 130, 255 }, 0.45f * (1.f - t)));
        }
    }
    // Slam telegraph.
    if (_state == State::SlamWindup)
    {
        float pulse = sinf((float)GetTime() * 12.f) * 0.5f + 0.5f;
        DrawCircleLines((int)screenPos.x, (int)screenPos.y, _slamRadius,
            Fade(Color{ 255, 160, 70, 255 }, 0.4f + 0.3f * pulse));
    }
    if (_state == State::Slamming)
    {
        float t = std::min(1.f, _stateTimer / 0.3f);
        DrawCircleLines((int)screenPos.x, (int)screenPos.y, _slamRadius * t, Fade(Color{ 255, 190, 100, 255 }, 0.8f * (1.f - t * 0.5f)));
    }
    // Ignited runes glow.
    if (IsRuneIgnited() && !_dying)
    {
        float pulse = sinf((float)GetTime() * 6.f) * 0.5f + 0.5f;
        DrawCircleV(screenPos, drawWidth * 0.36f, Fade(Color{ 110, 255, 130, 255 }, 0.08f + 0.05f * pulse));
    }

    bool burning = !_pendingBurns.empty();
    Color tint = IsElectroStunned() ? Color{ 255, 255,  60, 255 } :
                 IsFrozen()         ? Color{ 140, 200, 255, 255 } :
                 burning            ? Color{ 255, 180, 180, 255 } :
                                      WHITE;

    Vector2 animDrawOffset = GetCurrentAnimDrawOffset();
    Rectangle source{ _frame * _width, 0.f, _rightLeft * _width, _height };
    Rectangle dest{ screenPos.x - drawWidth / 2.f + animDrawOffset.x,
                    screenPos.y - drawHeight / 2.f + animDrawOffset.y, drawWidth, drawHeight };
    DrawTexturePro(_texture, source, dest, Vector2{}, 0.f, tint);

    DrawHealthBar(screenPos, drawWidth, drawHeight);
}

Rectangle AncientBear::GetCollisionRec() const
{
    Rectangle animBodyRect;
    if (GetAnimBodyRectWorld(animBodyRect))
        return animBodyRect;
    if (_hasTunedCollision)
        return GetTunedCollisionRec();

    float halfW = _stableFrameW * _scale * 0.5f;
    float halfH = _stableFrameH * _scale * 0.5f;
    return Rectangle{ _worldPos.x - halfW * 0.56f, _worldPos.y - halfH * 0.56f, halfW * 1.12f, halfH * 1.20f };
}

Capsule2D AncientBear::GetCapsule() const
{
    Capsule2D animBodyCapsule;
    if (GetAnimBodyCapsuleWorld(animBodyCapsule))
        return animBodyCapsule;
    if (_capsuleRadius > 0.f)
        return Capsule2D{ { _worldPos.x + _capsuleOffset.x, _worldPos.y + _capsuleOffset.y }, _capsuleHalfHeight, _capsuleRadius };
    return Capsule2D{ { _worldPos.x, _worldPos.y + 14.f }, 22.f, _stableFrameW * _scale * 0.26f };
}

void AncientBear::TakeDamage(int damage, Vector2 attackerPos)
{
    (void)attackerPos;
    if (_dying || _hitTimer > 0.f) return;

    _health -= (float)damage;
    if (_health > 0.f) PlayHurtSound();

    if (_health <= 0.f)
    {
        _health = 0.f; _dying = true;
        _takingDamage = false; _attacking = false;
        _state = State::Recovery;
        _deathTimer = 0.7f;
        SetAnimation(_sharedDeathAnim, 1.f / 6.f, true);
        PlayDeathSound();
        return;
    }

    _velocity = Vector2Zero();
    if (_state == State::Lumbering)
    {
        _takingDamage = true;
        SetAnimation(_sharedHurtAnim, 1.f / 12.f, true);
    }
    _hitTimer = 0.02f;
}

void AncientBear::ApplyFreeze(float duration)
{
    if (_dying || !IsAlive()) return;
    if (duration > _freezeTimer) _freezeTimer = duration;
}

void AncientBear::SetWaveScale(int wave)
{
    (void)wave;
    _expValue = _bossBaseExpValue;
    _health = Balance::Boss::kAncientBearHealth; _maxHealth = Balance::Boss::kAncientBearHealth;
    _enrageThreshold = 0.35f;   // runes ignite below this HP fraction
    _speed = _moveSpeed; _attackPower = 1.f;
}

void AncientBear::EnsureSharedResourcesLoaded()
{
    if (_sharedResourcesLoaded) return;

    _sharedIdleAnim  = LoadTexture(AssetPath("Bosses/AncientBearIdle.png").c_str());
    _sharedWalkAnim  = LoadTexture(AssetPath("Bosses/AncientBearWalk.png").c_str());
    _sharedMeleeAnim = LoadTexture(AssetPath("Bosses/AncientBearMeleeAttack.png").c_str());
    _sharedRoarAnim  = LoadTexture(AssetPath("Bosses/AncientBearRoar.png").c_str());
    _sharedHurtAnim  = LoadTexture(AssetPath("Bosses/AncientBearHurt.png").c_str());
    _sharedDeathAnim = LoadTexture(AssetPath("Bosses/AncientBearDeath.png").c_str());
    _sharedAttackSound = LoadSound(AssetPath("Sounds/SwordSwipe2.ogg").c_str());
    _sharedHurtSound   = LoadSound(AssetPath("Sounds/SmallMonsterDamage.ogg").c_str());
    _sharedDeathSound  = LoadSound(AssetPath("Sounds/MonsterDeath.ogg").c_str());
    _sharedRoarSound   = LoadSound(AssetPath("Sounds/OgreChargeHit.ogg").c_str());
    _sharedResourcesLoaded = true;
}

void AncientBear::UnloadSharedResources()
{
    if (!_sharedResourcesLoaded) return;

    UnloadTexture(_sharedIdleAnim);  UnloadTexture(_sharedWalkAnim);
    UnloadTexture(_sharedMeleeAnim); UnloadTexture(_sharedRoarAnim);
    UnloadTexture(_sharedHurtAnim);  UnloadTexture(_sharedDeathAnim);
    UnloadSound(_sharedAttackSound); UnloadSound(_sharedHurtSound);
    UnloadSound(_sharedDeathSound);  UnloadSound(_sharedRoarSound);
    _sharedIdleAnim = Texture2D{};  _sharedWalkAnim = Texture2D{};
    _sharedMeleeAnim = Texture2D{}; _sharedRoarAnim = Texture2D{};
    _sharedHurtAnim = Texture2D{};  _sharedDeathAnim = Texture2D{};
    _sharedAttackSound = Sound{};   _sharedHurtSound = Sound{};
    _sharedDeathSound = Sound{};    _sharedRoarSound = Sound{};
    _sharedResourcesLoaded = false;
}
