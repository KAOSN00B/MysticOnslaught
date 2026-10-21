// Native validation of the handcrafted room content for every biome.
//
//  1. Each Forest-family variant (tools/biome_content/forest_variants.py) has
//     exactly its Forest source's gameplay: tile/solid/fall grids, colliders,
//     fall rectangles, prop hitboxes, doors, room role, sampled body/fall
//     positions and door clearing. Its art comes only from permitted sheets
//     and stays inside the images.
//  2. Every room in the runtime library builds without warnings.
//  3. Every biome's library can form complete dungeons, including the
//     Zeph's-camp entrance used by every domain after the Forest.
// Runs from the project root (tools\run_tests.ps1).
#include "RoomBlueprint.h"
#include "RoomAssetCatalog.h"
#include "RoomCollision.h"
#include "RoomLibrary.h"
#include "DungeonGen.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace
{
    struct Target
    {
        const char* slug;      // file suffix
        const char* folder;    // Rooms/Generated/<folder>/forest_variants
        const char* sheet;     // biome tileset stem
        std::vector<std::string> permitted;
    };

    const std::vector<Target>& Targets()
    {
        static const std::vector<Target> targets = {
            { "caverns",       "Caverns",       "Caverns",        { "Caverns" } },
            { "graveyard",     "Graveyard",     "Graveyard",      { "Graveyard", "Village_Ground" } },
            { "demonsinsides", "DemonsInsides", "Demons Insides", { "Demons Insides" } },
            { "dreamrealm",    "DreamRealm",    "Dream Realm",    { "Dream Realm" } },
            { "lostcity",      "LostCity",      "Lost City",      { "Lost City" } },
            { "thesanctuary",  "TheSanctuary",  "The Sanctuary",  { "The Sanctuary" } },
            { "wastelands",    "Wastelands",    "Wastelands",     { "Wastelands", "Ground TIles" } },
            { "ancientcastle", "AncientCastle", "Ancient Castle", { "Ancient Castle", "Lost City" } },
        };
        return targets;
    }

    bool SameRect(Rectangle a, Rectangle b)
    {
        return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
    }

    int g_failures = 0;
    void Expect(bool ok, const std::string& what)
    {
        if (ok) return;
        ++g_failures;
        std::cerr << "FAIL " << what << '\n';
    }

    std::string AtlasStem(const Target& t)
    {
        std::string stem = t.folder;
        return stem + "_ForestVariants";
    }

    void CheckVariant(const Target& t, const std::string& name, const RoomAssetCatalog& catalog)
    {
        const std::string label = std::string(t.folder) + "/" + name;
        std::string error;
        auto source = RoomBlueprint::Load("TestGame/Rooms/Forest/forest/" + name + ".mroom", error);
        Expect(source.has_value(), label + ": Forest source loads");
        const std::string variantPath = std::string("TestGame/Rooms/Generated/") + t.folder +
            "/forest_variants/forest_" + name + "_" + t.slug + ".mroom";
        auto target = RoomBlueprint::Load(variantPath, error);
        Expect(target.has_value() && error.empty(), label + ": variant loads (" + error + ")");
        if (!source || !target) return;

        const auto* forest = catalog.Find("Forest");
        const auto* biome = catalog.Find(t.sheet);
        Expect(forest && biome, label + ": catalogs present");
        if (!forest || !biome) return;
        std::string warnA, warnB;
        auto before = BuildRoomLayout(*source, forest->definitions, warnA, &catalog);
        auto after = BuildRoomLayout(*target, biome->definitions, warnB, &catalog);
        Expect(before.has_value() && after.has_value(), label + ": layouts build");
        Expect(warnB.empty(), label + ": variant builds without warnings (" + warnB + ")");
        if (!before || !after) return;

        Expect(source->DoorMask() == target->DoorMask(), label + ": door mask");
        Expect(source->roomType == target->roomType, label + ": room role");
        Expect(source->combatCapacityOverride == target->combatCapacityOverride, label + ": capacity");
        Expect(source->placements.size() == target->placements.size(), label + ": placement count");
        Expect(before->colliders.size() == after->colliders.size(), label + ": collider count");
        Expect(before->fallRects.size() == after->fallRects.size(), label + ": fall rect count");
        for (size_t i = 0; i < before->colliders.size() && i < after->colliders.size(); ++i)
            Expect(SameRect(before->colliders[i], after->colliders[i]), label + ": collider identical");
        for (size_t i = 0; i < before->fallRects.size() && i < after->fallRects.size(); ++i)
            Expect(SameRect(before->fallRects[i], after->fallRects[i]), label + ": fall rect identical");

        std::vector<Rectangle> beforeProps, afterProps;
        for (size_t i = 0; i < source->placements.size() && i < target->placements.size(); ++i)
        {
            const auto& a = source->placements[i];
            const auto& b = target->placements[i];
            Expect(a.kind == b.kind && a.col == b.col && a.row == b.row && a.band == b.band,
                   label + ": placement position/kind/band");
            const auto* ad = catalog.Find(a.sourceTileset.empty() ? source->tilesetStem : a.sourceTileset);
            const auto* bd = catalog.Find(b.sourceTileset.empty() ? target->tilesetStem : b.sourceTileset);
            Expect(ad && bd, label + ": placement catalogs");
            if (!ad || !bd) continue;
            Expect(b.sourceTileset == AtlasStem(t) || b.sourceTileset == "Village_Animations",
                   label + ": placement uses the biome atlas or shared water (" + b.sourceTileset + ")");
            const int ai = ad->definitions.FindAssetIndex(a.kind, a.assetId);
            const int bi = bd->definitions.FindAssetIndex(b.kind, b.assetId);
            Expect(ai >= 0 && bi >= 0, label + ": placement asset resolves (" + b.assetId + ")");
            if (ai < 0 || bi < 0) continue;
            if (a.kind == RoomAssetKind::Prop)
            {
                const auto& ap = ad->definitions.props[ai];
                const auto& bp = bd->definitions.props[bi];
                Expect(SameRect(ap.collision, bp.collision), label + ": prop hitbox identical");
                Expect(ap.src.width == bp.src.width && ap.src.height == bp.src.height, label + ": prop footprint identical");
                beforeProps.push_back({ a.col * 16.f + ap.collision.x, a.row * 16.f + ap.collision.y,
                                        ap.collision.width, ap.collision.height });
                afterProps.push_back({ b.col * 16.f + bp.collision.x, b.row * 16.f + bp.collision.y,
                                       bp.collision.width, bp.collision.height });
            }
        }

        Expect(before->visualTiles.size() == after->visualTiles.size(), label + ": visual count");
        for (int r = 0; r < RoomLayout::kRows; ++r)
            for (int c = 0; c < RoomLayout::kCols; ++c)
                Expect(before->tiles[r][c] == after->tiles[r][c] && before->solid[r][c] == after->solid[r][c] &&
                       before->fall[r][c] == after->fall[r][c], label + ": grids identical");

        std::set<std::string> permitted(t.permitted.begin(), t.permitted.end());
        permitted.insert(AtlasStem(t));
        for (size_t i = 0; i < after->visualTiles.size() && i < before->visualTiles.size(); ++i)
        {
            const auto& a = before->visualTiles[i];
            const auto& b = after->visualTiles[i];
            Expect(permitted.count(b.sourceTileset) == 1, label + ": permitted sheet (" + b.sourceTileset + ")");
            Expect(a.col == b.col && a.row == b.row && a.ground == b.ground && a.door == b.door &&
                   a.anchorOffset.x == b.anchorOffset.x && a.anchorOffset.y == b.anchorOffset.y,
                   label + ": visual placement identical");
            const auto* sheet = catalog.Find(b.sourceTileset);
            Expect(sheet != nullptr, label + ": visual sheet in catalog");
            if (!sheet) continue;
            Expect(b.src.x >= 0 && b.src.y >= 0 && b.src.width > 0 && b.src.height > 0 &&
                   b.src.x + b.src.width <= sheet->tileColumns * 16 &&
                   b.src.y + b.src.height <= sheet->tileRows * 16, label + ": sprite inside its image");
        }

        bool spawnSame = true, fallSame = true, doorSame = true;
        for (unsigned char mask : { (unsigned char)0, source->DoorMask() })
        {
            ApplyActiveRoomDoorMask(*before, mask);
            ApplyActiveRoomDoorMask(*after, mask);
            for (int y = 0; y < 256; y += 4)
                for (int x = 0; x < 448; x += 4)
                {
                    Rectangle body{ (float)x, (float)y, 8, 8 };
                    spawnSame = spawnSame && IsRoomSpawnAreaValid(*before, body, 16, 16, beforeProps) ==
                                             IsRoomSpawnAreaValid(*after, body, 16, 16, afterProps);
                    fallSame = fallSame && IsRoomFallPoint(*before, { (float)x, (float)y }, 16, 16) ==
                                           IsRoomFallPoint(*after, { (float)x, (float)y }, 16, 16);
                }
            for (size_t i = 0; i < after->visualTiles.size() && i < before->visualTiles.size(); ++i)
                doorSame = doorSame && RoomVisualClearedByOpenDoor(before->visualTiles[i], *before) ==
                                       RoomVisualClearedByOpenDoor(after->visualTiles[i], *after);
        }
        Expect(spawnSame, label + ": sampled body positions identical");
        Expect(fallSame, label + ": sampled fall positions identical");
        Expect(doorSame, label + ": door clearing identical");
    }

    // Mirrors Engine::GenerateHandcraftedDungeon.
    bool FormsDungeon(const RoomLibrary& library, DungeonGen& gen, Biome biome, bool zephCamp)
    {
        for (int attempt = 0; attempt < 600; ++attempt)
        {
            gen.Generate();
            if (zephCamp) gen.ForceRoomType(gen.GetStartIndex(), RoomType::Store);
            std::vector<LayoutSlot> slots;
            for (const DungeonRoom& room : gen.GetRooms())
                slots.push_back({ room.type, RoomDoorMask(room.hasNorth, room.hasSouth, room.hasEast, room.hasWest) });
            if (!library.AssignDungeonLayouts(biome, slots, [](int n) { return GetRandomValue(0, n - 1); }).empty())
                return true;
        }
        return false;
    }
}

