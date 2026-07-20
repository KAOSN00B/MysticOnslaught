#include "ToxicVermin.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "Character.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

Texture2D ToxicVermin::_sharedIdleAnim{};
Texture2D ToxicVermin::_sharedWalkAnim{};
Texture2D ToxicVermin::_sharedMeleeAnim{};
Texture2D ToxicVermin::_sharedSpitAnim{};
Texture2D ToxicVermin::_sharedBurrowAnim{};
Texture2D ToxicVermin::_sharedEmergeAnim{};
Texture2D ToxicVermin::_sharedHurtAnim{};
Texture2D ToxicVermin::_sharedDeathAnim{};
Sound     ToxicVermin::_sharedAttackSound{};
Sound     ToxicVermin::_sharedHurtSound{};
Sound     ToxicVermin::_sharedDeathSound{};
Sound     ToxicVermin::_sharedBurrowSound{};
bool      ToxicVermin::_sharedResourcesLoaded = false;

ToxicVermin::ToxicVermin(Vector2 pos) : Enemy(pos) {}
ToxicVermin::~ToxicVermin() {}

void ToxicVermin::Init()
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

void ToxicVermin::ResetForSpawn(Vector2 pos)
{
    _worldPos = pos; _worldPosLastFrame = pos; _homePos = pos;
    _velocity = Vector2Zero();
    _isActive = true;

    _stableFrameW = (float)_sharedIdleAnim.width / (float)_sheetFrameCount;
    _stableFrameH = (float)_sharedIdleAnim.height;

    SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
    _scale = _bossScale;

    _health = 66.f; _maxHealth = 66.f;
    _attackPower = 1.f;
    _speed = _surfaceSpeed;
    _expValue = _bossBaseExpValue;

    _hitTimer = 0.f; _deathTimer = 0.7f; _freezeTimer = 0.f;
    _isCharged = false; _chargeNextStunTime = 0.f; _electricChargeTotalTimer = 0.f;
    _isEliteMiniboss = false; _isInvulnerable = false; _leapInvulnerable = false;
    _takingDamage = false; _attacking = false; _dying = false;

    _state = State::Surface;
    _stateTimer = 0.f;
    _meleeCooldown = 1.2f;
    _contactCooldown = 1.0f;
    _spitCooldown = 2.4f;
    _burrowCooldown = 3.4f;
    _pendingSpit = false;
    _spitDirection = Vector2Zero();
    _spitCount = 3;
    _pendingPoisonPool = false;
    _poisonPoolPos = pos;
    _eruptDamageApplied = false;
    _eruptChainRemaining = 0;
    _pendingPhaseBurrow = false;
    _impactShakeRequested = false;
    ClearEliteEvents();

    // 3 phases: 66% widens the toxic spit to 5 globs + a double eruption; 33% erupts
    // THREE times in a row and fouls the ground on every surface attack.
    SetPhaseThresholds({ 0.66f, 0.33f });

    _forcedPushActive = false; _forcedPushDirection = Vector2Zero(); _forcedPushSpeed = 0.f;
    _pendingBurns.clear();
    _waypoints.clear(); _waypointIndex = 0;

    ResetTuningState();
    ApplyStoredTuning();
}

void ToxicVermin::SetAnimation(const Texture2D& sheet, float frameTime, bool resetFrame)
{
    SetSpriteSheet(sheet, _sheetFrameCount, frameTime, resetFrame);
}

int ToxicVermin::GetCurrentAnimSlot() const
{
    if (_texture.id == _sharedIdleAnim.id)   return 0;
    if (_texture.id == _sharedWalkAnim.id)   return 1;
    if (_texture.id == _sharedMeleeAnim.id)  return 2;
    if (_texture.id == _sharedSpitAnim.id)   return 3;
    if (_texture.id == _sharedBurrowAnim.id) return 4;
    if (_texture.id == _sharedEmergeAnim.id) return 5;
    if (_texture.id == _sharedHurtAnim.id)   return 6;
    if (_texture.id == _sharedDeathAnim.id)  return 7;
    return 0;
}

const char* ToxicVermin::GetEditorAnimName(int index) const
{
    static const char* kNames[8] = { "Idle", "Slither", "Bite", "Spit", "Burrow", "Erupt", "Hurt", "Death" };
    return (index >= 0 && index < 8) ? kNames[index] : "";
}

