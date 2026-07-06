#include "Werewolf.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "Character.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

Texture2D Werewolf::_sharedIdleAnim{};
Texture2D Werewolf::_sharedWalkAnim{};
Texture2D Werewolf::_sharedMeleeAnim{};
Texture2D Werewolf::_sharedMelee2Anim{};
Texture2D Werewolf::_sharedJumpAnim{};
Texture2D Werewolf::_sharedHowlAnim{};
Texture2D Werewolf::_sharedHurtAnim{};
Texture2D Werewolf::_sharedDeathAnim{};
Sound     Werewolf::_sharedAttackSound{};
Sound     Werewolf::_sharedHurtSound{};
Sound     Werewolf::_sharedDeathSound{};
Sound     Werewolf::_sharedHowlSound{};
bool      Werewolf::_sharedResourcesLoaded = false;

Werewolf::Werewolf(Vector2 pos) : Enemy(pos) {}
Werewolf::~Werewolf() {}

void Werewolf::Init()
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

void Werewolf::ResetForSpawn(Vector2 pos)
{
    _worldPos = pos; _worldPosLastFrame = pos; _homePos = pos;
    _velocity = Vector2Zero();
    _isActive = true;

    _stableFrameW = (float)_sharedIdleAnim.width / (float)_sheetFrameCount;
    _stableFrameH = (float)_sharedIdleAnim.height;

    SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
    _scale = _bossScale;

    _health = 62.f; _maxHealth = 62.f;
    _attackPower = 1.f;
    _speed = _moveSpeed;
    _expValue = _bossBaseExpValue;

    _hitTimer = 0.f; _deathTimer = 0.7f; _freezeTimer = 0.f;
    _isCharged = false; _chargeNextStunTime = 0.f; _electricChargeTotalTimer = 0.f;
    _isEliteMiniboss = false; _isInvulnerable = false; _leapInvulnerable = false;
    _takingDamage = false; _attacking = false; _dying = false;

    _state = State::Chasing;
    _stateTimer = 0.f;
    _comboCooldown = 1.0f;
    _contactCooldown = 1.0f;
    _pounceCooldown = 3.0f;
    _frenzyTimer = 0.f;
    _howledAt66 = false;
    _howledAt33 = false;
    _airborneTimer = 0.f;
    _swipeDamageApplied = false;
    _landingDamageApplied = false;
    _impactShakeRequested = false;
    _circleSign = (GetRandomValue(0, 1) == 0) ? -1.f : 1.f;

    _forcedPushActive = false; _forcedPushDirection = Vector2Zero(); _forcedPushSpeed = 0.f;
    _pendingBurns.clear();
    _waypoints.clear(); _waypointIndex = 0;

    ResetTuningState();
    ApplyStoredTuning();
}

void Werewolf::SetAnimation(const Texture2D& sheet, float frameTime, bool resetFrame)
{
    SetSpriteSheet(sheet, _sheetFrameCount, frameTime, resetFrame);
}

int Werewolf::GetCurrentAnimSlot() const
{
    if (_texture.id == _sharedIdleAnim.id)   return 0;
    if (_texture.id == _sharedWalkAnim.id)   return 1;
    if (_texture.id == _sharedMeleeAnim.id)  return 2;
    if (_texture.id == _sharedMelee2Anim.id) return 3;
    if (_texture.id == _sharedJumpAnim.id)   return 4;
    if (_texture.id == _sharedHowlAnim.id)   return 5;
    if (_texture.id == _sharedHurtAnim.id)   return 6;
    if (_texture.id == _sharedDeathAnim.id)  return 7;
    return 0;
}

const char* Werewolf::GetEditorAnimName(int index) const
{
    static const char* kNames[8] = { "Idle", "Walk", "Swipe1", "Swipe2", "Pounce", "Howl", "Hurt", "Death" };
    return (index >= 0 && index < 8) ? kNames[index] : "";
}

void Werewolf::PlayEditorAnim(int index)
{
    const Texture2D* sheets[8] = {
        &_sharedIdleAnim, &_sharedWalkAnim, &_sharedMeleeAnim, &_sharedMelee2Anim,
        &_sharedJumpAnim, &_sharedHowlAnim, &_sharedHurtAnim, &_sharedDeathAnim
    };
    if (index < 0 || index > 7) return;
    float frameTimeOverride = _editorAnimFrameTimes[index];
    SetAnimation(*sheets[index], (frameTimeOverride > 0.f) ? frameTimeOverride : 1.f / 8.f, true);
}

