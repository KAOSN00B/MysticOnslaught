#pragma warning(disable: 4996)
#include "MapEditor.h"
#include "AssetPaths.h"
#include "VirtualCanvas.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
    constexpr float kPanelW = 390.f;
    constexpr float kStatusH = 44.f;
    constexpr float kRowH = 38.f;
    constexpr float kHandleSize = 12.f;

    std::string StemFromFileName(const char* fileName)
    {
        std::string stem = fileName ? fileName : "asset";
        size_t dot = stem.find_last_of('.');
        if (dot != std::string::npos) stem = stem.substr(0, dot);
        return stem;
    }

    std::string JoinPath(const std::string& folder, const std::string& file)
    {
        if (folder.empty()) return file;
        char last = folder[folder.size() - 1];
        if (last == '/' || last == '\\') return folder + file;
        return folder + "/" + file;
    }

    float ClampFloat(float v, float lo, float hi)
    {
        return std::max(lo, std::min(v, hi));
    }
}

void MapEditor::Init()
{
    _wantsToExit = false;
    _helpOpen = false;
    _selectedCollider = -1;
    _selectedMarker = -1;
    _dragMode = DragMode::None;
    _dragCollider = -1;
    _dragMarker = -1;
    _assetListScroll = 0.f;
    _statusTimer = 0.f;
    LoadAssets();
    SelectAsset(_assets.empty() ? -1 : 0);
    _status = _assets.empty()
        ? "No PNGs found in VillageAssets"
        : "Village Asset Adjuster: add colliders, set Zeph/Poe/respawn markers, S save";
    _statusTimer = 5.f;
}

void MapEditor::Unload()
{
    for (Asset& asset : _assets)
    {
        if (asset.texture.id != 0) UnloadTexture(asset.texture);
        asset.texture = Texture2D{};
    }
    _assets.clear();
    _activeAsset = -1;
}

void MapEditor::LoadAssets()
{
    Unload();
    _assetFolder = AssetFolderPath("VillageAssets");
    if (!DirectoryExists(_assetFolder.c_str()))
        _assetFolder = "VillageAssets";

    FilePathList files = LoadDirectoryFiles(_assetFolder.c_str());
    for (unsigned int i = 0; i < files.count; ++i)
    {
        const char* fileName = GetFileName(files.paths[i]);
        if (!fileName || !IsFileExtension(fileName, ".png")) continue;

        Asset asset;
        asset.name = StemFromFileName(fileName);
        asset.pngFile = fileName;
        asset.pngPath = JoinPath(_assetFolder, asset.pngFile);
        asset.metaPath = JoinPath(_assetFolder, asset.name + ".vasset");
        asset.texture = LoadTexture(asset.pngPath.c_str());
        SetTextureFilter(asset.texture, TEXTURE_FILTER_POINT);
        LoadMetadata(asset);
        _assets.push_back(asset);
    }
    UnloadDirectoryFiles(files);

    std::sort(_assets.begin(), _assets.end(), [](const Asset& a, const Asset& b) {
        return a.name < b.name;
    });
}

void MapEditor::SelectAsset(int index)
{
    if (_assets.empty())
    {
        _activeAsset = -1;
        _selectedCollider = -1;
        _selectedMarker = -1;
        return;
    }

    _activeAsset = std::max(0, std::min(index, (int)_assets.size() - 1));
    _selectedCollider = -1;
    _selectedMarker = -1;
    _dragMarker = -1;
    _dragMode = DragMode::None;
    Rectangle canvas{ kPanelW, 0.f, (float)kVirtualWidth - kPanelW, (float)kVirtualHeight - kStatusH };
    FitActiveAssetToCanvas(canvas);
}

MapEditor::Asset* MapEditor::ActiveAsset()
{
    if (_activeAsset < 0 || _activeAsset >= (int)_assets.size()) return nullptr;
    return &_assets[_activeAsset];
}

const MapEditor::Asset* MapEditor::ActiveAsset() const
{
    if (_activeAsset < 0 || _activeAsset >= (int)_assets.size()) return nullptr;
    return &_assets[_activeAsset];
}

const char* MapEditor::MarkerName(MarkerKind kind) const
{
    switch (kind)
    {
    case MarkerKind::Zeph: return "Zeph";
    case MarkerKind::Poe: return "Poe";
    case MarkerKind::Respawn: return "Respawn";
    default: return "Marker";
    }
}

Color MapEditor::MarkerColor(MarkerKind kind) const
{
    switch (kind)
    {
    case MarkerKind::Zeph: return Color{ 90, 220, 255, 255 };
    case MarkerKind::Poe: return Color{ 205, 150, 255, 255 };
    case MarkerKind::Respawn: return Color{ 120, 255, 145, 255 };
    default: return GOLD;
    }
}

