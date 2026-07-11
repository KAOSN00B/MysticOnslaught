#include "TitanGuard.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "Character.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

Texture2D TitanGuard::_sharedIdleAnim{};
Texture2D TitanGuard::_sharedWalkAnim{};
Texture2D TitanGuard::_sharedMeleeAnim{};
Texture2D TitanGuard::_sharedBombAnim{};
Texture2D TitanGuard::_sharedDefendAnim{};
Texture2D TitanGuard::_sharedHurtAnim{};
Texture2D TitanGuard::_sharedDeathAnim{};
Sound     TitanGuard::_sharedAttackSound{};
Sound     TitanGuard::_sharedHurtSound{};
Sound     TitanGuard::_sharedDeathSound{};
Sound     TitanGuard::_sharedBlockSound{};
Sound     TitanGuard::_sharedSlamSound{};
bool      TitanGuard::_sharedResourcesLoaded = false;

TitanGuard::TitanGuard(Vector2 pos) : Enemy(pos) {}
TitanGuard::~TitanGuard() {}

void TitanGuard::Init()
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

void TitanGuard::ResetForSpawn(Vector2 pos)
{
    _worldPos = pos; _worldPosLastFrame = pos; _homePos = pos;
    _velocity = Vector2Zero();
    _isActive = true;

    _stableFrameW = (float)_sharedIdleAnim.width / (float)_sheetFrameCount;
    _stableFrameH = (float)_sharedIdleAnim.height;

    SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
    _scale = _bossScale;

    _health = 80.f; _maxHealth = 80.f;   // the tank of the roster
    _attackPower = 1.f;
    _speed = _moveSpeed;
    _expValue = _bossBaseExpValue;

    _hitTimer = 0.f; _deathTimer = 0.7f; _freezeTimer = 0.f;
    _isCharged = false; _chargeNextStunTime = 0.f; _electricChargeTotalTimer = 0.f;
    _isEliteMiniboss = false; _isInvulnerable = false; _leapInvulnerable = false;
    _takingDamage = false; _attacking = false; _dying = false;

    _state = State::Advancing;
    _stateTimer = 0.f;
    _meleeCooldown = 1.4f;
    _contactCooldown = 1.0f;
    _bombCooldown = 2.6f;
    _pendingBomb = false;
    _bombTarget = pos;
    _slamRingRadius = 0.f;
    _slamDamageApplied = false;
    _impactShakeRequested = false;
    _bombsRemaining = 0;
    _pendingSlamQueued = false;
    _shieldDownTimer = 0.f;
    _chargeDir = Vector2{ 1.f, 0.f };
    _chargeTravelled = 0.f;

    // 3 phases: each Bulwark Slam (at 66% and 33%) is a transition that opens a
    // full-damage stagger window. Phase 1 unlocks the Bomb Salvo; phase 2 the
    // Shield Charge. The shield only chips frontal hits while it's actually raised
    // (see _shieldDownTimer) so punishing his attacks from the front is real counterplay.
    SetPhaseThresholds({ 0.66f, 0.33f });

    _forcedPushActive = false; _forcedPushDirection = Vector2Zero(); _forcedPushSpeed = 0.f;
    _pendingBurns.clear();
    _waypoints.clear(); _waypointIndex = 0;

    ResetTuningState();
    ApplyStoredTuning();
}

void TitanGuard::SetAnimation(const Texture2D& sheet, float frameTime, bool resetFrame)
{
    SetSpriteSheet(sheet, _sheetFrameCount, frameTime, resetFrame);
}

int TitanGuard::GetCurrentAnimSlot() const
{
    if (_texture.id == _sharedIdleAnim.id)   return 0;
    if (_texture.id == _sharedWalkAnim.id)   return 1;
    if (_texture.id == _sharedMeleeAnim.id)  return 2;
    if (_texture.id == _sharedBombAnim.id)   return 3;
    if (_texture.id == _sharedDefendAnim.id) return 4;
    if (_texture.id == _sharedHurtAnim.id)   return 5;
    if (_texture.id == _sharedDeathAnim.id)  return 6;
    return 0;
}

