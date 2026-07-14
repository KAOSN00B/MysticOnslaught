#include "RoomBlueprint.h"
#include "RoomAssetCatalog.h"
#include "RoomCollision.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>

#ifdef _WIN32
// Pulling Windows.h in after raylib.h redeclares CloseWindow and ShowCursor.
// Room saving only needs these two Kernel32 exports, so keep the dependency narrow.
extern "C" __declspec(dllimport) int __stdcall MoveFileExW(
    const wchar_t* existingName, const wchar_t* newName, unsigned long flags);
extern "C" __declspec(dllimport) unsigned long __stdcall GetLastError();
constexpr unsigned long kMoveFileReplaceExisting = 0x1;
constexpr unsigned long kMoveFileWriteThrough = 0x8;
#endif

namespace
{
    constexpr float kMinWallDepth = 0.25f;
    constexpr float kMaxWallDepth = 4.0f;

    const char* AssetKindTag(RoomAssetKind kind)
    {
        switch (kind)
        {
        case RoomAssetKind::Prop:      return "PROP";
        case RoomAssetKind::AnimProp:  return "ANIMPROP";
        case RoomAssetKind::Decor:     return "DECOR";
        case RoomAssetKind::AnimDecor: return "ANIMDECOR";
        }
        return "PROP";
    }

    bool ParseAssetKind(const std::string& tag, RoomAssetKind& kind)
    {
        if (tag == "PROP")      kind = RoomAssetKind::Prop;
        else if (tag == "ANIMPROP")  kind = RoomAssetKind::AnimProp;
        else if (tag == "DECOR")     kind = RoomAssetKind::Decor;
        else if (tag == "ANIMDECOR") kind = RoomAssetKind::AnimDecor;
        else return false;
        return true;
    }

    bool ReplaceFile(const std::filesystem::path& source,
                     const std::filesystem::path& destination,
                     std::string& error)
    {
#ifdef _WIN32
        if (MoveFileExW(source.c_str(), destination.c_str(),
                        kMoveFileReplaceExisting | kMoveFileWriteThrough) != 0)
            return true;
        error = "Could not replace room file (Windows error "
              + std::to_string(GetLastError()) + ")";
        return false;
#else
        std::error_code ec;
        std::filesystem::rename(source, destination, ec);
        if (!ec) return true;
        error = "Could not replace room file: " + ec.message();
        return false;
#endif
    }

    bool ParseQuotedValue(const std::string& line, const char* expectedTag,
                          std::string& value)
    {
        std::istringstream in(line);
        std::string tag;
        return (in >> tag >> std::quoted(value)) && tag == expectedTag;
    }
}

unsigned char RoomDoorMask(bool north, bool south, bool east, bool west)
{
    return static_cast<unsigned char>((north ? 1 : 0) |
                                      (south ? 2 : 0) |
                                      (east  ? 4 : 0) |
                                      (west  ? 8 : 0));
}

Rectangle PredeterminedDoorZone(RoomWallSide side)
{
    const float startC = (float)RoomLayout::kDoorStartCol;
    const float startR = (float)RoomLayout::kDoorStartRow;
    const float spanC  = (float)RoomLayout::kDoorSpanCols;
    const float spanR  = (float)RoomLayout::kDoorSpanRows;
    switch (side)
    {
    case RoomWallSide::Top:    return { startC, 0.f, spanC, 1.f };
    case RoomWallSide::Bottom: return { startC, (float)(RoomLayout::kRows - 1), spanC, 1.f };
    case RoomWallSide::Left:   return { 0.f, startR, 1.f, spanR };
    case RoomWallSide::Right:  return { (float)(RoomLayout::kCols - 1), startR, 1.f, spanR };
    }
    return {};
}

void ApplyActiveRoomDoorMask(RoomLayout& layout, unsigned char doorMask)
{
    layout.doorZones[(int)RoomWallSide::Top].enabled = (doorMask & 1) != 0;
    layout.doorZones[(int)RoomWallSide::Bottom].enabled = (doorMask & 2) != 0;
    layout.doorZones[(int)RoomWallSide::Left].enabled = (doorMask & 8) != 0;
    layout.doorZones[(int)RoomWallSide::Right].enabled = (doorMask & 4) != 0;
}

