#include "Enemy.h"
#include "AssetPaths.h"

#include "raymath.h"
#include <algorithm>
#include <cmath>

Texture2D Enemy::_sharedIdleAnim{};
Texture2D Enemy::_sharedWalkAnim{};
Texture2D Enemy::_sharedAttackAnim{};
Texture2D Enemy::_sharedTakeDamageAnim{};
Texture2D Enemy::_sharedDeathAnim{};
Sound Enemy::_sharedAttackSound{};
Sound Enemy::_sharedHurtSound{};
Sound Enemy::_sharedDeathSound{};
bool Enemy::_sharedResourcesLoaded = false;

Enemy::Enemy(Vector2 pos)
{
    _worldPos = pos;
    _homePos = pos;
}

Enemy::~Enemy()
{
}

void Enemy::Init()
{
    EnsureSharedResourcesLoaded();

    _idleAnim = _sharedIdleAnim;
    _walkAnim = _sharedWalkAnim;
    _attackAnim = _sharedAttackAnim;
    _takeDamageAnim = _sharedTakeDamageAnim;
    _deathAnim = _sharedDeathAnim;
    _attackSound = _sharedAttackSound;
    _hurtSound = _sharedHurtSound;
    _deathSound = _sharedDeathSound;

    ResetForSpawn(_worldPos);
}

void Enemy::ResetForSpawn(Vector2 pos)
{
    _worldPos = pos;
    _worldPosLastFrame = pos;
    _homePos = pos;
    _velocity = Vector2Zero();
    _isActive         = true;
    _isEliteMiniboss  = false;
    _isInvulnerable   = false;
    _leapInvulnerable = false;
    _texture = _idleAnim;
    _updateTime = 1.f / 8.f;

    _width = 32.f;
    _height = _texture.height;
    _scale = 6.f;
    _speed = 200.f;

    _health = 3.f;
    _maxHealth = 3.f;
    _attackPower = 1.f;

    _maxFrames = (int)(_texture.width / _width);
    _frame = GetRandomValue(0, _maxFrames - 1);
    _runningTime = GetRandomValue(0, 200) / 100.f * _updateTime;
    _hitTimer = 0.f;
    _deathTimer = 0.4f;
    _freezeTimer              = 0.f;
    _isCharged                = false;
    _chargeNextStunTime       = 0.f;
    _electricChargeTotalTimer = 0.f;
    _attacking = false;
    _damageApplied = false;
    _lungeState = LungeState::None;
    _lungeTimer = 0.f;
    _lungeCooldown = (float)GetRandomValue(80, 180) / 100.f;
    _lungeDir = Vector2Zero();
    _lungeDamageApplied = false;
    _burnPanicDir = Vector2Zero();
    _burnPanicTurnTimer = 0.f;
    _burnSoundTimer = 0.f;
    _takingDamage = false;
    _dying = false;
    _pendingBurns.clear();
    _stuckTimer    = 0.f;
    _stuckCheckPos = _worldPos;

    // Clear the cached waypoint path and stagger each enemy's first refresh
    // with a small random offset so they don't all hit the nav grid at once.
    _waypoints.clear();
    _waypointIndex = 0;
    _pathRefreshInterval = kPathRefreshMin
        + (float)GetRandomValue(0, 100) / 100.f * (kPathRefreshMax - kPathRefreshMin);
    _pathRefreshTimer = (float)GetRandomValue(0, (int)(_pathRefreshInterval * 100.f)) / 100.f;

    _forcedPushActive    = false;
    _forcedPushDirection = Vector2Zero();
    _forcedPushSpeed     = 0.f;
    _inAttackRange       = false;

    // Each enemy gets its own flank slot so nearby enemies naturally choose
    // slightly different approach lanes around the player instead of piling
    // into one exact point.
    _flankSide = (GetRandomValue(0, 1) == 0) ? -1.f : 1.f;
    _flankDistance = (float)GetRandomValue((int)_minFlankDistance, (int)_maxFlankDistance);

    PickApproachOffset();
    _approachOffsetTimer = (float)GetRandomValue(0, 250) / 100.f;
}

void Enemy::SetIsEliteMiniboss(bool b)
{
    _isEliteMiniboss = b;

    if (!b)
        return;

    _maxHealth = std::ceil(_maxHealth * 2.5f);
    _health = _maxHealth;
    _attackPower *= 1.25f;
    _speed *= 1.10f;
    _expValue = std::max(_expValue + 4, (int)std::ceil(_expValue * 2.0f));
}

void Enemy::ApplyEnrage()
{
    _speed       *= 1.5f;
    _attackDelay *= 0.5f;
}

