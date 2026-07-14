#include "RoomEditor.h"
#include "RoomCollision.h"
#include "VirtualCanvas.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
    constexpr int kDoorSpanCols = 5;
    constexpr int kDoorSpanRows = 3;
    constexpr int kDoorStartCol = RoomLayout::kCols / 2 - kDoorSpanCols / 2;
    constexpr int kDoorStartRow = RoomLayout::kRows / 2 - kDoorSpanRows / 2;
    constexpr std::size_t kHistoryLimit = 128;

    int TilesFor(float sourcePixels)
    {
        return std::max(1, (int)std::ceil(sourcePixels / 16.f));
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

bool RoomEditor::IsProtectedDoorCell(int col, int row) const
{
    const bool inVerticalLane = col >= kDoorStartCol - 1 &&
                                col < kDoorStartCol + kDoorSpanCols + 1;
    const bool inHorizontalLane = row >= kDoorStartRow - 1 &&
                                  row < kDoorStartRow + kDoorSpanRows + 1;
    if (_room.hasNorth && inVerticalLane && row <= 1) return true;
    if (_room.hasSouth && inVerticalLane && row >= RoomLayout::kRows - 2) return true;
    if (_room.hasWest && inHorizontalLane && col <= 1) return true;
    if (_room.hasEast && inHorizontalLane && col >= RoomLayout::kCols - 2) return true;
    return false;
}

void RoomEditor::PushUndo()
{
    if (_undo.size() >= kHistoryLimit) _undo.erase(_undo.begin());
    _undo.push_back(_room);
    _redo.clear();
}

bool RoomEditor::SetTerrain(int col, int row, TileType tile)
{
    if (!IsCellValid(col, row) || tile < TileType::Floor || tile >= TileType::Count)
        return false;
    if (_room.tiles[row][col] == tile) return false;
    PushUndo();
    _room.tiles[row][col] = tile;
    UpdateDoorTiles();
    return true;
}

bool RoomEditor::SetVisual(int col, int row, bool ground, const std::string& source,
                           Rectangle sourceRect)
{
    if (!IsCellValid(col, row) || source.empty() ||
        sourceRect.width <= 0.f || sourceRect.height <= 0.f) return false;
    auto& visuals = _room.visualTiles;
    const auto same = std::find_if(visuals.begin(), visuals.end(),
        [=](const RoomTilePlacement& item)
        {
            return item.col == col && item.row == row && item.ground == ground &&
                   item.sourceTileset == source && item.src.x == sourceRect.x &&
                   item.src.y == sourceRect.y && item.src.width == sourceRect.width &&
                   item.src.height == sourceRect.height;
        });
    if (same != visuals.end()) return false;
    PushUndo();
    visuals.erase(std::remove_if(visuals.begin(), visuals.end(),
        [=](const RoomTilePlacement& item)
        {
            return item.col == col && item.row == row && item.ground == ground;
        }), visuals.end());
    visuals.push_back({ source, TileType::Floor, ground, sourceRect, col, row });
    return true;
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
    if (!IsCellValid(col, row) || IsProtectedDoorCell(col, row)) return false;
    if (_room.fall[row][col] == enabled) return false;
    PushUndo();
    _room.fall[row][col] = enabled;
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
    for (int row = placement.row; row < placement.row + height; ++row)
        for (int col = placement.col; col < placement.col + width; ++col)
            if (IsProtectedDoorCell(col, row)) return false;

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
    _room.doorZones[(int)RoomWallSide::Top].enabled = north;
    _room.doorZones[(int)RoomWallSide::Bottom].enabled = south;
    _room.doorZones[(int)RoomWallSide::Left].enabled = west;
    _room.doorZones[(int)RoomWallSide::Right].enabled = east;
    UpdateDoorTiles();
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

void RoomEditor::UpdateDoorTiles()
{
    for (int offset = 0; offset < kDoorSpanCols; ++offset)
    {
        const int col = kDoorStartCol + offset;
        _room.tiles[0][col] = _room.hasNorth ? TileType::DoorOpen : TileType::WallTopFace;
        _room.tiles[RoomLayout::kRows - 1][col] =
            _room.hasSouth ? TileType::DoorOpen : TileType::WallBottom;
    }
    for (int offset = 0; offset < kDoorSpanRows; ++offset)
    {
        const int row = kDoorStartRow + offset;
        _room.tiles[row][0] = _room.hasWest ? TileType::DoorOpen : TileType::WallLeft;
        _room.tiles[row][RoomLayout::kCols - 1] =
            _room.hasEast ? TileType::DoorOpen : TileType::WallRight;
    }
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
}

void RoomEditor::Bind(const std::string& tilesetStem, Biome biome,
                      const TileDefSet& definitions, Texture2D sheet, Texture2D groundSheet,
                      const std::filesystem::path& roomRoot,
                      const std::filesystem::path& tilesetRoot)
{
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
    if (_layer == Layer::Props)
        return (int)definitions.props.size() + (int)definitions.animProps.size();
    if (_layer == Layer::Decor)
        return (int)definitions.decors.size() + (int)definitions.animDecors.size();
    return 0;
}

RoomAssetKind RoomEditor::AssetKindAtPaletteIndex(int index, int& definitionIndex) const
{
    const TileDefSet& definitions = SelectedDefinitions();
    if (_layer == Layer::Props)
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

void RoomEditor::UpdateLibrary()
{
    Vector2 mouse = GetVirtualMousePos();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        CheckCollisionPointRec(mouse, {1500,82,120,38}))
    {
        _showLibrary = false;
        return;
    }
    _libraryScroll = std::max(0.f, _libraryScroll - GetMouseWheelMove() * 65.f);
    const auto& rooms = _library.Rooms();
    for (int i = 0; i < (int)rooms.size(); ++i)
    {
        float y = 180.f + i * 62.f - _libraryScroll;
        Rectangle row{ 240.f, y, 1360.f, 52.f };
        Rectangle remove{ 1535.f, y + 7.f, 50.f, 38.f };
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, remove))
        {
            std::string error;
            _library.DeleteRoom(rooms[(std::size_t)i].id, error);
            _status = error.empty() ? "Room deleted" : error;
            _statusTimer = 3.f;
            return;
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, row))
        {
            OpenRoom(rooms[(std::size_t)i]);
            return;
        }
    }
    if (IsKeyPressed(KEY_ESCAPE)) _showLibrary = false;
}