std::string MapEditor::MarkerOffsetText(const Asset& asset, Vector2 markerPos) const
{
    std::string text;
    auto append = [&text](const char* part)
    {
        if (!text.empty()) text += ", ";
        text += part;
    };

    if (markerPos.x < 0.f) append(TextFormat("%.0f px left", -markerPos.x));
    else if (markerPos.x > asset.texture.width) append(TextFormat("%.0f px right", markerPos.x - asset.texture.width));

    if (markerPos.y < 0.f) append(TextFormat("%.0f px above", -markerPos.y));
    else if (markerPos.y > asset.texture.height) append(TextFormat("%.0f px below", markerPos.y - asset.texture.height));

    return text.empty() ? std::string("inside PNG") : std::string("outside PNG: ") + text;
}

void MapEditor::LoadMetadata(Asset& asset)
{
    asset.colliders.clear();
    for (int i = 0; i < (int)MarkerKind::Count; ++i) asset.markers[i] = Marker{};

    FILE* file = fopen(asset.metaPath.c_str(), "r");
    if (!file) return;

    char line[512];
    while (fgets(line, sizeof(line), file))
    {
        float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
        if (sscanf(line, "collider %f %f %f %f", &x, &y, &w, &h) == 4)
        {
            asset.colliders.push_back(ColliderBox{ Rectangle{ x, y, w, h } });
            continue;
        }

        char markerName[64] = {};
        if (sscanf(line, "marker %63s %f %f", markerName, &x, &y) == 3)
        {
            MarkerKind kind = MarkerKind::Count;
            if (strcmp(markerName, "Zeph") == 0) kind = MarkerKind::Zeph;
            else if (strcmp(markerName, "Poe") == 0) kind = MarkerKind::Poe;
            else if (strcmp(markerName, "Respawn") == 0) kind = MarkerKind::Respawn;
            if (kind != MarkerKind::Count)
            {
                asset.markers[(int)kind].has = true;
                asset.markers[(int)kind].pos = Vector2{ x, y };
            }
        }
    }

    fclose(file);
    asset.dirty = false;
}

void MapEditor::SaveActiveMetadata()
{
    Asset* asset = ActiveAsset();
    if (!asset) return;

    FILE* file = fopen(asset->metaPath.c_str(), "w");
    if (!file)
    {
        _status = "Could not save " + asset->metaPath;
        _statusTimer = 4.f;
        return;
    }

    fprintf(file, "village_asset 1\n");
    fprintf(file, "image %s\n", asset->pngFile.c_str());
    fprintf(file, "size %d %d\n", asset->texture.width, asset->texture.height);
    for (const ColliderBox& box : asset->colliders)
        fprintf(file, "collider %.2f %.2f %.2f %.2f\n", box.rect.x, box.rect.y, box.rect.width, box.rect.height);

    for (int i = 0; i < (int)MarkerKind::Count; ++i)
    {
        const Marker& marker = asset->markers[i];
        if (!marker.has) continue;
        MarkerKind kind = (MarkerKind)i;
        fprintf(file, "marker %s %.2f %.2f\n", MarkerName(kind), marker.pos.x, marker.pos.y);
    }

    fclose(file);
    asset->dirty = false;
    _status = "Saved " + asset->name + ".vasset";
    _statusTimer = 3.f;
}

void MapEditor::FitActiveAssetToCanvas(Rectangle canvas)
{
    const Asset* asset = ActiveAsset();
    if (!asset || asset->texture.id == 0) return;
    float fitX = (canvas.width - 80.f) / std::max(1.f, (float)asset->texture.width);
    float fitY = (canvas.height - 80.f) / std::max(1.f, (float)asset->texture.height);
    _zoom = ClampFloat(std::min(fitX, fitY), 0.5f, 8.f);
    _viewOffset = Vector2{};
}

Vector2 MapEditor::ImageToScreen(Vector2 imagePos) const
{
    Rectangle canvas{ kPanelW, 0.f, (float)kVirtualWidth - kPanelW, (float)kVirtualHeight - kStatusH };
    const Asset* asset = ActiveAsset();
    if (!asset) return Vector2{};
    Vector2 origin{ canvas.x + canvas.width * 0.5f - asset->texture.width * _zoom * 0.5f + _viewOffset.x,
                    canvas.y + canvas.height * 0.5f - asset->texture.height * _zoom * 0.5f + _viewOffset.y };
    return Vector2{ origin.x + imagePos.x * _zoom, origin.y + imagePos.y * _zoom };
}