Rectangle Enemy::GetCollisionRec() const
{
    // Stable idle-frame dimensions are the sprite-space reference.
    // _width is always 32 for the grunt idle sheet; _scale is set in ResetForSpawn.
    float stableHalfW = 32.f * _scale * 0.5f;
    float stableHalfH = (_idleAnim.id > 0 ? (float)_idleAnim.height : _height) * _scale * 0.5f;

    if (_collisionSize.x == 0.f && stableHalfW > 0.f)
    {
        auto* s = const_cast<Enemy*>(this);
        s->_collisionSize   = { 87.00f, 79.00f };
        s->_collisionOffset = { 58.00f, 18.00f };
    }
    return Rectangle{
        _worldPos.x - stableHalfW + _collisionOffset.x,
        _worldPos.y - stableHalfH + _collisionOffset.y,
        _collisionSize.x, _collisionSize.y
    };
}

Capsule2D Enemy::GetCapsule() const
{
    if (_capsuleRadius == 0.f)
    {
        auto* s = const_cast<Enemy*>(this);
        s->_capsuleRadius     = 36.f;
        s->_capsuleHalfHeight = 0.f;
        s->_capsuleOffset     = { -6.f, 6.f };
    }
    return Capsule2D{
        { _worldPos.x + _capsuleOffset.x, _worldPos.y + _capsuleOffset.y },
        _capsuleHalfHeight,
        _capsuleRadius
    };
}

Rectangle Enemy::GetAttackCollisionRec() const
{
    // Attack box anchored to sprite center (_worldPos), independent of body offset.
    return Rectangle{
        _worldPos.x + _attackBoxOffsetX * _rightLeft - _attackBoxWidth  * 0.5f,
        _worldPos.y + _attackBoxOffsetY              - _attackBoxHeight * 0.5f,
        _attackBoxWidth,
        _attackBoxHeight
    };
}

void Enemy::PickApproachOffset()
{
    // 6 slots — 3 per side. Enemies cycle through them so each one picks a
    // unique spot: right side spreads NE/E/SE, left side spreads NW/W/SW.
    // This keeps enemies on the flanks while preventing them from stacking.
    static const Vector2 dirs[6] = {
        {  0.707f, -0.707f },   // right-up   (NE)
        {  1.000f,  0.000f },   // right       (E)
        {  0.707f,  0.707f },   // right-down (SE)
        { -0.707f, -0.707f },   // left-up    (NW)
        { -1.000f,  0.000f },   // left        (W)
        { -0.707f,  0.707f },   // left-down  (SW)
    };
    static int s_nextSlot = 0;
    int idx = s_nextSlot % 6;
    s_nextSlot++;
    _approachOffset = Vector2Scale(dirs[idx], _approachOffsetRadius);
    _approachOffsetTimer = _approachOffsetDuration + (float)GetRandomValue(0, 150) / 100.f;
}

void Enemy::Update(float dt, Vector2 heroWorldPos, Vector2 navigationTarget, bool hasNavigationTarget,
    const std::vector<std::unique_ptr<Enemy>>& enemies, const std::vector<Vector2>& propCenters)
{
    if (!_isActive)
        return;

    // UpdateDeath is intentionally NOT called here.
    // It is called once per frame in Engine::UpdateEnemyCount so the
    // drop world position can be captured before Death() teleports the enemy.

    _worldPosLastFrame = _worldPos;

    // Slide in the push direction; Engine::HandleCollisions stops us on walls/props.
    if (_forcedPushActive)
    {
        _worldPos = Vector2Add(_worldPos, Vector2Scale(_forcedPushDirection, _forcedPushSpeed * dt));
        return;
    }

    if (_attacking)
        _velocity = Vector2Zero();
    ApplyVelocity(dt);
    UpdateHit(dt);
    UpdateBurns(dt);
    UpdateElectricCharge(dt);
    UpdateLaunchVisual(dt);

    if (_freezeTimer > 0.f)
        _freezeTimer -= dt;

    // Periodically repick approach direction so enemies shift positions
    // and don't permanently crowd one side of the player.
    _approachOffsetTimer -= dt;
    if (_approachOffsetTimer <= 0.f)
        PickApproachOffset();

    if (!_dying)
    {
        if (_target == nullptr)
            return;

        _attackCooldown -= dt;
        if (_lungeCooldown > 0.f)
            _lungeCooldown -= dt;

        if (UpdateEliteLunge(dt))
        {
            HandleAnimation(dt);
            return;
        }

        HandleMovement(dt, navigationTarget, hasNavigationTarget, enemies, propCenters);
        HandleAttack(enemies);
    }

    HandleAnimation(dt);
}

