#include "ProjectileSystem.h"
#include "VirtualCanvas.h"

#include "AnimationUtils.h"
#include "VirtualCanvas.h"
#include "Character.h"
#include "VirtualCanvas.h"

#include <algorithm>
#include <cmath>

void ProjectileSystem::UpdateUltimateBlasts(std::vector<UltimateBlast>& blasts, float dt) const
{
    for (auto& blast : blasts)
        blast.timer -= dt;

    blasts.erase(
        std::remove_if(blasts.begin(), blasts.end(),
            [](const UltimateBlast& b) { return b.timer <= 0.f; }),
        blasts.end());
}

void ProjectileSystem::DrawUltimateBlasts(const std::vector<UltimateBlast>& blasts, Vector2 worldOffset) const
{
    for (const auto& blast : blasts)
    {
        if (blast.timer <= 0.f)
            continue;

        float progress = 1.f - (blast.timer / blast.lifetime);

        float pulse;
        if      (progress < 0.3f)  pulse = progress / 0.3f;
        else if (progress < 0.75f) pulse = 1.f;
        else                       pulse = 1.f - (progress - 0.75f) / 0.25f;

        if (pulse <= 0.f) continue;

        const Texture2D& tex = SpreadProjectile::GetAnimTexture(blast.element);
        const float fw = (float)SpreadProjectile::GetFrameWFor(blast.element);
        const float fh = (float)SpreadProjectile::GetFrameHFor(blast.element);
        const int frameCount = SpreadProjectile::GetFrameCountFor(blast.element);

        float elapsed = blast.lifetime - blast.timer;
        int frame = (int)(elapsed / (1.f / 16.f)) % frameCount;

        Rectangle src = GetAnimationFrameRect(tex, (int)fw, (int)fh, frame);

        const float maxSize = 200.f;
        float size = maxSize * pulse;

        Vector2 screenPos = {
            blast.worldPos.x + worldOffset.x + kVirtualWidth  * 0.5f,
            blast.worldPos.y + worldOffset.y + kVirtualHeight * 0.5f
        };

        Rectangle dest = { screenPos.x, screenPos.y, size, size };
        Vector2 origin = { size * 0.5f, size * 0.5f };
        unsigned char alpha = (unsigned char)(pulse * 255.f);

        float glowSize = size * 1.65f;
        Rectangle glowDest = { screenPos.x, screenPos.y, glowSize, glowSize };
        Vector2 glowOrigin = { glowSize * 0.5f, glowSize * 0.5f };
        DrawTexturePro(tex, src, glowDest, glowOrigin, blast.rotation,
            { 255, 255, 255, (unsigned char)(pulse * 55.f) });

        DrawTexturePro(tex, src, dest, origin, blast.rotation, { 255, 255, 255, alpha });
    }
}

