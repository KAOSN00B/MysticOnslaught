#include "RoomEditor.h"
#include "RoomCollision.h"
#include "VirtualCanvas.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

namespace
{
    constexpr std::size_t kHistoryLimit = 128;
    constexpr std::size_t kMaxGroundBrushTiles = 8;

    int TilesFor(float sourcePixels)
    {
        return std::max(1, (int)std::ceil(sourcePixels / 16.f));
    }

    std::string DoorMaskName(unsigned char mask)
    {
        std::string name;
        if ((mask & 1) != 0) name += 'N';
        if ((mask & 2) != 0) name += 'S';
        if ((mask & 4) != 0) name += 'E';
        if ((mask & 8) != 0) name += 'W';
        return name;
    }

    Rectangle FallSurfaceButtonRect(int index)
    {
        return { 1230.f + index * 180.f, 535.f, 165.f, 38.f };
    }

    Rectangle GroundRandomToggleRect() { return { 1210.f, 270.f, 150.f, 34.f }; }
    Rectangle GroundRandomAddRect()    { return { 1370.f, 270.f, 160.f, 34.f }; }
    Rectangle GroundRandomClearRect()  { return { 1540.f, 270.f, 105.f, 34.f }; }
    Rectangle TileOverhangButtonRect(int index, bool groundPalette)
    {
        return { 1210.f + index * 78.f, groundPalette ? 311.f : 270.f, 72.f, 30.f };
    }
    Rectangle GroundBrushPanelRect()   { return { 1200.f, 754.f, 680.f, 234.f }; }
    Rectangle GroundBrushNameRect()    { return { 1210.f, 765.f, 250.f, 32.f }; }
    Rectangle GroundBrushSaveRect()    { return { 1470.f, 765.f, 78.f, 32.f }; }
    Rectangle GroundBrushPrevRect()    { return { 1558.f, 765.f, 38.f, 32.f }; }
    Rectangle GroundBrushNextRect()    { return { 1604.f, 765.f, 38.f, 32.f }; }

    Rectangle GroundBrushCardRect(int index)
    {
        return { 1210.f + (index % 4) * 165.f,
                 808.f + (index / 4) * 82.f, 155.f, 72.f };
    }

    Rectangle GroundBrushMinusRect(int index)
    {
        const Rectangle card = GroundBrushCardRect(index);
        return { card.x + 48.f, card.y + 39.f, 25.f, 25.f };
    }

    Rectangle GroundBrushPlusRect(int index)
    {
        const Rectangle card = GroundBrushCardRect(index);
        return { card.x + 102.f, card.y + 39.f, 25.f, 25.f };
    }

    Rectangle GroundBrushRemoveRect(int index)
    {
        const Rectangle card = GroundBrushCardRect(index);
        return { card.x + 129.f, card.y + 5.f, 20.f, 22.f };
    }

    const char* FallSurfaceLabel(FallSurface surface)
    {
        switch (surface)
        {
        case FallSurface::Water: return "Water";
        case FallSurface::Lava:  return "Lava";
        default:                 return "Void";
        }
    }

    Color FallSurfaceColor(FallSurface surface, unsigned char alpha)
    {
        switch (surface)
        {
        case FallSurface::Water: return { 35, 125, 225, alpha };
        case FallSurface::Lava:  return { 245, 85, 25, alpha };
        default:                 return { 105, 55, 145, alpha };
        }
    }

    void DrawChestMarker(Rectangle bounds, Color color)
    {
        const float rim = std::max(2.f, bounds.width * .08f);
        DrawRectangleRec(bounds, Fade(color, .22f));
        DrawRectangleLinesEx(bounds, rim, color);
        DrawRectangleRec({ bounds.x + bounds.width * .12f,
                           bounds.y + bounds.height * .18f,
                           bounds.width * .76f, bounds.height * .30f },
                         Fade(color, .78f));
        DrawRectangleRec({ bounds.x + bounds.width * .12f,
                           bounds.y + bounds.height * .52f,
                           bounds.width * .76f, bounds.height * .32f },
                         Fade(color, .92f));
        DrawRectangleRec({ bounds.x + bounds.width * .44f,
                           bounds.y + bounds.height * .47f,
                           bounds.width * .12f, bounds.height * .22f },
                         Color{ 70, 45, 12, color.a });
    }
}

std::string RoomEditor::MakeRoomId(const std::string& tilesetStem)
{
    static unsigned long long sequence = 0;
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    return RoomLibrary::Slugify(tilesetStem) + "_" + std::to_string(ticks) +
           "_" + std::to_string(++sequence);
}

void RoomEditor::BindForTesting(const std::string& tilesetStem, Biome biome,
                                const TileDefSet& definitions,
                                const std::filesystem::path& roomRoot)
{
    _definitions = definitions;
    _roomRoot = roomRoot;
    _library.Refresh(roomRoot);
    _room = RoomBlueprint::CreateDefault();
    _room.id = MakeRoomId(tilesetStem);
    _room.name = "New Room";
    _room.tilesetStem = tilesetStem;
    _room.biome = biome;
    _room.roomType = RoomType::Standard;
    _undo.clear();
    _redo.clear();
}

bool RoomEditor::AddGroundBrushTile(const std::string& sourceTileset,
                                    Rectangle source, int weight, Vector2 anchorOffset)
{
    if (sourceTileset.empty() || source.width <= 0.f || source.height <= 0.f ||
        weight < 1 || weight > 20 || _groundBrushTiles.size() >= kMaxGroundBrushTiles)
        return false;
    const auto duplicate = std::find_if(_groundBrushTiles.begin(), _groundBrushTiles.end(),
        [&](const GroundBrushTile& tile)
        {
            return tile.sourceTileset == sourceTileset && tile.source.x == source.x &&
                   tile.source.y == source.y && tile.source.width == source.width &&
                   tile.source.height == source.height;
        });
    if (duplicate != _groundBrushTiles.end()) return false;
    const bool wasReady = _groundBrushTiles.size() >= 2;
    _groundBrushTiles.push_back({ sourceTileset, source, weight, anchorOffset });
    // Enter weighted mode as soon as the mix first becomes usable. Previously
    // the brush silently stayed in single-tile mode, so _selectedRawTile (the
    // most recently selected tile) won regardless of the displayed weights.
    if (!wasReady && _groundBrushTiles.size() >= 2)
        _randomGroundBrushEnabled = true;
    return true;
}

bool RoomEditor::AddSelectedGroundBrushTile()
{
    const RoomAssetSource* source = SelectedSource();
    if (source == nullptr) return false;
    // The first tile is normally the base terrain, so make it common by default.
    // Added variants start rare and can be tuned with the visible +/- controls.
    return AddGroundBrushTile(source->stem, _selectedRawTile,
                              _groundBrushTiles.empty() ? 8 : 1,
                              _selectedTileAnchorOffset);
}

bool RoomEditor::RemoveGroundBrushTile(std::size_t index)
{
    if (index >= _groundBrushTiles.size()) return false;
    _groundBrushTiles.erase(_groundBrushTiles.begin() + (std::ptrdiff_t)index);
    if (_groundBrushTiles.size() < 2) _randomGroundBrushEnabled = false;
    return true;
}

bool RoomEditor::SetGroundBrushWeight(std::size_t index, int weight)
{
    if (index >= _groundBrushTiles.size() || weight < 1 || weight > 20 ||
        _groundBrushTiles[index].weight == weight)
        return false;
    _groundBrushTiles[index].weight = weight;
    return true;
}

void RoomEditor::ClearGroundBrush()
{
    _groundBrushTiles.clear();
    _randomGroundBrushEnabled = false;
}

std::size_t RoomEditor::ChooseGroundBrushIndex(std::uint32_t sample) const
{
    if (_groundBrushTiles.empty()) return std::numeric_limits<std::size_t>::max();
    std::uint32_t total = 0;
    for (const GroundBrushTile& tile : _groundBrushTiles)
        total += (std::uint32_t)std::max(1, tile.weight);
    std::uint32_t value = total > 0 ? sample % total : 0;
    for (std::size_t i = 0; i < _groundBrushTiles.size(); ++i)
    {
        const std::uint32_t weight = (std::uint32_t)std::max(1, _groundBrushTiles[i].weight);
        if (value < weight) return i;
        value -= weight;
    }
    return _groundBrushTiles.size() - 1;
}

const RoomEditor::GroundBrushTile* RoomEditor::GroundBrushTileForCell(int col, int row) const
{
    if (!_randomGroundBrushEnabled || _groundBrushTiles.empty()) return nullptr;
    // FNV-1a plus the cell coordinates gives a stable per-room result. The same
    // value drives preview and painting, while different rooms receive a fresh
    // pattern because their ids differ.
    std::uint32_t sample = 2166136261u;
    for (unsigned char ch : _room.id)
    {
        sample ^= ch;
        sample *= 16777619u;
    }
    sample ^= (std::uint32_t)col + 0x9e3779b9u + (sample << 6) + (sample >> 2);
    sample ^= (std::uint32_t)row + 0x85ebca6bu + (sample << 6) + (sample >> 2);
    const std::size_t index = ChooseGroundBrushIndex(sample);
    return index < _groundBrushTiles.size() ? &_groundBrushTiles[index] : nullptr;
}

std::filesystem::path RoomEditor::GroundBrushPresetFolder() const
{
    return _roomRoot / "_GroundBrushes" / RoomLibrary::BiomeFolderName(_room.biome) /
           RoomLibrary::Slugify(_room.tilesetStem);
}

void RoomEditor::RefreshGroundBrushPresets()
{
    _groundBrushPresetPaths.clear();
    const std::filesystem::path folder = GroundBrushPresetFolder();
    std::error_code ec;
    if (!std::filesystem::exists(folder, ec)) { _groundBrushPresetIndex = -1; return; }
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(folder,
             std::filesystem::directory_options::skip_permission_denied, ec))
    {
        if (ec) break;
        if (entry.is_regular_file(ec) && entry.path().extension() == ".gbrush")
            _groundBrushPresetPaths.push_back(entry.path());
    }
    std::sort(_groundBrushPresetPaths.begin(), _groundBrushPresetPaths.end());
    if (_groundBrushPresetPaths.empty()) _groundBrushPresetIndex = -1;
    else _groundBrushPresetIndex = std::clamp(_groundBrushPresetIndex, 0,
                                              (int)_groundBrushPresetPaths.size() - 1);
}

bool RoomEditor::SaveGroundBrushPreset(const std::string& name, std::string& error)
{
    error.clear();
    if (_groundBrushTiles.empty()) { error = "Add at least one ground tile first"; return false; }
    const std::string slug = RoomLibrary::Slugify(name);
    if (name.empty() || slug.empty()) { error = "Give the ground brush a name"; return false; }
    const std::filesystem::path folder = GroundBrushPresetFolder();
    std::error_code ec;
    std::filesystem::create_directories(folder, ec);
    if (ec) { error = "Could not create ground brush folder: " + ec.message(); return false; }
    const std::filesystem::path path = folder / (slug + ".gbrush");
    std::ofstream out(path, std::ios::trunc);
    if (!out) { error = "Could not open ground brush preset for writing"; return false; }
    out << "MYSTIC_GROUND_BRUSH 2\n";
    out << "name " << std::quoted(name) << "\n";
    for (const GroundBrushTile& tile : _groundBrushTiles)
        out << "tile " << std::quoted(tile.sourceTileset) << ' '
            << tile.source.x << ' ' << tile.source.y << ' '
            << tile.source.width << ' ' << tile.source.height << ' '
            << tile.weight << ' ' << tile.anchorOffset.x << ' '
            << tile.anchorOffset.y << "\n";
    if (!out) { error = "Could not finish writing ground brush preset"; return false; }
    _groundBrushPresetName = name;
    RefreshGroundBrushPresets();
    for (int i = 0; i < (int)_groundBrushPresetPaths.size(); ++i)
        if (_groundBrushPresetPaths[(std::size_t)i] == path) { _groundBrushPresetIndex = i; break; }
    return true;
}

bool RoomEditor::LoadGroundBrushPresetPath(const std::filesystem::path& path,
                                           std::string& error)
{
    error.clear();
    std::ifstream in(path);
    if (!in) { error = "Ground brush preset was not found"; return false; }
    std::string magic;
    int version = 0;
    if (!(in >> magic >> version) || magic != "MYSTIC_GROUND_BRUSH" ||
        version < 1 || version > 2)
    { error = "Unsupported ground brush preset"; return false; }

    std::string loadedName;
    std::vector<GroundBrushTile> loaded;
    std::string command;
    while (in >> command)
    {
        if (command == "name")
        {
            if (!(in >> std::quoted(loadedName)))
            { error = "Malformed ground brush name"; return false; }
        }
        else if (command == "tile")
        {
            GroundBrushTile tile;
            if (!(in >> std::quoted(tile.sourceTileset) >> tile.source.x >> tile.source.y >>
                      tile.source.width >> tile.source.height >> tile.weight) ||
                tile.sourceTileset.empty() || tile.source.width <= 0.f ||
                tile.source.height <= 0.f || tile.weight < 1 || tile.weight > 20 ||
                loaded.size() >= kMaxGroundBrushTiles)
            { error = "Malformed ground brush tile"; return false; }
            if (version >= 2 && !(in >> tile.anchorOffset.x >> tile.anchorOffset.y))
            { error = "Malformed ground brush tile anchor"; return false; }
            const auto duplicate = std::find_if(loaded.begin(), loaded.end(),
                [&](const GroundBrushTile& other)
                {
                    return other.sourceTileset == tile.sourceTileset &&
                           other.source.x == tile.source.x && other.source.y == tile.source.y &&
                           other.source.width == tile.source.width &&
                           other.source.height == tile.source.height;
                });
            if (duplicate != loaded.end()) { error = "Ground brush contains a duplicate tile"; return false; }
            loaded.push_back(std::move(tile));
        }
        else
        { error = "Unknown ground brush command: " + command; return false; }
    }
    if (loaded.empty()) { error = "Ground brush preset has no tiles"; return false; }
    _groundBrushTiles = std::move(loaded);
    _groundBrushPresetName = loadedName.empty() ? path.stem().string() : loadedName;
    _randomGroundBrushEnabled = _groundBrushTiles.size() > 1;
    return true;
}

bool RoomEditor::LoadGroundBrushPreset(const std::string& name, std::string& error)
{
    const std::filesystem::path path = GroundBrushPresetFolder() /
        (RoomLibrary::Slugify(name) + ".gbrush");
    const bool loaded = LoadGroundBrushPresetPath(path, error);
    if (loaded)
    {
        RefreshGroundBrushPresets();
        for (int i = 0; i < (int)_groundBrushPresetPaths.size(); ++i)
            if (_groundBrushPresetPaths[(std::size_t)i] == path) { _groundBrushPresetIndex = i; break; }
    }
    return loaded;
}

bool RoomEditor::IsCellValid(int col, int row) const
{
    return col >= 0 && col < RoomLayout::kCols &&
           row >= 0 && row < RoomLayout::kRows;
}

const RoomAssetSource* RoomEditor::SelectedSource() const
{
    const auto& sources = _catalog.Sources();
    return _selectedSource >= 0 && _selectedSource < (int)sources.size()
        ? &sources[(std::size_t)_selectedSource] : nullptr;
}

const TileDefSet& RoomEditor::SelectedDefinitions() const
{
    const RoomAssetSource* source = SelectedSource();
    return source != nullptr ? source->definitions : _definitions;
}

const TileDefSet* RoomEditor::DefinitionsFor(const std::string& stem) const
{
    if (stem.empty() || stem == _room.tilesetStem) return &_definitions;
    const RoomAssetSource* source = _catalog.Find(stem);
    return source != nullptr ? &source->definitions : nullptr;
}

void RoomEditor::PushUndo()
{
    // While a stroke / fill is in progress the first PushUndo already snapshotted
    // the room, so the rest of the operation collapses into that one undo entry.
    if (_suppressUndo) return;
    if (_undo.size() >= kHistoryLimit) _undo.erase(_undo.begin());
    _undo.push_back(_room);
    _redo.clear();
}

bool RoomEditor::LayerOwnsKind(Layer layer, RoomAssetKind kind)
{
    if (layer == Layer::Props)
        return kind == RoomAssetKind::Prop || kind == RoomAssetKind::AnimProp;
    if (layer == Layer::Decor)
        return kind == RoomAssetKind::Decor || kind == RoomAssetKind::AnimDecor;
    return false;
}

bool RoomEditor::DecorModeOnTileLayer() const
{
    return (_layer == Layer::Ground || _layer == Layer::Visual) && _tileLayerDecorMode;
}

bool RoomEditor::ShowingAssetPalette() const
{
    return _layer == Layer::Props || _layer == Layer::Decor || DecorModeOnTileLayer();
}

bool RoomEditor::PaletteShowsProps() const
{
    return _layer == Layer::Props;   // Decor layer + decor-mode both show decors
}