void Enemy::HandleMovement(float dt, Vector2 navigationTarget, bool hasNavigationTarget,
    const std::vector<std::unique_ptr<Enemy>>& enemies, const std::vector<Vector2>& propCenters)
{
    if (_target == nullptr || _dying)
        return;

    // Update attack-range hysteresis first — needed whether moving or attacking.
    // Zero velocity on the frame we enter attack range so there is no slide.
    {
        float distToPlayer = Vector2Length(Vector2Subtract(_target->GetFeetWorldPos(), _worldPos));
        if (!_inAttackRange)
        {
            if (distToPlayer <= _attackRange)
            {
                _inAttackRange = true;
                _velocity = Vector2Zero();
            }
        }
        else
        {
            _inAttackRange = (distToPlayer <= _attackRange + _attackRangeHysteresis);
        }
    }

    // Position is completely locked during attack — mirrors how Character::HandleMovement works.
    if (_attacking)
        return;

    Vector2 playerCenter = _target->GetFeetWorldPos();

    if (!_pendingBurns.empty() && !IsFrozen() && !_takingDamage)
    {
        UpdateBurnPanic(dt);
        Vector2 oldPos = _worldPos;
        _worldPos = Vector2Add(_worldPos, Vector2Scale(_burnPanicDir, _speed * 1.18f * dt));
        if (Vector2Length(Vector2Subtract(_worldPos, oldPos)) > 0.01f)
        {
            _texture = _walkAnim;
            if (_burnPanicDir.x < 0.f) _rightLeft = -1;
            if (_burnPanicDir.x > 0.f) _rightLeft = 1;
        }
        return;
    }

    // ── Waypoint path management ──────────────────────────────────────────────
    // Tick down this enemy's personal refresh timer.
    _pathRefreshTimer -= dt;
    bool needsRefresh = (_pathRefreshTimer <= 0.f || _waypoints.empty());

    if (needsRefresh && _nav != nullptr)
    {
        // Extract a fresh waypoint list by following the shared flow-field gradient.
        // This is cheap (O(path length)) because the BFS is already computed.
        _waypoints = _nav->GetWaypointPath(_worldPos, playerCenter, kMaxWaypoints);
        _waypointIndex = 0;
        _pathRefreshTimer = _pathRefreshInterval;
    }

    // Advance through waypoints: move to the next one when close enough.
    if (!_waypoints.empty())
    {
        const float waypointReachRadius = _nav ? _nav->GetCellSize() * 0.6f : 48.f;
        while (_waypointIndex < (int)_waypoints.size() - 1 &&
               Vector2Distance(_worldPos, _waypoints[_waypointIndex]) < waypointReachRadius)
        {
            _waypointIndex++;
        }
    }

    // Choose movement target: use the current waypoint when we have a cached path,
    // otherwise fall back to the engine-supplied single-step or direct approach.
    Vector2 targetPos;
    bool usingWaypoints = (!_waypoints.empty() && _waypointIndex < (int)_waypoints.size());

    if (usingWaypoints)
        targetPos = _waypoints[_waypointIndex];
    else if (hasNavigationTarget)
        targetPos = navigationTarget;
    else
        targetPos = Vector2Add(playerCenter, _approachOffset);

    Vector2 toPlayer = Vector2Subtract(targetPos, _worldPos);

    Vector2 moveDir = Vector2Zero();

    if (Vector2Length(toPlayer) > 0.01f)
        moveDir = Vector2Normalize(toPlayer);

    // Blend a gentle pull toward this enemy's approach slot so enemies fan out
    // along the path rather than single-filing to the same waypoint cell.
    if (usingWaypoints || hasNavigationTarget)
    {
        Vector2 slotTarget = Vector2Add(playerCenter, _approachOffset);
        Vector2 toSlot = Vector2Subtract(slotTarget, _worldPos);
        if (Vector2Length(toSlot) > 0.01f)
            moveDir = Vector2Add(moveDir, Vector2Scale(Vector2Normalize(toSlot), 0.3f));
    }

    Vector2 separation = Vector2Zero();
    Vector2 propSlide = Vector2Zero();

    for (const auto& enemy : enemies)
    {
        if (enemy.get() == this)
            continue;
        if (!enemy->IsActive() || enemy->IsDying() || !enemy->IsAlive())
            continue;

        float dist = Vector2Distance(_worldPos, enemy->_worldPos);

        if (dist < 130.f && dist > 0.f)
        {
            Vector2 away = Vector2Subtract(_worldPos, enemy->_worldPos);

            if (Vector2Length(away) > 0.01f)
            {
                float strength = (130.f - dist) / 130.f;
                separation = Vector2Add(separation, Vector2Scale(Vector2Normalize(away), strength));
            }
        }
    }

    // Prop repulsion — steer away from nearby pillars
    for (const Vector2& propCenter : propCenters)
    {
        float dist = Vector2Distance(_worldPos, propCenter);
        if (dist < 110.f && dist > 0.f)
        {
            Vector2 away = Vector2Subtract(_worldPos, propCenter);
            if (Vector2Length(away) > 0.01f)
            {
                away = Vector2Normalize(away);
                float strength = (110.f - dist) / 110.f;
                separation = Vector2Add(separation, Vector2Scale(away, strength * 1.8f));

                // Add a small tangential slide along the pillar so enemies keep
                // flowing around props instead of just pushing directly away
                // and bunching up at the same corner.
                Vector2 tangentA = { -away.y, away.x };
                Vector2 tangentB = { away.y, -away.x };
                float dotA = Vector2DotProduct(tangentA, moveDir);
                float dotB = Vector2DotProduct(tangentB, moveDir);
                Vector2 bestTangent = (dotA >= dotB) ? tangentA : tangentB;
                propSlide = Vector2Add(propSlide, Vector2Scale(bestTangent, strength));
            }
        }
    }

    separation = Vector2Scale(separation, 0.80f);
    propSlide = Vector2Scale(propSlide, _propSlideStrength);
    moveDir = Vector2Add(moveDir, separation);
    moveDir = Vector2Add(moveDir, propSlide);

    if (Vector2Length(moveDir) > 0.01f)
        moveDir = Vector2Normalize(moveDir);

    Vector2 oldPos = _worldPos;
    bool slotAvailable = CanTakeAttackSlot(enemies);
    bool shouldFlank = _inAttackRange && !slotAvailable;
    bool inAttackRange = _inAttackRange && !shouldFlank;

    if (shouldFlank)
    {
        Vector2 toPlayerNow = Vector2Subtract(playerCenter, _worldPos);
        if (Vector2Length(toPlayerNow) > 0.01f)
        {
            Vector2 forward = Vector2Normalize(toPlayerNow);
            Vector2 lateral = { -forward.y * _flankSide, forward.x * _flankSide };
            Vector2 desired = Vector2Add(Vector2Add(playerCenter, _approachOffset), Vector2Scale(lateral, _flankDistance));
            Vector2 flankDir = Vector2Subtract(desired, _worldPos);
            if (Vector2Length(flankDir) > 0.01f)
                moveDir = Vector2Normalize(flankDir);
        }
    }

    bool intentionalMove = false;
    if (!_takingDamage && !IsFrozen() && !inAttackRange)
    {
        _worldPos = Vector2Add(_worldPos, Vector2Scale(moveDir, _speed * dt));
        intentionalMove = true;
    }

    // Position-level push: resolve physical overlap with other enemies.
    {
        const float minSep = 60.f;
        for (const auto& other : enemies)
        {
            if (other.get() == this) continue;
            if (!other->IsActive() || other->IsDying()) continue;

            float dist = Vector2Distance(_worldPos, other->_worldPos);
            if (dist < minSep && dist > 0.01f)
            {
                Vector2 push = Vector2Normalize(Vector2Subtract(_worldPos, other->_worldPos));
                _worldPos = Vector2Add(_worldPos, Vector2Scale(push, (minSep - dist) * 0.35f));
            }
        }
    }

    // Stuck detection — only runs when the enemy is actively trying to chase.
    // Skipped inside attack range since standing still there is intentional.
    if (!_takingDamage && !IsFrozen() && !inAttackRange)
    {
        _stuckTimer += dt;

        if (_stuckTimer >= _stuckThreshold)
        {
            float moved = Vector2Distance(_worldPos, _stuckCheckPos);

            if (moved < _stuckMinMove)
            {
                // Sample 8 directions and pick the one that points most toward
                // the player while avoiding the direction we're already stuck in.
                // This routes the enemy around the wall rather than bouncing off it.
                Vector2 toPlayer = Vector2Normalize(
                    Vector2Subtract(_target->GetFeetWorldPos(), _worldPos));
                float bestDot  = -2.f;
                Vector2 bestDir = { -moveDir.y, moveDir.x };   // perp fallback

                for (int d = 0; d < 8; d++)
                {
                    float angle = d * (PI / 4.f);
                    Vector2 candidate = { cosf(angle), sinf(angle) };
                    // Skip directions too close to the stuck heading
                    if (Vector2DotProduct(candidate, moveDir) > 0.7f) continue;
                    float dot = Vector2DotProduct(candidate, toPlayer);
                    if (dot > bestDot) { bestDot = dot; bestDir = candidate; }
                }

                _velocity = Vector2Add(_velocity, Vector2Scale(bestDir, _speed * 2.0f));
            }

            _stuckTimer    = 0.f;
            _stuckCheckPos = _worldPos;
        }
    }
    else
    {
        // Reset timer whenever frozen/attacking so the check stays meaningful
        _stuckTimer    = 0.f;
        _stuckCheckPos = _worldPos;
    }

    // Only switch to walk when the enemy is intentionally chasing and actually moving.
    if (!_takingDamage)
    {
        if (intentionalMove && Vector2Length(Vector2Subtract(_worldPos, oldPos)) > 0.01f)
        {
            _texture = _walkAnim;

            if (moveDir.x < 0) _rightLeft = -1;
            if (moveDir.x > 0) _rightLeft = 1;
        }
        else
        {
            _texture = _idleAnim;
        }
    }
}