void Werewolf::Update(float dt, Vector2 heroWorldPos, Vector2, bool,
    const std::vector<std::unique_ptr<Enemy>>&, const std::vector<Vector2>&)
{
    if (!_isActive) return;
    _worldPosLastFrame = _worldPos;

    UpdateHit(dt);
    UpdateBurns(dt);
    UpdateElectricCharge(dt);

    if (_freezeTimer > 0.f)     _freezeTimer -= dt;
    if (_comboCooldown > 0.f)   _comboCooldown -= dt;
    if (_contactCooldown > 0.f) _contactCooldown -= dt;
    if (_frenzyTimer > 0.f)     _frenzyTimer -= dt;
    if (_pounceCooldown > 0.f && _state == State::Chasing) _pounceCooldown -= dt;

    if (!_dying && _target != nullptr)
    {
        // Blood Howl thresholds interrupt the chase.
        bool canHowl = (_state == State::Chasing);
        if (canHowl && !_howledAt66 && _health <= _maxHealth * 0.66f)
        {
            _howledAt66 = true;
            _state = State::Howling; _stateTimer = 0.f;
            SetAnimation(_sharedHowlAnim, _howlDuration / (float)_sheetFrameCount, true);
            PlaySound(_sharedHowlSound);
        }
        else if (canHowl && !_howledAt33 && _health <= _maxHealth * 0.33f)
        {
            _howledAt33 = true;
            _state = State::Howling; _stateTimer = 0.f;
            SetAnimation(_sharedHowlAnim, _howlDuration / (float)_sheetFrameCount, true);
            PlaySound(_sharedHowlSound);
        }

        bool controlled = IsFrozen() || IsElectroStunned();
        float frenzyMult = IsFrenzied() ? 1.35f : 1.f;

        switch (_state)
        {
        case State::Chasing:
            if (!controlled) HandleChasing(dt, heroWorldPos);
            break;

        case State::Combo1:
            HandleCombo(false, dt);
            break;

        case State::Combo2:
            HandleCombo(true, dt);
            break;

        case State::PounceCharging:
            _stateTimer += dt;
            _pounceTarget = _target->GetFeetWorldPos();   // tracks until launch
            if (_stateTimer >= _pounceChargeDuration / frenzyMult)
            {
                _state = State::Airborne;
                _pounceStart = _worldPos;
                _airborneTimer = 0.f;
                _landingDamageApplied = false;
                SetAnimation(_sharedJumpAnim, _pounceAirDuration / (float)_sheetFrameCount, true);
            }
            break;

        case State::Airborne:
            HandleAirborne(dt);
            break;

        case State::Landing:
            _stateTimer += dt;
            if (!_landingDamageApplied)
            {
                _landingDamageApplied = true;
                if (Vector2Distance(_worldPos, _target->GetFeetWorldPos()) < _landingRadius)
                {
                    _target->TakeFractionalDamage(1.0f, _worldPos);
                    _target->StartForcedPush(GetPushDirectionToPlayer(), _bossPushSpeed);
                }
            }
            if (_stateTimer >= 0.32f)
            {
                _state = State::Recovery; _stateTimer = 0.f;
                _pounceCooldown = IsFrenzied() ? _pounceCooldownBase * 0.55f : _pounceCooldownBase;
                SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
            }
            break;

        case State::Howling:
            _stateTimer += dt;
            if (_stateTimer >= _howlDuration)
            {
                _frenzyTimer = _frenzyDuration;
                _state = State::Recovery; _stateTimer = 0.f;
                SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
            }
            break;

        case State::Recovery:
            _stateTimer += dt;
            if (_stateTimer >= _recoveryDuration / frenzyMult)
            {
                _state = State::Chasing; _stateTimer = 0.f;
            }
            break;
        }

        if (_state != State::Airborne)
            TryDealContactDamage();

        _worldPos.x = std::clamp(_worldPos.x, 180.f, (float)kVirtualWidth  - 180.f);
        _worldPos.y = std::clamp(_worldPos.y, 180.f, (float)kVirtualHeight - 180.f);
    }

    HandleAnimation(dt);
}

