#pragma once
#include "Pickup.h"

// ─────────────────────────────────────────────────────────────────────────────
// ManaGemPickup — LEGACY / NOT CURRENTLY SPAWNED
//
// Mana is now restored by passive regeneration (Character::kManaRegenPerSecond).
// This class is kept on disk and compiles cleanly, but it is not referenced by
// SpawnEnemyDrop or SpawnTimedPickup.
//
// Store hook: if a future store sells mana refills, this class (or a variant)
// can be re-activated as the delivery mechanism for that purchase.
// ─────────────────────────────────────────────────────────────────────────────
class ManaGemPickup : public Pickup
{
public:
    ManaGemPickup();
    ~ManaGemPickup() override = default;

    void       Init(Vector2 spawnPos)       override;
    void       Draw(Vector2 worldOffset)    override;
    void       OnCollect(Character& player) override;
    PickupType GetType()          const     override { return PickupType::Mana; }
    Rectangle  GetCollisionRec()  const     override;

    static void UnloadSharedResources();

private:
    static void EnsureTextureLoaded();

    float _bob = 0.f;   // simple vertical bob offset

    static Texture2D _sharedTexture;
    static bool      _textureLoaded;
};
