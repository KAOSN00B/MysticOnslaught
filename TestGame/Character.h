#pragma once
#include "BaseCharacter.h"
#include "KeyBindings.h"
#include "AbilityType.h"
#include "Relic.h"
#include "PlayerClass.h"
#include "CharacterTuning.h"
#include "GameBalance.h"
#include <algorithm>
#include <vector>
#include <string>

enum class UpgradeRarity { Common, Rare, Epic };

enum class UpgradeType
{
    // ── Common ─────────────────────────────────────────────────────────────────
    AttackPower, AttackRange, MaxHealth, MaxMana, Defense, MoveSpeed,
    // ── Rare ──────────────────────────────────────────────────────────────────
    // Comments below state the REAL ApplyUpgrade effects (balance truth pass —
    // the old comments advertised bigger numbers than the code ever gave).
    IronConstitution,  // +15% max HP (heals the gained HP)
    SwiftFeet,         // +8% move speed
    Ferocity,          // +10% attack power (min +1)
    ArcaneMind,        // +8 max mana (+4 now), +10% mana regen
    IronSkin,          // +2 armour
    BladeEdge,         // +1 attack power, +0.10x attack range
    // ── Epic ──────────────────────────────────────────────────────────────────
    WarGod,            // +15% attack power (min +1.75), +0.12x attack range
    Resilience,        // +18% max HP (heals the gained HP)
    BladeStorm,        // +1.5 attack power, +8% move speed
    Juggernaut,        // +12% max HP (heals gained), +2 armour
    ArcaneColossus,    // +10 max mana (+5 now), +1.5 attack power, +15% mana regen
    // ── Power Choice additions (level-up pool; build-shaping, not raw stats) ──
    ManaFlow,          // common: +1 max mana, +15% mana regen — tempo, not a pool dump
    ClassAttunement,   // rare: +25% class resource gain (Rage / Faith / Combo Points)
    Overload,          // rare TRADEOFF: class abilities +15% damage, all casts cost +1 mana
    // -- Class-specific Power Choice cards (offered only to their class) ------
    // Mage - elemental mastery
    MagePyromancy, MageInfernalMastery,       // fire +20% / +35%
    MageCryomancy, MageGlacialMastery,        // ice +20% / +35%
    MageStormAttunement, MageTempestMastery,  // electric +20% / +35%
    MageComboResonance,                       // elemental combo payoff x2
    MageArcaneHaste,                          // +20% mana regen
    // Warrior - rage and iron
    WarriorSmolderingFury,   // Rage decays 50% slower
    WarriorFuriousMight,     // full-Rage damage bonus +20%
    WarriorBattleTrance,     // Rage gain +25% AND full-Rage bonus +10%
    WarriorUnbreakable,      // +1 armour, +1 max HP
    WarriorColossus,         // +2 max HP, +0.5 attack
    WarriorWarlordsReach,    // +0.10x attack range
    WarriorBattleMeditation, // heal 1 HP on room clear
    WarriorWeaponMaster,     // class abilities +8%
    // Hunter - marks and arrows
    HunterPredatorsRhythm,   // mark every 2nd shot instead of 3rd
    HunterQuarry,            // +15% damage vs marked
    HunterApexPredator,      // +30% damage vs marked
    HunterFletcher,          // +0.5 attack
    HunterTrappersCunning,   // class abilities +10%
    HunterSwiftQuiver,       // +12 move speed
    HunterSurvivalist,       // +1 max HP, +1 armour
    HunterFocusedBreathing,  // +15% mana regen
    // Rogue - combo and venom
    RogueDeepReserves,       // +1 max combo point (cap 7)
    RogueRuthlessFinisher,   // Eviscerate +5% per combo point
    RogueExsanguinate,       // Eviscerate +10% per combo point
    RogueToxinExpert,        // poison ticks +50%
    RogueMasterPoisoner,     // poison ticks +100%
    RogueFleetFootwork,      // +12 move speed
    RogueShadowConditioning, // +1 max HP
    RogueOpportunist,        // combo gain +25% AND Eviscerate +5%/point
    // Paladin - faith and holy might
    PaladinHolyMight,        // holy (class ability) damage +10%
    PaladinDivineWrath,      // holy damage +18%
    PaladinZealotsFury,      // Retribution +6% per stack (base 12%)
    PaladinMirroredAegis,    // reflects 50% stronger
    PaladinDevotion,         // Faith gain +15%
    PaladinCrusadersVitality,// +2 max HP
    PaladinSanctuary,        // +2 armour AND heal 1 on room clear
    PaladinDivineConduit,    // +2 max mana, +15% mana regen
    // Warlock - curses and hunger
    WarlockGrimHarvest,      // lifesteal 50% stronger
    WarlockDarkPact,         // +10% damage vs cursed
    WarlockSoulBargain,      // +25% damage vs cursed
    WarlockLingeringMalice,  // curses last +50% longer
    WarlockVoidAttunement,   // +20% mana regen
    WarlockCorruptedVitality,// +2 max HP
    WarlockSoulConduit,      // lifesteal +50% AND curses +25% longer
    WarlockOccultPower,      // class abilities +8%
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
    // ── Hunter ability learn / upgrade ─────────────────────────────────────────
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
    void TakePitfallDamage(int damage);
    virtual void Death() override;

