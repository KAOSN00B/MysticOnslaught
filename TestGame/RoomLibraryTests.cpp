#include "RoomLibrary.h"

#include <cassert>
#include <filesystem>
#include <string>

namespace
{
    RoomBlueprint MakeRoom(const char* id, const char* name, Biome biome,
                          const char* stem, RoomType type,
                          bool north, bool south, bool east, bool west)
    {
        RoomBlueprint room = RoomBlueprint::CreateDefault();
        room.id = id;
        room.name = name;
        room.biome = biome;
        room.tilesetStem = stem;
        room.roomType = type;
        room.hasNorth = north;
        room.hasSouth = south;
        room.hasEast = east;
        room.hasWest = west;
        return room;
    }
}

int main()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "mystic_onslaught_room_library_tests";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);

    RoomLibrary library;
    library.Refresh(root);
    assert(library.Rooms().empty());

    std::string error;
    RoomBlueprint crossingA = MakeRoom("crossing-a", "Narrow Crossing", Biome::Caverns,
        "Caverns", RoomType::Standard, true, true, false, false);
    RoomBlueprint crossingB = MakeRoom("crossing-b", "Flooded Crossing", Biome::Caverns,
        "Caverns", RoomType::Standard, true, true, false, false);
    RoomBlueprint corner = MakeRoom("corner", "Corner Chamber", Biome::Caverns,
        "Caverns", RoomType::Standard, true, false, true, false);
    RoomBlueprint forest = MakeRoom("forest", "Forest Lane", Biome::Forest,
        "Forest", RoomType::Standard, true, true, false, false);
    RoomBlueprint cavernFallback = MakeRoom("cavern-four-way", "Cavern Crossroads", Biome::Caverns,
        "Caverns", RoomType::Standard, true, true, true, true);
    RoomBlueprint forestFallback = MakeRoom("forest-four-way", "Forest Crossroads", Biome::Forest,
        "Forest", RoomType::Standard, true, true, true, true);
    RoomBlueprint treasureCrossing = MakeRoom("treasure-crossing", "Treasure Crossing", Biome::Caverns,
        "Caverns", RoomType::Treasure, true, true, false, false);
    RoomBlueprint treasureFallback = MakeRoom("treasure-four-way", "Treasure Crossroads", Biome::Caverns,
        "Caverns", RoomType::Treasure, true, true, true, true);

    assert(library.SaveRoom(crossingA, false, error));
    assert(library.SaveRoom(crossingB, false, error));
    assert(library.SaveRoom(corner, false, error));
    assert(library.SaveRoom(forest, false, error));
    assert(library.SaveRoom(cavernFallback, false, error));
    assert(library.SaveRoom(forestFallback, false, error));
    assert(library.SaveRoom(treasureCrossing, false, error));
    assert(library.SaveRoom(treasureFallback, false, error));
    library.Refresh(root);
    assert(library.Rooms().size() == 8);

    const auto cavernRooms = library.RoomsFor(Biome::Caverns, "Caverns");
    assert(cavernRooms.size() == 6);
    for (const RoomBlueprint* room : cavernRooms)
    {
        assert(room->biome == Biome::Caverns);
        assert(room->tilesetStem == "Caverns");
    }
    const auto forestRooms = library.RoomsFor(Biome::Forest, "Forest");
    assert(forestRooms.size() == 2);
    assert(library.RoomsFor(Biome::Caverns, "Forest").empty());

    assert(library.NameExists("Narrow Crossing", Biome::Caverns, "Caverns"));
    assert(!library.NameExists("Narrow Crossing", Biome::Forest, "Forest"));
    assert(!library.NameExists("Narrow Crossing", Biome::Caverns, "Caverns", "crossing-a"));

    // Names are unique inside one biome/tileset library, not across the whole
    // game. Every biome can therefore use concise names such as N, EW, or Room 1.
    RoomBlueprint forestSharedName = MakeRoom(
        "forest-shared-name", "Narrow Crossing", Biome::Forest,
        "Forest", RoomType::Standard, false, false, true, true);
    assert(library.SaveRoom(forestSharedName, false, error));
    assert(library.FindById("crossing-a") != nullptr);
    assert(library.FindById("missing") == nullptr);

    // Editor playtest respects authored room type, prefers exact door masks, and
    // uses only a same-type four-way room as its safety net.
    auto exactCandidates = library.PlaytestCandidates(
        Biome::Caverns, RoomType::Standard,
        RoomDoorMask(true, true, false, false));
    assert(exactCandidates.size() == 2);
    for (const RoomBlueprint* room : exactCandidates)
        assert(room->biome == Biome::Caverns &&
               room->DoorMask() == RoomDoorMask(true, true, false, false));

    auto fallbackCandidates = library.PlaytestCandidates(
        Biome::Caverns, RoomType::Standard,
        RoomDoorMask(false, false, true, true));
    assert(fallbackCandidates.size() == 1);
    assert(fallbackCandidates[0]->id == "cavern-four-way");

    auto otherBiomeCandidates = library.PlaytestCandidates(
        Biome::Jungle, RoomType::Standard,
        RoomDoorMask(false, false, true, true));
    assert(otherBiomeCandidates.empty());

    auto treasureCandidates = library.PlaytestCandidates(
        Biome::Caverns, RoomType::Treasure,
        RoomDoorMask(true, true, false, false));
    assert(treasureCandidates.size() == 1);
    assert(treasureCandidates[0]->id == "treasure-crossing");
    auto treasureFourWayFallback = library.PlaytestCandidates(
        Biome::Caverns, RoomType::Treasure,
        RoomDoorMask(false, false, true, true));
    assert(treasureFourWayFallback.size() == 1);
    assert(treasureFourWayFallback[0]->id == "treasure-four-way");

    // Encounter-only Elite rooms do not require a second copy of every authored
    // room shape. When no Elite-specific art exists, use Standard room art with
    // the requested doors; the graph still owns the Elite encounter behavior.
    auto eliteGeometryFallback = library.PlaytestCandidates(
        Biome::Caverns, RoomType::Elite,
        RoomDoorMask(true, true, false, false));
    assert(eliteGeometryFallback.size() == 2);
    for (const RoomBlueprint* room : eliteGeometryFallback)
    {
        assert(room->roomType == RoomType::Standard);
        assert(room->DoorMask() == RoomDoorMask(true, true, false, false));
    }

    RoomRequest request;
    request.biome = Biome::Caverns;
    request.tilesetStem = "Caverns";
    request.roomType = RoomType::Standard;
    request.doorMask = RoomDoorMask(true, true, false, false);
    const RoomBlueprint* selected = library.Choose(request);
    assert(selected != nullptr);
    assert(selected->biome == Biome::Caverns);
    assert(selected->tilesetStem == "Caverns");
    assert(selected->roomType == RoomType::Standard);
    assert(selected->DoorMask() == request.doorMask);

    const RoomBlueprint* alternate = library.Choose(request, selected->id);
    assert(alternate != nullptr);
    assert(alternate->id != selected->id);

    TileDefSet definitions{};
    std::string selectedId;
    std::string warning;
    std::optional<RoomLayout> resolved = library.Resolve(
        request, definitions, nullptr, selected->id, selectedId, warning);
    assert(resolved.has_value());
    assert(resolved->handcrafted);
    assert(resolved->sourceRoomId == alternate->id);
    assert(selectedId == alternate->id);

    RoomRequest missingRequest = request;
    missingRequest.roomType = RoomType::Boss;
    selectedId = "unchanged";
    resolved = library.Resolve(missingRequest, definitions, nullptr, {}, selectedId, warning);
    assert(!resolved.has_value());
    assert(selectedId.empty());

    request.roomType = RoomType::Boss;
    assert(library.Choose(request) == nullptr);
    request.roomType = RoomType::Standard;
    request.doorMask = RoomDoorMask(false, false, true, true);
    assert(library.Choose(request) == nullptr);

    // Coverage counts actual masks for only the requested biome + tileset.
    RoomBlueprint wrongStem = MakeRoom("wrong-stem", "Wrong Stem", Biome::Caverns,
        "Forest", RoomType::Standard, true, true, false, false);
    RoomBlueprint wrongBiome = MakeRoom("wrong-biome", "Wrong Biome", Biome::Forest,
        "Caverns", RoomType::Standard, true, true, false, false);
    assert(library.SaveRoom(wrongStem, false, error));
    assert(library.SaveRoom(wrongBiome, false, error));
    const auto coverage = library.DoorMaskCounts(Biome::Caverns, "Caverns");
    assert(coverage[0] == 0);
    assert(coverage[RoomDoorMask(true, true, false, false)] == 3);
    assert(coverage[RoomDoorMask(true, false, true, false)] == 1);
    assert(coverage[RoomDoorMask(true, true, true, true)] == 2);

    assert(!library.SaveRoom(crossingA, false, error));
    assert(!error.empty());
    assert(library.DeleteRoom("crossing-a", error));
    assert(!library.NameExists("Narrow Crossing", Biome::Caverns, "Caverns"));
    assert(library.NameExists("Narrow Crossing", Biome::Forest, "Forest"));
    assert(!library.DeleteRoom("does-not-exist", error));

    std::filesystem::remove_all(root, ec);
    return 0;
}
