#pragma once
#include "BaseCharacter.h"
#include "Character.h"
#include "NavigationGrid.h"
#include "GameBalance.h"
#include "raymath.h"
#include <vector>
#include <memory>

class Cyclops;
class Ogre;
class Molarbeast;
class SkeletonArcher;
class FlameWisp;
class SlimeEnemy;
class AbyssSlime;
class PumpkinJack;
class Minotaur;
class Sporeling;
class Shieldbearer;
class Phantom;
class BomberImp;
class Warchief;
class LivingBlade;
class Werewolf;
class ChompBug;
class Osiris;
class TitanGuard;
class ToxicVermin;
class AncientBear;

// ── Encounter roles ───────────────────────────────────────────────────────────
// Lightweight tactical role used by the encounter director to compose fights and
// place spawns (ranged in back, tanks mid, assassins off-angle, etc.). Purely
// metadata — it does not change an enemy's own AI. HeavyRanged is a Ranged that
// also counts as an anchor (Cyclops). Boss is set for bosses so the director can
// exclude them from the add budget.
enum class EnemyRole
{
    Grunt, Charger, Ranged, Tank, Support, Assassin, Zoner, Summoner, HeavyRanged, Boss
};

class Enemy : public BaseCharacter
{
public:
    Enemy(Vector2 pos);
    ~Enemy() override;
    static void UnloadSharedResources();

    // Main enemy tick. Engine calls this for every active enemy each frame.
    // Derived enemy types can override it when they need custom combat logic,
    // but they still participate in the same pooled enemy vector.
    virtual void Update(float dt, Vector2 heroWorldPos, Vector2 navigationTarget, bool hasNavigationTarget,
        const std::vector<std::unique_ptr<Enemy>>& enemies, const std::vector<Vector2>& propCenters);
    virtual void SetWaveScale(int wave);
    virtual void ApplyEnemyPowerLevel(int enemyPowerLevel);

    // Extra flat multipliers on top of power level — used by the ascension
    // difficulty tiers. Called after ApplyEnemyPowerLevel in ConfigureSpawnedEnemy.
    void ApplyDifficultyScaling(float healthMult, float damageMult);

    // Shorten the gap between attacks (mult < 1 = more aggressive). Used by the
    // Zeph pressure-debt bargains; a depth-based cadence curve can reuse it.
    void QuickenAttacks(float mult) { if (mult > 0.f) _attackDelay *= mult; }

    // Colour-variant tier (0-3) — later world zones spawn recoloured, visibly
    // tougher versions. Default is a no-op; types with variant art override.
    virtual void SetVariantTier(int tier) { (void)tier; }

    // ── Character Animator (dev tool) + tuning interface ─────────────────────
    // GetTuningName() returns nullptr for types without tuning support; a name
    // enables loading charactertuning_<Name>.txt at the end of ResetForSpawn.
    virtual const char* GetTuningName() const { return nullptr; }

    // Human-readable type name for the bestiary. Records once per death.
    const char* GetBestiaryName();
    bool BestiaryRecorded() const { return _bestiaryRecorded; }
    void SetBestiaryRecorded() { _bestiaryRecorded = true; }

    // Animation preview: the default implementation covers the five standard
    // grunt sheets; bosses override with their own sheet lists.
    virtual int         GetEditorAnimCount() const { return 5; }
    virtual const char* GetEditorAnimName(int index) const;
    virtual void        PlayEditorAnim(int index);
    void  TickEditorAnimation(float dt);   // generic looping frame advance
    float GetEditorAnimFrameTime(int index) const;
    void  SetEditorAnimFrameTime(int index, float frameTime);

    // Tuned solid hitbox — stored relative to _worldPos so it is convention-free.
    // Every GetCollisionRec override returns this when set.
    void      SetCollisionRecWorld(Rectangle relativeRect) { _tunedCollisionRel = relativeRect; _hasTunedCollision = true; }
    void      ClearTunedCollision() { _hasTunedCollision = false; }
    bool      HasTunedCollision() const { return _hasTunedCollision; }
    Rectangle GetCollisionRecRelative() const;

