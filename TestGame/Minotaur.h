#pragma once

#include "Enemy.h"
#include "raymath.h"
#include <vector>
#include <memory>

// =============================================================================
// Minotaur — boss of the ancient-civilisation biomes (Ancient Castle, Lost City).
// A hulking axe-wielder built around raw momentum — the fight is a matador
// dance:
//   - Bull Rush: telegraphed straight-line charge. If it ends at a WALL he
//     crashes and is STUNNED for 1.6s, taking full damage — baiting rushes
//     into walls is the core punish loop.
//   - Enraged (<40% HP) he chains a second rush immediately after the first
//     with a much shorter re-aim windup.
//   - Earthshatter Stomp: when the player hugs him, he slams the ground —
//     radial shockwave damage with a visible expanding ring.
//   - Axe cleave for standard close-range pressure; fastest walker of the
//     bosses, so there is no safe camping spot.
// =============================================================================
class Minotaur : public Enemy
{
public:
    Minotaur(Vector2 pos);
    ~Minotaur() override;

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
    Capsule2D GetCapsule()      const override;

    Minotaur* AsMinotaur() override { return this; }
    bool IsBoss() const override { return true; }
    bool UsesDirectPursuit() const override { return true; }

    // Character Animator support
    const char* GetTuningName() const override { return "Minotaur"; }
    int         GetEditorAnimCount() const override { return 6; }
    const char* GetEditorAnimName(int index) const override;
    void        PlayEditorAnim(int index) override;
    int         GetCurrentAnimSlot() const override;

    bool ConsumeImpactShakeRequest();

private:
    enum class State
    {
        Chasing,
        MeleeAttacking,
        RushWindup,     // paws the ground, locks the charge line
        Rushing,        // full-speed charge
        Stunned,        // crashed into a wall — wide-open punish window
        StompWindup,    // raises the leg
        Stomping,       // shockwave frame
        Recovery
    };

    static void EnsureSharedResourcesLoaded();

    void SetAnimation(const Texture2D& sheet, float frameTime, bool resetFrame);
    void HandleChasing(float dt, Vector2 heroWorldPos);
    void HandleMelee();
    void HandleRushWindup(float dt);
    void HandleRushing(float dt);
    void HandleStunned(float dt);
    void HandleStompWindup(float dt);
    void HandleStomping(float dt);
    void HandleRecovery(float dt);
    void HandleAnimation(float dt);
    void TryDealContactDamage();
    void EndRush(bool crashedIntoWall);
    Rectangle GetBodyContactRec() const;
    Vector2 GetPushDirectionToPlayer() const;

    State _state = State::Chasing;
    float _stateTimer      = 0.f;
    float _meleeCooldown   = 0.f;
    float _contactCooldown = 0.f;
    float _rushCooldown    = 0.f;
    float _stompCooldown   = 0.f;
    Vector2 _rushDirection{ 1.f, 0.f };
    float _rushTravelled   = 0.f;
    bool  _rushHitPlayer   = false;
    int   _rushChainRemaining = 0;   // enraged: extra rushes queued after this one
    float _stompRingRadius = 0.f;
    bool  _stompDamageApplied = false;
    bool  _impactShakeRequested = false;
    float _stableFrameW = 0.f;
    float _stableFrameH = 0.f;

    static constexpr int   _sheetFrameCount     = 6;
    static constexpr float _bossScale           = 4.8f;
    static constexpr float _moveSpeed           = 285.f;   // fastest boss on foot
    static constexpr float _stunDuration        = 1.6f;
    static constexpr float _rushChainWindup     = 0.38f;   // re-aim between chained rushes
    static constexpr float _meleeRange          = 220.f;
    static constexpr float _meleeCooldownBase   = 1.8f;
    static constexpr float _contactCooldownBase = 0.75f;
    static constexpr float _bossDamagePerHit    = 0.5f;
    static constexpr float _bossPushSpeed       = 1500.f;
    static constexpr float _rushWindupDuration  = 0.8f;
    static constexpr float _rushSpeed           = 980.f;
    static constexpr float _rushMaxDistance     = 1500.f;
    static constexpr float _rushCooldownBase    = 5.5f;
    static constexpr float _stompWindupDuration = 0.55f;
    static constexpr float _stompRadius         = 300.f;
    static constexpr float _stompCooldownBase   = 6.5f;
    static constexpr float _recoveryDuration    = 0.6f;
    static constexpr int   _bossBaseExpValue    = 15;

    static Texture2D _sharedIdleAnim;
    static Texture2D _sharedWalkAnim;
    static Texture2D _sharedMeleeAnim;
    static Texture2D _sharedStompAnim;
    static Texture2D _sharedHurtAnim;
    static Texture2D _sharedDeathAnim;
    static Sound     _sharedAttackSound;
    static Sound     _sharedHurtSound;
    static Sound     _sharedDeathSound;
    static Sound     _sharedRushSound;
    static Sound     _sharedStompSound;
    static bool      _sharedResourcesLoaded;
};
