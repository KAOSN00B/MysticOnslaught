#include "RunLog.h"
#include "SaveFiles.h"
#include "SaveStore.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

namespace
{
    RunLogEntry Death(long long runId)
    {
        RunLogEntry e;
        e.runId = runId;
        e.endedAt = 1760700000;
        e.outcome = RunOutcome::Death;
        e.cls = PlayerClass::Rogue;
        e.skin = "Hero06";
        e.ascension = 1;
        e.domains = { { Biome::Forest, 6, 410.5f, true }, { Biome::Caverns, 3, 200.25f, false } };
        e.domainReached = 2;
        e.seconds = 610.75f;
        e.roomsCleared = 9;
        e.enemiesKilled = 120;
        e.elitesSlain = 2;
        e.bossesDefeated = 1;
        e.gold = 230;
        e.echoes = 41;
        e.level = 6;
        e.maxHealth = 9.5f;
        e.abilities = { { AbilityType::FanOfKnives, 2 }, { AbilityType::DeathMark, 1 } };
        e.relics = { RelicType::LastStand, RelicType::Bloodlust };
        e.ending = "Fell to an elite in Caverns";
        e.hasDeath = true;
        e.death.cause = DamageSource::FromEnemy("Ogre", true, false);
        e.death.burning = true;
        e.death.inEliteCage = true;
        e.death.roomType = 1;
        e.death.roomDepth = 3;
        e.death.activeModifiers = { "Cursed Cache" };
        e.death.roomAffix = "Volatile";
        e.death.secondsInRoom = 12.5f;
        e.death.secondsSinceDash = 3.25f;
        e.death.goldCarried = 230;
        return e;
    }

    // The document with one member of the newest record replaced.
    JsonValue WithRunField(const RunLog& log, const char* section, const char* key, const JsonValue& value)
    {
        JsonValue doc = log.ToJson();
        JsonValue runs = *doc.Find("runs");
        JsonValue first = runs.At(0);
        if (section)
        {
            JsonValue part = *first.Find(section);
            part.Set(key, value);
            first.Set(section, part);
        }
        else
            first.Set(key, value);
        JsonValue rebuilt = JsonValue::MakeArray();
        rebuilt.Push(first);
        for (std::size_t i = 1; i < runs.Size(); ++i) rebuilt.Push(runs.At(i));
        doc.Set("runs", rebuilt);
        return doc;
    }

    bool Accepts(const JsonValue& doc)
    {
        RunLog out;
        std::string error;
        return RunLog::FromJson(doc, out, error);
    }
}

