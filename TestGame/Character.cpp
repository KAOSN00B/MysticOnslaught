#include "Character.h"
#include "AssetPaths.h"

#include "raymath.h"

#include <cmath>

Character::Character()
{
    _worldPos = Vector2Zero();
    _direction = Vector2Zero();
    _dashDirection = Vector2Zero();

    _hasIFrames = false;
    _dashInvincible = false;
    _isDashing = false;
    _attacking = false;
    _takingDamage = false;
    _dying = false;
    _damageApplied = false;
    _castingAbility = false;
    _playDashParticles = false;
    _queuedCast = CastType::None;
    for (int i = 0; i < _hardAbilityCap; i++) _learnedAbilities[i] = AbilityType::None;
    _learnedCount    = 0;
    _maxAbilitySlots = 4;
    _pendingHealEffects = 0;
    _forcedPushActive = false;
    _forcedPushDirection = Vector2Zero();
    _forcedPushSpeed = 0.f;
    _forcedPushStunTimer = 0.f;

    _invincibleTimer = 0.f;
    _dashTimer = 0.f;
    _dashCooldown = 0.f;
    _runningTime = 0.f;
    _frame = 0;
    _rightLeft = 1;
}

Character::~Character()
{
    UnloadTexture(_idleAnim);
    UnloadTexture(_walkAnim);
    UnloadTexture(_attackAnim);
    UnloadTexture(_staffAnim);
    UnloadTexture(_takeDamageAnim);
    UnloadTexture(_deathAnim);
    UnloadTexture(_dashAnim);

    UnloadSound(_footStepSound);
    UnloadSound(_attackSound);
    UnloadSound(_hurtSound);
    UnloadSound(_deathSound);
}

void Character::Init()
{
    if (_idleAnim.id != 0) UnloadTexture(_idleAnim);
    if (_walkAnim.id != 0) UnloadTexture(_walkAnim);
    if (_attackAnim.id != 0) UnloadTexture(_attackAnim);
    if (_staffAnim.id != 0) UnloadTexture(_staffAnim);
    if (_dashAnim.id != 0) UnloadTexture(_dashAnim);
    if (_deathAnim.id != 0) UnloadTexture(_deathAnim);
    if (_takeDamageAnim.id != 0) UnloadTexture(_takeDamageAnim);

    if (_footStepSound.frameCount != 0) UnloadSound(_footStepSound);
    if (_attackSound.frameCount != 0) UnloadSound(_attackSound);
    if (_hurtSound.frameCount != 0) UnloadSound(_hurtSound);
    if (_deathSound.frameCount != 0) UnloadSound(_deathSound);

    _idleAnim = LoadTexture(AssetPath("Hero/Hero_Idle.png").c_str());
    _walkAnim = LoadTexture(AssetPath("Hero/Hero_Walk.png").c_str());
    _attackAnim = LoadTexture(AssetPath("Hero/Hero_Slash.png").c_str());
    _staffAnim = LoadTexture(AssetPath("Hero/Hero_Staff.png").c_str());
    _dashAnim = LoadTexture(AssetPath("Hero/Hero_Dash.png").c_str());
    _deathAnim = LoadTexture(AssetPath("Hero/Hero_Death.png").c_str());
    _takeDamageAnim = LoadTexture(AssetPath("Hero/Hero_TakeDamage.png").c_str());

    _footStepSound = LoadSound(AssetPath("Sounds/FootSteps.wav").c_str());
    _attackSound = LoadSound(AssetPath("Sounds/SwordSwipe.wav").c_str());
    _hurtSound = LoadSound(AssetPath("Sounds/PlayerHurt.wav").c_str());
    _deathSound = LoadSound(AssetPath("Sounds/PlayerDeath.wav").c_str());

    _texture = _idleAnim;

    _width = 32.f;
    _height = _texture.height;
    _scale = 6.f;
    _speed = 380.f;

    _health = 8;
    _maxHealth = 8;
    _attackPower = 3.f;

    _maxFrames = _texture.width / _width;
    _updateTime = 1.f / 8.f;

    _hasIFrames = false;
    _dashInvincible = false;
    _isDashing = false;
    _attacking = false;
    _takingDamage = false;
    _dying = false;
    _damageApplied = false;
    _castingAbility = false;
    _playDashParticles = false;
    _queuedCast = CastType::None;
    for (int i = 0; i < _hardAbilityCap; i++) _learnedAbilities[i] = AbilityType::None;
    _learnedCount    = 0;
    _maxAbilitySlots = 4;
    _bindings = KeyBindings{};
    _pendingHealEffects = 0;
    _forcedPushActive = false;
    _forcedPushDirection = Vector2Zero();
    _forcedPushSpeed = 0.f;
    _forcedPushStunTimer = 0.f;

    _exp            = 0;
    _level          = 1;
    _expToNextLevel = 15;
    _pendingBurnTicks.clear();

    _mana    = 60;
    _maxMana = 60;
    _defense = 0.f;
    _attackRangeMultiplier = 1.5f;

    _invincibleTimer = 0.f;
    _dashTimer = 0.f;
    _dashCooldown = 0.f;
    _runningTime = 0.f;
    _frame = 0;
    _stepTimer = 0.f;
    _rightLeft = 1;
}