void Werewolf::HandleChasing(float dt, Vector2 heroWorldPos)
{
    Vector2 toPlayer = Vector2Subtract(heroWorldPos, _worldPos);
    float dist = Vector2Length(toPlayer);

    if (toPlayer.x < -20.f) _rightLeft = -1.f;
    if (toPlayer.x >  20.f) _rightLeft =  1.f;

    if (_pounceCooldown <= 0.f && dist > 300.f)
    {
        _state = State::PounceCharging; _stateTimer = 0.f;
        SetAnimation(_sharedJumpAnim, 0.3f, true);   // crouch pose (held frames)
        return;
    }

    if (dist < _meleeRange && _comboCooldown <= 0.f)
    {
        _state = State::Combo1;
        _swipeDamageApplied = false;
        SetAnimation(_sharedMeleeAnim, 1.f / (IsFrenzied() ? 15.f : 11.f), true);
        PlaySound(_attackSound);
        return;
    }

    // Predator approach: mostly straight in, with a sideways arc blended in so
    // it circles around the player's guard instead of walking a beeline.
    float speed = (IsFrenzied() ? _speed * 1.35f : _speed);
    Vector2 dir = (dist > 0.01f) ? Vector2Scale(toPlayer, 1.f / dist) : Vector2{ 1.f, 0.f };
    Vector2 strafe{ -dir.y * _circleSign, dir.x * _circleSign };
    Vector2 moveDir = Vector2Normalize(Vector2Add(dir, Vector2Scale(strafe, dist < 420.f ? 0.55f : 0.15f)));
    _worldPos = Vector2Add(_worldPos, Vector2Scale(moveDir, speed * dt));

    if (GetRandomValue(0, 200) == 0)
        _circleSign = -_circleSign;

    if (_texture.id != _sharedWalkAnim.id && !_takingDamage)
        SetAnimation(_sharedWalkAnim, 1.f / 10.f, true);
}

void Werewolf::HandleCombo(bool secondSwipe, float dt)
{
    // The second swipe lunges forward so back-stepping players still get clipped.
    if (secondSwipe && _target != nullptr && !_swipeDamageApplied)
    {
        Vector2 toPlayer = Vector2Subtract(_target->GetFeetWorldPos(), _worldPos);
        if (Vector2Length(toPlayer) > 40.f)
            _worldPos = Vector2Add(_worldPos, Vector2Scale(Vector2Normalize(toPlayer), 340.f * dt));
    }
    else
    {
        _velocity = Vector2Zero();
    }

    if (!_swipeDamageApplied && _frame >= 3 && _target != nullptr)
    {
        Rectangle attackRec;
        if (!GetAnimMeleeRectWorld(secondSwipe ? 3 : 2, attackRec))
        {
            attackRec = GetBodyContactRec();
            attackRec.width += 100.f;
            if (_rightLeft < 0.f) attackRec.x -= 100.f;
            attackRec.y -= 20.f; attackRec.height += 40.f;
        }
        if (CheckCollisionRecs(attackRec, _target->GetCollisionRec()))
        {
            _target->TakeFractionalDamage(_bossDamagePerHit, _worldPos);
            _target->StartForcedPush(GetPushDirectionToPlayer(), _bossPushSpeed);
        }
        _swipeDamageApplied = true;
    }

    if (_frame >= _maxFrames - 1)
    {
        if (!secondSwipe)
        {
            // Chain straight into the follow-up swipe.
            _state = State::Combo2;
            _swipeDamageApplied = false;
            SetAnimation(_sharedMelee2Anim, 1.f / (IsFrenzied() ? 15.f : 11.f), true);
            PlaySound(_attackSound);
        }
        else
        {
            _state = State::Recovery; _stateTimer = 0.f;
            _comboCooldown = IsFrenzied() ? _comboCooldownBase * 0.55f : _comboCooldownBase;
            SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
        }
    }
}

void Werewolf::HandleAirborne(float dt)
{
    _airborneTimer += dt;
    float duration = IsFrenzied() ? _pounceAirDuration * 0.8f : _pounceAirDuration;
    float t = std::min(1.f, _airborneTimer / duration);
    _worldPos = Vector2Lerp(_pounceStart, _pounceTarget, t);

    if (t >= 1.f)
    {
        _state = State::Landing; _stateTimer = 0.f;
        _impactShakeRequested = true;
        SetAnimation(_sharedMeleeAnim, 1.f / 16.f, true);
    }
}

void Werewolf::TryDealContactDamage()
{
    if (_target == nullptr || !_target->IsAlive()) return;
    if (_target->IsBeingForcedPushed()) return;
    if (_state == State::Combo1 || _state == State::Combo2 || _state == State::Landing) return;
    if (_contactCooldown > 0.f) return;
    if (!CheckCollisionRecs(GetBodyContactRec(), _target->GetCollisionRec())) return;

    _target->TakeFractionalDamage(_bossDamagePerHit, _worldPos);
    _target->StartForcedPush(GetPushDirectionToPlayer(), _bossPushSpeed);
    _contactCooldown = _contactCooldownBase;
}

