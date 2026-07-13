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

    assert(library.SaveRoom(crossingA, false, error));
    assert(library.SaveRoom(crossingB, false, error));
    assert(library.SaveRoom(corner, false, error));
    assert(library.SaveRoom(forest, false, error));
    library.Refresh(root);
    assert(library.Rooms().size() == 4);
    assert(library.NameExists("Narrow Crossing"));
    assert(!library.NameExists("Narrow Crossing", "crossing-a"));
    assert(library.FindById("crossing-a") != nullptr);
    assert(library.FindById("missing") == nullptr);

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
        request, definitions, selected->id, selectedId, warning);
    assert(resolved.has_value());
    assert(resolved->handcrafted);
    assert(resolved->sourceRoomId == alternate->id);
    assert(selectedId == alternate->id);

    RoomRequest missingRequest = request;
    missingRequest.roomType = RoomType::Boss;
    selectedId = "unchanged";
    resolved = library.Resolve(missingRequest, definitions, {}, selectedId, warning);
    assert(!resolved.has_value());
    assert(selectedId.empty());

    request.roomType = RoomType::Boss;
    assert(library.Choose(request) == nullptr);
    request.roomType = RoomType::Standard;
    request.doorMask = RoomDoorMask(false, false, true, true);
    assert(library.Choose(request) == nullptr);

    assert(!library.SaveRoom(crossingA, false, error));
    assert(!error.empty());
    assert(library.DeleteRoom("crossing-a", error));
    assert(!library.NameExists("Narrow Crossing"));
    assert(!library.DeleteRoom("does-not-exist", error));

    std::filesystem::remove_all(root, ec);
    return 0;
}