void Character::Update(float dt)
{
    _worldPosLastFrame = _worldPos;

    ApplyVelocity(dt);
    UpdateHit(dt);
    UpdateDeath(dt);
    UpdatePendingBurns(dt);

    if (!_dying && !_takingDamage)
    {
        if (!HandleForcedPush(dt))
        {
            if (_forcedPushStunTimer > 0.f)
                _forcedPushStunTimer -= dt;

            if (_forcedPushStunTimer < 0.f)
                _forcedPushStunTimer = 0.f;

            if (!IsForceLocked())
            {
                HandleInput();

                if (!Dashing(dt))
                    HandleMovement(dt);

                HandleAttackInput();
            }
        }
    }

    if (_hasIFrames)
    {
        _invincibleTimer -= dt;

        if (_invincibleTimer <= 0.f)
        {
            _invincibleTimer = 0.f;
            _hasIFrames = false;
        }
    }

    HandleAnimation(dt);
}

void Character::HandleInput()
{
    if (IsForceLocked())
        return;

    _direction = Vector2Zero();

    if (IsKeyDown(_bindings.moveLeft))  _direction.x -= 1;
    if (IsKeyDown(_bindings.moveRight)) _direction.x += 1;
    if (IsKeyDown(_bindings.moveUp))    _direction.y -= 1;
    if (IsKeyDown(_bindings.moveDown))  _direction.y += 1;

    // Ability hotkeys — blocked during wave intro or other combat-locked states
    if (_combatLocked)
        return;

    for (int i = 0; i < _learnedCount; i++)
        if (_bindings.ability[i] != KEY_NULL && IsKeyPressed(_bindings.ability[i]))
            TriggerAbilityCast(i);

    if (IsKeyPressed(_bindings.dash) && !_isDashing && _dashCooldown <= 0.f)
    {
        // Cancel any ongoing attack or cast so movement isn't blocked after the dash
        _attacking = false;
        _castingAbility = false;
        _damageApplied = false;

        _isDashing = true;
        _dashInvincible = true;
        _dashTimer = _dashDuration;
        _dashCooldown = _dashCooldownTime;
        _velocity = Vector2Zero();
        _texture = _dashAnim;
        _frame = 0;
        _runningTime = 0.f;
        _maxFrames = _texture.width / _width;
        _updateTime = 1.f / 16.f;
        _dashDirection = _direction;

        if (Vector2Length(_direction) > 0.f)
            _dashDirection = Vector2Normalize(_direction);
        else
            _dashDirection = Vector2{ (float)_rightLeft, 0.f };
    }
}

