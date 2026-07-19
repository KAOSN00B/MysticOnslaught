#include "Ogre.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "AttackTuning.h"
#include "VirtualCanvas.h"

#include "raymath.h"
#include "VirtualCanvas.h"

#include <algorithm>

Texture2D Ogre::_sharedIdleAnim{};
Texture2D Ogre::_sharedWalkAnim{};
Texture2D Ogre::_sharedAttackAnim{};
Texture2D Ogre::_sharedTakeDamageAnim{};
Texture2D Ogre::_sharedDeathAnim{};
Sound     Ogre::_sharedRushHitSound{};
Sound     Ogre::_sharedWallHitSound{};
Sound     Ogre::_sharedOgreHurtSound{};
Sound     Ogre::_sharedOgreDeathSound{};
bool      Ogre::_sharedResourcesLoaded = false;

Ogre::Ogre(Vector2 pos)
    : Enemy(pos)
{
}

Ogre::~Ogre() {}

void Ogre::Init()
{
    Enemy::EnsureSharedResourcesLoaded();
    EnsureSharedResourcesLoaded();

    _idleAnim       = _sharedIdleAnim;
    _walkAnim       = _sharedWalkAnim;
    _attackAnim     = _sharedAttackAnim;
    _takeDamageAnim = _sharedTakeDamageAnim;
    _deathAnim      = _sharedDeathAnim;
    // Ogre uses its own impact/hurt/death audio so its rush reads differently
    // from the base enemy archetype. AttackSound is left unused for now because
    // the rush uses explicit hit/wall sounds instead of the normal melee cue.
    _attackSound    = Sound{};
    _hurtSound      = _sharedOgreHurtSound;
    _deathSound     = _sharedOgreDeathSound;

    ResetForSpawn(_worldPos);
}

void Ogre::ResetForSpawn(Vector2 pos)
{
    // The SHARED reset owns every pooled-lifetime field (statuses, pit fall,
    // revive, elite events, guard link, phase latch, telemetry, nav, flicker)
    // so a reused instance can never keep a previous life's data.
    Enemy::ResetForSpawn(pos);

    // ── Ogre profile on top of the shared defaults ───────────────────────────
    SetIdleAnimation(false);

    // Keep ogre scale aligned with the rest of the enemy cast so it feels like
    // part of the same world. The ogre still reads larger because its sprite
    // art itself is larger, so it does not need extra scale on top.
    _scale       = 4.8f;
    _speed       = _walkSpeed;
    _health      = 8.f;
    _maxHealth   = 8.f;
    _attackPower = _rushDamage;
    _expValue    = 3;

    _frame       = GetRandomValue(0, _maxFrames - 1);
    _runningTime = GetRandomValue(0, 200) / 100.f * _updateTime;

    _rushState     = RushState::Repositioning;
    _chargeTimer   = 0.f;
    _rushCooldown  = 0.f;
    _stunTimer     = 0.f;
    _impactShakeRequested = false;
    _rushDirection = Vector2Zero();
    _rushedEnemies.clear();
    _chargesRemaining = 0;
    _retargetTimer    = 0.f;
    // Reset to design defaults; SetWaveScale will override these.
    _chargeDurationInst         = 3.0f;
    _chargeCooldownDurationInst = 6.0f;
    _activeChargeDuration       = _chargeDurationInst;
}