    // Lightweight revive — resets HP, dying/hit state, and push state without reloading assets.
    // Used on true respawns (death -> village); fully heals.
    void Revive();
    // Room-entry state reset WITHOUT healing — HP carries between rooms so
    // mistakes matter beyond a single fight (sustain rework, Phase 4).
    void RefreshForRoomEntry();

    void DrawPlayer(Vector2 cameraPos);
    void DashParticles(float h, Vector2 playerScreenCenter);
    void SetWorldPos(Vector2 pos);
    void PlayHurtSound() override;
    void PlayAttackSound() override;   // per-class weapon basic (routes to SfxBank)
    void StartForcedPush(Vector2 direction, float speed);
    void OnForcedPushCollision();
    void ApplyBurnTicks(float tickDelay, int tickCount, float damagePerTick, Vector2 sourcePos);
    // Icy hit: slow movement for a duration (Bonechill elite). Re-applying refreshes.
    void ApplyChill(float duration, float speedMult);
    bool IsChilled() const { return _chillTimer > 0.f; }
    // Strong decaying shove away from a blow (Stormclub elite). Rides the normal
    // velocity channel (ApplyVelocity decays it), so it ends on its own — unlike
    // StartForcedPush, which locks the player until a wall stops it.
    void ApplyKnockbackImpulse(Vector2 direction, float speed);
    void GrantInvulnerability(float duration);
    // Persistent damage immunity (map-editor playtest "invincible" mode). Unlike
    // i-frames it never expires and does not flicker the sprite.
    void SetInvulnerableLock(bool locked) { _invulnerableLock = locked; }
    bool IsInvulnerableLocked() const { return _invulnerableLock; }

    // Fast upright pit sink that freezes input, after
    // which the engine applies the penalty and respawns the player at the edge.
    void BeginPitFall(Color tint = WHITE);
    void EndPitFall();
    bool IsPitFalling() const { return _isPitFalling; }
    bool PitFallComplete() const;
    float PitFallProgress() const;   // 0..1
    bool IsBeingForcedPushed() const { return _forcedPushActive; }
    bool IsForceLocked() const { return _forcedPushActive || _forcedPushStunTimer > 0.f; }
    bool IsDashing() const { return _isDashing; }
    void CancelDash();
    void SetCombatLocked(bool locked)    { _combatLocked = locked; }
    void SetDashAllowedWhileCombatLocked(bool allowed) { _dashAllowedWhileCombatLocked = allowed; }
    void SetManaRegenPaused(bool paused) { _manaRegenPaused = paused; }
    void SetAbilityAimMoveScale(float scale) { _abilityAimMoveScale = std::clamp(scale, 0.f, 1.f); }

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
    bool CanBeginAbilityCast(int slot) const;
    bool CanCastAbility(AbilityType type) const;
    // ── Ability cooldowns (per slot, ticked down in Update) ────────────────────
    // A successful cast starts GetAbilityCooldownSeconds(ability) on its slot;
    // TriggerAbilityCast refuses while the slot is still counting down.
    float GetSlotCooldownRemaining(int slot) const
        { return (slot >= 0 && slot < _hardAbilityCap) ? _slotCooldownRemaining[slot] : 0.f; }
    // 0..1 fraction of the cooldown still left — drives the HUD sweep overlay.
    float GetSlotCooldownFraction(int slot) const
    {
        if (slot < 0 || slot >= _hardAbilityCap || _slotCooldownDuration[slot] <= 0.f)
            return 0.f;
        return _slotCooldownRemaining[slot] / _slotCooldownDuration[slot];
    }
    bool IsSlotOnCooldown(int slot) const { return GetSlotCooldownRemaining(slot) > 0.f; }
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