RoomDrawBand RoomEditor::CurrentTileBand() const
{
    return _layer == Layer::Ground ? RoomDrawBand::Ground : RoomDrawBand::Visual;
}

// Remove any Decor/AnimDecor placement sitting at (col,row) that belongs to the
// active tile band (so re-painting a cell overwrites cleanly).
bool RoomEditor::EraseBandedDecorAt(int col, int row)
{
    const RoomDrawBand band = CurrentTileBand();
    auto found = std::find_if(_room.placements.rbegin(), _room.placements.rend(),
        [=](const RoomAssetPlacement& p)
        {
            return p.col == col && p.row == row && p.band == band &&
                   (p.kind == RoomAssetKind::Decor || p.kind == RoomAssetKind::AnimDecor);
        });
    if (found == _room.placements.rend()) return false;
    PushUndo();
    _room.placements.erase(std::next(found).base());
    return true;
}

bool RoomEditor::PaintBandedDecor(int col, int row)
{
    if (!IsCellValid(col, row) || _selectedAssetId.empty()) return false;
    if (_selectedAssetKind != RoomAssetKind::Decor &&
        _selectedAssetKind != RoomAssetKind::AnimDecor) return false;
    const RoomDrawBand band = CurrentTileBand();
    const RoomAssetSource* source = SelectedSource();
    RoomAssetPlacement placement{ _selectedAssetKind, _selectedAssetId, col, row,
        source != nullptr ? source->stem : _room.tilesetStem, band };
    // Overwrite a same-band decor already on this cell.
    for (auto it = _room.placements.begin(); it != _room.placements.end(); ++it)
        if (it->col == col && it->row == row && it->band == band &&
            (it->kind == RoomAssetKind::Decor || it->kind == RoomAssetKind::AnimDecor))
        {
            if (it->assetId == placement.assetId && it->kind == placement.kind &&
                it->sourceTileset == placement.sourceTileset)
                return false;   // identical — nothing to do
            PushUndo();
            *it = placement;
            return true;
        }
    PushUndo();
    _room.placements.push_back(placement);
    return true;
}

bool RoomEditor::EraseVisual(int col, int row, bool ground, bool door)
{
    auto found = std::find_if(_room.visualTiles.rbegin(), _room.visualTiles.rend(),
        [=](const RoomTilePlacement& visual)
        {
            return visual.col == col && visual.row == row &&
                   visual.ground == ground && visual.door == door;
        });
    if (found == _room.visualTiles.rend()) return false;
    PushUndo();
    _room.visualTiles.erase(std::next(found).base());
    return true;
}

bool RoomEditor::EraseAt(int col, int row)
{
    if (!IsCellValid(col, row)) return false;
    switch (_layer)
    {
    case Layer::Ground:
    case Layer::Visual:
        if (DecorModeOnTileLayer()) return EraseBandedDecorAt(col, row);
        return EraseVisual(col, row, _layer == Layer::Ground);
    case Layer::Door:
        return EraseVisual(col, row, false, true);
    case Layer::Collision: return SetSolid(col, row, false);
    case Layer::FallZones: return SetFall(col, row, false);
    case Layer::ChestSpawn:
        return _room.treasureChestCol == col && _room.treasureChestRow == row
            ? ClearTreasureChestSpawn() : false;
    case Layer::Props:
    case Layer::Decor:
    {
        auto found = std::find_if(_room.placements.rbegin(), _room.placements.rend(),
            [=](const RoomAssetPlacement& placement)
            {
                return placement.col == col && placement.row == row &&
                       LayerOwnsKind(_layer, placement.kind);
            });
        if (found == _room.placements.rend()) return false;
        PushUndo();
        _room.placements.erase(std::next(found).base());
        return true;
    }
    default: return false;
    }
}

// Paint a single cell of the active layer. add=true paints/places, add=false
// erases/clears. Shared by the brush drag, rectangle fill and bucket fill so the
// three tools stay in lockstep.
bool RoomEditor::PaintCell(int col, int row, bool add)
{
    switch (_layer)
    {
    case Layer::Ground:
    case Layer::Visual:
    {
        const bool ground = _layer == Layer::Ground;
        // Decor mode paints banded Decor/AnimDecor assets instead of flat tiles.
        if (DecorModeOnTileLayer())
            return add ? PaintBandedDecor(col, row) : EraseBandedDecorAt(col, row);
        if (!add) return EraseVisual(col, row, ground);
        if (ground)
            if (const GroundBrushTile* tile = GroundBrushTileForCell(col, row))
                return SetVisual(col, row, true, tile->sourceTileset, tile->source,
                                 tile->anchorOffset);
        const RoomAssetSource* source = SelectedSource();
        if (source == nullptr) return false;
        return SetVisual(col, row, ground, source->stem, _selectedRawTile,
                         _selectedTileAnchorOffset);
    }
    case Layer::Door:
    {
        if (!add) return EraseVisual(col, row, false, true);
        const RoomAssetSource* source = SelectedSource();
        if (source == nullptr) return false;
        return SetDoorVisual(col, row, source->stem, _selectedRawTile,
                             _selectedTileAnchorOffset);
    }
    case Layer::Collision: return SetSolid(col, row, add);
    case Layer::FallZones: return SetFall(col, row, add);
    default: return false;   // Props/Decor/DoorZones are not brush-painted
    }
}

bool RoomEditor::FillRect(int col0, int row0, int col1, int row1, bool add)
{
    int c0 = std::max(0, std::min(col0, col1));
    int r0 = std::max(0, std::min(row0, row1));
    int c1 = std::min(RoomLayout::kCols - 1, std::max(col0, col1));
    int r1 = std::min(RoomLayout::kRows - 1, std::max(row0, row1));
    if (c1 < c0 || r1 < r0) return false;

    const bool nested = _suppressUndo;
    if (!nested) { PushUndo(); _suppressUndo = true; }
    bool changed = false;
    for (int r = r0; r <= r1; ++r)
        for (int c = c0; c <= c1; ++c)
            changed = PaintCell(c, r, add) || changed;
    if (!nested)
    {
        _suppressUndo = false;
        if (!changed && !_undo.empty()) _undo.pop_back(); // drop the no-op snapshot
    }
    return changed;
}

bool RoomEditor::FloodFillFrom(int col, int row, bool add)
{
    if (!IsCellValid(col, row)) return false;

    // Region membership is decided against the ORIGINAL grid: two cells match
    // when they carry the same value on the active layer as the seed cell.
    auto sig = [&](int c, int r) -> std::string
    {
        switch (_layer)
        {
        case Layer::Collision: return _room.solid[r][c] ? "1" : "0";
        case Layer::FallZones: return _room.fall[r][c] ? "1" : "0";
        case Layer::Ground:
        case Layer::Visual:
        case Layer::Door:
        {
            if (DecorModeOnTileLayer())
            {
                const RoomDrawBand band = CurrentTileBand();
                for (auto it = _room.placements.rbegin(); it != _room.placements.rend(); ++it)
                    if (it->col == c && it->row == r && it->band == band &&
                        (it->kind == RoomAssetKind::Decor || it->kind == RoomAssetKind::AnimDecor))
                        return "D:" + it->sourceTileset + "@" + it->assetId;
                return {}; // empty cell
            }
            const bool ground = _layer == Layer::Ground;
            const bool door = _layer == Layer::Door;
            for (auto it = _room.visualTiles.rbegin(); it != _room.visualTiles.rend(); ++it)
                if (it->col == c && it->row == r && it->ground == ground && it->door == door)
                    return it->sourceTileset + "@" + std::to_string((int)it->src.x) + "," +
                           std::to_string((int)it->src.y);
            return {}; // empty cell
        }
        default: return {};
        }
    };
    const std::string seed = sig(col, row);

    std::vector<std::pair<int, int>> region;
    std::vector<char> seen((std::size_t)(RoomLayout::kCols * RoomLayout::kRows), 0);
    std::vector<std::pair<int, int>> stack{ { col, row } };
    auto mark = [&](int c, int r) -> char& { return seen[(std::size_t)(r * RoomLayout::kCols + c)]; };
    mark(col, row) = 1;
    while (!stack.empty())
    {
        auto [c, r] = stack.back(); stack.pop_back();
        if (sig(c, r) != seed) continue;
        region.push_back({ c, r });
        const int dc[4] = { 1, -1, 0, 0 };
        const int dr[4] = { 0, 0, 1, -1 };
        for (int k = 0; k < 4; ++k)
        {
            int nc = c + dc[k], nr = r + dr[k];
            if (!IsCellValid(nc, nr) || mark(nc, nr)) continue;
            mark(nc, nr) = 1;
            stack.push_back({ nc, nr });
        }
    }

    const bool nested = _suppressUndo;
    if (!nested) { PushUndo(); _suppressUndo = true; }
    bool changed = false;
    for (const auto& cell : region)
        changed = PaintCell(cell.first, cell.second, add) || changed;
    if (!nested)
    {
        _suppressUndo = false;
        if (!changed && !_undo.empty()) _undo.pop_back();
    }
    return changed;
}

bool RoomEditor::ClearActiveLayer()
{
    PushUndo();
    bool changed = false;
    switch (_layer)
    {
    case Layer::Ground:
    case Layer::Visual:
    case Layer::Door:
    {
        const bool ground = _layer == Layer::Ground;
        const bool door = _layer == Layer::Door;
        const std::size_t before = _room.visualTiles.size();
        _room.visualTiles.erase(std::remove_if(_room.visualTiles.begin(), _room.visualTiles.end(),
            [=](const RoomTilePlacement& v)
            { return v.ground == ground && v.door == door; }), _room.visualTiles.end());
        changed = _room.visualTiles.size() != before;
        break;
    }
    case Layer::Collision:
        for (auto& rowArr : _room.solid) for (bool& s : rowArr) { changed = changed || s; s = false; }
        if (!_room.colliders.empty()) { _room.colliders.clear(); changed = true; }
        _selectedCollider = -1;
        break;
    case Layer::FallZones:
        for (auto& rowArr : _room.fall) for (bool& f : rowArr) { changed = changed || f; f = false; }
        if (!_room.fallRects.empty()) { _room.fallRects.clear(); changed = true; }
        _selectedCollider = -1;
        break;
    case Layer::Props:
    case Layer::Decor:
    {
        const std::size_t before = _room.placements.size();
        const Layer layer = _layer;
        _room.placements.erase(std::remove_if(_room.placements.begin(), _room.placements.end(),
            [=](const RoomAssetPlacement& p) { return LayerOwnsKind(layer, p.kind); }), _room.placements.end());
        changed = _room.placements.size() != before;
        break;
    }
    case Layer::ChestSpawn:
        changed = _room.HasTreasureChestSpawn();
        _room.treasureChestCol = -1;
        _room.treasureChestRow = -1;
        break;
    default: break;
    }
    if (!changed && !_undo.empty()) _undo.pop_back();
    return changed;
}

void RoomEditor::SelectSourceByStem(const std::string& stem)
{
    const auto& sources = _catalog.Sources();
    for (int i = 0; i < (int)sources.size(); ++i)
        if (sources[(std::size_t)i].stem == stem) { _selectedSource = i; return; }
}

void RoomEditor::PickAt(int col, int row)
{
    if (!IsCellValid(col, row)) return;
    if (DecorModeOnTileLayer())
    {
        const RoomDrawBand band = CurrentTileBand();
        for (auto it = _room.placements.rbegin(); it != _room.placements.rend(); ++it)
            if (it->col == col && it->row == row && it->band == band &&
                (it->kind == RoomAssetKind::Decor || it->kind == RoomAssetKind::AnimDecor))
            {
                _selectedAssetKind = it->kind;
                _selectedAssetId = it->assetId;
                SelectSourceByStem(it->sourceTileset);
                return;
            }
        return;
    }
    if (_layer == Layer::Ground || _layer == Layer::Visual || _layer == Layer::Door)
    {
        const bool ground = _layer == Layer::Ground;
        const bool door = _layer == Layer::Door;
        for (auto it = _room.visualTiles.rbegin(); it != _room.visualTiles.rend(); ++it)
            if (it->col == col && it->row == row && it->ground == ground && it->door == door)
            {
                _selectedRawTile = it->src;
                _selectedTileAnchorOffset = it->anchorOffset;
                SelectSourceByStem(it->sourceTileset);
                return;
            }
    }
    else if (_layer == Layer::Props || _layer == Layer::Decor)
    {
        for (auto it = _room.placements.rbegin(); it != _room.placements.rend(); ++it)
            if (it->col == col && it->row == row && LayerOwnsKind(_layer, it->kind))
            {
                _selectedAssetKind = it->kind;
                _selectedAssetId = it->assetId;
                SelectSourceByStem(it->sourceTileset);
                return;
            }
    }
}

RoomBlueprint RoomEditor::Duplicate(const RoomBlueprint& source)
{
    RoomBlueprint copy = source;
    copy.id = MakeRoomId(source.tilesetStem);
    copy.name = source.name + " copy";
    return copy;
}

bool RoomEditor::SetTerrain(int col, int row, TileType tile)
{
    if (!IsCellValid(col, row) || tile < TileType::Floor || tile >= TileType::Count)
        return false;
    if (_room.tiles[row][col] == tile) return false;
    PushUndo();
    _room.tiles[row][col] = tile;
    return true;
}

bool RoomEditor::SetVisual(int col, int row, bool ground, const std::string& source,
                           Rectangle sourceRect, Vector2 anchorOffset)
{
    if (!IsCellValid(col, row) || source.empty() ||
        sourceRect.width <= 0.f || sourceRect.height <= 0.f) return false;
    auto& visuals = _room.visualTiles;
    const auto same = std::find_if(visuals.begin(), visuals.end(),
        [=](const RoomTilePlacement& item)
        {
            return item.col == col && item.row == row && item.ground == ground && !item.door &&
                   item.sourceTileset == source && item.src.x == sourceRect.x &&
                   item.src.y == sourceRect.y && item.src.width == sourceRect.width &&
                   item.src.height == sourceRect.height &&
                   item.anchorOffset.x == anchorOffset.x &&
                   item.anchorOffset.y == anchorOffset.y;
        });
    if (same != visuals.end()) return false;
    PushUndo();
    visuals.erase(std::remove_if(visuals.begin(), visuals.end(),
        [=](const RoomTilePlacement& item)
        {
            return item.col == col && item.row == row && item.ground == ground && !item.door;
        }), visuals.end());
    RoomTilePlacement placement{ source, TileType::Floor, ground, sourceRect, col, row };
    placement.anchorOffset = anchorOffset;
    visuals.push_back(std::move(placement));
    return true;
}

bool RoomEditor::DoorVisualFitsEnabledZone(int col, int row, Rectangle sourceRect,
                                           Vector2 anchorOffset) const
{
    if (!IsCellValid(col, row) || sourceRect.width <= 0.f || sourceRect.height <= 0.f)
        return false;
    const Rectangle occupied{ (float)col - anchorOffset.x / 16.f,
                              (float)row - anchorOffset.y / 16.f,
                              sourceRect.width / 16.f, sourceRect.height / 16.f };
    constexpr float epsilon = 0.001f;
    for (const RoomDoorZone& zone : _room.doorZones)
    {
        if (!zone.enabled) continue;
        if (occupied.x + epsilon >= zone.tiles.x &&
            occupied.y + epsilon >= zone.tiles.y &&
            occupied.x + occupied.width <= zone.tiles.x + zone.tiles.width + epsilon &&
            occupied.y + occupied.height <= zone.tiles.y + zone.tiles.height + epsilon)
            return true;
    }
    return false;
}

bool RoomEditor::SetDoorVisual(int col, int row, const std::string& source,
                               Rectangle sourceRect, Vector2 anchorOffset)
{
    if (source.empty() ||
        !DoorVisualFitsEnabledZone(col, row, sourceRect, anchorOffset)) return false;
    auto& visuals = _room.visualTiles;
    const auto same = std::find_if(visuals.begin(), visuals.end(),
        [=](const RoomTilePlacement& item)
        {
            return item.col == col && item.row == row && item.door &&
                   item.sourceTileset == source && item.src.x == sourceRect.x &&
                   item.src.y == sourceRect.y && item.src.width == sourceRect.width &&
                   item.src.height == sourceRect.height &&
                   item.anchorOffset.x == anchorOffset.x &&
                   item.anchorOffset.y == anchorOffset.y;
        });
    if (same != visuals.end()) return false;
    PushUndo();
    visuals.erase(std::remove_if(visuals.begin(), visuals.end(),
        [=](const RoomTilePlacement& item)
        { return item.col == col && item.row == row && item.door; }), visuals.end());
    RoomTilePlacement placement{ source, TileType::Floor, false, sourceRect, col, row };
    placement.door = true;
    placement.anchorOffset = anchorOffset;
    visuals.push_back(std::move(placement));
    return true;
}