void Ogre::Update(float dt, Vector2 heroWorldPos, Vector2 navigationTarget, bool hasNavigationTarget,
                  const std::vector<std::unique_ptr<Enemy>>& enemies,
                  const std::vector<Vector2>& propCenters)
{
    if (!_isActive)
        return;

    _worldPosLastFrame = _worldPos;

    ApplyVelocity(dt);
    UpdateHit(dt);
    UpdateBurns(dt);
    UpdateElectricCharge(dt);

    if (_freezeTimer > 0.f)
        _freezeTimer -= dt;

    if (_rushCooldown > 0.f)
        _rushCooldown -= dt;

    if (!_dying && _target != nullptr)
    {
        // SECOND WIND: one-time 50% escalation (shared elite phase latch).
        // Cancel the current action safely; the next charge sequence doubles.
        if (_isEliteMiniboss && ConsumePhaseChange() >= 1)
        {
            RequestBossCallout("SECOND WIND");
            EmitEliteEvent({ EliteEventKind::PhaseChange, EliteArchetype::Ogre,
                             EliteMove::OgreCharge, 0, _worldPos });
            if (_rushState == RushState::Charging || _rushState == RushState::Retargeting)
            {
                _rushState   = RushState::Repositioning;
                _chargeTimer = 0.f;
                SetIdleAnimation(true);
            }
            _rushCooldown = std::min(_rushCooldown, 1.2f);   // come back angry soon
        }

        switch (_rushState)
        {
        case RushState::Repositioning:
            HandleRepositioning(dt, navigationTarget, hasNavigationTarget);
            break;

        case RushState::Charging:
            if (IsFrozen() || _takingDamage)
            {
                _rushState   = RushState::Repositioning;
                _chargeTimer = 0.f;
                _chargesRemaining = 0;   // freeze interrupts the whole sequence
                SetIdleAnimation(true);
            }
            else
                HandleCharging(dt);
            break;

        case RushState::Rushing:
            HandleRush(dt, enemies);
            break;

        case RushState::Retargeting:
            // Visible pause between SECOND WIND charges: stand still, face the
            // player, then lock a NEW direction through a (shorter) telegraph.
            // The second charge can never silently rotate mid-travel.
            _velocity = Vector2Zero();
            {
                float dxToPlayer = _target->GetFeetWorldPos().x - _worldPos.x;
                if      (dxToPlayer < -20.f) _rightLeft = -1.f;
                else if (dxToPlayer >  20.f) _rightLeft =  1.f;
            }
            _retargetTimer -= dt;
            if (IsFrozen() || _takingDamage)
            {
                _rushState = RushState::Repositioning;
                _chargesRemaining = 0;
                SetIdleAnimation(true);
            }
            else if (_retargetTimer <= 0.f)
            {
                _rushState = RushState::Charging;
                _chargeTimer = 0.f;
                _activeChargeDuration = _secondChargeTelegraph;
                EmitEliteEvent({ EliteEventKind::Telegraph, EliteArchetype::Ogre,
                                 EliteMove::OgreCharge, 0, _worldPos,
                                 _target->GetFeetWorldPos() });
                SetIdleAnimation(true);
            }
            break;

        case RushState::Stunned:
            _stunTimer -= dt;
            if (_stunTimer <= 0.f)
            {
                _stunTimer = 0.f;
                _rushState = RushState::Repositioning;
                SetIdleAnimation(true);
            }
            break;
        }

        // Prop centers are intentionally unused in the rush state because the
        // engine collision pass owns the "stop on prop / wall" rule.
        (void)propCenters;
        (void)heroWorldPos;
    }

    HandleAnimation(dt);
}

void Ogre::SetWaveScale(int wave)
{
    // Fixed base profile — stat growth comes from ApplyEnemyPowerLevel.
    // Charge timing still tightens each 5-wave band for behavioural escalation.
    _expValue    = 5;
    _health      = Balance::Elite::kOgreHealth;
    _maxHealth   = Balance::Elite::kOgreHealth;
    _speed       = _walkSpeed;
    _attackPower = _rushDamage;

    int tier = (wave - 1) / 5;
    _chargeDurationInst         = std::max(1.5f, 3.0f - tier * 0.25f);
    _chargeCooldownDurationInst = std::max(3.0f, 6.0f - tier * 0.5f);
}

