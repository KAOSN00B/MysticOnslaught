#include "CombatDirector.h"

#include "AbyssSlime.h"
#include "AncientBear.h"
#include "BomberImp.h"
#include "Character.h"
#include "ChompBug.h"
#include "Cyclops.h"
#include "FlameWisp.h"
#include "GoldPickup.h"
#include "Minotaur.h"
#include "Molarbeast.h"
#include "Ogre.h"
#include "Osiris.h"
#include "PumpkinJack.h"
#include "SkeletonArcher.h"
#include "SlimeEnemy.h"
#include "Sporeling.h"
#include "TitanGuard.h"
#include "ToxicVermin.h"
#include "Werewolf.h"
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
        *ctx.eliteEnrageWarningTimer = kEliteEnrageWarningDuration;
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
        {
            (*ctx.eliteMinibossPtr)->SetInvulnerable(false);
            // Payoff callout: the shield only drops once its bodyguards fall, so
            // announce it — this is what teaches the "kill the adds first" rule.
            (*ctx.eliteMinibossPtr)->RequestBossCallout("SHIELD DOWN");
        }
    }

    if (*ctx.eliteEnrageWarningTimer > 0.f)
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
    if (ctx.propCenters != nullptr && !ctx.propCenters->empty())
    {
        _propCentersScratch = *ctx.propCenters;
    }
    else if (ctx.props != nullptr && !ctx.props->empty())
    {
        _propCentersScratch.reserve(ctx.props->size());
        for (const auto& prop : *ctx.props)
            _propCentersScratch.push_back(prop.GetWorldPos());
    }
    const std::vector<Vector2>& propCenters = _propCentersScratch;

    // Spawning during iteration would invalidate the loop when ctx.enemies
    // reallocates, so boss summons are collected here and executed after.
    std::vector<Vector2> pendingSmallSlimeSpawns;
    std::vector<Vector2> pendingBasicEnemySpawns;

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

        // Boss enrage transition — any boss that just crossed into its enrage /
        // ignite / final phase gets a big screen shake so the phase change reads.
        if (enemy->IsBoss() && enemy->ConsumeEnrageShakeRequest() && ctx.triggerScreenShake)
            ctx.triggerScreenShake(12.f, 0.35f);

        // Floating state word (ENRAGED / PHASE SHIFT) announced on a transition.
        // Independent of the shake consumer above so both fire on the same frame.
        if (const char* callout = enemy->ConsumeBossCallout())
            if (ctx.spawnBossCallout)
                ctx.spawnBossCallout(enemy->GetWorldPos(), callout);

        if (Ogre* ogre = enemy->AsOgre())
        {
            if (ogre->ConsumeImpactShakeRequest())
            {
                if (ctx.triggerScreenShake) ctx.triggerScreenShake(8.f, 0.14f);
                if (ctx.spawnBossFx) ctx.spawnBossFx(ogre->GetWorldPos(), (int)BossFx::HeavyStrike);
            }
        }

        if (SkeletonArcher* archer = enemy->AsSkeletonArcher())
        {
            if (archer->WantsToFireArrow() && ctx.enemyProjectiles != nullptr)
            {
                EnemyProjectile arrow;
                arrow.Init(archer->GetWorldPos(), archer->GetArrowDirection(),
                    EnemyProjectileKind::Arrow, archer->GetAttackPower());
                ctx.enemyProjectiles->push_back(arrow);
                archer->OnArrowFired();
                archer->PlayAttackSound();
            }
        }

        if (FlameWisp* wisp = enemy->AsFlameWisp())
        {
            if (wisp->WantsToCastBolt() && ctx.enemyProjectiles != nullptr)
            {
                EnemyProjectile bolt;
                bolt.Init(wisp->GetWorldPos(), wisp->GetBoltDirection(),
                    EnemyProjectileKind::FireBolt, wisp->GetAttackPower());
                ctx.enemyProjectiles->push_back(bolt);
                wisp->OnBoltCast();
                wisp->PlayAttackSound();
            }
        }

        if (Molarbeast* molarbeast = enemy->AsMolarbeast())
        {
            if (molarbeast->WantsToFireLavaBall())
            {
                LavaBallProjectile projectile;
                Vector2 toTarget = Vector2Subtract(molarbeast->GetQueuedLavaBallTarget(), molarbeast->GetLavaBallSpawnPos());
                projectile.Init(molarbeast->GetLavaBallSpawnPos(), toTarget, "Molarbeast_Ranged_Volley");
                ctx.lavaBalls->push_back(projectile);
                molarbeast->OnLavaBallSpawned();
            }

            if (molarbeast->ConsumeImpactShakeRequest())
            {
                if (ctx.triggerScreenShake) ctx.triggerScreenShake(8.f, 0.14f);
                if (ctx.spawnBossFx) ctx.spawnBossFx(molarbeast->GetWorldPos(), (int)BossFx::HeavyStrike);
            }
        }

        if (AbyssSlime* abyssSlime = enemy->AsAbyssSlime())
        {
            // Abyss Call — small slimes burst out in a ring around the boss.
            int summonCount = abyssSlime->ConsumeSummonRequest();
            if (summonCount > 0 && ctx.spawnSmallSlime)
            {
                for (int i = 0; i < summonCount; i++)
                {
                    float angle = ((float)i / (float)summonCount) * 2.f * PI;
                    pendingSmallSlimeSpawns.push_back(Vector2{
                        abyssSlime->GetWorldPos().x + cosf(angle) * 190.f,
                        abyssSlime->GetWorldPos().y + sinf(angle) * 150.f
                    });
                }
            }

            if (abyssSlime->ConsumeImpactShakeRequest())
            {
                if (ctx.triggerScreenShake) ctx.triggerScreenShake(9.f, 0.16f);
                if (ctx.spawnBossFx) ctx.spawnBossFx(abyssSlime->GetWorldPos(), (int)BossFx::SlimeSlam);
            }
        }

        if (PumpkinJack* pumpkinJack = enemy->AsPumpkinJack())
        {
            // Harvest Volley — fan of fire bolts aimed at the player.
            if (pumpkinJack->WantsToCastVolley() && ctx.enemyProjectiles != nullptr)
            {
                int boltCount = pumpkinJack->GetVolleyBoltCount();
                Vector2 aimDir = pumpkinJack->GetVolleyDirection();
                float baseAngle = atan2f(aimDir.y, aimDir.x);
                float fanSpread = 0.28f;   // radians between adjacent bolts

                for (int i = 0; i < boltCount; i++)
                {
                    float angleOffset = ((float)i - (float)(boltCount - 1) * 0.5f) * fanSpread;
                    Vector2 boltDir{ cosf(baseAngle + angleOffset), sinf(baseAngle + angleOffset) };
                    EnemyProjectile bolt;
                    bolt.Init(pumpkinJack->GetWorldPos(), boltDir,
                        EnemyProjectileKind::FireBolt, pumpkinJack->GetAttackPower(), "PumpkinJack_Volley");
                    ctx.enemyProjectiles->push_back(bolt);
                }
                pumpkinJack->OnVolleyCast();
            }

            // Grave Call — shadow grunts crawl out near the boss.
            int summonCount = pumpkinJack->ConsumeSummonRequest();
            if (summonCount > 0 && ctx.spawnBasicEnemy)
            {
                for (int i = 0; i < summonCount; i++)
                {
                    float angle = ((float)i / (float)summonCount) * 2.f * PI + 0.6f;
                    pendingBasicEnemySpawns.push_back(Vector2{
                        pumpkinJack->GetWorldPos().x + cosf(angle) * 210.f,
                        pumpkinJack->GetWorldPos().y + sinf(angle) * 160.f
                    });
                }
            }
        }

        if (Minotaur* minotaur = enemy->AsMinotaur())
        {
            if (minotaur->ConsumeImpactShakeRequest())
            {
                if (ctx.triggerScreenShake) ctx.triggerScreenShake(10.f, 0.18f);
                if (ctx.spawnBossFx) ctx.spawnBossFx(minotaur->GetWorldPos(), (int)BossFx::CrushingSlam);
            }
        }

        if (BomberImp* bomber = enemy->AsBomberImp())
        {
            if (bomber->ConsumeImpactShakeRequest())
            {
                if (ctx.triggerScreenShake) ctx.triggerScreenShake(7.f, 0.14f);
                if (ctx.spawnBossFx) ctx.spawnBossFx(bomber->GetWorldPos(), (int)BossFx::HeavyStrike);
            }
        }

        if (Werewolf* werewolf = enemy->AsWerewolf())
        {
            if (werewolf->ConsumeImpactShakeRequest())
            {
                if (ctx.triggerScreenShake) ctx.triggerScreenShake(8.f, 0.14f);
                if (ctx.spawnBossFx) ctx.spawnBossFx(werewolf->GetWorldPos(), (int)BossFx::PounceImpact);
            }
        }

        if (ChompBug* chompBug = enemy->AsChompBug())
        {
            // Acid Barrage — fan of toxic globs.
            if (chompBug->WantsToSpit() && ctx.enemyProjectiles != nullptr)
            {
                int globCount = chompBug->GetSpitCount();
                Vector2 aimDir = chompBug->GetSpitDirection();
                float baseAngle = atan2f(aimDir.y, aimDir.x);
                for (int i = 0; i < globCount; i++)
                {
                    float angleOffset = ((float)i - (float)(globCount - 1) * 0.5f) * 0.24f;
                    EnemyProjectile glob;
                    glob.Init(chompBug->GetWorldPos(),
                        Vector2{ cosf(baseAngle + angleOffset), sinf(baseAngle + angleOffset) },
                        EnemyProjectileKind::Spit, chompBug->GetAttackPower(), "ChompBug_Acid_Spit_Fan");
                    ctx.enemyProjectiles->push_back(glob);
                }
                chompBug->OnSpitFired();
            }
        }

        if (Osiris* osiris = enemy->AsOsiris())
        {
            // Judgement Nova — a full ring of bolts.
            if (osiris->WantsToCastNova() && ctx.enemyProjectiles != nullptr)
            {
                int boltCount = osiris->GetNovaBoltCount();
                for (int i = 0; i < boltCount; i++)
                {
                    float angle = ((float)i / (float)boltCount) * 2.f * PI;
                    EnemyProjectile bolt;
                    bolt.Init(osiris->GetWorldPos(), Vector2{ cosf(angle), sinf(angle) },
                        EnemyProjectileKind::FireBolt, osiris->GetAttackPower(), "Osiris_Judgement_Nova");
                    ctx.enemyProjectiles->push_back(bolt);
                }
                osiris->OnNovaCast();
            }
            // Wrath Volley — tight aimed fan.
            if (osiris->WantsToCastVolley() && ctx.enemyProjectiles != nullptr)
            {
                int boltCount = osiris->GetVolleyBoltCount();
                Vector2 aimDir = osiris->GetVolleyDirection();
                float baseAngle = atan2f(aimDir.y, aimDir.x);
                for (int i = 0; i < boltCount; i++)
                {
                    float angleOffset = ((float)i - (float)(boltCount - 1) * 0.5f) * 0.18f;
                    EnemyProjectile bolt;
                    bolt.Init(osiris->GetWorldPos(),
                        Vector2{ cosf(baseAngle + angleOffset), sinf(baseAngle + angleOffset) },
                        EnemyProjectileKind::FireBolt, osiris->GetAttackPower(), "Osiris_Wrath_Volley");
                    ctx.enemyProjectiles->push_back(bolt);
                }
                osiris->OnVolleyCast();
            }
        }

        if (TitanGuard* titanGuard = enemy->AsTitanGuard())
        {
            // Bomb Lob — reuses the lavaball projectile like Molarbeast.
            if (titanGuard->WantsToThrowBomb() && ctx.lavaBalls != nullptr)
            {
                LavaBallProjectile bomb;
                Vector2 toTarget = Vector2Subtract(titanGuard->GetBombTarget(), titanGuard->GetBombSpawnPos());
                bomb.Init(titanGuard->GetBombSpawnPos(), toTarget, "TitanGuard_Bomb_Lob");
                ctx.lavaBalls->push_back(bomb);
                titanGuard->OnBombThrown();
            }
            if (titanGuard->ConsumeImpactShakeRequest())
            {
                if (ctx.triggerScreenShake) ctx.triggerScreenShake(11.f, 0.2f);
                if (ctx.spawnBossFx) ctx.spawnBossFx(titanGuard->GetWorldPos(), (int)BossFx::BulwarkSlam);
            }
        }

        if (ToxicVermin* vermin = enemy->AsToxicVermin())
        {
            // Toxic spit fan.
            if (vermin->WantsToSpit() && ctx.enemyProjectiles != nullptr)
            {
                int globCount = vermin->GetSpitCount();
                Vector2 aimDir = vermin->GetSpitDirection();
                float baseAngle = atan2f(aimDir.y, aimDir.x);
                for (int i = 0; i < globCount; i++)
                {
                    float angleOffset = ((float)i - (float)(globCount - 1) * 0.5f) * 0.26f;
                    EnemyProjectile glob;
                    glob.Init(vermin->GetWorldPos(),
                        Vector2{ cosf(baseAngle + angleOffset), sinf(baseAngle + angleOffset) },
                        EnemyProjectileKind::Spit, vermin->GetAttackPower(), "ToxicVermin_Toxic_Spit_Fan");
                    ctx.enemyProjectiles->push_back(glob);
                }
                vermin->OnSpitFired();
            }
            // Poison pools left by burrows and eruptions.
            Vector2 poolPos;
            if (vermin->ConsumePoisonPoolRequest(poolPos))
            {
                if (ctx.spawnBossPoisonPool) ctx.spawnBossPoisonPool(poolPos);
                if (ctx.spawnBossFx) ctx.spawnBossFx(poolPos, (int)BossFx::PoisonPool);
            }

            if (vermin->ConsumeImpactShakeRequest())
            {
                if (ctx.triggerScreenShake) ctx.triggerScreenShake(9.f, 0.16f);
                if (ctx.spawnBossFx) ctx.spawnBossFx(vermin->GetWorldPos(), (int)BossFx::ToxicEruption);
            }
        }

        if (AncientBear* bear = enemy->AsAncientBear())
        {
            if (bear->ConsumeImpactShakeRequest())
            {
                if (ctx.triggerScreenShake) ctx.triggerScreenShake(12.f, 0.22f);
                if (ctx.spawnBossFx) ctx.spawnBossFx(bear->GetWorldPos(), (int)BossFx::CrushingSlam);
            }
        }
    }

    // Safe to grow the enemy vector now that the iteration is finished.
    for (const Vector2& spawnPos : pendingSmallSlimeSpawns)
        ctx.spawnSmallSlime(spawnPos);
    for (const Vector2& spawnPos : pendingBasicEnemySpawns)
        ctx.spawnBasicEnemy(spawnPos);
}

