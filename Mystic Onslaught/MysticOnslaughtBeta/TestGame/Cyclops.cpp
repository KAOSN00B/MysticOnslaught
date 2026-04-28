#include "Cyclops.h"
#include "AssetPaths.h"
#include "raymath.h"
#include <algorithm>

// ---- Static member definitions ----------------------------------------------
Texture2D Cyclops::_sharedIdleAnim{};
Texture2D Cyclops::_sharedWalkAnim{};
Texture2D Cyclops::_sharedAttackAnim{};
Texture2D Cyclops::_sharedTakeDamageAnim{};
Texture2D Cyclops::_sharedDeathAnim{};
Sound     Cyclops::_sharedAttackSound{};
Sound     Cyclops::_sharedHurtSound{};
Sound     Cyclops::_sharedDeathSound{};
bool      Cyclops::_sharedResourcesLoaded = false;

namespace
{
    // The cyclops idle and walk sheets use different frame widths, so the
    // state transitions need named values instead of repeating raw numbers.
    // These sheets are six frames wide but the art is not authored in exact
    // 28px / 30px slices. Using the true sheet-width / frame-count values keeps
    // DrawTexturePro from bleeding into the next frame near the end of the loop.
    constexpr int   kCyclopsFrameCount     = 6;
    constexpr float kCyclopsIdleFrameWidth = 171.0f / 6.0f;
    constexpr float kCyclopsWalkFrameWidth = 181.0f / 6.0f;

    // Idle plays a little slower than walk so the charge pose reads clearly.
    constexpr float kCyclopsIdleFrameTime = 1.f / 10.f;
    constexpr float kCyclopsWalkFrameTime = 1.f / 8.f;

    // The death sound was reading lower than the hurt sound. This slightly
    // higher range keeps the death cue consistent with the cyclops voice.
    constexpr int kCyclopsDeathPitchMin = 120;
    constexpr int kCyclopsDeathPitchMax = 155;
    constexpr float kLaserSightLength = 2400.f;
}

// =============================================================================
Cyclops::Cyclops(Vector2 pos)
    : Enemy(pos)
{
}

Cyclops::~Cyclops() {}

// =============================================================================
void Cyclops::Init()
{
    EnsureSharedResourcesLoaded();

    _idleAnim        = _sharedIdleAnim;
    _walkAnim        = _sharedWalkAnim;
    _attackAnim      = _sharedAttackAnim;
    _takeDamageAnim  = _sharedTakeDamageAnim;
    _deathAnim       = _sharedDeathAnim;
    _attackSound     = _sharedAttackSound;
    _hurtSound       = _sharedHurtSound;
    _deathSound      = _sharedDeathSound;

    ResetForSpawn(_worldPos);
}

// =============================================================================
void Cyclops::ResetForSpawn(Vector2 pos)
{
    _worldPos          = pos;
    _worldPosLastFrame = pos;
    _homePos           = pos;
    _velocity          = Vector2Zero();
    _isActive          = true;

    // Spawn starts in the idle sheet so the first rendered frame uses the
    // correct source-rectangle dimensions for the standing pose.
    SetIdleAnimation(false);
    _scale = 5.0f;

    _health      = 6.f;
    _maxHealth   = 6.f;
    _attackPower = 2.f;
    _speed       = 200.f;
    _expValue    = 5; // 5 exp — ranged threat, worth more than a regular enemy

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

    _stuckTimer    = 0.f;
    _stuckCheckPos = _worldPos;

    _chargeTimer    = 0.f;
    _attackCooldown = 0.f;
    _charging       = false;
    _wantsToFire    = false;
    _fireDirection  = Vector2Zero();
    _queuedFireMode = FireMode::Sweep;
    _postFireLockTimer = 0.f;
    // Reset timing to design defaults; SetWaveScale will override these.
    _chargeDurationInst    = 1.0f;
    _attackCooldownMaxInst = 3.5f;
    _chargeRangeInst       = 480.f;
    _scatterRangeInst      = 170.f;

    _pendingBurns.clear();
}

void Cyclops::SetIdleAnimation(bool resetFrame)
{
    // Charge, recovery, and standing states all share the same idle sheet.
    // Keeping this in one helper prevents the cyclops from carrying walk-frame
    // dimensions into the idle animation, which was causing the sideways loop.
    _texture    = _idleAnim;
    _width      = kCyclopsIdleFrameWidth;
    _height     = _idleAnim.height;
    _updateTime = kCyclopsIdleFrameTime;
    _maxFrames  = kCyclopsFrameCount;

    if (resetFrame)
    {
        _frame       = 0;
        _runningTime = 0.f;
    }
}