Vector2 MapEditor::ScreenToImage(Vector2 screenPos) const
{
    Rectangle canvas{ kPanelW, 0.f, (float)kVirtualWidth - kPanelW, (float)kVirtualHeight - kStatusH };
    const Asset* asset = ActiveAsset();
    if (!asset || _zoom <= 0.f) return Vector2{};
    Vector2 origin{ canvas.x + canvas.width * 0.5f - asset->texture.width * _zoom * 0.5f + _viewOffset.x,
                    canvas.y + canvas.height * 0.5f - asset->texture.height * _zoom * 0.5f + _viewOffset.y };
    return Vector2{ (screenPos.x - origin.x) / _zoom, (screenPos.y - origin.y) / _zoom };
}

Rectangle MapEditor::ImageRectToScreen(Rectangle imageRect) const
{
    Vector2 p = ImageToScreen(Vector2{ imageRect.x, imageRect.y });
    return Rectangle{ p.x, p.y, imageRect.width * _zoom, imageRect.height * _zoom };
}

Rectangle MapEditor::NormalizeImageRect(Rectangle rect) const
{
    if (rect.width < 0.f) { rect.x += rect.width; rect.width = -rect.width; }
    if (rect.height < 0.f) { rect.y += rect.height; rect.height = -rect.height; }
    const Asset* asset = ActiveAsset();
    if (!asset) return rect;
    rect.x = ClampFloat(rect.x, 0.f, (float)asset->texture.width);
    rect.y = ClampFloat(rect.y, 0.f, (float)asset->texture.height);
    rect.width = ClampFloat(rect.width, 1.f, (float)asset->texture.width - rect.x);
    rect.height = ClampFloat(rect.height, 1.f, (float)asset->texture.height - rect.y);
    return rect;
}

Rectangle MapEditor::ActiveImageBoundsScreen() const
{
    const Asset* asset = ActiveAsset();
    if (!asset) return Rectangle{};
    Vector2 topLeft = ImageToScreen(Vector2{ 0.f, 0.f });
    return Rectangle{ topLeft.x, topLeft.y, asset->texture.width * _zoom, asset->texture.height * _zoom };
}

int MapEditor::ColliderAt(Vector2 imagePos) const
{
    const Asset* asset = ActiveAsset();
    if (!asset) return -1;
    for (int i = (int)asset->colliders.size() - 1; i >= 0; --i)
    {
        if (CheckCollisionPointRec(imagePos, asset->colliders[i].rect)) return i;
    }
    return -1;
}

int MapEditor::MarkerAt(Vector2 imagePos) const
{
    const Asset* asset = ActiveAsset();
    if (!asset) return -1;

    float pickRadius = 14.f / std::max(0.25f, _zoom);
    float pickRadiusSq = pickRadius * pickRadius;
    for (int i = 0; i < (int)MarkerKind::Count; ++i)
    {
        const Marker& marker = asset->markers[i];
        if (!marker.has) continue;
        float dx = marker.pos.x - imagePos.x;
        float dy = marker.pos.y - imagePos.y;
        if (dx * dx + dy * dy <= pickRadiusSq) return i;
    }
    return -1;
}

bool MapEditor::HandleAt(const ColliderBox& box, Vector2 screenPos) const
{
    Rectangle screen = ImageRectToScreen(box.rect);
    Rectangle handle{ screen.x + screen.width - kHandleSize * 0.5f,
                      screen.y + screen.height - kHandleSize * 0.5f,
                      kHandleSize, kHandleSize };
    return CheckCollisionPointRec(screenPos, handle);
}

void MapEditor::AddCollider(Rectangle imageRect)
{
    Asset* asset = ActiveAsset();
    if (!asset) return;
    imageRect = NormalizeImageRect(imageRect);
    asset->colliders.push_back(ColliderBox{ imageRect });
    _selectedCollider = (int)asset->colliders.size() - 1;
    asset->dirty = true;
    _status = "Added collider box";
    _statusTimer = 2.f;
}

void MapEditor::SetMarker(MarkerKind kind, Vector2 imagePos)
{
    Asset* asset = ActiveAsset();
    if (!asset) return;

    Marker& marker = asset->markers[(int)kind];
    marker.has = true;
    marker.pos = imagePos;
    _selectedMarker = (int)kind;
    _selectedCollider = -1;
    asset->dirty = true;
    _status = std::string("Set ") + MarkerName(kind) + " marker (" + MarkerOffsetText(*asset, imagePos) + ")";
    _statusTimer = 3.f;
}

