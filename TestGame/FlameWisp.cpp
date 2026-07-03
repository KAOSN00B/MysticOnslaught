#include "FlameWisp.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

// ---- Static member definitions ----------------------------------------------
Texture2D FlameWisp::_sharedIdleAnim{};
Texture2D FlameWisp::_sharedWalkAnim{};
Texture2D FlameWisp::_sharedAttackAnim{};
Texture2D FlameWisp::_sharedTakeDamageAnim{};
Texture2D FlameWisp::_sharedDeathAnim{};
Sound     FlameWisp::_sharedAttackSound{};
Sound     FlameWisp::_sharedHurtSound{};
Sound     FlameWisp::_sharedDeathSound{};
bool      FlameWisp::_sharedResourcesLoaded = false;

namespace
{
    // All wisp sheets are stitched 6-frame strips of 32x32 pixels.
    constexpr int   kWispFrameCount = 6;
    constexpr float kWispFrameWidth = 32.f;
    constexpr float kWispFrameTime  = 1.f / 10.f;
}

// =============================================================================
FlameWisp::FlameWisp(Vector2 pos)
    : Enemy(pos)
{
}

FlameWisp::~FlameWisp() {}

// =============================================================================
void FlameWisp::Init()
{
    EnsureSharedResourcesLoaded();

    _idleAnim       = _sharedIdleAnim;
    _walkAnim       = _sharedWalkAnim;
    _attackAnim     = _sharedAttackAnim;
    _takeDamageAnim = _sharedTakeDamageAnim;
    _deathAnim      = _sharedDeathAnim;
    _attackSound    = _sharedAttackSound;
    _hurtSound      = _sharedHurtSound;
    _deathSound     = _sharedDeathSound;

    ResetForSpawn(_worldPos);
}

// =============================================================================
void FlameWisp::ResetForSpawn(Vector2 pos)
{
    _worldPos          = pos;
    _worldPosLastFrame = pos;
    _homePos           = pos;
    _velocity          = Vector2Zero();
    _isActive          = true;

    SetIdleAnimation(false);
    _scale = 4.8f;

    _health      = 2.f;
    _maxHealth   = 2.f;
    _attackPower = 1.f;
    _speed       = 120.f;   // drift speed — teleports cover real distance
    _expValue    = 4;

    _frame       = GetRandomValue(0, _maxFrames - 1);
    _runningTime = GetRandomValue(0, 200) / 100.f * _updateTime;

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

    _state            = WispState::Drifting;
    _stateTimer       = 0.f;
    _teleportCooldown = (float)GetRandomValue(120, 260) / 100.f;   // stagger first teleports
    _hoverBobTimer    = (float)GetRandomValue(0, 628) / 100.f;
    _teleportTarget   = pos;
    _wantsToCast      = false;
    _castDirection    = Vector2Zero();

    _castWindupInst       = 0.55f;
    _teleportCooldownInst = 3.4f;

    _forcedPushActive    = false;
    _forcedPushDirection = Vector2Zero();
    _forcedPushSpeed     = 0.f;

    _pendingBurns.clear();
    _waypoints.clear();
    _waypointIndex = 0;
}

void FlameWisp::SetIdleAnimation(bool resetFrame)
{
    _texture    = _idleAnim;
    _width      = kWispFrameWidth;
    _height     = _idleAnim.height;
    _updateTime = kWispFrameTime;
    _maxFrames  = kWispFrameCount;
    if (resetFrame) { _frame = 0; _runningTime = 0.f; }
}