    void  SetDrawScale(float scale) { _scale = (scale < 0.5f) ? 0.5f : scale; }
    float GetDrawScale() const { return _scale; }
    void  SetEditorFacing(float direction) { _rightLeft = direction; }
    float GetEditorFacing() const { return _rightLeft; }

    // ── Per-animation tuning (Character Animator) ─────────────────────────────
    // Each animation slot can carry its own body circle (drives BOTH the solid
    // capsule and the hurt rect), melee box (facing right), and sprite draw
    // offset. The active slot is derived from whichever sheet is playing.
    virtual int GetCurrentAnimSlot() const;

    bool    GetAnimBodySet(int slot)    const { return (slot >= 0 && slot < kAnimSlots) && _animBodySet[slot]; }
    Vector2 GetAnimBodyOffset(int slot) const { return (slot >= 0 && slot < kAnimSlots) ? _animBodyOffset[slot] : Vector2{}; }
    float   GetAnimBodyRadius(int slot) const { return (slot >= 0 && slot < kAnimSlots) ? _animBodyRadius[slot] : 0.f; }
    void    SetAnimBody(int slot, Vector2 offset, float radius);
    void    ClearAnimBody(int slot);

    bool      GetAnimMeleeSet(int slot)     const { return (slot >= 0 && slot < kAnimSlots) && _animMeleeSet[slot]; }
    Rectangle GetAnimMeleeRelRect(int slot) const { return (slot >= 0 && slot < kAnimSlots) ? _animMeleeRel[slot] : Rectangle{}; }
    void      SetAnimMelee(int slot, Rectangle relativeRect);
    void      ClearAnimMelee(int slot);

    bool    GetAnimDrawSet(int slot)         const { return (slot >= 0 && slot < kAnimSlots) && _animDrawSet[slot]; }
    Vector2 GetAnimDrawOffsetValue(int slot) const { return (slot >= 0 && slot < kAnimSlots) ? _animDrawOffset[slot] : Vector2{}; }
    void    SetAnimDrawOffset(int slot, Vector2 offset);

    static constexpr int kAnimSlots = 10;

    void SetTarget(Character* character)  { _target = character; }

    // Give each enemy a non-owning pointer to the shared nav grid so it can
    // request its own waypoint path instead of relying on the engine to pass
    // a single-step target every frame.
    void SetNavigationGrid(NavigationGrid* nav) { _nav = nav; }
    void Init();
    virtual void ResetForSpawn(Vector2 pos);
    virtual void DrawEnemy(Vector2 heroWorldPos);
    virtual void ApplyBurn(float delay, int damage, Vector2 sourcePos);
    virtual void ApplyFreeze(float duration);
    virtual void ApplyElectricCharge();
    virtual void ApplyExternalImpulse(Vector2 impulse, bool cancelLockedAnimation);

    // ── Shared status effects (ARPG combat-identity pass) ───────────────────────
    // Any class can apply these via the same funcs; bosses take reduced durations
    // (resistance, not immunity — see kBossStatusDurMult) so control effects stay
    // meaningful without trivialising boss fights. Damage-over-time ticks route
    // through the universal per-frame hook (UpdateStatuses, called from UpdateBurns).
    void ApplyPoison(int damagePerTick, float duration, int stacks = 1);  // stacking DoT
    void ApplyBleed(int damagePerTick, float duration);                   // physical DoT, +while moving
    void ApplySlow(float speedMult, float duration);                      // speedMult < 1
    void ApplyVulnerability(float damageTakenMult, float duration);       // mult > 1 (armor break)
    void ApplyMark(float duration);                                       // "marked"/execute window
    // Warlock class identity: cursed enemies take bonus damage from all Warlock
    // effects (see Engine::ScalePlayerHit). A timed tag like Mark, with its own
    // purple indicator so the player can track which targets are primed.
    void ApplyCurse(float duration) { if (duration > _curseTimer) _curseTimer = duration; }
    void UpdateStatuses(float dt);
    void ResetStatuses();                                                 // clear all on (re)spawn

