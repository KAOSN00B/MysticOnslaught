#pragma once
// [SHELVED] — FireBallPickup is no longer spawned by Engine.
// The old ammo-pickup system has been replaced by the mana economy.
// Engine no longer includes this header. This file is kept so the project
// compiles without .vcxproj changes. Safe to remove from the project entirely.
#include "Pickup.h"

class FireBallPickup : public Pickup
{
public:
    FireBallPickup();
    ~FireBallPickup() override = default;

    // Satisfies the base class pure virtual (defaults ammo to 1)
    void Init(Vector2 spawnPos)              override;
    // Non-virtual overload for when you need a custom ammo value
    void Init(Vector2 spawnPos, int ammoValue);

    void       Draw(Vector2 worldOffset)    override;
    void       OnCollect(Character& player) override;
    PickupType GetType()          const     override { return PickupType::FireBall; }
    Rectangle  GetCollisionRec()  const     override;

    int GetAmmoValue() const;

    // Static shared texture — used by Engine HUD as well
    static Texture2D GetSharedTexture();
    static void      UnloadSharedResources();

private:
    static void EnsureTextureLoaded();

    float _scale     = 4.f;
    int   _ammoValue = 1;

    static Texture2D _sharedTexture;
    static bool      _textureLoaded;
};