int main()
{
    // ── Round trip, newest first, totals ─────────────────────────────────────
    RunLog log;
    log.Add(Death(1));
    RunLogEntry win = Death(2);
    win.outcome = RunOutcome::Victory;
    win.hasDeath = false;
    log.Add(win);
    CHECK(log.TotalRuns() == 2 && log.Wins() == 1 && log.Runs()[0].runId == 2);

    RunLog back;
    std::string error;
    CHECK(RunLog::FromJson(log.ToJson(), back, error));
    std::string a, b;
    CHECK(WriteJson(log.ToJson(), a) && WriteJson(back.ToJson(), b) && a == b);
    const RunLogEntry& death = back.Runs()[1];
    CHECK(death.outcome == RunOutcome::Death && death.cls == PlayerClass::Rogue && death.skin == "Hero06");
    CHECK(death.domains.size() == 2 && death.domains[0].bossDefeated && death.domains[1].seconds == 200.25f);
    CHECK(death.abilities.size() == 2 && death.abilities[1].ability == AbilityType::DeathMark);
    CHECK(death.relics.size() == 2 && death.relics[0] == RelicType::LastStand);
    CHECK(death.hasDeath && death.death.cause.kind == DamageSourceKind::Enemy && death.death.cause.id == "Ogre" &&
          death.death.cause.elite && death.death.burning && !death.death.poisoned && death.death.inEliteCage);
    CHECK(death.death.roomAffix == "Volatile" && death.death.secondsInRoom == 12.5f && death.death.bossHealthFraction == -1.f);
    CHECK(death.death.roomDepth == 3 && death.death.activeModifiers.size() == 1 && death.death.activeModifiers[0] == "Cursed Cache");
    CHECK(!back.Runs()[0].hasDeath && back.Runs()[0].outcome == RunOutcome::Victory);
    CHECK(a.find("\"cause\":{\"kind\":\"enemy\",\"id\":\"Ogre\"") != std::string::npos);

    // Statuses credit whoever applied them.
    {
        const DamageSource burn = DamageSource::FromStatus(DamageNames::kBurn, DamageSource::FromEnemy("Infernal", true, false));
        CHECK(burn.kind == DamageSourceKind::Status && burn.id == "Burn" && burn.by == "Infernal" && burn.elite);
    }

    // ── Only the newest records are kept; totals keep counting ───────────────
    {
        RunLog many;
        for (int i = 1; i <= RunLog::kMaxRuns + 25; ++i) many.Add(Death(i));
        CHECK((int)many.Runs().size() == RunLog::kMaxRuns && many.TotalRuns() == RunLog::kMaxRuns + 25);
        CHECK(many.Runs().front().runId == RunLog::kMaxRuns + 25 && many.Runs().back().runId == 26);
        RunLog reread;
        CHECK(RunLog::FromJson(many.ToJson(), reread, error) && reread.TotalRuns() == many.TotalRuns());
    }

    // ── Validation ───────────────────────────────────────────────────────────
    CHECK(!Accepts(WithRunField(log, nullptr, "outcome", JsonValue::MakeString("teleported"))));
    CHECK(!Accepts(WithRunField(log, nullptr, "seconds", JsonValue::MakeFloat(-1.f))));
    CHECK(!Accepts(WithRunField(log, nullptr, "enemiesKilled", JsonValue::MakeInt(-3))));
    CHECK(!Accepts(WithRunField(log, nullptr, "ending", JsonValue::MakeString(std::string(SaveFiles::kMaxTextLength + 1, 'x')))));
    CHECK(!Accepts(WithRunField(log, "build", "level", JsonValue::MakeInt(0))));
    CHECK(!Accepts(WithRunField(log, nullptr, "runId", JsonValue::MakeInt(0))));
    {
        JsonValue doc = log.ToJson();
        JsonValue totals = *doc.Find("totals");
        totals.Set("wins", JsonValue::MakeInt(5));                      // more wins than runs
        doc.Set("totals", totals);
        CHECK(!Accepts(doc));
        totals.Set("runs", JsonValue::MakeInt(1));                      // fewer runs than records
        totals.Set("wins", JsonValue::MakeInt(0));
        doc.Set("totals", totals);
        CHECK(!Accepts(doc));
    }
    {
        // A record whose class or domain was removed since keeps its numbers.
        JsonValue doc = WithRunField(log, nullptr, "class", JsonValue::MakeString("Necromancer"));
        CHECK(Accepts(doc));
        JsonValue deathDoc = log.ToJson();
        JsonValue runs = *deathDoc.Find("runs");
        JsonValue second = runs.At(1);
        JsonValue cause = *second.Find("death")->Find("cause");
        cause.Set("kind", JsonValue::MakeString("gremlins"));
        JsonValue deathObject = *second.Find("death");
        deathObject.Set("cause", cause);
        second.Set("death", deathObject);
        JsonValue rebuilt = JsonValue::MakeArray();
        rebuilt.Push(runs.At(0));
        rebuilt.Push(second);
        deathDoc.Set("runs", rebuilt);
        CHECK(!Accepts(deathDoc));                                       // an unknown cause kind is not a name change
    }

    // ── On disk: signed, and a tampered log falls back to its backup ─────────
    {
        std::error_code ignored;
        std::filesystem::remove_all("Saves", ignored);
        SaveFile file(SaveFiles::RunLogPath(), SaveKind::RunLog, SaveFiles::kMaxRunLogBytes);
        auto verify = [](const JsonValue& doc, std::string& why) { RunLog check; return RunLog::FromJson(doc, check, why); };
        RunLog first;
        first.Add(Death(1));
        CHECK(file.Save(first.ToJson(), verify));
        CHECK(file.Save(log.ToJson(), verify));
        std::string bytes;
        CHECK(SaveFile::ReadBounded(file.Path(), SaveFiles::kMaxRunLogBytes, bytes) == SaveFile::ReadStatus::Ok);
        CHECK(bytes.find("Ogre") == std::string::npos);                    // not plain text
        bytes[SaveFile::kHeaderSize + 3] ^= 0x01;
        { std::ofstream(file.Path(), std::ios::binary | std::ios::trunc) << bytes; }
        RunLog loaded;
        CHECK(file.Load([&](const JsonValue& doc, std::string& why) { return RunLog::FromJson(doc, loaded, why); }) ==
              SaveLoadResult::RestoredBackup);
        CHECK(loaded.TotalRuns() == 1);
        std::filesystem::remove_all("Saves", ignored);
    }

    std::puts("Run log tests passed: round trip, newest-first trimming with running totals, death cause and context, "
              "validation, removed content kept, tampered log -> backup");
    return 0;
}