    bool  IsPoisoned()      const { return _poisonTimer  > 0.f; }
    bool  IsBleeding()      const { return _bleedTimer   > 0.f; }
    bool  IsSlowed()        const { return _slowTimer    > 0.f; }
    bool  IsVulnerable()    const { return _vulnTimer    > 0.f; }
    bool  IsMarked()        const { return _markTimer    > 0.f; }
    bool  IsCursed()        const { return _curseTimer   > 0.f; }
    // Multiplier applied to incoming damage (armor break / vulnerability). 1 = none.
    float GetIncomingDamageMult() const { return (_vulnTimer > 0.f) ? _vulnMult : 1.f; }
    // Combined movement multiplier from slow + chill. 1 = full speed, 0 = stopped.
    float GetStatusMoveSpeedMult() const;

    // Ogre charge — sustained push until the enemy slides into a wall or prop.
    void StartForcedPush(Vector2 direction, float speed);
    virtual void OnForcedPushCollision();
    bool IsBeingForcedPushed() const { return _forcedPushActive; }

    virtual bool IsFrozen()        const { return _freezeTimer > 0.f; }
    // Burning status for relic synergies (Ember Heart / Wildfire). Based on the
    // shared base burn queue — covers grunts, new enemies, and the new bosses.
    bool IsBurning()               const { return !_pendingBurns.empty(); }

    // Graveyard revive
    void SetGraveReviveAvailable(bool b)   { _graveReviveAvailable = b; }
    bool IsGraveReviveAvailable()    const { return _graveReviveAvailable; }
    bool IsGraveReviveInvulnerable() const { return _graveReviveInvulTimer > 0.f; }
    void TakeDamage(int damage, Vector2 attackerPos) override;
    // Unblockable variant — identical to TakeDamage for normal enemies, but a
    // Shieldbearer overrides it to skip its frontal block (Hunter Puncture Shot).
    virtual void TakeDamageUnblockable(int damage, Vector2 attackerPos) { TakeDamage(damage, attackerPos); }

    // ── Blocked-hit feedback ──────────────────────────────────────────────────
    // When a hit is denied (bodyguard/leap i-frames, or a Shieldbearer's frontal
    // block) TakeDamage records WHY instead of applying damage. The hit code then
    // reads it once to show the matching floating word ("SHIELDED" / "BLOCKED")
    // instead of a phantom damage number. See VFXManager::SpawnFloatingLabel.
    enum class HitBlockReason { None, Shielded, Blocked, Immune };
    HitBlockReason ConsumeHitBlock() { HitBlockReason r = _hitBlock; _hitBlock = HitBlockReason::None; return r; }

