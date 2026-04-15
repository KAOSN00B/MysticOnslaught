#pragma once
#include "Pickup.h"

// ─────────────────────────────────────────────────────────────
// GoldPickup — three denominations: Single (1g), Five (5g), Ten (10g).
// Uses separate shared textures per denomination.
// ─────────────────────────────────────────────────────────────
enum class GoldDenomination { Single = 1, Five = 5, Ten = 10 };

class GoldPickup : public Pickup
{
public:
    GoldPickup() = default;
    ~GoldPickup() override = default;

    void       Init(Vector2 spawnPos, GoldDenomination denomination);
    void       Init(Vector2 spawnPos)       override { Init(spawnPos, GoldDenomination::Single); }
    void       Draw(Vector2 worldOffset)    override;
    void       OnCollect(Character& player) override;
    PickupType GetType()          const     override { return PickupType::Gold; }
    Rectangle  GetCollisionRec()  const     override;

    int GetValue() const { return (int)_denomination; }

    static void UnloadSharedResources();

private:
    static void EnsureTexturesLoaded();

    GoldDenomination _denomination = GoldDenomination::Single;

    static Texture2D _texSingle;
    static Texture2D _texFive;
    static Texture2D _texTen;
    static bool      _texturesLoaded;
};