Rectangle Werewolf::GetBodyContactRec() const
{
    Rectangle bodyRec = GetCollisionRec();
    bodyRec.x += 24.f; bodyRec.y += 20.f;
    bodyRec.width  = std::max(1.f, bodyRec.width  - 48.f);
    bodyRec.height = std::max(1.f, bodyRec.height - 40.f);
    return bodyRec;
}

Vector2 Werewolf::GetPushDirectionToPlayer() const
{
    if (_target == nullptr) return Vector2{ 1.f, 0.f };
    Vector2 away = Vector2Subtract(_target->GetWorldPos(), _worldPos);
    return (Vector2Length(away) > 0.01f) ? Vector2Normalize(away) : Vector2{ 1.f, 0.f };
}

bool Werewolf::ConsumeImpactShakeRequest()
{
    bool requested = _impactShakeRequested;
    _impactShakeRequested = false;
    return requested;
}

void Werewolf::HandleAnimation(float dt)
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
            // Pounce crouch holds its last frame until launch.
            if (_state == State::PounceCharging) { _frame = _maxFrames - 1; return; }
            _frame = 0;
        }
    }
}

void Werewolf::DrawEnemy(Vector2 cameraRef)
{
    if (!_isActive) return;

    float drawWidth  = _stableFrameW * _scale;
    float drawHeight = _stableFrameH * _scale;

    Vector2 screenPos = Vector2Subtract(_worldPos, cameraRef);
    screenPos.x += kVirtualWidth  / 2.f;
    screenPos.y += kVirtualHeight / 2.f;

    // Pounce landing telegraph.
    if (_state == State::PounceCharging || _state == State::Airborne)
    {
        Vector2 targetScreen = Vector2Subtract(_pounceTarget, cameraRef);
        targetScreen.x += kVirtualWidth / 2.f; targetScreen.y += kVirtualHeight / 2.f;
        float pulse = sinf((float)GetTime() * 12.f) * 0.5f + 0.5f;
        DrawCircleLines((int)targetScreen.x, (int)targetScreen.y, _landingRadius,
            Fade(Color{ 220, 60, 60, 255 }, 0.5f + 0.3f * pulse));
    }

    float airLift = 0.f;
    if (_state == State::Airborne)
    {
        float t = std::min(1.f, _airborneTimer / _pounceAirDuration);
        airLift = sinf(t * PI) * 240.f;
    }

    bool burning = !_pendingBurns.empty();
    Color tint = IsElectroStunned() ? Color{ 255, 255,  60, 255 } :
                 IsFrozen()         ? Color{ 140, 200, 255, 255 } :
                 burning            ? Color{ 255, 180, 180, 255 } :
                 IsFrenzied()       ? Color{ 255, 150, 150, 255 } :
                                      WHITE;

    DrawEllipse((int)screenPos.x, (int)(screenPos.y + drawHeight * 0.36f),
        drawWidth * 0.28f, drawHeight * 0.10f, Fade(BLACK, 0.35f));

    // Howl rings.
    if (_state == State::Howling)
    {
        float t = fmodf(_stateTimer * 1.4f, 1.f);
        DrawCircleLines((int)screenPos.x, (int)(screenPos.y - drawHeight * 0.3f),
            30.f + t * 140.f, Fade(Color{ 255, 90, 90, 255 }, 0.6f * (1.f - t)));
    }
    // Frenzy glow.
    if (IsFrenzied())
        DrawCircleV(screenPos, drawWidth * 0.4f, Fade(Color{ 255, 60, 60, 255 }, 0.10f));

    Vector2 animDrawOffset = GetCurrentAnimDrawOffset();
    Rectangle source{ _frame * _width, 0.f, _rightLeft * _width, _height };
    Rectangle dest{ screenPos.x - drawWidth / 2.f + animDrawOffset.x,
                    screenPos.y - drawHeight / 2.f - airLift + animDrawOffset.y, drawWidth, drawHeight };
    DrawTexturePro(_texture, source, dest, Vector2{}, 0.f, tint);

    DrawHealthBar(screenPos, drawWidth, drawHeight);
}

Rectangle Werewolf::GetCollisionRec() const
{
    Rectangle animBodyRect;
    if (GetAnimBodyRectWorld(animBodyRect))
        return animBodyRect;
    if (_hasTunedCollision)
        return GetTunedCollisionRec();

    float halfW = _stableFrameW * _scale * 0.5f;
    float halfH = _stableFrameH * _scale * 0.5f;
    return Rectangle{ _worldPos.x - halfW * 0.50f, _worldPos.y - halfH * 0.55f, halfW * 1.00f, halfH * 1.20f };
}