    // Dream Realm flicker
    bool    IsFlickerInWindup()   const { return _flickerInWindup; }
    Vector2 GetFlickerTarget()    const { return _flickerTarget; }
    float   GetFlickerCooldown()  const { return _flickerCooldown; }
    void    SetFlickerCooldown(float t) { _flickerCooldown = t; }
    void    StartFlickerWindup(float duration, Vector2 target);
    bool    ConsumeFlickerComplete();
    void    TickFlicker(float dt);
    virtual bool IsElectroStunned() const { return _isCharged && _takingDamage; }
    bool IsCharged()               const { return _isCharged; }
    virtual int  GetExpValue()  const { return _expValue; }
    virtual int  GetAttackPower() const { return (int)_attackPower; }
    bool IsDying()      const { return _dying; }
    virtual bool IsActive()     const { return _isActive; }
    virtual void SetActive(bool active) { _isActive = active; }
    virtual void Teleport(Vector2 pos) { _worldPos = pos; _worldPosLastFrame = pos; _velocity = Vector2Zero(); }
    virtual Cyclops* AsCyclops() { return nullptr; }
    virtual Ogre* AsOgre() { return nullptr; }
    virtual Molarbeast* AsMolarbeast() { return nullptr; }
    virtual SkeletonArcher* AsSkeletonArcher() { return nullptr; }
    virtual FlameWisp* AsFlameWisp() { return nullptr; }
    virtual SlimeEnemy* AsSlime() { return nullptr; }
    virtual AbyssSlime* AsAbyssSlime() { return nullptr; }
    virtual PumpkinJack* AsPumpkinJack() { return nullptr; }
    virtual Minotaur* AsMinotaur() { return nullptr; }
    virtual Sporeling* AsSporeling() { return nullptr; }
    virtual Shieldbearer* AsShieldbearer() { return nullptr; }
    virtual Phantom* AsPhantom() { return nullptr; }
    virtual BomberImp* AsBomberImp() { return nullptr; }
    virtual Warchief* AsWarchief() { return nullptr; }
    virtual LivingBlade* AsLivingBlade() { return nullptr; }
    virtual ChompBug* AsChompBug() { return nullptr; }
    virtual Osiris* AsOsiris() { return nullptr; }
    virtual TitanGuard* AsTitanGuard() { return nullptr; }
    virtual ToxicVermin* AsToxicVermin() { return nullptr; }
    virtual AncientBear* AsAncientBear() { return nullptr; }
    virtual Werewolf* AsWerewolf() { return nullptr; }

    // ── Warchief aura ─────────────────────────────────────────────────────────
    // Nearby allies move faster while inside the warchief's banner radius.
    // The warchief refreshes this every frame; it decays within half a second
    // of leaving the aura. Ticked inside UpdateBurns (which every type calls).
    void  GrantWarAura(float duration) { if (duration > _warAuraTimer) _warAuraTimer = duration; }
    bool  HasWarAura() const { return _warAuraTimer > 0.f; }
    static constexpr float kWarAuraSpeedMultiplier = 1.3f;
    virtual bool UsesDirectPursuit() const { return false; }
    virtual bool IgnoresPropCollisions() const { return false; }
    virtual bool IsBoss() const { return false; }
    bool IsEliteMiniboss() const { return _isEliteMiniboss; }
    void SetIsEliteMiniboss(bool b);

    // ── Encounter metadata ────────────────────────────────────────────────────
    // Tactical role + spawn-budget cost consumed by the encounter director. Base
    // default is a cheap Grunt; specific enemy types override these. Bosses report
    // Boss so the director can keep them out of the add budget.
    virtual EnemyRole GetEncounterRole() const { return IsBoss() ? EnemyRole::Boss : EnemyRole::Grunt; }
    virtual int       GetSpawnCost()     const { return 1; }

    // ── Elite-room mechanics ──────────────────────────────────────────────
    // _isInvulnerable : bodyguard shield — engine clears when all grunts die
    // _leapInvulnerable: gap-closer wind-up — engine sets/clears around leap
    void SetInvulnerable(bool v)   { _isInvulnerable    = v; }
    void SetLeapFrozen(bool v)     { _leapInvulnerable  = v; }
    bool IsInvulnerable()    const { return _isInvulnerable; }
    bool IsLeapFrozen()      const { return _leapInvulnerable; }
    void ApplyEnrage();            // +50% speed, half attack cooldown, called by Engine on elite spawn

    // ── Boss enrage phase ──────────────────────────────────────────────────────
    // A boss enters an enraged "final phase" once its HP drops below
    // _enrageThreshold (a fraction of max HP; 0 = this enemy never enrages). The
    // latch is ONE-WAY within a fight — healing back up will NOT un-enrage it —
    // and auto-resets when the enemy is at full HP (i.e. on a fresh/pooled spawn).
    // Driven every frame from UpdateBurns(); the transition requests a screen
    // shake (ConsumeEnrageShakeRequest) so the phase change actually reads.
    void UpdateEnrageLatch(float dt);
    bool IsEnraged() const { return _enrageLatched; }
    bool ConsumeEnrageShakeRequest();       // true once, on the frame enrage begins
    bool IsEnrageFlashing() const { return _enrageFlashTimer > 0.f; }

