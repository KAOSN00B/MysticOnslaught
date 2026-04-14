#pragma once
#include "Pickup.h"

// ─────────────────────────────────────────────────────────────
// HealPickup — restores 1 HP on collect, capped at player max HP.
// Uses the world pickup sprite only; the original heal animation still plays
// on collection through Engine::SpawnHealEffect().
// Intentionally rare in the drop pool — see Engine::SpawnEnemyDrop().
// ─────────────────────────────────────────────────────────────
class HealPickup : public Pickup
{
public:
    HealPickup();
    ~HealPickup() override = default;

    void       Init(Vector2 spawnPos)       override;
    void       Draw(Vector2 worldOffset)    override;
    void       OnCollect(Character& player) override;
    PickupType GetType()          const     override { return PickupType::Heal; }
    Rectangle  GetCollisionRec()  const     override;

    static void UnloadSharedResources();

private:
    static void EnsureTextureLoaded();

    static Texture2D _sharedTexture;
    static bool _textureLoaded;
};
