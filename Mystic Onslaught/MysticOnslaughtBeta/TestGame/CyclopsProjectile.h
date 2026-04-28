#pragma once

#include "raylib.h"

// =============================================================================
// CyclopsProjectile — the laser bolt fired by the Cyclops enemy.
//
// WHAT THIS CLASS DOES:
//   Represents a single laser projectile in world space. Unlike the player's
//   projectiles (Fireball, SwordBeam, FreezeProjectile) which load sprite sheet
//   textures, this projectile is rendered entirely with Raylib primitive drawing
//   calls. Two overlapping rotated rectangles create a glowing laser look:
//       - Outer layer: wide, dark red, semi-transparent (the glow halo)
//       - Inner layer: narrow, bright orange-red, fully opaque (the beam core)
//
// HOW IT CONNECTS TO OTHER SYSTEMS:
//   - Owned by Engine in a std::vector<CyclopsProjectile> (_cyclopsProjectiles).
//   - Engine calls Init() when a Cyclops fires (via ConsumeLaserFire()).
//   - Engine calls Update() every frame to advance the projectile.
//   - Engine calls Draw() every frame via the render loop.
//   - Engine checks GetCollisionRec() against the player's collision rect.
//   - When a collision or lifetime expiry occurs, Engine calls Destroy().
//   - Inactive projectiles are erased from the vector after each update pass.
//
// POOLING / REUSE:
//   The vector is cleared each wave reset in Engine::ResetRunState(). Within
//   a wave, inactive entries are erased with remove_if. No object-pool reuse
//   is needed because fire rate is low (one shot every 4 seconds per Cyclops).
//
// DESIGN NOTE ON SPEED:
//   Speed is intentionally set lower than the player's Fireball (650 u/s) so
//   a player who sees the red charge-up effect has a fair chance to dash away.
//   480 u/s still reaches across the visible arena in ~2 seconds, keeping the
//   threat meaningful.
// =============================================================================

class CyclopsProjectile
{
public:
    CyclopsProjectile() = default;

    // -------------------------------------------------------------------------
    // Init() — arm and launch the projectile.
    //   spawnPos : world-space origin (usually the Cyclops's eye/center position)
    //   direction: unnormalized direction toward the player — normalized internally
    // -------------------------------------------------------------------------
    void Init(Vector2 spawnPos, Vector2 direction);

    // -------------------------------------------------------------------------
    // Update() — advance the projectile and count down its lifetime.
    //   Must be called once per frame while IsActive() == true.
    //   Automatically calls Destroy() when lifetime expires.
    // -------------------------------------------------------------------------
    void Update(float dt);

    // -------------------------------------------------------------------------
    // Draw() — render the laser as two layered rotated rectangles.
    //   worldOffset: Engine's current (-cameraX + shakeX, -cameraY + shakeY).
    //                All other projectile classes use the same convention.
    // -------------------------------------------------------------------------
    void Draw(Vector2 worldOffset) const;

    // -------------------------------------------------------------------------
    // Destroy() — immediately deactivate this projectile.
    //   Engine calls this on collision or manual cleanup.
    // -------------------------------------------------------------------------
    void Destroy();

    // Accessors used by Engine for collision and rendering logic
    bool      IsActive()        const { return _isActive; }
    Vector2   GetWorldPos()     const { return _worldPos; }
    Vector2   GetDirection()    const { return _direction; }

    // -------------------------------------------------------------------------
    // GetCollisionRec() — axis-aligned square hitbox centered on the projectile.
    //   Intentionally smaller than the visual beam so the player has a reaction
    //   window — rewarding good positioning and dashe timing.
    // -------------------------------------------------------------------------
    Rectangle GetCollisionRec() const;

private:
    Vector2 _worldPos{};
    Vector2 _direction{};

    // Travel speed in world units per second.
    // Slower than player projectiles (650) — gives the player a dodge window.
    float _speed = 480.f;

    // How many real-time seconds the laser stays alive before auto-deactivating.
    // At 480 u/s, 5 seconds = 2400 world units of potential travel.
    // Per spec: "disappear after 5 seconds realtime waiting to be reused."
    float _lifeTimer = 5.f;

    bool _isActive = false;

    // --- Visual dimensions (screen-space at draw scale 1:1) ------------------
    // The laser is a long, flat rectangle oriented along its direction of travel.
    static constexpr float _beamLength = 110.f;   // length along travel direction
    static constexpr float _beamWidth  = 16.f;    // thickness perpendicular to travel

    // --- Collision ----------------------------------------------------------
    // Square half-extents for the axis-aligned collision box.
    // Smaller than visual width/length so it feels fair to dodge.
    static constexpr float _collisionHalfSize = 18.f;
};