void RoomEditor::UpdateCanvas()
{
    Vector2 mouse = GetVirtualMousePos();
    int col = 0, row = 0;
    if (!ScreenToCell(mouse, col, row)) return;

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    {
        const RoomAssetSource* source = SelectedSource();
        if ((_layer == Layer::Ground || _layer == Layer::Visual) && source != nullptr)
            SetVisual(col, row, _layer == Layer::Ground, source->stem, _selectedRawTile);
        else if (_layer == Layer::Collision) SetSolid(col, row, true);
        else if (_layer == Layer::FallZones) SetFall(col, row, true);
    }
    if ((_layer == Layer::Props || _layer == Layer::Decor) &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !_selectedAssetId.empty())
    {
        const RoomAssetSource* source = SelectedSource();
        PlaceAsset({ _selectedAssetKind, _selectedAssetId, col, row,
                     source != nullptr ? source->stem : _room.tilesetStem });
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
    {
        if (_layer == Layer::Ground || _layer == Layer::Visual)
        {
            const bool ground = _layer == Layer::Ground;
            auto found = std::find_if(_room.visualTiles.rbegin(), _room.visualTiles.rend(),
                [=](const RoomTilePlacement& visual)
                { return visual.col == col && visual.row == row && visual.ground == ground; });
            if (found != _room.visualTiles.rend())
            {
                PushUndo();
                _room.visualTiles.erase(std::next(found).base());
            }
        }
        else if (_layer == Layer::Collision) SetSolid(col, row, false);
        else if (_layer == Layer::FallZones) SetFall(col, row, false);
        else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
            RemoveAssetAt(col, row, _selectedAssetKind);
    }
}

void RoomEditor::Update()
{
    if (_statusTimer > 0.f) _statusTimer -= GetFrameTime();
    if (IsKeyPressed(KEY_TAB)) _paletteVisible = !_paletteVisible;
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Z)) Undo();
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Y)) Redo();
    if (!_editingName && IsKeyPressed(KEY_S)) SaveRoom();

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
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        _editingName = CheckCollisionPointRec(mouse, nameBox);
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

    const Rectangle layerButtons[] = {
        {30,112,100,34}, {136,112,100,34}, {242,112,100,34},
        {348,112,100,34}, {454,112,100,34}, {560,112,110,34},
        {676,112,120,34}
    };
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        for (int i = 0; i < 7; ++i)
            if (CheckCollisionPointRec(mouse, layerButtons[i])) _layer = (Layer)i;

    if (_paletteVisible && mouse.x >= 1180.f)
    {
        _paletteScroll = std::max(0.f, _paletteScroll - GetMouseWheelMove() * 90.f);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            const int sourceCount = (int)_catalog.Sources().size();
            if (sourceCount > 0 && CheckCollisionPointRec(mouse, {1210,230,42,34}))
            {
                _selectedSource = (_selectedSource + sourceCount - 1) % sourceCount;
                _selectedAssetId.clear();
                _selectedRawTile = {0.f,0.f,16.f,16.f};
                _paletteScroll = 0.f;
            }
            else if (sourceCount > 0 && CheckCollisionPointRec(mouse, {1828,230,42,34}))
            {
                _selectedSource = (_selectedSource + 1) % sourceCount;
                _selectedAssetId.clear();
                _selectedRawTile = {0.f,0.f,16.f,16.f};
                _paletteScroll = 0.f;
            }
            else if (_layer == Layer::Ground || _layer == Layer::Visual)
            {
                const RoomAssetSource* source = SelectedSource();
                if (source != nullptr)
                {
                    const int count = source->tileColumns * source->tileRows;
                    for (int i = 0; i < count; ++i)
                    {
                        Rectangle item{1210.f+(i%10)*64.f,285.f+(i/10)*64.f-_paletteScroll,58,58};
                        if (!CheckCollisionPointRec(mouse, item)) continue;
                        _selectedRawTile = { (float)((i % source->tileColumns) * 16),
                            (float)((i / source->tileColumns) * 16), 16.f, 16.f };
                        break;
                    }
                }
            }
            else if (_layer == Layer::Props || _layer == Layer::Decor)
            {
                const TileDefSet& definitions = SelectedDefinitions();
                for (int i = 0; i < AssetCountForLayer(); ++i)
                {
                    Rectangle item{ 1210.f + (i % 5) * 128.f,
                                    285.f + (i / 5) * 108.f - _paletteScroll, 116.f, 96.f };
                    if (!CheckCollisionPointRec(mouse, item)) continue;
                    int definitionIndex = 0;
                    _selectedAssetKind = AssetKindAtPaletteIndex(i, definitionIndex);
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
                for (int i = 0; i < 4; ++i)
                    if (CheckCollisionPointRec(mouse, {1210.f+i*90.f,285,82,34})) _selectedDoorZone=i;
                RoomDoorZone& zone = _room.doorZones[_selectedDoorZone];
                float* values[] = { &zone.tiles.x, &zone.tiles.y, &zone.tiles.width, &zone.tiles.height };
                for (int i = 0; i < 4; ++i)
                {
                    if (CheckCollisionPointRec(mouse,{1210.f+i*155.f,350,34,34}))
                    { PushUndo(); *values[i] = std::max(i < 2 ? 0.f : .25f, *values[i]-.25f); }
                    if (CheckCollisionPointRec(mouse,{1318.f+i*155.f,350,34,34}))
                    { PushUndo(); *values[i] += .25f; }
                }
            }
        }
    }
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
    const char* names[] = { "Ground", "Visual", "Collision", "Props", "Decor", "Fall", "Door Zones" };
    const float widths[] = {100,100,100,100,100,110,120};
    float x = 30.f;
    for (int i = 0; i < 7; ++i)
    {
        DrawButton({x,112,widths[i],34}, names[i], (int)_layer == i);
        x += widths[i] + 6.f;
    }
    DrawText("TAB hides palette   Ctrl+Z/Y undo/redo", 820, 120, 15, Fade(WHITE,.55f));
}