void ToxicVermin::PlayEditorAnim(int index)
{
    const Texture2D* sheets[8] = {
        &_sharedIdleAnim, &_sharedWalkAnim, &_sharedMeleeAnim, &_sharedSpitAnim,
        &_sharedBurrowAnim, &_sharedEmergeAnim, &_sharedHurtAnim, &_sharedDeathAnim
    };
    if (index < 0 || index > 7) return;
    float frameTimeOverride = _editorAnimFrameTimes[index];
    SetAnimation(*sheets[index], (frameTimeOverride > 0.f) ? frameTimeOverride : 1.f / 8.f, true);
}

bool ToxicVermin::ConsumePoisonPoolRequest(Vector2& outPos)
{
    if (!_pendingPoisonPool)
        return false;
    _pendingPoisonPool = false;
    outPos = _poisonPoolPos;
    return true;
}

void ToxicVermin::Update(float dt, Vector2 heroWorldPos, Vector2, bool,
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
    if (_state == State::Surface)
    {
        if (_spitCooldown > 0.f)   _spitCooldown -= dt;
        if (_burrowCooldown > 0.f) _burrowCooldown -= dt;
    }

    if (!_dying && _target != nullptr)
    {
        bool controlled = IsFrozen() || IsElectroStunned();
        Vector2 toPlayer = Vector2Subtract(heroWorldPos, _worldPos);
        float dist = Vector2Length(toPlayer);

        // Each phase change erupts with a toxic spit on the SURFACE — deliberately
        // NOT a burrow, so a phase transition never yanks the worm underground
        // (untouchable) and locks the player out of damaging it.
        int newPhase = ConsumePhaseChange();
        if (newPhase >= 0)
        {
            EmitEliteEvent({ EliteEventKind::PhaseChange, EliteArchetype::Ogre,
                             EliteMove::None, 0, _worldPos });
            _pendingPhaseBurrow = true; _impactShakeRequested = true;
            RequestBossCallout(newPhase >= 2 ? "PLAGUE FLOOD" : "VILE SURGE");
        }
        if (_pendingPhaseBurrow && !controlled && _state == State::Surface)
        {
            _pendingPhaseBurrow = false;
            _spitCooldown = 0.f;   // Surface handler will spit this frame (stays hittable)
        }

        switch (_state)
        {
        case State::Surface:
        {
            if (controlled) break;

            if (toPlayer.x < -20.f) _rightLeft = -1.f;
            if (toPlayer.x >  20.f) _rightLeft =  1.f;

            if (_burrowCooldown <= 0.f)
            {
                _state = State::Burrowing; _stateTimer = 0.f;
                // Erupt chain scales with phase: double at phase 1, triple at phase 2.
                _eruptChainRemaining = (GetPhase() >= 2) ? 2 : (GetPhase() >= 1 ? 1 : 0);
                SetAnimation(_sharedBurrowAnim, _burrowDuration / (float)_sheetFrameCount, true);
                PlaySound(_sharedBurrowSound);
                break;
            }
            if (dist < _meleeRange && _meleeCooldown <= 0.f)
            {
                _state = State::MeleeAttacking;
                _damageApplied = false;
                SetAnimation(_sharedMeleeAnim, 1.f / 11.f, true);
                PlaySound(_attackSound);
                break;
            }
            if (_spitCooldown <= 0.f && dist > 240.f)
            {
                _state = State::SpitCasting; _stateTimer = 0.f;
                SetAnimation(_sharedSpitAnim, _spitCastDuration / (float)_sheetFrameCount, true);
                break;
            }

            Vector2 dir = (dist > 0.01f) ? Vector2Scale(toPlayer, 1.f / dist) : Vector2{ 1.f, 0.f };
            _worldPos = Vector2Add(_worldPos, Vector2Scale(dir, _surfaceSpeed * dt));
            if (_texture.id != _sharedWalkAnim.id && !_takingDamage)
                SetAnimation(_sharedWalkAnim, 1.f / 9.f, true);
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
                    attackRec.width += 90.f;
                    if (_rightLeft < 0.f) attackRec.x -= 90.f;
                    attackRec.y -= 20.f; attackRec.height += 40.f;
                }
                if (CheckCollisionRecs(attackRec, _target->GetCollisionRec()))
                {
                    _target->TakeFractionalDamage(_bossDamagePerHit, _worldPos);
                    _target->StartForcedPush(GetPushDirectionToPlayer(), _bossPushSpeed);
                }
                if (GetPhase() >= 2) { _pendingPoisonPool = true; _poisonPoolPos = _worldPos; }   // bite fouls the ground
                _damageApplied = true;
            }
            if (_frame >= _maxFrames - 1)
            {
                _state = State::Recovery; _stateTimer = 0.f;
                _meleeCooldown = _meleeCooldownBase;
                SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
            }
            break;

        case State::SpitCasting:
            _velocity = Vector2Zero();
            _stateTimer += dt;
            if (_stateTimer >= _spitCastDuration)
            {
                Vector2 aim = Vector2Subtract(_target->GetFeetWorldPos(), _worldPos);
                _pendingSpit = true;
                _spitDirection = (Vector2LengthSqr(aim) > 0.0001f) ? Vector2Normalize(aim) : Vector2{ 1.f, 0.f };
                _spitCount = (GetPhase() >= 1) ? 5 : 3;
                if (GetPhase() >= 2) { _pendingPoisonPool = true; _poisonPoolPos = _worldPos; }   // fouls the ground
                _state = State::Recovery; _stateTimer = 0.f;
                _spitCooldown = _spitCooldownBase;
                SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
            }
            break;

        case State::Burrowing:
            _velocity = Vector2Zero();
            _stateTimer += dt;
            if (_stateTimer >= _burrowDuration)
            {
                // Going under leaves a pool where it sank.
                _pendingPoisonPool = true;
                _poisonPoolPos = _worldPos;
                _state = State::Tunnelling; _stateTimer = 0.f;
            }
            break;

        case State::Tunnelling:
        {
            // Underground: unhittable, the mound races toward the player.
            _stateTimer += dt;
            float tunnelSpeed = IsEnraged() ? _tunnelSpeed * 1.25f : _tunnelSpeed;
            if (dist > 30.f)
            {
                Vector2 dir = Vector2Scale(toPlayer, 1.f / dist);
                _worldPos = Vector2Add(_worldPos, Vector2Scale(dir, tunnelSpeed * dt));
            }
            float duration = IsEnraged() ? _tunnelDuration * 1.2f : _tunnelDuration;
            // Erupt when underneath the player, or when the tunnel times out.
            if ((dist < 60.f && _stateTimer > 0.6f) || _stateTimer >= duration)
            {
                _state = State::Erupting; _stateTimer = 0.f;
                _eruptDamageApplied = false;
                _impactShakeRequested = true;
                SetAnimation(_sharedEmergeAnim, 1.f / 14.f, true);
                PlaySound(_sharedBurrowSound);
            }
            break;
        }

        case State::Erupting:
            _stateTimer += dt;
            // The eruption point LOCKED the moment the mound stopped (position
            // froze on entering this state); the burst now waits behind a real
            // marked warning instead of striking 0.12s after surfacing —
            // stepping off the marker always works.
            if (!_eruptDamageApplied && _stateTimer > 0.50f)
            {
                _eruptDamageApplied = true;
                if (Vector2Distance(_worldPos, _target->GetFeetWorldPos()) < _eruptRadius)
                {
                    _target->TakeFractionalDamage(1.0f, _worldPos);
                    _target->StartForcedPush(GetPushDirectionToPlayer(), _bossPushSpeed);
                }
                // Every eruption fouls the ground.
                _pendingPoisonPool = true;
                _poisonPoolPos = _worldPos;
            }
            if (_stateTimer >= 0.85f)
            {
                if (_eruptChainRemaining > 0)
                {
                    // Enraged: dives straight back under for a second strike.
                    _eruptChainRemaining--;
                    _state = State::Tunnelling; _stateTimer = 0.f;
                }
                else
                {
                    _state = State::Recovery; _stateTimer = 0.f;
                    _burrowCooldown = _burrowCooldownBase;
                    SetAnimation(_sharedIdleAnim, 1.f / 8.f, true);
                }
            }
            break;

        case State::Recovery:
            _stateTimer += dt;
            if (_stateTimer >= _recoveryDuration)
            {
                _state = State::Surface; _stateTimer = 0.f;
            }
            break;
        }

        if (_state == State::Surface || _state == State::Recovery)
            TryDealContactDamage();

        _worldPos.x = std::clamp(_worldPos.x, 180.f, (float)kVirtualWidth  - 180.f);
        _worldPos.y = std::clamp(_worldPos.y, 180.f, (float)kVirtualHeight - 180.f);
    }

    HandleAnimation(dt);
}

