#include "Character.h"
#include "VirtualCanvas.h"
#include "AssetPaths.h"
#include "VirtualCanvas.h"
#include "AttackTuning.h"

#include "raymath.h"
#include "VirtualCanvas.h"

#include <algorithm>
#include <cmath>

namespace
{
    // Hunter animation: shots (basic attack + arrow abilities) draw the bow; traps
    // and Roll fall back to the appearance's own attack ("hero push") animation.
    bool HunterUsesBowFor(AbilityType ability)
    {
        switch (ability)
        {
        case AbilityType::PiercingShot:
        case AbilityType::Multishot:
        case AbilityType::Volley:          // Puncture Shot
        case AbilityType::ArrowStorm:
        case AbilityType::Deadeye:
        case AbilityType::PiercingBarrage:
            return true;
        default:
            return false;                  // Freezing / Explosive Trap, Roll → hero push
        }
    }
}

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
    UnloadTexture(_pushAnim);
    UnloadTexture(_takeDamageAnim);
    UnloadTexture(_deathAnim);
    UnloadTexture(_dashAnim);
    UnloadTexture(_bowAnim);

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
    if (_pushAnim.id != 0) UnloadTexture(_pushAnim);
    if (_dashAnim.id != 0) UnloadTexture(_dashAnim);
    if (_deathAnim.id != 0) UnloadTexture(_deathAnim);
    if (_takeDamageAnim.id != 0) UnloadTexture(_takeDamageAnim);
    if (_bowAnim.id != 0) UnloadTexture(_bowAnim);

    if (_footStepSound.frameCount != 0) UnloadSound(_footStepSound);
    if (_attackSound.frameCount != 0) UnloadSound(_attackSound);
    if (_hurtSound.frameCount != 0) UnloadSound(_hurtSound);
    if (_deathSound.frameCount != 0) UnloadSound(_deathSound);

    // Class-specific sprite set. Weapon attack and caster staff sheets are loaded separately.
    // Appearance is independent of class: use the chosen hero sprite set if set,
    // otherwise fall back to the class's default look.
    const char* prefix = !_appearancePrefix.empty()
                       ? _appearancePrefix.c_str()
                       : GetPlayerClassInfo(_class).spritePrefix;
    _idleAnim   = LoadTexture(AssetPath(TextFormat("Hero/%s_Idle.png",   prefix)).c_str());
    _walkAnim   = LoadTexture(AssetPath(TextFormat("Hero/%s_Walk.png",   prefix)).c_str());
    // The Rogue and Warrior use the forward-thrust (stab) sword animation instead
    // of the big overhead swing. Caster basics/elemental casts need a real staff
    // pose, so load it separately instead of reusing the weapon attack sheet.
    auto loadAttackSheet = [&]() -> Texture2D {
        Texture2D t{};
        if (_class == PlayerClass::Rogue || _class == PlayerClass::Warrior)
            t = LoadTexture(AssetPath(TextFormat("Hero/%s_Stab.png", prefix)).c_str());
        if (t.id == 0)
            t = LoadTexture(AssetPath(TextFormat("Hero/%s_Attack.png", prefix)).c_str());
        return t;
    };
    auto loadStaffSheet = [&]() -> Texture2D {
        Texture2D t = LoadTexture(AssetPath(TextFormat("Hero/%s_Staff.png", prefix)).c_str());
        if (t.id == 0)
            t = LoadTexture(AssetPath("Hero/Hero_Staff.png").c_str());
        if (t.id == 0)
            t = LoadTexture(AssetPath(TextFormat("Hero/%s_Attack.png", prefix)).c_str());
        return t;
    };
    _attackAnim = loadAttackSheet();
    _staffAnim  = loadStaffSheet();
    _pushAnim   = LoadTexture(AssetPath("Hero/Hero_003_Push.png").c_str());
    // Hunter: the bow-draw sheet shown for the basic shot + arrow abilities. Each
    // hero has their OWN bow animation (Hero/<prefix>_Bow.png, extracted from the
    // Rogue Adventure character packs), so the shot matches the selected
    // appearance. Falls back to the Hunter class art only if a bow sheet is absent.
    if (_class == PlayerClass::Hunter)
    {
        _bowAnim = LoadTexture(AssetPath(TextFormat("Hero/%s_Bow.png", prefix)).c_str());
        if (_bowAnim.id == 0)
            _bowAnim = LoadTexture(AssetPath("Hero/Hunter_Attack.png").c_str());
    }

    // Per-class basic-attack tempo — heavy for the Warrior, snappy for the Rogue.
    switch (_class)
    {
    case PlayerClass::Warrior: _attackUpdateTime = 1.f / 11.f; break;   // slower, weightier
    case PlayerClass::Paladin: _attackUpdateTime = 1.f / 12.f; break;
    case PlayerClass::Rogue:   _attackUpdateTime = 1.f / 19.f; break;   // rapid jabs
    default:                   _attackUpdateTime = 1.f / 14.f; break;
    }
    _dashAnim   = LoadTexture(AssetPath(TextFormat("Hero/%s_Jump.png", prefix)).c_str());   // each hero's Jump anim, repurposed as the dash
    _deathAnim  = LoadTexture(AssetPath(TextFormat("Hero/%s_Death.png",  prefix)).c_str());
    _takeDamageAnim = LoadTexture(AssetPath(TextFormat("Hero/%s_Hurt.png", prefix)).c_str());

    _footStepSound = LoadSound(AssetPath("Sounds/FootSteps.ogg").c_str());
    _attackSound = LoadSound(AssetPath("Sounds/SwordSwipe.ogg").c_str());
    _hurtSound = LoadSound(AssetPath("Sounds/PlayerHurt.ogg").c_str());
    _deathSound = LoadSound(AssetPath("Sounds/PlayerDeath.ogg").c_str());

    _texture = _idleAnim;

    _width = 32.f;
    _height = _texture.height;
    _scale = 6.f;

    // Colliders authored in the Character Animator (charactertuning_Player.txt).
    // nullptr when the file doesn't exist → the procedural boxes are used instead.
    _playerTuning = CharacterTuningStore::Get("Player");

    // Class base stats define each class's feel before any upgrades.
    const PlayerClassInfo& classInfo = GetPlayerClassInfo(_class);
    _speed       = classInfo.baseMoveSpeed;
    _health      = (float)classInfo.baseHealth;
    _maxHealth   = (float)classInfo.baseHealth;
    _attackPower = classInfo.baseAttackPower;

    _maxFrames = _texture.width / _width;
    _updateTime = 3.0f / (float)_maxFrames;

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
    for (int i = 0; i < _hardAbilityCap; i++) _abilityLevels[i] = 1;
    _learnedCount    = 0;
    _maxAbilitySlots = 4;
    _bindings = KeyBindings{};
    _pendingHealEffects = 0;
    _forcedPushActive = false;
    _forcedPushDirection = Vector2Zero();
    _forcedPushSpeed = 0.f;
    _forcedPushStunTimer = 0.f;

    _exp            = 0;
    _gold           = 0;
    _cells          = 0;
    _level          = 1;
    _expToNextLevel = Balance::Levelling::ExpToNextForLevel(_level);

    // Relics are per-run — wipe the loadout on a fresh Init.
    for (int i = 0; i < (int)RelicType::Count; i++)
        _relicOwned[i] = false;
    _relicCount     = 0;
    _killsSinceHeal = 0;
    _pendingBurnTicks.clear();

    _mana                = 0;
    _maxMana             = classInfo.baseMana;
    _manaRegenMultiplier = classInfo.manaRegenMult;
    _abilityDamageMultiplier = 1.0f;
    _armour    = classInfo.startArmour;
    _maxArmour = kMaxArmour;  // reset max to base each run
    _attackRangeMultiplier = 1.5f;
    _shopWhetstoneHits = 0;
    _shopFreeCasts     = 0;
    _shopWardHits      = 0;
    _shopGoldPickups   = 0;
    _shopCellPickups   = 0;

    // Run-scoped Power Choice / class-identity / bargain state. Character is a
    // long-lived Engine member, so every run-scoped field MUST reset here —
    // default member initializers only apply at construction, not per run.
    _rage = 0.f; _faith = 0.f; _comboPoints = 0; _comboFraction = 0.f;
    _classResourceGainMult = 1.f;
    _abilityPowerMult      = 1.f;
    _abilityCostBonus      = 0;
    _fireDamageMult = _iceDamageMult = _electricDamageMult = 1.f;
    _comboBonusMult     = 1.f;
    _rageDecayMult      = 1.f;
    _rageFullBonus      = 0.5f;
    _retributionPerStack = 0.12f;
    _reflectPotency     = 1.f;
    _hunterMarkEvery    = 3;
    _markedBonus        = 0.30f;
    _cursedBonus        = 0.25f;
    _curseDurationMult  = 1.f;
    _lifestealPotency   = 1.f;
    _maxComboPointsRun  = kMaxComboPoints;
    _evisceratePerCombo = 0.15f;
    _poisonPotency      = 1.f;
    _healOnRoomClear    = 0;
    _shopPressureDebtRooms = 0;
    _shopWagerArmed     = false;
    _shopRareStatBought = false;
    _shopBloodPriceUsed = false;
    _telemDamageTaken   = 0.f;
    _telemHealed        = 0.f;
    _damageBuffMult = 1.f; _damageBuffTimer = 0.f;
    _lifestealFraction = 0.f; _lifestealTimer = 0.f;
    _reflectFraction = 0.f; _reflectTimer = 0.f;
    _retributionStacks = 0; _retributionTimer = 0.f;

    _invincibleTimer = 0.f;
    _dashTimer = 0.f;
    _dashCooldown = 0.f;
    _runningTime = 0.f;
    _frame = 0;
    _stepTimer = 0.f;
    _rightLeft = 1;
}

