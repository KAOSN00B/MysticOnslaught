#pragma once

#include "RoomLibrary.h"
#include "RoomAssetCatalog.h"
#include "TileDefs.h"

#include <filesystem>
#include <vector>

class RoomEditor
{
public:
    enum class Layer { Ground, Visual, Collision, Props, Decor, FallZones, DoorZones };

    void Bind(const std::string& tilesetStem, Biome biome,
              const TileDefSet& definitions, Texture2D sheet, Texture2D groundSheet,
              const std::filesystem::path& roomRoot,
              const std::filesystem::path& tilesetRoot);
    void BindForTesting(const std::string& tilesetStem, Biome biome,
                        const TileDefSet& definitions,
                        const std::filesystem::path& roomRoot);

    void Update();
    void Draw() const;
    void Unload();
    bool WantsBack() const { return _wantsBack; }
    void ClearWantsBack() { _wantsBack = false; }
    bool ConsumePlaytestRequest();

    const RoomBlueprint& Blueprint() const { return _room; }
    RoomBlueprint& Blueprint() { return _room; }
    const TileDefSet& Definitions() const { return _definitions; }

    bool SetTerrain(int col, int row, TileType tile);
    bool SetVisual(int col, int row, bool ground, const std::string& source,
                   Rectangle sourceRect);
    bool SetSolid(int col, int row, bool enabled);
    bool SetFall(int col, int row, bool enabled);
    bool PlaceAsset(const RoomAssetPlacement& placement);
    bool RemoveAssetAt(int col, int row, RoomAssetKind kind);
    void SetDoors(bool north, bool south, bool east, bool west);
    bool SetWallDepth(RoomWallSide side, float depth);
    bool Undo();
    bool Redo();
    bool CanUndo() const { return !_undo.empty(); }
    bool CanRedo() const { return !_redo.empty(); }

private:
    static std::string MakeRoomId(const std::string& tilesetStem);
    bool IsCellValid(int col, int row) const;
    bool IsProtectedDoorCell(int col, int row) const;
    bool PlacementFits(const RoomAssetPlacement& placement) const;
    const RoomAssetSource* SelectedSource() const;
    const TileDefSet& SelectedDefinitions() const;
    const TileDefSet* DefinitionsFor(const std::string& stem) const;
    Texture2D TextureFor(const std::string& stem) const;
    void ReleaseCatalogTextures();
    void PushUndo();
    void UpdateDoorTiles();
    void NewRoom();
    void SaveRoom();
    void OpenRoom(const RoomBlueprint& room);
    void UpdateLibrary();
    void UpdateCanvas();
    void DrawToolbar() const;
    void DrawCanvas() const;
    void DrawPlacementPreview() const;
    void DrawWallColliderOverlay() const;
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
    RoomAssetCatalog _catalog;
    struct SourceTexture { std::string stem; Texture2D texture{}; };
    std::vector<SourceTexture> _sourceTextures;
    RoomLibrary _library;
    std::vector<RoomBlueprint> _undo;
    std::vector<RoomBlueprint> _redo;
    Texture2D _sheet{};
    Texture2D _groundSheet{};
    std::filesystem::path _roomRoot;
    Layer _layer = Layer::Ground;
    TileType _selectedTile = TileType::Floor;
    RoomAssetKind _selectedAssetKind = RoomAssetKind::Prop;
    std::string _selectedAssetId;
    int _selectedSource = 0;
    Rectangle _selectedRawTile{ 0.f, 0.f, 16.f, 16.f };
    int _selectedDoorZone = 0;
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