void ToxicVermin::DrawEliteTelegraph() const
{
    // Eruption warning: the burst point locked when the mound stopped; the
    // 0.5s ring is the read — step off the marker before it blows.
    if (_state == State::Erupting && !_eruptDamageApplied)
    {
        const float pulse = 0.7f + 0.3f * sinf((float)GetTime() * 11.f);
        DrawCircleV(_worldPos, _eruptRadius, Fade(Color{ 140, 220, 80, 255 }, 0.16f));
        DrawCircleLines((int)_worldPos.x, (int)_worldPos.y, _eruptRadius * pulse,
                        Fade(Color{ 170, 245, 100, 255 }, 0.7f));
        DrawCircleLines((int)_worldPos.x, (int)_worldPos.y, _eruptRadius,
                        Fade(Color{ 170, 245, 100, 255 }, 0.4f));
    }
    // The chasing mound itself stays visible so the player can track it.
    else if (_state == State::Tunnelling)
    {
        DrawCircleLines((int)_worldPos.x, (int)_worldPos.y, 55.f,
                        Fade(Color{ 140, 220, 80, 255 }, 0.45f));
    }
}

void ToxicVermin::DebugForceEliteSignature()
{
    if (_dying || !IsAlive() || _state != State::Surface)
        return;
    _burrowCooldown = 0.f;   // the signature IS the burrow-eruption chain
}

