#pragma once

#include "raylib.h"
#include "GameTypes.h"
#include "Enemy.h"
#include "Prop.h"
#include "Pickup.h"
#include "HealPickup.h"
#include "CyclopsLaserProjectile.h"
#include "LavaBallProjectile.h"
#include "NavigationGrid.h"
#include "VFXManager.h"

#include <functional>
#include <memory>
#include <vector>

class Character;

struct BossSupportState
{
    Enemy* enemy = nullptr;
    float respawnTimer = 0.f;
};

struct CombatSpawnContext
{
    Texture2D* map = nullptr;
    float mapScale = 1.f;
    RoomType currentRoomType = RoomType::Standard;
    int currentAct = 1;
    int currentRoom = 1;
    int forcedEliteMechanic = -1;
    std::vector<std::unique_ptr<Pickup>>* pickups = nullptr;

    int* eliteMechanic = nullptr;
    Enemy** eliteMinibossPtr = nullptr;
    Vector2* eliteCageCenter = nullptr;
    float* eliteCageRadius = nullptr;
    float* eliteCageDamageTimer = nullptr;
    float* eliteEnrageWarningTimer = nullptr;
    bool* eliteIsLeaping = nullptr;
    float* eliteLeapCooldown = nullptr;
    float* eliteLeapTimer = nullptr;
    float* eliteHazardSpawnTimer = nullptr;

    std::function<bool(Vector2)> isSpawnPositionValid;
    std::function<Enemy*(Vector2)> spawnBasicEnemy;
    std::function<Enemy*(Vector2)> spawnCyclops;
    std::function<Enemy*(Vector2)> spawnOgre;
    std::function<void(Vector2)> spawnMolarbeast;
    std::function<void()> spawnBossSupportAdds;
};

struct EliteMechanicsContext
{
    RoomType currentRoomType = RoomType::Standard;
    Texture2D* map = nullptr;
    float mapScale = 1.f;
    // Override for map->width/height * mapScale (used in DungeonRun where no map exists).
    float worldBoundsW = 0.f;
    float worldBoundsH = 0.f;
    Character* player = nullptr;
    std::vector<std::unique_ptr<Enemy>>* enemies = nullptr;
    std::vector<LavaBallProjectile>* lavaBalls = nullptr;

    int* eliteMechanic = nullptr;
    Enemy** eliteMinibossPtr = nullptr;
    Vector2* eliteCageCenter = nullptr;
    float* eliteCageRadius = nullptr;
    float* eliteCageDamageTimer = nullptr;
    float* eliteEnrageWarningTimer = nullptr;
    bool* eliteIsLeaping = nullptr;
    Vector2* eliteLeapStartPos = nullptr;
    Vector2* eliteLeapTarget = nullptr;
    float* eliteLeapCooldown = nullptr;
    float* eliteLeapTimer = nullptr;
    float* eliteHazardSpawnTimer = nullptr;

    std::function<bool(Vector2)> isSpawnPositionValid;
    std::function<void(float, float)> triggerScreenShake;
};

struct EnemyRuntimeContext
{
    Character* player = nullptr;
    NavigationGrid* nav = nullptr;
    const std::vector<Prop>* props = nullptr;
    const std::vector<Vector2>* propCenters = nullptr;
    std::vector<std::unique_ptr<Enemy>>* enemies = nullptr;
    std::vector<CyclopsLaserProjectile>* cyclopsLasers = nullptr;
    std::vector<LavaBallProjectile>* lavaBalls = nullptr;
    std::function<void(float, float)> triggerScreenShake;
};

struct EnemyDeathContext
{
    std::vector<std::unique_ptr<Enemy>>* enemies = nullptr;
    BossSupportState* bossCyclopsSupport = nullptr;
    BossSupportState* bossOgreSupport = nullptr;
    int wave = 0;
    int* enemiesKilled = nullptr;
    int* bossesDefeated = nullptr;
    bool* demoCompleted = nullptr;
    float* pendingExp = nullptr;
    std::function<void(Vector2, bool, bool)> spawnEnemyDrop;
};

struct BossSupportContext
{
    BossSupportState* bossCyclopsSupport = nullptr;
    BossSupportState* bossOgreSupport = nullptr;
    std::function<bool(Vector2&, float)> tryGetFarSpawnPosition;
    std::function<Enemy*(Vector2)> spawnCyclops;
    std::function<Enemy*(Vector2)> spawnOgre;
    std::function<bool()> isBossFightActive;
};

class CombatDirector
{
public:
    int GetActiveEnemyCount(const std::vector<std::unique_ptr<Enemy>>& enemies) const;
    bool IsBossFightActive(const std::vector<std::unique_ptr<Enemy>>& enemies) const;

    void SpawnEnemies(const CombatSpawnContext& ctx) const;
    void UpdateEliteMechanics(const EliteMechanicsContext& ctx, float dt) const;
    void UpdateEnemyRuntime(const EnemyRuntimeContext& ctx, float dt) const;
    void UpdateEnemyDeaths(const EnemyDeathContext& ctx, float dt) const;

    void SpawnBossSupportAdds(const BossSupportContext& ctx) const;
    void ClearBossSupportAdds(BossSupportState& cyclopsSupport, BossSupportState& ogreSupport) const;
    void UpdateBossSupportRespawns(const BossSupportContext& ctx, float dt) const;

private:
    mutable std::vector<Vector2> _propCentersScratch;
};
