#include "RunCheckpoint.h"
#include "SaveFiles.h"
#include "SaveIds.h"

#include <cstdint>
#include <iterator>
#include <limits>
#include <set>

namespace
{
    namespace L = RunCheckpointLimits;

    constexpr std::size_t kMaxRoomLayoutLength = SaveFiles::kMaxTextLength;   // authored room ids
    constexpr long long   kMaxSeed = std::numeric_limits<std::uint32_t>::max();
    constexpr long long   kMaxRgba = std::numeric_limits<std::uint32_t>::max();
    constexpr int         kNoSelection = -1;   // "none" for indices, mechanics and options

    // Grid offsets for the four doors (north, south, east, west), and the
    // index of the door that faces back.
    struct DoorStep { int dCol, dRow, opposite; };
    constexpr DoorStep kDoorSteps[] = { { 0, -1, 1 }, { 0, 1, 0 }, { 1, 0, 3 }, { -1, 0, 2 } };

    bool HasDoor(const RoomCheckpoint& room, int door)
    {
        const bool doors[] = { room.north, room.south, room.east, room.west };
        return doors[door];
    }

    JsonValue BiomeList(const std::vector<Biome>& biomes)
    {
        JsonValue list = JsonValue::MakeArray();
        for (Biome biome : biomes)
            if (const char* id = SaveIdOf(biome)) list.Push(JsonValue::MakeString(id));
        return list;
    }

    JsonValue IntList(const std::vector<int>& values)
    {
        JsonValue list = JsonValue::MakeArray();
        for (int value : values) list.Push(JsonValue::MakeInt(value));
        return list;
    }

    // A route list: every name must be known (the run can't continue on a
    // domain this build doesn't have).
    std::vector<Biome> ReadBiomes(const JsonReader& list)
    {
        std::vector<Biome> biomes;
        for (std::size_t i = 0; i < list.Count(); ++i)
        {
            const JsonReader item = list.Item(i);
            Biome biome{};
            if (!SaveIdTo(item.AsString(SaveFiles::kMaxIdLength), biome)) { item.Fail("unknown domain"); break; }
            biomes.push_back(biome);
        }
        return biomes;
    }

    std::vector<int> ReadInts(const JsonReader& list, int min, int max)
    {
        std::vector<int> values;
        for (std::size_t i = 0; i < list.Count(); ++i) values.push_back((int)list.Item(i).AsInt(min, max));
        return values;
    }

    Biome ReadBiome(const JsonReader& object, const char* key)
    {
        Biome biome{};
        const std::string id = object.String(key, SaveFiles::kMaxIdLength);
        if (object.Ok() && !SaveIdTo(id, biome)) object.Fail(std::string(key) + ": unknown domain");
        return biome;
    }

    // Character numbers: finite, bounded, and exact ranges for the ones that
    // carry meaning. Names not listed only need a sane magnitude.
    struct StatRange { const char* name; double min; double max; bool whole; };
    const StatRange kStatRanges[] = {
        { "health",          0.0, L::kMaxStat,          false },
        { "maxHealth",       1.0, L::kMaxStat,          false },
        { "mana",            0.0, L::kMaxStat,          true  },
        { "maxMana",         0.0, L::kMaxStat,          true  },
        { "defense",         0.0, L::kMaxStat,          false },
        { "ironcladDefense", 0.0, L::kMaxStat,          false },
        { "critChance",      0.0, L::kMaxChance,        false },
        { "critDamage",      0.0, L::kMaxStat,          false },
        { "level",           1.0, L::kMaxLevel,         true  },
        { "exp",             0.0, (double)L::kMaxCount, true  },
        { "expToNextLevel",  0.0, (double)L::kMaxCount, true  },
        { "gold",            0.0, (double)L::kMaxCount, true  },
        { "cells",           0.0, (double)L::kMaxCount, true  },
        { "learnedCount",    0.0, L::kMaxAbilitySlots,  true  },
        { "maxAbilitySlots", 1.0, L::kMaxAbilitySlots,  true  },
    };

