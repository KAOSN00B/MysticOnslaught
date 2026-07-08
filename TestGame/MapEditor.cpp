#include "MapEditor.h"
#include "AssetPaths.h"
#include "VirtualCanvas.h"

#include <algorithm>
#include <vector>

namespace
{
    // Fixed sheet list — cell.sheet indexes THIS order, so it must never be
    // reshuffled (saved maps depend on it). Missing files stay as empty slots.
    const char* kSheetStems[] = {
        "Village_Ground",      // 0: grass / dirt paths / water
        "Village",             // 1: houses, fences, trees, wells
        "Village_Objects01",   // 2: extra village props
        "Village_Objects02",   // 3: extra village props
        "Interior",            // 4: building interiors + furniture
    };
    constexpr int kSheetCount = 5;

    // Layout (virtual-canvas space).
    constexpr float kPanelW      = 520.f;
    constexpr float kTabH        = 40.f;
    constexpr float kStatusH     = 36.f;
    constexpr float kPaletteScale = 2.f;   // palette draws tiles at 32px

    const char* LayerName(VillageMap::Layer layer, bool collisionMode)
    {
        if (collisionMode) return "COLLISION";
        switch (layer)
        {
        case VillageMap::Layer::Objects:  return "OBJECTS";
        case VillageMap::Layer::Overhead: return "OVERHEAD";
        default:                          return "GROUND";
        }
    }
}

void MapEditor::Init()
{
    _wantsToExit    = false;
    _confirmingExit = false;
    _confirmingNew  = false;
    _helpOpen       = false;
    _dirty          = false;
    _activeSheet    = 0;
    _activeLayer    = VillageMap::Layer::Ground;
    _collisionMode  = false;
    _brush          = Brush{};
    _zoom           = 2.f;
    _cameraPan      = Vector2{ -40.f, -40.f };   // small margin around the map
    _paletteScroll  = Vector2{};

    LoadSheets();

    if (_map.Load(_mapName)) { _status = "Loaded villagemap_" + _mapName + ".txt"; }
    else                     { _map.Resize(VillageMap::kDefaultCols, VillageMap::kDefaultRows);
                               _status = "New blank map (no save found)"; }
    _statusTimer = 3.f;
}

void MapEditor::LoadSheets()
{
    if (!_sheets.empty()) return;   // already loaded (Init after re-entry)
    for (int i = 0; i < kSheetCount; i++)
    {
        std::string path = std::string("MapTilesets/") + kSheetStems[i] + ".png";
        _sheets.push_back(LoadTexture(AssetPath(path.c_str()).c_str()));
        _sheetNames.push_back(kSheetStems[i]);
    }
}

void MapEditor::Unload()
{
    for (Texture2D& sheet : _sheets)
        if (sheet.id != 0) UnloadTexture(sheet);
    _sheets.clear();
    _sheetNames.clear();
}