    // ── Warrior Rage (class-identity mechanic) ─────────────────────────────────
    // The Warrior builds Rage by landing hits and by BEING hit, and slowly loses
    // it out of combat. Rage passively empowers Warrior abilities (up to +50% at
    // full) and Ground Slam consumes it all for a larger quake. Warrior-only —
    // every call is a no-op for other classes so hit-site hooks stay unconditional.
    static constexpr float kMaxRage = 100.f;
    void  AddRage(float amount)
    {
        if (_class != PlayerClass::Warrior) return;
        if (amount > 0.f) amount *= _classResourceGainMult;   // Class Attunement
        _rage = (_rage + amount > kMaxRage) ? kMaxRage : ((_rage + amount < 0.f) ? 0.f : _rage + amount);
    }
    float GetRage()        const { return (_class == PlayerClass::Warrior) ? _rage : 0.f; }
    float GetRagePercent() const { return GetRage() / kMaxRage; }   // 0..1
    // Spend ALL current Rage; returns the fraction (0..1) that was consumed so the
    // ability can scale its payoff (Ground Slam's quake radius).
    float ConsumeAllRage()
    {
        if (_class != PlayerClass::Warrior) return 0.f;
        float fraction = _rage / kMaxRage;
        _rage = 0.f;
        return fraction;
    }

    // ── Rogue Combo Points (class-identity mechanic) ───────────────────────────
    // The Rogue banks a combo point for each quick hit that lands (Backstab banks
    // two). Eviscerate spends ALL points for a scaled burst — the class's brain is
    // "build with fast strikes, cash out with the finisher". Rogue-only; every
    // call is a no-op for other classes so hit-site hooks stay unconditional.
    static constexpr int kMaxComboPoints = 5;
    void AddComboPoints(int n)
    {
        if (_class != PlayerClass::Rogue) return;
        // Class Attunement: bank fractional bonus combo across hits so +25%
        // gain means an extra point every 4th single-point hit.
        _comboFraction += (float)n * (_classResourceGainMult - 1.f);
        while (_comboFraction >= 1.f) { _comboFraction -= 1.f; n += 1; }
        // Deep Reserves class card can raise the bank depth above the base 5.
        _comboPoints = (_comboPoints + n > _maxComboPointsRun) ? _maxComboPointsRun : _comboPoints + n;
    }
    int GetComboPoints() const { return (_class == PlayerClass::Rogue) ? _comboPoints : 0; }
    // Spend ALL banked points; returns how many were consumed for the finisher.
    int ConsumeAllComboPoints()
    {
        if (_class != PlayerClass::Rogue) return 0;
        int spent = _comboPoints;
        _comboPoints = 0;
        return spent;
    }

    // ── Paladin Faith (class-identity mechanic) ────────────────────────────────
    // The Paladin builds Faith by standing ground: being struck and landing holy
    // hits both add Faith, which passively empowers Paladin abilities (up to +40%
    // at full). Divine Storm consumes it all for a wider nova. Unlike Rage, Faith
    // does not decay — holding ground is the fantasy, not frenzy. Paladin-only;
    // no-ops for other classes so hit-site hooks stay unconditional.
    static constexpr float kMaxFaith = 100.f;
    void  AddFaith(float amount)
    {
        if (_class != PlayerClass::Paladin) return;
        if (amount > 0.f) amount *= _classResourceGainMult;   // Class Attunement
        _faith = (_faith + amount > kMaxFaith) ? kMaxFaith : ((_faith + amount < 0.f) ? 0.f : _faith + amount);
    }
    float GetFaith()        const { return (_class == PlayerClass::Paladin) ? _faith : 0.f; }
    float GetFaithPercent() const { return GetFaith() / kMaxFaith; }   // 0..1
    // Spend ALL current Faith; returns the fraction (0..1) consumed so the ability
    // can scale its payoff (Divine Storm's nova radius).
    float ConsumeAllFaith()
    {
        if (_class != PlayerClass::Paladin) return 0.f;
        float fraction = _faith / kMaxFaith;
        _faith = 0.f;
        return fraction;
    }

