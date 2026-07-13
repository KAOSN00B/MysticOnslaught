#include "RoomEditor.h"
#include "VirtualCanvas.h"

#include <algorithm>
#include <chrono>
#include <cmath>
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
    const int index = _definitions.FindAssetIndex(placement.kind, placement.assetId);
    if (index < 0 || !IsCellValid(placement.col, placement.row)) return false;

    Rectangle source{};
    switch (placement.kind)
    {
    case RoomAssetKind::Prop: source = _definitions.props[(std::size_t)index].src; break;
    case RoomAssetKind::AnimProp:
        if (_definitions.animProps[(std::size_t)index].frames.empty()) return false;
        source = _definitions.animProps[(std::size_t)index].frames[0]; break;
    case RoomAssetKind::Decor: source = _definitions.decors[(std::size_t)index].src; break;
    case RoomAssetKind::AnimDecor:
        if (_definitions.animDecors[(std::size_t)index].frames.empty()) return false;
        source = _definitions.animDecors[(std::size_t)index].frames[0]; break;
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
    UpdateDoorTiles();
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

    void DrawButton(Rectangle rect, const char* label, bool active = false)
    {
        Vector2 mouse = GetVirtualMousePos();
        bool hovered = CheckCollisionPointRec(mouse, rect);
        DrawRectangleRec(rect, active ? Color{ 38, 105, 166, 255 }
                                      : hovered ? Color{ 58, 62, 72, 255 }
                                                : Color{ 37, 40, 48, 255 });
        DrawRectangleLinesEx(rect, 1.f, active ? SKYBLUE : Fade(WHITE, 0.25f));
        int fontSize = 17;
        int width = MeasureText(label, fontSize);
        DrawText(label, (int)(rect.x + rect.width * 0.5f - width * 0.5f),
                 (int)(rect.y + rect.height * 0.5f - fontSize * 0.5f), fontSize, RAYWHITE);
    }
}

void RoomEditor::Bind(const std::string& tilesetStem, Biome biome,
                      const TileDefSet& definitions, Texture2D sheet, Texture2D groundSheet,
                      const std::filesystem::path& roomRoot)
{
    BindForTesting(tilesetStem, biome, definitions, roomRoot);
    _sheet = sheet;
    _groundSheet = groundSheet;
    _roomRoot = roomRoot;
    _wantsBack = false;
    _playtestRequested = false;
    _paletteVisible = true;
    _showLibrary = false;
    _status = "Paint terrain, then place props and decor";
    _statusTimer = 4.f;
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
    if (index < 0) return {};
    switch (kind)
    {
    case RoomAssetKind::Prop:
        if (index < (int)_definitions.props.size()) return _definitions.props[(std::size_t)index].src;
        break;
    case RoomAssetKind::AnimProp:
        if (index < (int)_definitions.animProps.size())
        {
            const auto& def = _definitions.animProps[(std::size_t)index];
            if (!def.frames.empty())
                return def.frames[(std::size_t)AnimationFrame((int)def.frames.size(), def.fps, def.playback)];
        }
        break;
    case RoomAssetKind::Decor:
        if (index < (int)_definitions.decors.size()) return _definitions.decors[(std::size_t)index].src;
        break;
    case RoomAssetKind::AnimDecor:
        if (index < (int)_definitions.animDecors.size())
        {
            const auto& def = _definitions.animDecors[(std::size_t)index];
            if (!def.frames.empty())
                return def.frames[(std::size_t)AnimationFrame((int)def.frames.size(), def.fps, def.playback)];
        }
        break;
    }
    return {};
}

const char* RoomEditor::AssetName(RoomAssetKind kind, int index) const
{
    switch (kind)
    {
    case RoomAssetKind::Prop: return _definitions.props[(std::size_t)index].name.c_str();
    case RoomAssetKind::AnimProp: return _definitions.animProps[(std::size_t)index].name.c_str();
    case RoomAssetKind::Decor: return _definitions.decors[(std::size_t)index].name.c_str();
    case RoomAssetKind::AnimDecor: return _definitions.animDecors[(std::size_t)index].name.c_str();
    }
    return "Asset";
}

