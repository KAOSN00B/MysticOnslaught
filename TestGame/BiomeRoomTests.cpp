// Biome-specific rooms (tools/biome_content/biome_rooms.py): each loads and
// builds with every asset resolved, has the role, biome and doors it declares,
// keeps its door lanes and the event-shrine centre open, has one connected
// floor, passes through exactly the doors of every shape it can be assigned
// (unused sides sealed), is not a copy of an existing layout, and is picked by
// dungeon layout assignment.
// Build: tools\run_tests.ps1 -Suite BiomeRooms

#include "DungeonGen.h"
#include "RoomAssetCatalog.h"
#include "RoomBlueprint.h"
#include "RoomCapacity.h"
#include "RoomCollision.h"
#include "RoomLibrary.h"
#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>

static int g_checks = 0;
#define CHECK(x) do { ++g_checks; if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)

namespace
{
    struct NewRoom
    {
        const char* path;
        const char* id;
        Biome biome;
        unsigned char doorMask;   // N=1 S=2 E=4 W=8
        FallSurface surface;
        bool pits;
    };

    const NewRoom kNewRooms[] = {
        { "TestGame/Rooms/Generated/LostCity/biome_rooms/colonnade.mroom", "biome_lostcity_colonnade_r1",
          Biome::LostCity, 1 | 2, FallSurface::Void, false },
        { "TestGame/Rooms/Generated/AncientCastle/biome_rooms/pillar_hall.mroom", "biome_ancientcastle_pillar_hall_r1",
          Biome::AncientCastle, 4 | 8, FallSurface::Void, false },
        { "TestGame/Rooms/Generated/TheSanctuary/biome_rooms/basin_court.mroom", "biome_thesanctuary_basin_court_r1",
          Biome::TheSanctuary, 15, FallSurface::Void, false },
        { "TestGame/Rooms/Generated/Wastelands/biome_rooms/rubble_field.mroom", "biome_wastelands_rubble_field_r1",
          Biome::Wastelands, 15, FallSurface::Void, false },
        { "TestGame/Rooms/Generated/DreamRealm/biome_rooms/statue_circle.mroom", "biome_dreamrealm_statue_circle_r1",
          Biome::DreamRealm, 15, FallSurface::Void, false },
        { "TestGame/Rooms/Generated/DemonsInsides/biome_rooms/lava_pools.mroom", "biome_demonsinsides_lava_pools_r1",
          Biome::DemonsInsides, 15, FallSurface::Lava, true },
        { "TestGame/Rooms/Generated/Jungle/biome_rooms/sunken_pools.mroom", "biome_jungle_sunken_pools_r1",
          Biome::Jungle, 4 | 8, FallSurface::Void, true },
    };

    // Cells that must stay walkable: a lane inside each enabled door, and the
    // room centre (event shrine, Zeph's camp, crossroads anchors).
    struct CellRect { int col, row, w, h; };
    constexpr CellRect kLaneN{ 11, 1, 7, 4 }, kLaneS{ 11, 11, 7, 4 }, kLaneW{ 1, 6, 4, 5 }, kLaneE{ 23, 6, 4, 5 };
    constexpr CellRect kCentre{ 12, 5, 5, 4 };
    constexpr float kCell = 16.f;

    // Gameplay geometry: fall cells and where each prop stands. Two rooms with
    // the same signature play the same, whatever art they wear.
    std::string Signature(const RoomBlueprint& room)
    {
        std::string s;
        for (int r = 0; r < RoomLayout::kRows; ++r)
            for (int c = 0; c < RoomLayout::kCols; ++c) s += room.fall[r][c] ? '1' : '0';
        std::set<std::string> props;
        for (const RoomAssetPlacement& p : room.placements)
            if (p.kind == RoomAssetKind::Prop || p.kind == RoomAssetKind::AnimProp)
                props.insert(std::to_string(p.col) + "," + std::to_string(p.row));
        for (const std::string& p : props) s += "|" + p;
        return s;
    }

    bool CellsOpen(const RoomLayout& layout, const std::vector<Rectangle>& props, CellRect area)
    {
        for (int r = area.row; r < area.row + area.h; ++r)
            for (int c = area.col; c < area.col + area.w; ++c)
                if (!IsRoomSpawnAreaValid(layout, { c * kCell, r * kCell, kCell, kCell }, kCell, kCell, props))
                    return false;
        return true;
    }

