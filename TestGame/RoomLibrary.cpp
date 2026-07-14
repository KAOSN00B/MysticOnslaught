#include "RoomLibrary.h"

#include <algorithm>
#include <cctype>
#include <random>

namespace
{
    std::string FoldAscii(std::string_view text)
    {
        std::string folded;
        folded.reserve(text.size());
        for (unsigned char ch : text) folded.push_back((char)std::tolower(ch));
        return folded;
    }
}

void RoomLibrary::Refresh(const std::filesystem::path& root)
{
    _root = root;
    _rooms.clear();
    _paths.clear();

    std::error_code ec;
    if (!std::filesystem::exists(_root, ec)) return;
    std::filesystem::recursive_directory_iterator it(
        _root, std::filesystem::directory_options::skip_permission_denied, ec);
    const std::filesystem::recursive_directory_iterator end;
    while (!ec && it != end)
    {
        const std::filesystem::directory_entry entry = *it;
        it.increment(ec);
        if (!entry.is_regular_file(ec) || entry.path().extension() != ".mroom") continue;
        std::string error;
        std::optional<RoomBlueprint> room = RoomBlueprint::Load(entry.path(), error);
        if (!room.has_value()) continue;
        _rooms.push_back(std::move(*room));
        _paths.push_back(entry.path());
    }
}

const RoomBlueprint* RoomLibrary::FindById(std::string_view id) const
{
    for (const RoomBlueprint& room : _rooms)
        if (room.id == id) return &room;
    return nullptr;
}

const RoomBlueprint* RoomLibrary::Choose(const RoomRequest& request,
                                         std::string_view avoidId) const
{
    std::vector<const RoomBlueprint*> candidates;
    for (const RoomBlueprint& room : _rooms)
    {
        if (room.biome == request.biome && room.tilesetStem == request.tilesetStem &&
            room.roomType == request.roomType && room.DoorMask() == request.doorMask)
            candidates.push_back(&room);
    }
    if (candidates.size() > 1 && !avoidId.empty())
    {
        candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
            [avoidId](const RoomBlueprint* room) { return room->id == avoidId; }), candidates.end());
    }
    if (candidates.empty()) return nullptr;
    static thread_local std::mt19937 generator(std::random_device{}());
    std::uniform_int_distribution<std::size_t> pick(0, candidates.size() - 1);
    return candidates[pick(generator)];
}

std::vector<const RoomBlueprint*> RoomLibrary::PlaytestCandidates(
    Biome biome, unsigned char requiredDoorMask) const
{
    std::vector<const RoomBlueprint*> exact;
    std::vector<const RoomBlueprint*> fourDoorFallback;
    constexpr unsigned char kFourDoors = 1 | 2 | 4 | 8;

    for (const RoomBlueprint& room : _rooms)
    {
        if (room.biome != biome) continue;
        const unsigned char mask = room.DoorMask();
        if (mask == requiredDoorMask)
            exact.push_back(&room);
        else if (mask == kFourDoors)
            fourDoorFallback.push_back(&room);
    }
    return exact.empty() ? fourDoorFallback : exact;
}

std::optional<RoomLayout> RoomLibrary::Resolve(const RoomRequest& request,
                                                const TileDefSet& definitions,
                                                const RoomAssetCatalog* catalog,
                                                std::string_view avoidId,
                                                std::string& selectedId,
                                                std::string& warning) const
{
    selectedId.clear();
    warning.clear();
    const RoomBlueprint* blueprint = Choose(request, avoidId);
    if (blueprint == nullptr) return std::nullopt;

    std::optional<RoomLayout> layout = BuildRoomLayout(*blueprint, definitions, warning, catalog);
    if (!layout.has_value()) return std::nullopt;
    selectedId = blueprint->id;
    return layout;
}

bool RoomLibrary::NameExists(std::string_view name, std::string_view exceptId) const
{
    const std::string folded = FoldAscii(name);
    for (const RoomBlueprint& room : _rooms)
    {
        if (room.id != exceptId && FoldAscii(room.name) == folded) return true;
    }
    return false;
}