bool RoomEditor::AdjustSelectedTileOverhang(RoomWallSide side, int delta)
{
    const RoomAssetSource* source = SelectedSource();
    if (source == nullptr) return false;
    return AdjustTileSelectionOverhang(
        _selectedRawTile, _selectedTileAnchorOffset, side, delta,
        (float)(source->tileColumns * 16), (float)(source->tileRows * 16));
}

bool RoomEditor::SetSolid(int col, int row, bool enabled)
{
    if (!IsCellValid(col, row) || _room.solid[row][col] == enabled) return false;
    PushUndo();
    _room.solid[row][col] = enabled;
    return true;
}

bool RoomEditor::SetFall(int col, int row, bool enabled)
{
    if (!IsCellValid(col, row)) return false;
    if (_room.fall[row][col] == enabled) return false;
    PushUndo();
    _room.fall[row][col] = enabled;
    return true;
}

bool RoomEditor::SetFallSurface(FallSurface surface)
{
    const int value = static_cast<int>(surface);
    if (value < static_cast<int>(FallSurface::Void) ||
        value > static_cast<int>(FallSurface::Lava) ||
        _room.fallSurface == surface)
        return false;
    PushUndo();
    _room.fallSurface = surface;
    return true;
}

bool RoomEditor::TreasureChestSpawnFits(int col, int row) const
{
    if (_room.roomType != RoomType::Treasure || !IsCellValid(col, row)) return false;
    const TileType terrain = _room.tiles[row][col];
    if ((terrain != TileType::Floor && terrain != TileType::FloorVariant) ||
        _room.solid[row][col] || _room.fall[row][col])
        return false;

    const Rectangle chest{ (float)col, (float)row, 1.f, 1.f };
    for (const Rectangle& collider : _room.colliders)
        if (CheckCollisionRecs(chest, collider)) return false;
    for (const Rectangle& fall : _room.fallRects)
        if (CheckCollisionRecs(chest, fall)) return false;
    for (const RoomDoorZone& door : _room.doorZones)
        if (door.enabled && CheckCollisionRecs(chest, door.tiles)) return false;

    for (const RoomAssetPlacement& placement : _room.placements)
    {
        if (placement.kind != RoomAssetKind::Prop &&
            placement.kind != RoomAssetKind::AnimProp)
            continue;
        const TileDefSet* definitions = DefinitionsFor(placement.sourceTileset);
        if (definitions == nullptr) continue;
        const int index = definitions->FindAssetIndex(placement.kind, placement.assetId);
        if (index < 0) continue;

        Rectangle source{};
        Rectangle collision{};
        if (placement.kind == RoomAssetKind::Prop)
        {
            source = definitions->props[(std::size_t)index].src;
            collision = definitions->props[(std::size_t)index].collision;
        }
        else
        {
            const AnimPropDef& prop = definitions->animProps[(std::size_t)index];
            if (prop.frames.empty()) continue;
            source = prop.frames[0];
            collision = prop.collision;
        }
        if (collision.width <= 0.f || collision.height <= 0.f)
            collision = { 0.f, 0.f, source.width, source.height };
        const Rectangle propCollision{
            placement.col + collision.x / 16.f,
            placement.row + collision.y / 16.f,
            collision.width / 16.f,
            collision.height / 16.f
        };
        if (CheckCollisionRecs(chest, propCollision)) return false;
    }
    return true;
}

bool RoomEditor::SetTreasureChestSpawn(int col, int row)
{
    if (!TreasureChestSpawnFits(col, row) ||
        (_room.treasureChestCol == col && _room.treasureChestRow == row))
        return false;
    PushUndo();
    _room.treasureChestCol = col;
    _room.treasureChestRow = row;
    return true;
}

bool RoomEditor::ClearTreasureChestSpawn()
{
    if (!_room.HasTreasureChestSpawn()) return false;
    PushUndo();
    _room.treasureChestCol = -1;
    _room.treasureChestRow = -1;
    return true;
}

bool RoomEditor::PlacementFits(const RoomAssetPlacement& placement) const
{
    const TileDefSet* definitions = DefinitionsFor(placement.sourceTileset);
    if (definitions == nullptr) return false;
    const int index = definitions->FindAssetIndex(placement.kind, placement.assetId);
    if (index < 0 || !IsCellValid(placement.col, placement.row)) return false;

    Rectangle source{};
    switch (placement.kind)
    {
    case RoomAssetKind::Prop: source = definitions->props[(std::size_t)index].src; break;
    case RoomAssetKind::AnimProp:
        if (definitions->animProps[(std::size_t)index].frames.empty()) return false;
        source = definitions->animProps[(std::size_t)index].frames[0]; break;
    case RoomAssetKind::Decor: source = definitions->decors[(std::size_t)index].src; break;
    case RoomAssetKind::AnimDecor:
        if (definitions->animDecors[(std::size_t)index].frames.empty()) return false;
        source = definitions->animDecors[(std::size_t)index].frames[0]; break;
    }
    const int width = TilesFor(source.width);
    const int height = TilesFor(source.height);
    if (placement.col + width > RoomLayout::kCols ||
        placement.row + height > RoomLayout::kRows)
        return false;

    if (_room.HasTreasureChestSpawn() &&
        (placement.kind == RoomAssetKind::Prop ||
         placement.kind == RoomAssetKind::AnimProp))
    {
        Rectangle collision = placement.kind == RoomAssetKind::Prop
            ? definitions->props[(std::size_t)index].collision
            : definitions->animProps[(std::size_t)index].collision;
        if (collision.width <= 0.f || collision.height <= 0.f)
            collision = { 0.f, 0.f, source.width, source.height };
        const Rectangle propCollision{
            placement.col + collision.x / 16.f,
            placement.row + collision.y / 16.f,
            collision.width / 16.f,
            collision.height / 16.f
        };
        const Rectangle chest{
            (float)_room.treasureChestCol, (float)_room.treasureChestRow, 1.f, 1.f
        };
        if (CheckCollisionRecs(propCollision, chest)) return false;
    }

    for (const RoomAssetPlacement& existing : _room.placements)
    {
        if (existing.col == placement.col && existing.row == placement.row &&
            existing.kind == placement.kind)
            return false;
    }
    return true;
}

bool RoomEditor::PlaceAsset(const RoomAssetPlacement& placement)
{
    if (!PlacementFits(placement)) return false;
    PushUndo();
    _room.placements.push_back(placement);
    return true;
}

bool RoomEditor::RemoveAssetAt(int col, int row, RoomAssetKind kind)
{
    auto found = std::find_if(_room.placements.rbegin(), _room.placements.rend(),
        [=](const RoomAssetPlacement& placement)
        {
            return placement.col == col && placement.row == row && placement.kind == kind;
        });
    if (found == _room.placements.rend()) return false;
    PushUndo();
    _room.placements.erase(std::next(found).base());
    return true;
}

bool RoomEditor::CreateRoomForDoorMask(unsigned char mask)
{
    if (mask == 0 || mask > 15) return false;

    RoomBlueprint room = RoomBlueprint::CreateDefault();
    room.id = MakeRoomId(_room.tilesetStem);
    room.name = DoorMaskName(mask) + " Room";
    room.tilesetStem = _room.tilesetStem;
    room.biome = _room.biome;
    room.roomType = RoomType::Standard;
    room.hasNorth = (mask & 1) != 0;
    room.hasSouth = (mask & 2) != 0;
    room.hasEast = (mask & 4) != 0;
    room.hasWest = (mask & 8) != 0;
    const bool enabled[4] = {
        room.hasNorth, room.hasSouth, room.hasWest, room.hasEast
    };
    for (int i = 0; i < 4; ++i)
        room.doorZones[i] = { enabled[i], PredeterminedDoorZone((RoomWallSide)i) };

    _room = std::move(room);
    _undo.clear();
    _redo.clear();
    _savedOnce = false;
    _showLibrary = false;
    _status = "New " + DoorMaskName(mask) + " room - build and save when ready";
    _statusTimer = 3.f;
    return true;
}

void RoomEditor::SetDoors(bool north, bool south, bool east, bool west)
{
    if (_room.hasNorth == north && _room.hasSouth == south &&
        _room.hasEast == east && _room.hasWest == west)
        return;
    PushUndo();
    _room.hasNorth = north;
    _room.hasSouth = south;
    _room.hasEast = east;
    _room.hasWest = west;
    // Toggling an exit only flips the connection and its Door Zone on/off. Door
    // zones are predetermined (fixed lanes matching the dungeon) — we re-assert
    // their position here so they can never drift. The designer's Ground, Visual,
    // Collision, Prop, Decor and Fall layers are never touched.
    const bool enabled[4] = { north, south, west, east };
    for (int i = 0; i < 4; ++i)
        _room.doorZones[i] = { enabled[i], PredeterminedDoorZone((RoomWallSide)i) };
}

bool RoomEditor::SetWallDepth(RoomWallSide side, float depth)
{
    depth = std::clamp(depth, 0.25f, 4.0f);
    float* value = nullptr;
    switch (side)
    {
    case RoomWallSide::Top: value = &_room.wallTopDepth; break;
    case RoomWallSide::Bottom: value = &_room.wallBottomDepth; break;
    case RoomWallSide::Left: value = &_room.wallLeftDepth; break;
    case RoomWallSide::Right: value = &_room.wallRightDepth; break;
    }
    if (!value || std::abs(*value - depth) < 0.001f) return false;
    PushUndo();
    *value = depth;
    return true;
}

bool RoomEditor::Undo()
{
    if (_undo.empty()) return false;
    if (_redo.size() >= kHistoryLimit) _redo.erase(_redo.begin());
    _redo.push_back(_room);
    _room = _undo.back();
    _undo.pop_back();
    return true;
}

bool RoomEditor::Redo()
{
    if (_redo.empty()) return false;
    if (_undo.size() >= kHistoryLimit) _undo.erase(_undo.begin());
    _undo.push_back(_room);
    _room = _redo.back();
    _redo.pop_back();
    return true;
}

#ifndef ROOM_EDITOR_HEADLESS

namespace
{
    constexpr float kCanvasX = 30.f;
    constexpr float kCanvasY = 155.f;
    constexpr float kCell = 40.f;
    constexpr float kWallControlX = 1230.f;
    constexpr float kWallControlY = 70.f;
    constexpr float kWallControlStep = 165.f;

    Rectangle WallMinusRect(int index)
    {
        return { kWallControlX + index * kWallControlStep, kWallControlY, 30.f, 38.f };
    }

    Rectangle WallValueRect(int index)
    {
        return { kWallControlX + index * kWallControlStep + 34.f,
                 kWallControlY, 92.f, 38.f };
    }

    Rectangle WallPlusRect(int index)
    {
        return { kWallControlX + index * kWallControlStep + 130.f,
                 kWallControlY, 30.f, 38.f };
    }

    RoomWallSide WallSideAt(int index)
    {
        static constexpr RoomWallSide sides[] = {
            RoomWallSide::Top, RoomWallSide::Bottom,
            RoomWallSide::Left, RoomWallSide::Right
        };
        return sides[index];
    }

    float WallDepthAt(const RoomBlueprint& room, int index)
    {
        const float depths[] = {
            room.wallTopDepth, room.wallBottomDepth,
            room.wallLeftDepth, room.wallRightDepth
        };
        return depths[index];
    }

    Rectangle FitSourceInside(Rectangle source, Rectangle bounds)
    {
        if (source.width <= 0.f || source.height <= 0.f) return {};
        const float scale = std::min(bounds.width / source.width,
                                     bounds.height / source.height);
        const float width = source.width * scale;
        const float height = source.height * scale;
        return { bounds.x + (bounds.width - width) * 0.5f,
                 bounds.y + (bounds.height - height) * 0.5f,
                 width, height };
    }

    const char* RoomTypeLabel(RoomType type)
    {
        switch (type)
        {
        case RoomType::Standard: return "Standard";
        case RoomType::Elite: return "Elite";
        case RoomType::Treasure: return "Treasure";
        case RoomType::Boss: return "Boss";
        default: return "Standard";
        }
    }

    const char* TileLabel(int type)
    {
        static const char* labels[] = {
            "Floor", "Floor Variant", "Wall Body", "Wall Top", "Corner TL",
            "Corner TR", "Inner L", "Inner R", "Void", "Door Open",
            "Door Locked", "Boss Key", "Chest Closed", "Chest Open",
            "Wall Left", "Wall Right", "Wall Bottom", "Corner BL",
            "Corner BR", "Inner BL", "Inner BR"
        };
        return type >= 0 && type < (int)TileType::Count ? labels[type] : "Tile";
    }

    int AnimationFrame(int count, float fps, AnimPlaybackMode playback)
    {
        if (count <= 1) return 0;
        int tick = (int)(GetTime() * std::max(1.f, fps));
        if (playback == AnimPlaybackMode::PlayOnce) return std::min(tick, count - 1);
        if (playback == AnimPlaybackMode::PingPong)
        {
            int period = count * 2 - 2;
            int value = tick % period;
            return value < count ? value : period - value;
        }
        return tick % count;
    }

    void DrawButton(Rectangle rect, const char* label, bool active = false,
                    bool enabled = true)
    {
        Vector2 mouse = GetVirtualMousePos();
        bool hovered = enabled && CheckCollisionPointRec(mouse, rect);
        DrawRectangleRec(rect, !enabled ? Color{ 24, 26, 31, 255 }
                                      : active ? Color{ 38, 105, 166, 255 }
                                      : hovered ? Color{ 58, 62, 72, 255 }
                                                : Color{ 37, 40, 48, 255 });
        DrawRectangleLinesEx(rect, 1.f, active ? SKYBLUE : Fade(WHITE, enabled ? 0.25f : 0.10f));
        int fontSize = 17;
        int width = MeasureText(label, fontSize);
        DrawText(label, (int)(rect.x + rect.width * 0.5f - width * 0.5f),
                 (int)(rect.y + rect.height * 0.5f - fontSize * 0.5f), fontSize,
                 enabled ? RAYWHITE : Fade(WHITE, 0.28f));
    }

    struct CoverageChip
    {
        unsigned char mask;
        const char* label;
        int group;
        int slot;
    };

    constexpr CoverageChip kCoverageChips[] = {
        { 1, "N",    0, 0 }, { 2, "S",    0, 1 },
        { 4, "E",    0, 2 }, { 8, "W",    0, 3 },
        { 3, "NS",   1, 0 }, { 5, "NE",   1, 1 },
        { 9, "NW",   1, 2 }, { 6, "SE",   1, 3 },
        {10, "SW",   1, 4 }, {12, "EW",   1, 5 },
        { 7, "NSE",  2, 0 }, {11, "NSW",  2, 1 },
        {13, "NEW",  2, 2 }, {14, "SEW",  2, 3 },
        {15, "NSEW", 3, 0 },
    };

    Rectangle CoverageToggleRect() { return { 1210.f, 82.f, 270.f, 38.f }; }
    Rectangle CoveragePanelRect()  { return { 1280.f, 145.f, 600.f, 820.f }; }

    Rectangle CoverageChipRect(const CoverageChip& chip)
    {
        switch (chip.group)
        {
        case 0: return { 1305.f + chip.slot * 137.f, 285.f, 125.f, 42.f };
        case 1: return { 1305.f + (chip.slot % 3) * 183.f,
                         405.f + (chip.slot / 3) * 52.f, 170.f, 42.f };
        case 2: return { 1305.f + chip.slot * 137.f, 580.f, 125.f, 42.f };
        default:return { 1305.f, 700.f, 200.f, 42.f };
        }
    }

    int CompletedCoverageCount(const std::array<int, 16>& counts)
    {
        int complete = 0;
        for (int mask = 1; mask < 16; ++mask)
            if (counts[(std::size_t)mask] > 0) ++complete;
        return complete;
    }
}

void RoomEditor::Bind(const std::string& tilesetStem, Biome biome,
                      const TileDefSet& definitions, Texture2D sheet, Texture2D groundSheet,
                      const std::filesystem::path& roomRoot,
                      const std::filesystem::path& tilesetRoot)
{
    ClearGroundBrush();
    _groundBrushPresetPaths.clear();
    _groundBrushPresetName = "Ground Mix";
    _groundBrushPresetIndex = -1;
    _editingGroundBrushName = false;
    BindForTesting(tilesetStem, biome, definitions, roomRoot);
    _sheet = sheet;
    _groundSheet = groundSheet;
    _roomRoot = roomRoot;
    ReleaseCatalogTextures();
    _catalog.Refresh(tilesetRoot);
    _selectedSource = 0;
    for (int i = 0; i < (int)_catalog.Sources().size(); ++i)
    {
        const RoomAssetSource& source = _catalog.Sources()[(std::size_t)i];
        Texture2D texture = LoadTexture(source.imagePath.string().c_str());
        if (texture.id != 0) _sourceTextures.push_back({ source.stem, texture });
        if (source.stem == tilesetStem) _selectedSource = i;
    }
    _wantsBack = false;
    _playtestRequested = false;
    _paletteVisible = true;
    _showLibrary = false;
    RefreshGroundBrushPresets();
    _status = "Paint terrain, then place props and decor";
    _statusTimer = 4.f;
}

