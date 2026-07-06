#pragma once

#include "Enemy.h"
#include "raymath.h"
#include <vector>
#include <memory>

// =============================================================================
// TitanGuard — boss of The Sanctuary. A living fortress with one rule:
//   HIS SHIELD ALWAYS FACES YOU. Frontal damage is reduced to chip (1/5);
//   full damage only lands from BEHIND — dash through or around him.
//   - Slow, deliberate advance; heavy mace sweep up close.
//   - Bomb Lob: hurls explosive spheres (lavaballs) at range.
//   - Bulwark Slam at 50% HP: plants the shield for a shockwave that must be
//     dodged, then briefly staggers — his only truly open window from the front.
// =============================================================================
class TitanGuard : public Enemy
{
public:
    TitanGuard(Vector2 pos);
    ~TitanGuard() override;

    void Init();
    void ResetForSpawn(Vector2 pos) override;
    static void UnloadSharedResources();

    void Update(float dt, Vector2 heroWorldPos, Vector2 navigationTarget, bool hasNavigationTarget,
        const std::vector<std::unique_ptr<Enemy>>& enemies,
        const std::vector<Vector2>& propCenters) override;
    void SetWaveScale(int wave) override;
    void DrawEnemy(Vector2 cameraRef) override;
    void TakeDamage(int damage, Vector2 attackerPos) override;
    void ApplyFreeze(float duration) override;
    Rectangle GetCollisionRec() const override;
    Capsule2D GetCapsule() const override;

    TitanGuard* AsTitanGuard() override { return this; }
    bool IsBoss() const override { return true; }
    bool UsesDirectPursuit() const override { return true; }

    bool ConsumeImpactShakeRequest();

    // Bomb interface (read by CombatDirector after Update; spawns lavaballs)
    bool    WantsToThrowBomb() const { return _pendingBomb; }
    Vector2 GetBombTarget() const { return _bombTarget; }
    Vector2 GetBombSpawnPos() const;
    void    OnBombThrown() { _pendingBomb = false; }

    // Character Animator support
    const char* GetTuningName() const override { return "TitanGuard"; }
    int         GetEditorAnimCount() const override { return 7; }
    const char* GetEditorAnimName(int index) const override;
    void        PlayEditorAnim(int index) override;
    int         GetCurrentAnimSlot() const override;

private:
    enum class State
    {
        Advancing,
        MeleeAttacking,
        BombThrowing,
        SlamWindup,     // Bulwark Slam telegraph (Defend anim)
        Slamming,       // shockwave
        Staggered,      // post-slam opening
        Recovery
    };

    static void EnsureSharedResourcesLoaded();
    void SetAnimation(const Texture2D& sheet, float frameTime, bool resetFrame);
    void TryDealContactDamage();
    Rectangle GetBodyContactRec() const;
    Vector2 GetPushDirectionToPlayer() const;
    void HandleAnimation(float dt);

    State _state = State::Advancing;
    float _stateTimer      = 0.f;
    float _meleeCooldown   = 0.f;
    float _contactCooldown = 0.f;
    float _bombCooldown    = 0.f;
    bool  _pendingBomb     = false;
    Vector2 _bombTarget{};
    bool  _slamDone        = false;   // once at 50% HP
    float _slamRingRadius  = 0.f;
    bool  _slamDamageApplied = false;
    bool  _impactShakeRequested = false;
    float _stableFrameW = 0.f;
    float _stableFrameH = 0.f;

    static constexpr int   _sheetFrameCount     = 6;
    static constexpr float _bossScale           = 4.9f;
    static constexpr float _moveSpeed           = 145.f;   // slowest boss on foot
    static constexpr float _meleeRange          = 220.f;
    static constexpr float _meleeCooldownBase   = 2.1f;
    static constexpr float _contactCooldownBase = 0.75f;
    static constexpr float _bossDamagePerHit    = 0.5f;
    static constexpr float _bossPushSpeed       = 1550.f;   // heaviest shove
    static constexpr float _bombCastDuration    = 0.9f;
    static constexpr float _bombCooldownBase    = 4.4f;
    static constexpr float _slamWindupDuration  = 1.0f;
    static constexpr float _slamRadius          = 330.f;
    static constexpr float _staggerDuration     = 2.0f;
    static constexpr float _recoveryDuration    = 0.55f;
    static constexpr int   _bossBaseExpValue    = 15;

    static Texture2D _sharedIdleAnim;
    static Texture2D _sharedWalkAnim;
    static Texture2D _sharedMeleeAnim;
    static Texture2D _sharedBombAnim;
    static Texture2D _sharedDefendAnim;
    static Texture2D _sharedHurtAnim;
    static Texture2D _sharedDeathAnim;
    static Sound     _sharedAttackSound;
    static Sound     _sharedHurtSound;
    static Sound     _sharedDeathSound;
    static Sound     _sharedBlockSound;
    static Sound     _sharedSlamSound;
    static bool      _sharedResourcesLoaded;
};