void MapEditor::Update()
{
    if (_statusTimer > 0.f) _statusTimer -= GetFrameTime();

    if (_helpOpen)
    {
        if (IsKeyPressed(KEY_H) || IsKeyPressed(KEY_ESCAPE)) _helpOpen = false;
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE)) { _wantsToExit = true; return; }
    if (IsKeyPressed(KEY_H)) { _helpOpen = true; return; }
    if (IsKeyPressed(KEY_S)) SaveActiveMetadata();
    if (IsKeyPressed(KEY_F))
    {
        Rectangle canvas{ kPanelW, 0.f, (float)kVirtualWidth - kPanelW, (float)kVirtualHeight - kStatusH };
        FitActiveAssetToCanvas(canvas);
    }
    if (IsKeyPressed(KEY_DELETE))
    {
        Asset* asset = ActiveAsset();
        if (asset && _selectedCollider >= 0 && _selectedCollider < (int)asset->colliders.size())
        {
            asset->colliders.erase(asset->colliders.begin() + _selectedCollider);
            _selectedCollider = -1;
            asset->dirty = true;
            _status = "Deleted collider";
            _statusTimer = 2.f;
        }
        else if (asset && _selectedMarker >= 0 && _selectedMarker < (int)MarkerKind::Count)
        {
            asset->markers[_selectedMarker] = Marker{};
            _selectedMarker = -1;
            asset->dirty = true;
            _status = "Cleared marker";
            _statusTimer = 2.f;
        }
    }

    Rectangle panel{ 0.f, 0.f, kPanelW, (float)kVirtualHeight - kStatusH };
    Rectangle canvas{ kPanelW, 0.f, (float)kVirtualWidth - kPanelW, (float)kVirtualHeight - kStatusH };
    UpdatePanel(panel);
    UpdateCanvas(canvas);
    UpdateSelectedColliderKeys();
}

void MapEditor::UpdatePanel(Rectangle panel)
{
    Vector2 mouse = GetVirtualMousePos();
    if (!CheckCollisionPointRec(mouse, panel)) return;

    float wheel = GetMouseWheelMove();
    if (wheel != 0.f)
    {
        float maxScroll = std::max(0.f, (float)_assets.size() * kRowH - (panel.height - 88.f));
        _assetListScroll = ClampFloat(_assetListScroll - wheel * kRowH * 2.f, 0.f, maxScroll);
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        int row = (int)((mouse.y - 82.f + _assetListScroll) / kRowH);
        if (row >= 0 && row < (int)_assets.size()) SelectAsset(row);
    }
}