    // ── Multi-phase boss system ─────────────────────────────────────────────────
    // Generalises the enrage latch: a boss declares HP-fraction thresholds (in
    // DESCENDING order, e.g. {0.66, 0.33}) and _phase counts how many have been
    // crossed (0 = full-strength opening phase). One-way within a fight; auto-resets
    // at full HP (fresh/pooled spawn). Driven every frame from UpdateBurns().
    // A boss polls ConsumePhaseChange() to react ONCE on the frame a new phase
    // begins (roar, unlock attacks, spawn a hazard); GetPhase() gates ongoing
    // behaviour. Bosses can honour IsInPhaseTransition() to grant a brief
    // invulnerable "set-piece" window while the transition animation plays.
    void SetPhaseThresholds(std::vector<float> descendingHpFractions);
    void UpdatePhaseLatch(float dt);
    int  GetPhase() const { return _phase; }
    int  GetPhaseCount() const { return (int)_phaseThresholds.size() + 1; }
    int  ConsumePhaseChange();              // returns the new phase once, else -1
    void BeginPhaseTransition(float seconds) { _phaseTransitionTimer = seconds; }
    bool IsInPhaseTransition() const { return _phaseTransitionTimer > 0.f; }

    // ── Boss-state callout ────────────────────────────────────────────────────
    // A floating word ("ENRAGED" / "PHASE SHIFT" / "SHIELD DOWN") announced on a
    // state transition. Set from the enrage/phase latches (or by the engine for
    // shield changes); the runtime polls it once per enemy and shows it via
    // VFXManager::SpawnFloatingLabel. Kept separate from the shake/phase
    // consumers so nothing double-consumes. `text` must be a string literal.
    void        RequestBossCallout(const char* text) { _bossCallout = text; }
    const char* ConsumeBossCallout() { const char* c = _bossCallout; _bossCallout = nullptr; return c; }
    Rectangle GetCollisionRec()       const override;
    Capsule2D GetCapsule()            const override;
    virtual Rectangle GetAttackCollisionRec() const;

    float GetAttackBoxWidth()   const { return _attackBoxWidth; }
    float GetAttackBoxHeight()  const { return _attackBoxHeight; }
    float GetAttackBoxOffsetX() const { return _attackBoxOffsetX; }
    float GetAttackBoxOffsetY() const { return _attackBoxOffsetY; }
    void  SetAttackBoxWidth(float v)   { _attackBoxWidth   = std::max(4.f, v); }
    void  SetAttackBoxHeight(float v)  { _attackBoxHeight  = std::max(4.f, v); }
    void  SetAttackBoxOffsetX(float v) { _attackBoxOffsetX = v; }
    void  SetAttackBoxOffsetY(float v) { _attackBoxOffsetY = v; }

    // Wider rect used only for player melee hit-detection; defaults to solid rect.
    // Cyclops overrides this so the body can be hit from any angle while the
    // narrow solid rect still lets the player walk up close.
    virtual Rectangle GetHitCollisionRec() const
    {
        Rectangle r = GetCollisionRec();
        constexpr float padX = 28.f;
        constexpr float padY = 18.f;
        return { r.x - padX, r.y - padY, r.width + padX * 2.f, r.height + padY * 2.f };
    }
	void PlayAttackSound() override;
    void PlayDeathSound() override;
    void PlayHurtSound() override;

protected:
    static void EnsureSharedResourcesLoaded();

    // Applies charactertuning_<Name>.txt overrides (scale, hitboxes, anim
    // speeds). Tunable classes call this at the END of their ResetForSpawn.
    void ApplyStoredTuning();