void Character::HandleMovement(float dt)
{
    if (_attacking || _castingAbility || _takingDamage || _dying || IsForceLocked())
        return;

    if (Vector2Length(_direction) > 0.f)
    {
        _worldPos = Vector2Add(_worldPos, Vector2Scale(Vector2Normalize(_direction), _speed * dt));

        if (_direction.x < 0) _rightLeft = -1;
        if (_direction.x > 0) _rightLeft = 1;

        _texture = _walkAnim;

        _stepTimer -= dt;

        if (_stepTimer <= 0.f)
        {
            PlayFootStepSound();
            _stepTimer = _stepDelay;
        }
    }
    else if (!_attacking && !_castingAbility)
    {
        _texture = _idleAnim;
        _stepTimer = 0.f;
    }
}

void Character::HandleAttackInput()
{
    if (_takingDamage || _dying || _isDashing || IsForceLocked() || _combatLocked)
        return;

    bool attackPressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON) ||
                         (_bindings.attack != KEY_NULL && IsKeyPressed(_bindings.attack));

    if (!_attacking && !_castingAbility && attackPressed)
    {
        _attacking = true;
        _damageApplied = false;
        _texture = _attackAnim;
        _frame = 0;
        _runningTime = 0.f;
        _maxFrames = _texture.width / _width;
        _updateTime = _attackUpdateTime;
        PlayAttackSound();
    }

}

void Character::TriggerAbilityCast(int slot)
{
    if (_attacking || _castingAbility || _takingDamage || _dying || _isDashing || IsForceLocked() || _combatLocked)
        return;

    if (slot < 0 || slot >= _learnedCount)
        return;

    AbilityType ability = _learnedAbilities[slot];
    if (ability == AbilityType::None)
        return;

    int cost = GetAbilityManaCost(ability);
    if (_mana < cost)
        return;

    // Ultimates drain every point of mana; other abilities deduct their fixed cost.
    if (AbilityDrainsAllMana(ability))
        _mana = 0;
    else
        _mana -= cost;

    CastType castType = CastType::None;
    switch (ability)
    {
    case AbilityType::FireSpread:     castType = CastType::FireSpread;     break;
    case AbilityType::IceSpread:      castType = CastType::IceSpread;      break;
    case AbilityType::ElectricSpread: castType = CastType::ElectricSpread; break;
    case AbilityType::FireBolt:        castType = CastType::FireBolt;        break;
    case AbilityType::IceBolt:         castType = CastType::IceBolt;         break;
    case AbilityType::ElectricBolt:    castType = CastType::ElectricBolt;    break;
    case AbilityType::FireUltimate:    castType = CastType::FireUltimate;    break;
    case AbilityType::IceUltimate:     castType = CastType::IceUltimate;     break;
    case AbilityType::ElectricUltimate:castType = CastType::ElectricUltimate;break;
    default: break;
    }

    if (castType == CastType::None)
        return;

    _castingAbility = true;
    _queuedCast     = castType;
    _texture        = _staffAnim;
    _frame          = 0;
    _runningTime    = 0.f;
    _maxFrames      = _texture.width / _width;
    _updateTime     = _staffCastUpdateTime;
}

