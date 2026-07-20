#include "ContentSerializers.h"

#include <map>
#include <sstream>

// -- Migration chain -----------------------------------------------------------

// Registered steps: fromVersion -> function producing fromVersion + 1.
static std::map<int, ContentMigrationStep>& MigrationSteps()
{
    static std::map<int, ContentMigrationStep> steps;
    return steps;
}

void RegisterContentMigrationStep(int fromVersion, ContentMigrationStep step)
{
    MigrationSteps()[fromVersion] = step;
}

void ClearContentMigrationStepsForTesting()
{
    MigrationSteps().clear();
}

// Reads the stored version, then applies registered steps until the document
// reaches kContentSchemaVersion. The version entry is updated as steps run.
bool MigrateContentDocument(ContentDocument& document, ContentError& error)
{
    int storedVersion = document.GlobalSection().GetIntOr("version", 1);

    if (storedVersion > kContentSchemaVersion)
    {
        std::ostringstream message;
        message << "file is schema version " << storedVersion
                << " but this build only understands up to " << kContentSchemaVersion
                << " (made by a newer build?)";
        error.message  = message.str();
        error.filePath = document.SourceFilePath();
        return false;
    }

    while (storedVersion < kContentSchemaVersion)
    {
        auto stepIterator = MigrationSteps().find(storedVersion);
        if (stepIterator == MigrationSteps().end())
        {
            std::ostringstream message;
            message << "no migration registered from schema version " << storedVersion;
            error.message  = message.str();
            error.filePath = document.SourceFilePath();
            return false;
        }
        if (!stepIterator->second(document, error))
            return false;
        ++storedVersion;

        // Keep the in-memory version entry honest after each step. Because
        // getters cannot rewrite values in place, migrations that change data
        // rewrite entries themselves; we only track the counter here.
    }
    return true;
}

// -- Shared small helpers ------------------------------------------------------

// Writes an x/y pair as two entries ("offset_x" / "offset_y").
static void AppendVectorEntries(ContentSection& section, const std::string& baseKey, ContentVector2 value)
{
    ContentDocument::AppendEntry(section, baseKey + "_x", value.x);
    ContentDocument::AppendEntry(section, baseKey + "_y", value.y);
}

static ContentVector2 ReadVectorOr(const ContentSection& section, const std::string& baseKey, ContentVector2 fallback)
{
    ContentVector2 result;
    result.x = section.GetFloatOr(baseKey + "_x", fallback.x);
    result.y = section.GetFloatOr(baseKey + "_y", fallback.y);
    return result;
}

// Splits "a,b,c" into trimmed pieces (used for tags and reference lists).
static std::vector<std::string> SplitCommaList(const std::string& text)
{
    std::vector<std::string> pieces;
    std::istringstream stream(text);
    std::string piece;
    while (std::getline(stream, piece, ','))
    {
        size_t first = piece.find_first_not_of(" \t");
        size_t last  = piece.find_last_not_of(" \t");
        if (first != std::string::npos)
            pieces.push_back(piece.substr(first, last - first + 1));
    }
    return pieces;
}

static std::string JoinCommaList(const std::vector<std::string>& pieces)
{
    std::string joined;
    for (size_t index = 0; index < pieces.size(); ++index)
    {
        if (index > 0)
            joined += ",";
        joined += pieces[index];
    }
    return joined;
}

// Requires the global id/name pair every content type shares.
static bool ReadCommonHeader(const ContentDocument& document, ContentId& outId,
                             std::string& outDisplayName, int& outVersion, ContentError& error)
{
    const ContentSection& global = document.GlobalSection();
    if (!global.RequireString("id", outId, error))
        return false;
    outDisplayName = global.GetStringOr("name", outId);
    outVersion     = global.GetIntOr("version", 1);
    return true;
}

// -- Gameplay asset loading ----------------------------------------------------

