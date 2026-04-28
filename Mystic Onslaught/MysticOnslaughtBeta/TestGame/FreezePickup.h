#pragma once
// [SHELVED] — FreezePickup is no longer spawned by Engine.
// See FireBallPickup.h for full explanation. Safe to remove from the project.
#include "Pickup.h"

// ─────────────────────────────────────────────────────────────
// FreezePickup — freezes all enemies for 3-5 seconds on collect.
// Drawn as a blue snowflake circle (no texture needed).
// The freeze effect itself is applied by Engine after OnCollect,
// since Engine owns the enemy list.
// ─────────────────────────────────────────────────────────────
class FreezePickup : public Pickup
{
public:
    FreezePickup();
    ~FreezePickup() override = default;

    void       Init(Vector2 spawnPos)       override;
    void       Draw(Vector2 worldOffset)    override;
    void       OnCollect(Character& player) override;
    PickupType GetType()          const     override { return PickupType::Freeze; }
    Rectangle  GetCollisionRec()  const     override;

    static Texture2D GetSharedTexture();
    static void UnloadSharedResources();

private:
    static void EnsureTextureLoaded();

    static Texture2D _sharedTexture;
    static bool _textureLoaded;
};
