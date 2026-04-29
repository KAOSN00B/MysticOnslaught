#pragma once
#include "BaseCharacter.h"
#include "KeyBindings.h"
#include "AbilityType.h"
#include <vector>

enum class UpgradeRarity { Common, Rare, Epic };

enum class UpgradeType
{
    // ── Common ─────────────────────────────────────────────────────────────────
    AttackPower, AttackRange, MaxHealth, MaxMana, Defense, MoveSpeed,
    // ── Rare ───────────────────────────────────────────────────────────────────
    IronConstitution,  // +25% max HP (heals too)
    SwiftFeet,         // +15% move speed
    Ferocity,          // +15% attack power
    ArcaneMind,        // +40 max mana
    IronSkin,          // +8% damage reduction
    BladeEdge,         // +15% attack range
    // ── Epic ───────────────────────────────────────────────────────────────────
    WarGod,            // +20% attack power, +10% attack range
    Resilience,        // +30% max HP, heal 3
    BladeStorm,        // +18% attack power, +18% move speed
    Juggernaut,        // +20% max HP, +8% defense
    ArcaneColossus,    // +50 max mana, +15% attack power
    // ── Ability unlocks (5th-wave ability screen only) ─────────────────────────
    LearnFireSpread, LearnIceSpread, LearnElectricSpread,
    LearnFireBolt, LearnIceBolt, LearnElectricBolt,
    LearnFireUltimate, LearnIceUltimate, LearnElectricUltimate,
    // ── Ability upgrades (5th-wave ability screen only) ────────────────────────
    UpgradeFireSpread, UpgradeIceSpread, UpgradeElectricSpread,
    UpgradeFireBolt, UpgradeIceBolt, UpgradeElectricBolt,
    UpgradeFireUltimate, UpgradeIceUltimate, UpgradeElectricUltimate,
    Count   // keep last
};

class Character : public BaseCharacter
{
public:
    enum class CastType
    {
        None,
        FireSpread,
        IceSpread,
        ElectricSpread,
        FireBolt,
        IceBolt,
        ElectricBolt,
        FireUltimate,
        IceUltimate,
        ElectricUltimate,
    };

    Character();
    ~Character() override;

    void Init();
    void ReloadSounds();
    void Update(float dt);

    void TakeDamage(int damage, Vector2 attackerPos) override;
    void TakeFractionalDamage(float damage, Vector2 attackerPos);
    virtual void Death() override;

    void DrawPlayer(Vector2 cameraPos);
    void DashParticles(float h, Vector2 playerScreenCenter);
    void SetWorldPos(Vector2 pos);
    void PlayHurtSound() override;
    void StartForcedPush(Vector2 direction, float speed);
    void OnForcedPushCollision();
    void ApplyBurnTicks(float tickDelay, int tickCount, float damagePerTick, Vector2 sourcePos);
    void GrantInvulnerability(float duration);
    bool IsBeingForcedPushed() const { return _forcedPushActive; }
    bool IsForceLocked() const { return _forcedPushActive || _forcedPushStunTimer > 0.f; }
    bool IsDashing() const { return _isDashing; }
    void SetCombatLocked(bool locked)    { _combatLocked = locked; }
    void SetManaRegenPaused(bool paused) { _manaRegenPaused = paused; }

    // ── Touch input (set by Engine each frame before Update) ─────────────────
    // joystickDir is a [-1,1] normalised vector from the virtual joystick.
    // The two "JustPressed" flags are consumed inside HandleInput /
    // HandleAttackInput and must be set BEFORE Character::Update is called.
    void SetTouchDirection(Vector2 dir)  { _touchMoveDir = dir; }
    void SetTouchAttack()                { _touchAttackJustPressed = true; }
    void SetTouchDash()                  { _touchDashJustPressed   = true; }
    // Engine sets this once when starting a run so HandleAttackInput knows to
    // suppress the mouse melee path even when no real touches are present.
    void SetTouchModeEnabled(bool on)    { _touchModeEnabled = on; }