bool LoadGameplayAssetFromDocument(ContentDocument& document,
                                   GameplayAssetDefinition& out, ContentError& error)
{
    if (!MigrateContentDocument(document, error))
        return false;

    if (!ReadCommonHeader(document, out.id, out.displayName, out.schemaVersion, error))
        return false;
    const ContentSection& global = document.GlobalSection();
    out.category = global.GetStringOr("category", "Uncategorized");
    out.tags     = SplitCommaList(global.GetStringOr("tags", ""));

    // [visual] is required -- every placeable object must draw something.
    const ContentSection* visualSection = document.FindSection("visual");
    if (!visualSection)
    {
        error.message  = "gameplay asset '" + out.id + "' is missing its [visual] section";
        error.filePath = document.SourceFilePath();
        return false;
    }
    if (!visualSection->RequireString("sprite", out.visual.spritePath, error))
        return false;
    out.visual.scale    = visualSection->GetFloatOr("scale", 1.f);
    out.visual.pivot    = ReadVectorOr(*visualSection, "pivot", { 0.5f, 0.5f });
    out.visual.drawBand = visualSection->GetIntOr("draw_band", 1);
    out.visual.visible  = visualSection->GetBoolOr("visible", true);

    // Optional components: present exactly when their section exists.
    if (const ContentSection* section = document.FindSection("animation"))
    {
        AnimationComponent animation;
        animation.defaultClip = section->GetStringOr("default_clip", "");
        for (const ContentSection* clipSection : document.FindNumberedSections("animation.clip"))
        {
            AnimationClip clip;
            if (!clipSection->RequireString("name", clip.name, error))
                return false;
            clip.frameCount = clipSection->GetIntOr("frame_count", 1);
            clip.frameTime  = clipSection->GetFloatOr("frame_time", 0.1f);
            clip.loops      = clipSection->GetBoolOr("loops", true);
            animation.clips.push_back(clip);
        }
        out.animation = animation;
    }

    if (document.FindSection("collider") || !document.FindNumberedSections("collider").empty())
    {
        CollisionComponent collision;
        for (const ContentSection* colliderSection : document.FindNumberedSections("collider"))
        {
            CollisionShape shapeData;
            shapeData.shape  = CollisionShapeFromName(colliderSection->GetStringOr("shape", "Rectangle"));
            if (shapeData.shape == CollisionShapeKind::Count)
            {
                error.message    = "unknown collider shape in asset '" + out.id + "'";
                error.lineNumber = colliderSection->lineNumber;
                return false;
            }
            shapeData.offset            = ReadVectorOr(*colliderSection, "offset", { 0.f, 0.f });
            shapeData.width             = colliderSection->GetFloatOr("width", 32.f);
            shapeData.height            = colliderSection->GetFloatOr("height", 32.f);
            shapeData.blocksPlayer      = colliderSection->GetBoolOr("blocks_player", true);
            shapeData.blocksEnemies     = colliderSection->GetBoolOr("blocks_enemies", true);
            shapeData.blocksProjectiles = colliderSection->GetBoolOr("blocks_projectiles", false);
            shapeData.blocksDashes      = colliderSection->GetBoolOr("blocks_dashes", false);
            collision.shapes.push_back(shapeData);
        }
        out.collision = collision;
    }

    if (const ContentSection* section = document.FindSection("damage"))
    {
        DamageComponent damage;
        damage.damageAmount          = section->GetFloatOr("amount", 10.f);
        damage.tickInterval          = section->GetFloatOr("tick_interval", 0.f);
        damage.knockback             = section->GetFloatOr("knockback", 0.f);
        damage.hitCooldown           = section->GetFloatOr("hit_cooldown", 0.5f);
        damage.statusEffect          = section->GetStringOr("status", "");
        damage.activeDuringTelegraph = section->GetBoolOr("active_during_telegraph", false);
        out.damage = damage;
    }

    if (const ContentSection* section = document.FindSection("movement"))
    {
        MovementComponent movement;
        movement.pattern = MovementPatternFromName(section->GetStringOr("pattern", "Static"));
        if (movement.pattern == MovementPatternKind::Count)
        {
            error.message    = "unknown movement pattern in asset '" + out.id + "'";
            error.lineNumber = section->lineNumber;
            return false;
        }
        movement.speed              = section->GetFloatOr("speed", 100.f);
        movement.waitAtPoints       = section->GetFloatOr("wait_at_points", 0.f);
        movement.restartOnRoomEntry = section->GetBoolOr("restart_on_room_entry", true);
        out.movement = movement;
    }

    if (const ContentSection* section = document.FindSection("targeting"))
    {
        TargetingComponent targeting;
        targeting.mode = TargetingModeFromName(section->GetStringOr("mode", "PlayerSnapshot"));
        if (targeting.mode == TargetingModeKind::Count)
        {
            error.message    = "unknown targeting mode in asset '" + out.id + "'";
            error.lineNumber = section->lineNumber;
            return false;
        }
        targeting.lockTime = section->GetFloatOr("lock_time", 0.35f);
        out.targeting = targeting;
    }

    if (const ContentSection* section = document.FindSection("emitter"))
    {
        EmitterComponent emitter;
        if (!section->RequireString("projectile", emitter.projectileId, error))
            return false;
        emitter.spawnOffset         = ReadVectorOr(*section, "spawn_offset", { 0.f, 0.f });
        emitter.count               = section->GetIntOr("count", 1);
        emitter.spreadDegrees       = section->GetFloatOr("spread", 0.f);
        emitter.burstDelay          = section->GetFloatOr("burst_delay", 0.f);
        emitter.cooldown            = section->GetFloatOr("cooldown", 1.f);
        emitter.maximumLiveChildren = section->GetIntOr("max_live_children", 8);
        out.emitter = emitter;
    }

    if (const ContentSection* section = document.FindSection("trigger"))
    {
        TriggerComponent trigger;
        trigger.source = TriggerSourceFromName(section->GetStringOr("type", "CombatStart"));
        if (trigger.source == TriggerSourceKind::Count)
        {
            error.message    = "unknown trigger type in asset '" + out.id + "'";
            error.lineNumber = section->lineNumber;
            return false;
        }
        trigger.oneShot           = section->GetBoolOr("one_shot", false);
        trigger.delay             = section->GetFloatOr("delay", 0.f);
        trigger.watchedInstanceId = section->GetStringOr("watched_instance", "");
        out.trigger = trigger;
    }

    if (const ContentSection* section = document.FindSection("destructible"))
    {
        DestructibleComponent destructible;
        destructible.health              = section->GetFloatOr("health", 30.f);
        destructible.hitCooldown         = section->GetFloatOr("hit_cooldown", 0.1f);
        destructible.recoveryTime        = section->GetFloatOr("recovery_time", 0.f);
        destructible.blocksWhenDestroyed = section->GetBoolOr("blocks_when_destroyed", false);
        out.destructible = destructible;
    }

    if (const ContentSection* section = document.FindSection("interaction"))
    {
        InteractionComponent interaction;
        interaction.prompt       = section->GetStringOr("prompt", "Interact");
        interaction.radius       = section->GetFloatOr("radius", 60.f);
        interaction.emittedEvent = section->GetStringOr("emitted_event", "");
        out.interaction = interaction;
    }

    if (const ContentSection* section = document.FindSection("feedback"))
    {
        AudioVfxComponent feedback;
        feedback.telegraphVfx    = section->GetStringOr("telegraph_vfx", "");
        feedback.activationVfx   = section->GetStringOr("activation_vfx", "");
        feedback.impactVfx       = section->GetStringOr("impact_vfx", "");
        feedback.destroyedVfx    = section->GetStringOr("destroyed_vfx", "");
        feedback.telegraphSound  = section->GetStringOr("telegraph_sound", "");
        feedback.activationSound = section->GetStringOr("activation_sound", "");
        feedback.impactSound     = section->GetStringOr("impact_sound", "");
        feedback.destroyedSound  = section->GetStringOr("destroyed_sound", "");
        out.feedback = feedback;
    }

    // Timeline: ordered [timeline.N] sections.
    out.timeline.steps.clear();
    for (const ContentSection* stepSection : document.FindNumberedSections("timeline"))
    {
        std::string actionName;
        if (!stepSection->RequireString("action", actionName, error))
            return false;
        TimelineStep step;
        step.action = TimelineActionFromName(actionName);
        if (step.action == TimelineActionKind::Count)
        {
            error.message    = "unknown timeline action '" + actionName + "' in asset '" + out.id + "'";
            error.lineNumber = stepSection->lineNumber;
            return false;
        }
        step.duration     = stepSection->GetFloatOr("duration", 0.f);
        step.target       = stepSection->GetStringOr("target", "");
        step.repeatTarget = stepSection->GetIntOr("repeat_target", -1);
        step.repeatCount  = stepSection->GetIntOr("repeat_count", -1);
        step.enabled      = stepSection->GetBoolOr("enabled", true);
        out.timeline.steps.push_back(step);
    }
    return true;
}