void MapEditor::UpdateCanvas(Rectangle canvas)
{
    Asset* asset = ActiveAsset();
    if (!asset) return;

    Vector2 mouse = GetVirtualMousePos();
    bool inCanvas = CheckCollisionPointRec(mouse, canvas);
    float wheel = inCanvas ? GetMouseWheelMove() : 0.f;
    if (wheel != 0.f)
        _zoom = ClampFloat(_zoom + wheel * 0.18f * _zoom, 0.25f, 12.f);

    if (IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON) && inCanvas)
    {
        _panning = true;
        _panStartMouse = mouse;
        _panStartOffset = _viewOffset;
    }
    if (_panning)
    {
        if (IsMouseButtonDown(MOUSE_MIDDLE_BUTTON))
            _viewOffset = Vector2{ _panStartOffset.x + mouse.x - _panStartMouse.x,
                                   _panStartOffset.y + mouse.y - _panStartMouse.y };
        else
            _panning = false;
    }

    Vector2 imageMouse = ScreenToImage(mouse);
    if (inCanvas && IsKeyPressed(KEY_C))
        AddCollider(Rectangle{ imageMouse.x - 32.f, imageMouse.y - 32.f, 64.f, 64.f });
    if (inCanvas && IsKeyPressed(KEY_Z)) SetMarker(MarkerKind::Zeph, imageMouse);
    if (inCanvas && IsKeyPressed(KEY_P)) SetMarker(MarkerKind::Poe, imageMouse);
    if (inCanvas && IsKeyPressed(KEY_X)) SetMarker(MarkerKind::Respawn, imageMouse);

    if (inCanvas && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
    {
        _dragMode = DragMode::DrawCollider;
        _dragStartMouseImage = imageMouse;
        _drawPreview = Rectangle{ imageMouse.x, imageMouse.y, 0.f, 0.f };
    }
    if (_dragMode == DragMode::DrawCollider)
    {
        _drawPreview = Rectangle{ _dragStartMouseImage.x, _dragStartMouseImage.y,
                                  imageMouse.x - _dragStartMouseImage.x,
                                  imageMouse.y - _dragStartMouseImage.y };
        if (!IsMouseButtonDown(MOUSE_RIGHT_BUTTON))
        {
            Rectangle rect = NormalizeImageRect(_drawPreview);
            if (rect.width >= 3.f && rect.height >= 3.f) AddCollider(rect);
            _dragMode = DragMode::None;
        }
        return;
    }

    if (inCanvas && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        int markerHit = MarkerAt(imageMouse);
        if (markerHit >= 0)
        {
            _selectedMarker = markerHit;
            _selectedCollider = -1;
            _dragMarker = markerHit;
            _dragMode = DragMode::MoveMarker;
            _status = std::string("Selected ") + MarkerName((MarkerKind)markerHit) + " marker";
            _statusTimer = 2.f;
            return;
        }

        _selectedCollider = -1;
        _selectedMarker = -1;
        for (int i = (int)asset->colliders.size() - 1; i >= 0; --i)
        {
            if (HandleAt(asset->colliders[i], mouse))
            {
                _selectedCollider = i;
                _dragCollider = i;
                _dragMode = DragMode::ResizeCollider;
                _dragStartMouseImage = imageMouse;
                _dragStartRect = asset->colliders[i].rect;
                return;
            }
        }

        int hit = ColliderAt(imageMouse);
        if (hit >= 0)
        {
            _selectedCollider = hit;
            _dragCollider = hit;
            _dragMode = DragMode::MoveCollider;
            _dragStartMouseImage = imageMouse;
            _dragStartRect = asset->colliders[hit].rect;
        }
    }

    if (_dragMode == DragMode::MoveMarker)
    {
        if (_dragMarker < 0 || _dragMarker >= (int)MarkerKind::Count || !asset->markers[_dragMarker].has)
        {
            _dragMode = DragMode::None;
            _dragMarker = -1;
            return;
        }

        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            asset->markers[_dragMarker].pos = imageMouse;
            asset->dirty = true;
        }
        else
        {
            _dragMode = DragMode::None;
            _dragMarker = -1;
            _status = std::string("Moved ") + MarkerName((MarkerKind)_selectedMarker) + " marker (" + MarkerOffsetText(*asset, asset->markers[_selectedMarker].pos) + ")";
            _statusTimer = 3.f;
        }
        return;
    }

    if (_dragMode == DragMode::MoveCollider || _dragMode == DragMode::ResizeCollider)
    {
        if (_dragCollider < 0 || _dragCollider >= (int)asset->colliders.size())
        {
            _dragMode = DragMode::None;
            return;
        }

        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            Vector2 delta{ imageMouse.x - _dragStartMouseImage.x, imageMouse.y - _dragStartMouseImage.y };
            Rectangle rect = _dragStartRect;
            if (_dragMode == DragMode::MoveCollider)
            {
                rect.x += delta.x;
                rect.y += delta.y;
            }
            else
            {
                rect.width += delta.x;
                rect.height += delta.y;
            }
            asset->colliders[_dragCollider].rect = NormalizeImageRect(rect);
            asset->dirty = true;
        }
        else
        {
            _dragMode = DragMode::None;
            _dragCollider = -1;
        }
    }
}

void MapEditor::UpdateSelectedColliderKeys()
{
    Asset* asset = ActiveAsset();
    if (!asset || _selectedCollider < 0 || _selectedCollider >= (int)asset->colliders.size()) return;

    float moveStep = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) ? 8.f : 1.f;
    Rectangle rect = asset->colliders[_selectedCollider].rect;
    bool changed = false;
    bool resizing = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

    if (IsKeyDown(KEY_LEFT))  { resizing ? rect.width -= moveStep : rect.x -= moveStep; changed = true; }
    if (IsKeyDown(KEY_RIGHT)) { resizing ? rect.width += moveStep : rect.x += moveStep; changed = true; }
    if (IsKeyDown(KEY_UP))    { resizing ? rect.height -= moveStep : rect.y -= moveStep; changed = true; }
    if (IsKeyDown(KEY_DOWN))  { resizing ? rect.height += moveStep : rect.y += moveStep; changed = true; }

    if (changed)
    {
        asset->colliders[_selectedCollider].rect = NormalizeImageRect(rect);
        asset->dirty = true;
    }
}