int RoomEditor::AssetCountForLayer() const
{
    if (_layer == Layer::Props)
        return (int)_definitions.props.size() + (int)_definitions.animProps.size();
    if (_layer == Layer::Decor)
        return (int)_definitions.decors.size() + (int)_definitions.animDecors.size();
    return 0;
}

RoomAssetKind RoomEditor::AssetKindAtPaletteIndex(int index, int& definitionIndex) const
{
    if (_layer == Layer::Props)
    {
        if (index < (int)_definitions.props.size())
        {
            definitionIndex = index;
            return RoomAssetKind::Prop;
        }
        definitionIndex = index - (int)_definitions.props.size();
        return RoomAssetKind::AnimProp;
    }
    if (index < (int)_definitions.decors.size())
    {
        definitionIndex = index;
        return RoomAssetKind::Decor;
    }
    definitionIndex = index - (int)_definitions.decors.size();
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
        if (_layer == Layer::Terrain) SetTerrain(col, row, _selectedTile);
        else if (_layer == Layer::FallZones) SetFall(col, row, true);
        else if (!_selectedAssetId.empty())
            PlaceAsset({ _selectedAssetKind, _selectedAssetId, col, row });
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
    {
        if (_layer == Layer::Terrain) SetTerrain(col, row, TileType::Floor);
        else if (_layer == Layer::FallZones) SetFall(col, row, false);
        else RemoveAssetAt(col, row, _selectedAssetKind);
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
        {{420, 18, 105, 38}, 3}, {{535, 18, 125, 38}, 4}
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
    }

    const Rectangle layerButtons[] = { {30,112,120,34}, {158,112,120,34}, {286,112,120,34}, {414,112,140,34} };
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        for (int i = 0; i < 4; ++i)
            if (CheckCollisionPointRec(mouse, layerButtons[i])) _layer = (Layer)i;

    if (_paletteVisible && mouse.x >= 1180.f)
    {
        _paletteScroll = std::max(0.f, _paletteScroll - GetMouseWheelMove() * 90.f);
        if (_layer == Layer::Terrain && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            int visible = 0;
            for (int type = 0; type < (int)TileType::Count; ++type)
            {
                if (!_definitions.assigned[type]) continue;
                Rectangle item{ 1210.f + (visible % 5) * 128.f,
                                235.f + (visible / 5) * 108.f - _paletteScroll, 116.f, 96.f };
                if (CheckCollisionPointRec(mouse, item)) _selectedTile = (TileType)type;
                ++visible;
            }
        }
        else if ((_layer == Layer::Props || _layer == Layer::Decor) &&
                 IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            for (int i = 0; i < AssetCountForLayer(); ++i)
            {
                Rectangle item{ 1210.f + (i % 5) * 128.f,
                                235.f + (i / 5) * 108.f - _paletteScroll, 116.f, 96.f };
                if (!CheckCollisionPointRec(mouse, item)) continue;
                int definitionIndex = 0;
                _selectedAssetKind = AssetKindAtPaletteIndex(i, definitionIndex);
                switch (_selectedAssetKind)
                {
                case RoomAssetKind::Prop: _selectedAssetId = _definitions.props[(std::size_t)definitionIndex].id; break;
                case RoomAssetKind::AnimProp: _selectedAssetId = _definitions.animProps[(std::size_t)definitionIndex].id; break;
                case RoomAssetKind::Decor: _selectedAssetId = _definitions.decors[(std::size_t)definitionIndex].id; break;
                case RoomAssetKind::AnimDecor: _selectedAssetId = _definitions.animDecors[(std::size_t)definitionIndex].id; break;
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
    const char* names[] = { "Terrain", "Props", "Decor", "Fall Zones" };
    for (int i = 0; i < 4; ++i)
        DrawButton({30.f+i*128.f,112, i==3?140.f:120.f,34}, names[i], (int)_layer == i);
    DrawText("TAB: hide/show palette", 650, 120, 16, Fade(WHITE,.55f));
    DrawText("Ctrl+Z / Ctrl+Y: undo/redo", 870, 120, 16, Fade(WHITE,.55f));
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

    for (const RoomAssetPlacement& placement : _room.placements)
    {
        int definitionIndex = _definitions.FindAssetIndex(placement.kind, placement.assetId);
        if (definitionIndex < 0) continue;
        Rectangle source = AssetSource(placement.kind, definitionIndex);
        Rectangle destination{ canvas.x + placement.col*kCell, canvas.y + placement.row*kCell,
                               source.width * (kCell/16.f), source.height * (kCell/16.f) };
        if (_sheet.id != 0) DrawTexturePro(_sheet, source, destination, {}, 0.f, WHITE);
        if (placement.kind == RoomAssetKind::Prop || placement.kind == RoomAssetKind::AnimProp)
        {
            Rectangle collision = placement.kind == RoomAssetKind::Prop
                ? _definitions.props[(std::size_t)definitionIndex].collision
                : _definitions.animProps[(std::size_t)definitionIndex].collision;
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
    for (int col = 0; col <= RoomLayout::kCols; ++col)
        DrawLine((int)(canvas.x+col*kCell),(int)canvas.y,(int)(canvas.x+col*kCell),(int)(canvas.y+canvas.height),Fade(WHITE,.10f));
    for (int row = 0; row <= RoomLayout::kRows; ++row)
        DrawLine((int)canvas.x,(int)(canvas.y+row*kCell),(int)(canvas.x+canvas.width),(int)(canvas.y+row*kCell),Fade(WHITE,.10f));
    DrawRectangleLinesEx(canvas, 2.f, Fade(WHITE,.5f));
}

void RoomEditor::DrawPalette() const
{
    if (!_paletteVisible) return;
    Rectangle panel{1180,155,710,850};
    DrawRectangleRec(panel, Color{20,23,29,250});
    DrawRectangleLinesEx(panel,1.f,Fade(WHITE,.25f));
    DrawText(_layer == Layer::Terrain ? "Terrain Tiles" : _layer == Layer::Props ? "Props (water/lava included)" :
             _layer == Layer::Decor ? "Decor" : "Fall Zone Brush", 1205,172,24,GOLD);
    DrawText("Mouse wheel scrolls this palette",1205,202,15,Fade(WHITE,.5f));
    BeginScissorMode(1190,228,690,760);
    if (_layer == Layer::Terrain)
    {
        int visible = 0;
        for (int type = 0; type < (int)TileType::Count; ++type)
        {
            if (!_definitions.assigned[type]) continue;
            Rectangle item{1210.f+(visible%5)*128.f,235.f+(visible/5)*108.f-_paletteScroll,116,96};
            DrawRectangleRec(item, _selectedTile == (TileType)type ? Color{35,95,145,255}:Color{35,38,45,255});
            Texture2D texture = _definitions.fromGround[type] ? _groundSheet : _sheet;
            if (texture.id) DrawTexturePro(texture,_definitions.rects[type],{item.x+36,item.y+8,44,44},{},0,WHITE);
            DrawText(TileLabel(type),(int)item.x+5,(int)item.y+61,12,RAYWHITE);
            ++visible;
        }
    }
    else if (_layer == Layer::Props || _layer == Layer::Decor)
    {
        for (int i=0;i<AssetCountForLayer();++i)
        {
            int definitionIndex=0; RoomAssetKind kind=AssetKindAtPaletteIndex(i,definitionIndex);
            Rectangle item{1210.f+(i%5)*128.f,235.f+(i/5)*108.f-_paletteScroll,116,96};
            std::string id;
            if (kind==RoomAssetKind::Prop) id=_definitions.props[(std::size_t)definitionIndex].id;
            else if (kind==RoomAssetKind::AnimProp) id=_definitions.animProps[(std::size_t)definitionIndex].id;
            else if (kind==RoomAssetKind::Decor) id=_definitions.decors[(std::size_t)definitionIndex].id;
            else id=_definitions.animDecors[(std::size_t)definitionIndex].id;
            DrawRectangleRec(item,_selectedAssetId==id?Color{35,95,145,255}:Color{35,38,45,255});
            Rectangle source=AssetSource(kind,definitionIndex);
            if (_sheet.id) DrawTexturePro(_sheet,source,{item.x+32,item.y+5,52,52},{},0,WHITE);
            DrawText(AssetName(kind,definitionIndex),(int)item.x+5,(int)item.y+62,12,RAYWHITE);
        }
    }
    else
    {
        DrawRectangle(1230,260,70,70,Color{210,35,35,80});
        DrawText("Paint cells that damage and return the player to the entry door",1230,350,16,RAYWHITE);
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
