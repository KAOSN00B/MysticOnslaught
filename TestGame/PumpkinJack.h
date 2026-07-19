#pragma once

#include "Enemy.h"
#include "raymath.h"
#include <vector>
#include <memory>

// =============================================================================
// PumpkinJack — boss of the haunted biomes (Graveyard, Forest, Jungle).
// A trickster knight with a jack-o'-lantern head who barely walks at all —
// he VANISHES in a swirl of purple flame and reappears somewhere else:
//   - Trick Step: at range he teleports to a new spot around the player and
//     immediately follows up with a volley (or an axe if he lands close).
//   - When enraged (<50% HP) he teleports DIRECTLY BEHIND the player for a
//     surprise axe swing — keep moving after he vanishes.
//   - Harvest Volley: fan of fire bolts (3, or 5 when enraged).
//   - Grave Call: summons shadow grunts to fight alongside him.
//   - Defend: periodically raises his shield — damage is quartered until it
//     drops, so back off instead of trading.
// =============================================================================
class PumpkinJack : public Enemy
{
public:
    PumpkinJack(Vector2 pos);
    CreatureFamily GetCreatureFamily() const override { return CreatureFamily::Boss; }
    ~PumpkinJack() override;

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

    PumpkinJack* AsPumpkinJack() override { return this; }
    bool IsBoss() const override { return true; }
    bool UsesDirectPursuit() const override { return true; }

    // ── Hybrid encounter pattern: Harvest Ritual ─────────────────────────────
    void DrawEliteTelegraph() const override;
    void DebugForceEliteSignature() override;
    void DebugForceElitePhaseTwo() override;
    const char* GetEliteSignatureStateName() const override;

    // Character Animator support
    const char* GetTuningName() const override { return "PumpkinJack"; }
    int         GetEditorAnimCount() const override { return 7; }
    const char* GetEditorAnimName(int index) const override;
    void        PlayEditorAnim(int index) override;
    int         GetCurrentAnimSlot() const override;

    // Volley interface (read by CombatDirector after Update)
    bool    WantsToCastVolley() const { return _pendingVolley; }
    Vector2 GetVolleyDirection() const { return _volleyDirection; }
    int     GetVolleyBoltCount() const { return _volleyBoltCount; }
    void    OnVolleyCast();

    // Summon interface — number of shadow grunts to spawn this frame (0 = none).
    int ConsumeSummonRequest();

private:
    enum class State
    {
        Chasing,
        MeleeAttacking,
        VolleyCasting,
        Summoning,
        Defending,
        TeleportOut,    // vanishing in purple flame
        TeleportIn,     // reappearing at the new spot
        Recovery
    };

    static void EnsureSharedResourcesLoaded();

    void SetAnimation(const Texture2D& sheet, float frameTime, bool resetFrame);
    void HandleChasing(float dt, Vector2 heroWorldPos);
    void HandleMelee();
    void HandleVolleyCast(float dt);
    void HandleSummoning(float dt);
    void HandleDefending(float dt);
    void HandleTeleportOut(float dt);
    void HandleTeleportIn(float dt);
    void BeginTeleport();
    Vector2 PickTeleportSpot() const;
    void HandleRecovery(float dt);
    void HandleAnimation(float dt);
    void TryDealContactDamage();
    Rectangle GetBodyContactRec() const;
    Vector2 GetPushDirectionToPlayer() const;

    State _state = State::Chasing;
    float _stateTimer      = 0.f;
    float _meleeCooldown   = 0.f;
    float _contactCooldown = 0.f;
    float _volleyCooldown  = 0.f;
    float _summonCooldown  = 0.f;
    float _defendCooldown  = 0.f;
    bool    _pendingVolley = false;
    Vector2 _volleyDirection{};
    int     _volleyBoltCount = 3;
    int     _pendingSummonCount = 0;
    float   _teleportCooldown = 0.f;
    Vector2 _teleportTarget{};
    bool    _teleportAmbush = false;   // phase 1+ behind-the-player teleport
    bool    _pendingPhaseSummon = false; // phase change opens with a Grave Call
    float _stableFrameW = 0.f;
    float _stableFrameH = 0.f;

    static constexpr int   _sheetFrameCount     = 6;
    static constexpr float _bossScale           = 5.2f;
    static constexpr float _moveSpeed           = 240.f;
    static constexpr float _meleeRange          = 200.f;
    static constexpr float _meleeCooldownBase   = 1.7f;
    static constexpr float _contactCooldownBase = 0.75f;
    static constexpr float _bossDamagePerHit    = 0.5f;
    static constexpr float _bossPushSpeed       = 1350.f;
    static constexpr float _volleyCooldownBase  = 5.2f;
    static constexpr float _volleyCastDuration  = 0.9f;
    static constexpr float _summonCooldownBase  = 12.f;
    static constexpr float _summonCastDuration  = 1.3f;
    static constexpr float _defendCooldownBase  = 9.f;
    static constexpr float _defendDuration      = 2.4f;
    static constexpr float _teleportOutDuration = 0.34f;
    static constexpr float _teleportInDuration  = 0.26f;
    static constexpr float _teleportCooldownBase = 3.6f;
    static constexpr float _recoveryDuration    = 0.45f;
    static constexpr int   _bossBaseExpValue    = 15;

    static Texture2D _sharedIdleAnim;
    static Texture2D _sharedWalkAnim;
    static Texture2D _sharedMeleeAnim;
    static Texture2D _sharedMagicAnim;
    static Texture2D _sharedDefendAnim;
    static Texture2D _sharedHurtAnim;
    static Texture2D _sharedDeathAnim;
    static Sound     _sharedAttackSound;
    static Sound     _sharedHurtSound;
    static Sound     _sharedDeathSound;
    static Sound     _sharedCastSound;
    static bool      _sharedResourcesLoaded;
};