const char* TitanGuard::GetEditorAnimName(int index) const
{
    static const char* kNames[7] = { "Idle", "Walk", "Mace", "BombThrow", "Slam", "Hurt", "Death" };
    return (index >= 0 && index < 7) ? kNames[index] : "";
}

void TitanGuard::PlayEditorAnim(int index)
{
    const Texture2D* sheets[7] = {
        &_sharedIdleAnim, &_sharedWalkAnim, &_sharedMeleeAnim, &_sharedBombAnim,
        &_sharedDefendAnim, &_sharedHurtAnim, &_sharedDeathAnim
    };
    if (index < 0 || index > 6) return;
    float frameTimeOverride = _editorAnimFrameTimes[index];
    SetAnimation(*sheets[index], (frameTimeOverride > 0.f) ? frameTimeOverride : 1.f / 8.f, true);
}

Vector2 TitanGuard::GetBombSpawnPos() const
{
    return Vector2{ _worldPos.x + _rightLeft * _stableFrameW * _scale * 0.25f,
                    _worldPos.y - _stableFrameH * _scale * 0.15f };
}

void TitanGuard::Update(float dt, Vector2 heroWorldPos, Vector2, bool,
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
    if (_shieldDownTimer > 0.f) _shieldDownTimer -= dt;
    if (_state == State::Advancing && _bombCooldown > 0.f) _bombCooldown -= dt;

    if (!_dying && _target != nullptr)
    {
        bool controlled = IsFrozen() || IsElectroStunned();
        Vector2 toPlayer = Vector2Subtract(heroWorldPos, _worldPos);
        float dist = Vector2Length(toPlayer);

        // Phase transition (66% / 33%): a Bulwark Slam that opens a stagger window.
        // Deferred until he's in a neutral state so it never interrupts mid-attack.
        int newPhase = ConsumePhaseChange();
        if (newPhase >= 0)
            ReactToPhaseChange(newPhase);
        if (_pendingSlamQueued && (_state == State::Advancing || _state == State::Recovery))
        {
            _pendingSlamQueued = false;
            BeginBulwarkSlam();
        }

        switch (_state)
        {
        case State::Advancing:
        {
            if (controlled) break;

            // The shield tracks the player at all times.
            if (toPlayer.x < -14.f) _rightLeft = -1.f;
            if (toPlayer.x >  14.f) _rightLeft =  1.f;

            if (dist < _meleeRange && _meleeCooldown <= 0.f)
            {
                _state = State::MeleeAttacking;
                _damageApplied = false;
                SetAnimation(_sharedMeleeAnim, 1.f / 10.f, true);
                PlaySound(_attackSound);
                break;
            }
            // Last Bastion (phase 2): close big gaps with a shield-leading charge
            // instead of plodding — you have to sidestep, not just outrun him.
            if (GetPhase() >= 2 && _meleeCooldown <= 0.f && dist > 360.f && dist < 1100.f)
            {
                _state = State::ShieldCharge; _stateTimer = 0.f;
                _chargeTravelled = 0.f;
                Vector2 d = (dist > 0.01f) ? Vector2Scale(toPlayer, 1.f / dist) : Vector2{ _rightLeft, 0.f };
                _chargeDir = d;
                _rightLeft = (d.x >= 0.f) ? 1.f : -1.f;   // shield leads the charge
                SetAnimation(_sharedWalkAnim, 1.f / 14.f, true);
                PlaySound(_sharedSlamSound);
                break;
            }
            if (_bombCooldown <= 0.f && dist > 320.f)
            {
                _state = State::BombThrowing; _stateTimer = 0.f;
                _bombsRemaining = (GetPhase() >= 1) ? _bombSalvoCount : 1;   // Bomb Salvo
                SetAnimation(_sharedBombAnim, _bombCastDuration / (float)_sheetFrameCount, true);
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
                    attackRec.width += 120.f;
                    if (_rightLeft < 0.f) attackRec.x -= 120.f;
                    attackRec.y -= 26.f; attackRec.height += 52.f;
                }
                if (CheckCollisionRecs(attackRec, _target->GetCollisionRec()))
                {
                    _target->TakeFractionalDamage(1.0f, _worldPos);   // heaviest melee hit
                    _target->StartForcedPush(GetPushDirectionToPlayer(), _bossPushSpeed);
                }
                _damageApplied = true;
                // Committing to the swing drops the guard — the front is open for a beat.
                _shieldDownTimer = _shieldDownDuration;
            }
            if (_frame >= _maxFrames - 1)
            {
                _state = State::Recovery; _stateTimer = 0.f;
                _meleeCooldown = _meleeCooldownBase;
                SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
            }
            break;

        case State::BombThrowing:
            _velocity = Vector2Zero();
            _stateTimer += dt;
            if (_stateTimer >= _bombCastDuration)
            {
                // Lob one bomb; a salvo scatters the rest around the player's feet.
                _pendingBomb = true;
                Vector2 aim = _target->GetFeetWorldPos();
                if (_bombsRemaining < _bombSalvoCount)   // spread the follow-ups
                    aim.x += (float)GetRandomValue(-220, 220);
                _bombTarget = aim;
                _shieldDownTimer = _shieldDownDuration;   // arm raised to throw -> front open
                _bombsRemaining--;

                if (_bombsRemaining > 0)
                {
                    _stateTimer = _bombCastDuration - _bombSalvoSpacing;   // quick follow-up
                }
                else
                {
                    _state = State::Recovery; _stateTimer = 0.f;
                    _bombCooldown = _bombCooldownBase;
                    SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
                }
            }
            break;

        case State::SlamWindup:
            _velocity = Vector2Zero();
            _stateTimer += dt;
            if (_stateTimer >= _slamWindupDuration)
            {
                _state = State::Slamming; _stateTimer = 0.f;
                _slamRingRadius = 0.f;
                _slamDamageApplied = false;
                _impactShakeRequested = true;
                PlaySound(_sharedSlamSound);
            }
            break;

        case State::Slamming:
            _velocity = Vector2Zero();
            _stateTimer += dt;
            _slamRingRadius = std::min(_slamRadius, (_stateTimer / 0.4f) * _slamRadius);
            if (!_slamDamageApplied)
            {
                float distToPlayer = Vector2Distance(_worldPos, _target->GetFeetWorldPos());
                if (distToPlayer <= _slamRingRadius && distToPlayer <= _slamRadius)
                {
                    _slamDamageApplied = true;
                    _target->TakeFractionalDamage(1.5f, _worldPos);
                    _target->StartForcedPush(GetPushDirectionToPlayer(), _bossPushSpeed);
                }
            }
            if (_stateTimer >= 0.5f)
            {
                // Overextended — briefly staggered and fully vulnerable.
                _state = State::Staggered; _stateTimer = 0.f;
                SetAnimation(_sharedHurtAnim, _staggerDuration / (float)_sheetFrameCount, true);
            }
            break;

        case State::Staggered:
            _velocity = Vector2Zero();
            _stateTimer += dt;
            if (_stateTimer >= _staggerDuration)
            {
                _state = State::Recovery; _stateTimer = 0.f;
                SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
            }
            break;

        case State::ShieldCharge:
        {
            _stateTimer += dt;
            float step = _chargeSpeed * dt;
            _worldPos = Vector2Add(_worldPos, Vector2Scale(_chargeDir, step));
            _chargeTravelled += step;

            // Run-over damage along the charge (once), then push through.
            if (_target->IsAlive() && _contactCooldown <= 0.f &&
                Vector2Distance(_worldPos, _target->GetFeetWorldPos()) < _chargeContactRadius)
            {
                _target->TakeFractionalDamage(1.0f, _worldPos);
                _target->StartForcedPush(GetPushDirectionToPlayer(), _bossPushSpeed);
                _contactCooldown = _contactCooldownBase;
            }

            // Stop when he's travelled far enough, reaches the player, or hits a wall.
            bool atWall = (_worldPos.x <= 205.f || _worldPos.x >= (float)kVirtualWidth - 205.f ||
                           _worldPos.y <= 205.f || _worldPos.y >= (float)kVirtualHeight - 205.f);
            if (_chargeTravelled >= _chargeMaxDistance || atWall ||
                Vector2Distance(_worldPos, _target->GetFeetWorldPos()) < _meleeRange * 0.6f)
            {
                _impactShakeRequested = true;
                _state = State::Recovery; _stateTimer = 0.f;
                _meleeCooldown = _meleeCooldownBase * 0.7f;
                SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
            }
            break;
        }

        case State::Recovery:
            _stateTimer += dt;
            if (_stateTimer >= _recoveryDuration)
            {
                _state = State::Advancing; _stateTimer = 0.f;
            }
            break;
        }

        if (_state != State::MeleeAttacking && _state != State::Slamming && _state != State::ShieldCharge)
            TryDealContactDamage();

        _worldPos.x = std::clamp(_worldPos.x, 200.f, (float)kVirtualWidth  - 200.f);
        _worldPos.y = std::clamp(_worldPos.y, 200.f, (float)kVirtualHeight - 200.f);
    }

    HandleAnimation(dt);
}