// =============================================================================
void FlameWisp::Update(float dt, Vector2 heroWorldPos,
                       Vector2 /*navigationTarget*/, bool /*hasNavigationTarget*/,
                       const std::vector<std::unique_ptr<Enemy>>& /*enemies*/,
                       const std::vector<Vector2>& /*propCenters*/)
{
    if (!_isActive)
        return;

    _worldPosLastFrame = _worldPos;
    _hoverBobTimer += dt;

    if (_forcedPushActive)
    {
        _worldPos = Vector2Add(_worldPos, Vector2Scale(_forcedPushDirection, _forcedPushSpeed * dt));
        return;
    }

    ApplyVelocity(dt);
    UpdateHit(dt);
    UpdateBurns(dt);
    UpdateElectricCharge(dt);
    UpdateLaunchVisual(dt);

    if (_freezeTimer > 0.f)
        _freezeTimer -= dt;

    if (!_dying)
    {
        if (_target == nullptr)
            return;

        if (_teleportCooldown > 0.f)
            _teleportCooldown -= dt;

        // Crowd control interrupts the teleport/cast cycle.
        bool controlled = IsFrozen() || IsElectroStunned() || _takingDamage;

        switch (_state)
        {
        case WispState::Drifting:
        {
            // Lazy float toward the player with a sine bob.
            Vector2 toPlayer = Vector2Subtract(heroWorldPos, _worldPos);
            float dist = Vector2Length(toPlayer);
            if (dist > 240.f && !controlled)
            {
                Vector2 drift = Vector2Scale(Vector2Normalize(toPlayer), _speed * dt);
                drift.y += sinf(_hoverBobTimer * 2.4f) * 26.f * dt;
                _worldPos = Vector2Add(_worldPos, drift);
            }

            if (_teleportCooldown <= 0.f && !controlled)
            {
                _state          = WispState::TeleportOut;
                _stateTimer     = 0.f;
                _teleportTarget = PickTeleportSpot();
            }
            break;
        }

        case WispState::TeleportOut:
            _stateTimer += dt;
            if (_stateTimer >= _teleportOutDuration)
            {
                _worldPos   = _teleportTarget;
                _state      = WispState::TeleportIn;
                _stateTimer = 0.f;
            }
            break;

        case WispState::TeleportIn:
            _stateTimer += dt;
            if (_stateTimer >= _teleportInDuration)
            {
                _state      = WispState::Casting;
                _stateTimer = 0.f;
            }
            break;

        case WispState::Casting:
            if (controlled)
            {
                // Interrupted — pay half the cooldown and go back to drifting.
                _state            = WispState::Drifting;
                _teleportCooldown = _teleportCooldownInst * 0.5f;
                break;
            }
            _stateTimer += dt;
            if (_stateTimer >= _castWindupInst)
            {
                Vector2 toTarget = Vector2Subtract(_target->GetFeetWorldPos(), _worldPos);
                _wantsToCast   = true;
                _castDirection = (Vector2LengthSqr(toTarget) > 0.0001f)
                    ? Vector2Normalize(toTarget)
                    : Vector2{ 1.f, 0.f };
                _state            = WispState::Drifting;
                _teleportCooldown = _teleportCooldownInst;
            }
            break;
        }

        // Face the player at all times.
        float dx = heroWorldPos.x - _worldPos.x;
        if      (dx < -20.f) _rightLeft = -1.f;
        else if (dx >  20.f) _rightLeft =  1.f;
    }

    HandleAnimation(dt);
}

Vector2 FlameWisp::PickTeleportSpot() const
{
    // Random ring position around the player, clamped inside the room.
    Vector2 playerPos = (_target != nullptr) ? _target->GetFeetWorldPos() : _worldPos;
    float angle  = (float)GetRandomValue(0, 628) / 100.f;
    float radius = (float)GetRandomValue((int)_teleportRadiusMin, (int)_teleportRadiusMax);
    Vector2 spot{
        playerPos.x + cosf(angle) * radius,
        playerPos.y + sinf(angle) * radius
    };
    spot.x = std::clamp(spot.x, 170.f, (float)kVirtualWidth  - 170.f);
    spot.y = std::clamp(spot.y, 170.f, (float)kVirtualHeight - 170.f);
    return spot;
}

// =============================================================================
void FlameWisp::HandleAnimation(float dt)
{
    if (_launchHoldingHurtPose)
        return;

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
                SetIdleAnimation(true);
                return;
            }
            _frame = 0;
        }
    }
}

// =============================================================================
void FlameWisp::DrawEnemy(Vector2 cameraRef)
{
    if (!_isActive)
        return;

    float drawWidth  = kWispFrameWidth * _scale;
    float drawHeight = (_idleAnim.id > 0 ? (float)_idleAnim.height : _height) * _scale;

    // Teleport shrink: scale down while phasing out, up while phasing in.
    float teleportScale = 1.f;
    if (_state == WispState::TeleportOut)
        teleportScale = 1.f - (_stateTimer / _teleportOutDuration) * 0.92f;
    else if (_state == WispState::TeleportIn)
        teleportScale = 0.08f + (_stateTimer / _teleportInDuration) * 0.92f;
    drawWidth  *= teleportScale;
    drawHeight *= teleportScale;

    Vector2 screenPos = Vector2Subtract(_worldPos, cameraRef);
    screenPos.x += kVirtualWidth  / 2.f;
    screenPos.y += kVirtualHeight / 2.f + sinf(_hoverBobTimer * 2.4f) * 7.f;

    bool frozen         = IsFrozen();
    bool electroStunned = IsElectroStunned();

    Color tint = electroStunned ? Color{ 255, 255,  60, 255 } :
                 frozen         ? Color{ 140, 200, 255, 255 } :
                                  WHITE;

    // Casting glow telegraph — brightening ring before the fire bolt.
    if (_state == WispState::Casting)
    {
        float castRatio = _stateTimer / _castWindupInst;
        float pulse = sinf((float)GetTime() * 16.f) * 0.5f + 0.5f;
        DrawCircleV(screenPos, 26.f + castRatio * 34.f, Fade(Color{ 255, 120, 30, 255 }, 0.20f + 0.15f * pulse));
        DrawCircleV(screenPos, 12.f + castRatio * 18.f, Fade(Color{ 255, 210, 90, 255 }, 0.30f + 0.20f * pulse));
    }

    // Soft ember glow at all times so the wisp reads as a light source.
    DrawCircleV(screenPos, drawWidth * 0.42f, Fade(Color{ 255, 120, 30, 255 }, 0.14f));

    Rectangle source{ _frame * _width, 0.f, _rightLeft * _width, _height };
    Rectangle dest{ screenPos.x - drawWidth / 2.f, screenPos.y - drawHeight / 2.f, drawWidth, drawHeight };
    DrawTexturePro(_texture, source, dest, Vector2{}, 0.f, tint);

    if (_graveReviveInvulTimer > 0.f)
    {
        float pulse = sinf((float)GetTime() * 10.f) * 0.4f + 0.6f;
        DrawCircleLines((int)screenPos.x, (int)screenPos.y, 50.f, Fade(Color{ 80, 255, 120, 255 }, pulse));
    }

    if (_health != _maxHealth)
        DrawHealthBar(screenPos, kWispFrameWidth * _scale, (float)_idleAnim.height * _scale);
    if (_isEliteMiniboss)
        DrawEliteLabel(screenPos, kWispFrameWidth * _scale, (float)_idleAnim.height * _scale);
}