// -- Gameplay asset saving -----------------------------------------------------

void SaveGameplayAssetToDocument(const GameplayAssetDefinition& definition, ContentDocument& out)
{
    out = ContentDocument{};
    ContentSection& global = out.EditGlobalSection();
    ContentDocument::AppendEntry(global, "version", kContentSchemaVersion);
    ContentDocument::AppendEntry(global, "id", definition.id);
    ContentDocument::AppendEntry(global, "name", definition.displayName);
    ContentDocument::AppendEntry(global, "category", definition.category);
    if (!definition.tags.empty())
        ContentDocument::AppendEntry(global, "tags", JoinCommaList(definition.tags));

    ContentSection& visual = out.AddSection("visual");
    ContentDocument::AppendEntry(visual, "sprite", definition.visual.spritePath);
    ContentDocument::AppendEntry(visual, "scale", definition.visual.scale);
    AppendVectorEntries(visual, "pivot", definition.visual.pivot);
    ContentDocument::AppendEntry(visual, "draw_band", definition.visual.drawBand);
    ContentDocument::AppendEntry(visual, "visible", definition.visual.visible);

    if (definition.animation)
    {
        ContentSection& animation = out.AddSection("animation");
        ContentDocument::AppendEntry(animation, "default_clip", definition.animation->defaultClip);
        int clipIndex = 0;
        for (const AnimationClip& clip : definition.animation->clips)
        {
            ContentSection& clipSection = out.AddSection("animation.clip", clipIndex++);
            ContentDocument::AppendEntry(clipSection, "name", clip.name);
            ContentDocument::AppendEntry(clipSection, "frame_count", clip.frameCount);
            ContentDocument::AppendEntry(clipSection, "frame_time", clip.frameTime);
            ContentDocument::AppendEntry(clipSection, "loops", clip.loops);
        }
    }

    if (definition.collision)
    {
        int colliderIndex = 0;
        for (const CollisionShape& shapeData : definition.collision->shapes)
        {
            ContentSection& colliderSection = out.AddSection("collider", colliderIndex++);
            ContentDocument::AppendEntry(colliderSection, "shape", CollisionShapeName(shapeData.shape));
            AppendVectorEntries(colliderSection, "offset", shapeData.offset);
            ContentDocument::AppendEntry(colliderSection, "width", shapeData.width);
            ContentDocument::AppendEntry(colliderSection, "height", shapeData.height);
            ContentDocument::AppendEntry(colliderSection, "blocks_player", shapeData.blocksPlayer);
            ContentDocument::AppendEntry(colliderSection, "blocks_enemies", shapeData.blocksEnemies);
            ContentDocument::AppendEntry(colliderSection, "blocks_projectiles", shapeData.blocksProjectiles);
            ContentDocument::AppendEntry(colliderSection, "blocks_dashes", shapeData.blocksDashes);
        }
    }

    if (definition.damage)
    {
        ContentSection& damage = out.AddSection("damage");
        ContentDocument::AppendEntry(damage, "amount", definition.damage->damageAmount);
        ContentDocument::AppendEntry(damage, "tick_interval", definition.damage->tickInterval);
        ContentDocument::AppendEntry(damage, "knockback", definition.damage->knockback);
        ContentDocument::AppendEntry(damage, "hit_cooldown", definition.damage->hitCooldown);
        if (!definition.damage->statusEffect.empty())
            ContentDocument::AppendEntry(damage, "status", definition.damage->statusEffect);
        ContentDocument::AppendEntry(damage, "active_during_telegraph", definition.damage->activeDuringTelegraph);
    }

    if (definition.movement)
    {
        ContentSection& movement = out.AddSection("movement");
        ContentDocument::AppendEntry(movement, "pattern", MovementPatternName(definition.movement->pattern));
        ContentDocument::AppendEntry(movement, "speed", definition.movement->speed);
        ContentDocument::AppendEntry(movement, "wait_at_points", definition.movement->waitAtPoints);
        ContentDocument::AppendEntry(movement, "restart_on_room_entry", definition.movement->restartOnRoomEntry);
    }

    if (definition.targeting)
    {
        ContentSection& targeting = out.AddSection("targeting");
        ContentDocument::AppendEntry(targeting, "mode", TargetingModeName(definition.targeting->mode));
        ContentDocument::AppendEntry(targeting, "lock_time", definition.targeting->lockTime);
    }

    if (definition.emitter)
    {
        ContentSection& emitter = out.AddSection("emitter");
        ContentDocument::AppendEntry(emitter, "projectile", definition.emitter->projectileId);
        AppendVectorEntries(emitter, "spawn_offset", definition.emitter->spawnOffset);
        ContentDocument::AppendEntry(emitter, "count", definition.emitter->count);
        ContentDocument::AppendEntry(emitter, "spread", definition.emitter->spreadDegrees);
        ContentDocument::AppendEntry(emitter, "burst_delay", definition.emitter->burstDelay);
        ContentDocument::AppendEntry(emitter, "cooldown", definition.emitter->cooldown);
        ContentDocument::AppendEntry(emitter, "max_live_children", definition.emitter->maximumLiveChildren);
    }

    if (definition.trigger)
    {
        ContentSection& trigger = out.AddSection("trigger");
        ContentDocument::AppendEntry(trigger, "type", TriggerSourceName(definition.trigger->source));
        ContentDocument::AppendEntry(trigger, "one_shot", definition.trigger->oneShot);
        ContentDocument::AppendEntry(trigger, "delay", definition.trigger->delay);
        if (!definition.trigger->watchedInstanceId.empty())
            ContentDocument::AppendEntry(trigger, "watched_instance", definition.trigger->watchedInstanceId);
    }

    if (definition.destructible)
    {
        ContentSection& destructible = out.AddSection("destructible");
        ContentDocument::AppendEntry(destructible, "health", definition.destructible->health);
        ContentDocument::AppendEntry(destructible, "hit_cooldown", definition.destructible->hitCooldown);
        ContentDocument::AppendEntry(destructible, "recovery_time", definition.destructible->recoveryTime);
        ContentDocument::AppendEntry(destructible, "blocks_when_destroyed", definition.destructible->blocksWhenDestroyed);
    }

    if (definition.interaction)
    {
        ContentSection& interaction = out.AddSection("interaction");
        ContentDocument::AppendEntry(interaction, "prompt", definition.interaction->prompt);
        ContentDocument::AppendEntry(interaction, "radius", definition.interaction->radius);
        if (!definition.interaction->emittedEvent.empty())
            ContentDocument::AppendEntry(interaction, "emitted_event", definition.interaction->emittedEvent);
    }

    if (definition.feedback)
    {
        ContentSection& feedback = out.AddSection("feedback");
        const AudioVfxComponent& audioVfx = *definition.feedback;
        // Only write the events that are actually set, keeping files short.
        if (!audioVfx.telegraphVfx.empty())    ContentDocument::AppendEntry(feedback, "telegraph_vfx", audioVfx.telegraphVfx);
        if (!audioVfx.activationVfx.empty())   ContentDocument::AppendEntry(feedback, "activation_vfx", audioVfx.activationVfx);
        if (!audioVfx.impactVfx.empty())       ContentDocument::AppendEntry(feedback, "impact_vfx", audioVfx.impactVfx);
        if (!audioVfx.destroyedVfx.empty())    ContentDocument::AppendEntry(feedback, "destroyed_vfx", audioVfx.destroyedVfx);
        if (!audioVfx.telegraphSound.empty())  ContentDocument::AppendEntry(feedback, "telegraph_sound", audioVfx.telegraphSound);
        if (!audioVfx.activationSound.empty()) ContentDocument::AppendEntry(feedback, "activation_sound", audioVfx.activationSound);
        if (!audioVfx.impactSound.empty())     ContentDocument::AppendEntry(feedback, "impact_sound", audioVfx.impactSound);
        if (!audioVfx.destroyedSound.empty())  ContentDocument::AppendEntry(feedback, "destroyed_sound", audioVfx.destroyedSound);
    }

    int stepIndex = 0;
    for (const TimelineStep& step : definition.timeline.steps)
    {
        ContentSection& stepSection = out.AddSection("timeline", stepIndex++);
        ContentDocument::AppendEntry(stepSection, "action", TimelineActionName(step.action));
        if (step.duration != 0.f)
            ContentDocument::AppendEntry(stepSection, "duration", step.duration);
        if (!step.target.empty())
            ContentDocument::AppendEntry(stepSection, "target", step.target);
        if (step.repeatTarget >= 0)
            ContentDocument::AppendEntry(stepSection, "repeat_target", step.repeatTarget);
        if (step.repeatCount >= 0)
            ContentDocument::AppendEntry(stepSection, "repeat_count", step.repeatCount);
        if (step.action == TimelineActionKind::SetColliderEnabled)
            ContentDocument::AppendEntry(stepSection, "enabled", step.enabled);
    }
}