void MapEditor::UpdateSelectedMarkerKeys()
{
    Asset* asset = ActiveAsset();
    if (!asset || _selectedCollider >= 0 || _selectedMarker < 0 || _selectedMarker >= (int)MarkerKind::Count) return;
    Marker& marker = asset->markers[_selectedMarker];
    if (!marker.has) return;

    float moveStep = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) ? 8.f : 1.f;
    bool changed = false;

    if (IsKeyDown(KEY_LEFT))  { marker.pos.x -= moveStep; changed = true; }
    if (IsKeyDown(KEY_RIGHT)) { marker.pos.x += moveStep; changed = true; }
    if (IsKeyDown(KEY_UP))    { marker.pos.y -= moveStep; changed = true; }
    if (IsKeyDown(KEY_DOWN))  { marker.pos.y += moveStep; changed = true; }

    if (changed)
    {
        asset->dirty = true;
        _status = std::string("Moved ") + MarkerName((MarkerKind)_selectedMarker) + " marker (" + MarkerOffsetText(*asset, marker.pos) + ")";
        _statusTimer = 1.5f;
    }
}

void MapEditor::Draw() const
{
    ClearBackground(Color{ 22, 23, 28, 255 });
    Rectangle panel{ 0.f, 0.f, kPanelW, (float)kVirtualHeight - kStatusH };
    Rectangle canvas{ kPanelW, 0.f, (float)kVirtualWidth - kPanelW, (float)kVirtualHeight - kStatusH };
    DrawCanvas(canvas);
    DrawPanel(panel);
    DrawStatusBar();
    if (_helpOpen) DrawHelp();
}

void MapEditor::DrawPanel(Rectangle panel) const
{
    DrawRectangleRec(panel, Color{ 30, 31, 38, 255 });
    DrawLineEx(Vector2{ panel.x + panel.width, panel.y }, Vector2{ panel.x + panel.width, panel.y + panel.height }, 2.f, Color{ 70, 72, 86, 255 });
    DrawText("VILLAGE ASSET ADJUSTER", 16, 14, 24, GOLD);
    DrawText("PNGs in VillageAssets | .vasset metadata", 16, 44, 16, Fade(RAYWHITE, 0.72f));
    DrawText("Z/P/X place markers | drag/arrows offset markers", 16, 64, 15, Fade(RAYWHITE, 0.72f));

    Rectangle list{ 10.f, 90.f, panel.width - 20.f, 230.f };
    DrawRectangleRec(list, Color{ 23, 24, 30, 255 });
    DrawRectangleLinesEx(list, 1.f, Color{ 75, 78, 90, 255 });
    BeginScissorMode((int)list.x, (int)list.y, (int)list.width, (int)list.height);
    for (int i = 0; i < (int)_assets.size(); ++i)
    {
        float y = list.y + i * kRowH - _assetListScroll;
        if (y + kRowH < list.y || y > list.y + list.height) continue;
        Rectangle row{ list.x + 5.f, y + 4.f, list.width - 10.f, kRowH - 6.f };
        bool active = i == _activeAsset;
        DrawRectangleRec(row, active ? Color{ 76, 82, 58, 255 } : Color{ 42, 44, 52, 255 });
        DrawRectangleLinesEx(row, 1.f, active ? GOLD : Color{ 65, 68, 78, 255 });
        DrawText(_assets[i].name.c_str(), (int)row.x + 8, (int)row.y + 7, 18, active ? GOLD : RAYWHITE);
        if (_assets[i].dirty) DrawText("*", (int)(row.x + row.width - 18), (int)row.y + 5, 22, GOLD);
    }
    if (_assets.empty()) DrawText("No PNG assets found", (int)list.x + 12, (int)list.y + 18, 20, Fade(RAYWHITE, 0.75f));
    EndScissorMode();

    const Asset* asset = ActiveAsset();
    if (!asset) return;

    int y = 340;
    DrawText(TextFormat("Asset: %s%s", asset->name.c_str(), asset->dirty ? " *" : ""), 16, y, 22, RAYWHITE); y += 30;
    DrawText(TextFormat("Image: %dx%d", asset->texture.width, asset->texture.height), 16, y, 18, Fade(RAYWHITE, 0.78f)); y += 26;
    DrawText(TextFormat("Colliders: %d", (int)asset->colliders.size()), 16, y, 18, Fade(RAYWHITE, 0.78f)); y += 26;

    for (int i = 0; i < (int)MarkerKind::Count; ++i)
    {
        MarkerKind kind = (MarkerKind)i;
        const Marker& marker = asset->markers[i];
        Color c = marker.has ? MarkerColor(kind) : Fade(RAYWHITE, 0.42f);
        const char* text = marker.has
            ? TextFormat("%s: %.0f, %.0f", MarkerName(kind), marker.pos.x, marker.pos.y)
            : TextFormat("%s: not set", MarkerName(kind));
        DrawText(text, 16, y, 18, c);
        y += 22;
        if (marker.has)
        {
            std::string offset = MarkerOffsetText(*asset, marker.pos);
            DrawText(offset.c_str(), 30, y, 15, offset == "inside PNG" ? Fade(RAYWHITE, 0.55f) : Fade(c, 0.78f));
            y += 20;
        }
        else
        {
            y += 4;
        }
    }

    y += 10;
    if (_selectedMarker >= 0 && _selectedMarker < (int)MarkerKind::Count && asset->markers[_selectedMarker].has)
    {
        const Marker& marker = asset->markers[_selectedMarker];
        DrawText(TextFormat("Selected marker: %s", MarkerName((MarkerKind)_selectedMarker)), 16, y, 18, GOLD); y += 24;
        DrawText(TextFormat("x %.0f y %.0f", marker.pos.x, marker.pos.y), 16, y, 17, RAYWHITE); y += 22;
        std::string offset = MarkerOffsetText(*asset, marker.pos);
        DrawText(offset.c_str(), 16, y, 15, offset == "inside PNG" ? Fade(RAYWHITE, 0.62f) : MarkerColor((MarkerKind)_selectedMarker)); y += 22;
        DrawText("Drag marker or use arrows; Shift moves faster", 16, y, 15, Fade(RAYWHITE, 0.68f)); y += 24;
    }

    DrawText("Selected collider:", 16, y, 18, GOLD); y += 24;
    if (_selectedCollider >= 0 && _selectedCollider < (int)asset->colliders.size())
    {
        Rectangle r = asset->colliders[_selectedCollider].rect;
        DrawText(TextFormat("x %.0f y %.0f w %.0f h %.0f", r.x, r.y, r.width, r.height), 16, y, 17, RAYWHITE); y += 22;
        DrawText("Drag box to move, drag corner to resize", 16, y, 15, Fade(RAYWHITE, 0.68f)); y += 20;
        DrawText("Arrows move, Ctrl+Arrows resize, Shift faster", 16, y, 15, Fade(RAYWHITE, 0.68f));
    }
    else
    {
        DrawText("none", 16, y, 17, Fade(RAYWHITE, 0.62f));
    }
}