std::string RoomLibrary::Slugify(std::string_view name)
{
    std::string slug;
    bool pendingUnderscore = false;
    for (unsigned char ch : name)
    {
        if (std::isalnum(ch))
        {
            if (pendingUnderscore && !slug.empty()) slug.push_back('_');
            pendingUnderscore = false;
            slug.push_back((char)std::tolower(ch));
        }
        else pendingUnderscore = true;
    }
    return slug.empty() ? "room" : slug;
}

std::string RoomLibrary::BiomeFolderName(Biome biome)
{
    switch (biome)
    {
    case Biome::AncientCastle: return "AncientCastle";
    case Biome::Caverns: return "Caverns";
    case Biome::DemonsInsides: return "DemonsInsides";
    case Biome::DreamRealm: return "DreamRealm";
    case Biome::Forest: return "Forest";
    case Biome::Graveyard: return "Graveyard";
    case Biome::Jungle: return "Jungle";
    case Biome::LostCity: return "LostCity";
    case Biome::TheSanctuary: return "TheSanctuary";
    case Biome::Wastelands: return "Wastelands";
    }
    return "Unknown";
}

std::filesystem::path RoomLibrary::PathFor(const RoomBlueprint& room) const
{
    return _root / BiomeFolderName(room.biome) / Slugify(room.tilesetStem) /
           (Slugify(room.name) + ".mroom");
}

bool RoomLibrary::IsInsideRoot(const std::filesystem::path& path) const
{
    std::error_code ec;
    const std::filesystem::path root = std::filesystem::weakly_canonical(_root, ec);
    if (ec) return false;
    const std::filesystem::path candidate = std::filesystem::weakly_canonical(path, ec);
    if (ec) return false;
    auto rootIt = root.begin();
    auto pathIt = candidate.begin();
    for (; rootIt != root.end(); ++rootIt, ++pathIt)
        if (pathIt == candidate.end() || *rootIt != *pathIt) return false;
    return true;
}

bool RoomLibrary::SaveRoom(const RoomBlueprint& room, bool overwrite, std::string& error)
{
    error.clear();
    if (_root.empty()) { error = "Room library has no root directory"; return false; }
    if (NameExists(room.name, overwrite ? room.id : std::string_view{}))
    {
        error = "A room with that name already exists";
        return false;
    }

    int existingIndex = -1;
    for (int i = 0; i < (int)_rooms.size(); ++i)
        if (_rooms[(std::size_t)i].id == room.id) { existingIndex = i; break; }
    if (existingIndex >= 0 && !overwrite)
    {
        error = "A room with that id already exists";
        return false;
    }

    const std::filesystem::path destination = PathFor(room);
    if (!room.Save(destination, error)) return false;

    if (existingIndex >= 0 && _paths[(std::size_t)existingIndex] != destination)
    {
        const std::filesystem::path oldPath = _paths[(std::size_t)existingIndex];
        if (IsInsideRoot(oldPath))
        {
            std::error_code ec;
            std::filesystem::remove(oldPath, ec);
        }
    }
    Refresh(_root);
    return true;
}

bool RoomLibrary::DeleteRoom(std::string_view id, std::string& error)
{
    error.clear();
    int index = -1;
    for (int i = 0; i < (int)_rooms.size(); ++i)
        if (_rooms[(std::size_t)i].id == id) { index = i; break; }
    if (index < 0) { error = "Room id was not found"; return false; }

    const std::filesystem::path path = _paths[(std::size_t)index];
    if (!IsInsideRoot(path)) { error = "Room path is outside the library"; return false; }
    std::error_code ec;
    if (!std::filesystem::remove(path, ec) || ec)
    {
        error = ec ? "Could not delete room: " + ec.message() : "Room file did not exist";
        return false;
    }
    Refresh(_root);
    return true;
}