    // Clears all per-anim + whole-character tuning state; tunable classes call
    // this at the start of their ResetForSpawn before re-applying from file.
    void ResetTuningState();

    // Gameplay helpers for the per-anim data (current facing applied):
    bool    GetAnimBodyCapsuleWorld(Capsule2D& out) const;    // current slot's circle
    bool    GetAnimBodyRectWorld(Rectangle& out) const;       // its bounding square
    bool    GetAnimMeleeRectWorld(int slot, Rectangle& out) const;
    Vector2 GetCurrentAnimDrawOffset() const;                 // sprite-only shift

    Rectangle GetTunedCollisionRec() const
    {
        return Rectangle{
            _worldPos.x + _tunedCollisionRel.x,
            _worldPos.y + _tunedCollisionRel.y,
            _tunedCollisionRel.width,
            _tunedCollisionRel.height
        };
    }

    void HandleMovement(float dt, Vector2 navigationTarget, bool hasNavigationTarget,
        const std::vector<std::unique_ptr<Enemy>>& enemies, const std::vector<Vector2>& propCenters);
    Vector2 ResolveNavTarget(float dt, Vector2 playerFeet, Vector2 navigationTarget, bool hasNavigationTarget);
    void HandleAttack(const std::vector<std::unique_ptr<Enemy>>& enemies);
    void PickApproachOffset();
    bool CanTakeAttackSlot(const std::vector<std::unique_ptr<Enemy>>& enemies) const;
    bool UpdateEliteLunge(float dt);
    void UpdateBurnPanic(float dt);
    void UpdateBurns(float dt);
    void UpdateElectricCharge(float dt);
    void UpdateLaunchVisual(float dt);

    void HandleAnimation(float dt);
    virtual void DrawHealthBar(Vector2 screenPos, float w, float h);
    void DrawEliteLabel(Vector2 screenPos, float w, float h);

    // Slice a horizontal sprite strip of `frameCount` frames into the active
    // animation. Shared by every enemy/boss so the frame-math lives in one place;
    // subclasses forward their own _sheetFrameCount.
    void SetSpriteSheet(const Texture2D& sheet, int frameCount, float frameTime, bool resetFrame);

    // Health-bar geometry, tunable per subclass. Defaults match the legacy
    // melee bosses (Cyclops/Ogre); taller sprites override these in their ctor.
    float _healthBarHeight  = 6.f;    // bar thickness in px
    float _healthBarYFrac   = 0.5f;   // fraction of sprite height above the origin
    float _healthBarYOffset = 12.f;   // extra px above that

    struct PendingBurn
    {
        float timer = 0.f;
        int damage = 0;
        Vector2 sourcePos{};
    };

    Character*      _target = nullptr;
    NavigationGrid* _nav    = nullptr;   // non-owning; set once by Engine on spawn

    // ── Waypoint path cache ───────────────────────────────────────────────────
    // Extracted from the shared flow field. Each enemy refreshes its own path
    // on a staggered timer so not all enemies query the grid on the same frame.
    std::vector<Vector2> _waypoints;        // cached path from flow field
    int   _waypointIndex          = 0;     // which waypoint we're heading toward
    float _pathRefreshTimer       = 0.f;   // countdown to next refresh
    float _pathRefreshInterval    = 0.5f;  // randomised per-enemy at spawn
    static constexpr float kPathRefreshMin = 0.30f;  // minimum refresh interval
    static constexpr float kPathRefreshMax = 0.70f;  // maximum refresh interval
    static constexpr int   kMaxWaypoints   = 14;     // path length cap
    bool _isActive        = true;
    bool _bestiaryRecorded = false;   // has this death been counted in the bestiary
    bool _isEliteMiniboss = false;
    bool _isInvulnerable  = false;   // bodyguard shield (engine-driven)
    bool _leapInvulnerable = false;  // gap-closer wind-up (engine-driven)
    // Why the most recent hit was denied; set by TakeDamage / a subclass block,
    // cleared when the hit code reads it via ConsumeHitBlock(). Subclasses (e.g.
    // Shieldbearer) set this before returning early from their own TakeDamage.
    HitBlockReason _hitBlock = HitBlockReason::None;