    void ValidateStats(const JsonReader& stats)
    {
        for (std::size_t i = 0; i < stats.Count(); ++i)
        {
            const JsonReader value = stats.ValueAt(i);
            const std::string& name = stats.KeyAt(i);
            if (name.size() > SaveFiles::kMaxIdLength) { value.Fail("stat name too long"); return; }
            const StatRange* range = nullptr;
            for (const StatRange& r : kStatRanges)
                if (name == r.name) range = &r;
            if (range && range->whole) value.AsInt((long long)range->min, (long long)range->max);
            else if (range)            value.AsDouble(range->min, range->max);
            else if (!value.Raw()->IsBool()) value.AsDouble(-L::kMaxStat, L::kMaxStat);   // a flag, or a sane number
        }
    }
}

// ── Writing ──────────────────────────────────────────────────────────────────

JsonValue RunCheckpointToJson(const RunCheckpoint& c)
{
    JsonValue doc = JsonValue::MakeObject();
    doc.Set("schema", JsonValue::MakeInt(SaveFiles::kRunCheckpointSchema));
    doc.Set("runId", JsonValue::MakeInt(c.runId));
    doc.Set("active", JsonValue::MakeBool(c.active));
    if (!c.active) return doc;

    JsonValue& character = doc.Set("character", JsonValue::MakeObject());
    character.Set("class", JsonValue::MakeString(SaveIdOf(c.character.cls) ? SaveIdOf(c.character.cls) : ""));
    character.Set("appearance", JsonValue::MakeString(c.character.appearance));
    character.Set("stats", c.character.stats);
    JsonValue& abilities = character.Set("abilities", JsonValue::MakeArray());
    for (const CharacterCheckpoint::Slot& slot : c.character.abilities)
    {
        JsonValue& item = abilities.Push(JsonValue::MakeObject());
        if (const char* id = SaveIdOf(slot.ability)) item.Set("id", JsonValue::MakeString(id));
        item.Set("level", JsonValue::MakeInt(slot.level));
    }
    JsonValue& relics = character.Set("relics", JsonValue::MakeArray());
    for (RelicType relic : c.character.relics)
        if (const char* id = SaveIdOf(relic)) relics.Push(JsonValue::MakeString(id));

    JsonValue& run = doc.Set("run", JsonValue::MakeObject());
    run.Set("biome", JsonValue::MakeString(SaveIdOf(c.biome) ? SaveIdOf(c.biome) : ""));
    run.Set("worldZone", JsonValue::MakeInt(c.worldZone));
    run.Set("completedBiomes", BiomeList(c.completedBiomes));
    run.Set("chosenNodes", IntList(c.chosenNodes));
    JsonValue& session = run.Set("session", JsonValue::MakeObject());
    session.Set("worldMapGenerated", JsonValue::MakeBool(c.worldMapGenerated));
    session.Set("completed", BiomeList(c.sessionCompleted));
    session.Set("chosen", BiomeList(c.sessionChosen));
    session.Set("nodes", IntList(c.sessionNodes));
    run.Set("ascensionTier", JsonValue::MakeInt(c.ascensionTier));
    run.Set("ascensionRecorded", JsonValue::MakeBool(c.ascensionRecorded));
    run.Set("playerDamageMult", JsonValue::MakeFloat(c.playerDamageMult));
    run.Set("enemyHealthMult", JsonValue::MakeFloat(c.enemyHealthMult));
    run.Set("enemyDamageMult", JsonValue::MakeFloat(c.enemyDamageMult));
    run.Set("wagerTier", JsonValue::MakeInt(c.wagerTier));
    run.Set("curseShrineUsed", JsonValue::MakeBool(c.curseShrineUsed));
    run.Set("wagerAccessGranted", JsonValue::MakeBool(c.wagerAccessGranted));
    run.Set("secondWind", JsonValue::MakeBool(c.secondWind));
    run.Set("pendingRelicChoices", JsonValue::MakeInt(c.pendingRelicChoices));
    run.Set("enemiesKilled", JsonValue::MakeInt(c.enemiesKilled));
    run.Set("bossesDefeated", JsonValue::MakeInt(c.bossesDefeated));
    run.Set("roomsCleared", JsonValue::MakeInt(c.roomsCleared));
    run.Set("elitesSlain", JsonValue::MakeInt(c.elitesSlain));
    run.Set("timer", JsonValue::MakeFloat(c.timer));
    run.Set("gameTimer", JsonValue::MakeFloat(c.gameTimer));
    run.Set("seed", JsonValue::MakeInt(c.seed));
    JsonValue& domainLog = run.Set("domainLog", JsonValue::MakeArray());
    for (const RunLogDomain& d : c.domainLog)
    {
        if (!SaveIdOf(d.biome)) continue;
        JsonValue& item = domainLog.Push(JsonValue::MakeObject());
        item.Set("biome", JsonValue::MakeString(SaveIdOf(d.biome)));
        item.Set("roomsCleared", JsonValue::MakeInt(d.roomsCleared));
        item.Set("seconds", JsonValue::MakeFloat(d.seconds));
        item.Set("bossDefeated", JsonValue::MakeBool(d.bossDefeated));
    }
    JsonValue& domainStart = run.Set("domainStart", JsonValue::MakeObject());
    domainStart.Set("rooms", JsonValue::MakeInt(c.domainStartRooms));
    domainStart.Set("seconds", JsonValue::MakeFloat(c.domainStartSeconds));
    JsonValue& modifiers = run.Set("modifiers", JsonValue::MakeArray());
    for (const ModifierCheckpoint& m : c.modifiers)
    {
        JsonValue& item = modifiers.Push(JsonValue::MakeObject());
        item.Set("label", JsonValue::MakeString(m.label));
        item.Set("enemyHpMult", JsonValue::MakeFloat(m.enemyHpMult));
        item.Set("enemyDmgMult", JsonValue::MakeFloat(m.enemyDmgMult));
        item.Set("goldMult", JsonValue::MakeFloat(m.goldMult));
        item.Set("xpMult", JsonValue::MakeFloat(m.xpMult));
        item.Set("tint", JsonValue::MakeInt(m.tintRgba));
        item.Set("active", JsonValue::MakeBool(m.active));
        item.Set("roomsRemaining", JsonValue::MakeInt(m.roomsRemaining));
        item.Set("shopContract", JsonValue::MakeInt(m.shopContract));
        item.Set("curse", JsonValue::MakeBool(m.curse));
    }

    JsonValue& location = doc.Set("location", JsonValue::MakeObject());
    location.Set("atWorldMap", JsonValue::MakeBool(c.atWorldMap));
    location.Set("atCrossroads", JsonValue::MakeBool(c.atCrossroads));
    JsonValue& doors = location.Set("crossroads", JsonValue::MakeArray());
    for (const CrossroadsDoorCheckpoint& door : c.crossroads)
    {
        JsonValue& item = doors.Push(JsonValue::MakeObject());
        item.Set("open", JsonValue::MakeBool(door.open));
        if (door.open && SaveIdOf(door.biome)) item.Set("biome", JsonValue::MakeString(SaveIdOf(door.biome)));
        item.Set("boon", JsonValue::MakeInt(door.boon));
    }

    if (c.hasDungeon)
    {
        const DungeonCheckpoint& d = c.dungeon;
        JsonValue& dungeon = doc.Set("dungeon", JsonValue::MakeObject());
        dungeon.Set("start", JsonValue::MakeInt(d.start));
        dungeon.Set("boss", JsonValue::MakeInt(d.boss));
        dungeon.Set("key", JsonValue::MakeInt(d.key));
        dungeon.Set("current", JsonValue::MakeInt(d.current));
        dungeon.Set("entrySide", JsonValue::MakeInt(d.entrySide));
        dungeon.Set("entryX", JsonValue::MakeFloat(d.entryX));
        dungeon.Set("entryY", JsonValue::MakeFloat(d.entryY));
        dungeon.Set("hasMagicGem", JsonValue::MakeBool(d.hasMagicGem));
        dungeon.Set("magicGemSpawned", JsonValue::MakeBool(d.magicGemSpawned));
        dungeon.Set("magicGemCollected", JsonValue::MakeBool(d.magicGemCollected));
        dungeon.Set("bossBarrierUnlocked", JsonValue::MakeBool(d.bossBarrierUnlocked));
        dungeon.Set("chestSpawned", JsonValue::MakeBool(d.chestSpawned));
        dungeon.Set("chestBroken", JsonValue::MakeBool(d.chestBroken));
        JsonValue& rooms = dungeon.Set("rooms", JsonValue::MakeArray());
        for (const RoomCheckpoint& r : d.rooms)
        {
            JsonValue& room = rooms.Push(JsonValue::MakeObject());
            room.Set("col", JsonValue::MakeInt(r.col));
            room.Set("row", JsonValue::MakeInt(r.row));
            room.Set("type", JsonValue::MakeInt(r.type));
            room.Set("profile", JsonValue::MakeInt(r.profile));
            room.Set("depth", JsonValue::MakeInt(r.depth));
            JsonValue& doorsOpen = room.Set("doors", JsonValue::MakeObject());
            doorsOpen.Set("north", JsonValue::MakeBool(r.north));
            doorsOpen.Set("south", JsonValue::MakeBool(r.south));
            doorsOpen.Set("east", JsonValue::MakeBool(r.east));
            doorsOpen.Set("west", JsonValue::MakeBool(r.west));
            room.Set("startsEmpty", JsonValue::MakeBool(r.startsEmpty));
            room.Set("layout", JsonValue::MakeString(r.layout));
            if (!r.hasProgress) continue;
            JsonValue& progress = room.Set("progress", JsonValue::MakeObject());
            progress.Set("cleared", JsonValue::MakeBool(r.cleared));
            progress.Set("visited", JsonValue::MakeBool(r.visited));
            progress.Set("eliteMechanic", JsonValue::MakeInt(r.eliteMechanic));
            progress.Set("holdout", JsonValue::MakeFloat(r.holdout));
            progress.Set("special", JsonValue::MakeInt(r.special));
            progress.Set("specialClaimed", JsonValue::MakeBool(r.specialClaimed));
            progress.Set("specialChoice", JsonValue::MakeInt(r.specialChoice));
            JsonValue& options = progress.Set("specialOptions", JsonValue::MakeArray());
            for (int option : r.specialOptions) options.Push(JsonValue::MakeInt(option));
        }
    }

    if (c.hasShop)
    {
        JsonValue& shop = doc.Set("shop", JsonValue::MakeObject());
        shop.Set("zone", JsonValue::MakeInt(c.shop.zone));
        shop.Set("rerollCost", JsonValue::MakeInt(c.shop.rerollCost));
        shop.Set("dailyDeal", JsonValue::MakeInt(c.shop.dailyDeal));
        shop.Set("purchaseMade", JsonValue::MakeBool(c.shop.purchaseMade));
        JsonValue& items = shop.Set("items", JsonValue::MakeArray());
        for (const ShopItemCheckpoint& it : c.shop.items)
        {
            JsonValue& item = items.Push(JsonValue::MakeObject());
            item.Set("isAbility", JsonValue::MakeBool(it.isAbility));
            item.Set("isContract", JsonValue::MakeBool(it.isContract));
            item.Set("upgradesAbility", JsonValue::MakeBool(it.upgradesAbility));
            item.Set("purchased", JsonValue::MakeBool(it.purchased));
            item.Set("reserved", JsonValue::MakeBool(it.reserved));
            item.Set("unavailable", JsonValue::MakeBool(it.unavailable));
            item.Set("ware", JsonValue::MakeInt(it.ware));
            item.Set("contract", JsonValue::MakeInt(it.contract));
            item.Set("upgrade", JsonValue::MakeInt(it.upgrade));
            if (const char* id = SaveIdOf(it.ability)) item.Set("ability", JsonValue::MakeString(id));
            item.Set("price", JsonValue::MakeInt(it.price));
        }
    }
    return doc;
}

