#pragma once
#include "BaseCharacter.h"
#include "KeyBindings.h"
#include "AbilityType.h"
#include "Relic.h"
#include "PlayerClass.h"
#include "CharacterTuning.h"
#include <vector>
#include <string>

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
    IronSkin,          // +1 armour
    BladeEdge,         // +15% attack range
    // ── Epic ───────────────────────────────────────────────────────────────────
    WarGod,            // +20% attack power, +10% attack range
    Resilience,        // +30% max HP, heal 3
    BladeStorm,        // +18% attack power, +18% move speed
    Juggernaut,        // +20% max HP, +1 armour
    ArcaneColossus,    // +50 max mana, +15% attack power
    // ── Ability unlocks (5th-wave ability screen only) ─────────────────────────
    LearnFireSpread, LearnIceSpread, LearnElectricSpread,
    LearnFireBolt, LearnIceBolt, LearnElectricBolt,
    LearnFireUltimate, LearnIceUltimate, LearnElectricUltimate,
    // ── Ability upgrades (5th-wave ability screen only) ────────────────────────
    UpgradeFireSpread, UpgradeIceSpread, UpgradeElectricSpread,
    UpgradeFireBolt, UpgradeIceBolt, UpgradeElectricBolt,
    UpgradeFireUltimate, UpgradeIceUltimate, UpgradeElectricUltimate,
    // ── Warrior ability learn / upgrade ────────────────────────────────────────
    LearnWarCleave, LearnWhirlwind, LearnThrowingAxe, LearnRend, LearnShieldBash,
    LearnWarCry, LearnGroundSlam, LearnRampage, LearnEarthshatter,
    UpgradeWarCleave, UpgradeWhirlwind, UpgradeThrowingAxe, UpgradeRend, UpgradeShieldBash,
    UpgradeWarCry, UpgradeGroundSlam, UpgradeRampage, UpgradeEarthshatter,
    // ── Rogue ability learn / upgrade ──────────────────────────────────────────
    LearnFanOfKnives, LearnShadowstep, LearnPoisonVial, LearnBackstab, LearnSmokeBomb,
    LearnEviscerate, LearnDeathMark, LearnBladeDance, LearnRainOfBlades,
    UpgradeFanOfKnives, UpgradeShadowstep, UpgradePoisonVial, UpgradeBackstab, UpgradeSmokeBomb,
    UpgradeEviscerate, UpgradeDeathMark, UpgradeBladeDance, UpgradeRainOfBlades,
    // ── Ranger ability learn / upgrade ─────────────────────────────────────────
    LearnPiercingShot, LearnMultishot, LearnFrostTrap, LearnExplosiveArrow, LearnRoll,
    LearnVolley, LearnArrowStorm, LearnDeadeye, LearnPiercingBarrage,
    UpgradePiercingShot, UpgradeMultishot, UpgradeFrostTrap, UpgradeExplosiveArrow, UpgradeRoll,
    UpgradeVolley, UpgradeArrowStorm, UpgradeDeadeye, UpgradePiercingBarrage,
    // ── Paladin ability learn / upgrade ────────────────────────────────────────
    LearnSmite, LearnConsecrate, LearnShieldOfFaith, LearnHolyBolt, LearnHammerThrow,
    LearnLayOnHands, LearnDivineStorm, LearnAvengingWrath, LearnHammerOfJustice,
    UpgradeSmite, UpgradeConsecrate, UpgradeShieldOfFaith, UpgradeHolyBolt, UpgradeHammerThrow,
    UpgradeLayOnHands, UpgradeDivineStorm, UpgradeAvengingWrath, UpgradeHammerOfJustice,
    // ── Warlock ability learn / upgrade ────────────────────────────────────────
    LearnShadowBolt, LearnDrainLife, LearnCurse, LearnCorruptionPool, LearnHellfire,
    LearnSoulSiphon, LearnCataclysm, LearnDemonForm, LearnShadowNova,
    UpgradeShadowBolt, UpgradeDrainLife, UpgradeCurse, UpgradeCorruptionPool, UpgradeHellfire,
    UpgradeSoulSiphon, UpgradeCataclysm, UpgradeDemonForm, UpgradeShadowNova,
    Count   // keep last
};