void RoomEditor::ReleaseCatalogTextures()
{
    for (SourceTexture& source : _sourceTextures)
        if (source.texture.id != 0) UnloadTexture(source.texture);
    _sourceTextures.clear();
}

void RoomEditor::Unload()
{
    ReleaseCatalogTextures();
}

Texture2D RoomEditor::TextureFor(const std::string& stem) const
{
    if (stem.empty() || stem == _room.tilesetStem) return _sheet;
    for (const SourceTexture& source : _sourceTextures)
        if (source.stem == stem) return source.texture;
    return {};
}

void RoomEditor::NewRoom()
{
    const std::string stem = _room.tilesetStem;
    const Biome biome = _room.biome;
    BindForTesting(stem, biome, _definitions, _roomRoot);
    _savedOnce = false;
    _showLibrary = false;
    _status = "New room";
    _statusTimer = 2.f;
}

void RoomEditor::OpenRoom(const RoomBlueprint& room)
{
    _room = room;
    _undo.clear();
    _redo.clear();
    _savedOnce = true;
    _showLibrary = false;
    _status = "Editing " + room.name;
    _statusTimer = 2.f;
}

void RoomEditor::SaveRoom()
{
    std::string error;
    if (_library.SaveRoom(_room, _savedOnce, error))
    {
        _savedOnce = true;
        _status = "Saved to the " + _room.tilesetStem + " room library";
        _statusTimer = 3.f;
    }
    else
    {
        _status = error;
        _statusTimer = 4.f;
    }
}

bool RoomEditor::ConsumePlaytestRequest()
{
    bool requested = _playtestRequested;
    _playtestRequested = false;
    return requested;
}

Rectangle RoomEditor::CanvasRect() const
{
    return { kCanvasX, kCanvasY, RoomLayout::kCols * kCell, RoomLayout::kRows * kCell };
}

bool RoomEditor::ScreenToCell(Vector2 mouse, int& col, int& row) const
{
    Rectangle canvas = CanvasRect();
    if (!CheckCollisionPointRec(mouse, canvas)) return false;
    col = (int)((mouse.x - canvas.x) / kCell);
    row = (int)((mouse.y - canvas.y) / kCell);
    return IsCellValid(col, row);
}

Rectangle RoomEditor::AssetSource(RoomAssetKind kind, int index) const
{
    const TileDefSet& definitions = SelectedDefinitions();
    if (index < 0) return {};
    switch (kind)
    {
    case RoomAssetKind::Prop:
        if (index < (int)definitions.props.size()) return definitions.props[(std::size_t)index].src;
        break;
    case RoomAssetKind::AnimProp:
        if (index < (int)definitions.animProps.size())
        {
            const auto& def = definitions.animProps[(std::size_t)index];
            if (!def.frames.empty())
                return def.frames[(std::size_t)AnimationFrame((int)def.frames.size(), def.fps, def.playback)];
        }
        break;
    case RoomAssetKind::Decor:
        if (index < (int)definitions.decors.size()) return definitions.decors[(std::size_t)index].src;
        break;
    case RoomAssetKind::AnimDecor:
        if (index < (int)definitions.animDecors.size())
        {
            const auto& def = definitions.animDecors[(std::size_t)index];
            if (!def.frames.empty())
                return def.frames[(std::size_t)AnimationFrame((int)def.frames.size(), def.fps, def.playback)];
        }
        break;
    }
    return {};
}

const char* RoomEditor::AssetName(RoomAssetKind kind, int index) const
{
    const TileDefSet& definitions = SelectedDefinitions();
    switch (kind)
    {
    case RoomAssetKind::Prop: return definitions.props[(std::size_t)index].name.c_str();
    case RoomAssetKind::AnimProp: return definitions.animProps[(std::size_t)index].name.c_str();
    case RoomAssetKind::Decor: return definitions.decors[(std::size_t)index].name.c_str();
    case RoomAssetKind::AnimDecor: return definitions.animDecors[(std::size_t)index].name.c_str();
    }
    return "Asset";
}

int RoomEditor::AssetCountForLayer() const
{
    const TileDefSet& definitions = SelectedDefinitions();
    if (!ShowingAssetPalette()) return 0;
    if (PaletteShowsProps())
        return (int)definitions.props.size() + (int)definitions.animProps.size();
    return (int)definitions.decors.size() + (int)definitions.animDecors.size();
}

RoomAssetKind RoomEditor::AssetKindAtPaletteIndex(int index, int& definitionIndex) const
{
    const TileDefSet& definitions = SelectedDefinitions();
    if (PaletteShowsProps())
    {
        if (index < (int)definitions.props.size())
        {
            definitionIndex = index;
            return RoomAssetKind::Prop;
        }
        definitionIndex = index - (int)definitions.props.size();
        return RoomAssetKind::AnimProp;
    }
    if (index < (int)definitions.decors.size())
    {
        definitionIndex = index;
        return RoomAssetKind::Decor;
    }
    definitionIndex = index - (int)definitions.decors.size();
    return RoomAssetKind::AnimDecor;
}

std::vector<int> RoomEditor::MatchingAssetIndices() const
{
    auto lower = [](std::string s)
    {
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    const std::string query = lower(_paletteSearch);
    std::vector<int> matches;
    for (int i = 0; i < AssetCountForLayer(); ++i)
    {
        if (query.empty()) { matches.push_back(i); continue; }
        int definitionIndex = 0;
        const RoomAssetKind kind = AssetKindAtPaletteIndex(i, definitionIndex);
        if (lower(AssetName(kind, definitionIndex)).find(query) != std::string::npos)
            matches.push_back(i);
    }
    return matches;
}

void RoomEditor::UpdateLibrary()
{
    Vector2 mouse = GetVirtualMousePos();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        CheckCollisionPointRec(mouse, {1500,82,120,38}))
    {
        _pendingDeleteId.clear();
        _showLibrary = false;
        return;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        CheckCollisionPointRec(mouse, CoverageToggleRect()))
    {
        _coverageExpanded = !_coverageExpanded;
        _pendingDeleteId.clear();
        return;
    }

    if (_coverageExpanded && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        const auto counts = _library.DoorMaskCounts(_room.biome, _room.tilesetStem);
        for (const CoverageChip& chip : kCoverageChips)
        {
            if (counts[chip.mask] != 0 ||
                !CheckCollisionPointRec(mouse, CoverageChipRect(chip)))
                continue;
            CreateRoomForDoorMask(chip.mask);
            return;
        }
    }

    const auto rooms = _library.RoomsFor(_room.biome, _room.tilesetStem);
    const float libMax = std::max(0.f, (float)rooms.size() * 62.f - 760.f);
    _libraryScroll = std::clamp(_libraryScroll - GetMouseWheelMove() * 65.f, 0.f, libMax);
    for (int i = 0; i < (int)rooms.size(); ++i)
    {
        float y = 180.f + i * 62.f - _libraryScroll;
        Rectangle row{ 240.f, y, _coverageExpanded ? 1015.f : 1360.f, 52.f };
        const bool armed = !_pendingDeleteId.empty() &&
                           _pendingDeleteId == rooms[(std::size_t)i]->id;
        const float removeWidth = armed ? 90.f : 50.f;
        Rectangle remove{ row.x + row.width - removeWidth - 10.f,
                          y + 7.f, removeWidth, 38.f };
        Rectangle copy{ remove.x - 70.f, y + 7.f, 60.f, 38.f };
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, copy))
        {
            RoomBlueprint dup = Duplicate(*rooms[(std::size_t)i]);
            std::string error;
            const bool ok = _library.SaveRoom(dup, false, error);
            _library.Refresh(_roomRoot);
            _pendingDeleteId.clear();
            _status = ok ? "Duplicated room" : error;
            _statusTimer = 3.f;
            return;
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, remove))
        {
            // First click arms this room, second click on the same one confirms —
            // guards against accidentally deleting authored rooms.
            const std::string& id = rooms[(std::size_t)i]->id;
            if (_pendingDeleteId != id)
            {
                _pendingDeleteId = id;
                _status = "Click X again to delete \"" + rooms[(std::size_t)i]->name + "\"";
                _statusTimer = 3.f;
                return;
            }
            std::string error;
            _library.DeleteRoom(id, error);
            _pendingDeleteId.clear();
            _status = error.empty() ? "Room deleted" : error;
            _statusTimer = 3.f;
            return;
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, row))
        {
            _pendingDeleteId.clear();
            OpenRoom(*rooms[(std::size_t)i]);
            return;
        }
    }
    if (IsKeyPressed(KEY_ESCAPE)) { _pendingDeleteId.clear(); _showLibrary = false; }
}