// -- Projectile loading / saving -----------------------------------------------

bool LoadProjectileFromDocument(ContentDocument& document,
                                ProjectileDefinition& out, ContentError& error)
{
    if (!MigrateContentDocument(document, error))
        return false;
    if (!ReadCommonHeader(document, out.id, out.displayName, out.schemaVersion, error))
        return false;

    const ContentSection* visualSection = document.FindSection("visual");
    if (!visualSection)
    {
        error.message  = "projectile '" + out.id + "' is missing its [visual] section";
        error.filePath = document.SourceFilePath();
        return false;
    }
    if (!visualSection->RequireString("sprite", out.spritePath, error))
        return false;
    out.frameCount = visualSection->GetIntOr("frame_count", 1);
    out.frameTime  = visualSection->GetFloatOr("frame_time", 0.08f);
    out.scale      = visualSection->GetFloatOr("scale", 1.f);

    if (const ContentSection* section = document.FindSection("collision"))
    {
        out.shape = CollisionShapeFromName(section->GetStringOr("shape", "Circle"));
        if (out.shape == CollisionShapeKind::Count)
        {
            error.message    = "unknown collision shape in projectile '" + out.id + "'";
            error.lineNumber = section->lineNumber;
            return false;
        }
        out.radius = section->GetFloatOr("radius", 8.f);
    }

    if (const ContentSection* section = document.FindSection("travel"))
    {
        out.speed        = section->GetFloatOr("speed", 300.f);
        out.acceleration = section->GetFloatOr("acceleration", 0.f);
        out.maxTurnRate  = section->GetFloatOr("max_turn_rate", 0.f);
        out.lifetime     = section->GetFloatOr("lifetime", 3.f);
        out.range        = section->GetFloatOr("range", 900.f);
        out.pierceCount  = section->GetIntOr("pierce_count", 0);
        out.stopsOnWalls = section->GetBoolOr("stops_on_walls", true);
    }

    if (const ContentSection* section = document.FindSection("payload"))
    {
        out.targeting = TargetingModeFromName(section->GetStringOr("targeting", "PlayerSnapshot"));
        if (out.targeting == TargetingModeKind::Count)
        {
            error.message    = "unknown targeting mode in projectile '" + out.id + "'";
            error.lineNumber = section->lineNumber;
            return false;
        }
        out.damage       = section->GetFloatOr("damage", 10.f);
        out.knockback    = section->GetFloatOr("knockback", 0.f);
        out.statusEffect = section->GetStringOr("status", "");
    }

    if (const ContentSection* section = document.FindSection("feedback"))
    {
        out.castVfx      = section->GetStringOr("cast_vfx", "");
        out.travelVfx    = section->GetStringOr("travel_vfx", "");
        out.impactVfx    = section->GetStringOr("impact_vfx", "");
        out.blockedVfx   = section->GetStringOr("blocked_vfx", "");
        out.castSound    = section->GetStringOr("cast_sound", "");
        out.travelSound  = section->GetStringOr("travel_sound", "");
        out.impactSound  = section->GetStringOr("impact_sound", "");
        out.blockedSound = section->GetStringOr("blocked_sound", "");
    }

    if (const ContentSection* section = document.FindSection("layers"))
    {
        out.hitsPlayer        = section->GetBoolOr("hits_player", true);
        out.hitsEnemies       = section->GetBoolOr("hits_enemies", false);
        out.hitsDestructibles = section->GetBoolOr("hits_destructibles", true);
    }

    if (const ContentSection* section = document.FindSection("limits"))
    {
        out.pressureCost         = section->GetFloatOr("pressure_cost", 1.f);
        out.maximumLiveInstances = section->GetIntOr("max_live_instances", 16);
    }
    return true;
}