void ProjectileSystem::UpdateSpreadProjectiles(std::vector<SpreadProjectile>& projectiles,
    const SpreadProjectileUpdateContext& ctx) const
{
    auto elementToCastType = [](AbilityType el) -> Character::CastType
    {
        if (el == AbilityType::IceSpread || el == AbilityType::IceBolt) return Character::CastType::IceSpread;
        if (el == AbilityType::ElectricSpread || el == AbilityType::ElectricBolt) return Character::CastType::ElectricSpread;
        return Character::CastType::FireSpread;
    };

    for (auto& projectile : projectiles)
    {
        if (!projectile.IsActive())
            continue;

        projectile.Update(ctx.dt);

        if (!projectile.IsActive())
            continue;

        {
            float mapW = ctx.map->width  * ctx.mapScale;
            float mapH = ctx.map->height * ctx.mapScale;
            Vector2 p = projectile.GetWorldPos();
            if (p.x < 76.f || p.x > mapW - 96.f || p.y < 42.f || p.y > mapH - 320.f)
            {
                ctx.vfx->SpawnHitEffect(elementToCastType(projectile.GetElement()),
                    projectile.GetWorldPos(), projectile.GetDirection());
                projectile.Destroy();
                continue;
            }
        }

        for (auto& prop : *ctx.props)
        {
            if (CheckCollisionRecs(projectile.GetCollisionRec(), prop.GetCollisionRec()))
            {
                ctx.vfx->SpawnHitEffect(elementToCastType(projectile.GetElement()),
                    projectile.GetWorldPos(), projectile.GetDirection());
                projectile.Destroy();
                break;
            }
        }

        if (!projectile.IsActive())
            continue;

        for (auto& enemy : *ctx.enemies)
        {
            if (!enemy->IsActive() || !enemy->IsAlive())
                continue;

            if (!CheckCollisionRecs(projectile.GetCollisionRec(), enemy->GetHitCollisionRec()))
                continue;

            AbilityType element = projectile.GetElement();
            bool isBolt = (element == AbilityType::FireBolt ||
                           element == AbilityType::IceBolt ||
                           element == AbilityType::ElectricBolt);

            int hitDamage;
            if (enemy->AsMolarbeast() != nullptr)
                hitDamage = isBolt ? 2 : 1;
            else
                hitDamage = isBolt ? ctx.player->GetBoltHitDamage(element) : ctx.player->GetSpreadHitDamage(element);

            enemy->TakeDamage(hitDamage, ctx.player->GetWorldPos());

            // Denied by a shield / i-frames: show the reason, consume the
            // projectile, and skip the damage number + on-hit status effects.
            Enemy::HitBlockReason blk = enemy->ConsumeHitBlock();
            if (blk != Enemy::HitBlockReason::None)
            {
                Color labelColor{ 170, 200, 255, 255 };
                const char* word = "SHIELDED";
                if (blk == Enemy::HitBlockReason::Blocked)
                    word = "BLOCKED";
                else if (blk == Enemy::HitBlockReason::Immune)
                {
                    word = "IMMUNE";
                    labelColor = Color{ 190, 190, 210, 255 };
                }
                ctx.vfx->SpawnFloatingLabel(enemy->GetWorldPos(), word, labelColor);
                ctx.vfx->SpawnImpactBurst(enemy->GetWorldPos(), labelColor, 5, 200.f);
                projectile.Destroy();
                break;
            }

            {
                Color dmgColor = (element == AbilityType::IceSpread || element == AbilityType::IceBolt) ? SKYBLUE :
                    (element == AbilityType::ElectricSpread || element == AbilityType::ElectricBolt) ? YELLOW : ORANGE;
                ctx.vfx->SpawnFloatingText(enemy->GetWorldPos(), hitDamage, dmgColor);
            }

            Character::CastType hitEffectType = Character::CastType::FireSpread;
            if (element == AbilityType::FireSpread || element == AbilityType::FireBolt)
            {
                int burnDmg = isBolt ? ctx.player->GetBoltBurnDamage(element) : ctx.player->GetSpreadBurnDamage(element);
                enemy->ApplyBurn(1.f, burnDmg, ctx.player->GetWorldPos());
                hitEffectType = Character::CastType::FireSpread;
            }
            else if (element == AbilityType::IceSpread || element == AbilityType::IceBolt)
            {
                enemy->ApplyFreeze(3.f);
                hitEffectType = Character::CastType::IceSpread;
            }
            else if (element == AbilityType::ElectricSpread || element == AbilityType::ElectricBolt)
            {
                enemy->ApplyElectricCharge();
                hitEffectType = Character::CastType::ElectricSpread;
            }

            ctx.vfx->SpawnHitEffect(hitEffectType, projectile.GetWorldPos(), projectile.GetDirection());
            projectile.Destroy();
            if (ctx.triggerScreenShake)
                ctx.triggerScreenShake(4.f, 0.05f);
            StopSound(*ctx.explosionSound);
            PlaySound(*ctx.explosionSound);
            break;
        }
    }

    projectiles.erase(
        std::remove_if(projectiles.begin(), projectiles.end(),
            [](const SpreadProjectile& p) { return !p.IsActive(); }),
        projectiles.end());
}

