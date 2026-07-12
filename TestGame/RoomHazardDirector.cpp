#include "RoomHazardDirector.h"
#include "GameBalance.h"
#include "AssetPaths.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

Texture2D RoomHazardDirector::_sharedLavaTex{};
Texture2D RoomHazardDirector::_sharedTotemTex{};
Texture2D RoomHazardDirector::_sharedTorchTex{};
bool      RoomHazardDirector::_sharedTexturesLoaded = false;

namespace
{
    // Sprite strip geometry (see PowerUps/Hazard_*.png, cropped from the
    // owned RA_Hell / Caverns tilesets).
    constexpr int   kLavaFrameW  = 84,  kLavaFrameH  = 96,  kLavaFrames  = 8;
    constexpr int   kTotemFrameW = 16,  kTotemFrameH = 32,  kTotemFrames = 12;
    constexpr int   kTorchFrameW = 16,  kTorchFrameH = 16,  kTorchFrames = 12;
    constexpr float kLavaFps  = 8.f;
    constexpr float kTotemFps = 10.f;
    constexpr float kTorchFps = 10.f;
    // How far the totem's aim telegraph line reaches (comfortably cross-room).
    constexpr float kAimLineLength = 1500.f;
}

void RoomHazardDirector::EnsureSharedTexturesLoaded()
{
    if (_sharedTexturesLoaded)
        return;
    _sharedLavaTex  = LoadTexture(AssetPath("PowerUps/Hazard_LavaPool.png").c_str());
    _sharedTotemTex = LoadTexture(AssetPath("PowerUps/Hazard_FireTotem.png").c_str());
    _sharedTorchTex = LoadTexture(AssetPath("PowerUps/Hazard_TorchEmitter.png").c_str());
    _sharedTexturesLoaded = true;
}

void RoomHazardDirector::UnloadSharedTextures()
{
    if (!_sharedTexturesLoaded)
        return;
    UnloadTexture(_sharedLavaTex);
    UnloadTexture(_sharedTotemTex);
    UnloadTexture(_sharedTorchTex);
    _sharedLavaTex = _sharedTotemTex = _sharedTorchTex = Texture2D{};
    _sharedTexturesLoaded = false;
}

void RoomHazardDirector::ClearRoom()
{
    _hazards.clear();
}

// Shared per-type defaults applied by both placement paths.
static void InitHazardDefaults(RoomHazard& hazard)
{
    switch (hazard.type)
    {
    case RoomHazardType::FireTotem:
        hazard.health       = Balance::Hazards::kTotemHealth;
        hazard.destructible = true;
        hazard.pressureCost = 2;
        break;
    case RoomHazardType::LavaPool:
        hazard.destructible = false;   // terrain, not a target
        hazard.pressureCost = 2;
        break;
    case RoomHazardType::FireballTorch:
        hazard.health       = Balance::Hazards::kTorchHealth;
        hazard.destructible = true;
        hazard.pressureCost = 2;
        break;
    default: break;
    }
    // First action waits out BOTH the telegraph and the room-entry grace, so a
    // hazard can never fire at a player who just walked through the door.
    hazard.actionTimer = Balance::Hazards::kFirstActionGrace;
    // Desync strips so multiple hazards don't blink in unison.
    hazard.animTime = (float)GetRandomValue(0, 100) / 20.f;
}