    // ── Power Choice card hooks ───────────────────────────────────────────────
    // Class Attunement: multiplies every positive Rage/Faith/Combo gain.
    float GetClassResourceGainMult() const { return _classResourceGainMult; }
    // Overload: permanent class-ability damage multiplier, paid for with +1 mana
    // on every cast. Consumed by Engine's dmgVal buff.
    float GetAbilityPowerMult() const { return _abilityPowerMult; }

    // ── Class-specific Power Choice hooks ─────────────────────────────────────
    // Small tunable dials the class card catalog adjusts. Each is consumed at
    // one (or very few) choke points, so adding a new class card is usually
    // just "enum + ApplyUpgrade case + name/desc" — no new gameplay wiring.
    // Mage: per-element damage (spread/bolt/burn/ultimate pipelines).
    float GetElementDamageMult(AbilityType type) const
    {
        switch (type)
        {
        case AbilityType::FireSpread: case AbilityType::FireBolt: case AbilityType::FireUltimate:
            return _fireDamageMult;
        case AbilityType::IceSpread: case AbilityType::IceBolt: case AbilityType::IceUltimate:
            return _iceDamageMult;
        case AbilityType::ElectricSpread: case AbilityType::ElectricBolt: case AbilityType::ElectricUltimate:
            return _electricDamageMult;
        default: return 1.f;
        }
    }
    float GetComboBonusMult()     const { return _comboBonusMult; }      // Mage: reaction payoff
    float GetRageDecayMult()      const { return _rageDecayMult; }       // Warrior: slower bleed-off
    float GetRageFullBonus()      const { return _rageFullBonus; }       // Warrior: dmg at full bar
    float GetRetributionPerStack() const { return _retributionPerStack; } // Paladin: counter potency
    float GetReflectPotency()     const { return _reflectPotency; }      // Paladin: Aegis strength
    int   GetHunterMarkEvery()    const { return _hunterMarkEvery; }     // Hunter: mark cadence
    float GetMarkedBonus()        const { return _markedBonus; }         // Hunter: dmg vs marked
    float GetCursedBonus()        const { return _cursedBonus; }         // Warlock: dmg vs cursed
    float GetCurseDurationMult()  const { return _curseDurationMult; }   // Warlock: curse windows
    float GetLifestealPotency()   const { return _lifestealPotency; }    // Warlock: drain strength
    int   GetMaxComboPoints()     const { return _maxComboPointsRun; }   // Rogue: bank depth
    float GetEvisceratePerCombo() const { return _evisceratePerCombo; }  // Rogue: finisher scaling
    float GetPoisonPotency()      const { return _poisonPotency; }       // Rogue/Warlock: DoT ticks
    int   GetHealOnRoomClear()    const { return _healOnRoomClear; }     // sustain-friendly recovery
    // The REAL cost of a cast = base table cost + Overload surcharge. Ultimates
    // drain all mana regardless, so the surcharge only gates normal abilities.
    int   GetAbilityCost(AbilityType type) const
    {
        return AbilityDrainsAllMana(type) ? GetAbilityManaCost(type)
                                          : GetAbilityManaCost(type) + _abilityCostBonus;
    }

    // ── Paladin Retribution (no self-heal): being struck fuels a stacking damage
    // buff, and Aegis reflects a fraction of damage back at the attacker. ──
    void  GrantReflect(float fraction, float duration)
    {
        // Mirrored Aegis class card strengthens every reflect grant (capped).
        _reflectFraction = std::min(0.9f, fraction * _reflectPotency);
        _reflectTimer    = duration;
    }
    void  AddRetribution(int n) { int s = _retributionStacks + n; _retributionStacks = (s > kRetributionMaxStacks) ? kRetributionMaxStacks : s; _retributionTimer = kRetributionDuration; }
    int   GetRetributionStacks() const { return _retributionTimer > 0.f ? _retributionStacks : 0; }
    bool  IsReflectActive() const { return _reflectTimer > 0.f; }
    // Engine drains any reflect owed this frame and applies it to the attacker.
    bool  ConsumeReflect(float& outDmg, Vector2& outPos)
    {
        if (!_hasPendingReflect) return false;
        outDmg = _pendingReflectDamage; outPos = _pendingReflectPos;
        _pendingReflectDamage = 0.f; _hasPendingReflect = false;
        return true;
    }
    void  MoveTowardFacing(float distance);   // short lunge/dash for melee abilities
    void  MoveAlongDirection(Vector2 direction, float distance);

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
    // Fire point with an explicit forward/height offset (from attack tuning). The
    // forward offset flips with facing; height is applied straight down/up.
    Vector2 GetCastOrigin(float forward, float height) const;
    Vector2 GetFacingDirection() const;
    Vector2 GetMoveDirection() const { return _direction; }
    Vector2 GetFeetWorldPos() const;
    int ConsumeHealEffectRequests();
    float GetDashCooldownPercent() const { return (_dashCooldownTime > 0.f) ? 1.f - (_dashCooldown / _dashCooldownTime) : 1.f; }

