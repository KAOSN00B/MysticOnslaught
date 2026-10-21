#include "RunLog.h"
#include "RunCheckpoint.h"
#include "SaveFiles.h"
#include "SaveIds.h"

#include <iterator>

namespace
{
    namespace L = RunCheckpointLimits;

    constexpr const char* kOutcomeNames[] = { "victory", "death", "abandoned" };
    constexpr const char* kStatusBurning  = "burning";
    constexpr const char* kStatusPoisoned = "poisoned";
    constexpr const char* kStatusChilled  = "chilled";
    constexpr long long   kMaxTimestamp   = 1LL << 40;   // seconds: far past any real clock
    constexpr float       kNotApplicable  = -1.f;

    JsonValue Text(const std::string& text) { return JsonValue::MakeString(text); }

    bool KindFromName(const std::string& name, DamageSourceKind& out)
    {
        for (DamageSourceKind kind : { DamageSourceKind::Unknown, DamageSourceKind::Enemy, DamageSourceKind::Hazard,
                                       DamageSourceKind::Status })
            if (name == DamageSourceKindName(kind)) { out = kind; return true; }
        return false;
    }

    JsonValue DeathToJson(const RunLogDeath& d)
    {
        JsonValue death = JsonValue::MakeObject();
        JsonValue& cause = death.Set("cause", JsonValue::MakeObject());
        cause.Set("kind", Text(DamageSourceKindName(d.cause.kind)));
        cause.Set("id", Text(d.cause.id));
        cause.Set("elite", JsonValue::MakeBool(d.cause.elite));
        cause.Set("boss", JsonValue::MakeBool(d.cause.boss));
        if (!d.cause.by.empty()) cause.Set("by", Text(d.cause.by));
        JsonValue& statuses = death.Set("statuses", JsonValue::MakeArray());
        if (d.burning)  statuses.Push(Text(kStatusBurning));
        if (d.poisoned) statuses.Push(Text(kStatusPoisoned));
        if (d.chilled)  statuses.Push(Text(kStatusChilled));
        JsonValue& context = death.Set("context", JsonValue::MakeObject());
        context.Set("inEliteCage", JsonValue::MakeBool(d.inEliteCage));
        context.Set("manaFull", JsonValue::MakeBool(d.manaFull));
        context.Set("emptyAbilitySlot", JsonValue::MakeBool(d.emptyAbilitySlot));
        context.Set("classResourceFull", JsonValue::MakeBool(d.classResourceFull));
        context.Set("curseActive", JsonValue::MakeBool(d.curseActive));
        JsonValue& modifiers = context.Set("activeModifiers", JsonValue::MakeArray());
        for (const std::string& label : d.activeModifiers) modifiers.Push(Text(label));
        context.Set("roomType", JsonValue::MakeInt(d.roomType));
        context.Set("roomDepth", JsonValue::MakeInt(d.roomDepth));
        context.Set("roomSpecial", JsonValue::MakeInt(d.roomSpecial));
        context.Set("roomAffix", Text(d.roomAffix));
        context.Set("secondsInRoom", JsonValue::MakeFloat(d.secondsInRoom));
        context.Set("secondsSinceDash", JsonValue::MakeFloat(d.secondsSinceDash));
        context.Set("bossHealthFraction", JsonValue::MakeFloat(d.bossHealthFraction));
        context.Set("goldCarried", JsonValue::MakeInt(d.goldCarried));
        return death;
    }

    RunLogDeath DeathFromJson(const JsonReader& death)
    {
        RunLogDeath d;
        const JsonReader cause = death.Object("cause");
        const std::string kind = cause.String("kind", SaveFiles::kMaxIdLength);
        if (cause.Ok() && !KindFromName(kind, d.cause.kind)) cause.Fail("unknown cause kind");
        d.cause.id = cause.String("id", SaveFiles::kMaxIdLength);
        d.cause.elite = cause.Bool("elite");
        d.cause.boss = cause.Bool("boss");
        d.cause.by = cause.StringOr("by", std::string(), SaveFiles::kMaxIdLength);
        const JsonReader statuses = death.Array("statuses", SaveFiles::kMaxIdListEntries);
        for (std::size_t i = 0; i < statuses.Count(); ++i)
        {
            const std::string status = statuses.Item(i).AsString(SaveFiles::kMaxIdLength);
            d.burning  = d.burning  || status == kStatusBurning;
            d.poisoned = d.poisoned || status == kStatusPoisoned;
            d.chilled  = d.chilled  || status == kStatusChilled;
        }
        const JsonReader context = death.Object("context");
        d.inEliteCage = context.Bool("inEliteCage");
        d.manaFull = context.Bool("manaFull");
        d.emptyAbilitySlot = context.Bool("emptyAbilitySlot");
        d.classResourceFull = context.Bool("classResourceFull");
        d.curseActive = context.Bool("curseActive");
        const JsonReader modifiers = context.Array("activeModifiers", L::kMaxModifiers);
        for (std::size_t i = 0; i < modifiers.Count(); ++i)
            d.activeModifiers.push_back(modifiers.Item(i).AsString(SaveFiles::kMaxTextLength));
        d.roomType = (int)context.Int("roomType", -1, L::kRoomTypes - 1);
        d.roomDepth = (int)context.Int("roomDepth", -1, L::kMaxDungeonRooms);
        d.roomSpecial = (int)context.Int("roomSpecial", 0, L::kRoomSpecials - 1);
        d.roomAffix = context.String("roomAffix", SaveFiles::kMaxIdLength);
        d.secondsInRoom = context.Float("secondsInRoom", kNotApplicable, L::kMaxSeconds);
        d.secondsSinceDash = context.Float("secondsSinceDash", kNotApplicable, L::kMaxSeconds);
        d.bossHealthFraction = context.Float("bossHealthFraction", kNotApplicable, 1.f);
        d.goldCarried = (int)context.Int("goldCarried", 0, L::kMaxCount);
        return d;
    }
}