void Character::ApplyMetaBonuses(int startingGold, int vitalityBonus, float manaRegenMultiplier,
                                bool fifthAbilitySlot, bool sixthAbilitySlot, int startingArmour)
{
    // Permanent unlocks purchased at the Poe's Altar — applied once per run,
    // right after Init(), so they stack cleanly with in-run upgrades.
    _gold += startingGold;
    _maxHealth += (float)vitalityBonus;
    _health    += (float)vitalityBonus;
    _manaRegenMultiplier *= manaRegenMultiplier;
    if (fifthAbilitySlot && _maxAbilitySlots < 5)
        _maxAbilitySlots = 5;
    if (sixthAbilitySlot && _maxAbilitySlots < 6)
        _maxAbilitySlots = 6;
    if (startingArmour > 0)
        AddArmour(startingArmour);
}

// ── Relics ──────────────────────────────────────────────────────────────────
void Character::AddRelic(RelicType type)
{
    int idx = (int)type;
    if (idx < 0 || idx >= (int)RelicType::Count || _relicOwned[idx])
        return;   // stacking the same relic does nothing

    _relicOwned[idx] = true;
    if (_relicCount < (int)RelicType::Count)
        _relicOrder[_relicCount++] = type;

    // Apply the passive stat effects immediately (on-hit / on-kill relics are
    // handled by ScaleOutgoingDamage / OnEnemyKilled instead).
    switch (type)
    {
    case RelicType::StoneSkin:
        _maxArmour = std::min(6, _maxArmour + 1);
        AddArmour(1);
        break;
    case RelicType::Bulwark:
        _maxArmour = std::min(6, _maxArmour + 1);
        AddArmour(1);
        Heal(3);
        break;
    case RelicType::SecondWind:
        _maxHealth = std::ceil(_maxHealth * 1.20f);
        _health    = std::min(_health + _maxHealth * 0.20f, _maxHealth);
        break;
    case RelicType::ThornmailRunes:
        _maxHealth += 2.f;
        _health    += 2.f;
        break;
    case RelicType::GlassCannon:
        _maxHealth = std::max(1.f, _maxHealth - 2.f);
        _health    = std::min(_health, _maxHealth);
        break;
    case RelicType::ArcaneBattery:
        _maxMana += 8;
        _manaRegenMultiplier *= 1.12f;
        break;
    case RelicType::SwiftBoots:
        _speed *= 1.08f;
        break;
    case RelicType::Momentum:
        _speed *= 1.10f;
        break;
    default:
        break;   // on-hit / on-kill relics carry no passive stat change
    }
}

bool Character::HasRelic(RelicType type) const
{
    int idx = (int)type;
    return idx >= 0 && idx < (int)RelicType::Count && _relicOwned[idx];
}

RelicType Character::GetRelicAt(int index) const
{
    if (index < 0 || index >= _relicCount)
        return RelicType::Count;
    return _relicOrder[index];
}

int Character::ScaleOutgoingDamage(bool targetFrozen, bool targetCharged, bool targetBurning,
                                   float targetHpFraction, int baseDamage, bool& outCrit) const
{
    float mult = 1.f;

    // Flat global damage boosts.
    if (HasRelic(RelicType::KeenEdge))    mult += 0.15f;
    if (HasRelic(RelicType::Bloodlust))   mult += 0.25f;
    if (HasRelic(RelicType::GlassCannon)) mult += 0.35f;
    if (HasRelic(RelicType::Momentum))    mult += 0.10f;
    if (HasRelic(RelicType::Reaper))      mult += 0.15f;

    // Berserker only while the player is hurting.
    if (HasRelic(RelicType::Berserker) && _maxHealth > 0.f &&
        (_health / _maxHealth) < 0.40f)
        mult += 0.30f;

    // Status synergies.
    if (targetFrozen  && HasRelic(RelicType::Permafrost)) mult += 0.25f;
    if (targetBurning && HasRelic(RelicType::EmberHeart)) mult += 0.25f;
    if (targetCharged && HasRelic(RelicType::Overcharge)) mult += 0.25f;

    // Executioner finisher on low-HP targets.
    if (HasRelic(RelicType::Executioner) && targetHpFraction <= 0.25f)
        mult += 0.35f;

    // Deadeye crit — rolled per hit for a controlled burst.
    outCrit = false;
    if (HasRelic(RelicType::Deadeye) && GetRandomValue(1, 100) <= 20)
    {
        outCrit = true;
        mult *= 1.5f;
    }

    int scaled = (int)std::ceil((float)baseDamage * mult);
    return std::max(1, scaled);
}

int Character::OnEnemyKilled(bool eliteOrBoss)
{
    int healAmount = 0;

    if (HasRelic(RelicType::Vampirism))
    {
        _killsSinceHeal++;
        if (_killsSinceHeal >= 5)
        {
            _killsSinceHeal = 0;
            healAmount += 1;
        }
    }
    if (eliteOrBoss && HasRelic(RelicType::Reaper))
        healAmount += 4;

    if (healAmount > 0)
        Heal(healAmount);
    return healAmount;
}

int Character::GetHealDropBonusPercent() const
{
    return HasRelic(RelicType::Scavenger) ? 22 : 0;
}

void Character::AddGoldFromDrop(int amount)
{
    if (HasRelic(RelicType::MidasTouch))
        amount = (int)std::ceil(amount * 1.60f);
    if (_shopGoldPickups > 0)
    {
        amount = (int)std::ceil(amount * 1.25f);
        --_shopGoldPickups;
    }
    if (_wagerRewardMult > 1.f)                          // Cursed Wager bonus
        amount = (int)std::ceil(amount * _wagerRewardMult);
    if (_contractGoldMult > 1.f)                         // Risk Shrine contract
    {
        int before = amount;
        amount = (int)std::ceil(amount * _contractGoldMult);
        _contractBonusGold += amount - before;          // bonus portion for the toast
    }
    _gold += amount;
}

void Character::ScaleMaxHealth(float mult)
{
    // Used by Cursed Shrine pacts. Keeps current health proportional and never
    // drops max HP below 1.
    if (mult <= 0.f) return;
    _maxHealth = std::max(1.f, _maxHealth * mult);
    _health    = std::min(_health * mult, _maxHealth);
    if (_health < 1.f) _health = 1.f;
}

void Character::AddCellsFromDrop(int amount)
{
    if (_cellGainMultiplier > 1.f)                       // Cell Surge meta unlock
        amount = (int)std::ceil(amount * _cellGainMultiplier);
    if (_shopCellPickups > 0)
    {
        amount = (int)std::ceil(amount * 1.25f);
        --_shopCellPickups;
    }
    if (HasRelic(RelicType::SoulSiphon))
        amount = (int)std::ceil(amount * 1.60f);
    if (_wagerRewardMult > 1.f)                          // Cursed Wager bonus
        amount = (int)std::ceil(amount * _wagerRewardMult);
    _cells += amount;
}

void Character::ReloadSounds()
{
    if (_footStepSound.frameCount == 0)
        _footStepSound = LoadSound(AssetPath("Sounds/FootSteps.ogg").c_str());
    if (_attackSound.frameCount == 0)
        _attackSound = LoadSound(AssetPath("Sounds/SwordSwipe.ogg").c_str());
    if (_hurtSound.frameCount == 0)
        _hurtSound = LoadSound(AssetPath("Sounds/PlayerHurt.ogg").c_str());
    if (_deathSound.frameCount == 0)
        _deathSound = LoadSound(AssetPath("Sounds/PlayerDeath.ogg").c_str());
}

