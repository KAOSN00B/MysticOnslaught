#include "ContentValidation.h"

#include <cmath>
#include <sstream>

// -- Report helpers ------------------------------------------------------------

bool ContentValidationReport::HasErrors() const
{
    for (const ContentIssue& issue : issues)
        if (issue.severity == ContentIssueSeverity::Error)
            return true;
    return false;
}

int ContentValidationReport::ErrorCount() const
{
    int count = 0;
    for (const ContentIssue& issue : issues)
        if (issue.severity == ContentIssueSeverity::Error)
            ++count;
    return count;
}

int ContentValidationReport::WarningCount() const
{
    return (int)issues.size() - ErrorCount();
}

void ContentValidationReport::AddError(const ContentId& contentId, const std::string& field, const std::string& message)
{
    issues.push_back({ ContentIssueSeverity::Error, contentId, field, message });
}

void ContentValidationReport::AddWarning(const ContentId& contentId, const std::string& field, const std::string& message)
{
    issues.push_back({ ContentIssueSeverity::Warning, contentId, field, message });
}

// -- Shared numeric checks -----------------------------------------------------

bool IsWellFormedContentId(const ContentId& id)
{
    if (id.empty())
        return false;
    for (char character : id)
    {
        bool isValid = (character >= 'a' && character <= 'z') ||
                       (character >= '0' && character <= '9') ||
                       character == '_';
        if (!isValid)
            return false;
    }
    return true;
}

// Rejects NaN and infinity, which corrupt saved files and runtime math.
static bool IsFiniteNumber(float value)
{
    return std::isfinite(value);
}

// Checks one named float for NaN/inf and an allowed minimum.
static void CheckNumber(ContentValidationReport& report, const ContentId& ownerId,
                        const std::string& field, float value, float minimumAllowed)
{
    if (!IsFiniteNumber(value))
        report.AddError(ownerId, field, field + " is NaN or infinite");
    else if (value < minimumAllowed)
    {
        std::ostringstream message;
        message << field << " is " << value << " but must be at least " << minimumAllowed;
        report.AddError(ownerId, field, message.str());
    }
}

// -- Timeline validation -------------------------------------------------------

void ValidateBehaviorTimeline(const ContentId& ownerId, const BehaviorTimeline& timeline,
                              bool hasDamageComponent, ContentValidationReport& report)
{
    const int stepCount = (int)timeline.steps.size();
    for (int stepIndex = 0; stepIndex < stepCount; ++stepIndex)
    {
        const TimelineStep& step = timeline.steps[stepIndex];
        std::ostringstream fieldName;
        fieldName << "timeline." << stepIndex;
        const std::string field = fieldName.str();

        CheckNumber(report, ownerId, field + ".duration", step.duration, 0.f);

        switch (step.action)
        {
        case TimelineActionKind::Repeat:
            // Repeat must jump to an existing EARLIER step; forward or
            // self-jumps could skip work or loop without progress.
            if (step.repeatTarget < 0 || step.repeatTarget >= stepCount)
                report.AddError(ownerId, field, "Repeat target is out of range");
            else if (step.repeatTarget >= stepIndex)
                report.AddError(ownerId, field, "Repeat must jump to an earlier step");
            else if (step.repeatCount < 0)
            {
                // Unbounded loops are allowed only when the loop body takes
                // real time; a zero-duration unbounded loop would spin forever
                // inside one frame.
                float loopBodyDuration = 0.f;
                for (int bodyIndex = step.repeatTarget; bodyIndex < stepIndex; ++bodyIndex)
                    loopBodyDuration += timeline.steps[bodyIndex].duration;
                if (loopBodyDuration <= 0.f)
                    report.AddError(ownerId, field, "unbounded Repeat loop has zero total duration");
            }
            break;

        case TimelineActionKind::EnableDamage:
            if (!hasDamageComponent)
                report.AddError(ownerId, field, "EnableDamage used but the asset has no [damage] component");
            break;

        case TimelineActionKind::PlayAnimation:
        case TimelineActionKind::PlayVfx:
        case TimelineActionKind::PlaySound:
        case TimelineActionKind::EmitEvent:
            if (step.target.empty())
                report.AddError(ownerId, field, std::string(TimelineActionName(step.action)) + " is missing its target");
            break;

        default:
            break;
        }
    }
}

// -- Definition validation -----------------------------------------------------