    void AddExp(int amount);
    void Heal(int amount);
    void AddGold(int amount) { _gold += amount; }
    void SetGold(int amount) { _gold = amount; }   // one-wallet meta loop: wipe on death / carry village wallet into a run
    int  GetGold()     const { return _gold; }
    // Gold from an enemy drop — applies the Midas Touch relic bonus.
    void AddGoldFromDrop(int amount);

    // ── Mystic Cells (meta progression currency) ─────────────────────────────
    // Carried during the run; banked with Zeph between zones; lost on death.
    void AddCells(int amount)  { _cells += amount; }
    void AddCellsFromDrop(int amount);   // applies the Soul Siphon relic bonus
    void SetCellGainMultiplier(float m) { _cellGainMultiplier = m; }
    void MultiplyCellGainMultiplier(float m) { _cellGainMultiplier *= m; }
    // Cursed Wager: bonus gold/XP/Echoes for the wagered biome (1.0 = no wager).
    void  SetWagerRewardMult(float m) { _wagerRewardMult = (m < 1.f) ? 1.f : m; }
    float GetWagerRewardMult() const  { return _wagerRewardMult; }
    // Risk Shrine contracts: separate gold / XP multipliers for the contracted
    // room (1.0 = none). The bonus portion is accumulated for the resolve toast.
    void SetContractGoldMult(float m) { _contractGoldMult = (m < 1.f) ? 1.f : m; }
    void SetContractXpMult(float m)   { _contractXpMult   = (m < 1.f) ? 1.f : m; }
    int  TakeContractBonusGold()      { int v = _contractBonusGold; _contractBonusGold = 0; return v; }
    int  TakeContractBonusXp()        { int v = _contractBonusXp;   _contractBonusXp   = 0; return v; }
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

    // Zeph wares are limited-use preparation, not permanent stat growth.
    void  GrantShopWhetstone(int hits = 6) { _shopWhetstoneHits = std::min(12, _shopWhetstoneHits + hits); }
    float GetShopBasicDamageMultiplier() const { return (_shopWhetstoneHits > 0) ? 1.15f : 1.f; }
    void  ConsumeShopWhetstoneHit() { if (_shopWhetstoneHits > 0) --_shopWhetstoneHits; }
    void  GrantShopFreeCast(int casts = 1) { _shopFreeCasts = std::min(2, _shopFreeCasts + casts); }
    void  GrantShopWard(int hits = 1) { _shopWardHits = std::min(2, _shopWardHits + hits); }
    void  GrantShopGoldCompass(int pickups = 5) { _shopGoldPickups = std::min(10, _shopGoldPickups + pickups); }
    void  GrantShopEchoSatchel(int pickups = 5) { _shopCellPickups = std::min(10, _shopCellPickups + pickups); }
    int   GetShopWhetstoneHits() const { return _shopWhetstoneHits; }
    int   GetShopFreeCasts() const { return _shopFreeCasts; }
    int   GetShopWardHits() const { return _shopWardHits; }
    int   GetShopGoldPickups() const { return _shopGoldPickups; }
    int   GetShopCellPickups() const { return _shopCellPickups; }

    // ── Balance telemetry counters ────────────────────────────────────────────
    // Accumulate real HP lost/gained; the Engine reads + resets them per room
    // for the debug telemetry overlay. Zero gameplay effect.
    void ResetTelemetryCounters() { _telemDamageTaken = 0.f; _telemHealed = 0.f; _telemAbilityCasts = 0; }
    float GetTelemDamageTaken() const { return _telemDamageTaken; }
    float GetTelemHealed()      const { return _telemHealed; }
    int   GetTelemAbilityCasts() const { return _telemAbilityCasts; }