void MapEditor::DrawCanvas(Rectangle canvas) const
{
    DrawRectangleRec(canvas, Color{ 18, 19, 24, 255 });
    BeginScissorMode((int)canvas.x, (int)canvas.y, (int)canvas.width, (int)canvas.height);

    const Asset* asset = ActiveAsset();
    if (!asset || asset->texture.id == 0)
    {
        DrawText("Select a PNG asset from VillageAssets", (int)canvas.x + 40, (int)canvas.y + 40, 24, RAYWHITE);
        EndScissorMode();
        return;
    }

    Rectangle img = ActiveImageBoundsScreen();
    const int checker = 32;
    for (int y = (int)canvas.y; y < canvas.y + canvas.height; y += checker)
        for (int x = (int)canvas.x; x < canvas.x + canvas.width; x += checker)
        {
            bool dark = ((x / checker) + (y / checker)) % 2 == 0;
            DrawRectangle(x, y, checker, checker, dark ? Color{ 24, 25, 30, 255 } : Color{ 28, 29, 35, 255 });
        }

    DrawTexturePro(asset->texture,
                   Rectangle{ 0.f, 0.f, (float)asset->texture.width, (float)asset->texture.height },
                   img, Vector2{ 0.f, 0.f }, 0.f, WHITE);
    DrawRectangleLinesEx(img, 2.f, Color{ 110, 114, 130, 255 });

    for (int i = 0; i < (int)asset->colliders.size(); ++i)
    {
        Rectangle sr = ImageRectToScreen(asset->colliders[i].rect);
        bool selected = i == _selectedCollider;
        Color fill = selected ? Fade(Color{ 255, 75, 55, 255 }, 0.34f) : Fade(Color{ 240, 50, 45, 255 }, 0.22f);
        Color line = selected ? GOLD : Color{ 245, 80, 70, 255 };
        DrawRectangleRec(sr, fill);
        DrawRectangleLinesEx(sr, selected ? 3.f : 2.f, line);
        DrawText(TextFormat("%d", i + 1), (int)sr.x + 4, (int)sr.y + 4, 16, line);
        if (selected)
            DrawRectangle((int)(sr.x + sr.width - kHandleSize * 0.5f), (int)(sr.y + sr.height - kHandleSize * 0.5f), (int)kHandleSize, (int)kHandleSize, GOLD);
    }

    for (int i = 0; i < (int)MarkerKind::Count; ++i)
    {
        MarkerKind kind = (MarkerKind)i;
        const Marker& marker = asset->markers[i];
        if (!marker.has) continue;
        Vector2 p = ImageToScreen(marker.pos);
        Color c = MarkerColor(kind);
        Vector2 nearestOnImage{
            ClampFloat(marker.pos.x, 0.f, (float)asset->texture.width),
            ClampFloat(marker.pos.y, 0.f, (float)asset->texture.height)
        };
        if (nearestOnImage.x != marker.pos.x || nearestOnImage.y != marker.pos.y)
        {
            Vector2 anchor = ImageToScreen(nearestOnImage);
            DrawLineEx(anchor, p, 2.f, Fade(c, 0.55f));
            DrawCircleV(anchor, 5.f, Fade(c, 0.65f));
        }
        bool selected = i == _selectedMarker;
        DrawCircleV(p, selected ? 15.f : 12.f, Fade(c, selected ? 0.45f : 0.32f));
        DrawCircleLines((int)p.x, (int)p.y, selected ? 15.f : 12.f, selected ? GOLD : c);
        DrawLine((int)p.x - 16, (int)p.y, (int)p.x + 16, (int)p.y, c);
        DrawLine((int)p.x, (int)p.y - 16, (int)p.x, (int)p.y + 16, c);
        DrawText(MarkerName(kind), (int)p.x + 14, (int)p.y - 10, 18, c);
    }

    if (_dragMode == DragMode::DrawCollider)
    {
        Rectangle sr = ImageRectToScreen(NormalizeImageRect(_drawPreview));
        DrawRectangleRec(sr, Fade(Color{ 255, 80, 60, 255 }, 0.18f));
        DrawRectangleLinesEx(sr, 2.f, Color{ 255, 110, 90, 255 });
    }

    EndScissorMode();

    DrawText("Mouse wheel zoom | middle-drag pan | C add box | RMB drag new box | S save | H help",
             (int)canvas.x + 14, (int)canvas.y + 12, 18, Fade(RAYWHITE, 0.78f));
}