bool RoomHazardDirector::TryPlaceHazard(RoomHazardType type, Vector2 pos,
                                        const std::vector<Vector2>& forbiddenSpots,
                                        float minForbiddenDist, Rectangle playableBounds)
{
    // Keep hazards off the walls and out of the doorway approach lanes.
    Rectangle safeBounds{
        playableBounds.x      + Balance::Hazards::kRoomEdgeMargin,
        playableBounds.y      + Balance::Hazards::kRoomEdgeMargin,
        playableBounds.width  - Balance::Hazards::kRoomEdgeMargin * 2.f,
        playableBounds.height - Balance::Hazards::kRoomEdgeMargin * 2.f
    };
    if (!CheckCollisionPointRec(pos, safeBounds))
    {
        TraceLog(LOG_INFO, "HAZARD placement rejected: outside safe bounds (%.0f, %.0f)", pos.x, pos.y);
        return false;
    }

    for (const Vector2& spot : forbiddenSpots)
    {
        if (Vector2Distance(pos, spot) < minForbiddenDist)
        {
            TraceLog(LOG_INFO, "HAZARD placement rejected: too close to a protected spot (%.0f, %.0f)", pos.x, pos.y);
            return false;
        }
    }

    for (const RoomHazard& other : _hazards)
    {
        if (Vector2Distance(pos, other.pos) < Balance::Hazards::kMinDistBetween)
        {
            TraceLog(LOG_INFO, "HAZARD placement rejected: overlaps another hazard (%.0f, %.0f)", pos.x, pos.y);
            return false;
        }
    }

    EnsureSharedTexturesLoaded();
    RoomHazard hazard;
    hazard.type  = type;
    hazard.pos   = pos;
    hazard.state = RoomHazardState::Telegraph;
    InitHazardDefaults(hazard);
    _hazards.push_back(hazard);
    return true;
}

bool RoomHazardDirector::TryPlaceWallTorch(Vector2 pos, Vector2 fireDir,
                                           const std::vector<Vector2>& forbiddenSpots,
                                           float minForbiddenDist)
{
    for (const Vector2& spot : forbiddenSpots)
    {
        if (Vector2Distance(pos, spot) < minForbiddenDist)
        {
            TraceLog(LOG_INFO, "HAZARD torch rejected: too close to a protected spot (%.0f, %.0f)", pos.x, pos.y);
            return false;
        }
    }
    for (const RoomHazard& other : _hazards)
    {
        if (Vector2Distance(pos, other.pos) < Balance::Hazards::kMinDistBetween)
        {
            TraceLog(LOG_INFO, "HAZARD torch rejected: overlaps another hazard (%.0f, %.0f)", pos.x, pos.y);
            return false;
        }
    }

    EnsureSharedTexturesLoaded();
    RoomHazard hazard;
    hazard.type    = RoomHazardType::FireballTorch;
    hazard.pos     = pos;
    hazard.fireDir = Vector2Normalize(fireDir);
    hazard.state   = RoomHazardState::Telegraph;
    InitHazardDefaults(hazard);
    _hazards.push_back(hazard);
    return true;
}

