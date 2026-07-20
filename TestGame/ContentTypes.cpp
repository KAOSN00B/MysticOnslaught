#include "ContentTypes.h"

// Shared name tables. Serializers write these strings, parsers read them back,
// and the future editor UI shows them -- one source of truth per enum.

// A tiny helper: linear scan of a name table, returning Count on no match.
template <typename EnumType>
static EnumType EnumFromNameTable(const char* const* names, int count, const std::string& name)
{
    for (int index = 0; index < count; ++index)
        if (name == names[index])
            return (EnumType)index;
    return (EnumType)count;   // == EnumType::Count, the "unknown" sentinel
}

// -- TimelineActionKind --------------------------------------------------------

static const char* kTimelineActionNames[(int)TimelineActionKind::Count] =
{
    "Wait", "Telegraph", "LockTarget", "Move", "FireProjectile",
    "EnableDamage", "DisableDamage", "PlayAnimation", "PlayVfx", "PlaySound",
    "SetColliderEnabled", "EmitEvent", "Repeat", "Stop",
};

const char* TimelineActionName(TimelineActionKind action)
{
    int index = (int)action;
    return (index >= 0 && index < (int)TimelineActionKind::Count) ? kTimelineActionNames[index] : "Unknown";
}

TimelineActionKind TimelineActionFromName(const std::string& name)
{
    return EnumFromNameTable<TimelineActionKind>(kTimelineActionNames, (int)TimelineActionKind::Count, name);
}

// -- TriggerSourceKind ---------------------------------------------------------

static const char* kTriggerSourceNames[(int)TriggerSourceKind::Count] =
{
    "AlwaysActive", "CombatStart", "PlayerEnter", "SwitchActivated",
    "ObjectDestroyed", "RoomClear",
};

const char* TriggerSourceName(TriggerSourceKind source)
{
    int index = (int)source;
    return (index >= 0 && index < (int)TriggerSourceKind::Count) ? kTriggerSourceNames[index] : "Unknown";
}

TriggerSourceKind TriggerSourceFromName(const std::string& name)
{
    return EnumFromNameTable<TriggerSourceKind>(kTriggerSourceNames, (int)TriggerSourceKind::Count, name);
}

// -- TargetingModeKind ---------------------------------------------------------

static const char* kTargetingModeNames[(int)TargetingModeKind::Count] =
{
    "FixedDirection", "PlayerSnapshot", "TrackedPlayer", "PredictedPosition",
    "Radial", "Spread", "Sweep", "AuthoredPath",
};

const char* TargetingModeName(TargetingModeKind mode)
{
    int index = (int)mode;
    return (index >= 0 && index < (int)TargetingModeKind::Count) ? kTargetingModeNames[index] : "Unknown";
}

TargetingModeKind TargetingModeFromName(const std::string& name)
{
    return EnumFromNameTable<TargetingModeKind>(kTargetingModeNames, (int)TargetingModeKind::Count, name);
}

// -- MovementPatternKind -------------------------------------------------------

static const char* kMovementPatternNames[(int)MovementPatternKind::Count] =
{
    "Static", "PointToPoint", "Loop", "PingPong", "OneShot", "Orbit", "AuthoredPath",
};

const char* MovementPatternName(MovementPatternKind pattern)
{
    int index = (int)pattern;
    return (index >= 0 && index < (int)MovementPatternKind::Count) ? kMovementPatternNames[index] : "Unknown";
}

MovementPatternKind MovementPatternFromName(const std::string& name)
{
    return EnumFromNameTable<MovementPatternKind>(kMovementPatternNames, (int)MovementPatternKind::Count, name);
}

// -- CollisionShapeKind --------------------------------------------------------

static const char* kCollisionShapeNames[(int)CollisionShapeKind::Count] =
{
    "Rectangle", "Circle",
};

const char* CollisionShapeName(CollisionShapeKind shape)
{
    int index = (int)shape;
    return (index >= 0 && index < (int)CollisionShapeKind::Count) ? kCollisionShapeNames[index] : "Unknown";
}

CollisionShapeKind CollisionShapeFromName(const std::string& name)
{
    return EnumFromNameTable<CollisionShapeKind>(kCollisionShapeNames, (int)CollisionShapeKind::Count, name);
}
