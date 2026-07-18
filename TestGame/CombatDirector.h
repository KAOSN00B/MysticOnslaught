#pragma once

#include "raylib.h"
#include "GameTypes.h"
#include "Enemy.h"
#include "Prop.h"
#include "Pickup.h"
#include "HealPickup.h"
#include "CyclopsLaserProjectile.h"
#include "EnemyProjectile.h"
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
    float* eliteHazardSpawnTimer = nullptr;

    // Player world position — used to place role-based spawns (ranged in back,
    // tanks between player and back line, assassins off-angle).
    Vector2 playerPos{};

    std::function<bool(Vector2)> isSpawnPositionValid;
    std::function<Enemy*(Vector2)> spawnBasicEnemy;
    std::function<Enemy*(Vector2)> spawnCyclops;
    std::function<Enemy*(Vector2)> spawnOgre;
    std::function<void(Vector2)> spawnMolarbeast;
    std::function<void()> spawnBossSupportAdds;
    // Role enemies the encounter director composes fights from (see EnemyRole).
    std::function<Enemy*(Vector2)> spawnSkeletonArcher;   // Ranged
    std::function<Enemy*(Vector2)> spawnFlameWisp;        // Zoner
    std::function<Enemy*(Vector2)> spawnShieldbearer;     // Tank
    std::function<Enemy*(Vector2)> spawnPhantom;          // Assassin
    std::function<Enemy*(Vector2)> spawnWarchief;         // Support
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
    float* eliteHazardSpawnTimer = nullptr;

    std::function<bool(Vector2)> isSpawnPositionValid;
    std::function<void(float, float)> triggerScreenShake;
};

// Boss impact/cast FX ids — map 1:1 to FX_Boss*.png sheets loaded by the Engine.
enum class BossFx
{
    SlimeSlam = 0, SlimeSplash, AbyssSummon, PounceImpact, CrushingSlam, BulwarkSlam,
    ToxicEruption, PoisonPool, DreamPull, DashDust, HeavyStrike, DiveImpact,
    ClawSwipe, BloodHowl, DivineSlash, SandStep, TeleportStrike, PumpkinSummon,
    ChitinBurst,
    Count
};

struct EnemyRuntimeContext
{
    Character* player = nullptr;
    NavigationGrid* nav = nullptr;
    const std::vector<Prop>* props = nullptr;
    const std::vector<Vector2>* propCenters = nullptr;
    // Player-made damage zones enemies steer around (see Enemy::SetHazardZones).
    const std::vector<HazardZone>* hazards = nullptr;
    std::vector<std::unique_ptr<Enemy>>* enemies = nullptr;
    std::vector<CyclopsLaserProjectile>* cyclopsLasers = nullptr;
    std::vector<LavaBallProjectile>* lavaBalls = nullptr;
    std::vector<EnemyProjectile>* enemyProjectiles = nullptr;   // arrows + fire bolts
    std::function<void(float, float)> triggerScreenShake;
    std::function<void(Vector2)> spawnSmallSlime;               // Abyss Slime summons
    std::function<Enemy*(Vector2)> spawnBasicEnemy;             // Pumpkin Jack summons
    std::function<void(Vector2)> spawnBossPoisonPool;           // Toxic Vermin pools
    // Play a themed owned FX_Boss*.png sprite at a world position. The int is a
    // BossFx id (see Engine); lets boss impact/cast moments show real art instead
    // of only procedural rings. Safe no-op if unset.
    std::function<void(Vector2, int)> spawnBossFx;
    // Show a floating boss-state word (ENRAGED / PHASE SHIFT / ...) at a world
    // position. Safe no-op if unset. See Enemy::ConsumeBossCallout.
    std::function<void(Vector2, const char*)> spawnBossCallout;
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
    bool awardKillExp = true;  // legacy/wave mode; dungeon mode pays once per room
    std::function<void(Vector2, bool, bool)> spawnEnemyDrop;
    std::function<void(Vector2)> spawnSmallSlime;    // big slime death split
    std::function<void(Vector2)> spawnPoisonCloud;   // sporeling death burst
    // Relic on-kill effects: pos + (wasBurning, wasFrozen, wasCharged, eliteOrBoss).
    std::function<void(Vector2, bool, bool, bool, bool)> onEnemyKilled;
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