void RoomHazardDirector::Update(const RoomHazardContext& ctx)
{
    const float dt = ctx.deltaTime;
    // Firing respects the shared environmental projectile cap; shots this
    // frame count immediately so two hazards can't both squeeze past it.
    int boltsInFlight = ctx.envProjectilesInFlight;

    for (RoomHazard& hazard : _hazards)
    {
        if (hazard.state == RoomHazardState::Destroyed)
            continue;

        hazard.stateTimer += dt;
        if (hazard.hitFlashTimer > 0.f) hazard.hitFlashTimer -= dt;
        if (hazard.hitCooldown   > 0.f) hazard.hitCooldown   -= dt;
        if (hazard.state != RoomHazardState::Disabled)
            hazard.animTime += dt;   // disabled hazards freeze visually

        if (hazard.state == RoomHazardState::Telegraph)
        {
            if (hazard.stateTimer >= Balance::Hazards::kTelegraphSeconds)
            {
                hazard.state = RoomHazardState::Active;
                hazard.stateTimer = 0.f;
            }
            continue;
        }
        if (hazard.state == RoomHazardState::Disabled)
        {
            if (hazard.stateTimer >= Balance::Hazards::kDisabledSeconds)
            {
                hazard.state = RoomHazardState::Active;
                hazard.stateTimer = 0.f;
                hazard.actionTimer = 1.f;   // re-arm beat, no instant revenge shot
            }
            continue;
        }

        // ── Active behaviour ──────────────────────────────────────────────────
        switch (hazard.type)
        {
        case RoomHazardType::FireTotem:
        {
            if (hazard.aimTimer > 0.f)
            {
                hazard.aimTimer -= dt;
                // The aim line tracks the player, then FREEZES for the last
                // kTotemAimLock seconds — that frozen beat is the dodge window.
                if (hazard.aimTimer > Balance::Hazards::kTotemAimLock)
                {
                    Vector2 toPlayer = Vector2Subtract(ctx.playerPos, hazard.pos);
                    if (Vector2Length(toPlayer) > 0.01f)
                        hazard.aimDir = Vector2Normalize(toPlayer);
                }
                if (hazard.aimTimer <= 0.f)
                {
                    if (boltsInFlight < Balance::Hazards::kEnvProjectileCap && ctx.spawnFireBolt)
                    {
                        ctx.spawnFireBolt(hazard.pos, hazard.aimDir, ctx.scaledBoltDamage);
                        boltsInFlight++;
                    }
                    hazard.actionTimer = Balance::Hazards::kTotemFireInterval;
                }
            }
            else
            {
                hazard.actionTimer -= dt;
                if (hazard.actionTimer <= 0.f)
                    hazard.aimTimer = Balance::Hazards::kTotemAimSeconds;
            }
            break;
        }
        case RoomHazardType::LavaPool:
        {
            bool playerInside = Vector2Distance(ctx.playerPos, hazard.pos) <= Balance::Hazards::kLavaRadius;
            if (playerInside)
            {
                if (hazard.graceTimer < 0.f)
                    hazard.graceTimer = Balance::Hazards::kLavaGraceSeconds;   // first touch is free
                else if (hazard.graceTimer > 0.f)
                    hazard.graceTimer = std::max(0.f, hazard.graceTimer - dt);

                if (hazard.graceTimer <= 0.f)
                {
                    hazard.tickAccum += dt;
                    if (hazard.tickAccum >= Balance::Hazards::kHazardTickInterval)
                    {
                        hazard.tickAccum -= Balance::Hazards::kHazardTickInterval;
                        if (ctx.damagePlayer)
                            ctx.damagePlayer(ctx.scaledTickDamage, hazard.pos);
                    }
                }
            }
            else
            {
                hazard.graceTimer = -1.f;
                hazard.tickAccum  = 0.f;
            }
            break;
        }
        case RoomHazardType::FireballTorch:
        {
            hazard.actionTimer -= dt;
            if (hazard.actionTimer <= 0.f)
            {
                if (boltsInFlight < Balance::Hazards::kEnvProjectileCap && ctx.spawnFireBolt)
                {
                    ctx.spawnFireBolt(hazard.pos, hazard.fireDir, ctx.scaledBoltDamage);
                    boltsInFlight++;
                }
                hazard.actionTimer = Balance::Hazards::kTorchFireInterval;
            }
            break;
        }
        default: break;
        }
    }
}

void RoomHazardDirector::Draw(Vector2 worldOffset) const
{
    if (!_sharedTexturesLoaded)
        return;

    float pulse = sinf((float)GetTime() * 9.f) * 0.5f + 0.5f;

    for (const RoomHazard& hazard : _hazards)
    {
        if (hazard.state == RoomHazardState::Destroyed)
            continue;

        const Texture2D* tex = &_sharedTotemTex;
        int frameW = kTotemFrameW, frameH = kTotemFrameH, frames = kTotemFrames;
        float fps = kTotemFps, scale = Balance::Hazards::kTotemScale;
        if (hazard.type == RoomHazardType::LavaPool)
        {
            tex = &_sharedLavaTex;
            frameW = kLavaFrameW; frameH = kLavaFrameH; frames = kLavaFrames;
            fps = kLavaFps; scale = Balance::Hazards::kLavaScale;
        }
        else if (hazard.type == RoomHazardType::FireballTorch)
        {
            tex = &_sharedTorchTex;
            frameW = kTorchFrameW; frameH = kTorchFrameH; frames = kTorchFrames;
            fps = kTorchFps; scale = Balance::Hazards::kTorchScale;
        }

        Vector2 screen = Vector2Add(hazard.pos, worldOffset);
        int frame = ((int)(hazard.animTime * fps)) % frames;

        Color tint = WHITE;
        if (hazard.state == RoomHazardState::Telegraph)      tint = Fade(WHITE, 0.55f);
        else if (hazard.state == RoomHazardState::Disabled)  tint = Fade(DARKGRAY, 0.85f);
        if (hazard.hitFlashTimer > 0.f)                      tint = Color{ 255, 130, 130, 255 };

        Rectangle src{ (float)(frame * frameW), 0.f, (float)frameW, (float)frameH };
        Rectangle dst{ screen.x, screen.y, frameW * scale, frameH * scale };
        DrawTexturePro(*tex, src, dst, Vector2{ dst.width * 0.5f, dst.height * 0.5f }, 0.f, tint);

        // Telegraph ring: simple warning geometry while the hazard arms.
        if (hazard.state == RoomHazardState::Telegraph)
        {
            float warnRadius = (hazard.type == RoomHazardType::LavaPool)
                             ? Balance::Hazards::kLavaRadius : 60.f;
            DrawCircleLines((int)screen.x, (int)screen.y, warnRadius,
                            Fade(Color{ 255, 150, 40, 255 }, 0.35f + 0.5f * pulse));
        }

        if (hazard.state != RoomHazardState::Active)
            continue;

        // Fire Totem: the aim line IS the tell. It tracks, then freezes and
        // brightens for the locked window right before the bolt.
        if (hazard.type == RoomHazardType::FireTotem && hazard.aimTimer > 0.f)
        {
            bool locked = hazard.aimTimer <= Balance::Hazards::kTotemAimLock;
            Vector2 lineEnd{ screen.x + hazard.aimDir.x * kAimLineLength,
                             screen.y + hazard.aimDir.y * kAimLineLength };
            DrawLineEx(screen, lineEnd, locked ? 5.f : 3.f,
                       Fade(RED, locked ? 0.75f : 0.30f + 0.25f * pulse));
        }

        // Fireball Torch: bright pre-fire flash on a learnable rhythm.
        if (hazard.type == RoomHazardType::FireballTorch &&
            hazard.actionTimer <= Balance::Hazards::kTorchPrefireFlash)
        {
            float flash = 1.f - hazard.actionTimer / Balance::Hazards::kTorchPrefireFlash;
            DrawCircleV(screen, 22.f + 16.f * flash,
                        Fade(Color{ 255, 200, 80, 255 }, 0.25f + 0.35f * flash));
        }
    }
}