void Character::DrawPlayer(Vector2 cameraPos)
{
    float w = _width * _scale;
    float h = _height * _scale;

    // Compute screen position: world pos relative to camera, offset to screen center
    float screenX = _worldPos.x - cameraPos.x + GetScreenWidth()  * 0.5f - w * 0.5f;
    float screenY = _worldPos.y - cameraPos.y + GetScreenHeight() * 0.5f - h * 0.5f;

    Rectangle source{ _frame * _width, 0.f, _rightLeft * _width, _height };
    Rectangle dest{ screenX, screenY, w, h };

    float shadowWidth  = w * 0.40f;
    float shadowHeight = h * 0.05f;
    float shadowOffsetX = -_rightLeft * 3.f;
    float shadowX = dest.x + w * 0.5f + shadowOffsetX;
    float shadowY = dest.y + h - 2.f;

    DrawEllipse(shadowX, shadowY, shadowWidth, shadowHeight, Color{ 0, 0, 0, 70 });
    DrawEllipse(shadowX, shadowY, shadowWidth * 0.7f, shadowHeight * 0.7f, Color{ 0, 0, 0, 40 });

    if (_playDashParticles)
    {
        Vector2 playerScreenCenter{ screenX + w * 0.5f, screenY + h * 0.5f };
        DashParticles(h, playerScreenCenter);
    }

    DrawTexturePro(_texture, source, dest, Vector2{}, 0.f, WHITE);

    // Dash recharge bar — only visible while cooling down
    if (_dashCooldown > 0.f)
    {
        float pct      = 1.f - (_dashCooldown / _dashCooldownTime);
        float barW     = 52.f;
        float barH     = 5.f;
        float barX     = screenX + w * 0.5f - barW * 0.5f;
        float barY     = screenY + h + 2.f;

        DrawRectangleRounded({ barX, barY, barW, barH }, 0.5f, 4, Fade(BLACK, 0.75f));
        DrawRectangleRounded({ barX, barY, barW * pct, barH }, 0.5f, 4, SKYBLUE);
    }
}

void Character::DashParticles(float h, Vector2 playerScreenCenter)
{
    Vector2 screenPos{ playerScreenCenter.x, playerScreenCenter.y + h * 0.15f };

    float dashPercent = _dashTimer / _dashDuration;
    float trailLength = 160.f * dashPercent;

    Vector2 dashTrail = Vector2Scale(_dashDirection, -trailLength);
    Vector2 perp{ -_dashDirection.y, _dashDirection.x };
    Vector2 offset = Vector2Scale(perp, 18.f);

    Color c = Fade(SKYBLUE, dashPercent);

    DrawLineEx(screenPos, Vector2Add(screenPos, dashTrail), 18, c);
    DrawLineEx(Vector2Add(screenPos, offset), Vector2Add(Vector2Add(screenPos, dashTrail), offset), 12, c);
    DrawLineEx(Vector2Subtract(screenPos, offset), Vector2Subtract(Vector2Add(screenPos, dashTrail), offset), 12, c);
}

void Character::HandleAnimation(float dt)
{
    // Ogre shove locks the player into the hit pose until the forced movement
    // ends on a wall or prop, which makes the impact read more clearly.
    if (_forcedPushActive)
        return;

    _runningTime += dt;

    if (_runningTime >= _updateTime)
    {
        _frame++;
        _runningTime = 0.f;

        if (_frame >= _maxFrames)
        {
            if (_dying)
            {
                _frame = _maxFrames - 1;
                return;
            }

            if (_takingDamage)
            {
                _takingDamage = false;
                _texture = _idleAnim;
                _updateTime = 1.f / 8.f;
                _maxFrames = _texture.width / _width;
                _frame = 0;
                return;
            }

            if (_attacking)
            {
                _attacking = false;
                _texture = _idleAnim;
                _updateTime = 1.f / 8.f;
                _maxFrames = _texture.width / _width;
            }

            if (_castingAbility)
            {
                _castingAbility = false;
                _texture = _idleAnim;
                _updateTime = 1.f / 8.f;
                _maxFrames = _texture.width / _width;
            }

            _frame = 0;
        }
    }
}