void Cyclops::SetWalkAnimation(bool resetFrame)
{
    // Movement uses a separate sheet with a different frame width.
    // Resetting when we switch sheets keeps the source rectangle aligned with
    // the walk frames instead of reusing an idle-frame index.
    _texture    = _walkAnim;
    _width      = kCyclopsWalkFrameWidth;
    _height     = _walkAnim.height;
    _updateTime = kCyclopsWalkFrameTime;
    _maxFrames  = kCyclopsFrameCount;

    if (resetFrame)
    {
        _frame       = 0;
        _runningTime = 0.f;
    }
}

// =============================================================================
// Update mirrors Enemy::Update but with charge-and-fire logic for the laser.
// =============================================================================
void Cyclops::Update(float dt, Vector2 heroWorldPos,
                     Vector2 navigationTarget, bool hasNavigationTarget,
                     const std::vector<std::unique_ptr<Enemy>>& enemies,
                     const std::vector<Vector2>& propCenters)
{
    if (!_isActive)
        return;

    _worldPosLastFrame = _worldPos;

    // Ogre forced push — slide until Engine::HandleCollisions stops us.
    if (_forcedPushActive)
    {
        _worldPos = Vector2Add(_worldPos, Vector2Scale(_forcedPushDirection, _forcedPushSpeed * dt));
        return;
    }

    // Lock position during charge — prevents knockback impulses from
    // displacing the sprite while the laser telegraph is playing.
    if (_charging || _wantsToFire)
        _velocity = Vector2Zero();

    ApplyVelocity(dt);
    UpdateHit(dt);
    UpdateBurns(dt);
    UpdateElectricCharge(dt);
    UpdateLaunchVisual(dt);

    if (_freezeTimer > 0.f)
        _freezeTimer -= dt;
    if (_postFireLockTimer > 0.f)
        _postFireLockTimer -= dt;

    if (!_dying)
    {
        if (_target == nullptr)
            return;

        // Count down between laser shots so the engine only sees fire
        // requests when the cyclops has completed its charge.
        if (_attackCooldown > 0.f)
            _attackCooldown -= dt;

        // Charging only begins when the player is close enough and the
        // pathfinding layer reports direct line of sight.
        if (!_charging && !_takingDamage)
        {
            float distToPlayer = Vector2Distance(_worldPos, heroWorldPos);
            bool  hasLOS       = !hasNavigationTarget;

            if (_attackCooldown <= 0.f && distToPlayer < _chargeRangeInst && hasLOS)
            {
                _charging    = true;
                _chargeTimer = 0.f;

                // Force the charge to start from the idle sheet so the cyclops
                // animates in place instead of freezing a walk frame.
                SetIdleAnimation(true);
            }
        }

        if (_charging && (IsFrozen() || IsElectroStunned()))
        {
            CancelCharge();
        }

        if (_charging)
        {
            _chargeTimer += dt;
            if (_chargeTimer >= _chargeDurationInst)
            {
                const Vector2 toTarget = Vector2Subtract(_target->GetFeetWorldPos(), _worldPos);
                const float distToPlayer = Vector2Length(toTarget);
                _wantsToFire    = true;
                _queuedFireMode = (distToPlayer <= _scatterRangeInst) ? FireMode::Scatter : FireMode::Sweep;
                _fireDirection  = (distToPlayer > 0.01f)
                    ? Vector2Normalize(toTarget)
                    : Vector2{ _rightLeft >= 0.f ? 1.f : -1.f, 0.f };
                _charging       = false;
                _attackCooldown = _attackCooldownMaxInst;

                // Hold the idle pose on the release frame as well. This keeps
                // the laser attack from being visually overwritten by movement
                // before Engine consumes WantsToFire().
                SetIdleAnimation(true);
            }
        }

        HandleMovement(dt, navigationTarget, hasNavigationTarget, enemies, propCenters);
    }

    HandleAnimation(dt);
}