void ValidateGameplayAssetDefinition(const GameplayAssetDefinition& definition, ContentValidationReport& report)
{
    if (!IsWellFormedContentId(definition.id))
        report.AddError(definition.id, "id", "id must be non-empty lowercase snake_case ([a-z0-9_])");
    if (definition.displayName.empty())
        report.AddWarning(definition.id, "name", "asset has no display name");
    if (definition.visual.spritePath.empty())
        report.AddError(definition.id, "visual.sprite", "visual sprite path is empty");
    CheckNumber(report, definition.id, "visual.scale", definition.visual.scale, 0.01f);

    if (definition.collision)
    {
        int shapeIndex = 0;
        for (const CollisionShape& shapeData : definition.collision->shapes)
        {
            std::ostringstream field;
            field << "collider." << shapeIndex++;
            CheckNumber(report, definition.id, field.str() + ".width",  shapeData.width, 0.01f);
            CheckNumber(report, definition.id, field.str() + ".height", shapeData.height, 0.01f);
        }
        if (definition.collision->shapes.empty())
            report.AddWarning(definition.id, "collider", "collision component present but has no shapes");
    }

    if (definition.damage)
    {
        CheckNumber(report, definition.id, "damage.amount", definition.damage->damageAmount, 0.f);
        CheckNumber(report, definition.id, "damage.hit_cooldown", definition.damage->hitCooldown, 0.f);
        // Damage needs geometry: either colliders or (later) explicit damage
        // shapes. Without any, the damage can never land or, worse, a future
        // runtime could interpret it as everywhere.
        bool hasGeometry = definition.collision && !definition.collision->shapes.empty();
        if (!hasGeometry)
            report.AddError(definition.id, "damage", "damage component present but the asset has no collision shapes");
    }

    if (definition.movement)
        CheckNumber(report, definition.id, "movement.speed", definition.movement->speed, 0.f);

    if (definition.targeting)
        CheckNumber(report, definition.id, "targeting.lock_time", definition.targeting->lockTime, 0.f);

    if (definition.emitter)
    {
        if (definition.emitter->projectileId.empty())
            report.AddError(definition.id, "emitter.projectile", "emitter has no projectile id");
        if (definition.emitter->count < 1)
            report.AddError(definition.id, "emitter.count", "emitter count must be at least 1");
        if (definition.emitter->maximumLiveChildren < 1)
            report.AddError(definition.id, "emitter.max_live_children", "emitter live-children cap must be at least 1");
        CheckNumber(report, definition.id, "emitter.cooldown", definition.emitter->cooldown, 0.f);
    }

    if (definition.trigger)
    {
        bool needsWatchedInstance = definition.trigger->source == TriggerSourceKind::SwitchActivated ||
                                    definition.trigger->source == TriggerSourceKind::ObjectDestroyed;
        if (needsWatchedInstance && definition.trigger->watchedInstanceId.empty())
            report.AddError(definition.id, "trigger.watched_instance",
                            "SwitchActivated/ObjectDestroyed triggers need a watched instance id");
    }

    if (definition.destructible)
        CheckNumber(report, definition.id, "destructible.health", definition.destructible->health, 0.01f);

    ValidateBehaviorTimeline(definition.id, definition.timeline, definition.damage.has_value(), report);
}

void ValidateProjectileDefinition(const ProjectileDefinition& definition, ContentValidationReport& report)
{
    if (!IsWellFormedContentId(definition.id))
        report.AddError(definition.id, "id", "id must be non-empty lowercase snake_case ([a-z0-9_])");
    if (definition.spritePath.empty())
        report.AddError(definition.id, "visual.sprite", "projectile sprite path is empty");

    CheckNumber(report, definition.id, "collision.radius", definition.radius, 0.01f);
    CheckNumber(report, definition.id, "travel.speed", definition.speed, 0.f);
    CheckNumber(report, definition.id, "payload.damage", definition.damage, 0.f);

    // Every projectile must expire: a positive lifetime or a positive range
    // (design doc: "projectiles without lifetime/range" are rejected).
    bool hasLifetime = IsFiniteNumber(definition.lifetime) && definition.lifetime > 0.f;
    bool hasRange    = IsFiniteNumber(definition.range) && definition.range > 0.f;
    if (!hasLifetime && !hasRange)
        report.AddError(definition.id, "travel", "projectile needs a positive lifetime or range to ever expire");

    if (definition.maximumLiveInstances < 1)
        report.AddError(definition.id, "limits.max_live_instances", "live-instance cap must be at least 1");
    if (definition.maximumLiveInstances > 128)
        report.AddError(definition.id, "limits.max_live_instances", "live-instance cap exceeds the hard cap of 128");

    if (!definition.hitsPlayer && !definition.hitsEnemies && !definition.hitsDestructibles)
        report.AddWarning(definition.id, "layers", "projectile cannot hit anything");
}

void ValidateEnemyDefinition(const EnemyDefinition& definition, ContentValidationReport& report)
{
    if (!IsWellFormedContentId(definition.id))
        report.AddError(definition.id, "id", "id must be non-empty lowercase snake_case ([a-z0-9_])");
    for (const auto& statPair : definition.baseStats)
        if (!IsFiniteNumber(statPair.second))
            report.AddError(definition.id, "stats." + statPair.first, "stat is NaN or infinite");
}

// -- Unknown-field reporting ---------------------------------------------------

void ReportUnconsumedEntries(const ContentDocument& document, const ContentId& ownerId,
                             ContentValidationReport& report)
{
    for (const ContentEntry* entry : document.CollectUnconsumedEntries())
    {
        std::ostringstream message;
        message << "unknown field '" << entry->key << "' (line " << entry->lineNumber
                << ") -- possible typo, it was ignored";
        report.AddWarning(ownerId, entry->key, message.str());
    }
}