bool Character::Dashing(float dt)
{
    if (IsForceLocked())
    {
        _isDashing = false;
        _dashInvincible = false;
        _playDashParticles = false;
        return false;
    }

    if (_dashCooldown > 0.f)
        _dashCooldown -= dt;

    if (!_isDashing)
    {
        _playDashParticles = false;
        _dashInvincible = false;
        return false;
    }

    _playDashParticles = true;
    _dashTimer -= dt;
    _worldPos = Vector2Add(_worldPos, Vector2Scale(_dashDirection, _dashSpeed * dt));

    if (_dashTimer <= 0.f)
    {
        _isDashing = false;
        _dashInvincible = false;
        _playDashParticles = false;
        _dashTimer = 0.f;

        _texture = _idleAnim;
        _frame = 0;
        _runningTime = 0.f;
        _maxFrames = _texture.width / _width;
        _updateTime = 1.f / 8.f;
    }

    return true;
}

void Character::Death()
{
    _velocity = Vector2Zero();
}

void Character::TakeDamage(int damage, Vector2 attackerPos)
{
    if (_hasIFrames || _dashInvincible || _forcedPushActive)
        return;

    _hasIFrames = true;
    _invincibleTimer = 0.4f;

    // Apply percentage defense reduction (rounded up so 1 damage always deals at least 1)
    if (_defense > 0.f)
        damage = std::max(1, (int)std::ceil(damage * (1.f - _defense)));

    BaseCharacter::TakeDamage(damage, attackerPos);
}

void Character::TakeFractionalDamage(float damage, Vector2 attackerPos)
{
    if (_hasIFrames || _dashInvincible || _forcedPushActive)
        return;
    if (_dying || damage <= 0.f)
        return;

    // Boss contact and heavy special pressure use fractional damage so the
    // fight can stay aggressive without deleting the player in a few touches.
    // This mirrors the normal player hit gate: one resolved hit grants a brief
    // invulnerability window before the next direct boss hit can land.
    _hasIFrames = true;
    _invincibleTimer = 0.4f;

    _health -= damage;

    if (_health > 0.f)
    {
        PlayHurtSound();
        return;
    }

    _health = 0.f;
    _dying = true;
    _attacking = false;
    _castingAbility = false;
    _takingDamage = false;
    _forcedPushActive = false;
    _forcedPushSpeed = 0.f;
    _forcedPushDirection = Vector2Zero();
    _velocity = Vector2Zero();

    _deathTimer = 0.4f;
    _texture = _deathAnim;
    _frame = 0;
    _runningTime = 0.f;
    _maxFrames = _texture.width / _width;
    _updateTime = 1.f / 4.f;
    (void)attackerPos;
    PlayDeathSound();
}

void Character::SetWorldPos(Vector2 pos)
{
    _worldPos = pos;
}

void Character::StartForcedPush(Vector2 direction, float speed)
{
    // Ogre charge pushes the player in a fixed direction until a wall or prop
    // stops the movement. During this forced movement the player cannot act
    // and ignores incoming damage for fairness.
    _forcedPushDirection = (Vector2Length(direction) > 0.01f)
        ? Vector2Normalize(direction)
        : Vector2{ (float)_rightLeft, 0.f };
    _forcedPushSpeed = speed;
    _forcedPushActive = true;
    _forcedPushStunTimer = 0.f;

    _attacking = false;
    _castingAbility = false;
    _isDashing = false;
    _dashInvincible = false;
    _playDashParticles = false;
    _velocity = Vector2Zero();
    // Clear the hurt-flag so Character::Update doesn't block HandleForcedPush
    // behind the !_takingDamage guard (TakeDamage sets it just before this call).
    _takingDamage = false;
    _texture = _takeDamageAnim;
    // Start on the second hurt frame so the shove reads as a held impact pose
    // instead of replaying the bright white flash frame while sliding.
    _frame = 1;
    _runningTime = 0.f;
    _maxFrames = _texture.width / _width;
    _updateTime = 1.f / 12.f;
}

bool Character::HandleForcedPush(float dt)
{
    if (!_forcedPushActive)
        return false;

    _worldPos = Vector2Add(_worldPos, Vector2Scale(_forcedPushDirection, _forcedPushSpeed * dt));
    return true;
}