Capsule2D Werewolf::GetCapsule() const
{
    Capsule2D animBodyCapsule;
    if (GetAnimBodyCapsuleWorld(animBodyCapsule))
        return animBodyCapsule;
    if (_capsuleRadius > 0.f)
        return Capsule2D{ { _worldPos.x + _capsuleOffset.x, _worldPos.y + _capsuleOffset.y }, _capsuleHalfHeight, _capsuleRadius };
    return Capsule2D{ { _worldPos.x, _worldPos.y + 12.f }, 16.f, _stableFrameW * _scale * 0.22f };
}

void Werewolf::TakeDamage(int damage, Vector2 attackerPos)
{
    (void)attackerPos;
    if (_dying || _hitTimer > 0.f) return;
    if (_state == State::Airborne) return;   // mid-pounce body can't be hit

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
    if (_state == State::Chasing)
    {
        _takingDamage = true;
        SetAnimation(_sharedHurtAnim, 1.f / 12.f, true);
    }
    _hitTimer = 0.02f;
}

void Werewolf::ApplyFreeze(float duration)
{
    if (_dying || !IsAlive()) return;
    if (_state == State::Airborne) return;
    if (duration > _freezeTimer) _freezeTimer = duration;
}

void Werewolf::SetWaveScale(int wave)
{
    (void)wave;
    _expValue = _bossBaseExpValue;
    _health = Balance::Boss::kWerewolfHealth; _maxHealth = Balance::Boss::kWerewolfHealth;
    _speed = _moveSpeed; _attackPower = 1.f;
}

void Werewolf::EnsureSharedResourcesLoaded()
{
    if (_sharedResourcesLoaded) return;

    _sharedIdleAnim   = LoadTexture(AssetPath("Bosses/WerewolfIdle.png").c_str());
    _sharedWalkAnim   = LoadTexture(AssetPath("Bosses/WerewolfWalk.png").c_str());
    _sharedMeleeAnim  = LoadTexture(AssetPath("Bosses/WerewolfMeleeAttack.png").c_str());
    _sharedMelee2Anim = LoadTexture(AssetPath("Bosses/WerewolfMeleeAttack2.png").c_str());
    _sharedJumpAnim   = LoadTexture(AssetPath("Bosses/WerewolfJump.png").c_str());
    _sharedHowlAnim   = LoadTexture(AssetPath("Bosses/WerewolfHowl.png").c_str());
    _sharedHurtAnim   = LoadTexture(AssetPath("Bosses/WerewolfHurt.png").c_str());
    _sharedDeathAnim  = LoadTexture(AssetPath("Bosses/WerewolfDeath.png").c_str());
    _sharedAttackSound = LoadSound(AssetPath("Sounds/SwordSwipe2.ogg").c_str());
    _sharedHurtSound   = LoadSound(AssetPath("Sounds/SmallMonsterDamage.ogg").c_str());
    _sharedDeathSound  = LoadSound(AssetPath("Sounds/MonsterDeath.ogg").c_str());
    _sharedHowlSound   = LoadSound(AssetPath("Sounds/OgreChargeHit.ogg").c_str());
    _sharedResourcesLoaded = true;
}

void Werewolf::UnloadSharedResources()
{
    if (!_sharedResourcesLoaded) return;

    UnloadTexture(_sharedIdleAnim);   UnloadTexture(_sharedWalkAnim);
    UnloadTexture(_sharedMeleeAnim);  UnloadTexture(_sharedMelee2Anim);
    UnloadTexture(_sharedJumpAnim);   UnloadTexture(_sharedHowlAnim);
    UnloadTexture(_sharedHurtAnim);   UnloadTexture(_sharedDeathAnim);
    UnloadSound(_sharedAttackSound);  UnloadSound(_sharedHurtSound);
    UnloadSound(_sharedDeathSound);   UnloadSound(_sharedHowlSound);
    _sharedIdleAnim = Texture2D{};   _sharedWalkAnim = Texture2D{};
    _sharedMeleeAnim = Texture2D{};  _sharedMelee2Anim = Texture2D{};
    _sharedJumpAnim = Texture2D{};   _sharedHowlAnim = Texture2D{};
    _sharedHurtAnim = Texture2D{};   _sharedDeathAnim = Texture2D{};
    _sharedAttackSound = Sound{};    _sharedHurtSound = Sound{};
    _sharedDeathSound = Sound{};     _sharedHowlSound = Sound{};
    _sharedResourcesLoaded = false;
}
