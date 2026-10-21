#include "RunCheckpoint.h"
#include "SaveFiles.h"
#include "SaveStore.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

namespace
{
    // A small but complete run: three rooms in a row (entrance, a cleared
    // event room, the boss), a curse, Zeph's stock.
    RunCheckpoint Valid()
    {
        RunCheckpoint c;
        c.runId = 7;
        c.active = true;
        c.character.cls = PlayerClass::Hunter;
        c.character.appearance = "Hero03";
        c.character.stats.Set("health", JsonValue::MakeFloat(7.5f));
        c.character.stats.Set("maxHealth", JsonValue::MakeFloat(10.f));
        c.character.stats.Set("level", JsonValue::MakeInt(3));
        c.character.stats.Set("gold", JsonValue::MakeInt(137));
        c.character.stats.Set("mana", JsonValue::MakeInt(5));
        c.character.stats.Set("maxMana", JsonValue::MakeInt(10));
        c.character.stats.Set("speed", JsonValue::MakeFloat(1.25f));
        c.character.stats.Set("shopWagerArmed", JsonValue::MakeBool(true));
        c.character.abilities = { { AbilityType::PiercingShot, 2 }, { AbilityType::None, 1 },
                                  { AbilityType::None, 1 }, { AbilityType::ArrowStorm, 1 } };
        c.character.relics = { RelicType::LastStand, RelicType::Deadeye };
        c.biome = Biome::Caverns;
        c.worldZone = 1;
        c.completedBiomes = { Biome::Forest };
        c.chosenNodes = { 2 };
        c.worldMapGenerated = true;
        c.sessionCompleted = { Biome::Forest };
        c.sessionChosen = { Biome::Caverns };
        c.sessionNodes = { 2 };
        c.playerDamageMult = 1.1f;
        c.timer = 321.5f;
        c.roomsCleared = 9;
        c.domainLog = { { Biome::Forest, 6, 300.25f, true } };
        c.domainStartRooms = 6;
        c.domainStartSeconds = 300.25f;
        c.gameTimer = 400.25f;
        c.seed = 4000000000u;
        ModifierCheckpoint curse;
        curse.label = "Cursed Cache";
        curse.enemyDmgMult = 1.3f;
        curse.tintRgba = 0xEBB45AFFu;
        curse.active = true;
        curse.roomsRemaining = 2;
        curse.curse = true;
        c.modifiers = { curse };
        c.hasDungeon = true;
        DungeonCheckpoint& d = c.dungeon;
        RoomCheckpoint entrance, event, boss;
        entrance.col = 3; entrance.row = 3; entrance.east = true; entrance.layout = "Caverns_Entrance";
        event.col = 4; event.row = 3; event.west = true; event.east = true; event.layout = "Caverns_A"; event.depth = 1;
        event.hasProgress = true; event.cleared = true; event.visited = true; event.special = 2;
        event.specialOptions[0] = 0; event.specialOptions[1] = 1; event.specialOptions[2] = 2;
        boss.col = 5; boss.row = 3; boss.west = true; boss.type = 5; boss.layout = "Caverns_Boss"; boss.depth = 2;
        d.rooms = { entrance, event, boss };
        d.start = 0; d.boss = 2; d.current = 1; d.entrySide = 3; d.entryX = 960.f; d.entryY = 540.f;
        c.hasShop = true;
        c.shop.zone = 1;
        c.shop.rerollCost = 25;
        ShopItemCheckpoint card;
        card.isAbility = true; card.ability = AbilityType::FireBolt; card.price = 80;
        ShopItemCheckpoint ware;
        ware.ware = 3; ware.price = 40;
        c.shop.items = { card, ware };
        return c;
    }