void Character::OnForcedPushCollision()
{
    if (!_forcedPushActive)
        return;

    // Revert to the last safe position and apply a short landing stun so the
    // player cannot instantly cancel the ogre impact by buffering an action.
    UndoMovement();
    _forcedPushActive = false;
    _forcedPushSpeed = 0.f;
    _forcedPushDirection = Vector2Zero();
    _forcedPushStunTimer = _forcedPushImpactStunDuration;
    _velocity = Vector2Zero();
}

void Character::ApplyBurnTicks(float tickDelay, int tickCount, float damagePerTick, Vector2 sourcePos)
{
    // Lavaball burn is intentionally applied as delayed fractional damage so
    // the boss pressures the player over time instead of front-loading all of
    // its damage into the projectile collision itself.
    for (int tickIndex = 0; tickIndex < tickCount; ++tickIndex)
    {
        PendingBurnTick tick;
        tick.timer = tickDelay * (float)(tickIndex + 1);
        tick.damage = damagePerTick;
        tick.sourcePos = sourcePos;
        _pendingBurnTicks.push_back(tick);
    }
}

void Character::GrantInvulnerability(float duration)
{
    // Reuse the player's normal i-frame system for wave-spawn protection so
    // all incoming damage checks continue to flow through the same gate.
    // This keeps spawn safety simple: the engine grants a fixed duration right
    // after enemies appear, and Character::TakeDamage / TakeFractionalDamage
    // automatically respect it.
    if (duration <= 0.f)
        return;

    _hasIFrames = true;
    _invincibleTimer = std::max(_invincibleTimer, duration);
}

void Character::UpdatePendingBurns(float dt)
{
    if (_pendingBurnTicks.empty() || _dying)
        return;

    int writeIndex = 0;
    for (int index = 0; index < (int)_pendingBurnTicks.size(); ++index)
    {
        PendingBurnTick tick = _pendingBurnTicks[index];
        tick.timer -= dt;

        if (tick.timer <= 0.f)
        {
            ApplyBurnTickDamage(tick.damage, tick.sourcePos);
            continue;
        }

        _pendingBurnTicks[writeIndex++] = tick;
    }

    _pendingBurnTicks.resize(writeIndex);
}

void Character::ApplyBurnTickDamage(float damage, Vector2 sourcePos)
{
    // Burn ticks bypass the direct-hit invulnerability gate because they are a
    // follow-up effect from an already-resolved projectile hit. They still use
    // the normal hurt/death feedback so the player can read the extra damage.
    (void)sourcePos;
    if (_dying || damage <= 0.f)
        return;

    _health -= damage;

    if (_health > 0.f)
    {
        PlayHurtSound();
        return;
    }

    _health = 0.f;
    _dying = true;
    _attacking = false;
    _castingAbility = false;
    _takingDamage = false;
    _forcedPushActive = false;
    _forcedPushSpeed = 0.f;
    _forcedPushDirection = Vector2Zero();
    _velocity = Vector2Zero();

    _deathTimer = 0.4f;
    _texture = _deathAnim;
    _frame = 0;
    _runningTime = 0.f;
    _maxFrames = _texture.width / _width;
    _updateTime = 1.f / 4.f;
    PlayDeathSound();
}

void Character::PlayHurtSound()
{
    float pitch = GetRandomValue(85, 110) / 100.f;
    SetSoundPitch(_hurtSound, pitch);
    SetSoundVolume(_hurtSound, 0.15f);
    PlaySound(_hurtSound);
}

bool Character::LearnAbility(AbilityType type)
{
    if (type == AbilityType::None || HasLearnedAbility(type))
        return false;
    if (_learnedCount >= _maxAbilitySlots)
        return false;

    _learnedAbilities[_learnedCount++] = type;
    return true;
}