void RoomEditor::DrawCanvas() const
{
    Rectangle canvas = CanvasRect();
    DrawRectangleRec(canvas, Color{10,12,15,255});
    for (int row = 0; row < RoomLayout::kRows; ++row)
    {
        for (int col = 0; col < RoomLayout::kCols; ++col)
        {
            TileType tile = _room.tiles[row][col];
            int index = (int)tile;
            Rectangle dst{ canvas.x + col*kCell, canvas.y + row*kCell, kCell, kCell };
            if (index >= 0 && index < (int)TileType::Count && _definitions.assigned[index])
            {
                Texture2D texture = _definitions.fromGround[index] ? _groundSheet : _sheet;
                if (texture.id != 0) DrawTexturePro(texture, _definitions.rects[index], dst, {}, 0.f, WHITE);
            }
            else DrawRectangleRec(dst, index == (int)TileType::Void ? BLACK : Color{35,45,35,255});
        }
    }

    for (const RoomTilePlacement& visual : _room.visualTiles)
    {
        Texture2D texture = TextureFor(visual.sourceTileset);
        if (texture.id == 0) continue;
        Rectangle destination{ canvas.x + visual.col*kCell, canvas.y + visual.row*kCell,
            visual.src.width*(kCell/16.f), visual.src.height*(kCell/16.f) };
        DrawTexturePro(texture, visual.src, destination, {}, 0.f,
                       visual.ground ? WHITE : Color{255,255,255,245});
    }

    for (const RoomAssetPlacement& placement : _room.placements)
    {
        const TileDefSet* definitions = DefinitionsFor(placement.sourceTileset);
        if (definitions == nullptr) continue;
        int definitionIndex = definitions->FindAssetIndex(placement.kind, placement.assetId);
        if (definitionIndex < 0) continue;
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
            if (def.frames.empty()) continue;
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
            if(def.frames.empty()) continue;
            source=def.frames[(std::size_t)AnimationFrame((int)def.frames.size(),def.fps,def.playback)];
            if(!def.sourceSheet.empty()) textureStem=def.sourceSheet; break;
        }
        }
        Rectangle destination{ canvas.x + placement.col*kCell, canvas.y + placement.row*kCell,
                               source.width * (kCell/16.f), source.height * (kCell/16.f) };
        Texture2D texture=TextureFor(textureStem);
        if (texture.id != 0) DrawTexturePro(texture, source, destination, {}, 0.f, WHITE);
        if (placement.kind == RoomAssetKind::Prop || placement.kind == RoomAssetKind::AnimProp)
        {
            Rectangle worldCollision{ destination.x + collision.x*(kCell/16.f),
                                      destination.y + collision.y*(kCell/16.f),
                                      collision.width*(kCell/16.f), collision.height*(kCell/16.f) };
            DrawRectangleRec(worldCollision, Color{30,130,255,35});
            DrawRectangleLinesEx(worldCollision, 1.f, Color{70,170,255,190});
        }
    }

    for (int row = 0; row < RoomLayout::kRows; ++row)
        for (int col = 0; col < RoomLayout::kCols; ++col)
            if (_room.fall[row][col])
            {
                Rectangle cell{canvas.x+col*kCell, canvas.y+row*kCell,kCell,kCell};
                DrawRectangleRec(cell, Color{210,35,35,80});
                DrawLine((int)cell.x,(int)cell.y,(int)(cell.x+kCell),(int)(cell.y+kCell),RED);
                DrawLine((int)(cell.x+kCell),(int)cell.y,(int)cell.x,(int)(cell.y+kCell),RED);
            }
    for (int row = 0; row < RoomLayout::kRows; ++row)
        for (int col = 0; col < RoomLayout::kCols; ++col)
            if (_room.solid[row][col])
            {
                Rectangle cell{canvas.x+col*kCell,canvas.y+row*kCell,kCell,kCell};
                DrawRectangleRec(cell,Color{30,130,255,28});
                DrawRectangleLinesEx(cell,1.f,Color{70,170,255,175});
            }
    for (int i=0;i<4;++i)
        if (_room.doorZones[i].enabled)
        {
            const Rectangle& z=_room.doorZones[i].tiles;
            Rectangle dst{canvas.x+z.x*kCell,canvas.y+z.y*kCell,z.width*kCell,z.height*kCell};
            DrawRectangleRec(dst,i==_selectedDoorZone?Color{255,190,30,48}:Color{255,190,30,24});
            DrawRectangleLinesEx(dst,2.f,i==_selectedDoorZone?GOLD:Fade(GOLD,.55f));
        }
    DrawWallColliderOverlay();
    DrawPlacementPreview();
    for (int col = 0; col <= RoomLayout::kCols; ++col)
        DrawLine((int)(canvas.x+col*kCell),(int)canvas.y,(int)(canvas.x+col*kCell),(int)(canvas.y+canvas.height),Fade(WHITE,.10f));
    for (int row = 0; row <= RoomLayout::kRows; ++row)
        DrawLine((int)canvas.x,(int)(canvas.y+row*kCell),(int)(canvas.x+canvas.width),(int)(canvas.y+row*kCell),Fade(WHITE,.10f));
    DrawRectangleLinesEx(canvas, 2.f, Fade(WHITE,.5f));
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
    if (_layer == Layer::Ground || _layer == Layer::Visual)
    {
        int col=0,row=0;
        const RoomAssetSource* source=SelectedSource();
        if (source==nullptr || !ScreenToCell(GetVirtualMousePos(),col,row)) return;
        Texture2D texture=TextureFor(source->stem);
        Rectangle canvas=CanvasRect();
        Rectangle destination{canvas.x+col*kCell,canvas.y+row*kCell,
            _selectedRawTile.width*(kCell/16.f),_selectedRawTile.height*(kCell/16.f)};
        if(texture.id) DrawTexturePro(texture,_selectedRawTile,destination,{},0.f,Color{180,255,195,155});
        DrawRectangleLinesEx(destination,2.f,LIME);
        return;
    }
    if ((_layer != Layer::Props && _layer != Layer::Decor) || _selectedAssetId.empty())
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
    const char* titles[] = { "Ground Tiles", "Visual Tiles", "Collision Brush", "Props",
                             "Decor", "Fall Zone Brush", "Door Clear Zones" };
    DrawText(titles[(int)_layer],1205,172,24,GOLD);
    DrawText("Mouse wheel scrolls. TAB hides this panel.",1205,202,15,Fade(WHITE,.5f));

    const RoomAssetSource* selectedSource=SelectedSource();
    DrawButton({1210,230,42,34},"<",false,selectedSource!=nullptr);
    DrawButton({1828,230,42,34},">",false,selectedSource!=nullptr);
    DrawRectangleRec({1260,230,560,34},Color{28,32,39,255});
    DrawRectangleLinesEx({1260,230,560,34},1.f,Fade(WHITE,.22f));
    const std::string sourceName=selectedSource!=nullptr?selectedSource->stem:"No source sheets found";
    DrawText(sourceName.c_str(),1272,238,17,RAYWHITE);

    BeginScissorMode(1190,275,690,713);
    if ((_layer == Layer::Ground || _layer == Layer::Visual) && selectedSource!=nullptr)
    {
        Texture2D texture=TextureFor(selectedSource->stem);
        const int count=selectedSource->tileColumns*selectedSource->tileRows;
        for(int i=0;i<count;++i)
        {
            Rectangle src{(float)((i%selectedSource->tileColumns)*16),
                          (float)((i/selectedSource->tileColumns)*16),16,16};
            Rectangle item{1210.f+(i%10)*64.f,285.f+(i/10)*64.f-_paletteScroll,58,58};
            const bool active=src.x==_selectedRawTile.x&&src.y==_selectedRawTile.y;
            DrawRectangleRec(item,active?Color{35,95,145,255}:Color{35,38,45,255});
            if(texture.id) DrawTexturePro(texture,src,{item.x+7,item.y+7,44,44},{},0,WHITE);
            DrawRectangleLinesEx(item,1.f,active?SKYBLUE:Fade(WHITE,.16f));
        }
    }
    else if (_layer == Layer::Props || _layer == Layer::Decor)
    {
        const TileDefSet& definitions=SelectedDefinitions();
        for (int i=0;i<AssetCountForLayer();++i)
        {
            int definitionIndex=0; RoomAssetKind kind=AssetKindAtPaletteIndex(i,definitionIndex);
            Rectangle item{1210.f+(i%5)*128.f,285.f+(i/5)*108.f-_paletteScroll,116,96};
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
        DrawText("Left-drag adds solid collision. Right-drag removes it.",1230,400,17,RAYWHITE);
        DrawText("Art and collision stay independent.",1230,430,17,Fade(WHITE,.65f));
    }
    else if (_layer==Layer::FallZones)
    {
        DrawRectangle(1230,305,70,70,Color{210,35,35,80});
        DrawText("Paint cells that damage and return the player to the entry door",1230,400,16,RAYWHITE);
    }
    else if (_layer==Layer::DoorZones)
    {
        static const char* sides[]={"Top","Bottom","Left","Right"};
        for(int i=0;i<4;++i) DrawButton({1210.f+i*90.f,285,82,34},sides[i],i==_selectedDoorZone);
        const RoomDoorZone& zone=_room.doorZones[_selectedDoorZone];
        const float values[]={zone.tiles.x,zone.tiles.y,zone.tiles.width,zone.tiles.height};
        static const char* labels[]={"X","Y","W","H"};
        for(int i=0;i<4;++i)
        {
            float x=1210.f+i*155.f;
            DrawButton({x,350,34,34},"-");
            DrawRectangleRec({x+38,350,66,34},Color{28,32,39,255});
            DrawText(TextFormat("%s %.2f",labels[i],values[i]),(int)x+43,359,14,RAYWHITE);
            DrawButton({x+108,350,34,34},"+");
        }
        DrawText("When this door opens, overlapping visual tiles disappear.",1210,415,16,RAYWHITE);
        DrawText("Ground tiles always remain. The gold box is the clear zone.",1210,445,16,Fade(WHITE,.7f));
    }
    EndScissorMode();
}