bool Enemy::CanTakeAttackSlot(const std::vector<std::unique_ptr<Enemy>>& enemies) const
{
    int committed = 0;
    constexpr int kMaxCommittedGrunts = 2;
    constexpr float kSlotRadius = 260.f;

    for (const auto& enemy : enemies)
    {
        const Enemy* other = enemy.get();
        if (other == this)
            continue;
        if (!other->IsActive() || other->IsDying() || !other->IsAlive())
            continue;
        if (Vector2Distance(_worldPos, other->_worldPos) > kSlotRadius)
            continue;
        if (other->_attacking || other->_lungeState == LungeState::Windup || other->_lungeState == LungeState::Lunging)
            committed++;
    }

    return committed < kMaxCommittedGrunts;
}

bool Enemy::UpdateEliteLunge(float dt)
{
    if (!_isEliteMiniboss || _target == nullptr || _dying || !IsAlive())
        return false;
    if (IsFrozen() || _takingDamage || !_pendingBurns.empty() || _attacking)
    {
        if (_lungeState != LungeState::None)
        {
            _lungeState = LungeState::Recovery;
            _lungeTimer = 0.f;
        }
        return _lungeState != LungeState::None;
    }

    Vector2 toPlayer = Vector2Subtract(_target->GetFeetWorldPos(), _worldPos);
    float dist = Vector2Length(toPlayer);

    if (_lungeState == LungeState::None)
    {
        if (_lungeCooldown <= 0.f && dist >= kEliteLungeMinRange && dist <= kEliteLungeMaxRange)
        {
            _lungeState = LungeState::Windup;
            _lungeTimer = 0.f;
            _lungeDamageApplied = false;
            _velocity = Vector2Zero();
            _texture = _idleAnim;
            _frame = 0;
            _runningTime = 0.f;
            _maxFrames = (int)(_texture.width / _width);
            _updateTime = 1.f / 10.f;
        }
        return false;
    }

    if (dist > 0.01f)
    {
        if (toPlayer.x < 0.f) _rightLeft = -1;
        else if (toPlayer.x > 0.f) _rightLeft = 1;
    }

    if (_lungeState == LungeState::Windup)
    {
        _velocity = Vector2Zero();
        _texture = _idleAnim;
        _lungeTimer += dt;
        if (_lungeTimer >= kEliteLungeWindup)
        {
            _lungeDir = (dist > 0.01f) ? Vector2Normalize(toPlayer) : Vector2{ (float)_rightLeft, 0.f };
            _lungeState = LungeState::Lunging;
            _lungeTimer = 0.f;
            _lungeDamageApplied = false;
            _texture = _attackAnim;
            _frame = 0;
            _runningTime = 0.f;
            _maxFrames = (int)(_texture.width / _width);
            _updateTime = kEliteLungeDuration / std::max(1, _maxFrames);
            PlayAttackSound();
        }
        return true;
    }

    if (_lungeState == LungeState::Lunging)
    {
        _texture = _attackAnim;
        _worldPos = Vector2Add(_worldPos, Vector2Scale(_lungeDir, kEliteLungeSpeed * dt));
        if (!_lungeDamageApplied && CheckCollisionRecs(GetCollisionRec(), _target->GetCollisionRec()))
        {
            _target->TakeDamage((int)_attackPower, _worldPos);
            _lungeDamageApplied = true;
        }
        _lungeTimer += dt;
        if (_lungeTimer >= kEliteLungeDuration)
        {
            _lungeState = LungeState::Recovery;
            _lungeTimer = 0.f;
            _lungeCooldown = kEliteLungeCooldown;
            _texture = _idleAnim;
            _frame = 0;
            _runningTime = 0.f;
            _maxFrames = (int)(_texture.width / _width);
            _updateTime = 1.f / 10.f;
        }
        return true;
    }

    if (_lungeState == LungeState::Recovery)
    {
        _velocity = Vector2Zero();
        _texture = _idleAnim;
        _lungeTimer += dt;
        if (_lungeTimer >= kEliteLungeRecovery)
        {
            _lungeState = LungeState::None;
            _lungeTimer = 0.f;
        }
        return true;
    }

    return false;
}