const char* RunOutcomeName(RunOutcome outcome)
{
    const int index = (int)outcome;
    return (index >= 0 && index < (int)std::size(kOutcomeNames)) ? kOutcomeNames[index] : kOutcomeNames[0];
}

void RunLog::Add(const RunLogEntry& entry)
{
    _runs.insert(_runs.begin(), entry);
    if ((int)_runs.size() > kMaxRuns) _runs.resize(kMaxRuns);
    if (_totalRuns < L::kMaxCount) ++_totalRuns;
    if (entry.outcome == RunOutcome::Victory && _wins < L::kMaxCount) ++_wins;
}

JsonValue RunLog::ToJson() const
{
    JsonValue doc = JsonValue::MakeObject();
    doc.Set("schema", JsonValue::MakeInt(SaveFiles::kRunLogSchema));
    JsonValue& totals = doc.Set("totals", JsonValue::MakeObject());
    totals.Set("runs", JsonValue::MakeInt(_totalRuns));
    totals.Set("wins", JsonValue::MakeInt(_wins));
    JsonValue& runs = doc.Set("runs", JsonValue::MakeArray());
    for (const RunLogEntry& e : _runs)
    {
        JsonValue& run = runs.Push(JsonValue::MakeObject());
        run.Set("runId", JsonValue::MakeInt(e.runId));
        run.Set("endedAt", JsonValue::MakeInt(e.endedAt));
        run.Set("outcome", Text(RunOutcomeName(e.outcome)));
        run.Set("class", Text(SaveIdOf(e.cls) ? SaveIdOf(e.cls) : ""));
        run.Set("skin", Text(e.skin));
        run.Set("ascension", JsonValue::MakeInt(e.ascension));
        JsonValue& domains = run.Set("domains", JsonValue::MakeArray());
        for (const RunLogDomain& d : e.domains)
        {
            if (!SaveIdOf(d.biome)) continue;
            JsonValue& domain = domains.Push(JsonValue::MakeObject());
            domain.Set("biome", Text(SaveIdOf(d.biome)));
            domain.Set("roomsCleared", JsonValue::MakeInt(d.roomsCleared));
            domain.Set("seconds", JsonValue::MakeFloat(d.seconds));
            domain.Set("bossDefeated", JsonValue::MakeBool(d.bossDefeated));
        }
        run.Set("domainReached", JsonValue::MakeInt(e.domainReached));
        run.Set("seconds", JsonValue::MakeFloat(e.seconds));
        run.Set("roomsCleared", JsonValue::MakeInt(e.roomsCleared));
        run.Set("enemiesKilled", JsonValue::MakeInt(e.enemiesKilled));
        run.Set("elitesSlain", JsonValue::MakeInt(e.elitesSlain));
        run.Set("bossesDefeated", JsonValue::MakeInt(e.bossesDefeated));
        run.Set("gold", JsonValue::MakeInt(e.gold));
        run.Set("echoes", JsonValue::MakeInt(e.echoes));
        JsonValue& build = run.Set("build", JsonValue::MakeObject());
        build.Set("level", JsonValue::MakeInt(e.level));
        build.Set("maxHealth", JsonValue::MakeFloat(e.maxHealth));
        JsonValue& abilities = build.Set("abilities", JsonValue::MakeArray());
        for (const RunLogAbility& a : e.abilities)
        {
            const char* id = SaveIdOf(a.ability);
            if (!id) continue;
            JsonValue& ability = abilities.Push(JsonValue::MakeObject());
            ability.Set("id", Text(id));
            ability.Set("level", JsonValue::MakeInt(a.level));
        }
        JsonValue& relics = build.Set("relics", JsonValue::MakeArray());
        for (RelicType relic : e.relics)
            if (const char* id = SaveIdOf(relic)) relics.Push(Text(id));
        run.Set("ending", Text(e.ending));
        if (e.hasDeath) run.Set("death", DeathToJson(e.death));
    }
    return doc;
}

