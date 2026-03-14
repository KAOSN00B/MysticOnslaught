#pragma once
#include "Pickup.h"

// ─────────────────────────────────────────────────────────────
// HealPickup — restores 1 HP on collect, capped at player max HP.
// Drawn as a red cross orb (no texture needed).
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

    float _runningTime = 0.f;
    float _updateTime = 1.f / 12.f;
    int _frame = 0;
    static constexpr int _frameWidth = 32;
    static constexpr int _frameHeight = 32;
    static constexpr int _frameCount = 13;

    static Texture2D _sharedTexture;
    static bool _textureLoaded;
};
