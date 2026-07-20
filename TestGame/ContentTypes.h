#pragma once

// ContentTypes -- the typed definitions for authored content (authoring engine
// design doc, "Core Content Types" and "Gameplay Asset Components").
//
// These are plain value types: explicit composition, not a general ECS. The
// serializers in ContentSerializers.cpp move them to and from the shared
// ContentDocument format, and the ContentDatabase owns the loaded instances.
//
// Project 2 (Content Core) defines the data. Runtime behavior (timers, damage,
// projectile spawning) arrives in Project 3's vertical slice.

#include <map>
#include <optional>
#include <string>
#include <vector>

// Stable identity: references between assets always use these strings, created
// once when the asset is first made. Display names are free to change;
// ContentIds are not (design doc, "Stable Identity And References").
using ContentId  = std::string;
using InstanceId = std::string;

// Current schema version written by every serializer. Loading a file with a
// LOWER version runs the migration chain; a HIGHER version is rejected as
// unsupported (made by a newer build).
constexpr int kContentSchemaVersion = 1;

// A simple 2D point. Content code deliberately avoids depending on raylib so
// the content core can compile in the standalone test binary.
struct ContentVector2
{
    float x = 0.f;
    float y = 0.f;
};

// -- Bounded enumerations ------------------------------------------------------

// The complete, bounded action list for behavior timelines. There is no
// arbitrary script execution (design doc, "Controlled Behavior Timeline").
enum class TimelineActionKind
{
    Wait,
    Telegraph,
    LockTarget,
    Move,
    FireProjectile,
    EnableDamage,
    DisableDamage,
    PlayAnimation,
    PlayVfx,
    PlaySound,
    SetColliderEnabled,
    EmitEvent,
    Repeat,
    Stop,
    Count
};

// Activation sources a trigger component may use.
enum class TriggerSourceKind
{
    AlwaysActive,
    CombatStart,
    PlayerEnter,
    SwitchActivated,
    ObjectDestroyed,
    RoomClear,
    Count
};

// How an attack or emitter chooses its direction/target.
enum class TargetingModeKind
{
    FixedDirection,
    PlayerSnapshot,
    TrackedPlayer,
    PredictedPosition,
    Radial,
    Spread,
    Sweep,
    AuthoredPath,
    Count
};

// Movement patterns a gameplay asset may follow.
enum class MovementPatternKind
{
    Static,
    PointToPoint,
    Loop,
    PingPong,
    OneShot,
    Orbit,
    AuthoredPath,
    Count
};

// Supported collider shapes (simple by design).
enum class CollisionShapeKind
{
    Rectangle,
    Circle,
    Count
};

// Name tables shared by serializers, validation, and future editor UI. Each
// returns nullptr / Count on unknown input so callers can report good errors.
const char*        TimelineActionName(TimelineActionKind action);
TimelineActionKind TimelineActionFromName(const std::string& name);
const char*        TriggerSourceName(TriggerSourceKind source);
TriggerSourceKind  TriggerSourceFromName(const std::string& name);
const char*        TargetingModeName(TargetingModeKind mode);
TargetingModeKind  TargetingModeFromName(const std::string& name);
const char*        MovementPatternName(MovementPatternKind pattern);
MovementPatternKind MovementPatternFromName(const std::string& name);
const char*        CollisionShapeName(CollisionShapeKind shape);
CollisionShapeKind CollisionShapeFromName(const std::string& name);

// -- Gameplay asset components -------------------------------------------------

struct VisualComponent
{
    std::string    spritePath;          // texture relative to the asset root
    float          scale = 1.f;
    ContentVector2 pivot{ 0.5f, 0.5f }; // normalized pivot inside the sprite
    int            drawBand = 1;        // room draw band (0 floor .. 2 overhead)
    bool           visible = true;
};

struct AnimationClip
{
    std::string name;
    int         frameCount = 1;
    float       frameTime  = 0.1f;
    bool        loops      = true;
};

struct AnimationComponent
{
    std::vector<AnimationClip> clips;
    std::string                defaultClip;
};

struct CollisionShape
{
    CollisionShapeKind shape = CollisionShapeKind::Rectangle;
    ContentVector2     offset{ 0.f, 0.f };
    float              width  = 32.f;   // rectangle size, or diameter for circles
    float              height = 32.f;
    bool               blocksPlayer      = true;
    bool               blocksEnemies     = true;
    bool               blocksProjectiles = false;
    bool               blocksDashes      = false;
};

struct CollisionComponent
{
    std::vector<CollisionShape> shapes;
};

struct DamageComponent
{
    float damageAmount   = 10.f;
    float tickInterval   = 0.f;    // 0 = damage once per contact
    float knockback      = 0.f;
    float hitCooldown    = 0.5f;
    std::string statusEffect;      // named status payload ("chill", "poison", "")
    bool  activeDuringTelegraph = false;
};

struct MovementComponent
{
    MovementPatternKind pattern = MovementPatternKind::Static;
    float               speed   = 100.f;
    float               waitAtPoints = 0.f;
    bool                restartOnRoomEntry = true;
};

struct TargetingComponent
{
    TargetingModeKind mode = TargetingModeKind::PlayerSnapshot;
    // A visible lock time so attacks cannot follow the player after their
    // warning has committed (design doc, "Targeting").
    float             lockTime = 0.35f;
};

struct EmitterComponent
{
    ContentId      projectileId;
    ContentVector2 spawnOffset{ 0.f, 0.f };
    int            count       = 1;
    float          spreadDegrees = 0.f;
    float          burstDelay  = 0.f;
    float          cooldown    = 1.f;
    int            maximumLiveChildren = 8;
};