// =============================================================================
// HandleMovement mirrors Enemy::HandleMovement, but charging and firing lock
// the cyclops in place so the ranged attack pose is readable.
// =============================================================================
void Cyclops::HandleMovement(float dt, Vector2 navigationTarget, bool hasNavigationTarget,
                             const std::vector<std::unique_ptr<Enemy>>& /*enemies*/,
                             const std::vector<Vector2>& propCenters)
{
    if (_target == nullptr || _dying)
        return;

    // Stand still and face the player while charging or on the frame that the
    // laser is emitted. That prevents movement logic from immediately swapping
    // the charge/release pose back to walk.
    if (_charging || _wantsToFire || _postFireLockTimer > 0.f)
    {
        _velocity = Vector2Zero();
        SetIdleAnimation(_texture.id != _idleAnim.id);

        float dx = _target->GetFeetWorldPos().x - _worldPos.x;
        if      (dx < -20.f) _rightLeft = -1.f;
        else if (dx >  20.f) _rightLeft =  1.f;
        return;
    }

    Vector2 moveDir     = Vector2Zero();
    float   effectSpeed = _speed;

    // The engine passes either a navigation waypoint or a direct target
    // position. The cyclops uses whichever option is currently valid.
    Vector2 targetPos = hasNavigationTarget ? navigationTarget : _target->GetFeetWorldPos();
    Vector2 toTarget  = Vector2Subtract(targetPos, _worldPos);
    if (Vector2Length(toTarget) > 0.01f)
        moveDir = Vector2Normalize(toTarget);

    // Nearby props push the cyclops away so it slides around pillars instead
    // of tunneling straight into them.
    Vector2 separation = Vector2Zero();
    for (const Vector2& propCenter : propCenters)
    {
        float dist = Vector2Distance(_worldPos, propCenter);
        if (dist < 110.f && dist > 0.f)
        {
            Vector2 away = Vector2Subtract(_worldPos, propCenter);
            if (Vector2Length(away) > 0.01f)
            {
                float strength = (110.f - dist) / 110.f;
                separation = Vector2Add(separation,
                    Vector2Scale(Vector2Normalize(away), strength * 1.8f));
            }
        }
    }

    separation = Vector2Scale(separation, 0.6f);
    moveDir    = Vector2Add(moveDir, separation);

    if (Vector2Length(moveDir) > 0.01f)
        moveDir = Vector2Normalize(moveDir);

    Vector2 oldPos = _worldPos;

    if (!_attacking && !_takingDamage && !IsFrozen())
        _worldPos = Vector2Add(_worldPos, Vector2Scale(moveDir, effectSpeed * dt));

    // If the cyclops has barely moved for a short time, inject a perpendicular
    // impulse to help it slide off corners and dense prop clusters.
    if (!_attacking && !_takingDamage && !IsFrozen())
    {
        _stuckTimer += dt;
        if (_stuckTimer >= _stuckThreshold)
        {
            float moved = Vector2Distance(_worldPos, _stuckCheckPos);
            if (moved < _stuckMinMove)
            {
                Vector2 perp = { -moveDir.y, moveDir.x };
                float sign   = (GetRandomValue(0, 1) == 0) ? 1.f : -1.f;
                _velocity    = Vector2Add(_velocity,
                    Vector2Scale(perp, sign * _speed * 1.2f));
            }

            _stuckTimer    = 0.f;
            _stuckCheckPos = _worldPos;
        }
    }
    else
    {
        _stuckTimer    = 0.f;
        _stuckCheckPos = _worldPos;
    }

    Vector2 movement = Vector2Subtract(_worldPos, oldPos);

    if (!_attacking && !_takingDamage)
    {
        if (Vector2Length(movement) > 0.01f)
            SetWalkAnimation(_texture.id != _walkAnim.id);
        else
            SetIdleAnimation(_texture.id != _idleAnim.id);

        // Debounced facing keeps the sprite from rapidly flipping due to prop
        // repulsion or tiny path adjustments around the player.
        float dx     = _target->GetFeetWorldPos().x - _worldPos.x;
        float wanted = _rightLeft;

        if      (dx < -20.f) wanted = -1.f;
        else if (dx >  20.f) wanted =  1.f;

        if (wanted != _rightLeft)
        {
            _facingTimer += dt;
            if (_facingTimer >= 0.15f)
            {
                _rightLeft   = wanted;
                _facingTimer = 0.f;
            }
        }
        else
        {
            _facingTimer = 0.f;
        }
    }
}

// =============================================================================
// HandleAnimation advances the current sprite sheet using the metadata set by
// the current movement, charge, hurt, or death state.
// =============================================================================
void Cyclops::HandleAnimation(float dt)
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
            if (_dying)
            {
                _frame = _maxFrames - 1;
                return;
            }

            if (IsFrozen())
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

            if (_attacking)
            {
                _attacking = false;
                SetIdleAnimation(true);
            }

            _frame = 0;
        }
    }
}