// ─────────────────────────────────────────────────────────────────────────────
// Update
// ─────────────────────────────────────────────────────────────────────────────
void MapEditor::Update()
{
    if (_statusTimer > 0.f) _statusTimer -= GetFrameTime();

    // ── Modal prompts eat all input ──
    if (_confirmingExit)
    {
        if (IsKeyPressed(KEY_ENTER))  { _map.Save(_mapName); _wantsToExit = true; }
        if (IsKeyPressed(KEY_X))      { _wantsToExit = true; }
        if (IsKeyPressed(KEY_ESCAPE)) { _confirmingExit = false; }
        return;
    }
    if (_confirmingNew)
    {
        if (IsKeyPressed(KEY_ENTER))
        {
            _map.Resize(VillageMap::kDefaultCols, VillageMap::kDefaultRows);
            _dirty = true;
            _status = "New blank map"; _statusTimer = 2.f;
            _confirmingNew = false;
        }
        if (IsKeyPressed(KEY_ESCAPE)) _confirmingNew = false;
        return;
    }
    if (_helpOpen)
    {
        if (IsKeyPressed(KEY_H) || IsKeyPressed(KEY_ESCAPE)) _helpOpen = false;
        return;
    }

    // ── Global keys ──
    if (IsKeyPressed(KEY_ESCAPE))
    {
        if (_dirty) _confirmingExit = true;
        else        _wantsToExit = true;
        return;
    }
    if (IsKeyPressed(KEY_H)) { _helpOpen = true; return; }
    if (IsKeyPressed(KEY_S)) { _map.Save(_mapName); _dirty = false;
                               _status = "Saved villagemap_" + _mapName + ".txt"; _statusTimer = 2.f; }
    if (IsKeyPressed(KEY_N)) { _confirmingNew = true; return; }
    if (IsKeyPressed(KEY_G)) _showGrid = !_showGrid;
    if (IsKeyPressed(KEY_C)) _showCollision = !_showCollision;

    // Layer tabs.
    if (IsKeyPressed(KEY_Q)) { _activeLayer = VillageMap::Layer::Ground;   _collisionMode = false; }
    if (IsKeyPressed(KEY_W)) { _activeLayer = VillageMap::Layer::Objects;  _collisionMode = false; }
    if (IsKeyPressed(KEY_E)) { _activeLayer = VillageMap::Layer::Overhead; _collisionMode = false; }
    if (IsKeyPressed(KEY_R)) { _collisionMode = true; }

    // Sheet tabs.
    for (int i = 0; i < kSheetCount; i++)
        if (IsKeyPressed(KEY_ONE + i)) _activeSheet = i;

    Rectangle panel { 0.f, 0.f, kPanelW, (float)kVirtualHeight - kStatusH };
    Rectangle canvas{ kPanelW, 0.f, (float)kVirtualWidth - kPanelW,
                      (float)kVirtualHeight - kStatusH };

    UpdatePalette(panel);
    UpdateCanvas(canvas);
}

void MapEditor::UpdatePalette(Rectangle panel)
{
    Vector2 mouse = GetVirtualMousePos();

    // Sheet tab buttons across the top of the panel.
    float tabW = panel.width / kSheetCount;
    for (int i = 0; i < kSheetCount; i++)
    {
        Rectangle tab{ panel.x + i * tabW, panel.y, tabW, kTabH };
        if (CheckCollisionPointRec(mouse, tab) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            _activeSheet = i;
    }

    Rectangle view{ panel.x, panel.y + kTabH, panel.width, panel.height - kTabH };
    if (!CheckCollisionPointRec(mouse, view))
    {
        if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON)) _paletteDragging = false;
        return;
    }

    // Wheel scrolls the palette (SHIFT = horizontal).
    float wheel = GetMouseWheelMove();
    if (wheel != 0.f)
    {
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
            _paletteScroll.x -= wheel * 64.f;
        else
            _paletteScroll.y -= wheel * 64.f;
        if (_paletteScroll.x < 0.f) _paletteScroll.x = 0.f;
        if (_paletteScroll.y < 0.f) _paletteScroll.y = 0.f;
    }

    const Texture2D& sheet = _sheets[_activeSheet];
    if (sheet.id == 0) return;

    // Which sheet tile is under the mouse?
    float tilePx  = VillageMap::kTileSize * kPaletteScale;
    int   tileCol = (int)((mouse.x - view.x + _paletteScroll.x) / tilePx);
    int   tileRow = (int)((mouse.y - view.y + _paletteScroll.y) / tilePx);
    int   sheetCols = sheet.width  / VillageMap::kTileSize;
    int   sheetRows = sheet.height / VillageMap::kTileSize;
    if (tileCol < 0 || tileRow < 0 || tileCol >= sheetCols || tileRow >= sheetRows)
        return;

    // Click = 1×1 brush; drag = rubber-band a multi-tile stamp (houses!).
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        _paletteDragging = true;
        _paletteDragStartTile = Vector2{ (float)tileCol, (float)tileRow };
    }
    if (_paletteDragging)
    {
        int startCol = (int)_paletteDragStartTile.x;
        int startRow = (int)_paletteDragStartTile.y;
        _brush.sheet = (short)_activeSheet;
        _brush.col   = (short)std::min(startCol, tileCol);
        _brush.row   = (short)std::min(startRow, tileRow);
        _brush.w     = (short)(std::abs(tileCol - startCol) + 1);
        _brush.h     = (short)(std::abs(tileRow - startRow) + 1);
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) _paletteDragging = false;
    }
}