void ToxicVermin::DebugForceElitePhaseTwo()
{
    const float nextThreshold = (GetPhase() == 0) ? 0.65f : 0.32f;
    _health = std::min(_health, std::max(1.f, std::floor(_maxHealth * nextThreshold)));
}

const char* ToxicVermin::GetEliteSignatureStateName() const
{
    switch (_state)
    {
    case State::Burrowing:  return "Burrowing";
    case State::Tunnelling: return "Tunnelling";
    case State::Erupting:   return _eruptDamageApplied ? "Erupted" : "EruptWarning";
    case State::SpitCasting:return "SpitCasting";
    default:                return "Surface";
    }
}

void ToxicVermin::TryDealContactDamage()
{
    if (_target == nullptr || !_target->IsAlive()) return;
    if (_target->IsBeingForcedPushed()) return;
    if (_contactCooldown > 0.f) return;
    if (!CheckCollisionRecs(GetBodyContactRec(), _target->GetCollisionRec())) return;

    _target->TakeFractionalDamage(_bossDamagePerHit, _worldPos);
    _target->StartForcedPush(GetPushDirectionToPlayer(), _bossPushSpeed);
    _contactCooldown = _contactCooldownBase;
}

Rectangle ToxicVermin::GetBodyContactRec() const
{
    Rectangle bodyRec = GetCollisionRec();
    bodyRec.x += 26.f; bodyRec.y += 22.f;
    bodyRec.width  = std::max(1.f, bodyRec.width  - 52.f);
    bodyRec.height = std::max(1.f, bodyRec.height - 44.f);
    return bodyRec;
}

Vector2 ToxicVermin::GetPushDirectionToPlayer() const
{
    if (_target == nullptr) return Vector2{ 1.f, 0.f };
    Vector2 away = Vector2Subtract(_target->GetWorldPos(), _worldPos);
    return (Vector2Length(away) > 0.01f) ? Vector2Normalize(away) : Vector2{ 1.f, 0.f };
}

bool ToxicVermin::ConsumeImpactShakeRequest()
{
    bool requested = _impactShakeRequested;
    _impactShakeRequested = false;
    return requested;
}

void ToxicVermin::HandleAnimation(float dt)
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
            if (_state == State::Burrowing) { _frame = _maxFrames - 1; return; }
            _frame = 0;
        }
    }
}