// =============================================================================
// DrawCyclops uses the active animation sheet plus burn/freeze tint overlays.
// =============================================================================
void Cyclops::DrawEnemy(Vector2 cameraRef)
{
    if (!_isActive)
        return;

    float w = _width * _scale;
    float h = _height * _scale;

    // Cyclops uses the same launch visual response as the base enemy so ogre
    // rushes can visibly throw it upward without needing a separate effect
    // system for each enemy subclass.
    float launchRatio = (_launchVisualDuration > 0.f)
        ? (_launchVisualTimer / _launchVisualDuration)
        : 0.f;
    float launchScale = 1.f + _launchVisualScaleBoost * launchRatio;
    float launchLift = _launchVisualLift * launchRatio;
    w *= launchScale;
    h *= launchScale;

    Vector2 screenPos = Vector2Subtract(_worldPos, cameraRef);
    screenPos.x += GetScreenWidth()  / 2.f;
    screenPos.y += GetScreenHeight() / 2.f - launchLift;

    bool burning       = !_pendingBurns.empty();
    bool frozen        = IsFrozen();
    bool electroStunned = IsElectroStunned();

    Color tint = electroStunned ? Color{ 255, 255,  60, 255 } :
                 frozen         ? Color{ 140, 200, 255, 255 } :
                 burning        ? Color{ 255, 180, 180, 255 } :
                                  WHITE;

    if (burning && !frozen)
    {
        for (int i = 0; i < 3; ++i)
        {
            float fx = (float)GetRandomValue(-14, 14);
            float fy = (float)GetRandomValue(-26, -4);
            DrawCircleV({ screenPos.x + fx, screenPos.y + fy }, 5.f, Fade(ORANGE, 0.55f));
            DrawCircleV({ screenPos.x + fx * 0.7f, screenPos.y + fy - 6.f }, 3.f, Fade(YELLOW, 0.45f));
        }
    }

    // Charge flash grows as the laser nears release so the ranged attack is
    // readable before the projectile is spawned by Engine.
    if (_charging)
    {
        float pulse       = sinf((float)GetTime() * 14.f) * 0.5f + 0.5f;
        float chargeRatio = _chargeTimer / _chargeDurationInst;
        float radius      = 30.f + chargeRatio * 70.f;
        Vector2 aimDir = (_target != nullptr)
            ? Vector2Normalize(Vector2Subtract(_target->GetFeetWorldPos(), _worldPos))
            : Vector2{ _rightLeft >= 0.f ? 1.f : -1.f, 0.f };
        if (Vector2LengthSqr(aimDir) < 0.0001f)
            aimDir = Vector2{ _rightLeft >= 0.f ? 1.f : -1.f, 0.f };
        Vector2 lineEnd = Vector2Add(screenPos, Vector2Scale(aimDir, kLaserSightLength));

        DrawLineEx(screenPos, lineEnd, 8.f, Fade(Color{ 255, 80, 80, 255 }, 0.18f + 0.08f * pulse));
        DrawLineEx(screenPos, lineEnd, 3.8f, Fade(Color{ 255, 120, 120, 255 }, 0.72f + 0.10f * pulse));

        DrawCircleV(screenPos, radius * 1.35f, Fade(Color{ 255, 200, 50, 255 }, 0.18f * pulse));
        DrawCircleV(screenPos, radius,          Fade(Color{ 255, 230, 80, 255 }, 0.35f * pulse));
        DrawCircleV(screenPos, radius * 0.45f,  Fade(WHITE,                      0.55f * pulse));
    }

    Rectangle source{ _frame * _width, 0.f, _rightLeft * _width, _height };
    Rectangle dest{ screenPos.x - w / 2.f, screenPos.y - h / 2.f, w, h };

    DrawTexturePro(_texture, source, dest, Vector2{}, 0.f, tint);

    if (_health != _maxHealth)
        DrawHealthBar(screenPos, w, h);
    if (_isEliteMiniboss)
        DrawEliteLabel(screenPos, w, h);
}