    // Walks a body from just inside a door lane out through the door, at the
    // game's cell size, and reports whether it got through.
    bool PassesDoor(const RoomLayout& layout, int side)   // 0 top, 1 bottom, 2 left, 3 right
    {
        constexpr float cell = 48.f;
        const Vector2 from[4] = { { 14.5f, 2.0f }, { 14.5f, 14.0f }, { 2.0f, 8.5f }, { 26.0f, 8.5f } };
        const Vector2 to[4]   = { { 14.5f, 0.4f }, { 14.5f, 15.6f }, { 0.4f, 8.5f }, { 27.6f, 8.5f } };
        const Vector2 a{ from[side].x * cell, from[side].y * cell }, b{ to[side].x * cell, to[side].y * cell };
        const Vector2 got = ResolveHandcraftedTileMovement(layout, a, b, { b.x - 10.f, b.y - 10.f, 20.f, 20.f }, cell, cell);
        return std::fabs(got.x - b.x) < 0.01f && std::fabs(got.y - b.y) < 0.01f;
    }
}

int main()
{
    SetTraceLogLevel(LOG_WARNING);
    SetRandomSeed(20261016);
    RoomAssetCatalog catalog;
    CHECK(catalog.Refresh("MapTilesets", "TestGame"));
    RoomLibrary library;
    library.Refresh("TestGame/Rooms");

    for (const NewRoom& expected : kNewRooms)
    {
        std::string error;
        const auto room = RoomBlueprint::Load(expected.path, error);
        if (!room) std::printf("  %s: %s\n", expected.path, error.c_str());
        CHECK(room.has_value());
        CHECK(room->id == expected.id && room->biome == expected.biome);
        CHECK(room->roomType == RoomType::Standard);
        CHECK(room->DoorMask() == expected.doorMask);
        CHECK(room->fallSurface == expected.surface);
        CHECK(library.FindById(expected.id) != nullptr);

        // Builds with every prop / pool tile resolved against its biome sheet.
        const RoomAssetSource* source = catalog.Find(room->tilesetStem);
        CHECK(source != nullptr);
        std::string warning;
        const auto layout = BuildRoomLayout(*room, source->definitions, warning, &catalog);
        if (!warning.empty()) std::printf("  %s: %s\n", expected.id, warning.c_str());
        CHECK(layout.has_value() && warning.empty());
        CHECK(layout->props.size() + layout->animProps.size() + layout->decors.size() + layout->animDecors.size() ==
              room->placements.size());
        CHECK(layout->doorZones[0].enabled == ((expected.doorMask & 1) != 0));
        CHECK(layout->doorZones[1].enabled == ((expected.doorMask & 2) != 0));
        CHECK(layout->doorZones[2].enabled == ((expected.doorMask & 8) != 0));
        CHECK(layout->doorZones[3].enabled == ((expected.doorMask & 4) != 0));

        int fallCells = 0;
        for (int r = 0; r < RoomLayout::kRows; ++r)
            for (int c = 0; c < RoomLayout::kCols; ++c) fallCells += layout->fall[r][c] ? 1 : 0;
        CHECK((fallCells > 0) == expected.pits);
        if (expected.pits) CHECK(!layout->animDecors.empty());   // pits are drawn, not invisible

        // Prop collision boxes, as enemies and the player collide with them.
        std::vector<Rectangle> propBoxes;
        for (const SpritePlacement& p : layout->props)
        {
            const TileDefSet* defs = ResolveRoomDefinitions(*layout, p, source->definitions);
            CHECK(defs != nullptr && p.defIdx >= 0 && p.defIdx < (int)defs->props.size());
            const Rectangle c = defs->props[(size_t)p.defIdx].collision;
            CHECK(c.width > 0.f && c.height > 0.f);
            const Rectangle box{ p.col * kCell + c.x, p.row * kCell + c.y, c.width, c.height };
            CHECK(box.x >= kCell && box.y >= kCell && box.x + box.width <= (RoomLayout::kCols - 1) * kCell &&
                  box.y + box.height <= (RoomLayout::kRows - 1) * kCell);   // inside the walls
            for (const Rectangle& other : propBoxes) CHECK(!CheckCollisionRecs(box, other));
            propBoxes.push_back(box);
        }

        // Door lanes and the shrine centre are open floor; the event anchor
        // at the centre needs no nudge.
        CHECK(CellsOpen(*layout, propBoxes, kCentre));
        if (expected.doorMask & 1) CHECK(CellsOpen(*layout, propBoxes, kLaneN));
        if (expected.doorMask & 2) CHECK(CellsOpen(*layout, propBoxes, kLaneS));
        if (expected.doorMask & 4) CHECK(CellsOpen(*layout, propBoxes, kLaneE));
        if (expected.doorMask & 8) CHECK(CellsOpen(*layout, propBoxes, kLaneW));
        const Vector2 centre{ 14.5f * kCell, 6.5f * kCell };
        const Vector2 safe = FindNearestSafeRoomPosition(*layout, centre, kCell, kCell);
        CHECK(safe.x == centre.x && safe.y == centre.y);

        // One connected floor (so every door reaches every other, and any door
        // sealed by the dungeon leaves the rest connected), sized for a fight.
        const RoomCombatCapacity capacity = RoomCapacityAnalyzer::Analyze(*layout, source->definitions);
        CHECK(capacity.walkableTiles == capacity.connectedTiles);
        CHECK(capacity.band != RoomCapacityBand::Small);

        // Every door shape it can be assigned to: the dungeon seals the unused
        // sides (ApplyActiveRoomDoorMask); once the room is cleared, the used
        // doors let the player through and the sealed ones are wall.
        int shapes = 0;
        for (int mask = 1; mask < 16; ++mask)
        {
            if (mask != expected.doorMask && expected.doorMask != 15) continue;
            RoomLayout sealed = *layout;
            ApplyActiveRoomDoorMask(sealed, (unsigned char)mask);
            sealed.roomCleared = true;
            CHECK(PassesDoor(sealed, 0) == ((mask & 1) != 0));
            CHECK(PassesDoor(sealed, 1) == ((mask & 2) != 0));
            CHECK(PassesDoor(sealed, 2) == ((mask & 8) != 0));
            CHECK(PassesDoor(sealed, 3) == ((mask & 4) != 0));
            ++shapes;
        }
        CHECK(shapes == (expected.doorMask == 15 ? 15 : 1));
        std::printf("  %-36s doors=%2d floor=%3d pits=%2d props=%2zu capacity=%s\n", expected.id, expected.doorMask,
                    capacity.connectedTiles, fallCells, layout->props.size(),
                    RoomCapacityAnalyzer::BandName(capacity.band));

        // A new layout, not a reskin: no other room in the library plays the same.
        const std::string signature = Signature(*room);
        for (const RoomBlueprint& other : library.Rooms())
            if (other.id != room->id) CHECK(Signature(other) != signature);

        // Dungeon assignment offers it to exactly the slots its doors fit, and
        // actually picks it.
        std::vector<const RoomBlueprint*> biomeRooms, exact, fourDoor;
        for (const RoomBlueprint& r : library.Rooms()) if (r.biome == expected.biome) biomeRooms.push_back(&r);
        const RoomBlueprint* added = library.FindById(expected.id);
        for (int mask = 1; mask < 16; ++mask)
        {
            RoomLibrary::CompatibleLayouts(biomeRooms, { RoomType::Standard, (unsigned char)mask }, exact, fourDoor);
            const bool offered = std::find(exact.begin(), exact.end(), added) != exact.end() ||
                                 std::find(fourDoor.begin(), fourDoor.end(), added) != fourDoor.end();
            CHECK(offered == (mask == expected.doorMask || expected.doorMask == 15));
        }
        bool picked = false;
        DungeonGen gen;
        for (int dungeon = 0; dungeon < 300 && !picked; ++dungeon)
        {
            gen.SetPlan(DungeonGen::PlanForDomain(dungeon % 6));
            std::vector<std::string> ids;
            for (int attempt = 0; attempt < 600 && ids.empty(); ++attempt)
            {
                gen.Generate();
                std::vector<LayoutSlot> slots;
                for (const DungeonRoom& r : gen.GetRooms())
                    slots.push_back({ r.type, RoomDoorMask(r.hasNorth, r.hasSouth, r.hasEast, r.hasWest) });
                ids = library.AssignDungeonLayouts(expected.biome, slots, [](int n) { return GetRandomValue(0, n - 1); });
            }
            CHECK(!ids.empty());
            for (const std::string& id : ids) picked = picked || id == expected.id;
        }
        CHECK(picked);
    }

    std::printf("Biome room tests passed (%d checks): %d new rooms load, build, keep doors and centre open, "
                "have one connected floor, open exactly their assigned doors, are new layouts, and are picked "
                "by dungeon assignment.\n",
                g_checks, (int)(sizeof(kNewRooms) / sizeof(kNewRooms[0])));
    return 0;
}
