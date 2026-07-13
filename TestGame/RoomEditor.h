#pragma once

#include "RoomLibrary.h"
#include "TileDefs.h"

#include <filesystem>
#include <vector>

class RoomEditor
{
public:
    enum class Layer { Terrain, Props, Decor, FallZones };

    void Bind(const std::string& tilesetStem, Biome biome,
              const TileDefSet& definitions, Texture2D sheet, Texture2D groundSheet,
              const std::filesystem::path& roomRoot);
    void BindForTesting(const std::string& tilesetStem, Biome biome,
                        const TileDefSet& definitions,
                        const std::filesystem::path& roomRoot);

    void Update();
    void Draw() const;
    bool WantsBack() const { return _wantsBack; }
    void ClearWantsBack() { _wantsBack = false; }
    bool ConsumePlaytestRequest();

    const RoomBlueprint& Blueprint() const { return _room; }
    RoomBlueprint& Blueprint() { return _room; }
    const TileDefSet& Definitions() const { return _definitions; }

    bool SetTerrain(int col, int row, TileType tile);
    bool SetFall(int col, int row, bool enabled);
    bool PlaceAsset(const RoomAssetPlacement& placement);
    bool RemoveAssetAt(int col, int row, RoomAssetKind kind);
    void SetDoors(bool north, bool south, bool east, bool west);
    bool Undo();
    bool Redo();

private:
    static std::string MakeRoomId(const std::string& tilesetStem);
    bool IsCellValid(int col, int row) const;
    bool IsProtectedDoorCell(int col, int row) const;
    bool PlacementFits(const RoomAssetPlacement& placement) const;
    void PushUndo();
    void UpdateDoorTiles();
    void NewRoom();
    void SaveRoom();
    void OpenRoom(const RoomBlueprint& room);
    void UpdateLibrary();
    void UpdateCanvas();
    void DrawToolbar() const;
    void DrawCanvas() const;
    void DrawPalette() const;
    void DrawLibrary() const;
    Rectangle CanvasRect() const;
    bool ScreenToCell(Vector2 mouse, int& col, int& row) const;
    Rectangle AssetSource(RoomAssetKind kind, int index) const;
    const char* AssetName(RoomAssetKind kind, int index) const;
    int AssetCountForLayer() const;
    RoomAssetKind AssetKindAtPaletteIndex(int index, int& definitionIndex) const;

    RoomBlueprint _room = RoomBlueprint::CreateDefault();
    TileDefSet _definitions{};
    RoomLibrary _library;
    std::vector<RoomBlueprint> _undo;
    std::vector<RoomBlueprint> _redo;
    Texture2D _sheet{};
    Texture2D _groundSheet{};
    std::filesystem::path _roomRoot;
    Layer _layer = Layer::Terrain;
    TileType _selectedTile = TileType::Floor;
    RoomAssetKind _selectedAssetKind = RoomAssetKind::Prop;
    std::string _selectedAssetId;
    bool _paletteVisible = true;
    bool _showLibrary = false;
    bool _editingName = false;
    bool _savedOnce = false;
    bool _wantsBack = false;
    bool _playtestRequested = false;
    float _paletteScroll = 0.f;
    float _libraryScroll = 0.f;
    mutable std::string _status;
    mutable float _statusTimer = 0.f;
};