struct TriggerComponent
{
    TriggerSourceKind source  = TriggerSourceKind::CombatStart;
    bool              oneShot = false;
    float             delay   = 0.f;
    InstanceId        watchedInstanceId;   // for SwitchActivated / ObjectDestroyed
};

struct DestructibleComponent
{
    float health        = 30.f;
    float hitCooldown   = 0.1f;
    float recoveryTime  = 0.f;    // 0 = destroyed permanently, > 0 = disabled then recovers
    bool  blocksWhenDestroyed = false;
};

struct InteractionComponent
{
    std::string prompt = "Interact";
    float       radius = 60.f;
    std::string emittedEvent;
};

struct AudioVfxComponent
{
    // Named feedback events, empty when unused. VFX are visual only; gameplay
    // geometry stays owned by collision/damage/projectile data.
    std::string telegraphVfx;
    std::string activationVfx;
    std::string impactVfx;
    std::string destroyedVfx;
    std::string telegraphSound;
    std::string activationSound;
    std::string impactSound;
    std::string destroyedSound;
};

// One bounded timeline step. Only the fields relevant to the action are used;
// validation enforces the per-action requirements.
struct TimelineStep
{
    TimelineActionKind action = TimelineActionKind::Wait;
    float       duration     = 0.f;   // Wait/Telegraph/Move/Recover-style durations
    std::string target;               // animation clip, VFX/sound name, or event name
    int         repeatTarget = -1;    // Repeat: index of the step to jump back to
    int         repeatCount  = -1;    // Repeat: bounded loop count, -1 = forever
    bool        enabled      = true;  // SetColliderEnabled payload
};

struct BehaviorTimeline
{
    std::vector<TimelineStep> steps;
};

// -- Definitions ---------------------------------------------------------------

// A reusable room object saved as .gasset (Ice Totem, Moving Blade, ...).
struct GameplayAssetDefinition
{
    ContentId                id;
    std::string              displayName;
    std::string              category;
    std::vector<std::string> tags;
    int                      schemaVersion = kContentSchemaVersion;

    VisualComponent                      visual;
    std::optional<AnimationComponent>    animation;
    std::optional<CollisionComponent>    collision;
    std::optional<DamageComponent>       damage;
    std::optional<MovementComponent>     movement;
    std::optional<TargetingComponent>    targeting;
    std::optional<EmitterComponent>      emitter;
    std::optional<TriggerComponent>      trigger;
    std::optional<DestructibleComponent> destructible;
    std::optional<InteractionComponent>  interaction;
    std::optional<AudioVfxComponent>     feedback;
    BehaviorTimeline                     timeline;
};

// A reusable projectile saved as .projectile, shared by gameplay assets,
// enemies, bosses, and abilities without duplicating travel/impact settings.
struct ProjectileDefinition
{
    ContentId   id;
    std::string displayName;
    int         schemaVersion = kContentSchemaVersion;

    // Visual strip.
    std::string spritePath;
    int         frameCount = 1;
    float       frameTime  = 0.08f;
    float       scale      = 1.f;

    // Collision.
    CollisionShapeKind shape = CollisionShapeKind::Circle;
    float              radius = 8.f;

    // Travel.
    float speed        = 300.f;
    float acceleration = 0.f;
    float maxTurnRate  = 0.f;     // degrees per second, 0 = no homing
    float lifetime     = 3.f;
    float range        = 900.f;
    int   pierceCount  = 0;
    bool  stopsOnWalls = true;

    // Targeting and payload.
    TargetingModeKind targeting = TargetingModeKind::PlayerSnapshot;
    float             damage    = 10.f;
    float             knockback = 0.f;
    std::string       statusEffect;

    // Feedback hooks (named VFX/sound events, empty when unused).
    std::string castVfx, travelVfx, impactVfx, blockedVfx;
    std::string castSound, travelSound, impactSound, blockedSound;

    // Collision layers this projectile can hit.
    bool hitsPlayer        = true;
    bool hitsEnemies       = false;
    bool hitsDestructibles = true;

    // Room pressure accounting and the definition-level hard cap.
    float pressureCost         = 1.f;
    int   maximumLiveInstances = 16;
};

// An enemy definition saved as .enemy. Base configuration plus references;
// the resolved-value layering UI arrives with the Enemy Inspector (Project 8).
struct EnemyDefinition
{
    ContentId   id;
    std::string displayName;
    int         schemaVersion = kContentSchemaVersion;

    // Editable base stats, stored as named values so the Inspector can later
    // show "base x tier + profile" breakdowns without a schema change.
    std::map<std::string, float> baseStats;

    // References to shared content this enemy uses.
    std::vector<ContentId> projectileIds;
};

// -- Prefab override support ---------------------------------------------------

// Deliberate per-instance field overrides ("field.path" -> serialized value).
// A room stores a reference to the asset plus this set, never a full copy.
struct PropertyOverrideSet
{
    std::map<std::string, std::string> values;

    bool  Has(const std::string& fieldPath) const { return values.count(fieldPath) != 0; }
    void  Set(const std::string& fieldPath, const std::string& value) { values[fieldPath] = value; }
    void  Revert(const std::string& fieldPath) { values.erase(fieldPath); }
    bool  Empty() const { return values.empty(); }
};

// A placed instance inside a room blueprint (serialized by Project 5; the type
// lives here because the dependency index must understand references).
struct GameplayAssetInstance
{
    ContentId                   assetId;
    InstanceId                  instanceId;
    ContentVector2              position{ 0.f, 0.f };
    float                       rotationDegrees = 0.f;
    bool                        mirrorX = false;
    int                         drawBand = 1;
    std::vector<ContentVector2> pathPoints;
    PropertyOverrideSet         overrides;
};