void ToxicVermin::DrawEnemy(Vector2 cameraRef)
{
    if (!_isActive) return;

    float drawWidth  = _stableFrameW * _scale;
    float drawHeight = _stableFrameH * _scale;

    Vector2 screenPos = Vector2Subtract(_worldPos, cameraRef);
    screenPos.x += kVirtualWidth  / 2.f;
    screenPos.y += kVirtualHeight / 2.f;

    // Underground: only the travelling dirt mound is visible.
    if (_state == State::Tunnelling)
    {
        float pulse = sinf((float)GetTime() * 10.f) * 0.5f + 0.5f;
        DrawEllipse((int)screenPos.x, (int)(screenPos.y + 24.f), 66.f + pulse * 8.f, 30.f + pulse * 4.f,
            Fade(Color{ 120, 90, 50, 255 }, 0.75f));
        DrawEllipse((int)screenPos.x, (int)(screenPos.y + 18.f), 44.f + pulse * 6.f, 20.f,
            Fade(Color{ 160, 120, 70, 255 }, 0.85f));
        for (int i = 0; i < 3; i++)
        {
            float t = fmodf((float)GetTime() * 1.6f + i * 0.31f, 1.f);
            DrawCircleV(Vector2{ screenPos.x + sinf((float)GetTime() * 5.f + i * 2.f) * 30.f,
                                 screenPos.y + 10.f - t * 26.f },
                5.f - t * 3.f, Fade(Color{ 140, 105, 60, 255 }, 0.7f * (1.f - t)));
        }
        return;   // body hidden while underground
    }

    DrawEllipse((int)screenPos.x, (int)(screenPos.y + drawHeight * 0.50f),
        drawWidth * 0.30f, drawHeight * 0.10f, Fade(BLACK, 0.35f));

    bool burning = !_pendingBurns.empty();
    Color tint = IsElectroStunned() ? Color{ 255, 255,  60, 255 } :
                 IsFrozen()         ? Color{ 140, 200, 255, 255 } :
                 burning            ? Color{ 255, 180, 180, 255 } :
                 IsEnraged()        ? Color{ 200, 255, 170, 255 } :
                                      WHITE;

    Vector2 animDrawOffset = GetCurrentAnimDrawOffset();
    Rectangle source{ _frame * _width, 0.f, _rightLeft * _width, _height };
    Rectangle dest{ screenPos.x - drawWidth / 2.f + animDrawOffset.x,
                    screenPos.y - drawHeight / 2.f + animDrawOffset.y, drawWidth, drawHeight };
    DrawTexturePro(_texture, source, dest, Vector2{}, 0.f, tint);

    DrawHealthBar(screenPos, drawWidth, drawHeight);
}

Rectangle ToxicVermin::GetCollisionRec() const
{
    Rectangle animBodyRect;
    if (GetAnimBodyRectWorld(animBodyRect))
        return animBodyRect;
    if (_hasTunedCollision)
        return GetTunedCollisionRec();

    float halfW = _stableFrameW * _scale * 0.5f;
    float halfH = _stableFrameH * _scale * 0.5f;
    return Rectangle{ _worldPos.x - halfW * 0.56f, _worldPos.y - halfH * 0.48f, halfW * 1.12f, halfH * 1.05f };
}

Capsule2D ToxicVermin::GetCapsule() const
{
    Capsule2D animBodyCapsule;
    if (GetAnimBodyCapsuleWorld(animBodyCapsule))
        return animBodyCapsule;
    if (_capsuleRadius > 0.f)
        return Capsule2D{ { _worldPos.x + _capsuleOffset.x, _worldPos.y + _capsuleOffset.y }, _capsuleHalfHeight, _capsuleRadius };
    return Capsule2D{ { _worldPos.x, _worldPos.y + 12.f }, 0.f, _stableFrameW * _scale * 0.28f };
}

