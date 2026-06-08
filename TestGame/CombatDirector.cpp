#include "CombatDirector.h"

#include "Character.h"
#include "Cyclops.h"
#include "GoldPickup.h"
#include "Molarbeast.h"
#include "Ogre.h"
#include "raymath.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float kBossSupportRespawnDelay = 28.f;
    constexpr float kBossSupportMinPlayerDistance = 520.f;
    constexpr float kBossOgreInitialDelay = 22.f;

    constexpr float kEliteCageRadius = 500.f;
    constexpr float kEliteCageDamageInterval = 0.5f;
    constexpr float kEliteEnrageWarningDuration = 4.0f;
    constexpr float kLeapInterval = 8.0f;
    constexpr float kLeapDuration = 1.5f;
    constexpr float kLeapAoERadius = 90.f;
    constexpr int   kLeapAoEDamage = 3;
    constexpr float kHazardVolleyMinInterval = 1.5f;
    constexpr float kHazardVolleyMaxInterval = 2.5f;
    constexpr int   kHazardVolleyMinCount = 3;
    constexpr int   kHazardVolleyMaxCount = 6;
}

int CombatDirector::GetActiveEnemyCount(const std::vector<std::unique_ptr<Enemy>>& enemies) const
{
    int count = 0;
    for (const auto& enemy : enemies)
    {
        if (enemy->IsActive())
            count++;
    }
    return count;
}

bool CombatDirector::IsBossFightActive(const std::vector<std::unique_ptr<Enemy>>& enemies) const
{
    for (const auto& enemy : enemies)
    {
        if (!enemy->IsActive() || !enemy->IsAlive())
            continue;
        if (enemy->AsMolarbeast() != nullptr)
            return true;
    }
    return false;
}