void Ogre::DrawEnemy(Vector2 cameraRef)
{
    if (!_isActive)
        return;

    // Source slicing follows the real sheet size, but drawing uses one stable
    // visual box. That keeps the ogre locked to the same ground point even
    // though its sheets do not all share the same authored dimensions.
    float drawWidth  = _visualFrameWidth * _scale;
    float drawHeight = _visualFrameHeight * _scale;

    Vector2 screenPos = Vector2Subtract(_worldPos, cameraRef);
    screenPos.x += kVirtualWidth / 2.f;
    screenPos.y += kVirtualHeight / 2.f;

    Color tint = IsElectroStunned() ? Color{ 255, 255,  60, 255 } :
                 _flickerInWindup   ? Color{ 180, 100, 255, 180 } :
                 IsFrozen()         ? Color{ 140, 200, 255, 255 } :
                 _takingDamage      ? Color{ 255, 215, 215, 255 } :
                                      WHITE;

    if (_rushState == RushState::Charging)
    {
        float chargeRatio = _chargeTimer / _chargeDurationInst;

        // Ogre charge telegraph should live on the character itself rather
        // than on a separate ground circle. As the charge fills, the sprite
        // aggressively shifts toward a bright red, then returns to its normal
        // tint when the rush starts. The green/blue channels drop harder than
        // before so the change reads clearly without darkening the sprite.
        tint.r = (unsigned char)(255.f);
        tint.g = (unsigned char)(255.f - 210.f * chargeRatio);
        tint.b = (unsigned char)(255.f - 210.f * chargeRatio);
    }

    float sourceX = _frame * _width;
    float sourceWidth = _width;

    // Walk still needs to advance in full frame-sized steps, but its art runs
    // close to the frame edge. Cropping a tiny amount from the visible source
    // rect avoids bleeding into the neighboring frame without breaking the
    // one-frame-at-a-time playback.
    if (_texture.id == _walkAnim.id)
    {
        sourceX += _walkSheetFrameCrop;
        sourceWidth -= _walkSheetFrameCrop * 2.f;
    }

    Rectangle source{ sourceX, 0.f, _rightLeft * sourceWidth, _height };
    // The walk sheet is centered slightly differently than the idle/charge
    // sheets, so it gets a small visual nudge to keep the ogre planted while
    // moving without affecting collision or world position.
    float visualOffsetX = (_texture.id == _walkAnim.id) ? _walkVisualOffsetX * _rightLeft : 0.f;
    Rectangle dest{ screenPos.x + visualOffsetX, screenPos.y + drawHeight / 2.f, drawWidth, drawHeight };

    if (_rushState == RushState::Rushing)
    {
        // Rush ghosting gives the ogre a cheap motion-blur feel without a
        // post-process shader. Each afterimage is drawn farther back along the
        // rush direction and with lower alpha so the charge reads as heavy and
        // fast.
        for (int ghostIndex = _rushGhostCount; ghostIndex >= 1; --ghostIndex)
        {
            float spacing = _rushGhostSpacing * (float)ghostIndex;
            float alpha   = _rushGhostAlphaFalloff * (float)(_rushGhostCount - ghostIndex + 1);
            Vector2 ghostOffset = Vector2Scale(_rushDirection, -spacing);
            Rectangle ghostDest{
                dest.x + ghostOffset.x,
                dest.y + ghostOffset.y,
                drawWidth,
                drawHeight
            };

            Color ghostTint = Fade(Color{ 120, 255, 200, 255 }, alpha);
            DrawTexturePro(_texture, source, ghostDest, Vector2{ drawWidth / 2.f, drawHeight }, 0.f, ghostTint);
        }
    }

    // The bottom-center origin anchors the feet to the ogre's world position.
    // This is the same visual goal as the cyclops fix: animation changes
    // frames, but the character should not appear to swim across the arena.
    DrawTexturePro(_texture, source, dest, Vector2{ drawWidth / 2.f, drawHeight }, 0.f, tint);

    if (_graveReviveInvulTimer > 0.f)
    {
        float pulse = sinf((float)GetTime() * 10.f) * 0.4f + 0.6f;
        DrawCircleLines((int)screenPos.x, (int)screenPos.y, 80.f, Fade(Color{  80, 255, 120, 255 }, pulse));
        DrawCircleLines((int)screenPos.x, (int)screenPos.y, 62.f, Fade(Color{ 160, 255, 200, 255 }, pulse * 0.5f));
    }

    if (_health != _maxHealth)
        DrawHealthBar(screenPos, drawWidth, drawHeight);
    if (_isEliteMiniboss)
        DrawEliteLabel(screenPos, drawWidth, drawHeight);
}

Rectangle Ogre::GetCollisionRec() const
{
    if (_collisionSize.x == 0.f)
    {
        auto* s = const_cast<Ogre*>(this);
        s->_collisionSize   = { 186.00f, 109.00f };
        s->_collisionOffset = { 30.00f, 11.00f };
    }
    float stableHalfW = _visualFrameWidth  * _scale * 0.5f;
    float stableHalfH = _visualFrameHeight * _scale * 0.5f;
    return Rectangle{
        _worldPos.x - stableHalfW + _collisionOffset.x,
        _worldPos.y - stableHalfH + _collisionOffset.y,
        _collisionSize.x, _collisionSize.y
    };
}

Capsule2D Ogre::GetCapsule() const
{
    if (_capsuleRadius == 0.f)
    {
        auto* s = const_cast<Ogre*>(this);
        s->_capsuleRadius     = 52.f;
        s->_capsuleHalfHeight = 12.f;
        s->_capsuleOffset     = { 0.f, 0.f };
    }
    return Capsule2D{
        { _worldPos.x + _capsuleOffset.x, _worldPos.y + _capsuleOffset.y },
        _capsuleHalfHeight,
        _capsuleRadius
    };
}

