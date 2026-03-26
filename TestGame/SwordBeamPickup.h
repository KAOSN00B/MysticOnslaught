#pragma once
// [SHELVED] — SwordBeamPickup is no longer spawned by Engine.
// See FireBallPickup.h for full explanation. Safe to remove from the project.
#include "Pickup.h"

class SwordBeamPickup : public Pickup
{
public:
    SwordBeamPickup();
    ~SwordBeamPickup() override = default;

    // Satisfies the base class pure virtual (defaults ammo to 1)
    void Init(Vector2 spawnPos)              override;
    // Non-virtual overload for when you need a custom ammo value
    void Init(Vector2 spawnPos, int ammoValue);

    void       Draw(Vector2 worldOffset)    override;
    void       OnCollect(Character& player) override;
    PickupType GetType()          const     override { return PickupType::SwordBeam; }
    Rectangle  GetCollisionRec()  const     override;

    int GetAmmoValue() const;
    static Texture2D GetSharedTexture();
    static void UnloadSharedResources();

private:
    static void EnsureTextureLoaded();

    float _radius    = 22.f;
    int   _ammoValue = 1;

    static Texture2D _sharedTexture;
    static bool _textureLoaded;
};