// Collision layer, Rectangle tool: draw a NEW free collider by dragging, grab an
// existing one to move (interior) or resize (right/bottom edge), right-click to
// delete. Smooth by default; hold Shift to snap to quarter-tile. One undo entry
// per gesture. Rects are tile-space and can be far smaller than a full tile.
void RoomEditor::UpdateLayerRects(Vector2 mouse, Rectangle canvas)
{
    const float tx = std::clamp((mouse.x - canvas.x) / kCell, 0.f, (float)RoomLayout::kCols);
    const float ty = std::clamp((mouse.y - canvas.y) / kCell, 0.f, (float)RoomLayout::kRows);
    const float edge = 0.30f;   // tiles from a border that count as a resize grab
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    auto snap = [&](float v) { return shift ? std::round(v / 0.25f) * 0.25f : v; };
    auto& cols = _layer == Layer::FallZones ? _room.fallRects : _room.colliders;

    // Right-click removes the top-most collider under the cursor.
    if (_colliderDragMode == 0 && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
    {
        for (int i = (int)cols.size() - 1; i >= 0; --i)
            if (CheckCollisionPointRec({ tx, ty }, cols[(std::size_t)i]))
            {
                PushUndo();
                cols.erase(cols.begin() + i);
                _selectedCollider = -1;
                return;
            }
        return;
    }

    if (_colliderDragMode == 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        // Grab an existing collider first (top-most): edge = resize, interior = move.
        for (int i = (int)cols.size() - 1; i >= 0; --i)
        {
            const Rectangle& c = cols[(std::size_t)i];
            const Rectangle grab{ c.x - edge, c.y - edge, c.width + edge * 2.f, c.height + edge * 2.f };
            if (!CheckCollisionPointRec({ tx, ty }, grab)) continue;
            _selectedCollider = i;
            const bool nearRight  = tx > c.x + c.width - edge;
            const bool nearBottom = ty > c.y + c.height - edge;
            _colliderDragMode = (nearRight || nearBottom) ? 2 : 1;
            _colliderGrab = { tx - c.x, ty - c.y };
            PushUndo();
            _suppressUndo = true;
            return;
        }
        // Nothing grabbed → start drawing a new collider from here.
        _colliderDragMode = 3;
        _colliderAnchor = { tx, ty };
        _selectedCollider = -1;
        PushUndo();
        _suppressUndo = true;
        return;
    }

    if (_colliderDragMode == 1 || _colliderDragMode == 2)
    {
        if (_selectedCollider >= 0 && _selectedCollider < (int)cols.size() &&
            IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        {
            Rectangle& c = cols[(std::size_t)_selectedCollider];
            if (_colliderDragMode == 1)   // move
            {
                c.x = std::clamp(snap(tx - _colliderGrab.x), 0.f, (float)RoomLayout::kCols - c.width);
                c.y = std::clamp(snap(ty - _colliderGrab.y), 0.f, (float)RoomLayout::kRows - c.height);
            }
            else                          // resize (right/bottom edges)
            {
                c.width  = std::clamp(snap(tx - c.x), 0.1f, (float)RoomLayout::kCols - c.x);
                c.height = std::clamp(snap(ty - c.y), 0.1f, (float)RoomLayout::kRows - c.y);
            }
        }
    }

    if (_colliderDragMode != 0 && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
    {
        bool changed = true;
        if (_colliderDragMode == 3)   // finish drawing a new rect
        {
            const float x0 = snap(std::min(_colliderAnchor.x, tx));
            const float y0 = snap(std::min(_colliderAnchor.y, ty));
            const float x1 = snap(std::max(_colliderAnchor.x, tx));
            const float y1 = snap(std::max(_colliderAnchor.y, ty));
            if (x1 - x0 >= 0.1f && y1 - y0 >= 0.1f)
            {
                cols.push_back({ x0, y0, x1 - x0, y1 - y0 });
                _selectedCollider = (int)cols.size() - 1;
            }
            else changed = false;     // too small — treat as a mis-click
        }
        _suppressUndo = false;
        if (!changed && !_undo.empty()) _undo.pop_back();
        _colliderDragMode = 0;
    }
}

void RoomEditor::UpdateCanvas()
{
    const Vector2 mouse = GetVirtualMousePos();
    const Rectangle canvas = CanvasRect();
    const bool overCanvas = CheckCollisionPointRec(mouse, canvas);

    // ── Door Zones: predetermined & fixed — nothing to drag on the canvas.
    // Doors are toggled on/off (toolbar N/S/W/E or the palette buttons); their
    // position never changes, so the canvas is view-only on this layer.
    if (_layer == Layer::DoorZones)
        return;

    // ── Collision layer + Rectangle tool: draw/move/resize free collider boxes.
    // Brush/Bucket/Eraser still paint the full-tile grid via the generic path.
    if ((_layer == Layer::Collision || _layer == Layer::FallZones) &&
        _paintTool == PaintTool::Rectangle)
    {
        UpdateLayerRects(mouse, canvas);
        return;
    }

    int col = 0, row = 0;
    const bool onCell = ScreenToCell(mouse, col, row);

    // Treasure rooms have one authored reward position. It behaves like a
    // marker tool rather than a paint stroke: click to place/move, right-click
    // to clear, with the same undo history as every other room edit.
    if (_layer == Layer::ChestSpawn)
    {
        if (onCell && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            SetTreasureChestSpawn(col, row);
        else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
            ClearTreasureChestSpawn();
        return;
    }

    // ── Eyedropper (Alt+left-click) samples the cell into the selection ──────
    const bool altHeld = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    if (altHeld && onCell && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        PickAt(col, row);
        return;
    }

    // The visible eraser is deliberately single-cell and left-click driven.
    // Right-click retains the selected brush/rectangle/bucket erase behaviour.
    if (_paintTool == PaintTool::Eraser && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        // On the Collision layer the eraser also removes a free collider box.
        if (_layer == Layer::Collision || _layer == Layer::FallZones)
        {
            const float tx = (mouse.x - canvas.x) / kCell;
            const float ty = (mouse.y - canvas.y) / kCell;
            auto& rects = _layer == Layer::FallZones ? _room.fallRects : _room.colliders;
            for (int i = (int)rects.size() - 1; i >= 0; --i)
                if (CheckCollisionPointRec({ tx, ty }, rects[(std::size_t)i]))
                {
                    PushUndo();
                    rects.erase(rects.begin() + i);
                    if (_selectedCollider == i) _selectedCollider = -1;
                    else if (_selectedCollider > i) --_selectedCollider;
                    return;
                }
        }
        if (onCell) { EraseAt(col, row); return; }
    }

    // ── Props / Decor: single click place / remove (not a stroke) ────────────
    if (_layer == Layer::Props || _layer == Layer::Decor)
    {
        if (onCell && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !_selectedAssetId.empty())
        {
            const RoomAssetSource* source = SelectedSource();
            PlaceAsset({ _selectedAssetKind, _selectedAssetId, col, row,
                         source != nullptr ? source->stem : _room.tilesetStem });
        }
        else if (onCell && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
            EraseAt(col, row);
        return;
    }

    // ── Brush / Rectangle / Bucket painting on Ground/Visual/Collision/Fall ──
    const bool leftPressed  = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const bool rightPressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    if (!_stroking && onCell && (leftPressed || rightPressed))
    {
        // One undo entry covers the whole stroke/fill.
        PushUndo();
        _suppressUndo = true;
        _stroking = true;
        _strokeAdd = leftPressed;
        _strokeChanged = false;
        _dragStartCol = col;
        _dragStartRow = row;
        // Shift forces a rectangle regardless of the selected tool.
        _strokeTool = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
            ? PaintTool::Rectangle : _paintTool;
        if (_strokeTool == PaintTool::Bucket)
            _strokeChanged = FloodFillFrom(col, row, _strokeAdd);
        else if (_strokeTool == PaintTool::Brush)
            _strokeChanged = PaintCell(col, row, _strokeAdd);
        // Rectangle waits for release to fill.
    }
    else if (_stroking && _strokeTool == PaintTool::Brush && onCell &&
             IsMouseButtonDown(_strokeAdd ? MOUSE_BUTTON_LEFT : MOUSE_BUTTON_RIGHT))
    {
        _strokeChanged = PaintCell(col, row, _strokeAdd) || _strokeChanged;
    }

    const bool released = _strokeAdd ? IsMouseButtonReleased(MOUSE_BUTTON_LEFT)
                                     : IsMouseButtonReleased(MOUSE_BUTTON_RIGHT);
    if (_stroking && (released || (!onCell && !overCanvas &&
                                   !IsMouseButtonDown(_strokeAdd ? MOUSE_BUTTON_LEFT
                                                                 : MOUSE_BUTTON_RIGHT))))
    {
        if (_strokeTool == PaintTool::Rectangle && onCell)
            _strokeChanged = FillRect(_dragStartCol, _dragStartRow, col, row, _strokeAdd)
                             || _strokeChanged;
        _suppressUndo = false;
        _stroking = false;
        if (!_strokeChanged && !_undo.empty()) _undo.pop_back();
    }
}

void RoomEditor::Update()
{
    if (_statusTimer > 0.f) _statusTimer -= GetFrameTime();
    const bool ctrl  = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT)   || IsKeyDown(KEY_RIGHT_SHIFT);
    const bool typing = _editingName || _editingSearch || _editingGroundBrushName;
    if (!typing && IsKeyPressed(KEY_TAB)) _paletteVisible = !_paletteVisible;
    if (ctrl && IsKeyPressed(KEY_Z)) { if (shift) Redo(); else Undo(); }
    if (ctrl && IsKeyPressed(KEY_Y)) Redo();
    if (ctrl && IsKeyPressed(KEY_S)) SaveRoom();
    if (!typing && !ctrl && IsKeyPressed(KEY_S)) SaveRoom();
    if (!typing && !ctrl)
    {
        // 1-9 select a layer (matches the toolbar order); B/E select tools.
        if (IsKeyPressed(KEY_ONE))   _layer = Layer::Ground;
        if (IsKeyPressed(KEY_TWO))   _layer = Layer::Visual;
        if (IsKeyPressed(KEY_THREE)) _layer = Layer::Door;
        if (IsKeyPressed(KEY_FOUR))  _layer = Layer::Collision;
        if (IsKeyPressed(KEY_FIVE))  _layer = Layer::Props;
        if (IsKeyPressed(KEY_SIX))   _layer = Layer::Decor;
        if (IsKeyPressed(KEY_SEVEN)) _layer = Layer::FallZones;
        if (IsKeyPressed(KEY_EIGHT)) _layer = Layer::DoorZones;
        if (IsKeyPressed(KEY_NINE) && _room.roomType == RoomType::Treasure)
            _layer = Layer::ChestSpawn;
        if (IsKeyPressed(KEY_B))
            _paintTool = _paintTool == PaintTool::Bucket ? PaintTool::Brush : PaintTool::Bucket;
        if (IsKeyPressed(KEY_E))
            _paintTool = _paintTool == PaintTool::Eraser ? PaintTool::Brush : PaintTool::Eraser;
    }

    if (_showLibrary) { UpdateLibrary(); return; }

    Vector2 mouse = GetVirtualMousePos();
    struct ToolbarAction { Rectangle rect; int action; };
    const ToolbarAction actions[] = {
        {{20, 18, 130, 38}, 0}, {{160, 18, 110, 38}, 1}, {{280, 18, 130, 38}, 2},
        {{420, 18, 105, 38}, 3}, {{535, 18, 125, 38}, 4},
        {{670, 18, 90, 38}, 5}, {{770, 18, 90, 38}, 6}
    };
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        for (const auto& action : actions)
        {
            if (!CheckCollisionPointRec(mouse, action.rect)) continue;
            if (action.action == 0) _wantsBack = true;
            if (action.action == 1) NewRoom();
            if (action.action == 2) { _library.Refresh(_roomRoot); _showLibrary = true; }
            if (action.action == 3) SaveRoom();
            if (action.action == 4) { SaveRoom(); if (_savedOnce) _playtestRequested = true; }
            if (action.action == 5 && CanUndo()) Undo();
            if (action.action == 6 && CanRedo()) Redo();
            return;
        }
    }

    Rectangle nameBox{ 130.f, 70.f, 360.f, 38.f };
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, nameBox))
    {
        _editingName = true;
        _editingSearch = false;
    }
    else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mouse, nameBox))
        _editingName = false;
    if (_editingName)
    {
        int key = GetCharPressed();
        while (key > 0)
        {
            if (key >= 32 && key <= 126 && _room.name.size() < 48) _room.name.push_back((char)key);
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) && !_room.name.empty()) _room.name.pop_back();
        if (IsKeyPressed(KEY_ENTER)) _editingName = false;
    }
    if (_editingSearch)
    {
        int key = GetCharPressed();
        while (key > 0)
        {
            if (key >= 32 && key <= 126 && _paletteSearch.size() < 32)
                _paletteSearch.push_back((char)key);
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) && !_paletteSearch.empty()) _paletteSearch.pop_back();
        if (IsKeyPressed(KEY_ENTER)) _editingSearch = false;
    }
    if (_editingGroundBrushName)
    {
        int key = GetCharPressed();
        while (key > 0)
        {
            if (key >= 32 && key <= 126 && _groundBrushPresetName.size() < 32)
                _groundBrushPresetName.push_back((char)key);
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) && !_groundBrushPresetName.empty())
            _groundBrushPresetName.pop_back();
        if (IsKeyPressed(KEY_ENTER)) _editingGroundBrushName = false;
    }

    const Rectangle doorButtons[] = { {760,70,52,38}, {818,70,52,38}, {876,70,52,38}, {934,70,52,38} };
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        for (int i = 0; i < 4; ++i)
        {
            if (!CheckCollisionPointRec(mouse, doorButtons[i])) continue;
            bool n = _room.hasNorth, s = _room.hasSouth, w = _room.hasWest, e = _room.hasEast;
            if (i == 0) n = !n; if (i == 1) s = !s; if (i == 2) w = !w; if (i == 3) e = !e;
            SetDoors(n, s, e, w);
        }
        Rectangle typeButton{ 1030,70,180,38 };
        if (CheckCollisionPointRec(mouse, typeButton))
        {
            PushUndo();
            if (_room.roomType == RoomType::Standard) _room.roomType = RoomType::Elite;
            else if (_room.roomType == RoomType::Elite) _room.roomType = RoomType::Treasure;
            else if (_room.roomType == RoomType::Treasure) _room.roomType = RoomType::Boss;
            else _room.roomType = RoomType::Standard;
            if (_room.roomType != RoomType::Treasure)
            {
                _room.treasureChestCol = -1;
                _room.treasureChestRow = -1;
                if (_layer == Layer::ChestSpawn) _layer = Layer::Ground;
            }
        }
        for (int i = 0; i < 4; ++i)
        {
            const float current = WallDepthAt(_room, i);
            if (CheckCollisionPointRec(mouse, WallMinusRect(i)))
                SetWallDepth(WallSideAt(i), current - 0.25f);
            else if (CheckCollisionPointRec(mouse, WallPlusRect(i)))
                SetWallDepth(WallSideAt(i), current + 0.25f);
        }
    }

    // Mouse-wheel over a wall-depth field nudges it ±0.25 — no more +/- grind.
    {
        const float wheel = GetMouseWheelMove();
        if (wheel != 0.f)
            for (int i = 0; i < 4; ++i)
                if (CheckCollisionPointRec(mouse, WallValueRect(i)))
                    SetWallDepth(WallSideAt(i),
                                 WallDepthAt(_room, i) + (wheel > 0.f ? 0.25f : -0.25f));
    }

    const Rectangle layerButtons[] = {
        {30,112,75,34}, {109,112,75,34}, {188,112,65,34},
        {257,112,85,34}, {346,112,65,34}, {415,112,65,34},
        {484,112,60,34}, {548,112,90,34}, {642,112,90,34}
    };
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        for (int i = 0; i < 9; ++i)
            if (CheckCollisionPointRec(mouse, layerButtons[i]) &&
                (i != (int)Layer::ChestSpawn || _room.roomType == RoomType::Treasure))
                _layer = (Layer)i;

    // Paint-tool selector + Clear Layer (right of the layer row).
    struct ToolButton { Rectangle rect; PaintTool tool; };
    const ToolButton toolButtons[] = {
        {{748,112,66,34}, PaintTool::Brush},
        {{818,112,66,34}, PaintTool::Rectangle},
        {{888,112,66,34}, PaintTool::Bucket},
        {{958,112,70,34}, PaintTool::Eraser},
    };
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        for (const auto& t : toolButtons)
            if (CheckCollisionPointRec(mouse, t.rect)) _paintTool = t.tool;
        if (CheckCollisionPointRec(mouse, {1032,112,80,34}) && ClearActiveLayer())
        { _status = "Layer cleared"; _statusTimer = 2.f; }
    }

    if (_paletteVisible && mouse.x >= 1180.f)
    {
        const bool groundTilePalette = _layer == Layer::Ground && !DecorModeOnTileLayer();
        const bool rawTilePalette = (_layer == Layer::Ground ||
            _layer == Layer::Visual || _layer == Layer::Door) &&
            !DecorModeOnTileLayer();
        const float tilePaletteTop = rawTilePalette
            ? (groundTilePalette ? 350.f : 310.f)
            : 283.f;
        const bool groundBrushPanelVisible = groundTilePalette && !_groundBrushTiles.empty();
        const float tilePaletteBottom = groundBrushPanelVisible
            ? 744.f : 988.f;
        float maxScroll = 0.f;
        if (ShowingAssetPalette())
        {
            const int rows = ((int)MatchingAssetIndices().size() + 4) / 5;
            maxScroll = std::max(0.f, rows * 108.f - 700.f);
        }
        else if (_layer == Layer::Ground || _layer == Layer::Visual || _layer == Layer::Door)
        {
            if (const RoomAssetSource* s = SelectedSource(); s && s->tileColumns > 0)
            {
                const float cellPx = 660.f / (float)s->tileColumns;
                maxScroll = std::max(0.f, s->tileRows * cellPx -
                    std::max(1.f, tilePaletteBottom - tilePaletteTop));
            }
        }
        _paletteScroll = std::clamp(_paletteScroll - GetMouseWheelMove() * 90.f, 0.f, maxScroll);
        // Ground/Visual layers: a "Tiles | Decor" toggle switches the palette to
        // banded Decor/AnimDecor assets (animated water/lava as ground/visual).
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
            (_layer == Layer::Ground || _layer == Layer::Visual) &&
            CheckCollisionPointRec(mouse, {1700.f, 170.f, 180.f, 30.f}))
        {
            _tileLayerDecorMode = !_tileLayerDecorMode;
            _selectedAssetId.clear();
            _paletteScroll = 0.f;
            _paletteSearch.clear();
            _editingSearch = false;
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            bool handledGroundControl = false;
            bool handledTileOverhang = false;
            if (groundTilePalette)
            {
                if (CheckCollisionPointRec(mouse, GroundRandomToggleRect()))
                {
                    handledGroundControl = true;
                    if (_groundBrushTiles.size() < 2)
                    {
                        _status = "Add at least two ground tiles to make a random brush";
                        _statusTimer = 3.f;
                    }
                    else
                    {
                        _randomGroundBrushEnabled = !_randomGroundBrushEnabled;
                        _paletteScroll = 0.f;
                        _status = _randomGroundBrushEnabled ? "Random ground brush on"
                                                           : "Random ground brush off";
                        _statusTimer = 2.f;
                    }
                }
                else if (CheckCollisionPointRec(mouse, GroundRandomAddRect()))
                {
                    handledGroundControl = true;
                    if (AddSelectedGroundBrushTile())
                    {
                        _status = _groundBrushTiles.size() == 1
                            ? "Base ground added at weight 8" : "Variant added at weight 1";
                        _statusTimer = 2.5f;
                    }
                    else
                    {
                        _status = "That tile is already in the mix, or the mix is full";
                        _statusTimer = 3.f;
                    }
                }
                else if (CheckCollisionPointRec(mouse, GroundRandomClearRect()))
                {
                    handledGroundControl = true;
                    ClearGroundBrush();
                    _groundBrushPresetName = "Ground Mix";
                    _groundBrushPresetIndex = -1;
                    _status = "Ground mix cleared";
                    _statusTimer = 2.f;
                }

                if (groundBrushPanelVisible &&
                    CheckCollisionPointRec(mouse, GroundBrushNameRect()))
                {
                    handledGroundControl = true;
                    _editingGroundBrushName = true;
                    _editingName = false;
                    _editingSearch = false;
                }
                else if (groundBrushPanelVisible &&
                         CheckCollisionPointRec(mouse, GroundBrushSaveRect()))
                {
                    handledGroundControl = true;
                    std::string error;
                    if (SaveGroundBrushPreset(_groundBrushPresetName, error))
                    {
                        _status = "Saved ground brush: " + _groundBrushPresetName;
                        _statusTimer = 2.5f;
                    }
                    else { _status = error; _statusTimer = 4.f; }
                }
                else if (groundBrushPanelVisible &&
                         (CheckCollisionPointRec(mouse, GroundBrushPrevRect()) ||
                          CheckCollisionPointRec(mouse, GroundBrushNextRect())))
                {
                    handledGroundControl = true;
                    RefreshGroundBrushPresets();
                    if (!_groundBrushPresetPaths.empty())
                    {
                        const int count = (int)_groundBrushPresetPaths.size();
                        const int delta = CheckCollisionPointRec(mouse, GroundBrushPrevRect()) ? -1 : 1;
                        _groundBrushPresetIndex = (_groundBrushPresetIndex + delta + count) % count;
                        std::string error;
                        if (LoadGroundBrushPresetPath(
                                _groundBrushPresetPaths[(std::size_t)_groundBrushPresetIndex], error))
                        {
                            _status = "Loaded ground brush: " + _groundBrushPresetName;
                            _statusTimer = 2.5f;
                        }
                        else { _status = error; _statusTimer = 4.f; }
                    }
                    else { _status = "No saved ground brushes yet"; _statusTimer = 2.5f; }
                }

                if (groundBrushPanelVisible)
                {
                    for (int i = 0; i < (int)_groundBrushTiles.size(); ++i)
                    {
                        if (CheckCollisionPointRec(mouse, GroundBrushMinusRect(i)))
                        {
                            handledGroundControl = true;
                            SetGroundBrushWeight((std::size_t)i,
                                std::max(1, _groundBrushTiles[(std::size_t)i].weight - 1));
                        }
                        else if (CheckCollisionPointRec(mouse, GroundBrushPlusRect(i)))
                        {
                            handledGroundControl = true;
                            SetGroundBrushWeight((std::size_t)i,
                                std::min(20, _groundBrushTiles[(std::size_t)i].weight + 1));
                        }
                        else if (CheckCollisionPointRec(mouse, GroundBrushRemoveRect(i)))
                        {
                            handledGroundControl = true;
                            RemoveGroundBrushTile((std::size_t)i);
                            break;
                        }
                    }
                }
                if (!CheckCollisionPointRec(mouse, GroundBrushNameRect()))
                    _editingGroundBrushName = false;
            }
            if (rawTilePalette)
            {
                static const RoomWallSide sides[8] = {
                    RoomWallSide::Top, RoomWallSide::Top,
                    RoomWallSide::Bottom, RoomWallSide::Bottom,
                    RoomWallSide::Left, RoomWallSide::Left,
                    RoomWallSide::Right, RoomWallSide::Right
                };
                for (int i = 0; i < 8; ++i)
                    if (CheckCollisionPointRec(mouse,
                                               TileOverhangButtonRect(i, groundTilePalette)))
                    {
                        handledTileOverhang = true;
                        const int delta = (i % 2 == 0) ? -1 : 1;
                        if (AdjustSelectedTileOverhang(sides[i], delta))
                        {
                            _status = "Tile overhang adjusted by 1 source pixel";
                            _statusTimer = 2.f;
                        }
                        break;
                    }
            }
            // Focus the props/decor search box; clicking elsewhere defocuses it.
            const bool assetPalette = ShowingAssetPalette();
            _editingSearch = assetPalette &&
                CheckCollisionPointRec(mouse, {1210.f, 255.f, 660.f, 26.f});
            if (_editingSearch) _editingName = false;
            if (_layer == Layer::FallZones)
            {
                const FallSurface options[] = { FallSurface::Water, FallSurface::Lava };
                for (int i = 0; i < 2; ++i)
                {
                    if (!CheckCollisionPointRec(mouse, FallSurfaceButtonRect(i))) continue;
                    const FallSurface next = _room.fallSurface == options[i]
                        ? FallSurface::Void : options[i];
                    if (SetFallSurface(next))
                    {
                        _status = std::string("Fall surface: ") + FallSurfaceLabel(next);
                        _statusTimer = 2.f;
                    }
                }
            }
            const int sourceCount = (int)_catalog.Sources().size();
            if (!handledGroundControl && sourceCount > 0 && CheckCollisionPointRec(mouse, {1210,230,42,34}))
            {
                _selectedSource = (_selectedSource + sourceCount - 1) % sourceCount;
                _selectedAssetId.clear();
                _selectedRawTile = {0.f,0.f,16.f,16.f};
                _selectedTileAnchorOffset = {};
                _paletteScroll = 0.f;
            }
            else if (!handledGroundControl && sourceCount > 0 && CheckCollisionPointRec(mouse, {1828,230,42,34}))
            {
                _selectedSource = (_selectedSource + 1) % sourceCount;
                _selectedAssetId.clear();
                _selectedRawTile = {0.f,0.f,16.f,16.f};
                _selectedTileAnchorOffset = {};
                _paletteScroll = 0.f;
            }
            else if (!handledGroundControl && !handledTileOverhang &&
                     (_layer == Layer::Ground || _layer == Layer::Visual || _layer == Layer::Door) &&
                     !DecorModeOnTileLayer())
            {
                const RoomAssetSource* source = SelectedSource();
                if (source != nullptr && source->tileColumns > 0 && source->tileRows > 0)
                {
                    // Map the click onto the contiguous sheet (matches DrawPalette).
                    const int cols = source->tileColumns, rows = source->tileRows;
                    const float cellPx = 660.f / (float)cols;
                    const float ox = 1210.f, oy = tilePaletteTop + 2.f - _paletteScroll;
                    if (mouse.x >= ox && mouse.x < ox + cols * cellPx &&
                        mouse.y >= oy && mouse.y < oy + rows * cellPx)
                    {
                        const int gc = std::clamp((int)((mouse.x - ox) / cellPx), 0, cols - 1);
                        const int gr = std::clamp((int)((mouse.y - oy) / cellPx), 0, rows - 1);
                        _selectedRawTile = { (float)(gc * 16), (float)(gr * 16), 16.f, 16.f };
                        _selectedTileAnchorOffset = {};
                    }
                }
            }
            else if (assetPalette)
            {
                const TileDefSet& definitions = SelectedDefinitions();
                const std::vector<int> matches = MatchingAssetIndices();
                for (int slot = 0; slot < (int)matches.size(); ++slot)
                {
                    Rectangle item{ 1210.f + (slot % 5) * 128.f,
                                    285.f + (slot / 5) * 108.f - _paletteScroll, 116.f, 96.f };
                    if (!CheckCollisionPointRec(mouse, item)) continue;
                    int definitionIndex = 0;
                    _selectedAssetKind = AssetKindAtPaletteIndex(matches[slot], definitionIndex);
                    switch (_selectedAssetKind)
                    {
                    case RoomAssetKind::Prop: _selectedAssetId = definitions.props[(std::size_t)definitionIndex].id; break;
                    case RoomAssetKind::AnimProp: _selectedAssetId = definitions.animProps[(std::size_t)definitionIndex].id; break;
                    case RoomAssetKind::Decor: _selectedAssetId = definitions.decors[(std::size_t)definitionIndex].id; break;
                    case RoomAssetKind::AnimDecor: _selectedAssetId = definitions.animDecors[(std::size_t)definitionIndex].id; break;
                    }
                }
            }
            else if (_layer == Layer::DoorZones)
            {
                // Predetermined doors: the only control is on/off per side. Order
                // matches doorZones[] — Top(N), Bottom(S), Left(W), Right(E).
                for (int i = 0; i < 4; ++i)
                    if (CheckCollisionPointRec(mouse, {1210.f+i*120.f,300,110,40}))
                    {
                        bool n=_room.hasNorth,s=_room.hasSouth,w=_room.hasWest,e=_room.hasEast;
                        if      (i==0) n=!n;
                        else if (i==1) s=!s;
                        else if (i==2) w=!w;
                        else           e=!e;
                        SetDoors(n,s,e,w);
                    }
            }
        }
    }
    // The search box only exists on the asset palette — never leave it focused elsewhere.
    if (!ShowingAssetPalette()) _editingSearch = false;
    if (_layer != Layer::Ground || DecorModeOnTileLayer()) _editingGroundBrushName = false;
    UpdateCanvas();
}

