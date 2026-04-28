#pragma once

#include "AbilityType.h"
#include "CyclopsLaserProjectile.h"
#include "Enemy.h"
#include "LavaBallProjectile.h"
#include "Prop.h"
#include "SpreadProjectile.h"
#include "VFXManager.h"

#include <functional>
#include <memory>
#include <vector>

class Character;

struct UltimateBlast
{
    Vector2     worldPos{};
    AbilityType element  = AbilityType::FireUltimate;
    float       timer    = 0.f;
    float       lifetime = 0.f;
    float       rotation = 0.f;
};

struct SpreadProjectileUpdateContext
{
    Character* player = nullptr;
    Texture2D* map = nullptr;
    float dt = 0.f;
    float mapScale = 1.f;
    std::vector<Prop>* props = nullptr;
    std::vector<std::unique_ptr<Enemy>>* enemies = nullptr;
    VFXManager* vfx = nullptr;
    Sound* explosionSound = nullptr;
    std::function<void(float, float)> triggerScreenShake;
};

struct CyclopsLaserUpdateContext
{
    Character* player = nullptr;
    VFXManager* vfx = nullptr;
    std::function<std::vector<Vector2>(const CyclopsLaserProjectile&)> getLaserEndpoints;
    std::function<bool(Vector2, Vector2, float, const Rectangle&)> segmentHitsRect;
    std::function<void(float, float)> triggerScreenShake;
};

struct LavaBallUpdateContext
{
    Character* player = nullptr;
    Texture2D* map = nullptr;
    float mapScale = 1.f;
    std::vector<Prop>* props = nullptr;
    VFXManager* vfx = nullptr;
    Sound* impactSound = nullptr;
};

class ProjectileSystem
{
public:
    void UpdateUltimateBlasts(std::vector<UltimateBlast>& blasts, float dt) const;
    void DrawUltimateBlasts(const std::vector<UltimateBlast>& blasts, Vector2 worldOffset) const;

    void UpdateSpreadProjectiles(std::vector<SpreadProjectile>& projectiles,
        const SpreadProjectileUpdateContext& ctx) const;
    void UpdateCyclopsLasers(std::vector<CyclopsLaserProjectile>& lasers,
        const CyclopsLaserUpdateContext& ctx, float dt) const;
    void DrawCyclopsLasers(const std::vector<CyclopsLaserProjectile>& lasers,
        Vector2 worldOffset,
        const std::function<std::vector<Vector2>(const CyclopsLaserProjectile&)>& getLaserEndpoints) const;
    void UpdateLavaBallProjectiles(std::vector<LavaBallProjectile>& projectiles,
        const LavaBallUpdateContext& ctx, float dt) const;
};