void Enemy::UpdateBurnPanic(float dt)
{
    _burnPanicTurnTimer -= dt;
    _burnSoundTimer -= dt;

    if (_burnPanicTurnTimer <= 0.f || Vector2Length(_burnPanicDir) < 0.01f)
    {
        float angle = (float)GetRandomValue(0, 359) * DEG2RAD;
        _burnPanicDir = { cosf(angle), sinf(angle) };
        _burnPanicTurnTimer = (float)GetRandomValue(18, 45) / 100.f;
    }

    if (_burnSoundTimer <= 0.f && _deathSound.frameCount > 0)
    {
        float pitch = (float)GetRandomValue(150, 210) / 100.f;
        SetSoundPitch(_deathSound, pitch);
        SetSoundVolume(_deathSound, 0.18f);
        PlaySound(_deathSound);
        _burnSoundTimer = (float)GetRandomValue(55, 110) / 100.f;
    }
}

void Enemy::HandleAttack(const std::vector<std::unique_ptr<Enemy>>& enemies)
{
    if (_dying || _target == nullptr || IsFrozen() || _takingDamage || !_pendingBurns.empty() || _lungeState != LungeState::None)
        return;

    float distance = Vector2Length(Vector2Subtract(_target->GetFeetWorldPos(), _worldPos));

    if (distance <= _attackRange && !_attacking && _attackCooldown <= 0.f && CanTakeAttackSlot(enemies))
    {
        _attacking = true;
        _damageApplied = false;
        _velocity = Vector2Zero();

        Vector2 toPlayer = Vector2Subtract(_target->GetFeetWorldPos(), _worldPos);
        if (toPlayer.x < 0.f) _rightLeft = -1;
        else if (toPlayer.x > 0.f) _rightLeft = 1;

        _attackCooldown = _attackDelay;

        _texture = _attackAnim;
        _frame = 0;
        _runningTime = 0.f;

        _maxFrames = (int)(_texture.width / _width);
        _updateTime = _attackUpdateTime;

        PlayAttackSound();
    }

    if (_attacking && !_damageApplied && _frame == 2)
    {
        if (CheckCollisionRecs(GetAttackCollisionRec(), _target->GetCollisionRec()))
        {
            _target->TakeDamage((int)_attackPower, _worldPos);
            _damageApplied = true;
            PickApproachOffset();
        }
    }

    if (!_target->IsAlive())
    {
        _attacking = false;
        _texture = _idleAnim;
        _updateTime = 1.f / 8.f;
        _maxFrames = (int)(_texture.width / _width);
        _frame = 0;
        _runningTime = 0.f;
        _speed = 0.f;
    }
}
void Enemy::DrawEnemy(Vector2 heroWorldPos)
{
    if (!_isActive)
        return;

    float w = _width * _scale;
    float h = _height * _scale;

    // External launch effects briefly make enemies read as if they were thrown
    // upward by the ogre charge. The sprite grows slightly and lifts off the
    // ground, then settles back to normal as the timer expires.
    float launchRatio = (_launchVisualDuration > 0.f)
        ? (_launchVisualTimer / _launchVisualDuration)
        : 0.f;
    float launchScale = 1.f + _launchVisualScaleBoost * launchRatio;
    float launchLift = _launchVisualLift * launchRatio;
    w *= launchScale;
    h *= launchScale;

    Vector2 screenPos = Vector2Subtract(_worldPos, heroWorldPos);
    screenPos.x += GetScreenWidth() / 2.f;
    screenPos.y += GetScreenHeight() / 2.f - launchLift;

    Rectangle source{ _frame * _width, 0.f, _rightLeft * _width, _height };

    float visualOffsetX = (_texture.id == _attackAnim.id) ? _attackVisualOffsetX * _rightLeft : 0.f;
    float visualOffsetY = (_texture.id == _attackAnim.id) ? _attackVisualOffsetY              : 0.f;

    Rectangle dest{ screenPos.x - w / 2.f + visualOffsetX, screenPos.y - h / 2.f + visualOffsetY, w, h };

    bool burning       = !_pendingBurns.empty();
    bool frozen        = IsFrozen();
    bool electroStunned = IsElectroStunned();
    bool charged       = _isCharged && !electroStunned;

    Color tint = electroStunned ? Color{ 255, 255,  60, 255 } :   // bright yellow — stunned
                 charged        ? Color{ 220, 220,  80, 255 } :   // dim yellow — charged, not stunned
                 frozen         ? Color{ 140, 200, 255, 255 } :
                 burning        ? Color{ 255, 180, 180, 255 } :
                                  WHITE;

    if (burning)
    {
        for (int i = 0; i < 3; ++i)
        {
            float flickerX = (float)GetRandomValue(-14, 14);
            float flickerY = (float)GetRandomValue(-26, -4);
            DrawCircleV(Vector2{ screenPos.x + flickerX, screenPos.y + flickerY }, 5.f, Fade(ORANGE, 0.55f));
            DrawCircleV(Vector2{ screenPos.x + flickerX * 0.7f, screenPos.y + flickerY - 6.f }, 3.f, Fade(YELLOW, 0.45f));
        }
    }

    DrawTexturePro(_texture, source, dest, Vector2{}, 0.f, tint);

    if (_health != _maxHealth)
        DrawHealthBar(screenPos, w, h);
    if (_isEliteMiniboss)
        DrawEliteLabel(screenPos, w, h);
}