Rectangle RoomHazardDirector::GetHazardHitBounds(const RoomHazard& hazard) const
{
    float w = kTotemFrameW * Balance::Hazards::kTotemScale;
    float h = kTotemFrameH * Balance::Hazards::kTotemScale;
    if (hazard.type == RoomHazardType::FireballTorch)
    {
        w = kTorchFrameW * Balance::Hazards::kTorchScale;
        h = kTorchFrameH * Balance::Hazards::kTorchScale;
    }
    return Rectangle{ hazard.pos.x - w * 0.5f, hazard.pos.y - h * 0.5f, w, h };
}

int RoomHazardDirector::DamageHazardsInRect(Rectangle worldRect, int damage)
{
    (void)damage;   // hazards count HITS, not scaled damage — kHealth pokes kill
    int hits = 0;
    for (RoomHazard& hazard : _hazards)
    {
        if (hazard.state == RoomHazardState::Destroyed || !hazard.destructible)
            continue;
        if (hazard.hitCooldown > 0.f)
            continue;
        if (!CheckCollisionRecs(worldRect, GetHazardHitBounds(hazard)))
            continue;

        hazard.health       -= 1.f;
        hazard.hitCooldown   = 0.25f;   // one melee swing = one chip, not one per frame
        hazard.hitFlashTimer = 0.15f;
        hazard.aimTimer      = 0.f;     // interrupts a totem mid-aim

        if (hazard.health <= 0.f)
        {
            hazard.state = RoomHazardState::Destroyed;
            TraceLog(LOG_INFO, "HAZARD destroyed at (%.0f, %.0f)", hazard.pos.x, hazard.pos.y);
        }
        else
        {
            // Every chip also knocks it out briefly — disabling a hazard is a
            // real alternative to committing to the full kill.
            hazard.state = RoomHazardState::Disabled;
            hazard.stateTimer = 0.f;
        }
        hits++;
    }
    return hits;
}

int RoomHazardDirector::TotalPressureCost() const
{
    int total = 0;
    for (const RoomHazard& hazard : _hazards)
        if (hazard.state != RoomHazardState::Destroyed)
            total += hazard.pressureCost;
    return total;
}

int RoomHazardDirector::ActiveCount() const
{
    int count = 0;
    for (const RoomHazard& hazard : _hazards)
        if (hazard.state != RoomHazardState::Destroyed)
            count++;
    return count;
}
