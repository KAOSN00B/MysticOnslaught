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

    // Paint tool + authoring operations (all headless-testable — no rendering).
    enum class PaintTool { Brush, Rectangle, Bucket };
    void SelectLayer(Layer layer) { _layer = layer; }
    Layer ActiveLayer() const { return _layer; }
    void SetPaintTool(PaintTool tool) { _paintTool = tool; }
    PaintTool ActivePaintTool() const { return _paintTool; }
    const std::string& SelectedAssetId() const { return _selectedAssetId; }
    Rectangle SelectedRawTile() const { return _selectedRawTile; }
    // Fill/flood/clear operate on the active layer and record ONE undo entry.
    bool FillRect(int col0, int row0, int col1, int row1, bool add = true);
    bool FloodFillFrom(int col, int row, bool add = true);
    bool ClearActiveLayer();
    // Eyedropper: sample the tile/asset under a cell into the current selection.
    void PickAt(int col, int row);
    // A room forked from source: fresh id + "<name> copy" (does not save).
    static RoomBlueprint Duplicate(const RoomBlueprint& source);

private:
    static std::string MakeRoomId(const std::string& tilesetStem);
    bool IsCellValid(int col, int row) const;
    bool PlacementFits(const RoomAssetPlacement& placement) const;
    const RoomAssetSource* SelectedSource() const;
    bool PaintCell(int col, int row, bool add);   // one cell of the active paint layer
    bool EraseVisual(int col, int row, bool ground);
    void SelectSourceByStem(const std::string& stem);
    static bool LayerOwnsKind(Layer layer, RoomAssetKind kind);
    const TileDefSet& SelectedDefinitions() const;
    const TileDefSet* DefinitionsFor(const std::string& stem) const;
    Texture2D TextureFor(const std::string& stem) const;
    void ReleaseCatalogTextures();
    void PushUndo();
    void NewRoom();
    void SaveRoom();
    void OpenRoom(const RoomBlueprint& room);
    void UpdateLibrary();
    void UpdateCanvas();
    void UpdateDoorZoneDrag(Vector2 mouse, Rectangle canvas);
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
    // Palette asset indices matching the current search (all when search empty).
    std::vector<int> MatchingAssetIndices() const;

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
    PaintTool _paintTool = PaintTool::Brush;
    bool _suppressUndo = false;      // true mid-stroke/fill so a multi-cell op = 1 undo
    bool _stroking = false;          // a left/right paint drag is in progress
    bool _strokeAdd = true;          // stroke paints (left) vs erases (right)
    bool _strokeChanged = false;     // any cell actually changed this stroke
    PaintTool _strokeTool = PaintTool::Brush; // tool locked in at stroke start
    int _dragStartCol = -1;          // rectangle-fill anchor cell
    int _dragStartRow = -1;
    int _zoneDragMode = 0;           // 0 none, 1 move, 2 resize (door-zone canvas drag)
    Vector2 _zoneDragGrab{};         // grab offset within the zone, tile space
    std::string _paletteSearch;      // props/decor palette filter
    bool _editingSearch = false;
    bool _paletteVisible = true;
    bool _showLibrary = false;
    std::string _pendingDeleteId; // room armed for delete; second click confirms
    bool _editingName = false;
    bool _savedOnce = false;
    bool _wantsBack = false;
    bool _playtestRequested = false;
    float _paletteScroll = 0.f;
    float _libraryScroll = 0.f;
    mutable std::string _status;
    mutable float _statusTimer = 0.f;
};