Rectangle Cyclops::GetCollisionRec() const
{
    // Narrow body-only collision so the player can walk up to melee range
    // without being stopped by the wide sprite extents.
    const float width  = _width  * _scale * 0.26f;
    const float height = _height * _scale * 0.36f;

    return Rectangle{
        _worldPos.x - width  * 0.5f,
        _worldPos.y - height * 0.5f + (_height * _scale * 0.22f),
        width,
        height
    };
}

Rectangle Cyclops::GetHitCollisionRec() const
{
    // Wider than the solid collision rect so melee attacks register from any
    // angle including from behind while the narrow solid rect still lets the
    // player walk close without being blocked.
    const float width  = _width  * _scale * 0.68f;
    const float height = _height * _scale * 0.55f;

    return Rectangle{
        _worldPos.x - width  * 0.5f,
        _worldPos.y - height * 0.5f + (_height * _scale * 0.22f),
        width,
        height
    };
}

// =============================================================================
void Cyclops::DrawHealthBar(Vector2 screenPos, float w, float h)
{
    if (_health <= 0)
        return;

    float healthPercent = _health / _maxHealth;
    float barWidth      = w * 0.8f;
    float barHeight     = 6.f;
    float barX          = screenPos.x - barWidth / 2.f;
    float barY          = screenPos.y - h / 2.f - 12.f;

    DrawRectangle((int)barX, (int)barY, (int)barWidth, (int)barHeight, RED);
    DrawRectangle((int)barX, (int)barY, (int)(barWidth * healthPercent), (int)barHeight, GREEN);
}

// =============================================================================
void Cyclops::ApplyFreeze(float duration)
{
    if (_dying || !IsAlive())
        return;

    if (duration > _freezeTimer)
        _freezeTimer = duration;

    CancelCharge();
}

// =============================================================================
void Cyclops::ApplyBurn(float delay, int damage, Vector2 sourcePos)
{
    if (_dying || !IsAlive())
        return;

    _pendingBurns.push_back(PendingBurn{ delay, damage, sourcePos });
}

void Cyclops::TakeDamage(int damage, Vector2 attackerPos)
{
    if (_isInvulnerable || _leapInvulnerable)
        return;
    if (_charging && !_dying)
    {
        _health -= damage;

        if (_health > 0.f)
        {
            PlayHurtSound();
            _hitTimer = 0.02f;
            return;
        }

        _health = 0.f;
        _dying = true;
        _charging = false;
        _wantsToFire = false;
        _fireDirection = Vector2Zero();
        _attacking = false;
        _takingDamage = false;
        _deathTimer = 0.4f;
        _texture = _deathAnim;
        _frame = 0;
        _runningTime = 0.f;
        _maxFrames = _texture.width / _width;
        _updateTime = 1.f / 4.f;
        PlayDeathSound();
        return;
    }

    BaseCharacter::TakeDamage(damage, attackerPos);
}

void Cyclops::ApplyElectricCharge()
{
    if (_dying || !IsAlive())
        return;

    _isCharged = true;

    // Elemental crowd control is the intended way to break its aim.
    CancelCharge();

    // Set up hurt animation with correct per-frame dimensions
    _texture     = _takeDamageAnim;
    _width       = _takeDamageAnim.width / (float)kCyclopsFrameCount;
    _height      = _takeDamageAnim.height;
    _updateTime  = 1.f / 12.f;
    _maxFrames   = kCyclopsFrameCount;
    _frame       = 0;
    _runningTime = 0.f;
    _takingDamage       = true;
    _hitTimer           = kCyclopsFrameCount * (1.f / 12.f) + 0.1f;
    _chargeNextStunTime = (float)GetRandomValue(150, 400) / 100.f;
}

// =============================================================================
void Cyclops::UpdateBurns(float dt)
{
    int writeIndex = 0;

    for (int i = 0; i < (int)_pendingBurns.size(); ++i)
    {
        PendingBurn burn = _pendingBurns[i];
        burn.timer -= dt;

        if (burn.timer <= 0.f)
        {
            if (IsAlive() && !_dying)
                TakeDamage(burn.damage, burn.sourcePos);
            continue;
        }

        _pendingBurns[writeIndex++] = burn;
    }

    _pendingBurns.resize(writeIndex);
}

// =============================================================================
void Cyclops::SetWaveScale(int wave)
{
    // Fixed base profile — stat growth comes from ApplyEnemyPowerLevel.
    _expValue    = 5;
    _health      = 7.f;
    _maxHealth   = 7.f;
    _speed       = 190.f;
    _attackPower = 1.f;

    int tier = (wave - 1) / 5;
    _chargeDurationInst    = 1.0f;
    _attackCooldownMaxInst = std::max(1.8f,  3.5f - tier * 0.28f);
    _chargeRangeInst       = std::min(640.f, 480.f + tier * 20.f);
    _scatterRangeInst      = 170.f;
}