void Enemy::HandleAnimation(float dt)
{
    // Ogre launch reactions intentionally hold enemies on the second hurt
    // frame so the shove reads as a sustained airborne hit, not a normal
    // walk/idle transition.
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

            // Frozen: hold on the last frame, no looping or state transitions
            if (IsFrozen())
            {
                _frame = _maxFrames - 1;
                return;
            }

            if (_takingDamage)
            {
                _takingDamage = false;
                _texture = _idleAnim;
                _updateTime = 1.f / 10.f;
                _maxFrames = (int)(_texture.width / _width);
                _frame = 0;
                return;
            }

            if (_attacking)
            {
                _attacking = false;
                _texture = _idleAnim;
                _updateTime = 1.f / 10.f;
                _maxFrames = (int)(_texture.width / _width);
            }

            _frame = 0;
        }
    }
}

void Enemy::DrawHealthBar(Vector2 screenPos, float w, float h)
{
    if (_health <= 0)
        return;

    float healthPercent = (float)_health / (float)_maxHealth;

    float barWidth = w * 0.8f;
    float barHeight = 6.f;

    float barX = screenPos.x - barWidth / 2.f;
    float barY = screenPos.y - h / 2.f - 12.f;

    DrawRectangle(barX, barY, barWidth, barHeight, RED);
    DrawRectangle(barX, barY, barWidth * healthPercent, barHeight, GREEN);
}