void SaveProjectileToDocument(const ProjectileDefinition& definition, ContentDocument& out)
{
    out = ContentDocument{};
    ContentSection& global = out.EditGlobalSection();
    ContentDocument::AppendEntry(global, "version", kContentSchemaVersion);
    ContentDocument::AppendEntry(global, "id", definition.id);
    ContentDocument::AppendEntry(global, "name", definition.displayName);

    ContentSection& visual = out.AddSection("visual");
    ContentDocument::AppendEntry(visual, "sprite", definition.spritePath);
    ContentDocument::AppendEntry(visual, "frame_count", definition.frameCount);
    ContentDocument::AppendEntry(visual, "frame_time", definition.frameTime);
    ContentDocument::AppendEntry(visual, "scale", definition.scale);

    ContentSection& collision = out.AddSection("collision");
    ContentDocument::AppendEntry(collision, "shape", CollisionShapeName(definition.shape));
    ContentDocument::AppendEntry(collision, "radius", definition.radius);

    ContentSection& travel = out.AddSection("travel");
    ContentDocument::AppendEntry(travel, "speed", definition.speed);
    ContentDocument::AppendEntry(travel, "acceleration", definition.acceleration);
    ContentDocument::AppendEntry(travel, "max_turn_rate", definition.maxTurnRate);
    ContentDocument::AppendEntry(travel, "lifetime", definition.lifetime);
    ContentDocument::AppendEntry(travel, "range", definition.range);
    ContentDocument::AppendEntry(travel, "pierce_count", definition.pierceCount);
    ContentDocument::AppendEntry(travel, "stops_on_walls", definition.stopsOnWalls);

    ContentSection& payload = out.AddSection("payload");
    ContentDocument::AppendEntry(payload, "targeting", TargetingModeName(definition.targeting));
    ContentDocument::AppendEntry(payload, "damage", definition.damage);
    ContentDocument::AppendEntry(payload, "knockback", definition.knockback);
    if (!definition.statusEffect.empty())
        ContentDocument::AppendEntry(payload, "status", definition.statusEffect);

    ContentSection& feedback = out.AddSection("feedback");
    if (!definition.castVfx.empty())      ContentDocument::AppendEntry(feedback, "cast_vfx", definition.castVfx);
    if (!definition.travelVfx.empty())    ContentDocument::AppendEntry(feedback, "travel_vfx", definition.travelVfx);
    if (!definition.impactVfx.empty())    ContentDocument::AppendEntry(feedback, "impact_vfx", definition.impactVfx);
    if (!definition.blockedVfx.empty())   ContentDocument::AppendEntry(feedback, "blocked_vfx", definition.blockedVfx);
    if (!definition.castSound.empty())    ContentDocument::AppendEntry(feedback, "cast_sound", definition.castSound);
    if (!definition.travelSound.empty())  ContentDocument::AppendEntry(feedback, "travel_sound", definition.travelSound);
    if (!definition.impactSound.empty())  ContentDocument::AppendEntry(feedback, "impact_sound", definition.impactSound);
    if (!definition.blockedSound.empty()) ContentDocument::AppendEntry(feedback, "blocked_sound", definition.blockedSound);

    ContentSection& layers = out.AddSection("layers");
    ContentDocument::AppendEntry(layers, "hits_player", definition.hitsPlayer);
    ContentDocument::AppendEntry(layers, "hits_enemies", definition.hitsEnemies);
    ContentDocument::AppendEntry(layers, "hits_destructibles", definition.hitsDestructibles);

    ContentSection& limits = out.AddSection("limits");
    ContentDocument::AppendEntry(limits, "pressure_cost", definition.pressureCost);
    ContentDocument::AppendEntry(limits, "max_live_instances", definition.maximumLiveInstances);
}