void CombatDirector::SpawnEnemies(const CombatSpawnContext& ctx) const
{
    float mapW = ctx.map->width * ctx.mapScale;
    float mapH = ctx.map->height * ctx.mapScale;

    if (ctx.currentRoomType == RoomType::Boss)
    {
        ctx.spawnMolarbeast(Vector2{ mapW * 0.5f, mapH * 0.28f });
        ctx.spawnBossSupportAdds();
        return;
    }

    if (ctx.currentRoomType == RoomType::Rest || ctx.currentRoomType == RoomType::Store)
    {
        if (ctx.currentRoomType == RoomType::Rest)
        {
            int healCount = GetRandomValue(2, 3);
            for (int i = 0; i < healCount; i++)
            {
                Vector2 pos{ mapW * 0.5f, mapH * 0.5f };
                for (int a = 0; a < 30; a++)
                {
                    Vector2 candidate{
                        (float)GetRandomValue(300, (int)mapW - 300),
                        (float)GetRandomValue(300, (int)mapH - 300)
                    };
                    if (ctx.isSpawnPositionValid(candidate)) { pos = candidate; break; }
                }
                auto p = std::make_unique<HealPickup>();
                p->Init(pos);
                ctx.pickups->push_back(std::move(p));
            }
        }
        return;
    }

    int cornerIdx = 0;
    auto spawnPos = [&]() -> Vector2 {
        const float m = 400.f;
        const Vector2 corners[4] = {
            { m,        m        },
            { mapW - m, m        },
            { m,        mapH - m },
            { mapW - m, mapH - m },
        };
        Vector2 base = corners[cornerIdx % 4];
        cornerIdx++;

        if (ctx.isSpawnPositionValid(base)) return base;
        for (int a = 0; a < 10; a++)
        {
            Vector2 p{
                base.x + (float)GetRandomValue(-120, 120),
                base.y + (float)GetRandomValue(-120, 120)
            };
            if (ctx.isSpawnPositionValid(p)) return p;
        }
        return base;
    };

    if (ctx.currentRoomType == RoomType::Elite)
    {
        const int fodderCount = std::min(7, 3 + ctx.currentAct + std::max(0, ctx.currentRoom - 2));
        const int eliteTypeRoll = GetRandomValue(0, 2);
        Enemy* eliteMiniboss = nullptr;
        switch (eliteTypeRoll)
        {
        case 0: eliteMiniboss = ctx.spawnBasicEnemy(spawnPos()); break;
        case 1: eliteMiniboss = ctx.spawnCyclops(spawnPos()); break;
        case 2: eliteMiniboss = ctx.spawnOgre(spawnPos()); break;
        }

        if (eliteMiniboss != nullptr)
            eliteMiniboss->SetIsEliteMiniboss(true);

        *ctx.eliteMechanic = (ctx.forcedEliteMechanic >= 0) ? ctx.forcedEliteMechanic : GetRandomValue(0, 4);
        *ctx.eliteMinibossPtr = eliteMiniboss;
        *ctx.eliteCageRadius = 0.f;
        *ctx.eliteCageDamageTimer = 0.f;
        *ctx.eliteEnrageWarningTimer = 0.f;
        *ctx.eliteIsLeaping = false;
        *ctx.eliteLeapCooldown = 0.f;
        *ctx.eliteLeapTimer = 0.f;
        *ctx.eliteHazardSpawnTimer = 0.f;

        switch (*ctx.eliteMechanic)
        {
        case 0:
            *ctx.eliteCageCenter = { mapW * 0.5f, mapH * 0.5f };
            *ctx.eliteCageRadius = kEliteCageRadius;
            *ctx.eliteCageDamageTimer = kEliteCageDamageInterval;
            break;
        case 1:
            if (*ctx.eliteMinibossPtr)
                (*ctx.eliteMinibossPtr)->SetInvulnerable(true);
            break;
        case 2:
            if (*ctx.eliteMinibossPtr)
                (*ctx.eliteMinibossPtr)->ApplyEnrage();
            *ctx.eliteEnrageWarningTimer = kEliteEnrageWarningDuration;
            break;
        case 3:
            *ctx.eliteLeapCooldown = kLeapInterval;
            break;
        case 4:
            *ctx.eliteHazardSpawnTimer = (float)GetRandomValue(
                (int)(kHazardVolleyMinInterval * 100.f),
                (int)(kHazardVolleyMaxInterval * 100.f)) / 100.f;
            break;
        }

        for (int i = 0; i < fodderCount; i++)
            ctx.spawnBasicEnemy(spawnPos());
        return;
    }

    int regularCount = 0, cyclopsCount = 0, ogreCount = 0;
    if (ctx.currentAct == 1)
    {
        switch (ctx.currentRoom)
        {
        case 1: regularCount = 2; cyclopsCount = 0; ogreCount = 0; break;
        case 2: regularCount = 4; cyclopsCount = 0; ogreCount = 0; break;
        case 3: regularCount = 4; cyclopsCount = 1; ogreCount = 0; break;
        case 4: regularCount = 5; cyclopsCount = 1; ogreCount = 0; break;
        case 5: regularCount = 6; cyclopsCount = 1; ogreCount = 1; break;
        default: regularCount = 4; cyclopsCount = 0; ogreCount = 0; break;
        }
    }
    else
    {
        switch (ctx.currentRoom)
        {
        case 1: regularCount = 6; cyclopsCount = 1; ogreCount = 1; break;
        case 2: regularCount = 7; cyclopsCount = 1; ogreCount = 1; break;
        case 3: regularCount = 7; cyclopsCount = 2; ogreCount = 1; break;
        case 4: regularCount = 8; cyclopsCount = 2; ogreCount = 2; break;
        case 5: regularCount = 8; cyclopsCount = 2; ogreCount = 2; break;
        default: regularCount = 6; cyclopsCount = 1; ogreCount = 1; break;
        }
    }

    for (int i = 0; i < regularCount; i++) ctx.spawnBasicEnemy(spawnPos());
    for (int i = 0; i < cyclopsCount; i++) ctx.spawnCyclops(spawnPos());
    for (int i = 0; i < ogreCount; i++) ctx.spawnOgre(spawnPos());
}