void Character::Update(float dt)
{
    _worldPosLastFrame = _worldPos;

    // Tick down temporary self-buffs (War Cry damage, Rampage lifesteal, etc.).
    if (_damageBuffTimer > 0.f) _damageBuffTimer -= dt;
    if (_reflectTimer    > 0.f) _reflectTimer    -= dt;
    if (_retributionTimer > 0.f) { _retributionTimer -= dt; if (_retributionTimer <= 0.f) _retributionStacks = 0; }
    if (_lifestealTimer  > 0.f) _lifestealTimer  -= dt;
    if (_basicAttackCdTimer > 0.f) _basicAttackCdTimer -= dt;

    // Warrior Rage bleeds off slowly, so it rewards STAYING in the fight rather
    // than banking a full bar between rooms.
    if (_class == PlayerClass::Warrior && _rage > 0.f)
        _rage = std::max(0.f, _rage - 4.f * _rageDecayMult * dt);   // Smoldering Fury slows this

    // Passive mana regen — paused during ultimate sequences.
    if (!_manaRegenPaused && _mana < _maxMana)
    {
        _manaRegenAccum += kManaRegenBase * _manaRegenMultiplier * dt;
        if (_manaRegenAccum >= 1.f)
        {
            int gained = (int)_manaRegenAccum;
            _manaRegenAccum -= (float)gained;
            _mana = std::min(_mana + gained, _maxMana);
        }
    }
    else
    {
        _manaRegenAccum = 0.f;
    }

    if (_ultimateManaWarningTimer > 0.f)
        _ultimateManaWarningTimer = std::max(0.f, _ultimateManaWarningTimer - dt);

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

    if (!_touchModeEnabled)
    {
        // Keyboard movement (disabled in touch mode)
        if (IsKeyDown(_bindings.moveLeft))  _direction.x -= 1;
        if (IsKeyDown(_bindings.moveRight)) _direction.x += 1;
        if (IsKeyDown(_bindings.moveUp))    _direction.y -= 1;
        if (IsKeyDown(_bindings.moveDown))  _direction.y += 1;
    }

    // Virtual joystick (only active in touch mode, zero otherwise)
    _direction.x += _touchMoveDir.x;
    _direction.y += _touchMoveDir.y;

    // Clamp combined input to unit circle
    if (Vector2Length(_direction) > 1.f)
        _direction = Vector2Normalize(_direction);

    // Combat lock blocks attacks/abilities. Village mode can opt into keeping dash.
    bool canDash = !_combatLocked || _dashAllowedWhileCombatLocked;

    if (!_combatLocked && !_touchModeEnabled)
    {
        for (int i = 0; i < _learnedCount; i++)
            if (_bindings.ability[i] != KEY_NULL && IsKeyPressed(_bindings.ability[i]))
                TriggerAbilityCast(i);
    }

    bool dashTrigger = canDash && ((!_touchModeEnabled && IsKeyPressed(_bindings.dash)) || _touchDashJustPressed);
    _touchDashJustPressed = false; // always consume

    if (dashTrigger && !_isDashing && _dashCooldown <= 0.f && !_biomeDashLocked)
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
        _worldPos = Vector2Add(_worldPos, Vector2Scale(Vector2Normalize(_direction), _speed * _biomeSlowFactor * dt));

        if (_direction.x < 0) _rightLeft = -1;
        if (_direction.x > 0) _rightLeft = 1;

        if (_texture.id != _walkAnim.id)
        {
            _texture   = _walkAnim;
            _maxFrames = _texture.width / _width;
            _updateTime = 1.f / 8.f;
        }

        _stepTimer -= dt;

        if (_stepTimer <= 0.f)
        {
            PlayFootStepSound();
            _stepTimer = _stepDelay;
        }
    }
    else if (!_attacking && !_castingAbility)
    {
        if (_texture.id != _idleAnim.id)
        {
            _texture    = _idleAnim;
            _maxFrames  = _texture.width / _width;
            _updateTime = 3.0f / (float)_maxFrames;
        }
        _stepTimer = 0.f;
    }
}

void Character::HandleAttackInput()
{
    if (_takingDamage || _dying || _isDashing || IsForceLocked() || _combatLocked)
        return;

    // _touchAttackJustPressed works in all modes (touch UI and gamepad both set it)
    bool attackPressed = _touchAttackJustPressed;

    if (!_touchModeEnabled)
    {
        // Mouse clicks on the bottom ability bar should not also count as melee
        // attacks. The HUD resolves those clicks into TriggerAbilityCast during
        // draw, so block the base attack path when the press lands in that UI row.
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            static constexpr float kSlotSize = 80.f;
            static constexpr float kSlotGap  = 10.f;
            static constexpr float kBottomPad = 12.f;
            const int   totalSlots = _maxAbilitySlots;
            const float totalW     = totalSlots * kSlotSize + (totalSlots - 1) * kSlotGap;
            const float startX     = kVirtualWidth / 2.f - totalW / 2.f;
            const float slotY      = (float)kVirtualHeight - kBottomPad - kSlotSize;
            Rectangle abilityBarBounds{ startX, slotY, totalW, kSlotSize };

            if (CheckCollisionPointRec(GetVirtualMousePos(), abilityBarBounds))
            {
                _touchAttackJustPressed = false;
                return;
            }
        }

        attackPressed |= IsMouseButtonPressed(MOUSE_LEFT_BUTTON) ||
                         (_bindings.attack != KEY_NULL && IsKeyPressed(_bindings.attack));
    }

    _touchAttackJustPressed = false; // always consume

    if (!_attacking && !_castingAbility && _basicAttackCdTimer <= 0.f && attackPressed)
    {
        _attacking = true;
        _damageApplied = false;
        // Enforce the tuned "seconds between shots". No file / cooldown 0 leaves the
        // old animation-frame cadence untouched; a larger cooldown adds a gap on top.
        if (const AttackTuning* bt = AttackTuningStore::Get(AttackTuningKeyForBasic((int)_class)))
            if (bt->hasCooldown && bt->cooldown > 0.f)
                _basicAttackCdTimer = bt->cooldown;
        // Ranged basics use their ranged-ready pose: Hunter draws bow, casters raise staff.
        if (_class == PlayerClass::Hunter && _bowAnim.id != 0)
            _texture = _bowAnim;
        else if ((_class == PlayerClass::Mage || _class == PlayerClass::Warlock) && _staffAnim.id != 0)
            _texture = _staffAnim;
        else
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

    if (!CanCastAbility(ability))
    {
        if (AbilityDrainsAllMana(ability))
            _ultimateManaWarningTimer = 1.5f;
        return;
    }

    ++_telemAbilityCasts;

    int cost = GetAbilityCost(ability);   // includes the Overload surcharge

    // Freecast Seal applies only to normal abilities; ultimates still require
    // and drain their full commitment.
    if (AbilityDrainsAllMana(ability))
        _mana = 0;
    else if (_shopFreeCasts > 0)
        --_shopFreeCasts;
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
    {
        // Non-elemental class ability (Warrior etc.): queue it for the engine and
        // play the weapon (attack) swing rather than the staff-cast animation.
        // Hunter shot abilities draw the bow; Hunter traps/Roll use the appearance
        // attack ("hero push"); every other class uses its normal attack sheet.
        _castingAbility     = true;
        _queuedClassAbility = ability;
        _texture            = (_class == PlayerClass::Hunter && _bowAnim.id != 0 && HunterUsesBowFor(ability))
                            ? _bowAnim : _attackAnim;
        _frame              = 0;
        _runningTime        = 0.f;
        _maxFrames          = _texture.width / _width;
        _updateTime         = _attackUpdateTime;
        return;
    }

    _castingAbility = true;
    _queuedCast     = castType;
    bool mageBolt = _class == PlayerClass::Mage &&
        (castType == CastType::FireBolt || castType == CastType::IceBolt || castType == CastType::ElectricBolt);
    _texture        = (mageBolt && _pushAnim.id != 0) ? _pushAnim : _staffAnim;
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
    float screenX = _worldPos.x - cameraPos.x + kVirtualWidth  * 0.5f - w * 0.5f;
    float screenY = _worldPos.y - cameraPos.y + kVirtualHeight * 0.5f - h * 0.5f;

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
    if (_damageBuffTimer > 0.f)
        DrawTexturePro(_texture, source, dest, Vector2{}, 0.f, Fade(Color{ 255, 45, 35, 255 }, 0.38f));

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

    _runningTime += dt * _biomeSlowFactor;

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
                _maxFrames = _texture.width / _width;
                _updateTime = 3.0f / (float)_maxFrames;
                _frame = 0;
                return;
            }

            if (_attacking)
            {
                _attacking = false;
                _texture = _idleAnim;
                _maxFrames = _texture.width / _width;
                _updateTime = 3.0f / (float)_maxFrames;
            }

            if (_castingAbility)
            {
                _castingAbility = false;
                _texture = _idleAnim;
                _maxFrames = _texture.width / _width;
                _updateTime = 3.0f / (float)_maxFrames;
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
        _updateTime = 3.0f / (float)_maxFrames;
    }

    return true;
}

void Character::Death()
{
    _velocity = Vector2Zero();
}

void Character::Revive()
{
    _health      = _maxHealth;
    _dying       = false;
    _takingDamage = false;
    _hitTimer    = 0.f;
    _deathTimer  = 0.4f;
    _velocity    = Vector2Zero();
    _forcedPushActive    = false;
    _forcedPushDirection = Vector2Zero();
    _forcedPushSpeed     = 0.f;
    _forcedPushStunTimer = 0.f;
    _pendingBurnTicks.clear();
    GrantInvulnerability(1.5f);  // brief i-frames so respawn isn't immediately punished
}

void Character::RefreshForRoomEntry()
{
    // Revive() minus the heal. Entering a room resets transient combat state
    // (push, burn ticks, hit flashes) but HP carries over — mistakes follow you
    // from room to room, which is the heart of the sustain rework. True
    // respawns (death -> village) still use Revive() and its full heal.
    _dying       = false;
    _takingDamage = false;
    _hitTimer    = 0.f;
    _deathTimer  = 0.4f;
    _velocity    = Vector2Zero();
    _forcedPushActive    = false;
    _forcedPushDirection = Vector2Zero();
    _forcedPushSpeed     = 0.f;
    _forcedPushStunTimer = 0.f;
    _pendingBurnTicks.clear();
    if (_health < 1.f) _health = 1.f;   // safety clamp — never enter a room dead
    GrantInvulnerability(1.5f);         // brief i-frames so entry isn't punished
}

