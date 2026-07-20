#pragma once

// ContentValidation -- the checks that run at asset save, database scan, and
// (later) playtest start and public packaging (authoring engine design doc,
// "Validation And Error Handling").
//
// Errors block saving; warnings permit save but stay visible. Reference checks
// (does this emitter's projectile exist?) live on the ContentDatabase because
// they need the full loaded set; the per-definition checks here are pure.

#include "ContentTypes.h"
#include "ContentDocument.h"

#include <string>
#include <vector>

enum class ContentIssueSeverity
{
    Warning,   // save allowed, shown in the console
    Error,     // blocks save / public packaging
};

struct ContentIssue
{
    ContentIssueSeverity severity = ContentIssueSeverity::Error;
    ContentId            contentId;    // which asset the issue belongs to
    std::string          field;        // component/field hint for editor focus
    std::string          message;
};

struct ContentValidationReport
{
    std::vector<ContentIssue> issues;

    bool HasErrors() const;
    int  ErrorCount() const;
    int  WarningCount() const;
    void AddError(const ContentId& contentId, const std::string& field, const std::string& message);
    void AddWarning(const ContentId& contentId, const std::string& field, const std::string& message);
};

// True when the id is non-empty and uses only [a-z0-9_] -- stable IDs are
// lowercase snake_case so file names and references stay predictable.
bool IsWellFormedContentId(const ContentId& id);

// Per-definition checks (ranges, NaN/inf, timeline structure, damage geometry).
void ValidateGameplayAssetDefinition(const GameplayAssetDefinition& definition, ContentValidationReport& report);
void ValidateProjectileDefinition(const ProjectileDefinition& definition, ContentValidationReport& report);
void ValidateEnemyDefinition(const EnemyDefinition& definition, ContentValidationReport& report);

// The bounded timeline rules: valid repeat targets, bounded zero-duration
// loops, and no damage activation without damage geometry.
void ValidateBehaviorTimeline(const ContentId& ownerId, const BehaviorTimeline& timeline,
                              bool hasDamageComponent, ContentValidationReport& report);

// Reports every entry no serializer consumed as a warning (typo detection).
void ReportUnconsumedEntries(const ContentDocument& document, const ContentId& ownerId,
                             ContentValidationReport& report);
