// Dungeon layout assignment (RoomLibrary::AssignLayouts): a dungeon does not
// repeat a layout while a compatible alternative is unused, a scarce pool
// still forms a dungeon, and biome / role / door-shape rules always hold.
// Build: tools\run_tests.ps1 -Suite RoomSelection

#include "DungeonGen.h"
#include "RoomLibrary.h"
#include "raylib.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

static int g_checks = 0;
#define CHECK(x) do { ++g_checks; if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)

namespace
{
    int RandomBelow(int n) { return GetRandomValue(0, n - 1); }

    RoomBlueprint Make(const char* id, RoomType role, unsigned char mask)
    {
        RoomBlueprint room = RoomBlueprint::CreateDefault();
        room.id = id;
        room.name = id;
        room.biome = Biome::Caverns;
        room.tilesetStem = "Caverns";
        room.roomType = role;
        room.hasNorth = (mask & 1) != 0;
        room.hasSouth = (mask & 2) != 0;
        room.hasEast  = (mask & 4) != 0;
        room.hasWest  = (mask & 8) != 0;
        return room;
    }

    std::vector<LayoutSlot> Slots(RoomType role, unsigned char mask, int count)
    {
        return std::vector<LayoutSlot>((size_t)count, LayoutSlot{ role, mask });
    }

    std::map<std::string, int> Counts(const std::vector<std::string>& ids)
    {
        std::map<std::string, int> counts;
        for (const std::string& id : ids) ++counts[id];
        return counts;
    }
}