void ToxicVermin::TakeDamage(int damage, Vector2 attackerPos)
{
    (void)attackerPos;
    if (_dying || _hitTimer > 0.f) return;

    // Untouchable underground.
    if (_state == State::Tunnelling || _state == State::Burrowing)
        return;

    _health -= (float)damage;
    if (_health > 0.f) PlayHurtSound();

    if (_health <= 0.f)
    {
        _health = 0.f; _dying = true;
        _takingDamage = false; _attacking = false;
        _pendingSpit = false;
        _state = State::Recovery;
        _deathTimer = 0.7f;
        SetAnimation(_sharedDeathAnim, 1.f / 6.f, true);
        PlayDeathSound();
        return;
    }

    _velocity = Vector2Zero();
    if (_state == State::Surface)
    {
        _takingDamage = true;
        SetAnimation(_sharedHurtAnim, 1.f / 12.f, true);
    }
    _hitTimer = 0.02f;
}

void ToxicVermin::ApplyFreeze(float duration)
{
    if (_dying || !IsAlive()) return;
    if (_state == State::Tunnelling || _state == State::Burrowing) return;
    if (duration > _freezeTimer) _freezeTimer = duration;
}

void ToxicVermin::SetWaveScale(int wave)
{
    (void)wave;
    _expValue = _bossBaseExpValue;
    _health = Balance::Boss::kToxicVerminHealth; _maxHealth = Balance::Boss::kToxicVerminHealth;
_enrageThreshold = 0.40f;
    _speed = _surfaceSpeed; _attackPower = 1.f;
}

void ToxicVermin::EnsureSharedResourcesLoaded()
{
    if (_sharedResourcesLoaded) return;

    _sharedIdleAnim   = LoadTexture(AssetPath("Bosses/ToxicVerminIdle.png").c_str());
    _sharedWalkAnim   = LoadTexture(AssetPath("Bosses/ToxicVerminWalk.png").c_str());
    _sharedMeleeAnim  = LoadTexture(AssetPath("Bosses/ToxicVerminMeleeAttack.png").c_str());
    _sharedSpitAnim   = LoadTexture(AssetPath("Bosses/ToxicVerminSpit.png").c_str());
    _sharedBurrowAnim = LoadTexture(AssetPath("Bosses/ToxicVerminBurrow.png").c_str());
    _sharedEmergeAnim = LoadTexture(AssetPath("Bosses/ToxicVerminEmerge.png").c_str());
    _sharedHurtAnim   = LoadTexture(AssetPath("Bosses/ToxicVerminHurt.png").c_str());
    _sharedDeathAnim  = LoadTexture(AssetPath("Bosses/ToxicVerminDeath.png").c_str());
    _sharedAttackSound = LoadSound(AssetPath("Sounds/SwordSwipe2.ogg").c_str());
    _sharedHurtSound   = LoadSound(AssetPath("Sounds/SmallMonsterDamage.ogg").c_str());
    _sharedDeathSound  = LoadSound(AssetPath("Sounds/MonsterDeath.ogg").c_str());
    _sharedBurrowSound = LoadSound(AssetPath("Sounds/OgreHitWall.ogg").c_str());
    _sharedResourcesLoaded = true;
}

void ToxicVermin::UnloadSharedResources()
{
    if (!_sharedResourcesLoaded) return;

    UnloadTexture(_sharedIdleAnim);   UnloadTexture(_sharedWalkAnim);
    UnloadTexture(_sharedMeleeAnim);  UnloadTexture(_sharedSpitAnim);
    UnloadTexture(_sharedBurrowAnim); UnloadTexture(_sharedEmergeAnim);
    UnloadTexture(_sharedHurtAnim);   UnloadTexture(_sharedDeathAnim);
    UnloadSound(_sharedAttackSound);  UnloadSound(_sharedHurtSound);
    UnloadSound(_sharedDeathSound);   UnloadSound(_sharedBurrowSound);
    _sharedIdleAnim = Texture2D{};   _sharedWalkAnim = Texture2D{};
    _sharedMeleeAnim = Texture2D{};  _sharedSpitAnim = Texture2D{};
    _sharedBurrowAnim = Texture2D{}; _sharedEmergeAnim = Texture2D{};
    _sharedHurtAnim = Texture2D{};   _sharedDeathAnim = Texture2D{};
    _sharedAttackSound = Sound{};    _sharedHurtSound = Sound{};
    _sharedDeathSound = Sound{};     _sharedBurrowSound = Sound{};
    _sharedResourcesLoaded = false;
}