void Ogre::TakeDamage(int damage, Vector2 attackerPos)
{
    if (_dying)
        return;
    if (_isInvulnerable || _leapInvulnerable)
        return;

    // Guard Links: visible reduction, never immunity (mirrors Enemy::TakeDamage
    // because the Ogre owns its own damage path).
    if (_eliteGuardLinked && damage > 0)
    {
        damage = ApplyGuardLinkReduction(damage);
        _eliteGuardReducedHit = true;
    }

    const bool stayCharging = (_rushState == RushState::Charging);

    _health -= damage;

    if (_health > 0.f)
        PlayHurtSound();

    if (_health <= 0.f)
    {
        _health = 0.f;
        _dying = true;
        _attacking = false;
        _takingDamage = false;
        _rushState = RushState::Repositioning;
        _deathTimer = 0.4f;
        SetDeathAnimation(true);
        PlayDeathSound();
    }
    else if (stayCharging)
    {
        // Charging can be pushed but should not flinch. The ogre keeps its
        // charge timer and idle telegraph while still receiving knockback.
        SetIdleAnimation(false);
        _takingDamage = false;
        _attacking = false;
    }
    else
    {
        _attacking = false;
        _takingDamage = true;
        _hitTimer = 0.18f;
        SetHurtAnimation(true);

        if (_rushState == RushState::Rushing)
            FinishRush(false);
    }

    Vector2 direction = Vector2Subtract(_worldPos, attackerPos);
    if (Vector2Length(direction) > 0.01f)
        direction = Vector2Normalize(direction);

    _velocity = Vector2Scale(direction, 2000.f);
}

void Ogre::OnRushBlocked()
{
    if (_rushState != RushState::Rushing)
        return;

    UndoMovement();
    PlayWallHitSound();
    FinishRush(true);
}

bool Ogre::ConsumeImpactShakeRequest()
{
    bool requested = _impactShakeRequested;
    _impactShakeRequested = false;
    return requested;
}

void Ogre::PlayHurtSound()
{
    // Ogre hurt now reuses the player hurt clip, but pitched much lower so it
    // reads like a deep monster impact instead of a human reaction.
    SetSoundPitch(_hurtSound, 0.45f);
    SetSoundVolume(_hurtSound, 0.24f);
    StopSound(_hurtSound);
    PlaySound(_hurtSound);
}

void Ogre::PlayDeathSound()
{
    SetSoundVolume(_deathSound, 0.2f);
    PlaySound(_deathSound);
}

void Ogre::PlayRushHitSound() const
{
    SetSoundVolume(_sharedRushHitSound, 0.45f);
    StopSound(_sharedRushHitSound);
    PlaySound(_sharedRushHitSound);
}

void Ogre::PlayWallHitSound() const
{
    SetSoundVolume(_sharedWallHitSound, 0.5f);
    StopSound(_sharedWallHitSound);
    PlaySound(_sharedWallHitSound);
}

void Ogre::SetIdleAnimation(bool resetFrame)
{
    _texture    = _idleAnim;
    _width      = _idleAnim.width / (float)_sheetFrameCount;
    _height     = _idleAnim.height;
    _updateTime = 1.f / 10.f;
    _maxFrames  = _playbackFrameCount;

    if (resetFrame)
    {
        _frame = 0;
        _runningTime = 0.f;
    }
}

void Ogre::SetWalkAnimation(bool resetFrame)
{
    _texture    = _walkAnim;
    _width      = _walkAnim.width / (float)_sheetFrameCount;
    _height     = _walkAnim.height;
    _updateTime = 1.f / 10.f;
    // The last walk frames in this sheet are visually unstable, so walk uses
    // the clean leading frames while the other ogre states still keep the full
    // six-frame playback.
    _maxFrames  = _walkPlaybackFrameCount;

    if (resetFrame)
    {
        _frame = 0;
        _runningTime = 0.f;
    }
}

void Ogre::SetRushAnimation(bool resetFrame)
{
    _texture    = _attackAnim;
    _width      = _attackAnim.width / (float)_sheetFrameCount;
    _height     = _attackAnim.height;
    _updateTime = 1.f / 12.f;
    _maxFrames  = _playbackFrameCount;

    if (resetFrame)
    {
        _frame = 0;
        _runningTime = 0.f;
    }
}

