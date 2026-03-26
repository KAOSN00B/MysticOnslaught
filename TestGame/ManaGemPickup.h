#pragma once
#include "Pickup.h"

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