void MapEditor::DrawStatusBar() const
{
    Rectangle bar{ 0.f, (float)kVirtualHeight - kStatusH, (float)kVirtualWidth, kStatusH };
    DrawRectangleRec(bar, Color{ 36, 37, 45, 255 });
    const Asset* asset = ActiveAsset();
    const char* name = asset ? asset->name.c_str() : "no asset";
    const char* info = TextFormat("ASSET %s%s | colliders %d | drag/arrows move selected marker outside PNG | C/RMB collider | S save | ESC exit",
                                  name, (asset && asset->dirty) ? "*" : "", asset ? (int)asset->colliders.size() : 0);
    DrawText(info, 14, (int)bar.y + 10, 20, RAYWHITE);
    if (_statusTimer > 0.f && !_status.empty())
    {
        int w = MeasureText(_status.c_str(), 20);
        DrawText(_status.c_str(), kVirtualWidth - w - 16, (int)bar.y + 10, 20, GOLD);
    }
}

void MapEditor::DrawHelp() const
{
    DrawRectangle(0, 0, kVirtualWidth, kVirtualHeight, Fade(BLACK, 0.80f));
    const char* lines[] = {
        "VILLAGE ASSET ADJUSTER",
        "",
        "This edits metadata for finished PNGs in VillageAssets.",
        "The PNG is the visual. The .vasset file stores colliders and markers beside it.",
        "",
        "Left panel: click ZephsShop, VillageGraveyard, or any future PNG asset.",
        "Canvas: mouse wheel zooms, middle mouse pans.",
        "",
        "Colliders: C adds a default box at mouse. Right-drag draws a new box.",
        "           Left-drag a box to move it. Drag its bottom-right handle to resize it.",
        "           Delete removes selected box. Arrows move selected; Ctrl+Arrows resize; Shift is faster.",
        "",
        "Markers:   Z places Zeph. P places Poe. X places player respawn point.",
        "           Markers may sit outside the PNG; the panel shows their pixel offset from it.",
        "           Click/drag a marker, or use Arrow keys after selecting/placing it.",
        "Saving:    S writes VillageAssets/<asset>.vasset. ESC returns to menu.",
        "",
        "Next step: the village builder will place these PNG assets on a large forest-ground village map.",
    };
    int y = 150;
    for (const char* line : lines)
    {
        int fs = (y == 150) ? 38 : 24;
        DrawText(line, kVirtualWidth / 2 - MeasureText(line, fs) / 2, y, fs, (y == 150) ? GOLD : RAYWHITE);
        y += (y == 150) ? 58 : 33;
    }
}