RoomBlueprint RoomBlueprint::CreateDefault()
{
    RoomBlueprint room;
    for (int row = 0; row < RoomLayout::kRows; ++row)
    {
        for (int col = 0; col < RoomLayout::kCols; ++col)
        {
            const bool border = row == 0 || col == 0 ||
                                row == RoomLayout::kRows - 1 ||
                                col == RoomLayout::kCols - 1;
            room.tiles[row][col] = border ? TileType::WallBody : TileType::Floor;
            room.fall[row][col] = false;
            room.solid[row][col] = border;
        }
    }
    // Door zones are predetermined and fixed for every room so they line up 1:1
    // with the procedural dungeon door lanes — the designer only toggles them
    // on/off, never repositions them.
    for (int i = 0; i < 4; ++i)
        room.doorZones[i] = { false, PredeterminedDoorZone((RoomWallSide)i) };
    return room;
}

unsigned char RoomBlueprint::DoorMask() const
{
    return RoomDoorMask(hasNorth, hasSouth, hasEast, hasWest);
}

bool RoomBlueprint::Validate(std::string& error) const
{
    error.clear();
    if (id.empty())            { error = "Room id cannot be empty"; return false; }
    if (name.empty())          { error = "Room name cannot be empty"; return false; }
    if (tilesetStem.empty())   { error = "Room tileset cannot be empty"; return false; }
    if (DoorMask() == 0)       { error = "Room must enable at least one door"; return false; }
    if (placements.size() > kMaxPlacements)
    {
        error = "Room has too many asset placements";
        return false;
    }
    const float wallDepths[] = {
        wallTopDepth, wallBottomDepth, wallLeftDepth, wallRightDepth
    };
    for (float depth : wallDepths)
    {
        if (depth < kMinWallDepth || depth > kMaxWallDepth)
        {
            error = "Room wall collider depths must be between 0.25 and 4 tiles";
            return false;
        }
    }

    const int biomeValue = static_cast<int>(biome);
    if (biomeValue < static_cast<int>(Biome::AncientCastle) ||
        biomeValue > static_cast<int>(Biome::Wastelands))
    {
        error = "Room biome value is invalid";
        return false;
    }

    const int roomTypeValue = static_cast<int>(roomType);
    if (roomTypeValue < static_cast<int>(RoomType::Standard) ||
        roomTypeValue > static_cast<int>(RoomType::Boss))
    {
        error = "Room type value is invalid";
        return false;
    }

    for (int row = 0; row < RoomLayout::kRows; ++row)
    {
        for (int col = 0; col < RoomLayout::kCols; ++col)
        {
            const int tile = static_cast<int>(tiles[row][col]);
            if (tile < 0 || tile >= static_cast<int>(TileType::Count))
            {
                error = "Room contains an invalid tile value";
                return false;
            }
        }
    }

    for (const RoomAssetPlacement& placement : placements)
    {
        if (placement.assetId.empty())
        {
            error = "Room placement has an empty asset id";
            return false;
        }
        if (placement.col < 0 || placement.col >= RoomLayout::kCols ||
            placement.row < 0 || placement.row >= RoomLayout::kRows)
        {
            error = "Room placement is outside the room grid";
            return false;
        }
    }
    for (const RoomTilePlacement& placement : visualTiles)
    {
        if (placement.sourceTileset.empty())
        {
            error = "Room visual tile has an empty source tileset";
            return false;
        }
        if (placement.src.width <= 0.f || placement.src.height <= 0.f)
        {
            error = "Room visual tile has an empty source rectangle";
            return false;
        }
        if (placement.col < 0 || placement.col >= RoomLayout::kCols ||
            placement.row < 0 || placement.row >= RoomLayout::kRows)
        {
            error = "Room visual tile is outside the room grid";
            return false;
        }
    }
    for (const Rectangle& c : colliders)
    {
        if (c.width <= 0.f || c.height <= 0.f ||
            c.x < 0.f || c.y < 0.f ||
            c.x + c.width > RoomLayout::kCols || c.y + c.height > RoomLayout::kRows)
        {
            error = "Room collider rectangle is outside the room grid";
            return false;
        }
    }
    for (const RoomDoorZone& zone : doorZones)
    {
        if (!zone.enabled) continue;
        if (zone.tiles.width <= 0.f || zone.tiles.height <= 0.f ||
            zone.tiles.x < 0.f || zone.tiles.y < 0.f ||
            zone.tiles.x + zone.tiles.width > RoomLayout::kCols ||
            zone.tiles.y + zone.tiles.height > RoomLayout::kRows)
        {
            error = "Room door zone is outside the room grid";
            return false;
        }
    }
    return true;
}