void TitanGuard::TryDealContactDamage()
{
    if (_target == nullptr || !_target->IsAlive()) return;
    if (_target->IsBeingForcedPushed()) return;
    if (_contactCooldown > 0.f) return;
    if (!CheckCollisionRecs(GetBodyContactRec(), _target->GetCollisionRec())) return;

    _target->TakeFractionalDamage(_bossDamagePerHit, _worldPos);
    _target->StartForcedPush(GetPushDirectionToPlayer(), _bossPushSpeed);
    _contactCooldown = _contactCooldownBase;
}

Rectangle TitanGuard::GetBodyContactRec() const
{
    Rectangle bodyRec = GetCollisionRec();
    bodyRec.x += 28.f; bodyRec.y += 22.f;
    bodyRec.width  = std::max(1.f, bodyRec.width  - 56.f);
    bodyRec.height = std::max(1.f, bodyRec.height - 44.f);
    return bodyRec;
}

Vector2 TitanGuard::GetPushDirectionToPlayer() const
{
    if (_target == nullptr) return Vector2{ 1.f, 0.f };
    Vector2 away = Vector2Subtract(_target->GetWorldPos(), _worldPos);
    return (Vector2Length(away) > 0.01f) ? Vector2Normalize(away) : Vector2{ 1.f, 0.f };
}

