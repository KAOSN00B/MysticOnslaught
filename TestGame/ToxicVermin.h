#pragma once

#include "Enemy.h"
#include "raymath.h"
#include <vector>
#include <memory>

// =============================================================================
// ToxicVermin — boss of the Wastelands. A burrowing plague worm:
//   - BURROWS underground (unhittable), tunnels toward the player — a moving
//     dirt mound telegraphs the path — then ERUPTS underneath them.
//   - Eruptions and spit attacks leave lingering POISON POOLS (Engine clouds).
//   - Above ground it lunges with bites and spits toxic glob fans.
//   - Below 40% HP it stays underground longer and erupts twice in a row.
// The floor itself becomes the enemy: track the mound, keep moving.
// =============================================================================
class ToxicVermin : public Enemy
{
public:
    ToxicVermin(Vector2 pos);
    CreatureFamily GetCreatureFamily() const override { return CreatureFamily::Small; }
    ~ToxicVermin() override;

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

    ToxicVermin* AsToxicVermin() override { return this; }
    bool IsBoss() const override { return true; }
    bool UsesDirectPursuit()     const override { return true; }
    bool IgnoresPropCollisions() const override { return true; }

    bool ConsumeImpactShakeRequest();

    // Spit interface (read by CombatDirector after Update)
    bool    WantsToSpit() const { return _pendingSpit; }
    Vector2 GetSpitDirection() const { return _spitDirection; }
    int     GetSpitCount() const { return _spitCount; }
    void    OnSpitFired() { _pendingSpit = false; }

    // Poison pool requests (Engine spawns clouds; consumed like the spit)
    bool    ConsumePoisonPoolRequest(Vector2& outPos);

    // ── Hybrid encounter pattern: Plague Flood ───────────────────────────────
    void DrawEliteTelegraph() const override;
    void DebugForceEliteSignature() override;
    void DebugForceElitePhaseTwo() override;
    const char* GetEliteSignatureStateName() const override;

    // Character Animator support
    const char* GetTuningName() const override { return "ToxicVermin"; }
    int         GetEditorAnimCount() const override { return 8; }
    const char* GetEditorAnimName(int index) const override;
    void        PlayEditorAnim(int index) override;
    int         GetCurrentAnimSlot() const override;

private:
    enum class State
    {
        Surface,        // slithers, bites, spits
        MeleeAttacking,
        SpitCasting,
        Burrowing,      // sinks (Jump anim)
        Tunnelling,     // underground — mound chases the player
        Erupting,       // bursts out under the player (Fall anim)
        Recovery
    };

    static void EnsureSharedResourcesLoaded();
    void SetAnimation(const Texture2D& sheet, float frameTime, bool resetFrame);
    void TryDealContactDamage();
    Rectangle GetBodyContactRec() const;
    Vector2 GetPushDirectionToPlayer() const;
    void HandleAnimation(float dt);

    State _state = State::Surface;
    float _stateTimer      = 0.f;
    float _meleeCooldown   = 0.f;
    float _contactCooldown = 0.f;
    float _spitCooldown    = 0.f;
    float _burrowCooldown  = 0.f;
    bool  _pendingSpit     = false;
    Vector2 _spitDirection{};
    int   _spitCount       = 3;
    bool  _pendingPoisonPool = false;
    Vector2 _poisonPoolPos{};
    bool  _eruptDamageApplied = false;
    int   _eruptChainRemaining = 0;
    bool  _pendingPhaseBurrow = false;   // phase change dives for an eruption
    bool  _impactShakeRequested = false;
    float _stableFrameW = 0.f;
    float _stableFrameH = 0.f;

    static constexpr int   _sheetFrameCount     = 6;
    static constexpr float _bossScale           = 5.6f;
    static constexpr float _surfaceSpeed        = 210.f;
    static constexpr float _tunnelSpeed         = 400.f;
    static constexpr float _meleeRange          = 190.f;
    static constexpr float _meleeCooldownBase   = 1.8f;
    static constexpr float _contactCooldownBase = 0.75f;
    static constexpr float _bossDamagePerHit    = 0.5f;
    static constexpr float _bossPushSpeed       = 1350.f;
    static constexpr float _spitCastDuration    = 0.75f;
    static constexpr float _spitCooldownBase    = 4.0f;
    static constexpr float _burrowDuration      = 0.5f;
    static constexpr float _tunnelDuration      = 2.4f;
    static constexpr float _burrowCooldownBase  = 6.0f;
    static constexpr float _eruptRadius         = 200.f;
    static constexpr float _recoveryDuration    = 0.6f;
    static constexpr int   _bossBaseExpValue    = 15;

    static Texture2D _sharedIdleAnim;
    static Texture2D _sharedWalkAnim;
    static Texture2D _sharedMeleeAnim;
    static Texture2D _sharedSpitAnim;
    static Texture2D _sharedBurrowAnim;
    static Texture2D _sharedEmergeAnim;
    static Texture2D _sharedHurtAnim;
    static Texture2D _sharedDeathAnim;
    static Sound     _sharedAttackSound;
    static Sound     _sharedHurtSound;
    static Sound     _sharedDeathSound;
    static Sound     _sharedBurrowSound;
    static bool      _sharedResourcesLoaded;
};