int main()
{
    SetTraceLogLevel(LOG_WARNING);
    SetRandomSeed(20261016);
    RoomAssetCatalog catalog;
    Expect(catalog.Refresh("MapTilesets", "TestGame"), "asset catalog refresh");

    int variants = 0;
    for (const Target& t : Targets())
        for (const auto& entry : std::filesystem::directory_iterator("TestGame/Rooms/Forest/forest"))
        {
            if (entry.path().extension() != ".mroom") continue;
            CheckVariant(t, entry.path().stem().string(), catalog);
            ++variants;
        }

    RoomLibrary library;
    library.Refresh("TestGame/Rooms");
    int built = 0;
    for (const RoomBlueprint& room : library.Rooms())
    {
        const auto* defs = catalog.Find(room.tilesetStem);
        Expect(defs != nullptr, room.id + ": tileset catalog (" + room.tilesetStem + ")");
        if (!defs) continue;
        std::string warning;
        auto layout = BuildRoomLayout(room, defs->definitions, warning, &catalog);
        Expect(layout.has_value(), room.id + ": builds");
        Expect(warning.empty(), room.id + ": builds without warnings (" + warning + ")");
        ++built;
    }

    DungeonGen gen;
    int dungeons = 0;
    for (int b = 0; b < 10; ++b)
    {
        const Biome biome = (Biome)b;
        for (int trial = 0; trial < 10; ++trial)
        {
            Expect(FormsDungeon(library, gen, biome, false), "biome " + std::to_string(b) + " forms a dungeon");
            Expect(FormsDungeon(library, gen, biome, true), "biome " + std::to_string(b) + " forms a Zeph's-camp dungeon");
            dungeons += 2;
        }
        // Every domain size a run can ask for (first domain's 10 rooms up to the
        // final domain's 16 with two elites and two treasure rooms).
        for (int domain = 0; domain <= 5; ++domain)
        {
            gen.SetPlan(DungeonGen::PlanForDomain(domain));
            Expect(FormsDungeon(library, gen, biome, domain > 0),
                   "biome " + std::to_string(b) + " forms a domain-" + std::to_string(domain + 1) + "-size dungeon");
            ++dungeons;
        }
        gen.SetPlan(DungeonGen::Plan{});
    }

    if (g_failures != 0)
    {
        std::cout << g_failures << " forest-variant check(s) failed\n";
        return 1;
    }
    std::cout << "Forest variants passed: " << variants << " variants identical to their sources, "
              << built << " library rooms build cleanly, " << dungeons << " dungeons formed across 10 biomes.\n";
    return 0;
}
