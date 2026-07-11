#include "Enemy.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "CharacterTuning.h"
#include "VirtualCanvas.h"

#include "raymath.h"
#include "VirtualCanvas.h"
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

const char* Enemy::GetBestiaryName()
{
    if (AsCyclops())       return "Cyclops";
    if (AsOgre())          return "Ogre";
    if (AsMolarbeast())    return "Molarbeast";
    if (AsSkeletonArcher())return "Skeleton Archer";
    if (AsFlameWisp())     return "Flame Wisp";
    if (AsAbyssSlime())    return "Abyss Slime";
    if (AsSlime())         return "Slime";
    if (AsPumpkinJack())   return "Pumpkin Jack";
    if (AsMinotaur())      return "Minotaur";
    if (AsSporeling())     return "Sporeling";
    if (AsShieldbearer())  return "Shieldbearer";
    if (AsPhantom())       return "Phantom";
    if (AsBomberImp())     return "Bomber Imp";
    if (AsWarchief())      return "Warchief";
    if (AsLivingBlade())   return "Living Blade";
    if (AsChompBug())      return "Chomp Bug";
    if (AsOsiris())        return "Osiris";
    if (AsTitanGuard())    return "Titan Guard";
    if (AsToxicVermin())   return "Toxic Vermin";
    if (AsAncientBear())   return "Ancient Bear";
    if (AsWerewolf())      return "Werewolf";
    const char* tn = GetTuningName();
    return tn ? tn : "Grunt";
}

