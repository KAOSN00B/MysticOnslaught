#pragma once

#include "Enemy.h"
#include "raymath.h"
#include <vector>
#include <memory>

// =============================================================================
// Osiris — boss of the Lost City. A judge-mage who controls space with magic:
//   - Sand Step: teleports away whenever the player closes in (mirage swirl).
//   - Judgement Nova: a full ring of bolts fired outward in all directions —
//     find the gap and thread it.
//   - Wrath Volley: tight aimed 3-bolt burst (5 when below half HP).
//   - Khopesh sweep if you do corner him.
// The anti-melee boss: you must chase a caster who refuses to be caught.
// =============================================================================
class Osiris : public Enemy
{
public:
    Osiris(Vector2 pos);
    CreatureFamily GetCreatureFamily() const override { return CreatureFamily::Boss; }
    ~Osiris() override;

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

    Osiris* AsOsiris() override { return this; }
    bool IsBoss() const override { return true; }
    bool UsesAttackLunge() const override { return false; }  // kiting caster; melee uses its own anim
    bool UsesDirectPursuit() const override { return true; }

    // Projectile interface (read by CombatDirector after Update)
    bool    WantsToCastNova() const { return _pendingNova; }
    int     GetNovaBoltCount() const { return _novaBoltCount; }
    void    OnNovaCast() { _pendingNova = false; }
    bool    WantsToCastVolley() const { return _pendingVolley; }
    Vector2 GetVolleyDirection() const { return _volleyDirection; }
    int     GetVolleyBoltCount() const { return _volleyBoltCount; }
    void    OnVolleyCast() { _pendingVolley = false; }

    // Character Animator support
    const char* GetTuningName() const override { return "Osiris"; }
    int         GetEditorAnimCount() const override { return 6; }
    const char* GetEditorAnimName(int index) const override;
    void        PlayEditorAnim(int index) override;
    int         GetCurrentAnimSlot() const override;

private:
    enum class State
    {
        Stalking,       // slow drift, keeps distance
        MeleeAttacking,
        NovaCasting,
        VolleyCasting,
        TeleportOut,
        TeleportIn,
        Recovery
    };

    static void EnsureSharedResourcesLoaded();
    void SetAnimation(const Texture2D& sheet, float frameTime, bool resetFrame);
    void BeginTeleport();
    Vector2 PickTeleportSpot() const;
    void TryDealContactDamage();
    Rectangle GetBodyContactRec() const;
    Vector2 GetPushDirectionToPlayer() const;
    void HandleAnimation(float dt);

    State _state = State::Stalking;
    float _stateTimer       = 0.f;
    float _meleeCooldown    = 0.f;
    float _contactCooldown  = 0.f;
    float _novaCooldown     = 0.f;
    float _volleyCooldown   = 0.f;
    float _teleportCooldown = 0.f;
    bool  _pendingNova      = false;
    int   _novaBoltCount    = 8;
    bool  _pendingVolley    = false;
    Vector2 _volleyDirection{};
    int   _volleyBoltCount  = 3;
    int   _novaChainRemaining = 0;    // phase 1+: fire a second Judgement Nova
    bool  _pendingPhaseNova   = false;// phase change opens with a nova
    Vector2 _teleportTarget{};
    float _stableFrameW = 0.f;
    float _stableFrameH = 0.f;

    static constexpr int   _sheetFrameCount     = 6;
    static constexpr float _bossScale           = 4.8f;
    static constexpr float _moveSpeed           = 165.f;
    static constexpr float _meleeRange          = 200.f;
    static constexpr float _meleeCooldownBase   = 2.0f;
    static constexpr float _contactCooldownBase = 0.75f;
    static constexpr float _bossDamagePerHit    = 0.5f;
    static constexpr float _bossPushSpeed       = 1350.f;
    static constexpr float _novaCastDuration    = 1.1f;
    static constexpr float _novaCooldownBase    = 6.0f;
    static constexpr float _volleyCastDuration  = 0.7f;
    static constexpr float _volleyCooldownBase  = 3.6f;
    static constexpr float _teleportOutDuration = 0.32f;
    static constexpr float _teleportInDuration  = 0.26f;
    static constexpr float _teleportCooldownBase = 2.8f;
    static constexpr float _panicDistance       = 230.f;   // player this close -> Sand Step
    static constexpr float _recoveryDuration    = 0.5f;
    static constexpr int   _bossBaseExpValue    = 15;

    static Texture2D _sharedIdleAnim;
    static Texture2D _sharedWalkAnim;
    static Texture2D _sharedMeleeAnim;
    static Texture2D _sharedMagicAnim;
    static Texture2D _sharedHurtAnim;
    static Texture2D _sharedDeathAnim;
    static Sound     _sharedAttackSound;
    static Sound     _sharedHurtSound;
    static Sound     _sharedDeathSound;
    static Sound     _sharedCastSound;
    static bool      _sharedResourcesLoaded;
};