Rectangle FlameWisp::GetCollisionRec() const
{
    float stableHalfW = kWispFrameWidth * _scale * 0.5f;
    float stableHalfH = (_idleAnim.id > 0 ? (float)_idleAnim.height : _height) * _scale * 0.5f;

    if (_collisionSize.x == 0.f && stableHalfW > 0.f)
    {
        auto* s = const_cast<FlameWisp*>(this);
        s->_collisionSize   = { 70.f, 78.f };
        s->_collisionOffset = { stableHalfW - 35.f, stableHalfH - 39.f };
    }
    return Rectangle{
        _worldPos.x - stableHalfW + _collisionOffset.x,
        _worldPos.y - stableHalfH + _collisionOffset.y,
        _collisionSize.x, _collisionSize.y
    };
}

Capsule2D FlameWisp::GetCapsule() const
{
    if (_capsuleRadius == 0.f)
    {
        auto* s = const_cast<FlameWisp*>(this);
        s->_capsuleRadius     = 26.f;
        s->_capsuleHalfHeight = 0.f;
        s->_capsuleOffset     = { 0.f, 4.f };
    }
    return Capsule2D{
        { _worldPos.x + _capsuleOffset.x, _worldPos.y + _capsuleOffset.y },
        _capsuleHalfHeight,
        _capsuleRadius
    };
}

void FlameWisp::OnBoltCast()
{
    _wantsToCast   = false;
    _castDirection = Vector2Zero();
}

// =============================================================================
void FlameWisp::SetWaveScale(int wave)
{
    _expValue    = 4;
    _health      = 2.f;
    _maxHealth   = 2.f;
    _speed       = 120.f;
    _attackPower = 1.f;

    // Later rooms teleport (and therefore attack) more often.
    int tier = (wave - 1) / 5;
    _teleportCooldownInst = std::max(2.2f, 3.4f - tier * 0.22f);
    _castWindupInst       = std::max(0.40f, 0.55f - tier * 0.03f);
}

// =============================================================================
void FlameWisp::PlayAttackSound()
{
    float pitch = GetRandomValue(140, 180) / 100.f;
    SetSoundPitch(_attackSound, pitch);
    SetSoundVolume(_attackSound, 0.55f);
    PlaySound(_attackSound);
}

// =============================================================================
void FlameWisp::EnsureSharedResourcesLoaded()
{
    if (_sharedResourcesLoaded)
        return;

    _sharedIdleAnim       = LoadTexture(AssetPath("Enemy/FlameWispIdle.png").c_str());
    _sharedWalkAnim       = LoadTexture(AssetPath("Enemy/FlameWispWalk.png").c_str());
    _sharedAttackAnim     = LoadTexture(AssetPath("Enemy/FlameWispAttack.png").c_str());
    _sharedTakeDamageAnim = LoadTexture(AssetPath("Enemy/FlameWispHurt.png").c_str());
    _sharedDeathAnim      = LoadTexture(AssetPath("Enemy/FlameWispDeath.png").c_str());
    _sharedAttackSound    = LoadSound(AssetPath("Sounds/GS1_Spell_Fire.ogg").c_str());
    _sharedHurtSound      = LoadSound(AssetPath("Sounds/SmallMonsterDamage.ogg").c_str());
    _sharedDeathSound     = LoadSound(AssetPath("Sounds/PlayerDeath.ogg").c_str());
    _sharedResourcesLoaded = true;
}

void FlameWisp::UnloadSharedResources()
{
    if (!_sharedResourcesLoaded)
        return;

    UnloadTexture(_sharedIdleAnim);
    UnloadTexture(_sharedWalkAnim);
    UnloadTexture(_sharedAttackAnim);
    UnloadTexture(_sharedTakeDamageAnim);
    UnloadTexture(_sharedDeathAnim);
    UnloadSound(_sharedAttackSound);
    UnloadSound(_sharedHurtSound);
    UnloadSound(_sharedDeathSound);

    _sharedIdleAnim       = Texture2D{};
    _sharedWalkAnim       = Texture2D{};
    _sharedAttackAnim     = Texture2D{};
    _sharedTakeDamageAnim = Texture2D{};
    _sharedDeathAnim      = Texture2D{};
    _sharedAttackSound    = Sound{};
    _sharedHurtSound      = Sound{};
    _sharedDeathSound     = Sound{};
    _sharedResourcesLoaded = false;
}
