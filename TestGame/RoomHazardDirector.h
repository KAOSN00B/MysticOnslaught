#pragma once

#include "raylib.h"
#include <functional>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// RoomHazardDirector — owns the environmental hazards of the CURRENT dungeon
// room (fire totems, lava pools, fireball torches...). One shared home for
// hazard state, telegraph/active/disabled timing, placement validation, and
// pressure cost, so new hazards stop accreting as unrelated blocks in Engine.cpp.
//
// Sprites are cropped from Robert's owned tilesets into PowerUps/Hazard_*.png
// (RA_Hell lava pool, Caverns standing torch + glow orb). Projectiles are the
// Engine's EnemyProjectile FireBolts, requested through the context callbacks —
// the director never owns projectile collision. Tuning: Balance::Hazards.
// ─────────────────────────────────────────────────────────────────────────────

enum class RoomHazardType
{
    FireTotem,      // destructible turret: telegraphs an aim line, fires a bolt
    LavaPool,       // persistent floor zone: modest tick damage, movement denial
    FireballTorch,  // wall-anchored lane launcher with a pre-fire flash
    Count
};

enum class RoomHazardState
{
    Telegraph,   // warning geometry only — cannot hurt the player yet
    Active,      // fully armed
    Disabled,    // player knocked it out; recovers after a downtime
    Destroyed    // permanently dead for this room
};

struct RoomHazard
{
    RoomHazardType  type;
    RoomHazardState state = RoomHazardState::Telegraph;
    Vector2 pos{};
    float   stateTimer   = 0.f;    // seconds spent in the current state
    float   actionTimer  = 0.f;    // per-type cadence (firing rhythm etc.)
    float   animTime     = 0.f;    // sprite strip playback clock
    float   health       = 3.f;    // hit points while destructible
    bool    destructible = true;
    int     pressureCost = 2;      // counts against the room pressure budget
    // Fire Totem aiming: >0 while the telegraph line is up; the aim direction
    // tracks the player until kTotemAimLock remains, then freezes (dodgeable).
    float   aimTimer = 0.f;
    Vector2 aimDir{ 1.f, 0.f };
    // Fireball Torch lane direction (fixed at placement: away from its wall).
    Vector2 fireDir{ 1.f, 0.f };
    // Lava Pool: grace countdown started when the player first steps in, and
    // the tick accumulator while they stay.
    float   graceTimer = -1.f;     // < 0 = player not inside
    float   tickAccum  = 0.f;
    float   hitFlashTimer = 0.f;   // brief flash when the player damages it
    float   hitCooldown   = 0.f;   // stops one melee swing landing every frame
};

// Per-frame runtime handles the director needs from the Engine. The director
// requests bolts/damage through callbacks so projectile ownership + player
// damage rules stay centralized in the Engine.
struct RoomHazardContext
{
    Vector2 playerPos{};
    float   deltaTime = 0.f;
    // Environmental FireBolts currently in flight (wisps + hazards) — firing
    // is skipped while this is at Balance::Hazards::kEnvProjectileCap.
    int     envProjectilesInFlight = 0;
    // Hazard damage pre-scaled by the Engine to the run's enemy power level,
    // so hazards keep pace with the rest of the game instead of staying at 1.
    int     scaledBoltDamage = 1;   // totem + torch shots
    int     scaledTickDamage = 1;   // lava tick
    std::function<void(Vector2 pos, Vector2 dir, int damage)> spawnFireBolt;
    std::function<void(int damage, Vector2 fromPos)>          damagePlayer;
};

class RoomHazardDirector
{
public:
    // Wipe all hazards — call on every room transition so nothing leaks
    // between rooms (or into shops/rest rooms, which never plan hazards).
    void ClearRoom();

    // Validated placement: rejects spots outside the playable margin or too
    // close to any forbidden spot (door centres, player entry, chest, NPC,
    // other hazards). Returns true and stores the hazard when the spot is
    // safe; logs the rejection reason otherwise.
    bool TryPlaceHazard(RoomHazardType type, Vector2 pos,
                        const std::vector<Vector2>& forbiddenSpots,
                        float minForbiddenDist, Rectangle playableBounds);

    // Wall torches sit inside the edge margin on purpose, so they get their
    // own placement that only checks the forbidden spots + hazard spacing.
    bool TryPlaceWallTorch(Vector2 pos, Vector2 fireDir,
                           const std::vector<Vector2>& forbiddenSpots,
                           float minForbiddenDist);

    // Rebuild a persisted hazard on room re-entry — no placement validation
    // (the spot was validated on first placement) and no telegraph replay,
    // but the room-entry action grace still applies.
    void RestoreHazard(RoomHazardType type, Vector2 pos, Vector2 fireDir, float health);

    void Update(const RoomHazardContext& ctx);
    // worldOffset = screen-shake offset (dungeon world == screen otherwise).
    void Draw(Vector2 worldOffset) const;

    // Player damage entry point — called from melee swings and ability rects.
    // Each hit chips 1 health: hazards flip to Disabled while hurt and to
    // Destroyed at 0. Returns how many hazards were struck.
    int DamageHazardsInRect(Rectangle worldRect, int damage);

    int  TotalPressureCost() const;   // live hazards' cost (budget accounting)
    int  ActiveCount() const;         // hazards not yet destroyed

    std::vector<RoomHazard>&       Hazards()       { return _hazards; }
    const std::vector<RoomHazard>& Hazards() const { return _hazards; }

    static void UnloadSharedTextures();   // Engine shutdown

private:
    static void EnsureSharedTexturesLoaded();
    Rectangle GetHazardHitBounds(const RoomHazard& hazard) const;

    std::vector<RoomHazard> _hazards;

    static Texture2D _sharedLavaTex;    // 8 frames, 84x96
    static Texture2D _sharedTotemTex;   // 12 frames, 16x32
    static Texture2D _sharedTorchTex;   // 12 frames, 16x16
    static bool      _sharedTexturesLoaded;
};