void RoomEditor::DrawLibrary() const
{
    DrawRectangle(0,0,kVirtualWidth,kVirtualHeight,Color{10,12,16,255});
    DrawText("ROOM LIBRARY",240,80,38,GOLD);
    DrawText((_room.tilesetStem+" rooms - click a row to edit").c_str(),240,125,20,Fade(WHITE,.65f));
    DrawButton({1500,82,120,38},"Close");
    BeginScissorMode(220,170,1400,780);
    const auto& rooms=_library.Rooms();
    for(int i=0;i<(int)rooms.size();++i)
    {
        float y=180.f+i*62.f-_libraryScroll;
        DrawRectangleRec({240,y,1360,52},Color{30,34,41,255});
        DrawRectangleLinesEx({240,y,1360,52},1.f,Fade(WHITE,.2f));
        DrawText(rooms[(std::size_t)i].name.c_str(),260,(int)y+14,20,RAYWHITE);
        DrawText(RoomTypeLabel(rooms[(std::size_t)i].roomType),700,(int)y+15,18,SKYBLUE);
        DrawText(TextFormat("Doors %s%s%s%s",rooms[(std::size_t)i].hasNorth?"N":"",rooms[(std::size_t)i].hasSouth?"S":"",rooms[(std::size_t)i].hasWest?"W":"",rooms[(std::size_t)i].hasEast?"E":""),900,(int)y+15,18,Fade(WHITE,.7f));
        DrawButton({1535,y+7,50,38},"X");
    }
    EndScissorMode();
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