void Ogre::SetHurtAnimation(bool resetFrame)
{
    _texture    = _takeDamageAnim;
    _width      = _takeDamageAnim.width / (float)_sheetFrameCount;
    _height     = _takeDamageAnim.height;
    _updateTime = 1.f / 12.f;
    _maxFrames  = _playbackFrameCount;

    if (resetFrame)
    {
        _frame = 0;
        _runningTime = 0.f;
    }
}

void Ogre::SetDeathAnimation(bool resetFrame)
{
    _texture    = _deathAnim;
    _width      = _deathAnim.width / (float)_sheetFrameCount;
    _height     = _deathAnim.height;
    _updateTime = 1.f / 8.f;
    _maxFrames  = _playbackFrameCount;

    if (resetFrame)
    {
        _frame = 0;
        _runningTime = 0.f;
    }
}

void Ogre::HandleRepositioning(float dt, Vector2 navigationTarget, bool hasNavigationTarget)
{
    // Use the shared waypoint path helper so the ogre navigates around
    // obstacles the same way the grunt does.
    Vector2 targetPos = ResolveNavTarget(dt, _target->GetFeetWorldPos(), navigationTarget, hasNavigationTarget);
    Vector2 toTarget  = Vector2Subtract(targetPos, _worldPos);
    float directDist  = Vector2Distance(_worldPos, _target->GetFeetWorldPos());

    if (_rushCooldown <= 0.f && !hasNavigationTarget && directDist <= _chargeRange && !IsFrozen())
    {
        _rushState = RushState::Charging;
        _chargeTimer = 0.f;
        // A fresh sequence: one charge normally, two after SECOND WIND.
        // Attack Library tuning (attacktuning_Ogre_Charge.txt) overrides the
        // telegraph length when authored.
        _chargesRemaining = NextOgreChargeCount(IsElitePhaseTwo());
        _activeChargeDuration = EliteSignatureValueOr(AttackTuningStore::Get("Ogre_Charge"),
                                                      &AttackTuning::telegraphTime, _chargeDurationInst);
        _eliteSignatureCasts++;
        EmitEliteEvent({ EliteEventKind::Telegraph, EliteArchetype::Ogre,
                         EliteMove::OgreCharge, 0, _worldPos, _target->GetFeetWorldPos() });
        SetIdleAnimation(true);
        return;
    }

    if (Vector2Length(toTarget) > 0.01f && !IsFrozen())
    {
        Vector2 moveDir = Vector2Normalize(toTarget);

        // Blend 30% toward the player when following waypoints so path refreshes
        // don't cause a sharp direction snap (same smoothing the grunt uses).
        bool usingWaypoints = (!_waypoints.empty() && _waypointIndex < (int)_waypoints.size());
        if (usingWaypoints && !hasNavigationTarget)
        {
            Vector2 toPlayer = Vector2Subtract(_target->GetFeetWorldPos(), _worldPos);
            if (Vector2Length(toPlayer) > 0.01f)
                moveDir = Vector2Add(moveDir, Vector2Scale(Vector2Normalize(toPlayer), 0.3f));
            if (Vector2Length(moveDir) > 0.01f)
                moveDir = Vector2Normalize(moveDir);
        }

        _worldPos = Vector2Add(_worldPos, Vector2Scale(moveDir, _speed * dt));

        if (moveDir.x < -0.01f) _rightLeft = -1.f;
        if (moveDir.x >  0.01f) _rightLeft =  1.f;

        SetWalkAnimation(_texture.id != _walkAnim.id);
    }
    else
    {
        SetIdleAnimation(_texture.id != _idleAnim.id);
    }
}