int main()
{
    SetTraceLogLevel(LOG_WARNING);
    SetRandomSeed(20261016);

    // ── 1. The policy on a small, known pool ────────────────────────────────
    const std::vector<RoomBlueprint> owned = {
        Make("n1", RoomType::Standard, 1), Make("n2", RoomType::Standard, 1), Make("n3", RoomType::Standard, 1),
        Make("four", RoomType::Standard, 15), Make("treasure-w", RoomType::Treasure, 8),
        Make("elite-n", RoomType::Elite, 1),
    };
    std::vector<const RoomBlueprint*> pool;
    for (const RoomBlueprint& room : owned) pool.push_back(&room);

    for (int trial = 0; trial < 50; ++trial)
    {
        // Three north dead ends, three north layouts: all different, all exact.
        auto ids = RoomLibrary::AssignLayouts(pool, Slots(RoomType::Standard, 1, 3), RandomBelow);
        CHECK(ids.size() == 3 && Counts(ids).size() == 3 && Counts(ids).count("four") == 0);
        // A fourth takes the unused four-door layout rather than repeat one.
        ids = RoomLibrary::AssignLayouts(pool, Slots(RoomType::Standard, 1, 4), RandomBelow);
        CHECK(Counts(ids).size() == 4 && Counts(ids)["four"] == 1);
        // Six: the pool repeats only as needed, and repeats exact shapes first.
        ids = RoomLibrary::AssignLayouts(pool, Slots(RoomType::Standard, 1, 6), RandomBelow);
        auto counts = Counts(ids);
        int most = 0;
        for (const auto& [id, n] : counts) most = std::max(most, n);
        CHECK(ids.size() == 6 && most == 2 && counts["four"] == 1);
    }

    // A pool of one layout still forms the dungeon: it repeats.
    {
        const RoomBlueprint only = Make("only-e", RoomType::Standard, 4);
        const std::vector<const RoomBlueprint*> scarce = { &only };
        const auto ids = RoomLibrary::AssignLayouts(scarce, Slots(RoomType::Standard, 4, 5), RandomBelow);
        CHECK(ids.size() == 5 && Counts(ids)["only-e"] == 5);
        // ...and a four-door layout alone can stand in for any shape.
        const RoomBlueprint cross = Make("cross", RoomType::Standard, 15);
        const std::vector<const RoomBlueprint*> crossOnly = { &cross };
        CHECK(RoomLibrary::AssignLayouts(crossOnly, Slots(RoomType::Standard, 6, 3), RandomBelow).size() == 3);
    }
    // No compatible layout at all: empty, so the caller rolls a new graph.
    CHECK(RoomLibrary::AssignLayouts(pool, Slots(RoomType::Treasure, 1, 1), RandomBelow).empty());
    CHECK(RoomLibrary::AssignLayouts(pool, { { RoomType::Standard, 1 }, { RoomType::Boss, 2 } }, RandomBelow).empty());

    // Roles: Elite uses Elite art when it has any (repeating it rather than
    // borrowing); Store borrows Standard; Treasure never borrows.
    {
        const auto elite = Counts(RoomLibrary::AssignLayouts(pool, Slots(RoomType::Elite, 1, 3), RandomBelow));
        CHECK(elite.size() == 1 && elite.count("elite-n") == 1);
        const auto store = RoomLibrary::AssignLayouts(pool, Slots(RoomType::Store, 1, 1), RandomBelow);
        CHECK(store.size() == 1 && store[0].rfind("n", 0) == 0);
        const std::vector<const RoomBlueprint*> noElite = { &owned[0], &owned[1] };
        const auto borrowed = RoomLibrary::AssignLayouts(noElite, Slots(RoomType::Elite, 1, 2), RandomBelow);
        CHECK(Counts(borrowed).size() == 2);
        CHECK(RoomLibrary::AssignLayouts(pool, Slots(RoomType::Treasure, 8, 2), RandomBelow) ==
              (std::vector<std::string>{ "treasure-w", "treasure-w" }));
    }

    // ── 2. The real library: rules and the no-needless-repeat property ──────
    RoomLibrary library;
    library.Refresh("TestGame/Rooms");
    CHECK(library.Rooms().size() >= 262);
    long long slotsTotal = 0, repeatsBefore = 0, repeatsAfter = 0, forced = 0;
    for (int b = 0; b < 10; ++b)
    {
        const Biome biome = (Biome)b;
        std::vector<const RoomBlueprint*> biomeRooms;
        for (const RoomBlueprint& room : library.Rooms()) if (room.biome == biome) biomeRooms.push_back(&room);
        for (int domain = 0; domain <= 5; ++domain)
            for (int trial = 0; trial < 20; ++trial)
            {
                DungeonGen gen;
                gen.SetPlan(DungeonGen::PlanForDomain(domain));
                std::vector<LayoutSlot> slots;
                std::vector<std::string> ids;
                for (int attempt = 0; attempt < 600 && ids.empty(); ++attempt)
                {
                    gen.Generate();
                    if (domain > 0) gen.ForceRoomType(gen.GetStartIndex(), RoomType::Store);   // Zeph's camp
                    slots.clear();
                    for (const DungeonRoom& room : gen.GetRooms())
                        slots.push_back({ room.type, RoomDoorMask(room.hasNorth, room.hasSouth, room.hasEast, room.hasWest) });
                    ids = library.AssignDungeonLayouts(biome, slots, RandomBelow);
                }
                CHECK(ids.size() == slots.size());

                std::map<std::string, int> uses;
                std::vector<const RoomBlueprint*> exact, fourDoor;
                for (size_t i = 0; i < ids.size(); ++i)
                {
                    const RoomBlueprint* chosen = library.FindById(ids[i]);
                    CHECK(chosen != nullptr && chosen->biome == biome);
                    const unsigned char mask = chosen->DoorMask();
                    CHECK(mask == slots[i].doorMask || mask == 15);
                    CHECK(chosen->roomType == slots[i].role ||
                          ((slots[i].role == RoomType::Elite || slots[i].role == RoomType::Store) &&
                           chosen->roomType == RoomType::Standard));
                    RoomLibrary::CompatibleLayouts(biomeRooms, slots[i], exact, fourDoor);
                    CHECK(std::count(exact.begin(), exact.end(), chosen) + std::count(fourDoor.begin(), fourDoor.end(), chosen) == 1);
                    // A repeat only when every compatible layout is already in the dungeon.
                    if (uses[ids[i]] > 0)
                    {
                        ++forced;
                        for (const RoomBlueprint* r : exact)    CHECK(uses[r->id] > 0);
                        for (const RoomBlueprint* r : fourDoor) CHECK(uses[r->id] > 0);
                    }
                    ++uses[ids[i]];
                }
                slotsTotal += (long long)ids.size();
                repeatsAfter += (long long)ids.size() - (long long)uses.size();

                // The old normal path, on the same graph: a uniform pick from
                // PlaytestCandidates per room, with no memory of the dungeon.
                std::set<std::string> seen;
                for (const LayoutSlot& slot : slots)
                {
                    const auto candidates = library.PlaytestCandidates(biome, slot.role, slot.doorMask);
                    CHECK(!candidates.empty());
                    seen.insert(candidates[(size_t)RandomBelow((int)candidates.size())]->id);
                }
                repeatsBefore += (long long)slots.size() - (long long)seen.size();
            }
    }
    std::printf("  %lld rooms in 1200 dungeons: repeated layouts %lld before, %lld after (%lld forced by small pools)\n",
                slotsTotal, repeatsBefore, repeatsAfter, forced);
    CHECK(repeatsAfter < repeatsBefore);
    CHECK(forced == repeatsAfter);

    std::printf("Room selection tests passed (%d checks): no repeat while an alternative is unused, scarce pools still "
                "form dungeons, biome/role/door rules hold.\n", g_checks);
    return 0;
}