void ProjectileSystem::DrawCyclopsLasers(const std::vector<CyclopsLaserProjectile>& lasers,
    Vector2 worldOffset,
    const std::function<std::vector<Vector2>(const CyclopsLaserProjectile&)>& getLaserEndpoints) const
{
    for (const auto& laser : lasers)
    {
        if (!laser.IsActive())
            continue;

        std::vector<Vector2> endpoints = getLaserEndpoints(laser);
        laser.Draw(worldOffset, endpoints.data(), (int)endpoints.size());
    }
}

void ProjectileSystem::UpdateCyclopsLasers(std::vector<CyclopsLaserProjectile>& lasers,
    const CyclopsLaserUpdateContext& ctx, float dt) const
{
    for (auto& laser : lasers)
    {
        if (!laser.IsActive())
            continue;

        laser.Update(dt);
        if (!laser.IsActive())
            continue;

        if (ctx.player->IsAlive() && laser.CanHitPlayer())
        {
            std::vector<Vector2> endpoints = ctx.getLaserEndpoints(laser);
            for (const Vector2& end : endpoints)
            {
                if (!ctx.segmentHitsRect(laser.GetWorldPos(), end, laser.GetBeamWidth(), ctx.player->GetCollisionRec()))
                    continue;

                int laserDmg = laser.GetDamage();
                ctx.player->TakeDamage(laserDmg, laser.GetWorldPos());
                ctx.vfx->SpawnFloatingText(ctx.player->GetWorldPos(), -laserDmg, RED);
                laser.OnHitPlayer();
                if (ctx.triggerScreenShake)
                    ctx.triggerScreenShake(2.5f, 0.07f);
                break;
            }
        }
    }

    lasers.erase(
        std::remove_if(lasers.begin(), lasers.end(),
            [](const CyclopsLaserProjectile& l) { return !l.IsActive(); }),
        lasers.end());
}

void ProjectileSystem::UpdateLavaBallProjectiles(std::vector<LavaBallProjectile>& projectiles,
    const LavaBallUpdateContext& ctx, float dt) const
{
    const float mapW = ctx.map->width * ctx.mapScale;
    const float mapH = ctx.map->height * ctx.mapScale;
    const float marginLeft = 76.f;
    const float marginRight = 96.f;
    const float marginTop = 42.f;
    const float marginBottom = 320.f;

    for (auto& projectile : projectiles)
    {
        if (!projectile.IsActive())
            continue;

        projectile.Update(dt);
        if (!projectile.IsActive())
            continue;

        Rectangle collisionRec = projectile.GetCollisionRec();
        if (projectile.IsFlying())
        {
            bool hitArenaWall =
                collisionRec.x < marginLeft ||
                collisionRec.x + collisionRec.width > mapW - marginRight ||
                collisionRec.y < marginTop ||
                collisionRec.y + collisionRec.height > mapH - marginBottom;

            if (hitArenaWall)
            {
                projectile.BeginHit();
                StopSound(*ctx.impactSound);
                PlaySound(*ctx.impactSound);
                continue;
            }

            bool hitProp = false;
            for (const auto& prop : *ctx.props)
            {
                if (CheckCollisionRecs(collisionRec, prop.GetCollisionRec()))
                {
                    hitProp = true;
                    break;
                }
            }
            if (hitProp)
            {
                projectile.BeginHit();
                StopSound(*ctx.impactSound);
                PlaySound(*ctx.impactSound);
                continue;
            }
        }

        if (ctx.player->IsAlive() &&
            !projectile.HasHitPlayer() &&
            CheckCollisionRecs(collisionRec, ctx.player->GetCollisionRec()))
        {
            static constexpr int kLavaBallDamage = 2;
            ctx.player->TakeDamage(kLavaBallDamage, projectile.GetWorldPos());
            ctx.vfx->SpawnFloatingText(ctx.player->GetWorldPos(), -kLavaBallDamage, RED);
            projectile.OnPlayerHit();
            if (projectile.IsFlying())
            {
                projectile.BeginHit();
                StopSound(*ctx.impactSound);
                PlaySound(*ctx.impactSound);
            }
        }
    }

    projectiles.erase(
        std::remove_if(projectiles.begin(), projectiles.end(),
            [](const LavaBallProjectile& projectile) { return !projectile.IsActive(); }),
        projectiles.end());
}