void CombatDirector::UpdateEliteMechanics(const EliteMechanicsContext& ctx, float dt) const
{
    if (ctx.currentRoomType != RoomType::Elite)
        return;

    if (*ctx.eliteMechanic == 0 && *ctx.eliteCageRadius > 0.f)
    {
        float dist = Vector2Distance(ctx.player->GetWorldPos(), *ctx.eliteCageCenter);
        if (dist > *ctx.eliteCageRadius)
        {
            *ctx.eliteCageDamageTimer -= dt;
            if (*ctx.eliteCageDamageTimer <= 0.f)
            {
                ctx.player->TakeDamage(1, *ctx.eliteCageCenter);
                *ctx.eliteCageDamageTimer = kEliteCageDamageInterval;
            }
        }
        else
        {
            *ctx.eliteCageDamageTimer = kEliteCageDamageInterval;
        }
    }

    if (*ctx.eliteMechanic == 1
        && *ctx.eliteMinibossPtr && (*ctx.eliteMinibossPtr)->IsInvulnerable()
        && (*ctx.eliteMinibossPtr)->IsActive() && !(*ctx.eliteMinibossPtr)->IsDying())
    {
        bool anyGruntAlive = false;
        for (const auto& e : *ctx.enemies)
        {
            if (e.get() == *ctx.eliteMinibossPtr) continue;
            if (e->IsActive() && e->IsAlive() && !e->IsDying())
            { anyGruntAlive = true; break; }
        }
        if (!anyGruntAlive)
            (*ctx.eliteMinibossPtr)->SetInvulnerable(false);
    }

    if (*ctx.eliteMechanic == 2 && *ctx.eliteEnrageWarningTimer > 0.f)
        *ctx.eliteEnrageWarningTimer -= dt;

    if (*ctx.eliteMechanic == 3
        && *ctx.eliteMinibossPtr && (*ctx.eliteMinibossPtr)->IsActive()
        && (*ctx.eliteMinibossPtr)->IsAlive() && !(*ctx.eliteMinibossPtr)->IsDying())
    {
        if (!*ctx.eliteIsLeaping)
        {
            *ctx.eliteLeapCooldown -= dt;
            if (*ctx.eliteLeapCooldown <= 0.f)
            {
                *ctx.eliteLeapStartPos = (*ctx.eliteMinibossPtr)->GetWorldPos();
                *ctx.eliteLeapTarget = ctx.player->GetFeetWorldPos();
                *ctx.eliteIsLeaping = true;
                *ctx.eliteLeapTimer = kLeapDuration;
                (*ctx.eliteMinibossPtr)->SetLeapFrozen(true);
            }
        }
        else
        {
            (*ctx.eliteMinibossPtr)->Teleport(*ctx.eliteLeapStartPos);
            *ctx.eliteLeapTimer -= dt;

            if (*ctx.eliteLeapTimer <= 0.f)
            {
                (*ctx.eliteMinibossPtr)->Teleport(*ctx.eliteLeapTarget);
                (*ctx.eliteMinibossPtr)->SetLeapFrozen(false);
                *ctx.eliteIsLeaping = false;
                *ctx.eliteLeapCooldown = kLeapInterval;

                float dist = Vector2Distance(ctx.player->GetWorldPos(), *ctx.eliteLeapTarget);
                if (dist <= kLeapAoERadius)
                    ctx.player->TakeDamage(kLeapAoEDamage, *ctx.eliteLeapTarget);

                if (ctx.triggerScreenShake)
                    ctx.triggerScreenShake(8.f, 0.25f);
            }
        }
    }

    if (*ctx.eliteMechanic == 4)
    {
        *ctx.eliteHazardSpawnTimer -= dt;
        if (*ctx.eliteHazardSpawnTimer <= 0.f)
        {
            const float mapW = (ctx.worldBoundsW > 0.f) ? ctx.worldBoundsW : ctx.map->width  * ctx.mapScale;
            const float mapH = (ctx.worldBoundsH > 0.f) ? ctx.worldBoundsH : ctx.map->height * ctx.mapScale;
            const float marginLeft = 120.f;
            const float marginRight = 120.f;
            const float marginTop = 90.f;
            const float marginBottom = 220.f;
            const Vector2 playerPos = ctx.player->GetWorldPos();
            const int volleyCount = GetRandomValue(kHazardVolleyMinCount, kHazardVolleyMaxCount);

            for (int i = 0; i < volleyCount; ++i)
            {
                Vector2 spawnPos{};
                bool foundSpawn = false;

                for (int attempt = 0; attempt < 24; ++attempt)
                {
                    spawnPos = {
                        (float)GetRandomValue((int)marginLeft, (int)(mapW - marginRight)),
                        (float)GetRandomValue((int)marginTop,  (int)(mapH - marginBottom))
                    };

                    if (Vector2Distance(spawnPos, playerPos) < 240.f)
                        continue;
                    if (!ctx.isSpawnPositionValid(spawnPos))
                        continue;

                    foundSpawn = true;
                    break;
                }

                if (!foundSpawn)
                    continue;

                Vector2 toPlayer = Vector2Subtract(playerPos, spawnPos);
                float baseAngle = atan2f(toPlayer.y, toPlayer.x);
                float spread = ((float)GetRandomValue(-28, 28)) * DEG2RAD;
                float angle = baseAngle + spread;

                LavaBallProjectile projectile;
                projectile.Init(spawnPos, Vector2{ cosf(angle), sinf(angle) });
                ctx.lavaBalls->push_back(projectile);
            }

            *ctx.eliteHazardSpawnTimer = (float)GetRandomValue(
                (int)(kHazardVolleyMinInterval * 100.f),
                (int)(kHazardVolleyMaxInterval * 100.f)) / 100.f;
            if (ctx.triggerScreenShake)
                ctx.triggerScreenShake(2.f, 0.06f);
        }
    }
}