bool RunLog::FromJson(const JsonValue& document, RunLog& out, std::string& error)
{
    JsonReadErrors errors;
    const JsonReader root(document, errors);
    RunLog log;
    root.Int("schema", SaveFiles::kRunLogSchema, SaveFiles::kRunLogSchema);
    const JsonReader totals = root.Object("totals");
    log._totalRuns = totals.Int("runs", 0, L::kMaxCount);
    log._wins = totals.Int("wins", 0, log._totalRuns);
    const JsonReader runs = root.Array("runs", kMaxRuns);
    if (runs.IsPresent() && (long long)runs.Count() > log._totalRuns) runs.Fail("more records than runs counted");
    for (std::size_t i = 0; i < runs.Count(); ++i)
    {
        const JsonReader run = runs.Item(i);
        RunLogEntry e;
        e.runId = run.Int("runId", 1, L::kMaxRunId);
        e.endedAt = run.Int("endedAt", 0, kMaxTimestamp);
        const std::string outcome = run.String("outcome", SaveFiles::kMaxIdLength);
        bool knownOutcome = false;
        for (int o = 0; o < (int)std::size(kOutcomeNames); ++o)
            if (outcome == kOutcomeNames[o]) { e.outcome = (RunOutcome)o; knownOutcome = true; }
        if (run.Ok() && !knownOutcome) run.Fail("unknown outcome");
        // A class or domain removed from the game since: the record keeps its
        // numbers, with the class shown as the default and the domain dropped.
        SaveIdTo(run.String("class", SaveFiles::kMaxIdLength), e.cls);
        e.skin = run.String("skin", SaveFiles::kMaxIdLength);
        e.ascension = (int)run.Int("ascension", 0, L::kMaxSmallCount);
        const JsonReader domains = run.Array("domains", L::kMaxRouteEntries);
        for (std::size_t d = 0; d < domains.Count(); ++d)
        {
            const JsonReader item = domains.Item(d);
            RunLogDomain domain;
            const bool known = SaveIdTo(item.String("biome", SaveFiles::kMaxIdLength), domain.biome);
            domain.roomsCleared = (int)item.Int("roomsCleared", 0, L::kMaxCount);
            domain.seconds = item.Float("seconds", 0.f, L::kMaxSeconds);
            domain.bossDefeated = item.Bool("bossDefeated");
            if (known) e.domains.push_back(domain);
        }
        e.domainReached = (int)run.Int("domainReached", 1, L::kMaxRouteEntries);
        e.seconds = run.Float("seconds", 0.f, L::kMaxSeconds);
        e.roomsCleared = (int)run.Int("roomsCleared", 0, L::kMaxCount);
        e.enemiesKilled = (int)run.Int("enemiesKilled", 0, L::kMaxCount);
        e.elitesSlain = (int)run.Int("elitesSlain", 0, L::kMaxCount);
        e.bossesDefeated = (int)run.Int("bossesDefeated", 0, L::kMaxCount);
        e.gold = (int)run.Int("gold", 0, L::kMaxCount);
        e.echoes = (int)run.Int("echoes", 0, L::kMaxCount);
        const JsonReader build = run.Object("build");
        e.level = (int)build.Int("level", 1, L::kMaxLevel);
        e.maxHealth = build.Float("maxHealth", 0.f, (float)L::kMaxStat);
        const JsonReader abilities = build.Array("abilities", L::kMaxAbilitySlots);
        for (std::size_t a = 0; a < abilities.Count(); ++a)
        {
            const JsonReader item = abilities.Item(a);
            RunLogAbility ability;
            const bool known = SaveIdTo(item.String("id", SaveFiles::kMaxIdLength), ability.ability);
            ability.level = (int)item.Int("level", 1, L::kMaxLevel);
            if (known) e.abilities.push_back(ability);
        }
        ReadSaveIdList<RelicType>(build.Array("relics", L::kMaxRelics), SaveFiles::kMaxIdLength,
                                  [&](RelicType relic) { e.relics.push_back(relic); });
        e.ending = run.String("ending", SaveFiles::kMaxTextLength);
        JsonReader death;
        e.hasDeath = run.OptionalObject("death", death);
        if (e.hasDeath) e.death = DeathFromJson(death);
        if (!errors.Ok()) break;
        log._runs.push_back(e);
    }
    if (log._wins > log._totalRuns) root.Fail("more wins than runs");
    if (!errors.Ok()) { error = errors.first; return false; }
    out = std::move(log);
    return true;
}