void Enemy::DrawEliteLabel(Vector2 screenPos, float w, float h)
{
    float barWidth = w * 0.8f;
    float barY = screenPos.y - h / 2.f - 12.f;
    Vector2 labelPos{ screenPos.x - barWidth * 0.5f, barY - 18.f };

    static Font font = GetFontDefault();
    static constexpr float kFontSize = 20.f;
    static constexpr float kSpacing = 1.f;
    const char* text = "ELITE";

    DrawTextEx(font, text, { labelPos.x + 1.f, labelPos.y + 1.f }, kFontSize, kSpacing, Fade(BLACK, 0.6f));
    DrawTextEx(font, text, labelPos, kFontSize, kSpacing, Color{255, 210, 70, 255});
}

void Enemy::ApplyFreeze(float duration)
{
    if (_dying || !IsAlive())
        return;

    static constexpr float kMaxFreezeDuration = 10.f;
    float capped = std::min(duration, kMaxFreezeDuration);
    if (capped > _freezeTimer)
        _freezeTimer = capped;
}

void Enemy::ApplyElectricCharge()
{
    if (_dying || !IsAlive())
        return;

    // Start the 10-second cap window on the very first charge application.
    if (!_isCharged)
        _electricChargeTotalTimer = 10.f;

    _isCharged    = true;
    _takingDamage = true;
    _texture      = _takeDamageAnim;
    _frame        = 0;
    _runningTime  = 0.f;
    _maxFrames    = (int)(_texture.width / _width);
    _hitTimer     = _maxFrames * _updateTime + 0.1f;
    // Schedule the next stun after this one finishes
    _chargeNextStunTime = (float)GetRandomValue(150, 400) / 100.f;
}

void Enemy::UpdateElectricCharge(float dt)
{
    if (!_isCharged || _dying || !IsAlive() || _takingDamage)
        return;

    _electricChargeTotalTimer -= dt;
    if (_electricChargeTotalTimer <= 0.f)
    {
        _isCharged = false;
        _electricChargeTotalTimer = 0.f;
        return;
    }

    _chargeNextStunTime -= dt;
    if (_chargeNextStunTime <= 0.f)
        ApplyElectricCharge();
}

void Enemy::StartForcedPush(Vector2 direction, float speed)
{
    _forcedPushDirection = (Vector2Length(direction) > 0.01f)
        ? Vector2Normalize(direction)
        : Vector2{ 1.f, 0.f };
    _forcedPushSpeed  = speed;
    _forcedPushActive = true;

    _attacking = false;
    _velocity  = Vector2Zero();
    _launchHoldingHurtPose = false;
    _launchVisualTimer     = 0.f;

    // Hold on frame 3 of the hurt animation while sliding.
    _texture     = _takeDamageAnim;
    _maxFrames   = _texture.width / _width;
    _frame       = std::min(3, _maxFrames - 1);
    _runningTime = 0.f;
    _updateTime  = 1.f / 12.f;
}

void Enemy::OnForcedPushCollision()
{
    if (!_forcedPushActive)
        return;

    UndoMovement();
    _forcedPushActive    = false;
    _forcedPushSpeed     = 0.f;
    _forcedPushDirection = Vector2Zero();
    _velocity            = Vector2Zero();

    _texture     = _idleAnim;
    _frame       = 0;
    _runningTime = 0.f;
    _maxFrames   = _texture.width / _width;
    _updateTime  = 1.f / 10.f;
}

void Enemy::ApplyExternalImpulse(Vector2 impulse, bool cancelLockedAnimation)
{
    // This helper lets special enemies, like the ogre, throw other enemies
    // around without needing to know their internal animation state. It also
    // starts a short launch visual so the shove reads as a heavy upward fling.
    _velocity = Vector2Add(_velocity, impulse);
    _launchVisualTimer = _launchVisualDuration;
    _launchHoldingHurtPose = true;
    _takingDamage = true;
    _texture = _takeDamageAnim;
    _frame = 1;
    _runningTime = 0.f;
    _updateTime = 1.f / 12.f;
    _maxFrames = (int)(_texture.width / _width);

    if (cancelLockedAnimation)
    {
        _attacking = false;
    }
}