void Character::RemoveUltimateIfPresent()
{
    for (int i = 0; i < _learnedCount; i++)
    {
        AbilityType a = _learnedAbilities[i];
        if (a == AbilityType::FireUltimate ||
            a == AbilityType::IceUltimate  ||
            a == AbilityType::ElectricUltimate)
        {
            // Shift the remaining slots down to fill the gap
            for (int j = i; j < _learnedCount - 1; j++)
                _learnedAbilities[j] = _learnedAbilities[j + 1];
            _learnedAbilities[--_learnedCount] = AbilityType::None;
            break;
        }
    }
}

bool Character::HasLearnedAbility(AbilityType type) const
{
    for (int i = 0; i < _learnedCount; i++)
        if (_learnedAbilities[i] == type) return true;
    return false;
}

AbilityType Character::GetLearnedAbility(int slot) const
{
    if (slot < 0 || slot >= _learnedCount) return AbilityType::None;
    return _learnedAbilities[slot];
}

// [SHELVED] — ammo stubs: FireBallPickup/SwordBeamPickup/FreezePickup call these
// but are never spawned. The mana system (RestoreMana / TriggerAbilityCast) is
// the only active resource path. These can be deleted once those pickup files are
// removed from the project.
void Character::AddFireballAmmo(int amount)  { (void)amount; }
void Character::AddSwordBeamAmmo(int amount) { (void)amount; }

Character::CastType Character::ConsumeCastRequest()
{
    CastType queuedCast = _queuedCast;
    _queuedCast = CastType::None;
    return queuedCast;
}

bool Character::CanApplyMeleeDamage() const
{
    return _attacking && !_damageApplied && _frame >= 2 && _frame <= 4;
}

void Character::ConsumeMeleeDamageFrame()
{
    _damageApplied = true;
}

Rectangle Character::GetAttackCollisionRec() const
{
    Rectangle attackRec = GetCollisionRec();
    attackRec.x      += _rightLeft * 8.f * _attackRangeMultiplier;
    attackRec.y      += 20.f;
    attackRec.height += 130.f;                    // vertical reach is fixed — only horizontal scales
    attackRec.width  *= _attackRangeMultiplier;
    return attackRec;
}

Vector2 Character::GetCastOrigin() const
{
    // Spawn slightly in front of the player so projectiles visually leave the body
    return Vector2Add(_worldPos, Vector2{ _rightLeft * 50.f, 0.f });
}

Vector2 Character::GetFacingDirection() const
{
    return Vector2{ (float)_rightLeft, 0.f };
}

Vector2 Character::GetFeetWorldPos() const
{
    // Enemies should steer toward the player's grounded position instead of the
    // sprite centre. Using the feet point keeps melee enemies from hovering
    // above the hero and makes their approach line up with the visible floor.
    return Vector2{
        _worldPos.x,
        _worldPos.y + (_height * _scale * 0.30f)
    };
}

int Character::ConsumeHealEffectRequests()
{
    int pending = _pendingHealEffects;
    _pendingHealEffects = 0;
    return pending;
}

void Character::AddFreezeAmmo(int amount)    { (void)amount; } // [SHELVED] — see AddFireballAmmo note above

int Character::GetSpecialDamageBonus() const
{
    // Specials now use flat damage bonuses instead of percentages so the
    // player's growth stays easy to read. The bonus still ramps by level band
    // rather than every single level to keep pickup abilities powerful without
    // letting them completely replace melee.
    if (_level <= 2)
        return 0;

    if (_level <= 5)
        return 1;

    if (_level <= 8)
        return 2;

    return 3;
}

int Character::GetSpreadHitDamage() const
{
    return (int)_spreadBaseDamage + GetSpecialDamageBonus();
}

int Character::GetSpreadBurnDamage() const
{
    return (int)_spreadBurnBaseDamage + GetSpecialDamageBonus();
}

int Character::GetBoltHitDamage() const
{
    return (int)_boltBaseDamage + GetSpecialDamageBonus();
}