void MapEditor::UpdateCanvas(Rectangle canvas)
{
    Vector2 mouse = GetVirtualMousePos();
    float dt = GetFrameTime();

    // Arrow-key panning always works; middle-drag panning when over the canvas.
    float panSpeed = 900.f * dt;
    if (IsKeyDown(KEY_LEFT))  _cameraPan.x -= panSpeed;
    if (IsKeyDown(KEY_RIGHT)) _cameraPan.x += panSpeed;
    if (IsKeyDown(KEY_UP))    _cameraPan.y -= panSpeed;
    if (IsKeyDown(KEY_DOWN))  _cameraPan.y += panSpeed;

    if (!CheckCollisionPointRec(mouse, canvas))
    {
        if (!IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) _panning = false;
        return;
    }

    if (IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON))
    {
        _panning      = true;
        _panMouseStart = mouse;
        _panCamStart   = _cameraPan;
    }
    if (_panning)
    {
        _cameraPan.x = _panCamStart.x - (mouse.x - _panMouseStart.x);
        _cameraPan.y = _panCamStart.y - (mouse.y - _panMouseStart.y);
        if (IsMouseButtonReleased(MOUSE_MIDDLE_BUTTON)) _panning = false;
        return;   // don't paint while panning
    }

    // Wheel zoom, anchored on the map point under the cursor.
    float wheel = GetMouseWheelMove();
    if (wheel != 0.f)
    {
        Vector2 origin = MapOrigin(canvas);
        float sourceX = (mouse.x - origin.x) / _zoom;   // map point in source px
        float sourceY = (mouse.y - origin.y) / _zoom;
        _zoom *= (wheel > 0.f) ? 1.25f : 0.8f;
        _zoom = std::max(0.5f, std::min(6.f, _zoom));
        _cameraPan.x = canvas.x - (mouse.x - sourceX * _zoom);
        _cameraPan.y = canvas.y - (mouse.y - sourceY * _zoom);
    }

    // Hovered map cell.
    Vector2 origin = MapOrigin(canvas);
    int cellCol = (int)((mouse.x - origin.x) / TilePx());
    int cellRow = (int)((mouse.y - origin.y) / TilePx());
    if (mouse.x < origin.x) cellCol = -1;   // int cast rounds toward 0
    if (mouse.y < origin.y) cellRow = -1;
    if (!_map.InBounds(cellCol, cellRow)) return;

    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))  PaintAt(cellCol, cellRow);
    if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) EraseAt(cellCol, cellRow);
    if (IsKeyPressed(KEY_F))                   FloodFillAt(cellCol, cellRow);
}

void MapEditor::PaintAt(int cellCol, int cellRow)
{
    if (_collisionMode)
    {
        _map.solid[_map.Index(cellCol, cellRow)] = 1;
        _dirty = true;
        return;
    }
    // Stamp the whole brush rectangle, anchored at the hovered cell.
    std::vector<VillageMapCell>& cells = _map.CellsFor(_activeLayer);
    for (int dy = 0; dy < _brush.h; dy++)
        for (int dx = 0; dx < _brush.w; dx++)
        {
            int c = cellCol + dx, r = cellRow + dy;
            if (!_map.InBounds(c, r)) continue;
            VillageMapCell cell;
            cell.sheet = _brush.sheet;
            cell.col   = _brush.col + (short)dx;
            cell.row   = _brush.row + (short)dy;
            cells[_map.Index(c, r)] = cell;
        }
    _dirty = true;
}

void MapEditor::EraseAt(int cellCol, int cellRow)
{
    if (_collisionMode)
    {
        _map.solid[_map.Index(cellCol, cellRow)] = 0;
        _dirty = true;
        return;
    }
    _map.CellsFor(_activeLayer)[_map.Index(cellCol, cellRow)] = VillageMapCell{};
    _dirty = true;
}