void Enemy::UpdateLaunchVisual(float dt)
{
    if (_launchVisualTimer <= 0.f)
        return;

    _launchVisualTimer -= dt;
    if (_launchVisualTimer < 0.f)
        _launchVisualTimer = 0.f;

    if (_launchVisualTimer == 0.f && _launchHoldingHurtPose)
    {
        _launchHoldingHurtPose = false;
        _takingDamage = false;
        _texture = _idleAnim;
        _frame = 0;
        _runningTime = 0.f;
        _updateTime = 1.f / 10.f;
        _maxFrames = (int)(_texture.width / _width);
    }
}

void Enemy::SetWaveScale(int /*wave*/)
{
    // Fixed base profile — all stat growth comes from ApplyEnemyPowerLevel.
    // Wave parameter kept for virtual signature compatibility.
    _expValue    = 3;
    _health      = 4.f;
    _maxHealth   = 4.f;
    _attackPower = 1.f;
    _speed       = 185.f;
    _attackDelay = 1.5f;
}

void Enemy::ApplyEnemyPowerLevel(int enemyPowerLevel)
{
    // Single growth system: advances every 10 waves.
    // +10% HP, +5% damage, +3% speed per power level above 1.
    if (enemyPowerLevel <= 1)
        return;

    const float t = (float)(enemyPowerLevel - 1);
    _maxHealth   = std::ceil(_maxHealth   * (1.f + 0.10f * t));
    _health      = _maxHealth;
    _attackPower *= (1.f + 0.05f * t);
    _speed       *= (1.f + 0.03f * t);
    _expValue    += (enemyPowerLevel - 1);
}

void Enemy::ApplyBurn(float delay, int damage, Vector2 sourcePos)
{
    if (_dying || !IsAlive())
        return;

    static constexpr float kMaxBurnDelay = 10.f;
    if (delay > kMaxBurnDelay)
        return;

    _pendingBurns.push_back(PendingBurn{ delay, damage, sourcePos });
}

void Enemy::UpdateBurns(float dt)
{
    int writeIndex = 0;

    for (int i = 0; i < static_cast<int>(_pendingBurns.size()); ++i)
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

void Enemy::PlayAttackSound()
{
    float pitch = GetRandomValue(100, 140) / 100.f;
    SetSoundPitch(_attackSound, pitch);
    SetSoundVolume(_attackSound, 0.5f);
    PlaySound(_attackSound);
}

void Enemy::PlayDeathSound()
{
    float pitch = GetRandomValue(140, 180) / 100.f;
    SetSoundPitch(_deathSound, pitch);
    SetSoundVolume(_deathSound, 0.5f);
    PlaySound(_deathSound);
}

void Enemy::PlayHurtSound()
{
    float pitch = GetRandomValue(140, 180) / 100.f;
    SetSoundPitch(_hurtSound, pitch);
    SetSoundVolume(_hurtSound, 0.85f);
    StopSound(_hurtSound);
    PlaySound(_hurtSound);
}

void Enemy::EnsureSharedResourcesLoaded()
{
    if (_sharedResourcesLoaded)
        return;

    _sharedIdleAnim = LoadTexture(AssetPath("Enemy/EnemyIdle.png").c_str());
    _sharedWalkAnim = LoadTexture(AssetPath("Enemy/EnemyWalk.png").c_str());
    _sharedAttackAnim = LoadTexture(AssetPath("Enemy/EnemyAttack.png").c_str());
    _sharedTakeDamageAnim = LoadTexture(AssetPath("Enemy/EnemyDamage.png").c_str());
    _sharedDeathAnim = LoadTexture(AssetPath("Enemy/EnemyDeath.png").c_str());
    _sharedAttackSound = LoadSound(AssetPath("Sounds/SwordSwipe2.ogg").c_str());
    _sharedHurtSound = LoadSound(AssetPath("Sounds/SmallMonsterDamage.ogg").c_str());
    _sharedDeathSound = LoadSound(AssetPath("Sounds/PlayerDeath.ogg").c_str());
    _sharedResourcesLoaded = true;
}

void Enemy::UnloadSharedResources()
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

    _sharedIdleAnim = Texture2D{};
    _sharedWalkAnim = Texture2D{};
    _sharedAttackAnim = Texture2D{};
    _sharedTakeDamageAnim = Texture2D{};
    _sharedDeathAnim = Texture2D{};
    _sharedAttackSound = Sound{};
    _sharedHurtSound = Sound{};
    _sharedDeathSound = Sound{};
    _sharedResourcesLoaded = false;
}