void Ogre::HandleCharging(float dt)
{
    SetIdleAnimation(_texture.id != _idleAnim.id);
    _velocity = Vector2Zero();

    float dx = _target->GetFeetWorldPos().x - _worldPos.x;
    if      (dx < -20.f) _rightLeft = -1.f;
    else if (dx >  20.f) _rightLeft =  1.f;

    _chargeTimer += dt;
    if (_chargeTimer >= _activeChargeDuration)
    {
        _rushState = RushState::Rushing;
        _chargeTimer = 0.f;
        _rushedEnemies.clear();
        // LOCK: the direction is committed here and never rotates during travel.
        _rushDirection = Vector2Subtract(_target->GetFeetWorldPos(), _worldPos);
        if (Vector2Length(_rushDirection) > 0.01f)
            _rushDirection = Vector2Normalize(_rushDirection);
        else
            _rushDirection = Vector2{ (float)_rightLeft, 0.f };

        if (_rushDirection.x < -0.01f) _rightLeft = -1.f;
        if (_rushDirection.x >  0.01f) _rightLeft =  1.f;

        EmitEliteEvent({ EliteEventKind::Lock, EliteArchetype::Ogre,
                         EliteMove::OgreCharge, 0, _worldPos, {}, _rushDirection });
        EmitEliteEvent({ EliteEventKind::Execute, EliteArchetype::Ogre,
                         EliteMove::OgreCharge, 0, _worldPos, {}, _rushDirection });
        _attacking = true;
        SetRushAnimation(true);
    }
}

void Ogre::HandleRush(float dt, const std::vector<std::unique_ptr<Enemy>>& enemies)
{
    if (IsFrozen())
        return;

    _worldPos = Vector2Add(_worldPos, Vector2Scale(_rushDirection, _rushSpeed * dt));

    for (const auto& enemy : enemies)
    {
        if (enemy.get() == this || !enemy->IsActive() || !enemy->IsAlive())
            continue;
        if (HasAlreadyRushedEnemy(enemy.get()))
            continue;

        if (CheckCollisionRecs(GetCollisionRec(), enemy->GetCollisionRec()))
        {
            // Push direction: away from the Ogre's centre so enemies that are
            // to the side get knocked sideways, not all forward. Add a random
            // lateral kick for extra scatter between enemies in the same pile.
            Vector2 away = Vector2Subtract(enemy->GetWorldPos(), _worldPos);
            Vector2 pushDir = (Vector2Length(away) > 0.01f)
                ? Vector2Normalize(away)
                : _rushDirection;

            Vector2 lateral{ -_rushDirection.y, _rushDirection.x };
            float sideSign = (GetRandomValue(0, 1) == 0) ? 1.f : -1.f;
            pushDir = Vector2Add(pushDir, Vector2Scale(lateral, 0.5f * sideSign));
            if (Vector2Length(pushDir) > 0.01f)
                pushDir = Vector2Normalize(pushDir);

            // Grunts and Cyclops get the full sustained push (slides to wall/prop).
            // Ogres and bosses just get the impulse scatter instead.
            if (!enemy->AsOgre() && !enemy->IsBoss())
                enemy->StartForcedPush(pushDir, _playerPushSpeed);
            else
                enemy->ApplyExternalImpulse(Vector2Scale(pushDir, _playerPushSpeed * 6.f), true);
            enemy->PlayHurtSound();
            PlayRushHitSound();
            _rushedEnemies.push_back(enemy.get());
        }
    }

    if (_target != nullptr && _target->IsAlive() &&
        !_target->IsBeingForcedPushed() &&
        CheckCollisionRecs(GetCollisionRec(), _target->GetCollisionRec()))
    {
        PlayRushHitSound();
        _target->TakeDamage((int)_attackPower, _worldPos);
        _target->StartForcedPush(_rushDirection, _playerPushSpeed);
        _eliteSignatureHits++;
        FinishRush(false);
    }
}

void Ogre::HandleAnimation(float dt)
{
    _runningTime += dt;

    if (_runningTime < _updateTime)
        return;

    _runningTime = 0.f;
    _frame++;

    if (_frame < _maxFrames)
        return;

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

    if (_rushState == RushState::Rushing)
    {
        _frame = 0;
        return;
    }

    _frame = 0;
}

void Ogre::ScatterEnemy(Enemy& enemy) const
{
    // Rush collisions should throw enemies far out of the ogre's lane. The
    // push direction starts from the actual contact side, then gets a small
    // lateral kick so piles split apart instead of stacking into one line.
    Vector2 away = Vector2Subtract(enemy.GetWorldPos(), _worldPos);
    if (Vector2Length(away) <= 0.01f)
        away = _rushDirection;
    else
        away = Vector2Normalize(away);

    Vector2 perpendicular{ -_rushDirection.y, _rushDirection.x };
    float sideSign = (GetRandomValue(0, 1) == 0) ? -1.f : 1.f;
    Vector2 lateralKick = Vector2Scale(perpendicular, 0.35f * sideSign);

    Vector2 impulseDir = Vector2Add(away, lateralKick);
    if (Vector2Length(impulseDir) <= 0.01f)
        impulseDir = away;
    else
        impulseDir = Vector2Normalize(impulseDir);

    enemy.ApplyExternalImpulse(Vector2Scale(impulseDir, _scatterImpulse), true);
}

