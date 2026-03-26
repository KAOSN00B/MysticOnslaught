#pragma once
#include "BaseCharacter.h"
#include "KeyBindings.h"
#include "AbilityType.h"
#include <vector>

enum class UpgradeType
{
    // Stat boosts
    AttackPower,
    AttackRange,
    MaxHealth,
    MaxMana,
    Defense,
    MoveSpeed,
    // Ability unlocks
    LearnFireSpread,
    LearnIceSpread,
    LearnElectricSpread,
    LearnFireBolt,
    LearnIceBolt,
    LearnElectricBolt,
    LearnFireUltimate,
    LearnIceUltimate,
    LearnElectricUltimate,
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
        // [SHELVED] — no UpgradeType exists to learn these; dispatch in Engine is
        // never reached. Kept so SpawnSwordBeam/SpawnFreezeWave still compile.
        SwordBeam,
        Freeze
    };

    Character();
    ~Character() override;

    void Init();
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
    void SetCombatLocked(bool locked) { _combatLocked = locked; }

    // ── Dynamic ability slot system ──────────────────────────────────────────
    // Learns an ability if not already known and a slot is free.
    // Returns true if successfully learned.
    bool LearnAbility(AbilityType type);
    bool HasLearnedAbility(AbilityType type) const;
    void RemoveUltimateIfPresent();   // ensures at most one ultimate is held
    AbilityType GetLearnedAbility(int slot) const;
    int  GetLearnedCount()     const { return _learnedCount; }
    int  GetMaxAbilitySlots()  const { return _maxAbilitySlots; }

    // [SHELVED] — ammo stubs kept so FireBallPickup/SwordBeamPickup/FreezePickup
    // still compile. Those pickup classes are never spawned by Engine and the
    // ammo system has been fully replaced by mana (see AbilityType.h).
    void AddFireballAmmo(int amount);
    void AddSwordBeamAmmo(int amount);
    void AddFreezeAmmo(int amount);

    // Full keybindings — includes movement, dash, attack, and ability slots
    const KeyBindings& GetBindings() const { return _bindings; }
    void               SetBindings(const KeyBindings& b) { _bindings = b; }
    // Ability keys — convenience wrappers over _bindings.ability[]
    KeyboardKey GetAbilityKey(int slot) const
        { return (slot >= 0 && slot < _maxAbilitySlots) ? _bindings.ability[slot] : KEY_NULL; }

    // Called by Engine when an ability icon is clicked or a hotkey fires
    void TriggerAbilityCast(int slot);

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
    int  GetLevel() const { return _level; }
    int  GetExp() const { return _exp; }
    int  GetExpToNext() const { return _expToNextLevel; }

    // Mana
    void  RestoreMana(int amount);
    int   GetMana()    const { return _mana; }
    int   GetMaxMana() const { return _maxMana; }

    // Defense & upgrade
    float GetDefense() const { return _defense; }
    void  ApplyUpgrade(UpgradeType type);

    // Combat damage accessors -------------------------------------------------
    // Melee scales through _attackPower from level-ups, so it already has a
    // strong progression path. Special abilities are burst / piercing tools,
    // so they only receive a small multiplier-based increase over the run.
    int   GetMeleeDamage()       const { return (int)_attackPower; }
    int   GetSpecialDamageBonus() const;
    // Spread ability damage — all elements share the same base for now
    int   GetSpreadHitDamage()   const;
    int   GetSpreadBurnDamage()  const;   // fire element DoT tick
    int   GetBoltHitDamage()     const;
    int   GetBoltBurnDamage()    const;   // fire bolt DoT tick
    // [SHELVED] — damage values for the unreachable SwordBeam/Freeze cast paths
    int   GetSwordBeamDamage()   const;
    int   GetFreezeDamage()      const;

private:
    void HandleInput();
    void HandleMovement(float dt);
    void HandleAttackInput();
    bool HandleForcedPush(float dt);
    void UpdatePendingBurns(float dt);
    void ApplyBurnTickDamage(float damage, Vector2 sourcePos);

    void HandleAnimation(float dt);
    bool Dashing(float dt);

    Texture _dashAnim{};
    Texture _staffAnim{};

    Vector2 _direction{};
    Vector2 _dashDirection{};
    Vector2 _forcedPushDirection{};

    bool _attacking = false;
    bool _damageApplied = false;
    bool _combatLocked = false;
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

    int _exp = 0;
    int _level = 1;
    int _expToNextLevel = 10;
    static constexpr int _maxLevel = 10;

    int   _mana    = 60;
    int   _maxMana = 60;
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

    // Base ability damage values
    static constexpr float _spreadBaseDamage     = 1.0f;
    static constexpr float _spreadBurnBaseDamage = 1.0f;
    static constexpr float _boltBaseDamage       = 3.0f;   // single shot, 3× spread
    static constexpr float _boltBurnBaseDamage   = 2.0f;
    static constexpr float _swordBeamBaseDamage  = 2.0f;
    static constexpr float _freezeBaseDamage     = 1.0f;
};