void CombatDirector::UpdateEnemyRuntime(const EnemyRuntimeContext& ctx, float dt) const
{
    Vector2 playerFeet = ctx.player->GetFeetWorldPos();

    _propCentersScratch.clear();
    if (ctx.props != nullptr && !ctx.props->empty())
    {
        _propCentersScratch.reserve(ctx.props->size());
        for (const auto& prop : *ctx.props)
            _propCentersScratch.push_back(prop.GetWorldPos());
    }
    const std::vector<Vector2>& propCenters = _propCentersScratch;

    for (auto& enemy : *ctx.enemies)
    {
        if (!enemy->IsActive())
            continue;

        Vector2 navigationTarget = playerFeet;
        bool hasNavigationTarget = false;

        if (!enemy->UsesDirectPursuit() &&
            !ctx.nav->HasLineOfSight(enemy->GetWorldPos(), playerFeet))
        {
            navigationTarget = ctx.nav->GetTarget(enemy->GetWorldPos(), playerFeet);
            hasNavigationTarget = !Vector2Equals(navigationTarget, playerFeet);
        }

        enemy->Update(dt, playerFeet, navigationTarget, hasNavigationTarget, *ctx.enemies, propCenters);

        if (Cyclops* cyclops = enemy->AsCyclops())
        {
            if (cyclops->WantsToFire())
            {
                CyclopsLaserProjectile laser;
                if (cyclops->GetFireMode() == Cyclops::FireMode::Scatter)
                    laser.InitScatter(cyclops->GetWorldPos(), cyclops->GetFireDirection(), cyclops->GetAttackPower());
                else
                    laser.InitSweep(cyclops->GetWorldPos(), cyclops->GetFireDirection(), cyclops->GetAttackPower());
                ctx.cyclopsLasers->push_back(laser);
                cyclops->OnFired();
                cyclops->PlayAttackSound();
            }
        }

        if (Ogre* ogre = enemy->AsOgre())
        {
            if (ogre->ConsumeImpactShakeRequest() && ctx.triggerScreenShake)
                ctx.triggerScreenShake(8.f, 0.14f);
        }

        if (Molarbeast* molarbeast = enemy->AsMolarbeast())
        {
            if (molarbeast->WantsToFireLavaBall())
            {
                LavaBallProjectile projectile;
                Vector2 toTarget = Vector2Subtract(molarbeast->GetQueuedLavaBallTarget(), molarbeast->GetLavaBallSpawnPos());
                projectile.Init(molarbeast->GetLavaBallSpawnPos(), toTarget);
                ctx.lavaBalls->push_back(projectile);
                molarbeast->OnLavaBallSpawned();
            }

            if (molarbeast->ConsumeImpactShakeRequest() && ctx.triggerScreenShake)
                ctx.triggerScreenShake(8.f, 0.14f);
        }
    }
}