void Character::TakeDamage(int damage, Vector2 attackerPos)
{
    if (_hasIFrames || _dashInvincible || _forcedPushActive)
        return;

    // Warrior Rage — getting hit stokes the fire. Triggers even if armour eats
    // the hit (pain is pain), mirroring how Paladin Retribution counts below.
    AddRage(12.f);

    // Paladin Faith — standing your ground and taking the blow strengthens
    // conviction. Same "even if armour absorbs it" rule as Rage/Retribution.
    AddFaith(15.f);

    // Paladin Retribution — every hit that connects fuels the counter, and Aegis
    // reflects a fraction back at the attacker. Triggers even if armour eats the
    // hit (you were still struck). No self-heal anywhere.
    if (_class == PlayerClass::Paladin)
    {
        AddRetribution(1);
        if (_reflectTimer > 0.f)
        {
            _pendingReflectDamage += damage * _reflectFraction;
            _pendingReflectPos     = attackerPos;
            _hasPendingReflect     = true;
        }
    }

    _hasIFrames = true;
    _invincibleTimer = 0.4f;

    // Armour absorbs the hit: lose one armour slot instead of HP.
    // If armour is already empty the damage falls through to HP normally.
    if (_armour > 0)
    {
        _armour--;
        return;
    }

    if (_shopWardHits > 0)
    {
        --_shopWardHits;
        damage = std::max(0, damage - 1);
        if (damage <= 0)
            return;
    }

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

    // Armour absorbs direct boss contact the same as melee hits.
    if (_armour > 0)
    {
        _armour--;
        return;
    }

    if (_shopWardHits > 0)
    {
        --_shopWardHits;
        damage = std::max(0.f, damage - 1.f);
        if (damage <= 0.f)
            return;
    }

    _health -= damage;
    _telemDamageTaken += damage;   // balance telemetry (per-room, read by Engine)

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
    _telemDamageTaken += damage;   // balance telemetry (per-room, read by Engine)

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

// IsUltimateAbility(AbilityType) is defined inline in AbilityType.h and now
// covers every class's ultimates (including the Warrior kit).

bool Character::CanCastAbility(AbilityType type) const
{
    if (type == AbilityType::None)
        return false;

    if (AbilityDrainsAllMana(type))
        return _mana >= GetUltimateManaRequired();

    if (_shopFreeCasts > 0)
        return true;

    return _mana >= GetAbilityCost(type);   // includes the Overload surcharge
}

int Character::GetUltimateManaRequired() const
{
    return (int)std::ceil((float)_maxMana * 0.70f);
}

bool Character::LearnAbility(AbilityType type)
{
    if (type == AbilityType::None || HasLearnedAbility(type))
        return false;

    if (IsUltimateAbility(type))
    {
        // Slot 3 (key 4) is permanently reserved for the ultimate.
        _learnedAbilities[3] = type;
        _abilityLevels[3]    = 1;
        if (_learnedCount < 4) _learnedCount = 4;
        return true;
    }

    // Non-ultimates fill slots 0-2 in order.
    for (int i = 0; i < 3; i++)
    {
        if (_learnedAbilities[i] == AbilityType::None)
        {
            _learnedAbilities[i] = type;
            _abilityLevels[i]    = 1;
            if (i + 1 > _learnedCount && _learnedCount < 3)
                _learnedCount = i + 1;
            return true;
        }
    }
    return false; // all three non-ultimate slots occupied
}

void Character::RemoveUltimateIfPresent()
{
    // Ultimate lives exclusively at slot 3 — just clear it.
    if (IsUltimateAbility(_learnedAbilities[3]))
    {
        _learnedAbilities[3] = AbilityType::None;
        _abilityLevels[3]    = 1;
        if (_learnedCount == 4) _learnedCount = 3;
    }
}

bool Character::HasLearnedAbility(AbilityType type) const
{
    // Scan all slots so slot 3 (ultimate) is always checked.
    for (int i = 0; i < _maxAbilitySlots; i++)
        if (_learnedAbilities[i] == type) return true;
    return false;
}

AbilityType Character::GetLearnedAbility(int slot) const
{
    if (slot < 0 || slot >= _maxAbilitySlots) return AbilityType::None;
    return _learnedAbilities[slot];
}

Character::CastType Character::ConsumeCastRequest()
{
    CastType queuedCast = _queuedCast;
    _queuedCast = CastType::None;
    return queuedCast;
}

AbilityType Character::ConsumeClassAbility()
{
    AbilityType queued = _queuedClassAbility;
    _queuedClassAbility = AbilityType::None;
    return queued;
}

// ── Temporary self-buffs ──────────────────────────────────────────────────────
float Character::GetClassDamageMult() const
{
    float m = _damageBuffTimer > 0.f ? _damageBuffMult : 1.f;
    if (_retributionTimer > 0.f)             // Paladin: recent hits taken boost damage
        m *= (1.f + _retributionStacks * _retributionPerStack);   // Zealot's Fury raises the rate
    return m;
}

void Character::GrantDamageBuff(float mult, float duration)
{
    // Refresh with the stronger of the two so re-casting never weakens the buff.
    _damageBuffMult  = std::max(_damageBuffTimer > 0.f ? _damageBuffMult : 1.f, mult);
    _damageBuffTimer = std::max(_damageBuffTimer, duration);
}

void Character::GrantLifesteal(float fraction, float duration)
{
    // Grim Harvest class card strengthens every lifesteal grant (capped so the
    // per-room lifesteal cap stays the real governor).
    _lifestealFraction = std::min(0.9f, fraction * _lifestealPotency);
    _lifestealTimer    = std::max(_lifestealTimer, duration);
}

void Character::MoveTowardFacing(float distance)
{
    // Nudge the player forward in the facing direction, used by lunge abilities.
    _worldPos.x += (_rightLeft > 0 ? 1.f : -1.f) * distance;
}

bool Character::CanApplyMeleeDamage() const
{
    return _attacking && !_damageApplied && _frame >= 2 && _frame <= 4;
}

void Character::ConsumeMeleeDamageFrame()
{
    _damageApplied = true;
}

Rectangle Character::GetCollisionRec() const
{
    // Prefer an authored body circle (Idle, then Walk) from the Player tuning.
    if (_playerTuning)
    {
        for (int slot : { 0, 1 })
        {
            const auto& b = _playerTuning->animBody[slot];
            if (b.set && b.radius > 0.f)
            {
                float sign = (_rightLeft > 0) ? 1.f : -1.f;
                float cx = _worldPos.x + sign * b.x;
                float cy = _worldPos.y + b.y;
                return Rectangle{ cx - b.radius, cy - b.radius, b.radius * 2.f, b.radius * 2.f };
            }
        }
    }
    return BaseCharacter::GetCollisionRec();
}

Rectangle Character::GetAttackCollisionRec() const
{
    // Authored melee box (Swing slot) takes precedence — precise, no class scaling.
    if (_playerTuning && _playerTuning->animMelee[2].set)
    {
        Rectangle m = _playerTuning->animMelee[2].rect;   // relative, facing right
        float rx = (_rightLeft > 0) ? (_worldPos.x + m.x)
                                    : (_worldPos.x - (m.x + m.width));   // mirror when facing left
        return Rectangle{ rx, _worldPos.y + m.y, m.width, m.height };
    }

    Rectangle body = GetCollisionRec();
    float attackWidth  = body.width  * 0.5f * _attackRangeMultiplier + _attackWidthAdjust;
    float attackHeight = std::max(4.f, body.height + _attackHeightAdjust);

    // Per-class basic-attack shape — each melee class swings differently:
    //   Warrior : wide sweeping cleave (tall arc, hits a cluster in front).
    //   Paladin : solid holy strike (a bit taller & longer than default).
    //   Rogue   : quick lunging stab (long reach, narrow vertical profile).
    switch (_class)
    {
    case PlayerClass::Warrior: attackWidth *= 1.15f; attackHeight *= 1.75f; break;
    case PlayerClass::Paladin: attackWidth *= 1.10f; attackHeight *= 1.35f; break;
    case PlayerClass::Rogue:   attackWidth *= 1.55f; attackHeight *= 0.60f; break;
    default: break;   // Mage/Hunter/Warlock fight at range; melee box stays default
    }

    float swordX;
    if (_rightLeft > 0)
        swordX = body.x + body.width;
    else
        swordX = body.x - attackWidth;

    // Keep the box vertically centred on the body box
    float swordY = body.y + (body.height - attackHeight) * 0.5f;
    return Rectangle{ swordX, swordY, attackWidth, attackHeight };
}

Vector2 Character::GetCastOrigin() const
{
    // Spawn slightly in front of the player so projectiles visually leave the body
    return Vector2Add(_worldPos, Vector2{ _rightLeft * 50.f, 0.f });
}

Vector2 Character::GetCastOrigin(float forward, float height) const
{
    // Tuned fire point: forward flips with facing, height is straight vertical.
    return Vector2Add(_worldPos, Vector2{ _rightLeft * forward, height });
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

int Character::GetSpecialDamageBonus() const
{
    // Raw level no longer grants hidden spell damage. Spell growth now comes
    // from upgrading that specific learned ability on the boss reward screen.
    return 0;
}

int Character::GetAbilityUpgradeBonus(AbilityType type) const
{
    int abilityLevel = GetAbilityLevel(type);
    if (abilityLevel <= 1)
        return 0;

    return abilityLevel - 1;
}

// All five elemental damage pipelines multiply by the per-element class-card
// dial (Pyromancy / Cryomancy / Storm Attunement) on top of the generic bonus.
int Character::GetSpreadHitDamage(AbilityType type) const
{
    return (int)((_spreadBaseDamage + GetAbilityUpgradeBonus(type)) * _abilityDamageMultiplier
                 * GetElementDamageMult(type));
}

int Character::GetSpreadBurnDamage(AbilityType type) const
{
    return (int)((_spreadBurnBaseDamage + GetAbilityUpgradeBonus(type)) * _abilityDamageMultiplier
                 * GetElementDamageMult(type));
}

int Character::GetBoltHitDamage(AbilityType type) const
{
    return (int)((_boltBaseDamage + GetAbilityUpgradeBonus(type)) * _abilityDamageMultiplier
                 * GetElementDamageMult(type));
}

int Character::GetBoltBurnDamage(AbilityType type) const
{
    return (int)((_boltBurnBaseDamage + GetAbilityUpgradeBonus(type)) * _abilityDamageMultiplier
                 * GetElementDamageMult(type));
}

int Character::GetUltimateHitDamage(AbilityType type) const
{
    return (int)((_ultimateBaseDamage + GetAbilityUpgradeBonus(type)) * _abilityDamageMultiplier
                 * GetElementDamageMult(type));
}

void Character::AddExp(int amount)
{
    if (_level >= _maxLevel)
        return;

    if (_wagerRewardMult > 1.f)                          // Cursed Wager bonus
        amount = (int)std::ceil(amount * _wagerRewardMult);
    if (_contractXpMult > 1.f)                           // Risk Shrine contract
    {
        int before = amount;
        amount = (int)std::ceil(amount * _contractXpMult);
        _contractBonusXp += amount - before;            // bonus portion for the toast
    }
    _exp += amount;

    while (_exp >= _expToNextLevel && _level < _maxLevel)
    {
        _exp -= _expToNextLevel;
        _level++;

        // Levels grant a Power Choice in Engine. No automatic HP, healing,
        // attack, mana, or sustain is bundled into the level itself.
        _expToNextLevel = Balance::Levelling::ExpToNextForLevel(_level);
    }

    // Clamp leftover EXP at max level
    if (_level >= _maxLevel)
        _exp = 0;
}

void Character::Heal(int amount)
{
    float before = _health;
    _health += amount;

    if (_health >= _maxHealth)
        _health = _maxHealth;

    if (amount > 0)
    {
        _pendingHealEffects += amount;
        _telemHealed += _health - before;   // balance telemetry (actual HP gained)
    }
}

void Character::RestoreMana(int amount)
{
    _mana += amount;
    if (_mana > _maxMana)
        _mana = _maxMana;
}

void Character::AddArmour(int amount)
{
    // Add armour slots, clamped to the maximum.
    // Armour gained beyond the cap is simply lost — there is no overflow.
    _armour += amount;
    if (_armour > _maxArmour)
        _armour = _maxArmour;
    if (_armour < 0)
        _armour = 0;
}

void Character::ApplyUpgrade(UpgradeType type)
{
    switch (type)
    {
    // ── Common ────────────────────────────────────────────────────────────────
    case UpgradeType::AttackPower:
        _attackPower += 0.5f;
        break;
    case UpgradeType::AttackRange:
        _attackRangeMultiplier += 0.08f;
        break;
    case UpgradeType::MaxHealth:
        _maxHealth += 2;
        break;
    case UpgradeType::MaxMana:
        _maxMana += 4;
        break;
    case UpgradeType::Defense:
        AddArmour(1);
        break;
    case UpgradeType::MoveSpeed:
        _speed += 12.0f;
        break;
    // ── Rare ──────────────────────────────────────────────────────────────────
    case UpgradeType::IronConstitution:
    {
        float oldMax = _maxHealth;
        _maxHealth = std::ceil(_maxHealth * 1.15f);
        Heal((int)std::max(1.f, _maxHealth - oldMax));
        break;
    }
    case UpgradeType::SwiftFeet:
        _speed *= 1.08f;
        break;
    case UpgradeType::Ferocity:
        _attackPower += std::max(1.0f, _attackPower * 0.10f);
        break;
    case UpgradeType::ArcaneMind:
        _maxMana += 8;
        _mana = std::min(_mana + 4, _maxMana);
        _manaRegenMultiplier += 0.10f;
        break;
    case UpgradeType::IronSkin:
        AddArmour(2);
        break;
    case UpgradeType::BladeEdge:
        _attackPower += 1.0f;
        _attackRangeMultiplier += 0.10f;
        break;
    // ── Epic ──────────────────────────────────────────────────────────────────
    case UpgradeType::WarGod:
        _attackPower += std::max(1.75f, _attackPower * 0.15f);
        _attackRangeMultiplier += 0.12f;
        break;
    case UpgradeType::Resilience:
    {
        float oldMax = _maxHealth;
        _maxHealth = std::ceil(_maxHealth * 1.18f);
        Heal((int)std::max(2.f, _maxHealth - oldMax));
        break;
    }
    case UpgradeType::BladeStorm:
        _attackPower += 1.5f;
        _speed *= 1.08f;
        break;
    case UpgradeType::Juggernaut:
    {
        float oldMax = _maxHealth;
        _maxHealth = std::ceil(_maxHealth * 1.12f);
        Heal((int)std::max(1.f, _maxHealth - oldMax));
        AddArmour(2);
        break;
    }
    case UpgradeType::ArcaneColossus:
        _maxMana += 10;
        _mana = std::min(_mana + 5, _maxMana);
        _attackPower += 1.5f;
        _manaRegenMultiplier += 0.15f;
        break;
    // ── Power Choice additions ────────────────────────────────────────────────
    case UpgradeType::ManaFlow:
        _maxMana += 1;
        _manaRegenMultiplier += 0.15f;
        break;
    case UpgradeType::ClassAttunement:
        _classResourceGainMult += 0.25f;
        break;
    case UpgradeType::Overload:
        // The tradeoff card: permanent ability damage, paid on every cast.
        _abilityPowerMult += 0.15f;
        _abilityCostBonus += 1;
        break;
    // -- Class-specific Power Choice cards ------------------------------------
    // Mage
    case UpgradeType::MagePyromancy:       _fireDamageMult     += 0.20f; break;
    case UpgradeType::MageInfernalMastery: _fireDamageMult     += 0.35f; break;
    case UpgradeType::MageCryomancy:       _iceDamageMult      += 0.20f; break;
    case UpgradeType::MageGlacialMastery:  _iceDamageMult      += 0.35f; break;
    case UpgradeType::MageStormAttunement: _electricDamageMult += 0.20f; break;
    case UpgradeType::MageTempestMastery:  _electricDamageMult += 0.35f; break;
    case UpgradeType::MageComboResonance:  _comboBonusMult     += 1.0f;  break;
    case UpgradeType::MageArcaneHaste:     _manaRegenMultiplier += 0.20f; break;
    // Warrior
    case UpgradeType::WarriorSmolderingFury: _rageDecayMult *= 0.5f; break;
    case UpgradeType::WarriorFuriousMight:   _rageFullBonus += 0.20f; break;
    case UpgradeType::WarriorBattleTrance:
        _classResourceGainMult += 0.25f;
        _rageFullBonus         += 0.10f;
        break;
    case UpgradeType::WarriorUnbreakable:
        AddArmour(1);
        _maxHealth += 1.f;
        break;
    case UpgradeType::WarriorColossus:
        _maxHealth   += 2.f;
        _attackPower += 0.5f;
        break;
    case UpgradeType::WarriorWarlordsReach:  _attackRangeMultiplier += 0.10f; break;
    case UpgradeType::WarriorBattleMeditation: _healOnRoomClear += 1; break;
    case UpgradeType::WarriorWeaponMaster:   _abilityPowerMult += 0.08f; break;
    // Hunter
    case UpgradeType::HunterPredatorsRhythm: _hunterMarkEvery = std::max(2, _hunterMarkEvery - 1); break;
    case UpgradeType::HunterQuarry:          _markedBonus += 0.15f; break;
    case UpgradeType::HunterApexPredator:    _markedBonus += 0.30f; break;
    case UpgradeType::HunterFletcher:        _attackPower += 0.5f;  break;
    case UpgradeType::HunterTrappersCunning: _abilityPowerMult += 0.10f; break;
    case UpgradeType::HunterSwiftQuiver:     _speed += 12.f; break;
    case UpgradeType::HunterSurvivalist:
        _maxHealth += 1.f;
        AddArmour(1);
        break;
    case UpgradeType::HunterFocusedBreathing: _manaRegenMultiplier += 0.15f; break;
    // Rogue
    case UpgradeType::RogueDeepReserves:     _maxComboPointsRun = std::min(7, _maxComboPointsRun + 1); break;
    case UpgradeType::RogueRuthlessFinisher: _evisceratePerCombo += 0.05f; break;
    case UpgradeType::RogueExsanguinate:     _evisceratePerCombo += 0.10f; break;
    case UpgradeType::RogueToxinExpert:      _poisonPotency += 0.5f; break;
    case UpgradeType::RogueMasterPoisoner:   _poisonPotency += 1.0f; break;
    case UpgradeType::RogueFleetFootwork:    _speed += 12.f; break;
    case UpgradeType::RogueShadowConditioning: _maxHealth += 1.f; break;
    case UpgradeType::RogueOpportunist:
        _classResourceGainMult += 0.25f;
        _evisceratePerCombo    += 0.05f;
        break;
    // Paladin
    case UpgradeType::PaladinHolyMight:      _abilityPowerMult += 0.10f; break;
    case UpgradeType::PaladinDivineWrath:    _abilityPowerMult += 0.18f; break;
    case UpgradeType::PaladinZealotsFury:    _retributionPerStack += 0.06f; break;
    case UpgradeType::PaladinMirroredAegis:  _reflectPotency += 0.5f; break;
    case UpgradeType::PaladinDevotion:       _classResourceGainMult += 0.15f; break;
    case UpgradeType::PaladinCrusadersVitality: _maxHealth += 2.f; break;
    case UpgradeType::PaladinSanctuary:
        AddArmour(2);
        _healOnRoomClear += 1;
        break;
    case UpgradeType::PaladinDivineConduit:
        _maxMana += 2;
        _manaRegenMultiplier += 0.15f;
        break;
    // Warlock
    case UpgradeType::WarlockGrimHarvest:    _lifestealPotency += 0.5f; break;
    case UpgradeType::WarlockDarkPact:       _cursedBonus += 0.10f; break;
    case UpgradeType::WarlockSoulBargain:    _cursedBonus += 0.25f; break;
    case UpgradeType::WarlockLingeringMalice: _curseDurationMult += 0.5f; break;
    case UpgradeType::WarlockVoidAttunement: _manaRegenMultiplier += 0.20f; break;
    case UpgradeType::WarlockCorruptedVitality: _maxHealth += 2.f; break;
    case UpgradeType::WarlockSoulConduit:
        _lifestealPotency  += 0.5f;
        _curseDurationMult += 0.25f;
        break;
    case UpgradeType::WarlockOccultPower:    _abilityPowerMult += 0.08f; break;
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
    // ── Ability upgrades (level existing ability from 1→2 or 2→3) ─────────────
    case UpgradeType::UpgradeFireSpread:     UpgradeAbility(AbilityType::FireSpread);      break;
    case UpgradeType::UpgradeIceSpread:      UpgradeAbility(AbilityType::IceSpread);       break;
    case UpgradeType::UpgradeElectricSpread: UpgradeAbility(AbilityType::ElectricSpread);  break;
    case UpgradeType::UpgradeFireBolt:       UpgradeAbility(AbilityType::FireBolt);        break;
    case UpgradeType::UpgradeIceBolt:        UpgradeAbility(AbilityType::IceBolt);         break;
    case UpgradeType::UpgradeElectricBolt:   UpgradeAbility(AbilityType::ElectricBolt);    break;
    case UpgradeType::UpgradeFireUltimate:   UpgradeAbility(AbilityType::FireUltimate);    break;
    case UpgradeType::UpgradeIceUltimate:    UpgradeAbility(AbilityType::IceUltimate);     break;
    case UpgradeType::UpgradeElectricUltimate: UpgradeAbility(AbilityType::ElectricUltimate); break;
    default:
        // Warrior (and future class) abilities are handled generically so each new
        // class only needs its enum entries + the mapping helpers above.
        if (AbilityType learn = AbilityForLearnType(type); learn != AbilityType::None)
        {
            if (IsUltimateAbility(learn))
                RemoveUltimateIfPresent();   // ults are exclusive — swap out any held ult
            LearnAbility(learn);
        }
        else if (AbilityType upg = AbilityForUpgradeType(type); upg != AbilityType::None)
        {
            UpgradeAbility(upg);
        }
        break;
    }
}

UpgradeRarity Character::GetUpgradeRarity(UpgradeType type) const
{
    // Power Choice additions sit after the epic block in the enum, so they need
    // explicit rarities before the index-range fallbacks below.
    if (type == UpgradeType::ManaFlow)                                          return UpgradeRarity::Common;
    if (type == UpgradeType::ClassAttunement || type == UpgradeType::Overload)  return UpgradeRarity::Rare;

    // Class-specific cards: explicit commons/epics, everything else rare.
    switch (type)
    {
    case UpgradeType::MageArcaneHaste:      case UpgradeType::WarriorWarlordsReach:
    case UpgradeType::WarriorWeaponMaster:  case UpgradeType::HunterFletcher:
    case UpgradeType::HunterSwiftQuiver:    case UpgradeType::HunterFocusedBreathing:
    case UpgradeType::RogueFleetFootwork:   case UpgradeType::RogueShadowConditioning:
    case UpgradeType::PaladinDevotion:      case UpgradeType::PaladinCrusadersVitality:
    case UpgradeType::WarlockVoidAttunement:case UpgradeType::WarlockCorruptedVitality:
    case UpgradeType::WarlockOccultPower:
        return UpgradeRarity::Common;
    case UpgradeType::MageInfernalMastery:  case UpgradeType::MageGlacialMastery:
    case UpgradeType::MageTempestMastery:   case UpgradeType::WarriorBattleTrance:
    case UpgradeType::WarriorColossus:      case UpgradeType::HunterPredatorsRhythm:
    case UpgradeType::HunterApexPredator:   case UpgradeType::RogueExsanguinate:
    case UpgradeType::RogueMasterPoisoner:  case UpgradeType::RogueOpportunist:
    case UpgradeType::PaladinDivineWrath:   case UpgradeType::PaladinSanctuary:
    case UpgradeType::WarlockSoulBargain:   case UpgradeType::WarlockSoulConduit:
        return UpgradeRarity::Epic;
    default: break;
    }
    if ((int)type >= (int)UpgradeType::MagePyromancy && (int)type <= (int)UpgradeType::WarlockOccultPower)
        return UpgradeRarity::Rare;

    int t = (int)type;
    if (t <= (int)UpgradeType::MoveSpeed)       return UpgradeRarity::Common;  // 0-5
    if (t <= (int)UpgradeType::BladeEdge)        return UpgradeRarity::Rare;    // 6-11
    if (t <= (int)UpgradeType::ArcaneColossus)   return UpgradeRarity::Epic;    // 12-16
    return UpgradeRarity::Rare;  // ability types — default for display purposes
}

int Character::GetAbilityLevel(AbilityType type) const
{
    for (int i = 0; i < _maxAbilitySlots; i++)
        if (_learnedAbilities[i] == type) return _abilityLevels[i];
    return 0;
}

bool Character::CanUpgradeAbility(AbilityType type) const
{
    for (int i = 0; i < _maxAbilitySlots; i++)
        if (_learnedAbilities[i] == type) return _abilityLevels[i] < 3;
    return false;
}

void Character::UpgradeAbility(AbilityType type)
{
    for (int i = 0; i < _maxAbilitySlots; i++)
    {
        if (_learnedAbilities[i] == type && _abilityLevels[i] < 3)
        {
            _abilityLevels[i]++;
            return;
        }
    }
}

// ── Ability ⇄ card-type mapping ───────────────────────────────────────────────
// Only the Warrior kit needs generic mapping; the elemental Mage cards keep their
// explicit switch cases in ApplyUpgrade for clarity/back-compat.
AbilityType AbilityForLearnType(UpgradeType type)
{
    switch (type)
    {
    case UpgradeType::LearnFireSpread:       return AbilityType::FireSpread;
    case UpgradeType::LearnIceSpread:        return AbilityType::IceSpread;
    case UpgradeType::LearnElectricSpread:   return AbilityType::ElectricSpread;
    case UpgradeType::LearnFireBolt:         return AbilityType::FireBolt;
    case UpgradeType::LearnIceBolt:          return AbilityType::IceBolt;
    case UpgradeType::LearnElectricBolt:     return AbilityType::ElectricBolt;
    case UpgradeType::LearnFireUltimate:     return AbilityType::FireUltimate;
    case UpgradeType::LearnIceUltimate:      return AbilityType::IceUltimate;
    case UpgradeType::LearnElectricUltimate: return AbilityType::ElectricUltimate;
    case UpgradeType::LearnWarCleave:    return AbilityType::WarCleave;
    case UpgradeType::LearnWhirlwind:    return AbilityType::Whirlwind;
    case UpgradeType::LearnThrowingAxe:  return AbilityType::ThrowingAxe;
    case UpgradeType::LearnRend:         return AbilityType::Rend;
    case UpgradeType::LearnShieldBash:   return AbilityType::ShieldBash;
    case UpgradeType::LearnWarCry:       return AbilityType::WarCry;
    case UpgradeType::LearnGroundSlam:   return AbilityType::GroundSlam;
    case UpgradeType::LearnRampage:      return AbilityType::Rampage;
    case UpgradeType::LearnEarthshatter: return AbilityType::Earthshatter;
    case UpgradeType::LearnFanOfKnives:  return AbilityType::FanOfKnives;
    case UpgradeType::LearnShadowstep:   return AbilityType::Shadowstep;
    case UpgradeType::LearnPoisonVial:   return AbilityType::PoisonVial;
    case UpgradeType::LearnBackstab:     return AbilityType::Backstab;
    case UpgradeType::LearnSmokeBomb:    return AbilityType::SmokeBomb;
    case UpgradeType::LearnEviscerate:   return AbilityType::Eviscerate;
    case UpgradeType::LearnDeathMark:    return AbilityType::DeathMark;
    case UpgradeType::LearnBladeDance:   return AbilityType::BladeDance;
    case UpgradeType::LearnRainOfBlades: return AbilityType::RainOfBlades;
    case UpgradeType::LearnPiercingShot:    return AbilityType::PiercingShot;
    case UpgradeType::LearnMultishot:       return AbilityType::Multishot;
    case UpgradeType::LearnFrostTrap:       return AbilityType::FrostTrap;
    case UpgradeType::LearnExplosiveArrow:  return AbilityType::ExplosiveArrow;
    case UpgradeType::LearnRoll:            return AbilityType::Roll;
    case UpgradeType::LearnVolley:          return AbilityType::Volley;
    case UpgradeType::LearnArrowStorm:      return AbilityType::ArrowStorm;
    case UpgradeType::LearnDeadeye:         return AbilityType::Deadeye;
    case UpgradeType::LearnPiercingBarrage: return AbilityType::PiercingBarrage;
    case UpgradeType::LearnSmite:           return AbilityType::Smite;
    case UpgradeType::LearnConsecrate:      return AbilityType::Consecrate;
    case UpgradeType::LearnShieldOfFaith:   return AbilityType::ShieldOfFaith;
    case UpgradeType::LearnHolyBolt:        return AbilityType::HolyBolt;
    case UpgradeType::LearnHammerThrow:     return AbilityType::HammerThrow;
    case UpgradeType::LearnLayOnHands:      return AbilityType::LayOnHands;
    case UpgradeType::LearnDivineStorm:     return AbilityType::DivineStorm;
    case UpgradeType::LearnAvengingWrath:   return AbilityType::AvengingWrath;
    case UpgradeType::LearnHammerOfJustice: return AbilityType::HammerOfJustice;
    case UpgradeType::LearnShadowBolt:      return AbilityType::ShadowBolt;
    case UpgradeType::LearnDrainLife:       return AbilityType::DrainLife;
    case UpgradeType::LearnCurse:           return AbilityType::Curse;
    case UpgradeType::LearnCorruptionPool:  return AbilityType::CorruptionPool;
    case UpgradeType::LearnHellfire:        return AbilityType::Hellfire;
    case UpgradeType::LearnSoulSiphon:      return AbilityType::SoulSiphon;
    case UpgradeType::LearnCataclysm:       return AbilityType::Cataclysm;
    case UpgradeType::LearnDemonForm:       return AbilityType::DemonForm;
    case UpgradeType::LearnShadowNova:      return AbilityType::ShadowNova;
    default:                             return AbilityType::None;
    }
}

AbilityType AbilityForUpgradeType(UpgradeType type)
{
    switch (type)
    {
    case UpgradeType::UpgradeFireSpread:       return AbilityType::FireSpread;
    case UpgradeType::UpgradeIceSpread:        return AbilityType::IceSpread;
    case UpgradeType::UpgradeElectricSpread:   return AbilityType::ElectricSpread;
    case UpgradeType::UpgradeFireBolt:         return AbilityType::FireBolt;
    case UpgradeType::UpgradeIceBolt:          return AbilityType::IceBolt;
    case UpgradeType::UpgradeElectricBolt:     return AbilityType::ElectricBolt;
    case UpgradeType::UpgradeFireUltimate:     return AbilityType::FireUltimate;
    case UpgradeType::UpgradeIceUltimate:      return AbilityType::IceUltimate;
    case UpgradeType::UpgradeElectricUltimate: return AbilityType::ElectricUltimate;
    case UpgradeType::UpgradeWarCleave:    return AbilityType::WarCleave;
    case UpgradeType::UpgradeWhirlwind:    return AbilityType::Whirlwind;
    case UpgradeType::UpgradeThrowingAxe:  return AbilityType::ThrowingAxe;
    case UpgradeType::UpgradeRend:         return AbilityType::Rend;
    case UpgradeType::UpgradeShieldBash:   return AbilityType::ShieldBash;
    case UpgradeType::UpgradeWarCry:       return AbilityType::WarCry;
    case UpgradeType::UpgradeGroundSlam:   return AbilityType::GroundSlam;
    case UpgradeType::UpgradeRampage:      return AbilityType::Rampage;
    case UpgradeType::UpgradeEarthshatter: return AbilityType::Earthshatter;
    case UpgradeType::UpgradeFanOfKnives:  return AbilityType::FanOfKnives;
    case UpgradeType::UpgradeShadowstep:   return AbilityType::Shadowstep;
    case UpgradeType::UpgradePoisonVial:   return AbilityType::PoisonVial;
    case UpgradeType::UpgradeBackstab:     return AbilityType::Backstab;
    case UpgradeType::UpgradeSmokeBomb:    return AbilityType::SmokeBomb;
    case UpgradeType::UpgradeEviscerate:   return AbilityType::Eviscerate;
    case UpgradeType::UpgradeDeathMark:    return AbilityType::DeathMark;
    case UpgradeType::UpgradeBladeDance:   return AbilityType::BladeDance;
    case UpgradeType::UpgradeRainOfBlades: return AbilityType::RainOfBlades;
    case UpgradeType::UpgradePiercingShot:    return AbilityType::PiercingShot;
    case UpgradeType::UpgradeMultishot:       return AbilityType::Multishot;
    case UpgradeType::UpgradeFrostTrap:       return AbilityType::FrostTrap;
    case UpgradeType::UpgradeExplosiveArrow:  return AbilityType::ExplosiveArrow;
    case UpgradeType::UpgradeRoll:            return AbilityType::Roll;
    case UpgradeType::UpgradeVolley:          return AbilityType::Volley;
    case UpgradeType::UpgradeArrowStorm:      return AbilityType::ArrowStorm;
    case UpgradeType::UpgradeDeadeye:         return AbilityType::Deadeye;
    case UpgradeType::UpgradePiercingBarrage: return AbilityType::PiercingBarrage;
    case UpgradeType::UpgradeSmite:           return AbilityType::Smite;
    case UpgradeType::UpgradeConsecrate:      return AbilityType::Consecrate;
    case UpgradeType::UpgradeShieldOfFaith:   return AbilityType::ShieldOfFaith;
    case UpgradeType::UpgradeHolyBolt:        return AbilityType::HolyBolt;
    case UpgradeType::UpgradeHammerThrow:     return AbilityType::HammerThrow;
    case UpgradeType::UpgradeLayOnHands:      return AbilityType::LayOnHands;
    case UpgradeType::UpgradeDivineStorm:     return AbilityType::DivineStorm;
    case UpgradeType::UpgradeAvengingWrath:   return AbilityType::AvengingWrath;
    case UpgradeType::UpgradeHammerOfJustice: return AbilityType::HammerOfJustice;
    case UpgradeType::UpgradeShadowBolt:      return AbilityType::ShadowBolt;
    case UpgradeType::UpgradeDrainLife:       return AbilityType::DrainLife;
    case UpgradeType::UpgradeCurse:           return AbilityType::Curse;
    case UpgradeType::UpgradeCorruptionPool:  return AbilityType::CorruptionPool;
    case UpgradeType::UpgradeHellfire:        return AbilityType::Hellfire;
    case UpgradeType::UpgradeSoulSiphon:      return AbilityType::SoulSiphon;
    case UpgradeType::UpgradeCataclysm:       return AbilityType::Cataclysm;
    case UpgradeType::UpgradeDemonForm:       return AbilityType::DemonForm;
    case UpgradeType::UpgradeShadowNova:      return AbilityType::ShadowNova;
    default:                               return AbilityType::None;
    }
}

UpgradeType LearnTypeForAbility(AbilityType ability)
{
    switch (ability)
    {
    case AbilityType::FireSpread:       return UpgradeType::LearnFireSpread;
    case AbilityType::IceSpread:        return UpgradeType::LearnIceSpread;
    case AbilityType::ElectricSpread:   return UpgradeType::LearnElectricSpread;
    case AbilityType::FireBolt:         return UpgradeType::LearnFireBolt;
    case AbilityType::IceBolt:          return UpgradeType::LearnIceBolt;
    case AbilityType::ElectricBolt:     return UpgradeType::LearnElectricBolt;
    case AbilityType::FireUltimate:     return UpgradeType::LearnFireUltimate;
    case AbilityType::IceUltimate:      return UpgradeType::LearnIceUltimate;
    case AbilityType::ElectricUltimate: return UpgradeType::LearnElectricUltimate;
    case AbilityType::WarCleave:        return UpgradeType::LearnWarCleave;
    case AbilityType::Whirlwind:        return UpgradeType::LearnWhirlwind;
    case AbilityType::ThrowingAxe:      return UpgradeType::LearnThrowingAxe;
    case AbilityType::Rend:             return UpgradeType::LearnRend;
    case AbilityType::ShieldBash:       return UpgradeType::LearnShieldBash;
    case AbilityType::WarCry:           return UpgradeType::LearnWarCry;
    case AbilityType::GroundSlam:       return UpgradeType::LearnGroundSlam;
    case AbilityType::Rampage:          return UpgradeType::LearnRampage;
    case AbilityType::Earthshatter:     return UpgradeType::LearnEarthshatter;
    case AbilityType::FanOfKnives:      return UpgradeType::LearnFanOfKnives;
    case AbilityType::Shadowstep:       return UpgradeType::LearnShadowstep;
    case AbilityType::PoisonVial:       return UpgradeType::LearnPoisonVial;
    case AbilityType::Backstab:         return UpgradeType::LearnBackstab;
    case AbilityType::SmokeBomb:        return UpgradeType::LearnSmokeBomb;
    case AbilityType::Eviscerate:       return UpgradeType::LearnEviscerate;
    case AbilityType::DeathMark:        return UpgradeType::LearnDeathMark;
    case AbilityType::BladeDance:       return UpgradeType::LearnBladeDance;
    case AbilityType::RainOfBlades:     return UpgradeType::LearnRainOfBlades;
    case AbilityType::PiercingShot:     return UpgradeType::LearnPiercingShot;
    case AbilityType::Multishot:        return UpgradeType::LearnMultishot;
    case AbilityType::FrostTrap:        return UpgradeType::LearnFrostTrap;
    case AbilityType::ExplosiveArrow:   return UpgradeType::LearnExplosiveArrow;
    case AbilityType::Roll:             return UpgradeType::LearnRoll;
    case AbilityType::Volley:           return UpgradeType::LearnVolley;
    case AbilityType::ArrowStorm:       return UpgradeType::LearnArrowStorm;
    case AbilityType::Deadeye:          return UpgradeType::LearnDeadeye;
    case AbilityType::PiercingBarrage:  return UpgradeType::LearnPiercingBarrage;
    case AbilityType::Smite:            return UpgradeType::LearnSmite;
    case AbilityType::Consecrate:       return UpgradeType::LearnConsecrate;
    case AbilityType::ShieldOfFaith:    return UpgradeType::LearnShieldOfFaith;
    case AbilityType::HolyBolt:         return UpgradeType::LearnHolyBolt;
    case AbilityType::HammerThrow:      return UpgradeType::LearnHammerThrow;
    case AbilityType::LayOnHands:       return UpgradeType::LearnLayOnHands;
    case AbilityType::DivineStorm:      return UpgradeType::LearnDivineStorm;
    case AbilityType::AvengingWrath:    return UpgradeType::LearnAvengingWrath;
    case AbilityType::HammerOfJustice:  return UpgradeType::LearnHammerOfJustice;
    case AbilityType::ShadowBolt:       return UpgradeType::LearnShadowBolt;
    case AbilityType::DrainLife:        return UpgradeType::LearnDrainLife;
    case AbilityType::Curse:            return UpgradeType::LearnCurse;
    case AbilityType::CorruptionPool:   return UpgradeType::LearnCorruptionPool;
    case AbilityType::Hellfire:         return UpgradeType::LearnHellfire;
    case AbilityType::SoulSiphon:       return UpgradeType::LearnSoulSiphon;
    case AbilityType::Cataclysm:        return UpgradeType::LearnCataclysm;
    case AbilityType::DemonForm:        return UpgradeType::LearnDemonForm;
    case AbilityType::ShadowNova:       return UpgradeType::LearnShadowNova;
    default:                            return UpgradeType::Count;
    }
}

UpgradeType UpgradeTypeForAbility(AbilityType ability)
{
    switch (ability)
    {
    case AbilityType::FireSpread:       return UpgradeType::UpgradeFireSpread;
    case AbilityType::IceSpread:        return UpgradeType::UpgradeIceSpread;
    case AbilityType::ElectricSpread:   return UpgradeType::UpgradeElectricSpread;
    case AbilityType::FireBolt:         return UpgradeType::UpgradeFireBolt;
    case AbilityType::IceBolt:          return UpgradeType::UpgradeIceBolt;
    case AbilityType::ElectricBolt:     return UpgradeType::UpgradeElectricBolt;
    case AbilityType::FireUltimate:     return UpgradeType::UpgradeFireUltimate;
    case AbilityType::IceUltimate:      return UpgradeType::UpgradeIceUltimate;
    case AbilityType::ElectricUltimate: return UpgradeType::UpgradeElectricUltimate;
    case AbilityType::WarCleave:        return UpgradeType::UpgradeWarCleave;
    case AbilityType::Whirlwind:        return UpgradeType::UpgradeWhirlwind;
    case AbilityType::ThrowingAxe:      return UpgradeType::UpgradeThrowingAxe;
    case AbilityType::Rend:             return UpgradeType::UpgradeRend;
    case AbilityType::ShieldBash:       return UpgradeType::UpgradeShieldBash;
    case AbilityType::WarCry:           return UpgradeType::UpgradeWarCry;
    case AbilityType::GroundSlam:       return UpgradeType::UpgradeGroundSlam;
    case AbilityType::Rampage:          return UpgradeType::UpgradeRampage;
    case AbilityType::Earthshatter:     return UpgradeType::UpgradeEarthshatter;
    case AbilityType::FanOfKnives:      return UpgradeType::UpgradeFanOfKnives;
    case AbilityType::Shadowstep:       return UpgradeType::UpgradeShadowstep;
    case AbilityType::PoisonVial:       return UpgradeType::UpgradePoisonVial;
    case AbilityType::Backstab:         return UpgradeType::UpgradeBackstab;
    case AbilityType::SmokeBomb:        return UpgradeType::UpgradeSmokeBomb;
    case AbilityType::Eviscerate:       return UpgradeType::UpgradeEviscerate;
    case AbilityType::DeathMark:        return UpgradeType::UpgradeDeathMark;
    case AbilityType::BladeDance:       return UpgradeType::UpgradeBladeDance;
    case AbilityType::RainOfBlades:     return UpgradeType::UpgradeRainOfBlades;
    case AbilityType::PiercingShot:     return UpgradeType::UpgradePiercingShot;
    case AbilityType::Multishot:        return UpgradeType::UpgradeMultishot;
    case AbilityType::FrostTrap:        return UpgradeType::UpgradeFrostTrap;
    case AbilityType::ExplosiveArrow:   return UpgradeType::UpgradeExplosiveArrow;
    case AbilityType::Roll:             return UpgradeType::UpgradeRoll;
    case AbilityType::Volley:           return UpgradeType::UpgradeVolley;
    case AbilityType::ArrowStorm:       return UpgradeType::UpgradeArrowStorm;
    case AbilityType::Deadeye:          return UpgradeType::UpgradeDeadeye;
    case AbilityType::PiercingBarrage:  return UpgradeType::UpgradePiercingBarrage;
    case AbilityType::Smite:            return UpgradeType::UpgradeSmite;
    case AbilityType::Consecrate:       return UpgradeType::UpgradeConsecrate;
    case AbilityType::ShieldOfFaith:    return UpgradeType::UpgradeShieldOfFaith;
    case AbilityType::HolyBolt:         return UpgradeType::UpgradeHolyBolt;
    case AbilityType::HammerThrow:      return UpgradeType::UpgradeHammerThrow;
    case AbilityType::LayOnHands:       return UpgradeType::UpgradeLayOnHands;
    case AbilityType::DivineStorm:      return UpgradeType::UpgradeDivineStorm;
    case AbilityType::AvengingWrath:    return UpgradeType::UpgradeAvengingWrath;
    case AbilityType::HammerOfJustice:  return UpgradeType::UpgradeHammerOfJustice;
    case AbilityType::ShadowBolt:       return UpgradeType::UpgradeShadowBolt;
    case AbilityType::DrainLife:        return UpgradeType::UpgradeDrainLife;
    case AbilityType::Curse:            return UpgradeType::UpgradeCurse;
    case AbilityType::CorruptionPool:   return UpgradeType::UpgradeCorruptionPool;
    case AbilityType::Hellfire:         return UpgradeType::UpgradeHellfire;
    case AbilityType::SoulSiphon:       return UpgradeType::UpgradeSoulSiphon;
    case AbilityType::Cataclysm:        return UpgradeType::UpgradeCataclysm;
    case AbilityType::DemonForm:        return UpgradeType::UpgradeDemonForm;
    case AbilityType::ShadowNova:       return UpgradeType::UpgradeShadowNova;
    default:                            return UpgradeType::Count;
    }
}

void Character::RemoveAbilityAtSlot(int slot)
{
    // Slot 3 is the reserved ultimate slot — clear it directly, no shift.
    if (slot == 3)
    {
        _learnedAbilities[3] = AbilityType::None;
        _abilityLevels[3]    = 1;
        if (_learnedCount == 4) _learnedCount = 3;
        return;
    }
    // Count actual non-ultimate abilities so the shift never reaches slot 3.
    int nonUltCount = 0;
    for (int i = 0; i < 3; i++)
        if (_learnedAbilities[i] != AbilityType::None) nonUltCount++;

    if (slot < 0 || slot >= nonUltCount)
        return;

    // Shift strictly within slots 0-2 — slot 3 (ultimate) is never touched.
    for (int i = slot; i < nonUltCount - 1; i++)
    {
        _learnedAbilities[i] = _learnedAbilities[i + 1];
        _abilityLevels[i]    = _abilityLevels[i + 1];
    }
    _learnedAbilities[nonUltCount - 1] = AbilityType::None;
    _abilityLevels[nonUltCount - 1]    = 1;

    // Rebuild _learnedCount from actual state so ultimate presence is preserved.
    _learnedCount = (_learnedAbilities[3] != AbilityType::None) ? 4 : nonUltCount - 1;
}