bool RoomBlueprint::Save(const std::filesystem::path& path, std::string& error) const
{
    if (!Validate(error)) return false;

    std::error_code ec;
    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path(), ec);
    if (ec)
    {
        error = "Could not create room directory: " + ec.message();
        return false;
    }

    const std::filesystem::path temporary = path.string() + ".tmp";
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        error = "Could not open temporary room file for writing";
        return false;
    }

    out << "MROOM " << kVersion << '\n';
    out << "ID " << std::quoted(id) << '\n';
    out << "NAME " << std::quoted(name) << '\n';
    out << "BIOME " << static_cast<int>(biome) << '\n';
    out << "TILESET " << std::quoted(tilesetStem) << '\n';
    out << "ROOMTYPE " << static_cast<int>(roomType) << '\n';
    out << "DOORS " << (hasNorth ? 1 : 0) << ' ' << (hasSouth ? 1 : 0) << ' '
        << (hasEast ? 1 : 0) << ' ' << (hasWest ? 1 : 0) << '\n';
    out << "WALL_DEPTHS " << wallTopDepth << ' ' << wallBottomDepth << ' '
        << wallLeftDepth << ' ' << wallRightDepth << '\n';
    out << "TILES_BEGIN\n";
    for (int row = 0; row < RoomLayout::kRows; ++row)
    {
        for (int col = 0; col < RoomLayout::kCols; ++col)
        {
            if (col > 0) out << ' ';
            out << static_cast<int>(tiles[row][col]);
        }
        out << '\n';
    }
    out << "TILES_END\nPLACEMENTS_BEGIN\n";
    for (const RoomAssetPlacement& placement : placements)
    {
        out << AssetKindTag(placement.kind) << ' ' << std::quoted(placement.assetId)
            << ' ' << std::quoted(placement.sourceTileset)
            << ' ' << placement.col << ' ' << placement.row
            << ' ' << (int)placement.band << '\n';
    }
    out << "PLACEMENTS_END\nFALL_BEGIN\n";
    for (int row = 0; row < RoomLayout::kRows; ++row)
    {
        for (int col = 0; col < RoomLayout::kCols; ++col)
            out << (fall[row][col] ? '1' : '0');
        out << '\n';
    }
    out << "FALL_END\n";
    out << "SOLID_BEGIN\n";
    for (int row = 0; row < RoomLayout::kRows; ++row)
    {
        for (int col = 0; col < RoomLayout::kCols; ++col)
            out << (solid[row][col] ? '1' : '0');
        out << '\n';
    }
    out << "SOLID_END\nVISUALS_BEGIN\n";
    for (const RoomTilePlacement& placement : visualTiles)
        out << (placement.ground ? "GROUND " : "VISUAL ")
            << std::quoted(placement.sourceTileset) << ' '
            << (int)placement.type << ' '
            << placement.src.x << ' ' << placement.src.y << ' '
            << placement.src.width << ' ' << placement.src.height << ' '
            << placement.col << ' ' << placement.row << '\n';
    out << "VISUALS_END\n";
    for (int i = 0; i < 4; ++i)
    {
        const RoomDoorZone& zone = doorZones[i];
        out << "DOORZONE " << i << ' ' << (zone.enabled ? 1 : 0) << ' '
            << zone.tiles.x << ' ' << zone.tiles.y << ' '
            << zone.tiles.width << ' ' << zone.tiles.height << '\n';
    }
    for (const Rectangle& c : colliders)
        out << "COLLIDER " << c.x << ' ' << c.y << ' ' << c.width << ' ' << c.height << '\n';
    out.flush();
    if (!out)
    {
        error = "Could not finish writing room file";
        out.close();
        std::filesystem::remove(temporary, ec);
        return false;
    }
    out.close();

    if (!ReplaceFile(temporary, path, error))
    {
        std::filesystem::remove(temporary, ec);
        return false;
    }
    error.clear();
    return true;
}