void MapEditor::FloodFillAt(int cellCol, int cellRow)
{
    if (_collisionMode) return;   // fill is for visual layers

    std::vector<VillageMapCell>& cells = _map.CellsFor(_activeLayer);
    VillageMapCell target = cells[_map.Index(cellCol, cellRow)];
    VillageMapCell paint;
    paint.sheet = _brush.sheet; paint.col = _brush.col; paint.row = _brush.row;
    if (target.sheet == paint.sheet && target.col == paint.col && target.row == paint.row)
        return;   // filling with itself

    // Simple BFS over 4-connected cells matching the clicked cell's tile.
    std::vector<int> frontier;
    frontier.push_back(_map.Index(cellCol, cellRow));
    while (!frontier.empty())
    {
        int idx = frontier.back(); frontier.pop_back();
        VillageMapCell& cell = cells[idx];
        if (cell.sheet != target.sheet || cell.col != target.col || cell.row != target.row)
            continue;
        cell = paint;

        int c = idx % _map.cols, r = idx / _map.cols;
        if (c > 0)              frontier.push_back(idx - 1);
        if (c < _map.cols - 1)  frontier.push_back(idx + 1);
        if (r > 0)              frontier.push_back(idx - _map.cols);
        if (r < _map.rows - 1)  frontier.push_back(idx + _map.cols);
    }
    _dirty = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw
// ─────────────────────────────────────────────────────────────────────────────
void MapEditor::Draw() const
{
    ClearBackground(Color{ 24, 24, 30, 255 });

    Rectangle panel { 0.f, 0.f, kPanelW, (float)kVirtualHeight - kStatusH };
    Rectangle canvas{ kPanelW, 0.f, (float)kVirtualWidth - kPanelW,
                      (float)kVirtualHeight - kStatusH };

    DrawCanvas(canvas);
    DrawPalette(panel);
    DrawStatusBar();

    // Modal prompts / help draw on top of everything.
    if (_confirmingExit)
    {
        DrawRectangle(0, 0, kVirtualWidth, kVirtualHeight, Fade(BLACK, 0.65f));
        const char* line1 = "Unsaved changes!";
        const char* line2 = "ENTER: save & exit     X: exit without saving     ESC: keep editing";
        DrawText(line1, kVirtualWidth / 2 - MeasureText(line1, 44) / 2, 460, 44, GOLD);
        DrawText(line2, kVirtualWidth / 2 - MeasureText(line2, 26) / 2, 530, 26, RAYWHITE);
    }
    if (_confirmingNew)
    {
        DrawRectangle(0, 0, kVirtualWidth, kVirtualHeight, Fade(BLACK, 0.65f));
        const char* line1 = "Clear the whole map?";
        const char* line2 = "ENTER: yes, start blank     ESC: cancel";
        DrawText(line1, kVirtualWidth / 2 - MeasureText(line1, 44) / 2, 460, 44, Color{ 235, 90, 90, 255 });
        DrawText(line2, kVirtualWidth / 2 - MeasureText(line2, 26) / 2, 530, 26, RAYWHITE);
    }
    if (_helpOpen) DrawHelp();
}

void MapEditor::DrawPalette(Rectangle panel) const
{
    DrawRectangleRec(panel, Color{ 34, 34, 42, 255 });
    DrawLineEx(Vector2{ panel.x + panel.width, panel.y },
               Vector2{ panel.x + panel.width, panel.y + panel.height }, 2.f, Color{ 70, 70, 84, 255 });

    // Sheet tabs.
    float tabW = panel.width / kSheetCount;
    for (int i = 0; i < kSheetCount; i++)
    {
        Rectangle tab{ panel.x + i * tabW, panel.y, tabW, kTabH };
        bool active  = (i == _activeSheet);
        bool missing = (_sheets[i].id == 0);
        DrawRectangleRec(tab, active ? Color{ 70, 70, 92, 255 } : Color{ 44, 44, 54, 255 });
        DrawRectangleLinesEx(tab, 1.f, Color{ 70, 70, 84, 255 });
        const char* label = TextFormat("%d", i + 1);
        DrawText(label, (int)(tab.x + tab.width * 0.5f - MeasureText(label, 22) * 0.5f),
                 (int)(tab.y + 9.f), 22, missing ? Color{ 120, 80, 80, 255 } : (active ? GOLD : LIGHTGRAY));
    }

    // Palette view (scissored so scrolling stays inside the panel).
    Rectangle view{ panel.x, panel.y + kTabH, panel.width, panel.height - kTabH };
    BeginScissorMode((int)view.x, (int)view.y, (int)view.width, (int)view.height);
    DrawRectangleRec(view, Color{ 28, 28, 34, 255 });

    const Texture2D& sheet = _sheets[_activeSheet];
    if (sheet.id != 0)
    {
        Rectangle src{ 0.f, 0.f, (float)sheet.width, (float)sheet.height };
        Rectangle dst{ view.x - _paletteScroll.x, view.y - _paletteScroll.y,
                       sheet.width * kPaletteScale, sheet.height * kPaletteScale };
        DrawTexturePro(sheet, src, dst, Vector2{ 0.f, 0.f }, 0.f, WHITE);

        // Brush highlight (only when the brush comes from this sheet).
        if (_brush.sheet == _activeSheet)
        {
            float tilePx = VillageMap::kTileSize * kPaletteScale;
            Rectangle sel{ dst.x + _brush.col * tilePx, dst.y + _brush.row * tilePx,
                           _brush.w * tilePx, _brush.h * tilePx };
            DrawRectangleLinesEx(sel, 2.f, GOLD);
        }
    }
    else
    {
        DrawText("sheet missing", (int)(view.x + 24.f), (int)(view.y + 24.f), 24, Color{ 150, 90, 90, 255 });
    }
    EndScissorMode();

    // Current sheet name under the tabs (drawn over the view's top edge).
    DrawText(_sheetNames[_activeSheet].c_str(), (int)(panel.x + 10.f),
             (int)(panel.y + kTabH + 6.f), 20, Fade(RAYWHITE, 0.75f));
}

void MapEditor::DrawCanvas(Rectangle canvas) const
{
    BeginScissorMode((int)canvas.x, (int)canvas.y, (int)canvas.width, (int)canvas.height);
    DrawRectangleRec(canvas, Color{ 20, 20, 26, 255 });

    Vector2 origin = MapOrigin(canvas);
    float tilePx = TilePx();
    float mapW = _map.cols * tilePx;
    float mapH = _map.rows * tilePx;

    // Checkerboard under the map so unpainted (transparent) cells read clearly.
    const float checker = tilePx * 2.f;
    for (float y = 0.f; y < mapH; y += checker)
        for (float x = ((int)(y / checker) % 2 == 0) ? 0.f : checker; x < mapW; x += checker * 2.f)
            DrawRectangle((int)(origin.x + x), (int)(origin.y + y),
                          (int)std::min(checker, mapW - x), (int)std::min(checker, mapH - y),
                          Color{ 30, 30, 38, 255 });

    // Layers, bottom → top; the active layer draws full-strength, others dim so
    // it is always obvious which layer the brush will hit.
    auto layerTint = [&](VillageMap::Layer layer) -> Color {
        if (_collisionMode) return Fade(WHITE, 0.6f);
        return (layer == _activeLayer) ? WHITE : Fade(WHITE, 0.45f);
    };
    _map.DrawLayer(VillageMap::Layer::Ground,   _sheets.data(), (int)_sheets.size(),
                   origin, _zoom, layerTint(VillageMap::Layer::Ground),
                   (float)kVirtualWidth, (float)kVirtualHeight);
    _map.DrawLayer(VillageMap::Layer::Objects,  _sheets.data(), (int)_sheets.size(),
                   origin, _zoom, layerTint(VillageMap::Layer::Objects),
                   (float)kVirtualWidth, (float)kVirtualHeight);
    _map.DrawLayer(VillageMap::Layer::Overhead, _sheets.data(), (int)_sheets.size(),
                   origin, _zoom, layerTint(VillageMap::Layer::Overhead),
                   (float)kVirtualWidth, (float)kVirtualHeight);

    // Collision overlay.
    if (_showCollision || _collisionMode)
    {
        for (int r = 0; r < _map.rows; r++)
            for (int c = 0; c < _map.cols; c++)
                if (_map.solid[_map.Index(c, r)])
                    DrawRectangle((int)(origin.x + c * tilePx), (int)(origin.y + r * tilePx),
                                  (int)tilePx, (int)tilePx,
                                  Fade(Color{ 235, 60, 60, 255 }, _collisionMode ? 0.45f : 0.25f));
    }

    // Grid.
    if (_showGrid && tilePx >= 12.f)
    {
        Color gridColor = Fade(WHITE, 0.07f);
        for (int c = 0; c <= _map.cols; c++)
            DrawLineEx(Vector2{ origin.x + c * tilePx, origin.y },
                       Vector2{ origin.x + c * tilePx, origin.y + mapH }, 1.f, gridColor);
        for (int r = 0; r <= _map.rows; r++)
            DrawLineEx(Vector2{ origin.x, origin.y + r * tilePx },
                       Vector2{ origin.x + mapW, origin.y + r * tilePx }, 1.f, gridColor);
    }

    // Map bounds.
    DrawRectangleLinesEx(Rectangle{ origin.x, origin.y, mapW, mapH }, 2.f, Color{ 90, 90, 110, 255 });

    // Ghost preview of the brush at the hovered cell.
    Vector2 mouse = GetVirtualMousePos();
    if (CheckCollisionPointRec(mouse, canvas) && !_panning)
    {
        int cellCol = (int)((mouse.x - origin.x) / tilePx);
        int cellRow = (int)((mouse.y - origin.y) / tilePx);
        if (mouse.x >= origin.x && mouse.y >= origin.y && _map.InBounds(cellCol, cellRow))
        {
            if (_collisionMode)
            {
                DrawRectangleLinesEx(Rectangle{ origin.x + cellCol * tilePx, origin.y + cellRow * tilePx,
                                                tilePx, tilePx }, 2.f, Color{ 235, 60, 60, 255 });
            }
            else if (_brush.sheet >= 0 && _brush.sheet < (int)_sheets.size() &&
                     _sheets[_brush.sheet].id != 0)
            {
                const Texture2D& sheet = _sheets[_brush.sheet];
                Rectangle src{ _brush.col * 16.f, _brush.row * 16.f, _brush.w * 16.f, _brush.h * 16.f };
                Rectangle dst{ origin.x + cellCol * tilePx, origin.y + cellRow * tilePx,
                               _brush.w * tilePx, _brush.h * tilePx };
                DrawTexturePro(sheet, src, dst, Vector2{ 0.f, 0.f }, 0.f, Fade(WHITE, 0.55f));
                DrawRectangleLinesEx(dst, 1.f, GOLD);
            }
        }
    }

    EndScissorMode();
}

void MapEditor::DrawStatusBar() const
{
    Rectangle bar{ 0.f, (float)kVirtualHeight - kStatusH, (float)kVirtualWidth, kStatusH };
    DrawRectangleRec(bar, Color{ 38, 38, 48, 255 });

    const char* info = TextFormat("%s%s  |  map %dx%d  |  layer %s  |  sheet %s  |  H help",
                                  _mapName.c_str(), _dirty ? "*" : "",
                                  _map.cols, _map.rows,
                                  LayerName(_activeLayer, _collisionMode),
                                  _sheetNames.empty() ? "-" : _sheetNames[_activeSheet].c_str());
    DrawText(info, 12, (int)(bar.y + 8.f), 22, RAYWHITE);

    if (_statusTimer > 0.f)
    {
        int w = MeasureText(_status.c_str(), 22);
        DrawText(_status.c_str(), kVirtualWidth - w - 16, (int)(bar.y + 8.f), 22, GOLD);
    }
}

void MapEditor::DrawHelp() const
{
    DrawRectangle(0, 0, kVirtualWidth, kVirtualHeight, Fade(BLACK, 0.75f));
    const char* lines[] = {
        "MAP EDITOR — CONTROLS",
        "",
        "Palette (left):  click = pick tile,  DRAG = pick a multi-tile stamp (houses!)",
        "                 1-5 or click tabs to switch sheets,  wheel scroll (SHIFT = sideways)",
        "",
        "Canvas:          LEFT-drag paint,  RIGHT-drag erase,  F flood-fill",
        "                 wheel zoom,  MIDDLE-drag or arrow keys pan",
        "",
        "Layers:          Q Ground   W Objects   E Overhead (draws above player)   R Collision",
        "                 on Collision: LEFT marks solid, RIGHT clears",
        "",
        "G grid   C collision overlay   S save   N new blank map   ESC exit",
    };
    int y = 300;
    for (const char* line : lines)
    {
        int fs = (y == 300) ? 40 : 26;
        DrawText(line, kVirtualWidth / 2 - MeasureText(line, fs) / 2, y, fs, (y == 300) ? GOLD : RAYWHITE);
        y += (y == 300) ? 64 : 38;
    }
}