void Ogre::ApplyElectricCharge()
{
    if (_dying || !IsAlive())
        return;

    _isCharged = true;

    // Interrupt any charging or rushing state
    if (_rushState == RushState::Charging || _rushState == RushState::Rushing)
    {
        _rushState   = RushState::Repositioning;
        _chargeTimer = 0.f;
        _attacking   = false;
    }

    SetHurtAnimation(true);
    _takingDamage       = true;
    _hitTimer           = _maxFrames * (1.f / 12.f) + 0.1f;
    _chargeNextStunTime = (float)GetRandomValue(150, 400) / 100.f;
}

bool Ogre::HasAlreadyRushedEnemy(const Enemy* enemy) const
{
    return std::find(_rushedEnemies.begin(), _rushedEnemies.end(), enemy) != _rushedEnemies.end();
}

void Ogre::DrawEliteTelegraph() const
{
    // Charge lane warning: drawn in world space by CombatDirector's world pass.
    // While charging, the lane follows the player (that IS the telegraph); the
    // direction locks the moment the rush starts, and the warning disappears.
    if (_rushState != RushState::Charging && _rushState != RushState::Retargeting)
        return;
    if (_target == nullptr)
        return;

    Vector2 aim = Vector2Subtract(_target->GetFeetWorldPos(), _worldPos);
    if (Vector2Length(aim) < 0.01f)
        aim = Vector2{ (float)_rightLeft, 0.f };
    aim = Vector2Normalize(aim);

    const float laneLength = 900.f;
    const float laneHalfWidth = 70.f;
    Vector2 laneEnd = Vector2Add(_worldPos, Vector2Scale(aim, laneLength));
    Vector2 side{ -aim.y, aim.x };

    // Retargeting shows a fainter lane (the second lock has not happened yet).
    const float chargeRatio = (_rushState == RushState::Charging && _activeChargeDuration > 0.f)
        ? std::min(1.f, _chargeTimer / _activeChargeDuration) : 0.25f;
    Color laneColor = Fade(Color{ 255, 80, 60, 255 }, 0.16f + 0.22f * chargeRatio);

    Vector2 cornerA = Vector2Add(_worldPos, Vector2Scale(side,  laneHalfWidth));
    Vector2 cornerB = Vector2Add(_worldPos, Vector2Scale(side, -laneHalfWidth));
    Vector2 cornerC = Vector2Add(laneEnd,   Vector2Scale(side, -laneHalfWidth));
    Vector2 cornerD = Vector2Add(laneEnd,   Vector2Scale(side,  laneHalfWidth));
    DrawTriangle(cornerA, cornerB, cornerC, laneColor);
    DrawTriangle(cornerA, cornerC, cornerD, laneColor);
    DrawLineEx(cornerA, cornerD, 2.f, Fade(Color{ 255, 120, 90, 255 }, 0.55f));
    DrawLineEx(cornerB, cornerC, 2.f, Fade(Color{ 255, 120, 90, 255 }, 0.55f));
}

void Ogre::DebugForceEliteSignature()
{
    if (_dying || !IsAlive() || _rushState != RushState::Repositioning)
        return;
    _rushCooldown = 0.f;
    _rushState = RushState::Charging;
    _chargeTimer = 0.f;
    _chargesRemaining = NextOgreChargeCount(IsElitePhaseTwo());
    _activeChargeDuration = _chargeDurationInst;
    SetIdleAnimation(true);
}

void Ogre::DebugForceElitePhaseTwo()
{
    // Drop health to the threshold; the shared phase latch (UpdatePhaseLatch)
    // fires the one-time SECOND WIND transition on the next frame.
    _health = std::min(_health, std::max(1.f, std::floor(_maxHealth * 0.5f)));
}

const char* Ogre::GetEliteSignatureStateName() const
{
    switch (_rushState)
    {
    case RushState::Charging:     return "Charging";
    case RushState::Rushing:      return "Rushing";
    case RushState::Retargeting:  return "Retargeting";
    case RushState::Stunned:      return "Stunned";
    default:                      return "Repositioning";
    }
}