    bool  _attacking    = false;
    bool  _damageApplied = false;

    enum class LungeState { None, Windup, Lunging, Recovery };
    LungeState _lungeState = LungeState::None;
    float   _lungeTimer = 0.f;
    float   _lungeCooldown = 0.f;
    Vector2 _lungeDir = {};
    bool    _lungeDamageApplied = false;
    static constexpr float kEliteLungeWindup = 0.42f;
    static constexpr float kEliteLungeDuration = 0.18f;
    static constexpr float kEliteLungeRecovery = 0.35f;
    static constexpr float kEliteLungeCooldown = 2.4f;
    static constexpr float kEliteLungeSpeed = 720.f;
    static constexpr float kEliteLungeMinRange = 145.f;
    static constexpr float kEliteLungeMaxRange = 430.f;

    Vector2 _burnPanicDir = {};
    float   _burnPanicTurnTimer = 0.f;
    float   _burnSoundTimer = 0.f;

    float   _warAuraTimer = 0.f;   // > 0 while buffed by a Warchief banner

    // Boss enrage phase (see UpdateEnrageLatch). _enrageThreshold set per boss.
    float   _enrageThreshold  = 0.f;   // fraction of max HP; 0 = never enrages
    bool    _enrageLatched    = false; // one-way once crossed (until full-HP respawn)
    bool    _enrageShakePending = false; // transition telegraph, consumed by CombatDirector
    float   _enrageFlashTimer = 0.f;   // brief visual flash window on transition
    const char* _bossCallout  = nullptr; // pending floating word, consumed by the runtime

    // Multi-phase boss system (see UpdatePhaseLatch). Empty thresholds = single-phase.
    std::vector<float> _phaseThresholds;      // descending HP fractions, e.g. {0.66,0.33}
    int     _phase              = 0;          // phases crossed so far (0 = opening phase)
    int     _pendingPhaseChange = -1;         // new phase to announce once; -1 = none
    float   _phaseTransitionTimer = 0.f;      // >0 while a transition set-piece plays

    float   _freezeTimer        = 0.f;

    // ── Shared status effects (ARPG combat pass) ────────────────────────────────
    // Bosses scale status durations by this so control never fully dominates them.
    static constexpr float kBossStatusDurMult = 0.5f;
    float _poisonTimer     = 0.f;   // remaining poison duration
    float _poisonTickTimer = 0.f;   // countdown to next poison tick
    int   _poisonPerTick   = 0;     // base damage per tick (before stacks)
    int   _poisonStacks    = 0;     // stacking intensity (capped)
    float _bleedTimer      = 0.f;
    float _bleedTickTimer  = 0.f;
    int   _bleedPerTick    = 0;
    float _slowTimer       = 0.f;
    float _slowMult        = 1.f;   // movement multiplier while slowed (<1)
    float _vulnTimer       = 0.f;
    float _vulnMult        = 1.f;   // incoming-damage multiplier while vulnerable (>1)
    float _markTimer       = 0.f;   // "marked"/execute window
    float _curseTimer      = 0.f;   // Warlock curse tag (bonus damage window)

    // Graveyard revive — set by Engine when entering a Graveyard room
    bool    _graveReviveAvailable  = false;
    float   _graveReviveInvulTimer = 0.f;

    // Dream Realm flicker — biome mechanic, managed externally by Engine
    float   _flickerCooldown    = 0.f;
    float   _flickerWindupTimer = 0.f;
    bool    _flickerInWindup    = false;
    Vector2 _flickerTarget      = {};

    // Electric charge — applied by ElectricSpread hits; repeating random stuns, capped at 10 s
    bool    _isCharged              = false;
    float   _chargeNextStunTime     = 0.f;   // countdown to next stun trigger
    float   _electricChargeTotalTimer = 0.f; // overall charge window; zeroes out the effect