int Character::GetBoltBurnDamage() const
{
    return (int)_boltBurnBaseDamage + GetSpecialDamageBonus();
}

int Character::GetSwordBeamDamage() const
{
    // Sword beam starts from a higher base because it is a rarer, directional
    // piercing ability. It still uses the same conservative flat bonus.
    return (int)_swordBeamBaseDamage + GetSpecialDamageBonus();
}

int Character::GetFreezeDamage() const
{
    // Freeze keeps its main identity as crowd control, so it uses the same
    // slow flat bonus but starts from a light 1-damage base.
    return (int)_freezeBaseDamage + GetSpecialDamageBonus();
}

void Character::AddExp(int amount)
{
    if (_level >= _maxLevel)
        return;

    _exp += amount;

    while (_exp >= _expToNextLevel && _level < _maxLevel)
    {
        _exp -= _expToNextLevel;
        _level++;
        // EXP thresholds rise by ~20 each level so the curve stays steep but
        // never becomes a pure grind wall the way doubling does past level 5.
        _expToNextLevel += 20;  // 15 → 35 → 55 → 75 → 95 ...

        _maxHealth  += 2;
        _attackPower += 2.f;
        _speed      += 10.f;
        Heal(1);
    }

    // Clamp leftover EXP at max level
    if (_level >= _maxLevel)
        _exp = 0;
}

void Character::Heal(int amount)
{
    _health += amount;

    if (_health >= _maxHealth)
        _health = _maxHealth;

    if (amount > 0)
        _pendingHealEffects += amount;
}

void Character::RestoreMana(int amount)
{
    _mana += amount;
    if (_mana > _maxMana)
        _mana = _maxMana;
}

void Character::ApplyUpgrade(UpgradeType type)
{
    switch (type)
    {
    case UpgradeType::AttackPower:
        _attackPower *= 1.08f;
        break;
    case UpgradeType::AttackRange:
        _attackRangeMultiplier *= 1.10f;
        break;
    case UpgradeType::MaxHealth:
    {
        int bonus = std::max(1, (int)std::ceil(_maxHealth * 0.12f));
        _maxHealth += bonus;
        Heal(bonus);
        break;
    }
    case UpgradeType::MaxMana:
        _maxMana += 20;
        _mana = std::min(_mana + 20, _maxMana);
        break;
    case UpgradeType::Defense:
        _defense = std::min(_defense + 0.05f, 0.60f);
        break;
    case UpgradeType::MoveSpeed:
        _speed *= 1.08f;
        break;
    case UpgradeType::LearnFireSpread:
        LearnAbility(AbilityType::FireSpread);
        break;
    case UpgradeType::LearnIceSpread:
        LearnAbility(AbilityType::IceSpread);
        break;
    case UpgradeType::LearnElectricSpread:
        LearnAbility(AbilityType::ElectricSpread);
        break;
    case UpgradeType::LearnFireBolt:
        LearnAbility(AbilityType::FireBolt);
        break;
    case UpgradeType::LearnIceBolt:
        LearnAbility(AbilityType::IceBolt);
        break;
    case UpgradeType::LearnElectricBolt:
        LearnAbility(AbilityType::ElectricBolt);
        break;
    // Ultimates are exclusive — remove any existing ultimate first so the new
    // one always fits, regardless of how full the ability bar is.
    case UpgradeType::LearnFireUltimate:
        RemoveUltimateIfPresent();
        LearnAbility(AbilityType::FireUltimate);
        break;
    case UpgradeType::LearnIceUltimate:
        RemoveUltimateIfPresent();
        LearnAbility(AbilityType::IceUltimate);
        break;
    case UpgradeType::LearnElectricUltimate:
        RemoveUltimateIfPresent();
        LearnAbility(AbilityType::ElectricUltimate);
        break;
    default:
        break;
    }
}