void Ogre::FinishRush(bool stunnedOnImpact)
{
    // The engine owns the camera, so the ogre exposes a one-frame request when
    // a rush ends on an impact. That keeps the shake centralized with the rest
    // of the screen-shake system.
    _impactShakeRequested = true;
    _attacking = false;
    _rushedEnemies.clear();
    _velocity = Vector2Zero();

    _chargesRemaining = std::max(0, _chargesRemaining - 1);

    // Attack Library tuning: authored recovery (stun) and cooldown win over the
    // coded defaults; the wall-stun floor still applies regardless.
    const AttackTuning* signatureTuning = AttackTuningStore::Get("Ogre_Charge");
    const float tunedCooldown = EliteSignatureValueOr(signatureTuning,
        &AttackTuning::signatureCooldown, _chargeCooldownDurationInst);

    if (stunnedOnImpact)
    {
        // Wall impact: the PRIMARY punish window. Ends the whole sequence and
        // must stay long enough for every class to land one meaningful ability.
        _chargesRemaining = 0;
        _rushState = RushState::Stunned;
        _stunTimer = std::max(EliteSignatureValueOr(signatureTuning,
                                  &AttackTuning::recoveryTime, _stunDuration),
                              Balance::Elite::kOgreWallStunMin);
        _rushCooldown = tunedCooldown;
        EmitEliteEvent({ EliteEventKind::Recover, EliteArchetype::Ogre,
                         EliteMove::OgreCharge, 0, _worldPos });
    }
    else if (!ShouldEndOgreChargeSequence(_chargesRemaining, false))
    {
        // SECOND WIND: pause visibly, retarget, then commit the second charge.
        _rushState = RushState::Retargeting;
        _retargetTimer = Balance::Elite::kOgreRetargetPause;
        _stunTimer = 0.f;
    }
    else
    {
        _rushState = RushState::Repositioning;
        _stunTimer = 0.f;
        _rushCooldown = tunedCooldown;
        EmitEliteEvent({ EliteEventKind::Recover, EliteArchetype::Ogre,
                         EliteMove::OgreCharge, 0, _worldPos });
    }

    SetIdleAnimation(true);
}

void Ogre::EnsureSharedResourcesLoaded()
{
    if (_sharedResourcesLoaded)
        return;

    _sharedIdleAnim       = LoadTexture(AssetPath("Enemy/ChargeOrcIdle.png").c_str());
    _sharedWalkAnim       = LoadTexture(AssetPath("Enemy/ChargeOrcWalk.png").c_str());
    _sharedAttackAnim     = LoadTexture(AssetPath("Enemy/ChargeOrcAttack.png").c_str());
    _sharedTakeDamageAnim = LoadTexture(AssetPath("Enemy/ChargeOrcHurt.png").c_str());
    _sharedDeathAnim      = LoadTexture(AssetPath("Enemy/ChargeOrcDeath.png").c_str());
    _sharedRushHitSound   = LoadSound(AssetPath("Sounds/OgreChargeHit.ogg").c_str());
    _sharedWallHitSound   = LoadSound(AssetPath("Sounds/OgreHitWall.ogg").c_str());
    _sharedOgreHurtSound  = LoadSound(AssetPath("Sounds/PlayerHurt.ogg").c_str());
    _sharedOgreDeathSound = LoadSound(AssetPath("Sounds/MonsterDeath.ogg").c_str());

    _sharedResourcesLoaded = true;
}

void Ogre::UnloadSharedResources()
{
    if (!_sharedResourcesLoaded)
        return;

    UnloadTexture(_sharedIdleAnim);
    UnloadTexture(_sharedWalkAnim);
    UnloadTexture(_sharedAttackAnim);
    UnloadTexture(_sharedTakeDamageAnim);
    UnloadTexture(_sharedDeathAnim);
    UnloadSound(_sharedRushHitSound);
    UnloadSound(_sharedWallHitSound);
    UnloadSound(_sharedOgreHurtSound);
    UnloadSound(_sharedOgreDeathSound);

    _sharedIdleAnim       = Texture2D{};
    _sharedWalkAnim       = Texture2D{};
    _sharedAttackAnim     = Texture2D{};
    _sharedTakeDamageAnim = Texture2D{};
    _sharedDeathAnim      = Texture2D{};
    _sharedRushHitSound   = Sound{};
    _sharedWallHitSound   = Sound{};
    _sharedOgreHurtSound  = Sound{};
    _sharedOgreDeathSound = Sound{};
    _sharedResourcesLoaded = false;
}