    // ── Dynamic ability slot system ──────────────────────────────────────────
    // Learns an ability if not already known and a slot is free.
    // Returns true if successfully learned.
    bool LearnAbility(AbilityType type);
    bool HasLearnedAbility(AbilityType type) const;
    void RemoveUltimateIfPresent();   // ensures at most one ultimate is held
    AbilityType GetLearnedAbility(int slot) const;
    int  GetLearnedCount()     const { return _learnedCount; }
    int  GetMaxAbilitySlots()  const { return _maxAbilitySlots; }

    // Full keybindings — includes movement, dash, attack, and ability slots
    const KeyBindings& GetBindings() const { return _bindings; }
    void               SetBindings(const KeyBindings& b) { _bindings = b; }
    // Ability keys — convenience wrappers over _bindings.ability[]
    KeyboardKey GetAbilityKey(int slot) const
        { return (slot >= 0 && slot < _maxAbilitySlots) ? _bindings.ability[slot] : KEY_NULL; }

    // Called by Engine when an ability icon is clicked or a hotkey fires
    void TriggerAbilityCast(int slot);
    bool CanCastAbility(AbilityType type) const;
    int  GetUltimateManaRequired() const;
    float GetUltimateManaWarningTimer() const { return _ultimateManaWarningTimer; }

    CastType ConsumeCastRequest();
    bool CanApplyMeleeDamage() const;
    void ConsumeMeleeDamageFrame();
    Rectangle GetAttackCollisionRec() const;
    Vector2 GetCastOrigin() const;
    Vector2 GetFacingDirection() const;
    Vector2 GetFeetWorldPos() const;
    int ConsumeHealEffectRequests();
    float GetDashCooldownPercent() const { return (_dashCooldownTime > 0.f) ? 1.f - (_dashCooldown / _dashCooldownTime) : 1.f; }

    void AddExp(int amount);
    void Heal(int amount);
    void AddGold(int amount) { _gold += amount; }
    int  GetGold()     const { return _gold; }
    int  GetLevel()    const { return _level; }
    int  GetMaxLevel() const { return _maxLevel; }
    int  GetExp()      const { return _exp; }
    int  GetExpToNext() const { return _expToNextLevel; }
    UpgradeRarity GetUpgradeRarity(UpgradeType type) const;
    int  GetAbilityLevel(AbilityType type) const;
    bool CanUpgradeAbility(AbilityType type) const;
    void UpgradeAbility(AbilityType type);
    void RemoveAbilityAtSlot(int slot);

    // Mana
    void  RestoreMana(int amount);
    int   GetMana()    const { return _mana; }
    int   GetMaxMana() const { return _maxMana; }
    float GetAttackPowerValue() const { return _attackPower; }
    float GetMoveSpeedValue() const { return _speed; }
    float GetAttackRangeMultiplierValue() const { return _attackRangeMultiplier; }
    float GetManaRegenPerSecond() const { return kManaRegenBase * _manaRegenMultiplier; }
    static constexpr int   kLevelHpGain      = 1;
    static constexpr float kLevelAttackGain  = 0.4f;
    static constexpr float kLevelDefenseGain = 0.01f;
    static constexpr int   kLevelManaGain    = 5;

    // Passive mana regen — flat rate scaled only by the regen multiplier.
    // Higher regen should always feel faster, regardless of current mana.
    // _manaRegenMultiplier scales via upgrades and (eventually) the store.
    static constexpr float kManaRegenBase = 0.2f;  // 1 mana per 5 seconds

    // Defense & upgrade
    float GetDefense() const { return _defense; }
    void  ApplyUpgrade(UpgradeType type);

    // Combat damage accessors -------------------------------------------------
    // Melee scales through _attackPower from level-ups, so it already has a
    // strong progression path. Special abilities are burst / piercing tools,
    // so they only receive a small multiplier-based increase over the run.
    int   GetMeleeDamage()            const { return (int)_attackPower; }
    float GetAbilityDamageMultiplier() const { return _abilityDamageMultiplier; }
    int   GetSpecialDamageBonus()     const;
    // Spread ability damage — all elements share the same base for now
    int   GetSpreadHitDamage(AbilityType type)   const;
    int   GetSpreadBurnDamage(AbilityType type)  const;   // fire element DoT tick
    int   GetBoltHitDamage(AbilityType type)     const;
    int   GetBoltBurnDamage(AbilityType type)    const;   // fire bolt DoT tick
    int   GetUltimateHitDamage(AbilityType type) const;

private:
    void HandleInput();
    void HandleMovement(float dt);
    void HandleAttackInput();
    bool HandleForcedPush(float dt);
    void UpdatePendingBurns(float dt);
    void ApplyBurnTickDamage(float damage, Vector2 sourcePos);
    int GetAbilityUpgradeBonus(AbilityType type) const;

