#pragma once

#include "raylib.h"

#include <string>
#include <vector>

// Village Asset Adjuster - dev tool opened from the main menu with V.
//
// Edits metadata for finished PNG village assets in VillageAssets/. The PNG is
// the visual source; this tool adds runtime data: box colliders, NPC anchors,
// and respawn/interact points. Metadata saves beside the PNG as .vasset.
class MapEditor
{
public:
    void Init();
    void Update();
    void Draw() const;
    void Unload();
    bool WantsToExit() const { return _wantsToExit; }

private:
    enum class MarkerKind
    {
        Zeph = 0,
        Poe,
        Respawn,
        Count
    };

    struct ColliderBox
    {
        Rectangle rect{}; // image-local pixels
    };

    struct Marker
    {
        bool has = false;
        Vector2 pos{}; // image-local pixels; may be outside the PNG bounds
    };

    struct Asset
    {
        std::string name;
        std::string pngFile;
        std::string pngPath;
        std::string metaPath;
        Texture2D texture{};
        std::vector<ColliderBox> colliders;
        Marker markers[(int)MarkerKind::Count];
        bool dirty = false;

        // Buying/service metadata — saved into .vasset, read by VillageAssetData.
        int  cost        = 0;      // gold price in the build menu (0 = free)
        int  categoryIdx = 0;      // index into kCategoryNames (Building/Decor/...)
        int  serviceIdx  = 0;      // index into kServiceNames (None/Shop/Relic/...)
        bool required    = false;  // cannot be skipped in the real village
        bool unique      = false;  // only one allowed
        bool removable   = true;   // may be removed after placement

        bool  animEnabled = false; // image is a columns x rows animated spritesheet
        int   animCols = 1;
        int   animRows = 1;
        int   animFrames = 1;
        float animFps = 6.f;
    };

    void LoadAssets();
    void SelectAsset(int index);
    void LoadMetadata(Asset& asset);
    void SaveActiveMetadata();
    void FitActiveAssetToCanvas(Rectangle canvas);

    void UpdatePanel(Rectangle panel);
    void UpdateCanvas(Rectangle canvas);
    void UpdateSelectedColliderKeys();
    void UpdateSelectedMarkerKeys();
    void UpdateAssetMetadataKeys();   // cost/category/service/flags + collider quick-modes
    void DrawPanel(Rectangle panel) const;
    void DrawCanvas(Rectangle canvas) const;
    void DrawHelp() const;
    void DrawStatusBar() const;

    Vector2 ImageToScreen(Vector2 imagePos) const;
    Vector2 ScreenToImage(Vector2 screenPos) const;
    Rectangle ImageRectToScreen(Rectangle imageRect) const;
    Rectangle NormalizeImageRect(Rectangle rect) const;
    Rectangle ActiveImageBoundsScreen() const;
    int AssetFrameWidth(const Asset& asset) const;
    int AssetFrameHeight(const Asset& asset) const;
    Rectangle AssetFrameSourceRect(const Asset& asset) const;
    int ColliderAt(Vector2 imagePos) const;
    int MarkerAt(Vector2 imagePos) const;
    enum class ColliderHandle { None, TopLeft, TopRight, BottomLeft, BottomRight };
    ColliderHandle ColliderHandleAt(const ColliderBox& box, Vector2 screenPos) const;
    void AddCollider(Rectangle imageRect);
    void SetMarker(MarkerKind kind, Vector2 imagePos);

    const char* MarkerName(MarkerKind kind) const;
    Color MarkerColor(MarkerKind kind) const;
    std::string MarkerOffsetText(const Asset& asset, Vector2 markerPos) const;
    Asset* ActiveAsset();
    const Asset* ActiveAsset() const;

    std::string _assetFolder;
    std::vector<Asset> _assets;
    int _activeAsset = -1;
    int _selectedCollider = -1;
    int _selectedMarker = -1;
    float _zoom = 1.f;
    Vector2 _viewOffset{}; // screen offset from canvas center
    bool _panning = false;
    Vector2 _panStartMouse{};
    Vector2 _panStartOffset{};

    enum class DragMode { None, MoveCollider, ResizeCollider, DrawCollider, MoveMarker };
    DragMode _dragMode = DragMode::None;
    ColliderHandle _dragHandle = ColliderHandle::None;
    int _dragCollider = -1;
    int _dragMarker = -1;
    Vector2 _dragStartMouseImage{};
    Rectangle _dragStartRect{};
    Rectangle _drawPreview{};

    float _assetListScroll = 0.f;
    bool _helpOpen = false;
    bool _wantsToExit = false;
    std::string _status;
    float _statusTimer = 0.f;
};