bool TitanGuard::ConsumeImpactShakeRequest()
{
    bool requested = _impactShakeRequested;
    _impactShakeRequested = false;
    return requested;
}

void TitanGuard::BeginBulwarkSlam()
{
    _state = State::SlamWindup; _stateTimer = 0.f;
    SetAnimation(_sharedDefendAnim, _slamWindupDuration / (float)_sheetFrameCount, true);
}

void TitanGuard::ReactToPhaseChange(int newPhase)
{
    (void)newPhase;
    // Queue the signature Bulwark Slam; the Update loop fires it once he's neutral
    // (so it can't cut off a mace swing mid-frame). The post-slam stagger is the
    // guaranteed full-damage opening each phase hands the player.
    _pendingSlamQueued = true;
    _impactShakeRequested = true;
    PlaySound(_sharedSlamSound);
}

void TitanGuard::HandleAnimation(float dt)
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
            // Slam windup + stagger hold their final frames.
            if (_state == State::SlamWindup || _state == State::Slamming || _state == State::Staggered)
            {
                _frame = _maxFrames - 1;
                return;
            }
            _frame = 0;
        }
    }
}

void TitanGuard::DrawEnemy(Vector2 cameraRef)
{
    if (!_isActive) return;

    float drawWidth  = _stableFrameW * _scale;
    float drawHeight = _stableFrameH * _scale;

    Vector2 screenPos = Vector2Subtract(_worldPos, cameraRef);
    screenPos.x += kVirtualWidth  / 2.f;
    screenPos.y += kVirtualHeight / 2.f;

    DrawEllipse((int)screenPos.x, (int)(screenPos.y + drawHeight * 0.36f),
        drawWidth * 0.30f, drawHeight * 0.10f, Fade(BLACK, 0.35f));

    // Frontal shield shimmer — a reminder of where NOT to attack from. It winks out
    // while he's staggered or has dropped his guard to attack, telegraphing the
    // window when the front is fully open.
    if (!_dying && _state != State::Staggered && !IsShieldDown())
    {
        float pulse = sinf((float)GetTime() * 4.f) * 0.5f + 0.5f;
        Vector2 shieldPos{ screenPos.x + _rightLeft * drawWidth * 0.34f, screenPos.y };
        DrawCircleV(shieldPos, drawWidth * 0.20f, Fade(Color{ 150, 220, 255, 255 }, 0.10f + 0.06f * pulse));
    }

    // Slam telegraph + shockwave ring.
    if (_state == State::SlamWindup)
    {
        float pulse = sinf((float)GetTime() * 12.f) * 0.5f + 0.5f;
        DrawCircleLines((int)screenPos.x, (int)screenPos.y, _slamRadius,
            Fade(Color{ 255, 200, 90, 255 }, 0.35f + 0.3f * pulse));
    }
    if (_state == State::Slamming)
    {
        DrawCircleLines((int)screenPos.x, (int)screenPos.y, _slamRingRadius, Fade(Color{ 255, 210, 110, 255 }, 0.85f));
        DrawCircleLines((int)screenPos.x, (int)screenPos.y, _slamRingRadius * 0.85f, Fade(Color{ 255, 160, 70, 255 }, 0.5f));
    }
    // Stagger stars.
    if (_state == State::Staggered)
    {
        float starSpin = (float)GetTime() * 6.f;
        for (int i = 0; i < 4; i++)
        {
            float angle = starSpin + (float)i * (2.f * PI / 4.f);
            DrawPoly(Vector2{ screenPos.x + cosf(angle) * drawWidth * 0.30f,
                              screenPos.y - drawHeight * 0.44f + sinf(angle) * 12.f },
                4, 9.f, starSpin * 40.f + i * 45.f, Fade(YELLOW, 0.9f));
        }
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

Rectangle TitanGuard::GetCollisionRec() const
{
    Rectangle animBodyRect;
    if (GetAnimBodyRectWorld(animBodyRect))
        return animBodyRect;
    if (_hasTunedCollision)
        return GetTunedCollisionRec();

    float halfW = _stableFrameW * _scale * 0.5f;
    float halfH = _stableFrameH * _scale * 0.5f;
    return Rectangle{ _worldPos.x - halfW * 0.54f, _worldPos.y - halfH * 0.56f, halfW * 1.08f, halfH * 1.20f };
}

Capsule2D TitanGuard::GetCapsule() const
{
    Capsule2D animBodyCapsule;
    if (GetAnimBodyCapsuleWorld(animBodyCapsule))
        return animBodyCapsule;
    if (_capsuleRadius > 0.f)
        return Capsule2D{ { _worldPos.x + _capsuleOffset.x, _worldPos.y + _capsuleOffset.y }, _capsuleHalfHeight, _capsuleRadius };
    return Capsule2D{ { _worldPos.x, _worldPos.y + 14.f }, 22.f, _stableFrameW * _scale * 0.25f };
}

void TitanGuard::TakeDamage(int damage, Vector2 attackerPos)
{
    if (_dying || _hitTimer > 0.f) return;

    int appliedDamage = damage;

    // The tower shield chips frontal hits — but ONLY while it's actually raised.
    // It drops when he's staggered, and for a beat whenever he commits to an attack
    // (mace swing / bomb throw), so punishing his attacks from the front is real,
    // readable counterplay instead of "nothing works unless you're behind him".
    bool shieldRaised = (_state != State::Staggered) && !IsShieldDown();
    if (shieldRaised)
    {
        float attackSide = attackerPos.x - _worldPos.x;
        bool fromTheFront = (attackSide * _rightLeft) > 0.f;
        if (fromTheFront)
        {
            appliedDamage = std::max(1, (damage * 2) / 5);   // ~40% through a raised shield
            float pitch = GetRandomValue(85, 110) / 100.f;
            SetSoundPitch(_sharedBlockSound, pitch);
            SetSoundVolume(_sharedBlockSound, 0.45f);
            PlaySound(_sharedBlockSound);
        }
    }

    _health -= (float)appliedDamage;
    if (_health > 0.f) PlayHurtSound();

    if (_health <= 0.f)
    {
        _health = 0.f; _dying = true;
        _takingDamage = false; _attacking = false;
        _pendingBomb = false;
        _state = State::Recovery;
        _deathTimer = 0.7f;
        SetAnimation(_sharedDeathAnim, 1.f / 6.f, true);
        PlayDeathSound();
        return;
    }

    _velocity = Vector2Zero();
    if (_state == State::Advancing)
    {
        _takingDamage = true;
        SetAnimation(_sharedHurtAnim, 1.f / 12.f, true);
    }
    _hitTimer = 0.02f;
}

void TitanGuard::ApplyFreeze(float duration)
{
    if (_dying || !IsAlive()) return;
    if (duration > _freezeTimer) _freezeTimer = duration;
}

void TitanGuard::SetWaveScale(int wave)
{
    (void)wave;
    _expValue = _bossBaseExpValue;
    _health = Balance::Boss::kTitanGuardHealth; _maxHealth = Balance::Boss::kTitanGuardHealth;
    _speed = _moveSpeed; _attackPower = 1.f;
}

void TitanGuard::EnsureSharedResourcesLoaded()
{
    if (_sharedResourcesLoaded) return;

    _sharedIdleAnim   = LoadTexture(AssetPath("Bosses/TitanGuardIdle.png").c_str());
    _sharedWalkAnim   = LoadTexture(AssetPath("Bosses/TitanGuardWalk.png").c_str());
    _sharedMeleeAnim  = LoadTexture(AssetPath("Bosses/TitanGuardMeleeAttack.png").c_str());
    _sharedBombAnim   = LoadTexture(AssetPath("Bosses/TitanGuardBombThrow.png").c_str());
    _sharedDefendAnim = LoadTexture(AssetPath("Bosses/TitanGuardDefend.png").c_str());
    _sharedHurtAnim   = LoadTexture(AssetPath("Bosses/TitanGuardHurt.png").c_str());
    _sharedDeathAnim  = LoadTexture(AssetPath("Bosses/TitanGuardDeath.png").c_str());
    _sharedAttackSound = LoadSound(AssetPath("Sounds/SwordSwipe2.ogg").c_str());
    _sharedHurtSound   = LoadSound(AssetPath("Sounds/SmallMonsterDamage.ogg").c_str());
    _sharedDeathSound  = LoadSound(AssetPath("Sounds/MonsterDeath.ogg").c_str());
    _sharedBlockSound  = LoadSound(AssetPath("Sounds/OgreHitWall.ogg").c_str());
    _sharedSlamSound   = LoadSound(AssetPath("Sounds/OgreChargeHit.ogg").c_str());
    _sharedResourcesLoaded = true;
}

void TitanGuard::UnloadSharedResources()
{
    if (!_sharedResourcesLoaded) return;

    UnloadTexture(_sharedIdleAnim);   UnloadTexture(_sharedWalkAnim);
    UnloadTexture(_sharedMeleeAnim);  UnloadTexture(_sharedBombAnim);
    UnloadTexture(_sharedDefendAnim); UnloadTexture(_sharedHurtAnim);
    UnloadTexture(_sharedDeathAnim);
    UnloadSound(_sharedAttackSound);  UnloadSound(_sharedHurtSound);
    UnloadSound(_sharedDeathSound);   UnloadSound(_sharedBlockSound);
    UnloadSound(_sharedSlamSound);
    _sharedIdleAnim = Texture2D{};   _sharedWalkAnim = Texture2D{};
    _sharedMeleeAnim = Texture2D{};  _sharedBombAnim = Texture2D{};
    _sharedDefendAnim = Texture2D{}; _sharedHurtAnim = Texture2D{};
    _sharedDeathAnim = Texture2D{};
    _sharedAttackSound = Sound{};    _sharedHurtSound = Sound{};
    _sharedDeathSound = Sound{};     _sharedBlockSound = Sound{};
    _sharedSlamSound = Sound{};
    _sharedResourcesLoaded = false;
}