// Maps between an ability and its Learn/Upgrade card types (both directions).
// Keeps the shop / reward screens data-driven instead of per-ability switches.
AbilityType AbilityForLearnType(UpgradeType type);    // None if not a Learn card
AbilityType AbilityForUpgradeType(UpgradeType type);  // None if not an Upgrade card
UpgradeType LearnTypeForAbility(AbilityType ability);
UpgradeType UpgradeTypeForAbility(AbilityType ability);

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

    // ── Class ─────────────────────────────────────────────────────────────────
    // Set BEFORE Init() at run start; Init loads the class sprites + base stats.
    void        SetClass(PlayerClass cls) { _class = cls; }
    PlayerClass GetClass() const { return _class; }
    // Appearance (hero sprite set) is chosen separately from class. Set BEFORE Init().
    void        SetAppearance(const char* prefix) { _appearancePrefix = prefix ? prefix : ""; }
    const std::string& GetAppearance() const { return _appearancePrefix; }
    bool        ClassAllows(AbilityType ability) const { return ClassAllowsAbility(_class, ability); }
    bool        UsesRangedBasic() const { return ClassUsesRangedBasic(_class); }

    void TakeDamage(int damage, Vector2 attackerPos) override;
    void TakeFractionalDamage(float damage, Vector2 attackerPos);
    virtual void Death() override;

    // Lightweight revive — resets HP, dying/hit state, and push state without reloading assets.
    // Used by DungeonRun on room entry and on death so the player never freezes.
    void Revive();

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
    // Non-elemental class abilities queue here instead of through CastType.
    AbilityType ConsumeClassAbility();

    // ── Temporary combat buffs (War Cry / Rampage etc.) ────────────────────────
    // Damage output is multiplied by this everywhere the player deals damage.
    float GetClassDamageMult() const;
    bool  IsLifestealActive() const { return _lifestealTimer > 0.f; }
    float GetLifestealFraction() const { return _lifestealTimer > 0.f ? _lifestealFraction : 0.f; }
    void  GrantDamageBuff(float mult, float duration);
    void  GrantLifesteal(float fraction, float duration);
    void  MoveTowardFacing(float distance);   // short lunge/dash for melee abilities

    bool CanApplyMeleeDamage() const;
    void ConsumeMeleeDamageFrame();
    Rectangle GetAttackCollisionRec() const;
    // Hurt/body box — uses the authored charactertuning_Player.txt when present.
    Rectangle GetCollisionRec() const override;
    float GetAttackWidthAdjust()   const { return _attackWidthAdjust; }
    float GetAttackHeightAdjust()  const { return _attackHeightAdjust; }
    void  SetAttackWidthAdjust(float v)  { _attackWidthAdjust  = v; }
    void  SetAttackHeightAdjust(float v) { _attackHeightAdjust = v; }
    Vector2 GetCastOrigin() const;
    Vector2 GetFacingDirection() const;
    Vector2 GetFeetWorldPos() const;
    int ConsumeHealEffectRequests();
    float GetDashCooldownPercent() const { return (_dashCooldownTime > 0.f) ? 1.f - (_dashCooldown / _dashCooldownTime) : 1.f; }

    void AddExp(int amount);
    void Heal(int amount);
    void AddGold(int amount) { _gold += amount; }
    int  GetGold()     const { return _gold; }
    // Gold from an enemy drop — applies the Midas Touch relic bonus.
    void AddGoldFromDrop(int amount);

    // ── Mystic Cells (meta progression currency) ─────────────────────────────
    // Carried during the run; banked with Zeph between zones; lost on death.
    void AddCells(int amount)  { _cells += amount; }
    void AddCellsFromDrop(int amount);   // applies the Soul Siphon relic bonus
    void SetCellGainMultiplier(float m) { _cellGainMultiplier = m; }
    void MultiplyCellGainMultiplier(float m) { _cellGainMultiplier *= m; }
    void ScaleMaxHealth(float mult);     // Cursed Shrine pacts (blessing/curse)
    int  GetCells()      const { return _cells; }
    int  TakeCells()           { int taken = _cells; _cells = 0; return taken; }

    // ── Relics (per-run build-defining passives) ─────────────────────────────
    void      AddRelic(RelicType type);            // marks owned + applies passive
    bool      HasRelic(RelicType type) const;
    int       GetRelicCount() const { return _relicCount; }
    RelicType GetRelicAt(int index) const;         // for HUD iteration

    // Scales one outgoing hit by every damage-affecting relic the player owns,
    // reading the target's status. Sets outCrit when Deadeye triggers.
    int  ScaleOutgoingDamage(bool targetFrozen, bool targetCharged, bool targetBurning,
                             float targetHpFraction, int baseDamage, bool& outCrit) const;

    // Called by Engine when an enemy dies; returns HP to heal from kill relics
    // (Vampirism cadence + Reaper elite/boss heal).
    int  OnEnemyKilled(bool eliteOrBoss);

    // On-kill synergy triggers (Engine reads these + the dead enemy's status).
    bool WantsWildfire()      const { return HasRelic(RelicType::Wildfire); }
    bool WantsShatterStrike() const { return HasRelic(RelicType::ShatterStrike); }
    bool WantsStormsReach()   const { return HasRelic(RelicType::StormsReach); }
    int  GetHealDropBonusPercent() const;          // Scavenger

    // Applies the permanent meta-progression bonuses at run start.
    // Called by Engine right after Init() when a new run begins.
    void ApplyMetaBonuses(int startingGold, int vitalityBonus, float manaRegenMultiplier,
                          bool fifthAbilitySlot, bool sixthAbilitySlot = false, int startingArmour = 0);

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
    void  SetBiomeDashLocked(bool b)    { _biomeDashLocked = b; }
    void  SetBiomeSlowFactor(float f)   { _biomeSlowFactor = f; }
    void  ClearBiomeDebuffs()           { _biomeDashLocked = false; _biomeSlowFactor = 1.f; }
    float GetAttackRangeMultiplierValue() const { return _attackRangeMultiplier; }
    float GetManaRegenPerSecond() const { return kManaRegenBase * _manaRegenMultiplier; }
    static constexpr int   kLevelHpGain    = 1;
    static constexpr float kLevelAttackGain = 0.4f;
    static constexpr int   kLevelManaGain   = 5;

    // Armour absorbs one direct hit before HP is affected (0–3 slots).
    static constexpr int kMaxArmour = 3;

    // Passive mana regen — flat rate scaled only by the regen multiplier.
    // Higher regen should always feel faster, regardless of current mana.
    // _manaRegenMultiplier scales via upgrades and (eventually) the store.
    static constexpr float kManaRegenBase = 0.2f;  // 1 mana per 5 seconds

    // Armour
    int  GetArmour()    const { return _armour; }
    int  GetMaxArmour() const { return _maxArmour; }
    void AddArmour(int amount);
    void ApplyUpgrade(UpgradeType type);

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

    float _attackWidthAdjust  = 0.f;   // pixel addition to melee box width (hitbox editor)
    float _attackHeightAdjust = 0.f;   // pixel addition to melee box height (hitbox editor)
    float _attackUpdateTime = 1.f / 14.f;
    float _staffCastUpdateTime = 1.f / 12.f;
    float _dashSpeed = 1500.f;
    float _dashDuration = 0.18f;
    float _dashTimer = 0.f;

    float _dashCooldown = 0.f;
    float _dashCooldownTime = 1.3f;

    // Sanctuary biome debuff zone — set each frame by Engine while player is inside a zone
    bool  _biomeDashLocked = false;
    float _biomeSlowFactor = 1.f;
    float _invincibleTimer = 0.f;
    float _invincibleDuration = 0.4f;
    float _forcedPushSpeed = 0.f;
    float _forcedPushStunTimer = 0.f;
    float _forcedPushImpactStunDuration = 0.35f;

    CastType _queuedCast = CastType::None;
    AbilityType _queuedClassAbility = AbilityType::None;
    int _pendingHealEffects = 0;

    // Temporary self-buffs (ticked down in Update).
    float _damageBuffMult   = 1.f;   // active multiplier while _damageBuffTimer > 0
    float _damageBuffTimer  = 0.f;
    float _lifestealFraction = 0.f;  // fraction of damage dealt returned as HP
    float _lifestealTimer    = 0.f;

    PlayerClass _class = PlayerClass::Mage;   // chosen at run start
    std::string _appearancePrefix;            // hero sprite set; empty = use class default
    float _cellGainMultiplier = 1.f;          // Cell Surge meta unlock (1.0 / 1.5)
    const CharacterTuning* _playerTuning = nullptr;   // authored hit/hurt colliders

    int _exp  = 0;
    int _gold = 0;
    int _cells = 0;   // carried Mystic Cells — reset every run, lost on death

    // ── Relic loadout (reset each run in Init) ───────────────────────────────
    bool _relicOwned[(int)RelicType::Count] = {};
    RelicType _relicOrder[(int)RelicType::Count] = {};   // acquisition order for HUD
    int  _relicCount    = 0;
    int  _killsSinceHeal = 0;   // Vampirism cadence counter
    int _level = 1;
    int _expToNextLevel = 10;
    static constexpr int _maxLevel = 20;

    int   _mana    = 0;
    int   _maxMana = 10;
    float _ultimateManaWarningTimer = 0.f;
    int   _armour    = 0;   // current armour slots filled
    int   _maxArmour = kMaxArmour;  // can be raised by upgrades; hard-capped at kMaxArmour
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
