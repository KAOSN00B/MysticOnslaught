#pragma once

#include "Enemy.h"
#include "raymath.h"
#include <vector>
#include <memory>

// =============================================================================
// Venomfang — a darting reptile; the POISON member of the curated elite-bruiser
// pool. The fastest bruiser: it uses the base melee AI's flanking to slip in
// off-angle, and every bite leaves lingering venom — a long, weak damage-over-
// time (several small delayed ticks) that punishes shrugging the hit off.
// Role: Assassin — quick, fragile for a bruiser, always at your back.
// =============================================================================
class Venomfang : public Enemy
{
public:
    Venomfang(Vector2 pos);
    ~Venomfang() override;

    void Init();
    void ResetForSpawn(Vector2 pos) override;
    static void UnloadSharedResources();

    void SetWaveScale(int wave) override;
    void SetVariantTier(int tier) override;
    Rectangle GetCollisionRec() const override;
    Capsule2D GetCapsule()      const override;

    Venomfang* AsVenomfang() override { return this; }
    CreatureFamily GetCreatureFamily() const override { return CreatureFamily::Beast; }

    EliteArchetype GetEliteArchetype() const override { return EliteArchetype::Venomfang; }
    EnemyRole   GetEncounterRole() const override { return EnemyRole::Assassin; }
    int         GetSpawnCost()     const override { return 3; }
    const char* GetTuningName()    const override { return "Venomfang"; }

    // Poison identity: every landed bite applies REAL stacking poison.
    void OnMeleeHitPlayer(Character* target) override;

    // ── Elite signature: The Ambush Predator ─────────────────────────────────
    // Venom Pounce: a narrow path telegraph, lock, then a darting bite at the
    // endpoint. A landed bite applies real poison and builds Predator's Mark
    // (cap 3, expires without another bite; each mark makes the next bite
    // hit harder). After a bite it disengages, leaving a short poison trail.
    // BLOOD SCENT at 50%: faster circling and a second, independently
    // telegraphed pounce. Never invisible, never untargetable.
    // Signatures run only for the elite-room miniboss.
    bool UpdateEliteSignature(float deltaTime, Vector2 navigationTarget,
        bool hasNavigationTarget, const std::vector<std::unique_ptr<Enemy>>& enemies,
        const std::vector<Vector2>& propCenters) override;
    void DrawEliteTelegraph() const override;
    void DebugForceEliteSignature() override;
    void DebugForceElitePhaseTwo() override;
    const char* GetEliteSignatureStateName() const override;

    void PlayAttackSound() override;
    void PlayDeathSound() override;

private:
    static void EnsureSharedResourcesLoaded();
    void SetIdleAnimation(bool resetFrame);
    void SetSignatureSheet(const Texture2D& sheet);
    void BeginPounceTelegraph(float telegraphSeconds);

    enum class SignatureState { None, PounceTelegraph, Pouncing, Retreating, Recovery };
    SignatureState _signatureState = SignatureState::None;
    float   _signatureTimer    = 0.f;
    float   _signatureCooldown = 0.f;
    Vector2 _pounceTarget{};        // locked at the end of the telegraph
    Vector2 _retreatDirection{ 1.f, 0.f };
    bool    _biteLanded = false;
    int     _pouncesRemaining = 0;  // BLOOD SCENT allows a second pounce
    int     _predatorMarks = 0;     // consecutive-bite escalation, cap 3
    float   _predatorMarkTimer = 0.f;
    float   _trailPatchAccumulator = 0.f;

    int _variantTier = 0;

    // Hit-and-run disengage dart after each landed bite.
    static constexpr float kDisengageSpeed    = 1400.f;
    static constexpr float kDisengageDuration = 0.35f;

    static constexpr int kVariantCount = 4;
    static Texture2D _sharedIdleAnim[kVariantCount];
    static Texture2D _sharedWalkAnim[kVariantCount];
    static Texture2D _sharedAttackAnim[kVariantCount];
    static Texture2D _sharedTakeDamageAnim[kVariantCount];
    static Texture2D _sharedDeathAnim[kVariantCount];
    static Sound     _sharedAttackSound;
    static Sound     _sharedHurtSound;
    static Sound     _sharedDeathSound;
    static bool      _sharedResourcesLoaded;
};