void CombatDirector::UpdateEnemyDeaths(const EnemyDeathContext& ctx, float dt) const
{
    for (auto& enemy : *ctx.enemies)
    {
        if (!enemy->IsActive())
            continue;

        Vector2 dropPos = enemy->GetWorldPos();
        if (enemy->UpdateDeath(dt))
        {
            if (enemy.get() == ctx.bossCyclopsSupport->enemy && IsBossFightActive(*ctx.enemies))
                ctx.bossCyclopsSupport->respawnTimer = kBossSupportRespawnDelay;
            if (enemy.get() == ctx.bossOgreSupport->enemy && IsBossFightActive(*ctx.enemies))
                ctx.bossOgreSupport->respawnTimer = kBossSupportRespawnDelay;

            bool isBoss = (dynamic_cast<Molarbeast*>(enemy.get()) != nullptr);
            bool isOgre = (dynamic_cast<Ogre*>(enemy.get()) != nullptr);

            if (isBoss)
            {
                *ctx.pendingExp += 10.f * ctx.wave;
                (*ctx.bossesDefeated)++;
                if (*ctx.bossesDefeated >= 2)
                    *ctx.demoCompleted = true;
            }
            else if (!IsBossFightActive(*ctx.enemies))
            {
                *ctx.pendingExp += (float)enemy->GetExpValue();
            }

            (*ctx.enemiesKilled)++;
            ctx.spawnEnemyDrop(dropPos, isOgre, isBoss);
            enemy->SetActive(false);
            enemy->Teleport(Vector2{ -5000.f, -5000.f });
        }
    }
}

void CombatDirector::SpawnBossSupportAdds(const BossSupportContext& ctx) const
{
    Vector2 cyclopsPos{};
    if (ctx.tryGetFarSpawnPosition(cyclopsPos, kBossSupportMinPlayerDistance))
        ctx.bossCyclopsSupport->enemy = ctx.spawnCyclops(cyclopsPos);
    ctx.bossCyclopsSupport->respawnTimer = 0.f;

    ctx.bossOgreSupport->enemy = nullptr;
    ctx.bossOgreSupport->respawnTimer = kBossOgreInitialDelay;
}

void CombatDirector::ClearBossSupportAdds(BossSupportState& cyclopsSupport, BossSupportState& ogreSupport) const
{
    if (cyclopsSupport.enemy != nullptr)
    {
        cyclopsSupport.enemy->SetActive(false);
        cyclopsSupport.enemy->Teleport(Vector2{ -5000.f, -5000.f });
    }

    if (ogreSupport.enemy != nullptr)
    {
        ogreSupport.enemy->SetActive(false);
        ogreSupport.enemy->Teleport(Vector2{ -5000.f, -5000.f });
    }

    cyclopsSupport = {};
    ogreSupport = {};
}

void CombatDirector::UpdateBossSupportRespawns(const BossSupportContext& ctx, float dt) const
{
    if (!ctx.isBossFightActive())
    {
        ClearBossSupportAdds(*ctx.bossCyclopsSupport, *ctx.bossOgreSupport);
        return;
    }

    if (ctx.bossCyclopsSupport->enemy != nullptr &&
        !ctx.bossCyclopsSupport->enemy->IsActive() &&
        ctx.bossCyclopsSupport->respawnTimer > 0.f)
    {
        ctx.bossCyclopsSupport->respawnTimer -= dt;
        if (ctx.bossCyclopsSupport->respawnTimer <= 0.f)
        {
            Vector2 spawnPos{};
            if (ctx.tryGetFarSpawnPosition(spawnPos, kBossSupportMinPlayerDistance))
                ctx.bossCyclopsSupport->enemy = ctx.spawnCyclops(spawnPos);
            ctx.bossCyclopsSupport->respawnTimer = 0.f;
        }
    }

    bool ogrePendingSpawn = (ctx.bossOgreSupport->enemy == nullptr && ctx.bossOgreSupport->respawnTimer > 0.f);
    bool ogrePendingRespawn = (ctx.bossOgreSupport->enemy != nullptr &&
                               !ctx.bossOgreSupport->enemy->IsActive() &&
                               ctx.bossOgreSupport->respawnTimer > 0.f);
    if (ogrePendingSpawn || ogrePendingRespawn)
    {
        ctx.bossOgreSupport->respawnTimer -= dt;
        if (ctx.bossOgreSupport->respawnTimer <= 0.f)
        {
            Vector2 spawnPos{};
            if (ctx.tryGetFarSpawnPosition(spawnPos, kBossSupportMinPlayerDistance))
                ctx.bossOgreSupport->enemy = ctx.spawnOgre(spawnPos);
            ctx.bossOgreSupport->respawnTimer = 0.f;
        }
    }
}