void RoomEditor::DrawToolbar() const
{
    DrawButton({20,18,130,38}, "Back to Tiles");
    DrawButton({160,18,110,38}, "New");
    DrawButton({280,18,130,38}, "Room Library");
    DrawButton({420,18,105,38}, "Save (S)");
    DrawButton({535,18,125,38}, "Playtest");
    DrawButton({670,18,90,38}, "Undo", false, CanUndo());
    DrawButton({770,18,90,38}, "Redo", false, CanRedo());
    DrawText("Name", 30, 79, 18, Fade(WHITE, .7f));
    DrawRectangleRec({130,70,360,38}, _editingName ? Color{45,70,100,255} : Color{27,30,36,255});
    DrawRectangleLinesEx({130,70,360,38}, 1.f, _editingName ? SKYBLUE : Fade(WHITE,.25f));
    DrawText(_room.name.c_str(), 140, 78, 20, RAYWHITE);
    DrawText("Doors", 680, 79, 18, Fade(WHITE,.7f));
    DrawButton({760,70,52,38}, "N", _room.hasNorth);
    DrawButton({818,70,52,38}, "S", _room.hasSouth);
    DrawButton({876,70,52,38}, "W", _room.hasWest);
    DrawButton({934,70,52,38}, "E", _room.hasEast);
    DrawButton({1030,70,180,38}, RoomTypeLabel(_room.roomType));
    DrawText("WALL COLLIDERS (tile depth)", 1230, 49, 14, Fade(WHITE, .58f));
    static const char* wallNames[] = { "T", "B", "L", "R" };
    for (int i = 0; i < 4; ++i)
    {
        DrawButton(WallMinusRect(i), "-");
        Rectangle valueRect = WallValueRect(i);
        DrawRectangleRec(valueRect, Color{27,30,36,255});
        DrawRectangleLinesEx(valueRect, 1.f, Fade(WHITE, .22f));
        char value[24]{};
        std::snprintf(value, sizeof(value), "%s  %.2f", wallNames[i], WallDepthAt(_room, i));
        const int width = MeasureText(value, 16);
        DrawText(value, (int)(valueRect.x + (valueRect.width - width) * .5f),
                 (int)valueRect.y + 11, 16, RAYWHITE);
        DrawButton(WallPlusRect(i), "+");
    }
    const char* names[] = { "Ground", "Visual", "Door", "Collision",
                            "Props", "Decor", "Fall", "Doors", "Chest" };
    const Rectangle layerButtons[] = {
        {30,112,75,34}, {109,112,75,34}, {188,112,65,34},
        {257,112,85,34}, {346,112,65,34}, {415,112,65,34},
        {484,112,60,34}, {548,112,90,34}, {642,112,90,34}
    };
    for (int i = 0; i < 9; ++i)
        DrawButton(layerButtons[i], names[i], (int)_layer == i,
                   i != (int)Layer::ChestSpawn || _room.roomType == RoomType::Treasure);
    // Paint-tool selector + Clear Layer (must match the hit-rects in Update()).
    DrawButton({748,112,66,34}, "Brush",  _paintTool == PaintTool::Brush);
    DrawButton({818,112,66,34}, "Rect",   _paintTool == PaintTool::Rectangle);
    DrawButton({888,112,66,34}, "Bucket", _paintTool == PaintTool::Bucket);
    DrawButton({958,112,70,34}, "Eraser", _paintTool == PaintTool::Eraser);
    DrawButton({1032,112,80,34}, "Clear");
    DrawText("1-9 layer  |  Shift-drag = rectangle  |  B bucket  |  E eraser  |  Alt-click = pick  |  Ctrl+S save  |  Ctrl+Z/Y undo",
             30, 805, 15, Fade(WHITE,.55f));
}

void RoomEditor::DrawCanvas() const
{
    Rectangle canvas = CanvasRect();
    DrawRectangleRec(canvas, Color{10,12,15,255});
    for (int row = 0; row < RoomLayout::kRows; ++row)
    {
        for (int col = 0; col < RoomLayout::kCols; ++col)
        {
            Rectangle dst{ canvas.x + col*kCell, canvas.y + row*kCell, kCell, kCell };
            // Handcrafted art is authoritative. Logical floor, wall, void, and
            // door values remain available to collision without drawing defaults.
            DrawRectangleRec(dst, Color{16,18,24,255});
        }
    }

    // Draw one asset placement (decor or prop). Mirrors the in-game band order so
    // the editor preview matches what the room looks like at runtime.
    auto drawPlacement = [&](const RoomAssetPlacement& placement, bool showPropCollision)
    {
        const TileDefSet* definitions = DefinitionsFor(placement.sourceTileset);
        if (definitions == nullptr) return;
        int definitionIndex = definitions->FindAssetIndex(placement.kind, placement.assetId);
        if (definitionIndex < 0) return;
        Rectangle source{};
        std::string textureStem = placement.sourceTileset;
        Rectangle collision{};
        switch (placement.kind)
        {
        case RoomAssetKind::Prop:
            source=definitions->props[(std::size_t)definitionIndex].src;
            collision=definitions->props[(std::size_t)definitionIndex].collision;
            if (!definitions->props[(std::size_t)definitionIndex].sourceSheet.empty())
                textureStem=definitions->props[(std::size_t)definitionIndex].sourceSheet;
            break;
        case RoomAssetKind::AnimProp:
        {
            const auto& def=definitions->animProps[(std::size_t)definitionIndex];
            if (def.frames.empty()) return;
            source=def.frames[(std::size_t)AnimationFrame((int)def.frames.size(),def.fps,def.playback)];
            collision=def.collision; if(!def.sourceSheet.empty()) textureStem=def.sourceSheet; break;
        }
        case RoomAssetKind::Decor:
            source=definitions->decors[(std::size_t)definitionIndex].src;
            if(!definitions->decors[(std::size_t)definitionIndex].sourceSheet.empty())
                textureStem=definitions->decors[(std::size_t)definitionIndex].sourceSheet;
            break;
        case RoomAssetKind::AnimDecor:
        {
            const auto& def=definitions->animDecors[(std::size_t)definitionIndex];
            if(def.frames.empty()) return;
            source=def.frames[(std::size_t)AnimationFrame((int)def.frames.size(),def.fps,def.playback)];
            if(!def.sourceSheet.empty()) textureStem=def.sourceSheet; break;
        }
        }
        Rectangle destination{ canvas.x + placement.col*kCell, canvas.y + placement.row*kCell,
                               source.width * (kCell/16.f), source.height * (kCell/16.f) };
        Texture2D texture=TextureFor(textureStem);
        if (texture.id != 0) DrawTexturePro(texture, source, destination, {}, 0.f, WHITE);
        if (showPropCollision &&
            (placement.kind == RoomAssetKind::Prop || placement.kind == RoomAssetKind::AnimProp))
        {
            Rectangle worldCollision{ destination.x + collision.x*(kCell/16.f),
                                      destination.y + collision.y*(kCell/16.f),
                                      collision.width*(kCell/16.f), collision.height*(kCell/16.f) };
            DrawRectangleRec(worldCollision, Color{30,130,255,35});
            DrawRectangleLinesEx(worldCollision, 1.f, Color{70,170,255,190});
        }
    };
    auto isDecor = [](const RoomAssetPlacement& p)
    { return p.kind == RoomAssetKind::Decor || p.kind == RoomAssetKind::AnimDecor; };
    auto isProp = [](const RoomAssetPlacement& p)
    { return p.kind == RoomAssetKind::Prop || p.kind == RoomAssetKind::AnimProp; };
    auto drawVisualTiles = [&](Layer layer)
    {
        for (const RoomTilePlacement& visual : _room.visualTiles)
        {
            const bool belongs = layer == Layer::Ground ? visual.ground
                               : layer == Layer::Door ? visual.door
                               : !visual.ground && !visual.door;
            if (!belongs) continue;
            Texture2D texture = TextureFor(visual.sourceTileset);
            if (texture.id == 0) continue;
            Rectangle destination = RoomTileDestination(
                visual, canvas.x + visual.col*kCell, canvas.y + visual.row*kCell,
                kCell/16.f, kCell/16.f);
            DrawTexturePro(texture, visual.src, destination, {}, 0.f,
                           visual.ground ? WHITE : Color{255,255,255,245});
            if (visual.door && _layer == Layer::Door)
                DrawRectangleLinesEx(destination, 2.f, GOLD);
        }
    };
    auto drawDecorBand = [&](RoomDrawBand band)
    {
        for (const RoomAssetPlacement& p : _room.placements)
            if (isDecor(p) && p.band == band) drawPlacement(p, false);
    };

    // Ground band: ground tiles, then decors painted as ground.
    drawVisualTiles(Layer::Ground);
    drawDecorBand(RoomDrawBand::Ground);
    // Visual band: wall art, explicit Door art, then decors painted as visual.
    drawVisualTiles(Layer::Visual);
    drawVisualTiles(Layer::Door);
    drawDecorBand(RoomDrawBand::Visual);
    // Decor band: normal floor decor on top of walls.
    drawDecorBand(RoomDrawBand::Decor);
    // Props band: always the top layer, with collision boxes.
    for (const RoomAssetPlacement& p : _room.placements)
        if (isProp(p)) drawPlacement(p, true);

    if (_room.roomType == RoomType::Treasure && _room.HasTreasureChestSpawn())
    {
        Rectangle marker{ canvas.x + _room.treasureChestCol * kCell + 3.f,
                          canvas.y + _room.treasureChestRow * kCell + 3.f,
                          kCell - 6.f, kCell - 6.f };
        DrawChestMarker(marker, GOLD);
        DrawText("CHEST", (int)marker.x - 2, (int)marker.y - 17, 13, GOLD);
    }

    for (int row = 0; row < RoomLayout::kRows; ++row)
        for (int col = 0; col < RoomLayout::kCols; ++col)
            if (_room.fall[row][col])
            {
                Rectangle cell{canvas.x+col*kCell, canvas.y+row*kCell,kCell,kCell};
                const Color tint = FallSurfaceColor(_room.fallSurface, 80);
                const Color line = FallSurfaceColor(_room.fallSurface, 235);
                DrawRectangleRec(cell, tint);
                DrawLine((int)cell.x,(int)cell.y,(int)(cell.x+kCell),(int)(cell.y+kCell),line);
                DrawLine((int)(cell.x+kCell),(int)cell.y,(int)cell.x,(int)(cell.y+kCell),line);
            }
    // Precise fall rectangles sit alongside the legacy full-cell fall brush.
    for (int i = 0; i < (int)_room.fallRects.size(); ++i)
    {
        const Rectangle& rect = _room.fallRects[(std::size_t)i];
        Rectangle dst{ canvas.x + rect.x*kCell, canvas.y + rect.y*kCell,
                       rect.width*kCell, rect.height*kCell };
        const bool selected = i == _selectedCollider && _layer == Layer::FallZones;
        DrawRectangleRec(dst, FallSurfaceColor(_room.fallSurface,
                         selected ? (unsigned char)90 : (unsigned char)55));
        DrawRectangleLinesEx(dst, selected ? 2.f : 1.5f,
                             FallSurfaceColor(_room.fallSurface, selected ? 255 : 220));
        if (selected)
            DrawRectangleRec({ dst.x+dst.width-6.f, dst.y+dst.height-6.f, 8.f, 8.f },
                             FallSurfaceColor(_room.fallSurface, 255));
    }
    for (int row = 0; row < RoomLayout::kRows; ++row)
        for (int col = 0; col < RoomLayout::kCols; ++col)
            if (_room.solid[row][col])
            {
                Rectangle cell{canvas.x+col*kCell,canvas.y+row*kCell,kCell,kCell};
                DrawRectangleRec(cell,Color{30,130,255,28});
                DrawRectangleLinesEx(cell,1.f,Color{70,170,255,175});
            }
    // Free-size collider rectangles (drawn with the Rectangle tool on Collision).
    for (int i = 0; i < (int)_room.colliders.size(); ++i)
    {
        const Rectangle& c = _room.colliders[(std::size_t)i];
        Rectangle dst{ canvas.x + c.x*kCell, canvas.y + c.y*kCell, c.width*kCell, c.height*kCell };
        const bool sel = (i == _selectedCollider && _layer == Layer::Collision);
        DrawRectangleRec(dst, Color{ 40, 150, 255, sel ? (unsigned char)70 : (unsigned char)45 });
        DrawRectangleLinesEx(dst, sel ? 2.f : 1.5f, sel ? SKYBLUE : Color{ 90, 185, 255, 220 });
        if (sel)   // bottom-right resize handle
            DrawRectangleRec({ dst.x + dst.width - 6.f, dst.y + dst.height - 6.f, 8.f, 8.f }, SKYBLUE);
    }
    // Live preview while drawing a new collision or fall rectangle.
    if ((_layer == Layer::Collision || _layer == Layer::FallZones) &&
        _colliderDragMode == 3)
    {
        const Vector2 m = GetVirtualMousePos();
        const float tx = std::clamp((m.x - canvas.x) / kCell, 0.f, (float)RoomLayout::kCols);
        const float ty = std::clamp((m.y - canvas.y) / kCell, 0.f, (float)RoomLayout::kRows);
        Rectangle prev{ canvas.x + std::min(_colliderAnchor.x, tx)*kCell,
                        canvas.y + std::min(_colliderAnchor.y, ty)*kCell,
                        std::fabs(tx - _colliderAnchor.x)*kCell,
                        std::fabs(ty - _colliderAnchor.y)*kCell };
        const Color rectColor = _layer == Layer::FallZones ? RED : SKYBLUE;
        DrawRectangleRec(prev, Fade(rectColor, .28f));
        DrawRectangleLinesEx(prev, 2.f, rectColor);
    }
    // Door Zones are an editor guide only — shown while their layer is selected,
    // never baked into the room's art and never drawn during gameplay.
    if (_layer == Layer::Door || _layer == Layer::DoorZones)
    {
        // Show ALL four fixed lanes so the designer knows exactly where to leave a
        // path / paint the wall: bright gold = door ON, faint = door OFF.
        static const char* const kSideLabels[4] = { "North", "South", "West", "East" };
        for (int i=0;i<4;++i)
        {
            const Rectangle z = PredeterminedDoorZone((RoomWallSide)i);
            const bool on = _room.doorZones[i].enabled;
            Rectangle dst{canvas.x+z.x*kCell,canvas.y+z.y*kCell,z.width*kCell,z.height*kCell};
            DrawRectangleRec(dst, on?Color{255,190,30,55}:Color{255,190,30,16});
            DrawRectangleLinesEx(dst,2.f, on?GOLD:Fade(GOLD,.35f));
            DrawText(kSideLabels[i],(int)dst.x+4,(int)dst.y+4,16, on?GOLD:Fade(GOLD,.5f));
        }
    }
    DrawWallColliderOverlay();
    DrawPlacementPreview();
    for (int col = 0; col <= RoomLayout::kCols; ++col)
        DrawLine((int)(canvas.x+col*kCell),(int)canvas.y,(int)(canvas.x+col*kCell),(int)(canvas.y+canvas.height),Fade(WHITE,.10f));
    for (int row = 0; row <= RoomLayout::kRows; ++row)
        DrawLine((int)canvas.x,(int)(canvas.y+row*kCell),(int)(canvas.x+canvas.width),(int)(canvas.y+row*kCell),Fade(WHITE,.10f));
    DrawRectangleLinesEx(canvas, 2.f, Fade(WHITE,.5f));

    // Live rectangle-fill preview while dragging the Rectangle tool.
    int hoverCol = 0, hoverRow = 0;
    const bool onCell = ScreenToCell(GetVirtualMousePos(), hoverCol, hoverRow);
    if (_stroking && _strokeTool == PaintTool::Rectangle && onCell)
    {
        int c0 = std::min(_dragStartCol, hoverCol), c1 = std::max(_dragStartCol, hoverCol);
        int r0 = std::min(_dragStartRow, hoverRow), r1 = std::max(_dragStartRow, hoverRow);
        Rectangle preview{ canvas.x + c0 * kCell, canvas.y + r0 * kCell,
                           (c1 - c0 + 1) * kCell, (r1 - r0 + 1) * kCell };
        DrawRectangleRec(preview, Fade(_strokeAdd ? LIME : RED, .18f));
        DrawRectangleLinesEx(preview, 2.f, _strokeAdd ? LIME : RED);
    }

    // Coordinate readout under the cursor.
    if (onCell)
        DrawText(TextFormat("%d, %d", hoverCol, hoverRow),
                 (int)canvas.x + 6, (int)(canvas.y + canvas.height) - 22, 16, Fade(WHITE, .8f));

    // Live validation nudge: a room needs at least one exit before it can save.
    if (_room.DoorMask() == 0)
    {
        Rectangle warn{ canvas.x + canvas.width * .5f - 190.f, canvas.y + 8.f, 380.f, 30.f };
        DrawRectangleRec(warn, Fade(Color{ 160, 40, 40, 255 }, .9f));
        DrawText("Enable at least one exit (N/S/W/E)",
                 (int)warn.x + 20, (int)warn.y + 7, 17, RAYWHITE);
    }
}

