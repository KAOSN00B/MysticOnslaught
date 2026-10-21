#pragma once

#include "RoomBlueprint.h"
#include "RoomAssetCatalog.h"

#include <filesystem>
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

struct RoomRequest
{
    Biome biome = Biome::Caverns;
    std::string tilesetStem;
    RoomType roomType = RoomType::Standard;
    unsigned char doorMask = 0;
};

// What one room of a dungeon graph needs from its authored layout.
struct LayoutSlot
{
    RoomType role = RoomType::Standard;
    unsigned char doorMask = 0;
};

class RoomLibrary
{
public:
    // Re-reads the .mroom files under root. Calling it again for the same root
    // only re-parses files whose size or last-write time changed (and picks up
    // added and removed files), so the editors, which call it after every save,
    // see exactly what is on disk without paying for the whole library.
    void Refresh(const std::filesystem::path& root);
    int  ParsedOnLastRefresh() const { return _parsedOnLastRefresh; }   // diagnostics/tests
    const std::vector<RoomBlueprint>& Rooms() const { return _rooms; }
    std::vector<const RoomBlueprint*> RoomsFor(Biome biome,
                                                std::string_view tilesetStem) const;
    const RoomBlueprint* FindById(std::string_view id) const;
    const RoomBlueprint* Choose(const RoomRequest& request,
                                std::string_view avoidId = {}) const;
    // Editor playtests preserve gameplay identity as well as geometry. Prefer an
    // exact door mask; a four-door blueprint of the same biome and room type is
    // the only fallback and has its unused exits sealed by the runtime graph.
    std::vector<const RoomBlueprint*> PlaytestCandidates(
        Biome biome, RoomType roomType, unsigned char requiredDoorMask) const;
    std::array<int, 16> DoorMaskCounts(Biome biome,
                                       std::string_view tilesetStem) const;

    // Dungeon layout assignment (the normal path of every main-run domain).
    // Each slot takes, in this order of preference:
    //   1. an exact-door layout not yet used in this dungeon;
    //   2. a four-door layout of the same role not yet used (the room graph
    //      seals its unused exits, as it already does for missing shapes);
    //   3. otherwise the least-used compatible layout, exact door shapes first,
    //      so a small pool repeats only when it has to.
    // Elite and Store slots borrow Standard layouts only when their own role has
    // none (as PlaytestCandidates does). Returns an empty vector only when some
    // slot has no compatible layout at all. randomBelow(n) returns 0..n-1.
    static std::vector<std::string> AssignLayouts(const std::vector<const RoomBlueprint*>& rooms,
                                                  const std::vector<LayoutSlot>& slots,
                                                  const std::function<int(int)>& randomBelow);
    std::vector<std::string> AssignDungeonLayouts(Biome biome, const std::vector<LayoutSlot>& slots,
                                                  const std::function<int(int)>& randomBelow) const;
    // The layouts one slot may use: exact-door ones, then four-door ones.
    static void CompatibleLayouts(const std::vector<const RoomBlueprint*>& rooms, const LayoutSlot& slot,
                                  std::vector<const RoomBlueprint*>& exact,
                                  std::vector<const RoomBlueprint*>& fourDoor);
    std::optional<RoomLayout> Resolve(const RoomRequest& request,
                                      const TileDefSet& definitions,
                                      const RoomAssetCatalog* catalog,
                                      std::string_view avoidId,
                                      std::string& selectedId,
                                      std::string& warning) const;

    bool SaveRoom(const RoomBlueprint& room, bool overwrite, std::string& error);
    bool DeleteRoom(std::string_view id, std::string& error);
    bool NameExists(std::string_view name, Biome biome,
                    std::string_view tilesetStem,
                    std::string_view exceptId = {}) const;

    static std::string Slugify(std::string_view name);
    static std::string BiomeFolderName(Biome biome);

private:
    std::filesystem::path PathFor(const RoomBlueprint& room) const;
    bool IsInsideRoot(const std::filesystem::path& path) const;

    // What a file looked like when it was parsed; an unchanged stamp means the
    // parsed room can be kept on the next Refresh.
    struct FileStamp
    {
        std::filesystem::file_time_type writeTime{};
        std::uintmax_t size = 0;
        bool SameAs(const FileStamp& other) const { return writeTime == other.writeTime && size == other.size; }
    };

    std::filesystem::path _root;
    std::vector<RoomBlueprint> _rooms;
    std::vector<std::filesystem::path> _paths;
    std::vector<FileStamp> _stamps;   // parallel to _rooms / _paths
    int _parsedOnLastRefresh = 0;
};
