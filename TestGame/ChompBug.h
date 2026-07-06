#pragma once

#include "Enemy.h"
#include "raymath.h"
#include <vector>
#include <memory>

// =============================================================================
// ChompBug — boss of the Jungle. The only FLYING boss:
//   - Never stands still: orbits the player at mid range, buzzing constantly.
//   - Dive-Bomb: locks a strafing line THROUGH the player and swoops along it
//     at high speed, chomping anything on the path (telegraphed line).
//   - Acid Barrage: hovers and spits a spread of toxic globs.
//   - Below 40% HP it enrages: tighter orbits, back-to-back dives.
// Grounded melee builds struggle while it orbits — punish it during the
// post-dive hover or force it down with crowd control.
// =============================================================================
class ChompBug : public Enemy
{
public:
    ChompBug(Vector2 pos);
    ~ChompBug() override;

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

    ChompBug* AsChompBug() override { return this; }
    bool IsBoss() const override { return true; }
    bool UsesDirectPursuit()     const override { return true; }
    bool IgnoresPropCollisions() const override { return true; }

    // Spit volley interface (read by CombatDirector after Update)
    bool    WantsToSpit() const { return _pendingSpit; }
    Vector2 GetSpitDirection() const { return _spitDirection; }
    int     GetSpitCount() const { return _spitCount; }
    void    OnSpitFired();

    // Character Animator support
    const char* GetTuningName() const override { return "ChompBug"; }
    int         GetEditorAnimCount() const override { return 6; }
    const char* GetEditorAnimName(int index) const override;
    void        PlayEditorAnim(int index) override;
    int         GetCurrentAnimSlot() const override;

private:
    enum class State
    {
        Orbiting,
        DiveAiming,     // hovers, locks the strafing line
        Diving,         // high-speed swoop along the line
        SpitCasting,
        Recovery
    };

    static void EnsureSharedResourcesLoaded();
    void SetAnimation(const Texture2D& sheet, float frameTime, bool resetFrame);
    void TryDealContactDamage();
    Rectangle GetBodyContactRec() const;
    Vector2 GetPushDirectionToPlayer() const;
    void HandleAnimation(float dt);

    State _state = State::Orbiting;
    float _stateTimer      = 0.f;
    float _contactCooldown = 0.f;
    float _diveCooldown    = 0.f;
    float _spitCooldown    = 0.f;
    float _orbitAngle      = 0.f;
    float _orbitSign       = 1.f;
    Vector2 _diveStart{};
    Vector2 _diveEnd{};
    float _diveTimer       = 0.f;
    bool  _diveHitApplied  = false;
    bool  _pendingSpit     = false;
    Vector2 _spitDirection{};
    int   _spitCount       = 3;
    float _buzzTimer       = 0.f;
    float _stableFrameW = 0.f;
    float _stableFrameH = 0.f;

    static constexpr int   _sheetFrameCount     = 6;
    static constexpr float _bossScale           = 5.2f;
    static constexpr float _orbitRadius         = 340.f;
    static constexpr float _orbitSpeed          = 1.7f;    // radians/sec
    static constexpr float _contactCooldownBase = 0.75f;
    static constexpr float _bossDamagePerHit    = 0.5f;
    static constexpr float _bossPushSpeed       = 1300.f;
    static constexpr float _diveAimDuration     = 0.7f;
    static constexpr float _diveTravelDuration  = 0.55f;
    static constexpr float _diveOvershoot       = 320.f;   // flies past the player
    static constexpr float _diveCooldownBase    = 4.2f;
    static constexpr float _spitCastDuration    = 0.8f;
    static constexpr float _spitCooldownBase    = 5.5f;
    static constexpr float _recoveryDuration    = 0.6f;
    static constexpr int   _bossBaseExpValue    = 15;

    static Texture2D _sharedIdleAnim;
    static Texture2D _sharedFlyAnim;
    static Texture2D _sharedMeleeAnim;
    static Texture2D _sharedSpitAnim;
    static Texture2D _sharedHurtAnim;
    static Texture2D _sharedDeathAnim;
    static Sound     _sharedAttackSound;
    static Sound     _sharedHurtSound;
    static Sound     _sharedDeathSound;
    static Sound     _sharedSpitSound;
    static bool      _sharedResourcesLoaded;
};