    void HandleAnimation(float dt);
    bool Dashing(float dt);

    Texture _dashAnim{};
    Texture _staffAnim{};

    Vector2 _direction{};
    Vector2 _dashDirection{};
    Vector2 _forcedPushDirection{};

    // Touch-input state set by Engine before Character::Update
    Vector2 _touchMoveDir{};
    bool    _touchAttackJustPressed = false;
    bool    _touchDashJustPressed   = false;
    bool    _touchModeEnabled       = false;

    bool _attacking = false;
    bool _damageApplied = false;
    bool _combatLocked      = false;
    bool _manaRegenPaused   = false;
    bool _castingAbility = false;
    bool _isDashing = false;
    bool _dashAnimPlaying = false;
    bool _playDashParticles = false;
    bool _dashInvincible = false;
    bool        _forcedPushActive  = false;
    KeyBindings _bindings;

    // Dynamic ability slots — filled as abilities are learned.
    // _maxAbilitySlots starts at 4; a future upgrade can increase it.
    static constexpr int _hardAbilityCap = 6;   // absolute upper limit
    AbilityType _learnedAbilities[_hardAbilityCap] = {
        AbilityType::None, AbilityType::None, AbilityType::None,
        AbilityType::None, AbilityType::None, AbilityType::None
    };
    int _abilityLevels[_hardAbilityCap] = { 1, 1, 1, 1, 1, 1 };
    int _learnedCount     = 0;
    int _maxAbilitySlots  = 4;

    float _attackUpdateTime = 1.f / 14.f;
    float _staffCastUpdateTime = 1.f / 12.f;
    float _dashSpeed = 1500.f;
    float _dashDuration = 0.18f;
    float _dashTimer = 0.f;

    float _dashCooldown = 0.f;
    float _dashCooldownTime = 1.3f;
    float _invincibleTimer = 0.f;
    float _invincibleDuration = 0.4f;
    float _forcedPushSpeed = 0.f;
    float _forcedPushStunTimer = 0.f;
    float _forcedPushImpactStunDuration = 0.35f;

    CastType _queuedCast = CastType::None;
    int _pendingHealEffects = 0;

    int _exp  = 0;
    int _gold = 0;
    int _level = 1;
    int _expToNextLevel = 10;
    static constexpr int _maxLevel = 20;

    int   _mana    = 0;
    int   _maxMana = 10;
    float _ultimateManaWarningTimer = 0.f;
    float _defense = 0.f;           // 0.0 – 1.0 damage reduction fraction
    float _attackRangeMultiplier = 1.f;

    // Fractional player burn is kept separate from the normal integer hit path
    // because lavaball damage arrives as two small delayed ticks instead of one
    // large melee-style hit. Each entry counts down independently so multiple
    // projectiles can stack cleanly.
    struct PendingBurnTick
    {
        float timer = 0.f;
        float damage = 0.f;
        Vector2 sourcePos{};
    };
    std::vector<PendingBurnTick> _pendingBurnTicks;

    float _manaRegenAccum      = 0.f;    // fractional accumulator — avoids float drift on int mana
    float _manaRegenMultiplier = 1.0f;   // boosted by upgrades / future store purchases
    float _abilityDamageMultiplier = 1.0f;   // reserved for future global spell mods

    // Base ability damage values
    static constexpr float _spreadBaseDamage     = 1.0f;
    static constexpr float _spreadBurnBaseDamage = 1.0f;
    static constexpr float _boltBaseDamage       = 3.0f;   // single shot, 3× spread
    static constexpr float _boltBurnBaseDamage   = 2.0f;
    static constexpr float _ultimateBaseDamage   = 4.0f;
};