void Enemy::ResetForSpawn(Vector2 pos)
{
    _worldPos = pos;
    _worldPosLastFrame = pos;
    _homePos = pos;
    _velocity = Vector2Zero();
    _isActive         = true;
    _bestiaryRecorded = false;
    _isEliteMiniboss  = false;
    _isInvulnerable   = false;
    _leapInvulnerable = false;
    ResetStatuses();      // clear poison/bleed/slow/vuln/mark from a pooled previous life
    ResetTuningState();   // re-applied from file at the end of the reset
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

    _graveReviveAvailable  = false;
    _graveReviveInvulTimer = 0.f;

    // Stagger each enemy's first flicker so a room full of grunts doesn't all
    // vanish at the same moment on the first tick.
    _flickerCooldown    = (float)GetRandomValue(150, 450) / 100.f;
    _flickerInWindup    = false;
    _flickerWindupTimer = 0.f;
    _flickerTarget      = Vector2Zero();

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
    Rectangle animBodyRect;
    if (GetAnimBodyRectWorld(animBodyRect))
        return animBodyRect;
    if (_hasTunedCollision)
        return GetTunedCollisionRec();

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
    Capsule2D animBodyCapsule;
    if (GetAnimBodyCapsuleWorld(animBodyCapsule))
        return animBodyCapsule;

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

// =============================================================================
// Character Animator (dev tool) + tuning interface
// =============================================================================

const char* Enemy::GetEditorAnimName(int index) const
{
    static const char* kStandardAnimNames[5] = { "Idle", "Walk", "Attack", "Hurt", "Death" };
    return (index >= 0 && index < 5) ? kStandardAnimNames[index] : "";
}

void Enemy::PlayEditorAnim(int index)
{
    const Texture2D* sheets[5] = { &_idleAnim, &_walkAnim, &_attackAnim, &_takeDamageAnim, &_deathAnim };
    if (index < 0 || index > 4)
        return;

    _texture = *sheets[index];
    if (_width > 0.f)
        _maxFrames = (int)(_texture.width / _width);
    if (_maxFrames < 1)
        _maxFrames = 1;
    _frame       = 0;
    _runningTime = 0.f;

    float frameTimeOverride = _editorAnimFrameTimes[index];
    if (frameTimeOverride > 0.f)
        _updateTime = frameTimeOverride;
}

void Enemy::TickEditorAnimation(float dt)
{
    // Editor-only frame advance: always loops, ignores gameplay state.
    _runningTime += dt;
    if (_runningTime >= _updateTime && _maxFrames > 0)
    {
        _runningTime = 0.f;
        _frame = (_frame + 1) % _maxFrames;
    }
}

float Enemy::GetEditorAnimFrameTime(int index) const
{
    if (index < 0 || index >= 10)
        return 0.f;
    return _editorAnimFrameTimes[index];
}

void Enemy::SetEditorAnimFrameTime(int index, float frameTime)
{
    if (index < 0 || index >= 10)
        return;
    _editorAnimFrameTimes[index] = frameTime;
}

Rectangle Enemy::GetCollisionRecRelative() const
{
    Rectangle rect = GetCollisionRec();
    return Rectangle{ rect.x - _worldPos.x, rect.y - _worldPos.y, rect.width, rect.height };
}

// =============================================================================
// Per-animation tuning — body circle, melee box, sprite draw offset
// =============================================================================

int Enemy::GetCurrentAnimSlot() const
{
    // Derive the slot from whichever sheet is playing so no gameplay code has
    // to remember to update it. Bosses override with their own sheet lists.
    if (_texture.id == _idleAnim.id)       return 0;
    if (_texture.id == _walkAnim.id)       return 1;
    if (_texture.id == _attackAnim.id)     return 2;
    if (_texture.id == _takeDamageAnim.id) return 3;
    if (_texture.id == _deathAnim.id)      return 4;
    return 0;
}

void Enemy::SetAnimBody(int slot, Vector2 offset, float radius)
{
    if (slot < 0 || slot >= kAnimSlots)
        return;
    _animBodySet[slot]    = true;
    _animBodyOffset[slot] = offset;
    _animBodyRadius[slot] = (radius < 4.f) ? 4.f : radius;
}

void Enemy::ClearAnimBody(int slot)
{
    if (slot >= 0 && slot < kAnimSlots)
        _animBodySet[slot] = false;
}

void Enemy::SetAnimMelee(int slot, Rectangle relativeRect)
{
    if (slot < 0 || slot >= kAnimSlots)
        return;
    _animMeleeSet[slot] = true;
    _animMeleeRel[slot] = relativeRect;
}

void Enemy::ClearAnimMelee(int slot)
{
    if (slot >= 0 && slot < kAnimSlots)
        _animMeleeSet[slot] = false;
}

void Enemy::SetAnimDrawOffset(int slot, Vector2 offset)
{
    if (slot < 0 || slot >= kAnimSlots)
        return;
    _animDrawSet[slot]    = true;
    _animDrawOffset[slot] = offset;
}

bool Enemy::GetAnimBodyCapsuleWorld(Capsule2D& out) const
{
    int slot = GetCurrentAnimSlot();

    // A slot only counts if it has a POSITIVE radius — a body circle authored at
    // (or defaulting to) radius 0 must never be treated as valid, or the enemy would
    // have a zero-size hurtbox and be impossible to hit.
    auto usable = [this](int i) {
        return i >= 0 && i < kAnimSlots && _animBodySet[i] && _animBodyRadius[i] > 0.f;
    };

    // If the current animation has no authored body circle, fall back to another
    // authored slot (Idle/slot 0 first, then the first set one) instead of failing.
    // The body barely moves between poses, so this keeps the enemy hittable in EVERY
    // animation — without this, an enemy tuned only in its Idle pose became effectively
    // invincible the moment it played its walk / attack / jump animation.
    if (!usable(slot))
    {
        slot = -1;
        if (usable(0))
            slot = 0;
        else
            for (int i = 0; i < kAnimSlots; ++i)
                if (usable(i)) { slot = i; break; }

        if (slot < 0)
            return false;   // no usable body circle on any slot — use other fallbacks
    }

    // Offsets are authored facing right; mirror X with the sprite.
    out = Capsule2D{
        { _worldPos.x + _animBodyOffset[slot].x * _rightLeft,
          _worldPos.y + _animBodyOffset[slot].y },
        0.f,
        _animBodyRadius[slot]
    };
    return true;
}

bool Enemy::GetAnimBodyRectWorld(Rectangle& out) const
{
    Capsule2D capsule;
    if (!GetAnimBodyCapsuleWorld(capsule))
        return false;

    // The hurt rect is the circle's bounding square so rect-based systems
    // (player melee, projectiles) match what the editor shows.
    out = Rectangle{
        capsule.center.x - capsule.radius,
        capsule.center.y - capsule.radius,
        capsule.radius * 2.f,
        capsule.radius * 2.f
    };
    return true;
}

bool Enemy::GetAnimMeleeRectWorld(int slot, Rectangle& out) const
{
    if (slot < 0 || slot >= kAnimSlots || !_animMeleeSet[slot])
        return false;

    Rectangle rel = _animMeleeRel[slot];
    if (_rightLeft < 0.f)
        rel.x = -(rel.x + rel.width);   // mirror around the sprite centre

    out = Rectangle{ _worldPos.x + rel.x, _worldPos.y + rel.y, rel.width, rel.height };
    return true;
}

Vector2 Enemy::GetCurrentAnimDrawOffset() const
{
    int slot = GetCurrentAnimSlot();
    if (slot < 0 || slot >= kAnimSlots || !_animDrawSet[slot])
        return Vector2{};
    return Vector2{ _animDrawOffset[slot].x * _rightLeft, _animDrawOffset[slot].y };
}

void Enemy::ResetTuningState()
{
    _hasTunedCollision = false;
    for (int i = 0; i < kAnimSlots; i++)
    {
        _editorAnimFrameTimes[i] = 0.f;
        _animBodySet[i]  = false;
        _animMeleeSet[i] = false;
        _animDrawSet[i]  = false;
    }
}

void Enemy::ApplyStoredTuning()
{
    const char* tuningName = GetTuningName();
    if (tuningName == nullptr)
        return;

    const CharacterTuning* tuning = CharacterTuningStore::Get(tuningName);
    if (tuning == nullptr)
        return;

    if (tuning->hasScale)
        _scale = tuning->scale;
    if (tuning->hasCollision)
        SetCollisionRecWorld(tuning->collisionRel);
    if (tuning->hasCapsule)
    {
        SetCapsuleRadius(tuning->capsuleRadius);
        SetCapsuleHalfHeight(tuning->capsuleHalfHeight);
        SetCapsuleOffset(tuning->capsuleOffset);
    }
    if (tuning->hasAttackBox)
    {
        SetAttackBoxWidth(tuning->attackBoxWidth);
        SetAttackBoxHeight(tuning->attackBoxHeight);
        SetAttackBoxOffsetX(tuning->attackBoxOffsetX);
        SetAttackBoxOffsetY(tuning->attackBoxOffsetY);
    }
    for (int i = 0; i < CharacterTuning::kMaxAnims && i < kAnimSlots; i++)
    {
        _editorAnimFrameTimes[i] = tuning->animFrameTime[i];

        if (tuning->animBody[i].set)
        {
            _animBodySet[i]    = true;
            _animBodyOffset[i] = Vector2{ tuning->animBody[i].x, tuning->animBody[i].y };
            _animBodyRadius[i] = tuning->animBody[i].radius;
        }
        if (tuning->animMelee[i].set)
        {
            _animMeleeSet[i] = true;
            _animMeleeRel[i] = tuning->animMelee[i].rect;
        }
        if (tuning->animDraw[i].set)
        {
            _animDrawSet[i]    = true;
            _animDrawOffset[i] = Vector2{ tuning->animDraw[i].x, tuning->animDraw[i].y };
        }
    }

    // Base grunt behaviour reads _attackUpdateTime directly, so the Attack
    // anim override (slot 2) maps onto it for types using the shared attack.
    if (_editorAnimFrameTimes[2] > 0.f)
        _attackUpdateTime = _editorAnimFrameTimes[2];
}

Rectangle Enemy::GetAttackCollisionRec() const
{
    // Per-animation melee box (Character Animator) wins; slot 2 = Attack.
    Rectangle animMeleeRect;
    if (GetAnimMeleeRectWorld(2, animMeleeRect))
        return animMeleeRect;

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
    if (_graveReviveInvulTimer > 0.f)
        _graveReviveInvulTimer -= dt;

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

// Shared waypoint path helper used by all enemies that don't have their own
// fully custom nav stack. Ticks the refresh timer, rebuilds the waypoint list
// when needed, advances past reached waypoints, then returns the best next target.
Vector2 Enemy::ResolveNavTarget(float dt, Vector2 playerFeet,
                                Vector2 navigationTarget, bool hasNavigationTarget)
{
    _pathRefreshTimer -= dt;
    bool needsRefresh = (_pathRefreshTimer <= 0.f || _waypoints.empty());

    if (needsRefresh && _nav != nullptr)
    {
        _waypoints = _nav->GetWaypointPath(_worldPos, playerFeet, kMaxWaypoints);
        _waypointIndex = 0;
        _pathRefreshTimer = _pathRefreshInterval;
    }

    if (!_waypoints.empty())
    {
        const float waypointReachRadius = _nav ? _nav->GetCellSize() * 0.6f : 48.f;
        while (_waypointIndex < (int)_waypoints.size() - 1 &&
               Vector2Distance(_worldPos, _waypoints[_waypointIndex]) < waypointReachRadius)
        {
            _waypointIndex++;
        }
    }

    if (!_waypoints.empty() && _waypointIndex < (int)_waypoints.size())
        return _waypoints[_waypointIndex];
    if (hasNavigationTarget)
        return navigationTarget;
    return playerFeet;
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

    // Choose movement target via the shared waypoint path helper.
    Vector2 targetPos = ResolveNavTarget(dt, playerCenter, navigationTarget, hasNavigationTarget);
    bool usingWaypoints = (!_waypoints.empty() && _waypointIndex < (int)_waypoints.size());
    // When no waypoints and no nav target, approach via the personal offset slot.
    if (!usingWaypoints && !hasNavigationTarget)
        targetPos = Vector2Add(playerCenter, _approachOffset);

    Vector2 toPlayer = Vector2Subtract(targetPos, _worldPos);

    Vector2 moveDir = Vector2Zero();

    if (Vector2Length(toPlayer) > 0.01f)
        moveDir = Vector2Normalize(toPlayer);

    // Blend a gentle pull toward this enemy's approach slot so enemies fan out
    // along the path rather than single-filing to the same waypoint cell.
    if (usingWaypoints && !hasNavigationTarget)
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
        // Warchief banner: allies inside the aura move noticeably faster.
        float moveSpeed = HasWarAura() ? _speed * kWarAuraSpeedMultiplier : _speed;
        _worldPos = Vector2Add(_worldPos, Vector2Scale(moveDir, moveSpeed * dt));
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
    screenPos.x += kVirtualWidth / 2.f;
    screenPos.y += kVirtualHeight / 2.f - launchLift;

    Rectangle source{ _frame * _width, 0.f, _rightLeft * _width, _height };

    float visualOffsetX = (_texture.id == _attackAnim.id) ? _attackVisualOffsetX * _rightLeft : 0.f;
    float visualOffsetY = (_texture.id == _attackAnim.id) ? _attackVisualOffsetY              : 0.f;

    // Per-animation sprite offset authored in the Character Animator.
    Vector2 animDrawOffset = GetCurrentAnimDrawOffset();
    visualOffsetX += animDrawOffset.x;
    visualOffsetY += animDrawOffset.y;

    Rectangle dest{ screenPos.x - w / 2.f + visualOffsetX, screenPos.y - h / 2.f + visualOffsetY, w, h };

    bool burning       = !_pendingBurns.empty();
    bool frozen        = IsFrozen();
    bool electroStunned = IsElectroStunned();
    bool charged       = _isCharged && !electroStunned;

    Color tint = electroStunned   ? Color{ 255, 255,  60, 255 } :   // bright yellow — stunned
                 charged         ? Color{ 220, 220,  80, 255 } :   // dim yellow — charged, not stunned
                 _flickerInWindup ? Color{ 180, 100, 255, 180 } :  // purple + semi-transparent — flicker windup
                 frozen          ? Color{ 140, 200, 255, 255 } :
                 burning         ? Color{ 255, 180, 180, 255 } :
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

    // Graveyard revive invincibility window — pulsing green ring so it's obvious when testing.
    if (_graveReviveInvulTimer > 0.f)
    {
        float pulse = sinf((float)GetTime() * 10.f) * 0.4f + 0.6f;
        DrawCircleLines((int)screenPos.x, (int)screenPos.y, 55.f, Fade(Color{  80, 255, 120, 255 }, pulse));
        DrawCircleLines((int)screenPos.x, (int)screenPos.y, 42.f, Fade(Color{ 160, 255, 200, 255 }, pulse * 0.5f));
    }

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

    float barX = screenPos.x - barWidth / 2.f;

    // Anchor the bar just above the actual body (top of the collision capsule),
    // NOT the padded sprite frame. Big/boss sprites have lots of empty frame above
    // the art, so the old "half the frame height" anchor floated their bars way up.
    // The capsule sits on the real character for every enemy, so this gives the same
    // small gap above the head that the legacy monsters have. (_healthBarYOffset is
    // still a per-enemy nudge in each ctor for the rare exception.)
    Capsule2D cap = GetCapsule();
    float bodyTopRelY = (cap.center.y - GetWorldPos().y) - cap.halfHeight - cap.radius;
    float barY = screenPos.y + bodyTopRelY - _healthBarYOffset;

    DrawRectangle((int)barX, (int)barY, (int)barWidth, (int)_healthBarHeight, RED);
    DrawRectangle((int)barX, (int)barY, (int)(barWidth * healthPercent), (int)_healthBarHeight, GREEN);
}

void Enemy::SetSpriteSheet(const Texture2D& sheet, int frameCount, float frameTime, bool resetFrame)
{
    _texture    = sheet;
    _width      = (float)sheet.width / (float)frameCount;
    _height     = (float)sheet.height;
    _updateTime = frameTime;
    _maxFrames  = frameCount;
    if (resetFrame) { _frame = 0; _runningTime = 0.f; }
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

void Enemy::TakeDamage(int damage, Vector2 attackerPos)
{
    // If the revive one-shot invul window is active, ignore all damage.
    if (_graveReviveInvulTimer > 0.f)
        return;

    // Check whether this hit would kill us and the revive is still available.
    bool wouldKill = (_health - damage <= 0);
    if (wouldKill && _graveReviveAvailable)
    {
        _graveReviveAvailable  = false;
        _graveReviveInvulTimer = 1.5f;
        _health                = _maxHealth * 0.5f;
        // Play hurt reaction so the revive is visually readable.
        _takingDamage = true;
        _texture      = _takeDamageAnim;
        _frame        = 0;
        _runningTime  = 0.f;
        _maxFrames    = (int)(_texture.width / _width);
        _hitTimer     = _maxFrames * _updateTime + 0.05f;
        return;
    }

    BaseCharacter::TakeDamage(damage, attackerPos);
}

void Enemy::StartFlickerWindup(float duration, Vector2 target)
{
    _flickerInWindup    = true;
    _flickerWindupTimer = duration;
    _flickerTarget      = target;
}

bool Enemy::ConsumeFlickerComplete()
{
    if (_flickerInWindup && _flickerWindupTimer <= 0.f)
    {
        _flickerInWindup = false;
        return true;
    }
    return false;
}

void Enemy::TickFlicker(float dt)
{
    if (_flickerCooldown > 0.f)    _flickerCooldown    -= dt;
    if (_flickerInWindup)           _flickerWindupTimer -= dt;
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
    _expValue    = Balance::Grunt::kBaseExpValue;
    _health      = Balance::Grunt::kBaseHealth;
    _maxHealth   = Balance::Grunt::kBaseHealth;
    _attackPower = Balance::Grunt::kBaseAttack;
    _speed       = Balance::Grunt::kBaseSpeed;
    _attackDelay = Balance::Grunt::kBaseAttackDelay;
}

void Enemy::ApplyEnemyPowerLevel(int enemyPowerLevel)
{
    // Single growth system: advances every few rooms (see
    // GetEnemyPowerLevelForWave). Steeper scaling gives the run a real
    // roguelite ramp — late-zone grunts are genuine threats, not fodder.
    // +16% HP, +8% damage, +4% speed per power level above 1.
    if (enemyPowerLevel <= 1)
        return;

    const float t = (float)(enemyPowerLevel - 1);
    _maxHealth   = std::ceil(_maxHealth   * (1.f + Balance::Curve::kHealthPerLevel * t));
    _health      = _maxHealth;
    _attackPower *= (1.f + Balance::Curve::kDamagePerLevel * t);
    _speed       *= (1.f + Balance::Curve::kSpeedPerLevel  * t);
    _expValue    += (enemyPowerLevel - 1);
}

void Enemy::ApplyDifficultyScaling(float healthMult, float damageMult)
{
    if (healthMult > 1.f)
    {
        _maxHealth = std::ceil(_maxHealth * healthMult);
        _health    = _maxHealth;
    }
    if (damageMult > 1.f)
        _attackPower *= damageMult;
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

void Enemy::UpdateEnrageLatch(float dt)
{
    if (_enrageFlashTimer > 0.f)
        _enrageFlashTimer -= dt;

    if (_enrageThreshold <= 0.f)
        return;

    // Full HP → a fresh (or pooled-reused) spawn: clear the latch so last fight's
    // enrage doesn't carry over.
    if (_health >= _maxHealth)
    {
        _enrageLatched = false;
        return;
    }

    // One-way transition into the enrage phase.
    if (!_enrageLatched && IsAlive() && _health <= _maxHealth * _enrageThreshold)
    {
        _enrageLatched      = true;
        _enrageShakePending = true;   // telegraph consumed by CombatDirector
        _enrageFlashTimer   = 0.6f;
    }
}

bool Enemy::ConsumeEnrageShakeRequest()
{
    bool r = _enrageShakePending;
    _enrageShakePending = false;
    return r;
}

// ── Multi-phase boss system ──────────────────────────────────────────────────
void Enemy::SetPhaseThresholds(std::vector<float> descendingHpFractions)
{
    _phaseThresholds = std::move(descendingHpFractions);
    _phase = 0;
    _pendingPhaseChange = -1;
    _phaseTransitionTimer = 0.f;
}

void Enemy::UpdatePhaseLatch(float dt)
{
    if (_phaseTransitionTimer > 0.f)
        _phaseTransitionTimer -= dt;

    if (_phaseThresholds.empty())
        return;

    // Full HP → fresh/pooled spawn: reset so last fight's phase doesn't carry over.
    if (_health >= _maxHealth)
    {
        _phase = 0;
        ResetStatuses();   // also drop any status carried from a pooled previous life
        return;
    }

    // One-way: advance through each threshold the HP has dropped below. Guarded so a
    // single big hit that crosses several thresholds only announces the last one.
    while (IsAlive() && _phase < (int)_phaseThresholds.size() &&
           _health <= _maxHealth * _phaseThresholds[_phase])
    {
        _phase++;
        _pendingPhaseChange = _phase;   // boss announces / reacts via ConsumePhaseChange
    }
}

int Enemy::ConsumePhaseChange()
{
    int p = _pendingPhaseChange;
    _pendingPhaseChange = -1;
    return p;
}

// ── Shared status effects (ARPG combat-identity pass) ────────────────────────
void Enemy::ApplyPoison(int damagePerTick, float duration, int stacks)
{
    if (_dying || !IsAlive() || damagePerTick <= 0) return;
    if (IsBoss()) duration *= kBossStatusDurMult;
    const int maxStacks = IsBoss() ? 5 : 8;   // capped, harder to overwhelm bosses
    _poisonStacks  = std::min(maxStacks, _poisonStacks + std::max(1, stacks));
    _poisonPerTick = std::max(_poisonPerTick, damagePerTick);
    _poisonTimer   = std::max(_poisonTimer, duration);
    if (_poisonTickTimer <= 0.f) _poisonTickTimer = 0.5f;
}

void Enemy::ApplyBleed(int damagePerTick, float duration)
{
    if (_dying || !IsAlive() || damagePerTick <= 0) return;
    if (IsBoss()) duration *= kBossStatusDurMult;
    _bleedPerTick = std::max(_bleedPerTick, damagePerTick);
    _bleedTimer   = std::max(_bleedTimer, duration);
    if (_bleedTickTimer <= 0.f) _bleedTickTimer = 0.5f;
}

void Enemy::ApplySlow(float speedMult, float duration)
{
    if (_dying || !IsAlive()) return;
    if (IsBoss()) duration *= kBossStatusDurMult;
    speedMult = std::clamp(speedMult, 0.1f, 1.f);
    // Strongest slow currently active wins; refresh its duration.
    _slowMult  = (_slowTimer > 0.f) ? std::min(_slowMult, speedMult) : speedMult;
    _slowTimer = std::max(_slowTimer, duration);
}

void Enemy::ApplyVulnerability(float damageTakenMult, float duration)
{
    if (_dying || !IsAlive()) return;
    if (IsBoss()) duration *= kBossStatusDurMult;
    damageTakenMult = std::clamp(damageTakenMult, 1.f, 3.f);
    _vulnMult  = (_vulnTimer > 0.f) ? std::max(_vulnMult, damageTakenMult) : damageTakenMult;
    _vulnTimer = std::max(_vulnTimer, duration);
}

void Enemy::ApplyMark(float duration)
{
    if (_dying || !IsAlive()) return;
    if (IsBoss()) duration *= kBossStatusDurMult;
    _markTimer = std::max(_markTimer, duration);
}

float Enemy::GetStatusMoveSpeedMult() const
{
    return (_slowTimer > 0.f) ? _slowMult : 1.f;
}

void Enemy::ResetStatuses()
{
    _poisonTimer = _poisonTickTimer = 0.f; _poisonPerTick = 0; _poisonStacks = 0;
    _bleedTimer  = _bleedTickTimer  = 0.f; _bleedPerTick  = 0;
    _slowTimer   = 0.f; _slowMult = 1.f;
    _vulnTimer   = 0.f; _vulnMult = 1.f;
    _markTimer   = 0.f;
}

void Enemy::UpdateStatuses(float dt)
{
    // Poison — stacking DoT.
    if (_poisonTimer > 0.f)
    {
        _poisonTimer -= dt;
        _poisonTickTimer -= dt;
        if (_poisonTickTimer <= 0.f && IsAlive() && !_dying)
        {
            _poisonTickTimer += 0.5f;
            TakeDamage(_poisonPerTick * std::max(1, _poisonStacks), _worldPos);
        }
        if (_poisonTimer <= 0.f) { _poisonStacks = 0; _poisonPerTick = 0; }
    }

    // Bleed — physical DoT, hits harder while the enemy is moving.
    if (_bleedTimer > 0.f)
    {
        _bleedTimer -= dt;
        _bleedTickTimer -= dt;
        if (_bleedTickTimer <= 0.f && IsAlive() && !_dying)
        {
            _bleedTickTimer += 0.5f;
            float dx = _worldPos.x - _worldPosLastFrame.x;
            float dy = _worldPos.y - _worldPosLastFrame.y;
            bool moving = (dx * dx + dy * dy) > 4.f;
            int dmg = moving ? (int)std::ceil(_bleedPerTick * 1.5f) : _bleedPerTick;
            TakeDamage(std::max(1, dmg), _worldPos);
        }
        if (_bleedTimer <= 0.f) _bleedPerTick = 0;
    }

    if (_slowTimer > 0.f) { _slowTimer -= dt; if (_slowTimer <= 0.f) _slowMult = 1.f; }
    if (_vulnTimer > 0.f) { _vulnTimer -= dt; if (_vulnTimer <= 0.f) _vulnMult = 1.f; }
    if (_markTimer > 0.f) { _markTimer -= dt; }
}

void Enemy::UpdateBurns(float dt)
{
    // Warchief aura decay lives here because every enemy type calls
    // UpdateBurns each frame regardless of its custom Update logic.
    if (_warAuraTimer > 0.f)
        _warAuraTimer -= dt;

    // Same reasoning for the boss enrage latch — piggy-backs on the universal
    // per-frame hook so no boss needs its own call (Molarbeast, which doesn't
    // call UpdateBurns, invokes UpdateEnrageLatch directly).
    UpdateEnrageLatch(dt);
    UpdatePhaseLatch(dt);
    UpdateStatuses(dt);

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