std::optional<RoomBlueprint> RoomBlueprint::Load(const std::filesystem::path& path,
                                                  std::string& error)
{
    error.clear();
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        error = "Could not open room file";
        return std::nullopt;
    }

    std::string line;
    if (!std::getline(in, line))
    {
        error = "Room file is empty";
        return std::nullopt;
    }
    int fileVersion = 0;
    {
        std::istringstream header(line);
        std::string tag;
        if (!(header >> tag >> fileVersion) || tag != "MROOM")
        {
            error = "Room file header is invalid";
            return std::nullopt;
        }
        if (fileVersion != 1 && fileVersion != kVersion)
        {
            error = "Unsupported room file version";
            return std::nullopt;
        }
    }

    RoomBlueprint room = CreateDefault();
    bool sawTiles = false;
    bool sawPlacements = false;
    bool sawFall = false;
    bool sawSolid = false;

    while (std::getline(in, line))
    {
        if (line.empty()) continue;
        if (line == "TILES_BEGIN")
        {
            for (int row = 0; row < RoomLayout::kRows; ++row)
            {
                if (!std::getline(in, line))
                {
                    error = "Room tile section is truncated";
                    return std::nullopt;
                }
                std::istringstream values(line);
                for (int col = 0; col < RoomLayout::kCols; ++col)
                {
                    int tile = -1;
                    if (!(values >> tile) || tile < 0 || tile >= static_cast<int>(TileType::Count))
                    {
                        error = "Room tile row must contain 28 valid tile values";
                        return std::nullopt;
                    }
                    room.tiles[row][col] = static_cast<TileType>(tile);
                }
                int extra = 0;
                if (values >> extra)
                {
                    error = "Room tile row contains too many values";
                    return std::nullopt;
                }
            }
            if (!std::getline(in, line) || line != "TILES_END")
            {
                error = "Room tile section has no end marker";
                return std::nullopt;
            }
            sawTiles = true;
            continue;
        }
        if (line == "PLACEMENTS_BEGIN")
        {
            bool ended = false;
            while (std::getline(in, line))
            {
                if (line == "PLACEMENTS_END") { ended = true; break; }
                if (line.empty()) continue;
                std::istringstream values(line);
                std::string tag;
                RoomAssetPlacement placement;
                if (!(values >> tag) || !ParseAssetKind(tag, placement.kind) ||
                    !(values >> std::quoted(placement.assetId)))
                {
                    error = "Room placement record is invalid";
                    return std::nullopt;
                }
                if (fileVersion >= 2)
                {
                    if (!(values >> std::quoted(placement.sourceTileset)
                                 >> placement.col >> placement.row))
                    {
                        error = "Room placement source record is invalid";
                        return std::nullopt;
                    }
                    // Optional trailing band (added later within v2) — absent in
                    // older v2 files, which default to the Decor band.
                    int bandValue = 0;
                    if (values >> bandValue && bandValue >= 0 && bandValue <= 2)
                        placement.band = (RoomDrawBand)bandValue;
                }
                else if (!(values >> placement.col >> placement.row))
                {
                    error = "Room placement record is invalid";
                    return std::nullopt;
                }
                if (room.placements.size() >= kMaxPlacements)
                {
                    error = "Room has too many asset placements";
                    return std::nullopt;
                }
                room.placements.push_back(std::move(placement));
            }
            if (!ended)
            {
                error = "Room placement section has no end marker";
                return std::nullopt;
            }
            sawPlacements = true;
            continue;
        }
        if (line == "FALL_BEGIN")
        {
            for (int row = 0; row < RoomLayout::kRows; ++row)
            {
                if (!std::getline(in, line) || line.size() != RoomLayout::kCols)
                {
                    error = "Room fall row must contain exactly 28 cells";
                    return std::nullopt;
                }
                for (int col = 0; col < RoomLayout::kCols; ++col)
                {
                    if (line[col] != '0' && line[col] != '1')
                    {
                        error = "Room fall row contains an invalid cell";
                        return std::nullopt;
                    }
                    room.fall[row][col] = line[col] == '1';
                }
            }
            if (!std::getline(in, line) || line != "FALL_END")
            {
                error = "Room fall section has no end marker";
                return std::nullopt;
            }
            sawFall = true;
            continue;
        }
        if (line == "SOLID_BEGIN")
        {
            for (int row = 0; row < RoomLayout::kRows; ++row)
            {
                if (!std::getline(in, line) || line.size() != RoomLayout::kCols)
                {
                    error = "Room solid row must contain exactly 28 cells";
                    return std::nullopt;
                }
                for (int col = 0; col < RoomLayout::kCols; ++col)
                {
                    if (line[col] != '0' && line[col] != '1')
                    {
                        error = "Room solid row contains an invalid cell";
                        return std::nullopt;
                    }
                    room.solid[row][col] = line[col] == '1';
                }
            }
            if (!std::getline(in, line) || line != "SOLID_END")
            {
                error = "Room solid section has no end marker";
                return std::nullopt;
            }
            sawSolid = true;
            continue;
        }
        if (line == "VISUALS_BEGIN")
        {
            bool ended = false;
            while (std::getline(in, line))
            {
                if (line == "VISUALS_END") { ended = true; break; }
                if (line.empty()) continue;
                std::istringstream values(line);
                std::string tag;
                RoomTilePlacement placement;
                int type = -1;
                if (!(values >> tag >> std::quoted(placement.sourceTileset)
                             >> type >> placement.src.x >> placement.src.y
                             >> placement.src.width >> placement.src.height
                             >> placement.col >> placement.row) ||
                    (tag != "GROUND" && tag != "VISUAL") ||
                    type < 0 || type >= (int)TileType::Count)
                {
                    error = "Room visual tile record is invalid";
                    return std::nullopt;
                }
                placement.ground = tag == "GROUND";
                placement.type = (TileType)type;
                room.visualTiles.push_back(std::move(placement));
            }
            if (!ended) { error = "Room visuals section has no end marker"; return std::nullopt; }
            continue;
        }

        std::istringstream values(line);
        std::string tag;
        values >> tag;
        if (tag == "ID")
        {
            if (!ParseQuotedValue(line, "ID", room.id)) { error = "Invalid room id"; return std::nullopt; }
        }
        else if (tag == "NAME")
        {
            if (!ParseQuotedValue(line, "NAME", room.name)) { error = "Invalid room name"; return std::nullopt; }
        }
        else if (tag == "TILESET")
        {
            if (!ParseQuotedValue(line, "TILESET", room.tilesetStem)) { error = "Invalid room tileset"; return std::nullopt; }
        }
        else if (tag == "BIOME")
        {
            int value = -1;
            if (!(values >> value)) { error = "Invalid room biome"; return std::nullopt; }
            room.biome = static_cast<Biome>(value);
        }
        else if (tag == "ROOMTYPE")
        {
            int value = -1;
            if (!(values >> value)) { error = "Invalid room type"; return std::nullopt; }
            room.roomType = static_cast<RoomType>(value);
        }
        else if (tag == "DOORS")
        {
            int north = 0, south = 0, east = 0, west = 0;
            if (!(values >> north >> south >> east >> west) ||
                north < 0 || north > 1 || south < 0 || south > 1 ||
                east < 0 || east > 1 || west < 0 || west > 1)
            {
                error = "Invalid room door flags";
                return std::nullopt;
            }
            room.hasNorth = north != 0;
            room.hasSouth = south != 0;
            room.hasEast = east != 0;
            room.hasWest = west != 0;
        }
        else if (tag == "WALL_DEPTHS")
        {
            if (!(values >> room.wallTopDepth >> room.wallBottomDepth
                         >> room.wallLeftDepth >> room.wallRightDepth))
            {
                error = "Invalid room wall collider depths";
                return std::nullopt;
            }
        }
        else if (tag == "DOORZONE")
        {
            int index = -1, enabled = 0;
            RoomDoorZone zone;
            if (!(values >> index >> enabled >> zone.tiles.x >> zone.tiles.y
                         >> zone.tiles.width >> zone.tiles.height) ||
                index < 0 || index >= 4 || enabled < 0 || enabled > 1)
            {
                error = "Invalid room door zone";
                return std::nullopt;
            }
            zone.enabled = enabled != 0;
            room.doorZones[index] = zone;
        }
        else if (tag == "COLLIDER")
        {
            Rectangle c{};
            if (!(values >> c.x >> c.y >> c.width >> c.height) ||
                c.width <= 0.f || c.height <= 0.f)
            {
                error = "Invalid room collider rectangle";
                return std::nullopt;
            }
            room.colliders.push_back(c);
        }
        // Unknown metadata tags are ignored for forward compatibility.
    }

    if (!sawTiles || !sawPlacements || !sawFall || (fileVersion >= 2 && !sawSolid))
    {
        error = "Room file is missing a required section";
        return std::nullopt;
    }
    if (fileVersion == 1)
    {
        for (int row = 0; row < RoomLayout::kRows; ++row)
            for (int col = 0; col < RoomLayout::kCols; ++col)
                room.solid[row][col] = IsSolidRoomTile(room.tiles[row][col]);
    }
    // Doors are predetermined now: snap every door zone to its fixed lane and key
    // its enabled state off the room's door flags. Any old free-placed/misaligned
    // zone is replaced, so handcrafted rooms always line up with the dungeon.
    const bool doorEnabled[4] = { room.hasNorth, room.hasSouth, room.hasWest, room.hasEast };
    for (int i = 0; i < 4; ++i)
        room.doorZones[i] = { doorEnabled[i], PredeterminedDoorZone((RoomWallSide)i) };
    if (!room.Validate(error)) return std::nullopt;
    error.clear();
    return room;
}

