#pragma once

#include "RoomLibrary.h"
#include "RoomAssetCatalog.h"
#include "TileDefs.h"

#include <filesystem>
#include <cstdint>
#include <string>
#include <vector>

class RoomEditor
{
public:
    enum class Layer { Ground, Visual, Door, Collision, Props, Decor, FallZones, DoorZones, ChestSpawn };

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
    void SetStatusMessage(const std::string& message, float seconds = 4.f)
    { _status = message; _statusTimer = seconds; }

    const RoomBlueprint& Blueprint() const { return _room; }
    RoomBlueprint& Blueprint() { return _room; }
    const TileDefSet& Definitions() const { return _definitions; }

    bool SetTerrain(int col, int row, TileType tile);
    bool SetVisual(int col, int row, bool ground, const std::string& source,
                   Rectangle sourceRect, Vector2 anchorOffset = {});
    bool SetDoorVisual(int col, int row, const std::string& source,
                       Rectangle sourceRect, Vector2 anchorOffset = {});
    bool SetSolid(int col, int row, bool enabled);
    bool SetFall(int col, int row, bool enabled);
    bool SetFallSurface(FallSurface surface);
    bool TreasureChestSpawnFits(int col, int row) const;
    bool SetTreasureChestSpawn(int col, int row);
    bool ClearTreasureChestSpawn();
    bool PlaceAsset(const RoomAssetPlacement& placement);
    bool RemoveAssetAt(int col, int row, RoomAssetKind kind);
    bool CreateRoomForDoorMask(unsigned char mask);
    void SetDoors(bool north, bool south, bool east, bool west);
    bool SetWallDepth(RoomWallSide side, float depth);
    bool Undo();
    bool Redo();
    bool CanUndo() const { return !_undo.empty(); }
    bool CanRedo() const { return !_redo.empty(); }

    // Paint tool + authoring operations (all headless-testable — no rendering).
    enum class PaintTool { Brush, Rectangle, Bucket, Eraser };
    struct GroundBrushTile
    {
        std::string sourceTileset;
        Rectangle source{};
        int weight = 1;
        Vector2 anchorOffset{};
    };
    void SelectLayer(Layer layer) { _layer = layer; }
    Layer ActiveLayer() const { return _layer; }
    void SetPaintTool(PaintTool tool) { _paintTool = tool; }
    PaintTool ActivePaintTool() const { return _paintTool; }
    const std::string& SelectedAssetId() const { return _selectedAssetId; }
    Rectangle SelectedRawTile() const { return _selectedRawTile; }
    Vector2 SelectedTileAnchorOffset() const { return _selectedTileAnchorOffset; }
    bool AdjustSelectedTileOverhang(RoomWallSide side, int delta);
    // Fill/flood/clear operate on the active layer and record ONE undo entry.
    bool FillRect(int col0, int row0, int col1, int row1, bool add = true);
    bool FloodFillFrom(int col, int row, bool add = true);
    bool EraseAt(int col, int row);
    bool ClearActiveLayer();
    // Optional weighted brush for Ground tiles. The chosen tile is written into
    // the room like any normal visual tile; randomness never reaches runtime.
    bool AddGroundBrushTile(const std::string& sourceTileset, Rectangle source,
                            int weight = 1, Vector2 anchorOffset = {});
    bool RemoveGroundBrushTile(std::size_t index);
    bool SetGroundBrushWeight(std::size_t index, int weight);
    void ClearGroundBrush();
    const std::vector<GroundBrushTile>& GroundBrushTiles() const { return _groundBrushTiles; }
    void SetRandomGroundBrushEnabled(bool enabled) { _randomGroundBrushEnabled = enabled; }
    bool RandomGroundBrushEnabled() const { return _randomGroundBrushEnabled; }
    std::size_t ChooseGroundBrushIndex(std::uint32_t sample) const;
    bool PaintActiveCell(int col, int row, bool add = true) { return PaintCell(col, row, add); }
    bool SaveGroundBrushPreset(const std::string& name, std::string& error);
    bool LoadGroundBrushPreset(const std::string& name, std::string& error);
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
    bool AddSelectedGroundBrushTile();
    const GroundBrushTile* GroundBrushTileForCell(int col, int row) const;
    std::filesystem::path GroundBrushPresetFolder() const;
    bool LoadGroundBrushPresetPath(const std::filesystem::path& path, std::string& error);
    void RefreshGroundBrushPresets();
    bool EraseVisual(int col, int row, bool ground, bool door = false);
    bool DoorVisualFitsEnabledZone(int col, int row, Rectangle sourceRect,
                                   Vector2 anchorOffset = {}) const;
    // Ground/Visual layers can also paint Decor/AnimDecor assets tagged to that
    // band (animated water/lava as ground). These helpers back that "decor mode".
    bool DecorModeOnTileLayer() const;             // Ground/Visual + decor-mode on
    bool ShowingAssetPalette() const;              // props/decor cards are visible
    bool PaletteShowsProps() const;                // cards are props (vs decors)
    RoomDrawBand CurrentTileBand() const;          // band for the active tile layer
    bool PaintBandedDecor(int col, int row);       // place selected decor at a cell
    bool EraseBandedDecorAt(int col, int row);     // remove decor of the active band
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
    // Collision layer, Rectangle tool: draw/move/resize free-size collider rects.
    void UpdateLayerRects(Vector2 mouse, Rectangle canvas);
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
    Vector2 _selectedTileAnchorOffset{};
    std::vector<GroundBrushTile> _groundBrushTiles;
    std::vector<std::filesystem::path> _groundBrushPresetPaths;
    std::string _groundBrushPresetName = "Ground Mix";
    int _groundBrushPresetIndex = -1;
    bool _randomGroundBrushEnabled = false;
    bool _editingGroundBrushName = false;
    int _selectedDoorZone = 0;
    int _selectedCollider = -1;      // index in active collision/fall rect list
    int _colliderDragMode = 0;       // 0 none, 1 move, 2 resize, 3 creating
    Vector2 _colliderGrab{};         // grab offset within a collider, tile space
    Vector2 _colliderAnchor{};       // creation anchor corner, tile space
    PaintTool _paintTool = PaintTool::Brush;
    bool _tileLayerDecorMode = false; // Ground/Visual: paint banded Decor assets
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
    bool _coverageExpanded = false;
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