    // The document with one member replaced (or removed, for a null value),
    // following object keys and array indices.
    JsonValue With(const JsonValue& node, const std::vector<std::string>& path, const JsonValue* value, std::size_t depth = 0)
    {
        if (depth + 1 == path.size() && node.IsObject() && !value)
        {
            JsonValue copy = node;
            copy.Remove(path[depth]);
            return copy;
        }
        if (depth == path.size()) return *value;
        if (node.IsArray())
        {
            const std::size_t index = std::stoul(path[depth]);
            JsonValue copy = JsonValue::MakeArray();
            for (std::size_t i = 0; i < node.Size(); ++i)
                copy.Push(i == index ? With(node.At(i), path, value, depth + 1) : node.At(i));
            return copy;
        }
        JsonValue copy = node;
        const JsonValue* child = node.Find(path[depth]);
        copy.Set(path[depth], With(child ? *child : JsonValue(), path, value, depth + 1));
        return copy;
    }

    bool Accepts(const JsonValue& document)
    {
        RunCheckpoint out;
        std::string error;
        return RunCheckpointFromJson(document, out, error);
    }

    std::string RejectionOf(const std::vector<std::string>& path, const JsonValue& value)
    {
        RunCheckpoint out;
        std::string error;
        if (RunCheckpointFromJson(With(RunCheckpointToJson(Valid()), path, &value), out, error)) return "accepted";
        return error;
    }

    bool Rejects(const std::vector<std::string>& path, const JsonValue& value) { return RejectionOf(path, value) != "accepted"; }
    bool RejectsMissing(const std::vector<std::string>& path)
    {
        return !Accepts(With(RunCheckpointToJson(Valid()), path, nullptr));
    }

    JsonValue Int(long long v) { return JsonValue::MakeInt(v); }
    JsonValue Text(const char* v) { return JsonValue::MakeString(v); }
    JsonValue Flag(bool v) { return JsonValue::MakeBool(v); }
}