std::optional<RoomLayout> BuildRoomLayout(const RoomBlueprint& blueprint,
                                          const TileDefSet& definitions,
                                          std::string& warning,
                                          const RoomAssetCatalog* catalog)
{
    warning.clear();
    std::string validationError;
    if (!blueprint.Validate(validationError))
    {
        warning = validationError;
        return std::nullopt;
    }

    RoomLayout layout{};
    layout.handcrafted = true;
    layout.sourceRoomId = blueprint.id;
    layout.wallTopDepth = blueprint.wallTopDepth;
    layout.wallBottomDepth = blueprint.wallBottomDepth;
    layout.wallLeftDepth = blueprint.wallLeftDepth;
    layout.wallRightDepth = blueprint.wallRightDepth;
    for (int i = 0; i < 4; ++i) layout.doorZones[i] = blueprint.doorZones[i];
    layout.colliders = blueprint.colliders;
    layout.visualTiles = blueprint.visualTiles;
    for (int row = 0; row < RoomLayout::kRows; ++row)
    {
        for (int col = 0; col < RoomLayout::kCols; ++col)
        {
            layout.tiles[row][col] = blueprint.tiles[row][col];
            layout.fall[row][col] = blueprint.fall[row][col];
            layout.solid[row][col] = blueprint.solid[row][col];
        }
    }

    int skipped = 0;
    for (const RoomAssetPlacement& placement : blueprint.placements)
    {
        const std::string sourceStem = placement.sourceTileset.empty()
            ? blueprint.tilesetStem : placement.sourceTileset;
        const TileDefSet* sourceDefinitions = &definitions;
        if (sourceStem != blueprint.tilesetStem)
        {
            const RoomAssetSource* source = catalog != nullptr ? catalog->Find(sourceStem) : nullptr;
            sourceDefinitions = source != nullptr ? &source->definitions : nullptr;
        }
        const int definitionIndex = sourceDefinitions != nullptr
            ? sourceDefinitions->FindAssetIndex(placement.kind, placement.assetId) : -1;
        if (definitionIndex < 0)
        {
            ++skipped;
            continue;
        }
        bool sourceStored = false;
        for (const RoomSourceDefinitions& source : layout.assetSources)
            sourceStored = sourceStored || source.stem == sourceStem;
        if (!sourceStored)
            layout.assetSources.push_back({ sourceStem, *sourceDefinitions });
        const SpritePlacement runtimePlacement{
            definitionIndex, placement.col, placement.row, sourceStem, placement.band
        };
        switch (placement.kind)
        {
        case RoomAssetKind::Prop:      layout.props.push_back(runtimePlacement); break;
        case RoomAssetKind::AnimProp:  layout.animProps.push_back(runtimePlacement); break;
        case RoomAssetKind::Decor:     layout.decors.push_back(runtimePlacement); break;
        case RoomAssetKind::AnimDecor: layout.animDecors.push_back(runtimePlacement); break;
        }
    }
    if (skipped > 0)
        warning = "Skipped " + std::to_string(skipped) + " missing room asset reference(s)";
    return layout;
}