// =============================================================================
void Cyclops::PlayAttackSound()
{
    SetSoundVolume(_attackSound, 0.75f);
    PlaySound(_attackSound);
}

void Cyclops::PlayDeathSound()
{
    float pitch = GetRandomValue(kCyclopsDeathPitchMin, kCyclopsDeathPitchMax) / 100.f;
    SetSoundPitch(_deathSound, pitch);
    SetSoundVolume(_deathSound, 0.6f);
    PlaySound(_deathSound);
}

void Cyclops::PlayHurtSound()
{
    float pitch = GetRandomValue(100, 140) / 100.f;
    SetSoundPitch(_hurtSound, pitch);
    SetSoundVolume(_hurtSound, 0.5f);
    PlaySound(_hurtSound);
}

void Cyclops::ApplyExternalImpulse(Vector2 impulse, bool cancelLockedAnimation)
{
    // Cyclops needs a custom launch response because its hurt sheet uses a
    // different frame width than its idle/walk sheets. The ogre shove should
    // still freeze it on the second hurt frame just like regular enemies.
    _velocity = Vector2Add(_velocity, impulse);
    _launchVisualTimer = _launchVisualDuration;
    _launchHoldingHurtPose = true;
    _takingDamage = true;

    if (cancelLockedAnimation)
        _attacking = false;

    _texture = _takeDamageAnim;
    _width = _takeDamageAnim.width / (float)kCyclopsFrameCount;
    _height = _takeDamageAnim.height;
    _updateTime = 1.f / 12.f;
    _maxFrames = kCyclopsFrameCount;
    _frame = 1;
    _runningTime = 0.f;
}

void Cyclops::OnForcedPushCollision()
{
    if (!_forcedPushActive)
        return;

    UndoMovement();
    _forcedPushActive    = false;
    _forcedPushSpeed     = 0.f;
    _forcedPushDirection = Vector2Zero();
    _velocity            = Vector2Zero();
    SetIdleAnimation(true);
}

void Cyclops::OnFired()
{
    _wantsToFire = false;
    _postFireLockTimer = (_queuedFireMode == FireMode::Scatter) ? 0.28f : 0.75f;
}

void Cyclops::CancelCharge()
{
    _charging      = false;
    _chargeTimer   = 0.f;
    _wantsToFire   = false;
    _fireDirection = Vector2Zero();
    _queuedFireMode = FireMode::Sweep;
    _postFireLockTimer = 0.f;
    SetIdleAnimation(true);
}

// =============================================================================
void Cyclops::EnsureSharedResourcesLoaded()
{
    if (_sharedResourcesLoaded)
        return;

    _sharedIdleAnim       = LoadTexture(AssetPath("Enemy/CyclopsIdle.png").c_str());
    _sharedWalkAnim       = LoadTexture(AssetPath("Enemy/CyclopsWalk.png").c_str());
    _sharedAttackAnim     = LoadTexture(AssetPath("Enemy/CyclopsAttack.png").c_str());
    _sharedTakeDamageAnim = LoadTexture(AssetPath("Enemy/CyclopsDamage.png").c_str());
    _sharedDeathAnim      = LoadTexture(AssetPath("Enemy/CyclopsDeath.png").c_str());

    _sharedAttackSound = LoadSound(AssetPath("Sounds/CyclopsAttack.ogg").c_str());
    _sharedHurtSound   = LoadSound(AssetPath("Sounds/SmallMonsterDamage2.ogg").c_str());
    _sharedDeathSound  = LoadSound(AssetPath("Sounds/MonsterDeath.ogg").c_str());

    _sharedResourcesLoaded = true;
}

// =============================================================================
void Cyclops::UnloadSharedResources()
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

    _sharedIdleAnim        = Texture2D{};
    _sharedWalkAnim        = Texture2D{};
    _sharedAttackAnim      = Texture2D{};
    _sharedTakeDamageAnim  = Texture2D{};
    _sharedDeathAnim       = Texture2D{};
    _sharedAttackSound     = Sound{};
    _sharedHurtSound       = Sound{};
    _sharedDeathSound      = Sound{};
    _sharedResourcesLoaded = false;
}
