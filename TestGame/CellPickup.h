#pragma once
#include "Pickup.h"

// ─────────────────────────────────────────────────────────────
// CellPickup — Mystic Cells, the meta-progression currency.
// Three denominations: Single (1), Five (5), Ten (10).
// Carried cells are banked with Zeph at each zone's store room;
// dying loses everything still being carried (Dead Cells rules).
// ─────────────────────────────────────────────────────────────
enum class CellDenomination { Single = 1, Five = 5, Ten = 10 };

class CellPickup : public Pickup
{
public:
    CellPickup() = default;
    ~CellPickup() override = default;

    void       Init(Vector2 spawnPos, CellDenomination denomination);
    void       Init(Vector2 spawnPos)       override { Init(spawnPos, CellDenomination::Single); }
    void       Draw(Vector2 worldOffset)    override;
    void       OnCollect(Character& player) override;
    PickupType GetType()          const     override { return PickupType::Cell; }
    Rectangle  GetCollisionRec()  const     override;

    int GetValue() const { return (int)_denomination; }

    // Shared cell texture is also used by the HUD counter and the Poe's Altar.
    static const Texture2D& GetMediumTexture();

    static void UnloadSharedResources();

private:
    static void EnsureTexturesLoaded();

    CellDenomination _denomination = CellDenomination::Single;
    float            _bobTimer     = 0.f;   // gentle floating motion

    static Texture2D _texSingle;
    static Texture2D _texFive;
    static Texture2D _texTen;
    static bool      _texturesLoaded;
};
