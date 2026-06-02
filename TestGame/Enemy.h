#pragma once
#include "BaseCharacter.h"
#include "Character.h"
#include "NavigationGrid.h"
#include "raymath.h"
#include <vector>
#include <memory>

class Cyclops;
class Ogre;
class Molarbeast;

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

    // Ogre charge — sustained push until the enemy slides into a wall or prop.
    void StartForcedPush(Vector2 direction, float speed);
    virtual void OnForcedPushCollision();
    bool IsBeingForcedPushed() const { return _forcedPushActive; }

    virtual bool IsFrozen()        const { return _freezeTimer > 0.f; }
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
    virtual bool UsesDirectPursuit() const { return false; }
    virtual bool IgnoresPropCollisions() const { return false; }
    virtual bool IsBoss() const { return false; }
    bool IsEliteMiniboss() const { return _isEliteMiniboss; }
    void SetIsEliteMiniboss(bool b);

    // ── Elite-room mechanics ──────────────────────────────────────────────
    // _isInvulnerable : bodyguard shield — engine clears when all grunts die
    // _leapInvulnerable: gap-closer wind-up — engine sets/clears around leap
    void SetInvulnerable(bool v)   { _isInvulnerable    = v; }
    void SetLeapFrozen(bool v)     { _leapInvulnerable  = v; }
    bool IsInvulnerable()    const { return _isInvulnerable; }
    bool IsLeapFrozen()      const { return _leapInvulnerable; }
    void ApplyEnrage();            // +50% speed, half attack cooldown, called by Engine on elite spawn
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

    void HandleMovement(float dt, Vector2 navigationTarget, bool hasNavigationTarget,
        const std::vector<std::unique_ptr<Enemy>>& enemies, const std::vector<Vector2>& propCenters);
    void HandleAttack();
    void PickApproachOffset();
    void UpdateBurns(float dt);
    void UpdateElectricCharge(float dt);
    void UpdateLaunchVisual(float dt);

    void HandleAnimation(float dt);
    virtual void DrawHealthBar(Vector2 screenPos, float w, float h);
    void DrawEliteLabel(Vector2 screenPos, float w, float h);

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
    bool _isEliteMiniboss = false;
    bool _isInvulnerable  = false;   // bodyguard shield (engine-driven)
    bool _leapInvulnerable = false;  // gap-closer wind-up (engine-driven)

    bool  _attacking    = false;
    bool  _damageApplied = false;

    float   _freezeTimer        = 0.f;

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
    float _attackVisualOffsetX = 20.f;
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