void RoomEditor::DrawWallColliderOverlay() const
{
    const Rectangle canvas = CanvasRect();
    const Color fill{ 35, 150, 255, 36 };
    const Color line{ 75, 185, 255, 185 };
    auto drawCollider = [&](Rectangle rect)
    {
        DrawRectangleRec(rect, fill);
        DrawRectangleLinesEx(rect, 1.f, line);
    };

    for (int col = 0; col < RoomLayout::kCols; ++col)
    {
        const float x = canvas.x + col * kCell;
        if (IsSolidRoomTile(_room.tiles[0][col]))
            drawCollider({ x, canvas.y, kCell, _room.wallTopDepth * kCell });
        if (IsSolidRoomTile(_room.tiles[RoomLayout::kRows - 1][col]))
            drawCollider({ x, canvas.y + canvas.height - _room.wallBottomDepth * kCell,
                           kCell, _room.wallBottomDepth * kCell });
    }
    for (int row = 0; row < RoomLayout::kRows; ++row)
    {
        const float y = canvas.y + row * kCell;
        if (IsSolidRoomTile(_room.tiles[row][0]))
            drawCollider({ canvas.x, y, _room.wallLeftDepth * kCell, kCell });
        if (IsSolidRoomTile(_room.tiles[row][RoomLayout::kCols - 1]))
            drawCollider({ canvas.x + canvas.width - _room.wallRightDepth * kCell, y,
                           _room.wallRightDepth * kCell, kCell });
    }
}

void RoomEditor::DrawPlacementPreview() const
{
    if (_layer == Layer::ChestSpawn)
    {
        int col = 0, row = 0;
        if (!ScreenToCell(GetVirtualMousePos(), col, row)) return;
        const bool fits = TreasureChestSpawnFits(col, row);
        const Rectangle canvas = CanvasRect();
        Rectangle marker{ canvas.x + col * kCell + 3.f,
                          canvas.y + row * kCell + 3.f,
                          kCell - 6.f, kCell - 6.f };
        DrawChestMarker(marker, fits ? GOLD : RED);
        DrawText(fits ? "Click to set chest spawn" : "Chest spawn blocked",
                 (int)marker.x, (int)std::max(canvas.y, marker.y - 22.f), 15,
                 fits ? GOLD : RED);
        return;
    }

    if ((_layer == Layer::Ground || _layer == Layer::Visual || _layer == Layer::Door) &&
        !DecorModeOnTileLayer())
    {
        int col=0,row=0;
        if (!ScreenToCell(GetVirtualMousePos(),col,row)) return;
        const RoomAssetSource* source=SelectedSource();
        const GroundBrushTile* randomTile = _layer == Layer::Ground
            ? GroundBrushTileForCell(col, row) : nullptr;
        if (source == nullptr && randomTile == nullptr) return;
        const std::string sourceStem = randomTile != nullptr
            ? randomTile->sourceTileset : source->stem;
        const Rectangle sourceRect = randomTile != nullptr
            ? randomTile->source : _selectedRawTile;
        Texture2D texture=TextureFor(sourceStem);
        Rectangle canvas=CanvasRect();
        RoomTilePlacement previewPlacement{
            sourceStem, TileType::Floor, _layer == Layer::Ground,
            sourceRect, col, row };
        previewPlacement.door = _layer == Layer::Door;
        previewPlacement.anchorOffset = randomTile != nullptr
            ? randomTile->anchorOffset : _selectedTileAnchorOffset;
        Rectangle destination = RoomTileDestination(
            previewPlacement, canvas.x+col*kCell, canvas.y+row*kCell,
            kCell/16.f, kCell/16.f);
        const bool fits = _layer != Layer::Door ||
                          DoorVisualFitsEnabledZone(
                              col, row, sourceRect, previewPlacement.anchorOffset);
        const Color tint = fits ? Color{180,255,195,155} : Color{255,90,90,155};
        if(texture.id) DrawTexturePro(texture,sourceRect,destination,{},0.f,tint);
        DrawRectangleLinesEx(destination,2.f,fits ? LIME : RED);
        if (randomTile != nullptr)
            DrawText("Random ground preview", (int)destination.x,
                     (int)std::max(CanvasRect().y, destination.y - 20.f), 15, GOLD);
        if (_layer == Layer::Door)
            DrawText(fits ? "Door art" : "Enable a door and paint inside its gold lane",
                     (int)destination.x,
                     (int)std::max(CanvasRect().y, destination.y - 20.f), 16,
                     fits ? GOLD : RED);
        return;
    }
    if (!ShowingAssetPalette() || _selectedAssetId.empty())
        return;

    int col = 0, row = 0;
    if (!ScreenToCell(GetVirtualMousePos(), col, row)) return;
    const RoomAssetSource* selectedSource = SelectedSource();
    const std::string sourceStem = selectedSource != nullptr ? selectedSource->stem : _room.tilesetStem;
    const TileDefSet& definitions = SelectedDefinitions();
    const RoomAssetPlacement placement{ _selectedAssetKind, _selectedAssetId, col, row, sourceStem };
    const int definitionIndex = definitions.FindAssetIndex(placement.kind, placement.assetId);
    if (definitionIndex < 0) return;

    const Rectangle source = AssetSource(placement.kind, definitionIndex);
    if (source.width <= 0.f || source.height <= 0.f) return;
    const Rectangle canvas = CanvasRect();
    const Rectangle destination{
        canvas.x + col * kCell, canvas.y + row * kCell,
        source.width * (kCell / 16.f), source.height * (kCell / 16.f)
    };
    const bool fits = PlacementFits(placement);
    const Color tint = fits ? Color{ 120, 255, 145, 150 }
                            : Color{ 255, 85, 85, 150 };
    std::string textureStem = sourceStem;
    if (_selectedAssetKind == RoomAssetKind::Prop &&
        !definitions.props[(std::size_t)definitionIndex].sourceSheet.empty())
        textureStem = definitions.props[(std::size_t)definitionIndex].sourceSheet;
    else if (_selectedAssetKind == RoomAssetKind::AnimProp &&
             !definitions.animProps[(std::size_t)definitionIndex].sourceSheet.empty())
        textureStem = definitions.animProps[(std::size_t)definitionIndex].sourceSheet;
    else if (_selectedAssetKind == RoomAssetKind::Decor &&
             !definitions.decors[(std::size_t)definitionIndex].sourceSheet.empty())
        textureStem = definitions.decors[(std::size_t)definitionIndex].sourceSheet;
    else if (_selectedAssetKind == RoomAssetKind::AnimDecor &&
             !definitions.animDecors[(std::size_t)definitionIndex].sourceSheet.empty())
        textureStem = definitions.animDecors[(std::size_t)definitionIndex].sourceSheet;
    Texture2D texture = TextureFor(textureStem);
    if (texture.id != 0) DrawTexturePro(texture, source, destination, {}, 0.f, tint);
    else DrawRectangleRec(destination, Fade(tint, .35f));
    DrawRectangleLinesEx(destination, 2.f, fits ? LIME : RED);

    if (_selectedAssetKind == RoomAssetKind::Prop ||
        _selectedAssetKind == RoomAssetKind::AnimProp)
    {
        const Rectangle collision = _selectedAssetKind == RoomAssetKind::Prop
            ? definitions.props[(std::size_t)definitionIndex].collision
            : definitions.animProps[(std::size_t)definitionIndex].collision;
        const Rectangle previewCollision{
            destination.x + collision.x * (kCell / 16.f),
            destination.y + collision.y * (kCell / 16.f),
            collision.width * (kCell / 16.f),
            collision.height * (kCell / 16.f)
        };
        DrawRectangleRec(previewCollision, fits ? Color{40,220,100,45} : Color{230,40,40,55});
        DrawRectangleLinesEx(previewCollision, 1.f, fits ? LIME : RED);
    }
    DrawText(fits ? "Click to place" : "Blocked", (int)destination.x,
             (int)std::max(CanvasRect().y, destination.y - 20.f), 16,
             fits ? LIME : RED);
}