// ── Reading ──────────────────────────────────────────────────────────────────

bool RunCheckpointFromJson(const JsonValue& document, RunCheckpoint& out, std::string& error)
{
    JsonReadErrors errors;
    const JsonReader root(document, errors);
    RunCheckpoint c;
    root.Int("schema", SaveFiles::kRunCheckpointSchema, SaveFiles::kRunCheckpointSchema);
    c.runId = root.Int("runId", 1, L::kMaxRunId);
    c.active = root.Bool("active");
    if (!c.active)
    {
        if (!errors.Ok()) { error = errors.first; return false; }
        out = c;
        return true;
    }

    // Character.
    const JsonReader character = root.Object("character");
    const std::string cls = character.String("class", SaveFiles::kMaxIdLength);
    if (character.Ok() && !SaveIdTo(cls, c.character.cls)) character.Fail("unknown class");
    c.character.appearance = character.String("appearance", SaveFiles::kMaxIdLength);
    const JsonReader stats = character.Map("stats", L::kMaxCharacterStats);
    ValidateStats(stats);
    if (stats.IsPresent()) c.character.stats = *stats.Raw();
    const JsonReader abilities = character.Array("abilities", L::kMaxAbilitySlots);
    for (std::size_t i = 0; i < abilities.Count(); ++i)
    {
        const JsonReader item = abilities.Item(i);
        CharacterCheckpoint::Slot slot;
        const std::string id = item.StringOr("id", std::string(), SaveFiles::kMaxIdLength);
        if (!id.empty() && !SaveIdTo(id, slot.ability)) slot.ability = AbilityType::None;   // removed since: empty slot
        slot.level = (int)item.Int("level", 1, L::kMaxLevel);
        c.character.abilities.push_back(slot);
    }
    ReadSaveIdList<RelicType>(character.Array("relics", L::kMaxRelics), SaveFiles::kMaxIdLength,
                              [&](RelicType relic) { c.character.relics.push_back(relic); });

    // Run progress.
    const JsonReader run = root.Object("run");
    c.biome = ReadBiome(run, "biome");
    c.worldZone = (int)run.Int("worldZone", 0, L::kMaxRouteEntries);
    c.completedBiomes = ReadBiomes(run.Array("completedBiomes", L::kMaxRouteEntries));
    c.chosenNodes = ReadInts(run.Array("chosenNodes", L::kMaxRouteEntries), 0, L::kMaxEnumValue);
    const JsonReader session = run.Object("session");
    c.worldMapGenerated = session.Bool("worldMapGenerated");
    c.sessionCompleted = ReadBiomes(session.Array("completed", L::kMaxRouteEntries));
    c.sessionChosen = ReadBiomes(session.Array("chosen", L::kMaxRouteEntries));
    c.sessionNodes = ReadInts(session.Array("nodes", L::kMaxRouteEntries), 0, L::kMaxEnumValue);
    c.ascensionTier = (int)run.Int("ascensionTier", 0, L::kMaxSmallCount);
    c.ascensionRecorded = run.Bool("ascensionRecorded");
    c.playerDamageMult = run.Float("playerDamageMult", 0.f, L::kMaxMultiplier);
    c.enemyHealthMult = run.Float("enemyHealthMult", 0.f, L::kMaxMultiplier);
    c.enemyDamageMult = run.Float("enemyDamageMult", 0.f, L::kMaxMultiplier);
    c.wagerTier = (int)run.Int("wagerTier", 0, L::kMaxSmallCount);
    c.curseShrineUsed = run.Bool("curseShrineUsed");
    c.wagerAccessGranted = run.Bool("wagerAccessGranted");
    c.secondWind = run.Bool("secondWind");
    c.pendingRelicChoices = (int)run.Int("pendingRelicChoices", 0, L::kMaxSmallCount);
    c.enemiesKilled = (int)run.Int("enemiesKilled", 0, L::kMaxCount);
    c.bossesDefeated = (int)run.Int("bossesDefeated", 0, L::kMaxCount);
    c.roomsCleared = (int)run.Int("roomsCleared", 0, L::kMaxCount);
    c.elitesSlain = (int)run.Int("elitesSlain", 0, L::kMaxCount);
    c.timer = run.Float("timer", 0.f, L::kMaxSeconds);
    c.gameTimer = run.Float("gameTimer", 0.f, L::kMaxSeconds);
    c.seed = (unsigned)run.Int("seed", 0, kMaxSeed);
    JsonReader domainLog;
    if (run.OptionalArray("domainLog", L::kMaxRouteEntries, domainLog))
        for (std::size_t i = 0; i < domainLog.Count(); ++i)
        {
            const JsonReader item = domainLog.Item(i);
            RunLogDomain d;
            d.biome = ReadBiome(item, "biome");
            d.roomsCleared = (int)item.Int("roomsCleared", 0, L::kMaxCount);
            d.seconds = item.Float("seconds", 0.f, L::kMaxSeconds);
            d.bossDefeated = item.Bool("bossDefeated");
            c.domainLog.push_back(d);
        }
    JsonReader domainStart;
    if (run.OptionalObject("domainStart", domainStart))
    {
        c.domainStartRooms = (int)domainStart.Int("rooms", 0, c.roomsCleared);
        c.domainStartSeconds = domainStart.Float("seconds", 0.f, c.timer);
    }
    const JsonReader modifiers = run.Array("modifiers", L::kMaxModifiers);
    for (std::size_t i = 0; i < modifiers.Count(); ++i)
    {
        const JsonReader item = modifiers.Item(i);
        ModifierCheckpoint m;
        m.label = item.String("label", SaveFiles::kMaxTextLength);
        m.enemyHpMult = item.Float("enemyHpMult", 0.f, L::kMaxMultiplier);
        m.enemyDmgMult = item.Float("enemyDmgMult", 0.f, L::kMaxMultiplier);
        m.goldMult = item.Float("goldMult", 0.f, L::kMaxMultiplier);
        m.xpMult = item.Float("xpMult", 0.f, L::kMaxMultiplier);
        m.tintRgba = (unsigned)item.Int("tint", 0, kMaxRgba);
        m.active = item.Bool("active");
        m.roomsRemaining = (int)item.Int("roomsRemaining", 0, L::kMaxSmallCount);
        m.shopContract = (int)item.Int("shopContract", 0, L::kShopContracts);
        m.curse = item.Bool("curse");
        c.modifiers.push_back(m);
    }

    // Where the player is.
    const JsonReader location = root.Object("location");
    c.atWorldMap = location.Bool("atWorldMap");
    c.atCrossroads = location.Bool("atCrossroads");
    const JsonReader doors = location.Array("crossroads", L::kCrossroadsDoors);
    if (doors.IsPresent() && doors.Count() != (std::size_t)L::kCrossroadsDoors) doors.Fail("expected three roads");
    for (std::size_t i = 0; i < doors.Count() && i < (std::size_t)L::kCrossroadsDoors; ++i)
    {
        const JsonReader item = doors.Item(i);
        CrossroadsDoorCheckpoint& door = c.crossroads[i];
        door.open = item.Bool("open");
        if (door.open) door.biome = ReadBiome(item, "biome");
        door.boon = (int)item.Int("boon", 0, L::kPathBoons);   // kPathBoons: no reward assigned yet
    }

    // The domain's room graph.
    JsonReader dungeon;
    c.hasDungeon = root.OptionalObject("dungeon", dungeon);
    if (c.hasDungeon == (c.atWorldMap || c.atCrossroads)) root.Fail("a dungeon is saved exactly when the run is in one");
    if (c.hasDungeon)
    {
        DungeonCheckpoint& d = c.dungeon;
        const JsonReader rooms = dungeon.Array("rooms", L::kMaxDungeonRooms);
        if (rooms.IsPresent() && rooms.Count() == 0) rooms.Fail("no rooms");
        const int count = (int)rooms.Count();
        for (int i = 0; i < count; ++i)
        {
            const JsonReader item = rooms.Item((std::size_t)i);
            RoomCheckpoint r;
            r.col = (int)item.Int("col", 0, L::kGridSize - 1);
            r.row = (int)item.Int("row", 0, L::kGridSize - 1);
            r.type = (int)item.Int("type", 0, L::kRoomTypes - 1);
            r.profile = (int)item.Int("profile", 0, L::kEncounterProfiles - 1);
            r.depth = (int)item.Int("depth", 0, L::kMaxDungeonRooms);
            const JsonReader doorFlags = item.Object("doors");
            r.north = doorFlags.Bool("north");
            r.south = doorFlags.Bool("south");
            r.east = doorFlags.Bool("east");
            r.west = doorFlags.Bool("west");
            r.startsEmpty = item.Bool("startsEmpty");
            r.layout = item.String("layout", kMaxRoomLayoutLength);
            if (item.Ok() && r.layout.empty()) item.Fail("room has no layout");
            JsonReader progress;
            r.hasProgress = item.OptionalObject("progress", progress);
            if (r.hasProgress)
            {
                r.cleared = progress.Bool("cleared");
                r.visited = progress.Bool("visited");
                r.eliteMechanic = (int)progress.Int("eliteMechanic", kNoSelection, L::kMaxEnumValue);
                r.holdout = progress.Float("holdout", (float)kNoSelection, L::kMaxSeconds);
                r.special = (int)progress.Int("special", 0, L::kRoomSpecials - 1);
                r.specialClaimed = progress.Bool("specialClaimed");
                r.specialChoice = (int)progress.Int("specialChoice", kNoSelection, L::kMaxEnumValue);
                const JsonReader options = progress.Array("specialOptions", L::kSpecialOptions);
                if (options.IsPresent() && options.Count() != (std::size_t)L::kSpecialOptions) options.Fail("expected three options");
                for (std::size_t o = 0; o < options.Count() && o < (std::size_t)L::kSpecialOptions; ++o)
                    r.specialOptions[o] = (int)options.Item(o).AsInt(kNoSelection, L::kMaxEnumValue);
            }
            d.rooms.push_back(r);
        }
        d.start = (int)dungeon.Int("start", 0, count - 1);
        d.boss = (int)dungeon.Int("boss", 0, count - 1);
        d.key = (int)dungeon.Int("key", kNoSelection, count - 1);
        d.current = (int)dungeon.Int("current", 0, count - 1);
        d.entrySide = (int)dungeon.Int("entrySide", kNoSelection, L::kDoorSides - 1);
        d.entryX = dungeon.Float("entryX", -L::kMaxCoordinate, L::kMaxCoordinate);
        d.entryY = dungeon.Float("entryY", -L::kMaxCoordinate, L::kMaxCoordinate);
        d.hasMagicGem = dungeon.Bool("hasMagicGem");
        d.magicGemSpawned = dungeon.Bool("magicGemSpawned");
        d.magicGemCollected = dungeon.Bool("magicGemCollected");
        d.bossBarrierUnlocked = dungeon.Bool("bossBarrierUnlocked");
        d.chestSpawned = dungeon.Bool("chestSpawned");
        d.chestBroken = dungeon.Bool("chestBroken");

        if (errors.Ok())
        {
            // The graph rules DungeonGen produces: one room per grid cell,
            // every door leads to a neighbour that opens back, and every
            // room can be reached from the entrance.
            int grid[L::kGridSize][L::kGridSize];
            for (auto& line : grid) for (int& cell : line) cell = kNoSelection;
            for (int i = 0; i < count; ++i)
            {
                int& cell = grid[d.rooms[i].row][d.rooms[i].col];
                if (cell != kNoSelection) { dungeon.Fail("two rooms share a grid cell"); break; }
                cell = i;
            }
            if (d.start == d.boss) dungeon.Fail("the entrance is the boss room");
            for (int i = 0; i < count && errors.Ok(); ++i)
                for (int door = 0; door < (int)std::size(kDoorSteps); ++door)
                {
                    if (!HasDoor(d.rooms[i], door)) continue;
                    const int col = d.rooms[i].col + kDoorSteps[door].dCol, row = d.rooms[i].row + kDoorSteps[door].dRow;
                    const int next = (col >= 0 && col < L::kGridSize && row >= 0 && row < L::kGridSize) ? grid[row][col] : kNoSelection;
                    if (next == kNoSelection || !HasDoor(d.rooms[next], kDoorSteps[door].opposite))
                    {
                        dungeon.Fail("a door leads nowhere");
                        break;
                    }
                }
            std::vector<bool> reached((std::size_t)count, false);
            std::vector<int> frontier{ d.start };
            reached[(std::size_t)d.start] = true;
            while (errors.Ok() && !frontier.empty())
            {
                const int at = frontier.back();
                frontier.pop_back();
                for (int door = 0; door < (int)std::size(kDoorSteps); ++door)
                {
                    if (!HasDoor(d.rooms[at], door)) continue;
                    const int next = grid[d.rooms[at].row + kDoorSteps[door].dRow][d.rooms[at].col + kDoorSteps[door].dCol];
                    if (!reached[(std::size_t)next]) { reached[(std::size_t)next] = true; frontier.push_back(next); }
                }
            }
            for (int i = 0; i < count && errors.Ok(); ++i)
                if (!reached[(std::size_t)i]) dungeon.Fail("a room can't be reached from the entrance");
        }
    }

    // Zeph's stock.
    JsonReader shop;
    c.hasShop = root.OptionalObject("shop", shop);
    if (c.hasShop)
    {
        c.shop.zone = (int)shop.Int("zone", 0, L::kMaxRouteEntries);
        c.shop.rerollCost = (int)shop.Int("rerollCost", 0, L::kMaxPrice);
        const JsonReader items = shop.Array("items", L::kMaxShopItems);
        c.shop.dailyDeal = (int)shop.Int("dailyDeal", kNoSelection, L::kMaxShopItems - 1);
        c.shop.purchaseMade = shop.Bool("purchaseMade");
        for (std::size_t i = 0; i < items.Count(); ++i)
        {
            const JsonReader item = items.Item(i);
            ShopItemCheckpoint it;
            it.isAbility = item.Bool("isAbility");
            it.isContract = item.Bool("isContract");
            it.upgradesAbility = item.Bool("upgradesAbility");
            it.purchased = item.Bool("purchased");
            it.reserved = item.Bool("reserved");
            it.unavailable = item.Bool("unavailable");
            it.ware = (int)item.Int("ware", 0, L::kShopWares - 1);
            it.contract = (int)item.Int("contract", 0, L::kShopContracts);
            it.upgrade = (int)item.Int("upgrade", 0, L::kUpgrades - 1);
            const std::string ability = item.StringOr("ability", std::string(), SaveFiles::kMaxIdLength);
            if (!ability.empty() && !SaveIdTo(ability, it.ability))
            {
                it.ability = AbilityType::None;   // removed since: the card can't be bought
                it.unavailable = true;
            }
            it.price = (int)item.Int("price", 0, L::kMaxPrice);
            c.shop.items.push_back(it);
        }
    }

    if (!errors.Ok()) { error = errors.first; return false; }
    out = std::move(c);
    return true;
}