void CombatDirector::UpdateEnemyDeaths(const EnemyDeathContext& ctx, float dt) const
{
    // Spawning during iteration would invalidate the loop when ctx.enemies
    // reallocates, so slime splits / death clouds are collected here and
    // executed after the loop.
    std::vector<Vector2> pendingSmallSlimeSpawns;
    std::vector<Vector2> pendingPoisonClouds;

    // Relic on-kill effects iterate the enemy list, so defer them until after
    // this loop finishes (same reason as the slime splits).
    struct KillEvent { Vector2 pos; bool burning, frozen, charged, eliteOrBoss; };
    std::vector<KillEvent> pendingKills;

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

            bool isBoss = enemy->IsBoss();
            bool isOgre = (dynamic_cast<Ogre*>(enemy.get()) != nullptr);

            // Splitting slime: a big slime bursts into two small ones where it
            // died. The engine provides the spawn callback so the new slimes go
            // through the normal pooled-spawn path.
            if (SlimeEnemy* slime = enemy->AsSlime())
            {
                if (slime->IsBig() && ctx.spawnSmallSlime)
                {
                    pendingSmallSlimeSpawns.push_back(Vector2{ dropPos.x - 42.f, dropPos.y + 16.f });
                    pendingSmallSlimeSpawns.push_back(Vector2{ dropPos.x + 42.f, dropPos.y + 16.f });
                }
            }

            // Sporeling: bursts into a lingering poison cloud where it fell.
            if (enemy->AsSporeling() != nullptr && ctx.spawnPoisonCloud)
                pendingPoisonClouds.push_back(dropPos);

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

            // Snapshot status for relic on-kill synergies before the body clears.
            if (ctx.onEnemyKilled)
                pendingKills.push_back(KillEvent{
                    dropPos, enemy->IsBurning(), enemy->IsFrozen(), enemy->IsCharged(),
                    isBoss || enemy->IsEliteMiniboss() });

            (*ctx.enemiesKilled)++;
            ctx.spawnEnemyDrop(dropPos, isOgre, isBoss);
            enemy->SetActive(false);
            enemy->Teleport(Vector2{ -5000.f, -5000.f });
        }
    }

    // Safe to grow / re-damage the enemy vector now that iteration is finished.
    for (const KillEvent& kill : pendingKills)
        ctx.onEnemyKilled(kill.pos, kill.burning, kill.frozen, kill.charged, kill.eliteOrBoss);
    for (const Vector2& spawnPos : pendingSmallSlimeSpawns)
        ctx.spawnSmallSlime(spawnPos);
    for (const Vector2& cloudPos : pendingPoisonClouds)
        ctx.spawnPoisonCloud(cloudPos);
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