    // ── Zeph Risk Bargains / Rare Stat shelf (run-scoped state) ──────────────
    // Pressure debt: the next COMBAT room spawns an extra enemy and everyone
    // attacks faster. Armed by Hunted Purse and Zeph's Wager; the Engine
    // consumes it when it actually spawns a fresh combat room, so buying it
    // before a rest room doesn't waste (or dodge) the debt.
    // Debts STACK (capped) so re-buying via rerolls means more hard rooms, not
    // free gold on a single shared downside.
    void ArmShopPressureDebt() { _shopPressureDebtRooms = std::min(3, _shopPressureDebtRooms + 1); }
    bool ConsumeShopPressureDebt()
    {
        if (_shopPressureDebtRooms <= 0) return false;
        --_shopPressureDebtRooms;
        return true;
    }
    // Zeph's Wager: same pressure, but the room pays out bonus gold + XP on clear.
    void ArmShopWager() { _shopWagerArmed = true; ArmShopPressureDebt(); }
    bool ConsumeShopWager() { bool armed = _shopWagerArmed; _shopWagerArmed = false; return armed; }
    // Rare Stat shelf: ONE permanent stat purchase from Zeph per run, total.
    bool HasShopRareStatBought() const { return _shopRareStatBought; }
    void MarkShopRareStatBought()      { _shopRareStatBought = true; }
    // Blood Price: once per run; the max-HP cost can never drop the cap below 4.
    bool HasUsedBloodPrice() const { return _shopBloodPriceUsed; }
    void MarkBloodPriceUsed()      { _shopBloodPriceUsed = true; }
    void SacrificeMaxHealth(int amount)
    {
        _maxHealth = std::max(4.f, _maxHealth - (float)amount);
        _health    = std::min(_health, _maxHealth);
    }
    // Armour absorbs one direct hit before HP is affected (0–3 slots).
    static constexpr int kMaxArmour = 3;

    // Passive mana regen — flat rate scaled only by the regen multiplier.
    // Higher regen should always feel faster, regardless of current mana.
    // _manaRegenMultiplier scales via upgrades and (eventually) the store.
    // Base lives in GameBalance (raised 0.2 -> 0.25 as compensation for the
    // removed +5-mana-per-level auto gain).
    static constexpr float kManaRegenBase = Balance::Sustain::kManaRegenPerSecond;

    // Armour
    int  GetArmour()    const { return _armour; }
    int  GetMaxArmour() const { return _maxArmour; }
    void AddArmour(int amount);
    void ApplyUpgrade(UpgradeType type);