// -- Enemy loading / saving ----------------------------------------------------

bool LoadEnemyFromDocument(ContentDocument& document,
                           EnemyDefinition& out, ContentError& error)
{
    if (!MigrateContentDocument(document, error))
        return false;
    if (!ReadCommonHeader(document, out.id, out.displayName, out.schemaVersion, error))
        return false;

    // [stats] holds every editable base value as key=number. Storing them as a
    // named map lets the Enemy Inspector (Project 8) show layered breakdowns
    // without a schema change.
    out.baseStats.clear();
    if (const ContentSection* statsSection = document.FindSection("stats"))
    {
        for (const ContentEntry& entry : statsSection->entries)
        {
            float parsedValue = statsSection->GetFloatOr(entry.key, 0.f);
            out.baseStats[entry.key] = parsedValue;
        }
    }

    out.projectileIds.clear();
    const ContentSection& global = document.GlobalSection();
    for (const std::string& projectileId : SplitCommaList(global.GetStringOr("projectiles", "")))
        out.projectileIds.push_back(projectileId);
    return true;
}

void SaveEnemyToDocument(const EnemyDefinition& definition, ContentDocument& out)
{
    out = ContentDocument{};
    ContentSection& global = out.EditGlobalSection();
    ContentDocument::AppendEntry(global, "version", kContentSchemaVersion);
    ContentDocument::AppendEntry(global, "id", definition.id);
    ContentDocument::AppendEntry(global, "name", definition.displayName);
    if (!definition.projectileIds.empty())
        ContentDocument::AppendEntry(global, "projectiles", JoinCommaList(definition.projectileIds));

    ContentSection& stats = out.AddSection("stats");
    for (const auto& statPair : definition.baseStats)
        ContentDocument::AppendEntry(stats, statPair.first, statPair.second);
}