int main()
{
    // ── Round trip ───────────────────────────────────────────────────────────
    const RunCheckpoint valid = Valid();
    const JsonValue doc = RunCheckpointToJson(valid);
    RunCheckpoint back;
    std::string error;
    CHECK(RunCheckpointFromJson(doc, back, error));
    std::string first, second;
    CHECK(WriteJson(doc, first) && WriteJson(RunCheckpointToJson(back), second) && first == second);
    CHECK(back.runId == 7 && back.character.cls == PlayerClass::Hunter && back.character.appearance == "Hero03");
    CHECK(back.character.abilities.size() == 4 && back.character.abilities[0].ability == AbilityType::PiercingShot &&
          back.character.abilities[0].level == 2 && back.character.abilities[1].ability == AbilityType::None);
    CHECK(back.character.relics.size() == 2 && back.character.relics[1] == RelicType::Deadeye);
    CHECK(back.seed == 4000000000u && back.timer == 321.5f && back.modifiers.size() == 1 && back.modifiers[0].curse);
    CHECK(back.modifiers[0].tintRgba == 0xEBB45AFFu && back.modifiers[0].enemyDmgMult == 1.3f);
    CHECK(back.hasDungeon && back.dungeon.rooms.size() == 3 && back.dungeon.rooms[1].hasProgress &&
          !back.dungeon.rooms[0].hasProgress && back.dungeon.rooms[1].specialOptions[2] == 2);
    CHECK(back.hasShop && back.shop.items[0].ability == AbilityType::FireBolt && back.shop.items[1].ware == 3);
    CHECK(back.domainLog.size() == 1 && back.domainLog[0].bossDefeated && back.domainStartRooms == 6 &&
          back.domainStartSeconds == 300.25f);
    CHECK(first.find("\"PiercingShot\"") != std::string::npos && first.find("\"Caverns\"") != std::string::npos);

    // An ended run stores only its id.
    {
        RunCheckpoint ended;
        ended.runId = 9;
        const JsonValue endedDoc = RunCheckpointToJson(ended);
        CHECK(!endedDoc.Find("character") && Accepts(endedDoc));
        CHECK(RunCheckpointFromJson(endedDoc, back, error) && !back.active && back.runId == 9);
    }

    // ── Every rule rejects the whole file ────────────────────────────────────
    CHECK(Rejects({ "schema" }, Int(2)));
    CHECK(Rejects({ "runId" }, Int(0)));
    CHECK(Rejects({ "active" }, Int(1)));
    CHECK(Rejects({ "character", "class" }, Text("Necromancer")));
    CHECK(RejectsMissing({ "character", "stats" }));
    CHECK(Rejects({ "character", "stats", "health" }, Int(-1)));
    CHECK(Rejects({ "character", "stats", "maxHealth" }, Int(0)));
    CHECK(Rejects({ "character", "stats", "level" }, Int(0)));
    CHECK(Rejects({ "character", "stats", "level" }, JsonValue::MakeFloat(2.5f)));
    CHECK(Rejects({ "character", "stats", "gold" }, Int(-5)));
    CHECK(Rejects({ "character", "stats", "maxAbilitySlots" }, Int(99)));
    CHECK(Rejects({ "character", "stats", "speed" }, JsonValue::MakeDouble(1e300)));
    CHECK(Rejects({ "character", "stats", "speed" }, Text("fast")));
    CHECK(Rejects({ "character", "stats", "speed" }, JsonValue::MakeArray()));
    CHECK(Rejects({ "character", "abilities", "0", "level" }, Int(0)));
    CHECK(Rejects({ "character", "relics", "1" }, Text("LastStand")));            // listed twice
    CHECK(Rejects({ "run", "biome" }, Text("Atlantis")));
    CHECK(Rejects({ "run", "completedBiomes", "0" }, Text("Atlantis")));
    CHECK(Rejects({ "run", "worldZone" }, Int(999)));
    CHECK(Rejects({ "run", "playerDamageMult" }, JsonValue::MakeFloat(-1.f)));
    CHECK(Rejects({ "run", "timer" }, JsonValue::MakeDouble(1e12)));
    CHECK(Rejects({ "run", "seed" }, Int(4294967296LL)));
    CHECK(Rejects({ "run", "enemiesKilled" }, Int(-1)));
    CHECK(Rejects({ "run", "modifiers", "0", "tint" }, Int(4294967296LL)));
    CHECK(Rejects({ "run", "modifiers", "0", "shopContract" }, Int(4)));
    CHECK(RejectsMissing({ "run", "secondWind" }));
    CHECK(Rejects({ "run", "domainStart", "seconds" }, JsonValue::MakeFloat(999.f)));   // after the run's own time
    CHECK(Rejects({ "run", "domainLog", "0", "biome" }, Text("Atlantis")));
    {
        JsonValue many = JsonValue::MakeArray();
        const JsonValue one = doc.Find("run")->Find("modifiers")->At(0);
        for (int i = 0; i <= RunCheckpointLimits::kMaxModifiers; ++i) many.Push(one);
        CHECK(Rejects({ "run", "modifiers" }, many));
    }
    CHECK(Rejects({ "location", "crossroads" }, JsonValue::MakeArray()));        // must hold three roads
    CHECK(Rejects({ "location", "crossroads", "0", "boon" }, Int(RunCheckpointLimits::kPathBoons + 1)));
    CHECK(!Rejects({ "location", "crossroads", "0", "boon" }, Int(RunCheckpointLimits::kPathBoons)));   // no reward yet
    CHECK(Rejects({ "location", "atWorldMap" }, Flag(true)));                     // a world map run has no dungeon
    CHECK(RejectsMissing({ "dungeon" }));                                         // ...and a dungeon run needs one

    // The room graph.
    CHECK(Rejects({ "dungeon", "rooms" }, JsonValue::MakeArray()));
    CHECK(Rejects({ "dungeon", "rooms", "0", "col" }, Int(RunCheckpointLimits::kGridSize)));
    CHECK(Rejects({ "dungeon", "rooms", "1", "col" }, Int(3)));                   // two rooms in one cell
    CHECK(Rejects({ "dungeon", "rooms", "0", "type" }, Int(RunCheckpointLimits::kRoomTypes)));
    CHECK(Rejects({ "dungeon", "rooms", "0", "layout" }, Text("")));
    CHECK(Rejects({ "dungeon", "rooms", "0", "doors", "north" }, Flag(true)));   // a door to nowhere
    CHECK(Rejects({ "dungeon", "rooms", "2", "doors", "west" }, Flag(false)));   // one-sided door, boss unreachable
    CHECK(Rejects({ "dungeon", "current" }, Int(3)));
    CHECK(Rejects({ "dungeon", "boss" }, Int(0)));                               // the entrance can't be the boss
    CHECK(Rejects({ "dungeon", "key" }, Int(-2)));
    CHECK(Rejects({ "dungeon", "entrySide" }, Int(RunCheckpointLimits::kDoorSides)));
    CHECK(Rejects({ "dungeon", "rooms", "1", "progress", "special" }, Int(RunCheckpointLimits::kRoomSpecials)));
    CHECK(Rejects({ "dungeon", "rooms", "1", "progress", "specialOptions" }, JsonValue::MakeArray()));
    {
        // A fourth room off to the side with no door to it can't be reached.
        JsonValue rooms = *doc.Find("dungeon")->Find("rooms");
        JsonValue island = rooms.At(0);
        island.Set("col", Int(0));
        island.Set("row", Int(0));
        island.Set("doors", *rooms.At(2).Find("doors"));
        JsonValue islandDoors = JsonValue::MakeObject();
        for (const char* side : { "north", "south", "east", "west" }) islandDoors.Set(side, Flag(false));
        island.Set("doors", islandDoors);
        rooms.Push(island);
        CHECK(RejectionOf({ "dungeon", "rooms" }, rooms).find("reached") != std::string::npos);
    }

    // Zeph's stock.
    CHECK(Rejects({ "shop", "items", "1", "ware" }, Int(RunCheckpointLimits::kShopWares)));
    CHECK(Rejects({ "shop", "items", "0", "price" }, Int(-1)));
    CHECK(Rejects({ "shop", "dailyDeal" }, Int(RunCheckpointLimits::kMaxShopItems)));

    // Content removed since the save: skipped, not fatal.
    {
        JsonValue changed = With(doc, { "character", "relics", "0" }, &(const JsonValue&)Text("RelicFromAnOldBuild"));
        changed = With(changed, { "character", "abilities", "0", "id" }, &(const JsonValue&)Text("RemovedSpell"));
        changed = With(changed, { "shop", "items", "0", "ability" }, &(const JsonValue&)Text("RemovedSpell"));
        CHECK(RunCheckpointFromJson(changed, back, error));
        CHECK(back.character.relics.size() == 1 && back.character.relics[0] == RelicType::Deadeye);
        CHECK(back.character.abilities[0].ability == AbilityType::None);
        CHECK(back.shop.items[0].ability == AbilityType::None && back.shop.items[0].unavailable);
    }

    // ── On disk: a tampered checkpoint is rejected and the backup restored ───
    {
        SaveFile file(SaveFiles::RunCheckpointPath(), SaveKind::RunCheckpoint, SaveFiles::kMaxRunCheckpointBytes);
        std::error_code ignored;
        std::filesystem::remove_all("Saves", ignored);
        auto verify = [](const JsonValue& d, std::string& why) { RunCheckpoint c; return RunCheckpointFromJson(d, c, why); };
        RunCheckpoint earlier = Valid();
        earlier.dungeon.current = 0;
        CHECK(file.Save(RunCheckpointToJson(earlier), verify));
        CHECK(file.Save(doc, verify));
        RunCheckpoint broken = Valid();
        broken.dungeon.rooms[2].west = false;                                     // unloadable: never written
        CHECK(!file.Save(RunCheckpointToJson(broken), verify));
        std::string bytes;
        CHECK(SaveFile::ReadBounded(file.Path(), SaveFiles::kMaxRunCheckpointBytes, bytes) == SaveFile::ReadStatus::Ok);
        bytes[bytes.size() - 5] ^= 0x10;
        { std::ofstream(file.Path(), std::ios::binary | std::ios::trunc) << bytes; }
        RunCheckpoint loaded;
        CHECK(file.Load([&](const JsonValue& d, std::string& why) { return RunCheckpointFromJson(d, loaded, why); }) ==
              SaveLoadResult::RestoredBackup);
        CHECK(loaded.active && loaded.dungeon.current == 0);
        std::filesystem::remove_all("Saves", ignored);
    }

    std::puts("Run checkpoint tests passed: round trip, ended runs, every range/type/name rule, room graph "
              "(cells, doors, reachability), shop, removed content skipped, tampered file -> backup");
    return 0;
}