void RoomEditor::DrawPalette() const
{
    if (!_paletteVisible) return;
    Rectangle panel{1180,155,710,850};
    DrawRectangleRec(panel, Color{20,23,29,250});
    DrawRectangleLinesEx(panel,1.f,Fade(WHITE,.25f));
    const char* titles[] = { "Ground Tiles", "Visual Tiles", "Door Tiles",
                             "Collision Brush", "Props", "Decor",
                             "Fall Zone Brush", "Door Clear Zones",
                             "Treasure Chest Spawn" };
    DrawText(titles[(int)_layer],1205,172,24,GOLD);
    DrawText("Mouse wheel scrolls. TAB hides this panel.",1205,202,15,Fade(WHITE,.5f));
    // Ground/Visual: toggle between painting flat tiles and banded Decor assets.
    if (_layer == Layer::Ground || _layer == Layer::Visual)
        DrawButton({1700,170,180,30},
                   _tileLayerDecorMode ? "Mode: Decor" : "Mode: Tiles",
                   _tileLayerDecorMode);

    const bool groundTilePalette = _layer == Layer::Ground && !DecorModeOnTileLayer();
    if (groundTilePalette)
    {
        DrawButton(GroundRandomToggleRect(),
                   _randomGroundBrushEnabled ? "Random: ON" : "Random: OFF",
                   _randomGroundBrushEnabled, _groundBrushTiles.size() >= 2);
        DrawButton(GroundRandomAddRect(), "+ Add Selected");
        DrawButton(GroundRandomClearRect(), "Clear", false, !_groundBrushTiles.empty());
        DrawText(TextFormat("%d/8 tiles", (int)_groundBrushTiles.size()), 1660, 278, 16,
                 _groundBrushTiles.size() >= 2 ? GOLD : Fade(WHITE,.55f));
    }

    const RoomAssetSource* selectedSource=SelectedSource();
    DrawButton({1210,230,42,34},"<",false,selectedSource!=nullptr);
    DrawButton({1828,230,42,34},">",false,selectedSource!=nullptr);
    DrawRectangleRec({1260,230,560,34},Color{28,32,39,255});
    DrawRectangleLinesEx({1260,230,560,34},1.f,Fade(WHITE,.22f));
    std::string sourceName=selectedSource!=nullptr?selectedSource->stem:"No source sheets found";
    if (selectedSource != nullptr)
        sourceName += TextFormat("   (%d/%d)", _selectedSource + 1, (int)_catalog.Sources().size());
    DrawText(sourceName.c_str(),1272,238,17,RAYWHITE);

    // Search box (props/decor) — filters the cards below (hit-rect matches Update).
    if (ShowingAssetPalette())
    {
        Rectangle searchBox{1210,255,660,26};
        DrawRectangleRec(searchBox, _editingSearch?Color{45,70,100,255}:Color{28,32,39,255});
        DrawRectangleLinesEx(searchBox,1.f,_editingSearch?SKYBLUE:Fade(WHITE,.22f));
        const bool empty=_paletteSearch.empty();
        DrawText(empty?"Search props / decor...":_paletteSearch.c_str(),
                 1218,260,16,empty?Fade(WHITE,.4f):RAYWHITE);
    }

    const bool rawTilePalette = (_layer == Layer::Ground ||
        _layer == Layer::Visual || _layer == Layer::Door) &&
        !DecorModeOnTileLayer();
    const float tilePaletteTop = rawTilePalette
        ? (groundTilePalette ? 350.f : 310.f)
        : 283.f;
    const bool groundBrushPanelVisible = groundTilePalette && !_groundBrushTiles.empty();
    const float tilePaletteBottom = groundBrushPanelVisible
        ? 744.f : 988.f;
    if (rawTilePalette)
    {
        static const char* labels[8] = {
            "T-", "T+", "B-", "B+", "L-", "L+", "R-", "R+"
        };
        for (int i = 0; i < 8; ++i)
            DrawButton(TileOverhangButtonRect(i, groundTilePalette), labels[i]);
        const float right = _selectedRawTile.width - _selectedTileAnchorOffset.x - 16.f;
        const float bottom = _selectedRawTile.height - _selectedTileAnchorOffset.y - 16.f;
        DrawText(TextFormat("Overhang  T %.0f  B %.0f  L %.0f  R %.0f px",
                           _selectedTileAnchorOffset.y, bottom,
                           _selectedTileAnchorOffset.x, right),
                 1210, (int)tilePaletteTop - 8, 13, Fade(WHITE,.68f));
    }
    BeginScissorMode(1190,(int)tilePaletteTop,690,(int)(tilePaletteBottom-tilePaletteTop));
    if ((_layer == Layer::Ground || _layer == Layer::Visual || _layer == Layer::Door) &&
        !DecorModeOnTileLayer() && selectedSource!=nullptr)
    {
        // Draw the whole tilesheet as one contiguous, aligned image (like the
        // TileMapper), fit to the panel width, with a grid overlay and a
        // highlight on the selected tile — instead of separate padded cells.
        Texture2D texture=TextureFor(selectedSource->stem);
        const int cols=std::max(1,selectedSource->tileColumns);
        const int rows=std::max(1,selectedSource->tileRows);
        const float cellPx=660.f/(float)cols;
        const float ox=1210.f, oy=tilePaletteTop+2.f-_paletteScroll;
        if(texture.id)
            DrawTexturePro(texture,{0,0,(float)(cols*16),(float)(rows*16)},
                           {ox,oy,cols*cellPx,rows*cellPx},{},0.f,WHITE);
        else
            DrawRectangleRec({ox,oy,cols*cellPx,rows*cellPx},Color{20,22,28,255});
        for(int c=0;c<=cols;++c)
            DrawLineV({ox+c*cellPx,oy},{ox+c*cellPx,oy+rows*cellPx},Fade(WHITE,.14f));
        for(int r=0;r<=rows;++r)
            DrawLineV({ox,oy+r*cellPx},{ox+cols*cellPx,oy+r*cellPx},Fade(WHITE,.14f));
        const Rectangle selectedRect{
            ox + (_selectedRawTile.x / 16.f) * cellPx,
            oy + (_selectedRawTile.y / 16.f) * cellPx,
            (_selectedRawTile.width / 16.f) * cellPx,
            (_selectedRawTile.height / 16.f) * cellPx
        };
        DrawRectangleRec(selectedRect, Fade(SKYBLUE,.25f));
        DrawRectangleLinesEx(selectedRect, 2.5f, SKYBLUE);
        const Rectangle anchorRect{
            selectedRect.x + (_selectedTileAnchorOffset.x / 16.f) * cellPx,
            selectedRect.y + (_selectedTileAnchorOffset.y / 16.f) * cellPx,
            cellPx, cellPx
        };
        DrawRectangleLinesEx(anchorRect, 1.5f, GOLD);
        if (groundTilePalette)
            for (const GroundBrushTile& tile : _groundBrushTiles)
            {
                if (tile.sourceTileset != selectedSource->stem) continue;
                const int tileCol = (int)(tile.source.x / 16.f);
                const int tileRow = (int)(tile.source.y / 16.f);
                const Rectangle member{
                    ox + (tile.source.x / 16.f) * cellPx,
                    oy + (tile.source.y / 16.f) * cellPx,
                    (tile.source.width / 16.f) * cellPx,
                    (tile.source.height / 16.f) * cellPx };
                DrawRectangleLinesEx(member, 3.f, GOLD);
                DrawRectangleRec({member.x, member.y, std::min(28.f, member.width),
                                  std::min(18.f, member.height)}, Color{20,20,20,210});
                DrawText(TextFormat("%d", tile.weight), (int)member.x + 3,
                         (int)member.y + 1, 14, GOLD);
            }
    }
    else if (ShowingAssetPalette())
    {
        const TileDefSet& definitions=SelectedDefinitions();
        const std::vector<int> matches=MatchingAssetIndices();
        for (int slot=0;slot<(int)matches.size();++slot)
        {
            int definitionIndex=0; RoomAssetKind kind=AssetKindAtPaletteIndex(matches[slot],definitionIndex);
            Rectangle item{1210.f+(slot%5)*128.f,285.f+(slot/5)*108.f-_paletteScroll,116,96};
            std::string id;
            std::string textureStem=selectedSource!=nullptr?selectedSource->stem:_room.tilesetStem;
            if (kind==RoomAssetKind::Prop) { const auto& d=definitions.props[(std::size_t)definitionIndex]; id=d.id; if(!d.sourceSheet.empty())textureStem=d.sourceSheet; }
            else if (kind==RoomAssetKind::AnimProp) { const auto& d=definitions.animProps[(std::size_t)definitionIndex]; id=d.id; if(!d.sourceSheet.empty())textureStem=d.sourceSheet; }
            else if (kind==RoomAssetKind::Decor) { const auto& d=definitions.decors[(std::size_t)definitionIndex]; id=d.id; if(!d.sourceSheet.empty())textureStem=d.sourceSheet; }
            else { const auto& d=definitions.animDecors[(std::size_t)definitionIndex]; id=d.id; if(!d.sourceSheet.empty())textureStem=d.sourceSheet; }
            DrawRectangleRec(item,_selectedAssetId==id?Color{35,95,145,255}:Color{35,38,45,255});
            Rectangle source=AssetSource(kind,definitionIndex);
            Rectangle preview=FitSourceInside(source,{item.x+7,item.y+5,102,52});
            Texture2D texture=TextureFor(textureStem);
            if (texture.id) DrawTexturePro(texture,source,preview,{},0,WHITE);
            DrawText(AssetName(kind,definitionIndex),(int)item.x+5,(int)item.y+62,12,RAYWHITE);
        }
    }
    else if (_layer==Layer::Collision)
    {
        DrawRectangle(1230,305,70,70,Color{30,130,255,55});
        DrawRectangleLines(1230,305,70,70,Color{70,170,255,220});
        DrawText("Brush/Bucket: paint FULL-TILE cells (left add, right remove).",1230,400,16,RAYWHITE);
        DrawText("Rect tool: drag a FREE box — any size, even sub-tile. Then",1230,428,16,Fade(WHITE,.8f));
        DrawText("drag its body to move, bottom-right to resize; right-click it",1230,456,16,Fade(WHITE,.8f));
        DrawText("to delete. Hold Shift = snap 1/4 tile. Great for fences /",1230,484,16,Fade(WHITE,.8f));
        DrawText("thin invisible walls. Art & collision stay independent.",1230,512,16,Fade(WHITE,.8f));
    }
    else if (_layer==Layer::FallZones)
    {
        DrawRectangleRec({1230,305,70,70}, FallSurfaceColor(_room.fallSurface, 80));
        DrawText("Brush/Bucket: paint full-tile fall cells.",1230,400,16,RAYWHITE);
        DrawText("Rect: drag a precise fall box, then drag it to move or resize.",1230,428,16,Fade(WHITE,.8f));
        DrawText("Right-click deletes a box. Hold Shift for quarter-tile snap.",1230,456,16,Fade(WHITE,.8f));
        DrawText("The player's feet must enter the marked area before falling.",1230,484,16,Fade(WHITE,.8f));
        DrawText(TextFormat("ROOM FALL SURFACE: %s", FallSurfaceLabel(_room.fallSurface)),
                 1230,510,16,GOLD);
        DrawButton(FallSurfaceButtonRect(0),
                   _room.fallSurface == FallSurface::Water ? "[x] Water" : "[ ] Water",
                   _room.fallSurface == FallSurface::Water);
        DrawButton(FallSurfaceButtonRect(1),
                   _room.fallSurface == FallSurface::Lava ? "[x] Lava" : "[ ] Lava",
                   _room.fallSurface == FallSurface::Lava);
        DrawText("Choose Water or Lava. Click the active choice again for Void.",
                 1230,585,15,Fade(WHITE,.7f));
    }
    else if (_layer==Layer::Door)
    {
        DrawText("Choose any tileset tile, then paint only inside an enabled",1230,400,16,RAYWHITE);
        DrawText("gold N/S/W/E lane. Both Door and overlapping Visual art",1230,428,16,Fade(WHITE,.8f));
        DrawText("disappear when that exit opens, revealing Ground underneath.",1230,456,16,Fade(WHITE,.8f));
    }
    else if (_layer==Layer::DoorZones)
    {
        // Predetermined doors: one on/off toggle per side, fixed positions.
        static const char* sides[]={"North","South","West","East"};
        const bool on[4]={_room.hasNorth,_room.hasSouth,_room.hasWest,_room.hasEast};
        for(int i=0;i<4;++i)
            DrawButton({1210.f+i*120.f,300,110,40}, sides[i], on[i]);
        DrawText("Doors are fixed to the dungeon door lanes — toggle each side",1210,360,16,RAYWHITE);
        DrawText("on or off. Any non-ground tile over an enabled gold lane",1210,388,16,Fade(WHITE,.7f));
        DrawText("clears when opened; Ground always remains underneath.",1210,416,16,Fade(WHITE,.7f));
        DrawText("Gold = door ON.  Faint = door OFF (still a fixed lane).",1210,456,16,Fade(GOLD,.85f));
    }
    else if (_layer==Layer::ChestSpawn)
    {
        DrawChestMarker({1230,305,70,70}, GOLD);
        DrawText("Click a clear floor tile to place or move the treasure chest.",
                 1230,400,16,RAYWHITE);
        DrawText("Red means the tile overlaps a wall, fall zone, door, collider,",
                 1230,428,16,Fade(WHITE,.8f));
        DrawText("or blocking prop. Right-click the canvas to clear the marker.",
                 1230,456,16,Fade(WHITE,.8f));
        DrawText(_room.HasTreasureChestSpawn()
                     ? TextFormat("CHEST CELL: %d, %d", _room.treasureChestCol,
                                  _room.treasureChestRow)
                     : "CHEST CELL: not placed (runtime uses room centre)",
                 1230,500,16,GOLD);
    }
    EndScissorMode();

    if (groundBrushPanelVisible)
    {
        const Rectangle mixPanel = GroundBrushPanelRect();
        DrawRectangleRec(mixPanel, Color{24,27,33,255});
        DrawRectangleLinesEx(mixPanel, 1.f, Fade(GOLD,.45f));
        DrawRectangleRec(GroundBrushNameRect(),
                         _editingGroundBrushName ? Color{45,70,100,255}
                                                : Color{31,35,42,255});
        DrawRectangleLinesEx(GroundBrushNameRect(), 1.f,
                             _editingGroundBrushName ? SKYBLUE : Fade(WHITE,.25f));
        DrawText(_groundBrushPresetName.empty() ? "Preset name..." : _groundBrushPresetName.c_str(),
                 1218, 772, 16, _groundBrushPresetName.empty() ? Fade(WHITE,.4f) : RAYWHITE);
        DrawButton(GroundBrushSaveRect(), "Save", false, !_groundBrushTiles.empty());
        DrawButton(GroundBrushPrevRect(), "<", false, !_groundBrushPresetPaths.empty());
        DrawButton(GroundBrushNextRect(), ">", false, !_groundBrushPresetPaths.empty());
        DrawText(_groundBrushPresetPaths.empty()
                     ? "No saved presets"
                     : TextFormat("Preset %d/%d", _groundBrushPresetIndex + 1,
                                  (int)_groundBrushPresetPaths.size()),
                 1655, 773, 15, Fade(WHITE,.65f));

        for (int i = 0; i < (int)_groundBrushTiles.size(); ++i)
        {
            const GroundBrushTile& tile = _groundBrushTiles[(std::size_t)i];
            const Rectangle card = GroundBrushCardRect(i);
            DrawRectangleRec(card, Color{34,38,46,255});
            DrawRectangleLinesEx(card, 1.f, Fade(WHITE,.18f));
            const Rectangle preview = FitSourceInside(tile.source,
                { card.x + 5.f, card.y + 5.f, 38.f, 58.f });
            const Texture2D texture = TextureFor(tile.sourceTileset);
            if (texture.id != 0) DrawTexturePro(texture, tile.source, preview, {}, 0.f, WHITE);
            DrawText(TextFormat("%d,%d", (int)tile.source.x / 16, (int)tile.source.y / 16),
                     (int)card.x + 48, (int)card.y + 7, 13, Fade(WHITE,.72f));
            DrawButton(GroundBrushMinusRect(i), "-", false, tile.weight > 1);
            DrawText(TextFormat("%d", tile.weight), (int)card.x + 78,
                     (int)card.y + 44, 16, GOLD);
            DrawButton(GroundBrushPlusRect(i), "+", false, tile.weight < 20);
            DrawButton(GroundBrushRemoveRect(i), "X");
        }
        DrawText("Weights control frequency: 8 is common, 1 is occasional.",
                 1210, 971, 14, Fade(WHITE,.55f));
    }
}

void RoomEditor::DrawLibrary() const
{
    DrawRectangle(0,0,kVirtualWidth,kVirtualHeight,Color{10,12,16,255});
    DrawText("ROOM LIBRARY",240,80,38,GOLD);
    DrawText((_room.tilesetStem+" rooms - click a row to edit").c_str(),240,125,20,Fade(WHITE,.65f));
    const auto coverage = _library.DoorMaskCounts(_room.biome, _room.tilesetStem);
    const int complete = CompletedCoverageCount(coverage);
    DrawButton(CoverageToggleRect(), TextFormat("Room Coverage  %d/15", complete),
               _coverageExpanded);
    DrawButton({1500,82,120,38},"Close");
    BeginScissorMode(220,170,1400,780);
    const auto rooms=_library.RoomsFor(_room.biome, _room.tilesetStem);
    for(int i=0;i<(int)rooms.size();++i)
    {
        float y=180.f+i*62.f-_libraryScroll;
        Rectangle row{240,y,_coverageExpanded?1015.f:1360.f,52};
        DrawRectangleRec(row,Color{30,34,41,255});
        DrawRectangleLinesEx(row,1.f,Fade(WHITE,.2f));
        DrawText(rooms[(std::size_t)i]->name.c_str(),260,(int)y+14,20,RAYWHITE);
        DrawText(RoomTypeLabel(rooms[(std::size_t)i]->roomType),700,(int)y+15,18,SKYBLUE);
        const bool armed = !_pendingDeleteId.empty() && _pendingDeleteId == rooms[(std::size_t)i]->id;
        const float removeWidth=armed?90.f:50.f;
        Rectangle remove{row.x+row.width-removeWidth-10.f,y+7,removeWidth,38};
        Rectangle copy{remove.x-70.f,y+7,60,38};
        const std::string doors="Doors "+DoorMaskName(rooms[(std::size_t)i]->DoorMask());
        DrawText(doors.c_str(),900,(int)y+15,18,Fade(WHITE,.7f));
        DrawButton(copy,"Copy");
        DrawButton(remove,armed?"Sure?":"X",armed);
    }
    EndScissorMode();

    if (_coverageExpanded)
    {
        const Rectangle panel=CoveragePanelRect();
        DrawRectangleRec(panel,Color{20,23,29,252});
        DrawRectangleLinesEx(panel,2.f,Fade(GOLD,.65f));
        DrawText((_room.tilesetStem+" ROOM COVERAGE").c_str(),1305,170,25,GOLD);
        DrawText("Uses the saved N/S/E/W exits, never room names.",1305,204,15,Fade(WHITE,.62f));
        DrawText(TextFormat("%d complete   |   %d missing",complete,15-complete),1305,230,19,RAYWHITE);

        static const char* groupNames[]={"ONE-WAY ROOMS","TWO-WAY ROOMS","THREE-WAY ROOMS","FOUR-WAY ROOM"};
        static const int groupY[]={257,377,552,672};
        for(int i=0;i<4;++i)
            DrawText(groupNames[i],1305,groupY[i],16,Fade(GOLD,.85f));

        for(const CoverageChip& chip:kCoverageChips)
        {
            const int count=coverage[chip.mask];
            const bool present=count>0;
            const Rectangle rect=CoverageChipRect(chip);
            const Color fill=present?Color{35,112,73,255}:Color{123,43,48,255};
            const Color border=present?Color{92,220,143,255}:Color{245,101,106,255};
            DrawRectangleRec(rect,fill);
            DrawRectangleLinesEx(rect,1.5f,border);
            DrawText(chip.label,(int)rect.x+10,(int)rect.y+10,20,RAYWHITE);
            const char* state=present?(count>1?TextFormat("x%d",count):"Done"):"Missing";
            const int stateWidth=MeasureText(state,15);
            DrawText(state,(int)(rect.x+rect.width-stateWidth-9),(int)rect.y+13,15,
                     present?Color{165,255,199,255}:Color{255,190,190,255});
        }
        DrawText("Click a red Missing room to create it with the correct exits.",1305,780,16,Fade(WHITE,.72f));
        DrawText("It stays unsaved until you finish the room and press Save.",1305,807,16,Fade(WHITE,.58f));
    }
    DrawText("Copy = duplicate a room   |   X = delete (click twice to confirm)",240,960,18,Fade(WHITE,.5f));
}

void RoomEditor::Draw() const
{
    ClearBackground(Color{14,16,20,255});
    if (_showLibrary) { DrawLibrary(); return; }
    DrawToolbar();
    DrawCanvas();
    DrawPalette();
    if (_statusTimer>0.f && !_status.empty())
    {
        DrawRectangle(30,1015,1120,38,Color{0,0,0,185});
        DrawText(_status.c_str(),45,1024,18,RAYWHITE);
    }
}

#endif