    int     _expValue       = 1;

    // Stuck detection — if barely moved for _stuckThreshold seconds, apply a kick
    float   _stuckTimer     = 0.f;
    Vector2 _stuckCheckPos  = {};
    static constexpr float _stuckThreshold  = 0.4f;
    static constexpr float _stuckMinMove    = 8.f;   // pixels needed to not be considered stuck

    bool    _forcedPushActive    = false;
    Vector2 _forcedPushDirection = {};
    float   _forcedPushSpeed     = 0.f;

    float _attackRange = 110.f;
    float _attackUpdateTime = 1.f / 8.f;
    bool  _inAttackRange = false;
    static constexpr float _attackRangeHysteresis = 15.f;

    float _attackBoxWidth   = 48.f;
    float _attackBoxHeight  = 75.f;
    float _attackBoxOffsetX = 57.f;
    float _attackBoxOffsetY = 0.f;

    // Sprite-only draw offset for the attack animation — compensates for
    // the weapon swing requiring the body to be off-centre in the art.
    // X is multiplied by _rightLeft so the correction mirrors correctly.
    float _attackVisualOffsetX = 0.f;
    float _attackVisualOffsetY = 0.f;

    float _attackCooldown = 0.f;
    float _attackDelay = 1.0f;
    float _launchVisualTimer = 0.f;
    bool _launchHoldingHurtPose = false;
    static constexpr float _launchVisualDuration = 0.22f;
    static constexpr float _launchVisualScaleBoost = 0.18f;
    static constexpr float _launchVisualLift = 18.f;

    // Flank tuning: regular enemies should not all aim for the exact same
    // player point. Each one picks a small left/right lane and tries to arc
    // into that lane once it gets near the player.
    float _flankSide = 1.f;
    float _flankDistance = 80.f;
    static constexpr float _flankStartDistance = 0.f;   // disabled — approach offset handles spread
    static constexpr float _flankBlendStrength = 0.35f;
    static constexpr float _minFlankDistance = 55.f;
    static constexpr float _maxFlankDistance = 110.f;
    static constexpr float _propSlideStrength = 0.75f;

    // 8-direction approach: each enemy picks one of 8 slots around the player
    // and moves toward that offset point. Resets on a timer and after each hit
    // so enemies shift positions and never permanently crowd one side.
    Vector2 _approachOffset = {};
    float   _approachOffsetTimer = 0.f;
    static constexpr float _approachOffsetRadius   = 120.f;
    static constexpr float _approachOffsetDuration = 3.5f;

    Vector2 _homePos;
    std::vector<PendingBurn> _pendingBurns;

    // ── Character Animator tuning state ──────────────────────────────────────
    bool      _hasTunedCollision = false;
    Rectangle _tunedCollisionRel{};              // relative to _worldPos
    float     _editorAnimFrameTimes[10] = {};    // per-anim overrides; <=0 = default

    // Per-animation body circle / melee box / draw offset (kAnimSlots entries).
    bool      _animBodySet[10] = {};
    Vector2   _animBodyOffset[10] = {};          // relative to _worldPos, facing right
    float     _animBodyRadius[10] = {};
    bool      _animMeleeSet[10] = {};
    Rectangle _animMeleeRel[10] = {};            // relative to _worldPos, facing right
    bool      _animDrawSet[10] = {};
    Vector2   _animDrawOffset[10] = {};          // sprite-only shift, facing right

    static Texture2D _sharedIdleAnim;
    static Texture2D _sharedWalkAnim;
    static Texture2D _sharedAttackAnim;
    static Texture2D _sharedTakeDamageAnim;
    static Texture2D _sharedDeathAnim;
    static Sound _sharedAttackSound;
    static Sound _sharedHurtSound;
    static Sound _sharedDeathSound;
    static bool _sharedResourcesLoaded;


};
