#pragma once

#include "Enemy.h"
#include "raymath.h"
#include <vector>
#include <memory>

// =============================================================================
// Bonechill — a skeletal knight; the ICE member of the curated elite-bruiser
// pool. Shares the base melee AI, but its blade carries a grave chill: every
// landed hit slows the player's movement for a couple of seconds
// (Character::ApplyChill), and the player tints icy blue while slowed.
// Role: Tank — a slow, implacable wall that is dangerous to trade with.
// =============================================================================
class Bonechill : public Enemy
{
public:
    Bonechill(Vector2 pos);
    ~Bonechill() override;

    void Init();
    void ResetForSpawn(Vector2 pos) override;
    static void UnloadSharedResources();

    void SetWaveScale(int wave) override;
    void SetVariantTier(int tier) override;
    Rectangle GetCollisionRec() const override;
    Capsule2D GetCapsule()      const override;

    Bonechill* AsBonechill() override { return this; }
    CreatureFamily GetCreatureFamily() const override { return CreatureFamily::Metal; }

    EliteArchetype GetEliteArchetype() const override { return EliteArchetype::Bonechill; }
    EnemyRole   GetEncounterRole() const override { return EnemyRole::Tank; }
    int         GetSpawnCost()     const override { return 4; }
    const char* GetTuningName()    const override { return "Bonechill"; }

    // Ice identity: every landed melee hit chills (slows) the player.
    void OnMeleeHitPlayer(Character* target) override;

    // Behavior identity: an IMMOVABLE wall — player hits never shove it. The
    // fight is about kiting a relentless advance, not pushing it around.
    void ApplyHitKnockback(Vector2 /*dir*/, float /*speed*/) override {}

    // ── Elite signature: The Frozen Wall ─────────────────────────────────────
    // Frontal frost armour (45% reduction while advancing — NEVER zero, never
    // "IMMUNE"; rear hits and hits during the slam windup/recovery are full),
    // Permafrost Slam (committed cone + three ice lanes with gaps), and
    // ARMOUR SHATTERED at 50% (armour gone, +25% speed, shorter cooldown).
    // Signatures run only for the elite-room miniboss.
    void TakeDamage(int damage, Vector2 attackerPos) override;
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
    bool IsFrostArmourActive() const;

    enum class SignatureState { None, SlamTelegraph, SlamRecovery };
    SignatureState _signatureState = SignatureState::None;
    float   _signatureTimer    = 0.f;
    float   _signatureCooldown = 0.f;
    float   _signatureCooldownDuration = 5.5f;   // shortened by ARMOUR SHATTERED
    Vector2 _signatureDirection{ 1.f, 0.f };
    bool    _frostArmourBroken = false;

    int _variantTier = 0;

    // Chill applied per landed hit (duration seconds, movement multiplier).
    static constexpr float kChillDuration  = 2.2f;
    static constexpr float kChillSpeedMult = 0.55f;

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