    // Combat damage accessors -------------------------------------------------
    // Melee scales through explicit rewards rather than automatic level gains.
    // Special abilities are burst / piercing tools and improve through their
    // own ability cards.
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
    Texture _pushAnim{};  // Mage bolt cast pose
    Texture _bowAnim{};   // Hunter only: bow-draw sheet for basic + shot abilities
                          // (traps use the appearance's own attack animation)

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
    // Gate between basic-attack starts. Set from the tuned "cooldown" in
    // attacktuning_<Class>_Basic.txt; 0 (or no file) = animation-driven cadence
    // as before, so untuned classes are unchanged.
    float _basicAttackCdTimer = 0.f;
    bool _combatLocked      = false;
    bool _dashAllowedWhileCombatLocked = false;
    bool _manaRegenPaused   = false;
    float _abilityAimMoveScale = 1.f;
    bool _castingAbility = false;
    bool _invulnerableLock = false;  // editor-playtest god mode; never expires
    bool _isPitFalling = false;      // playing the pit-fall sink animation
    float _pitFallTimer = 0.f;
    Color _pitFallTint = WHITE;
    static constexpr float kPitFallDuration = 0.22f;
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
    // Per-slot cast cooldowns (seconds remaining / full duration for HUD sweep).
    float _slotCooldownRemaining[_hardAbilityCap] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
    float _slotCooldownDuration[_hardAbilityCap]  = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };

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

    // Warrior Rage resource (0..kMaxRage) — decays slowly in Update.
    float _rage = 0.f;
    // Rogue combo points (0..kMaxComboPoints) — spent by Eviscerate.
    int   _comboPoints = 0;
    // Paladin Faith resource (0..kMaxFaith) — spent by Divine Storm, no decay.
    float _faith = 0.f;
    // Power Choice card state: Class Attunement resource-gain multiplier (with
    // a fractional combo-point bank so the Rogue benefits too), and Overload's
    // ability damage multiplier + per-cast mana surcharge.
    float _classResourceGainMult = 1.f;
    float _comboFraction         = 0.f;
    float _abilityPowerMult      = 1.f;
    int   _abilityCostBonus      = 0;
    // Class-specific card dials (see the public hook block; reset in Init).
    float _fireDamageMult     = 1.f;
    float _iceDamageMult      = 1.f;
    float _electricDamageMult = 1.f;
    float _comboBonusMult     = 1.f;
    float _rageDecayMult      = 1.f;
    float _rageFullBonus      = 0.5f;   // +50% ability dmg at full Rage (base)
    float _retributionPerStack = 0.12f; // Paladin counter dmg per stack (base)
    float _reflectPotency     = 1.f;
    int   _hunterMarkEvery    = 3;      // every Nth landed shot marks (base 3)
    float _markedBonus        = 0.30f;  // Hunter dmg bonus vs marked (base)
    float _cursedBonus        = 0.25f;  // Warlock dmg bonus vs cursed (base)
    float _curseDurationMult  = 1.f;
    float _lifestealPotency   = 1.f;
    int   _maxComboPointsRun  = kMaxComboPoints;   // Rogue bank depth (cap 7)
    float _evisceratePerCombo = 0.15f;  // finisher dmg per combo point (base)
    float _poisonPotency      = 1.f;
    int   _healOnRoomClear    = 0;
    float _lifestealFraction = 0.f;  // fraction of damage dealt returned as HP
    float _lifestealTimer    = 0.f;

    // Paladin Retribution / Aegis reflect.
    static constexpr int   kRetributionMaxStacks = 5;
    static constexpr float kRetributionDuration  = 5.f;
    static constexpr float kRetributionPerStack  = 0.12f;   // +12% damage per stack
    int     _retributionStacks    = 0;
    float   _retributionTimer     = 0.f;
    float   _reflectFraction      = 0.f;
    float   _reflectTimer         = 0.f;
    float   _pendingReflectDamage = 0.f;
    Vector2 _pendingReflectPos{};
    bool    _hasPendingReflect    = false;

    PlayerClass _class = PlayerClass::Mage;   // chosen at run start
    std::string _appearancePrefix;            // hero sprite set; empty = use class default
    float _cellGainMultiplier = 1.f;          // Cell Surge meta unlock (1.0 / 1.5)
    float _wagerRewardMult    = 1.f;          // Cursed Wager biome reward bonus
    float _contractGoldMult   = 1.f;          // Risk Shrine contract gold bonus (this room)
    float _contractXpMult     = 1.f;          // Risk Shrine contract XP bonus (this room)
    int   _contractBonusGold  = 0;            // gold gained beyond base while contracted
    int   _contractBonusXp    = 0;            // XP gained beyond base while contracted
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
    int _expToNextLevel = Balance::Levelling::kExpToNextBase;
    static constexpr int _maxLevel = Balance::Levelling::kMaxLevel;

    int   _mana    = 0;
    int   _maxMana = 10;
    float _ultimateManaWarningTimer = 0.f;
    int   _armour    = 0;   // current armour slots filled
    int   _maxArmour = kMaxArmour;  // can be raised by upgrades; hard-capped at kMaxArmour
    float _attackRangeMultiplier = 1.f;

    int _shopWhetstoneHits = 0;
    int _shopFreeCasts     = 0;
    int _shopWardHits      = 0;
    int _shopGoldPickups   = 0;
    int _shopCellPickups   = 0;
    // Balance telemetry (per-room HP flow, read + reset by Engine).
    float _telemDamageTaken = 0.f;
    int   _telemAbilityCasts = 0;
    float _telemHealed      = 0.f;
    // Zeph bargain / rare-shelf run state (see the public block above).
    int  _shopPressureDebtRooms = 0;   // hard rooms owed (stacks, capped at 3)
    bool _shopWagerArmed     = false;
    bool _shopRareStatBought = false;
    bool _shopBloodPriceUsed = false;

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

    // Chill status (icy enemy hits) — movement multiplier while the timer runs.
    float _chillTimer = 0.f;
    float _chillMult  = 1.f;

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
