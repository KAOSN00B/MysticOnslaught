#pragma once

#include "raylib.h"
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// VillageMap — a hand-authored, variable-size, LAYERED tile map for the hub
// village (and building interiors). Unlike the dungeon's RoomLayout (which is
// procedurally generated from a small TileType enum), a VillageMap stores raw
// tilesheet coordinates per cell, so the whole Village/Interior sheets are
// paintable directly — houses are stamped as multi-tile rectangles.
//
// Layers (drawn bottom → top):
//   ground    grass / paths / water            (under everything)
//   objects   houses, fences, wells, props     (under the player)
//   overhead  roof edges, tree tops            (drawn OVER the player)
//   solid     collision flags (not drawn; blocks walking)
//
// Saved as villagemap_<name>.txt next to the exe (same pattern as the other
// tuning files). Only non-empty cells are written.
// ─────────────────────────────────────────────────────────────────────────────

// One painted cell: which sheet and which 16px tile within it. sheet -1 = empty.
struct VillageMapCell
{
    short sheet = -1;   // index into the editor/game's loaded sheet list
    short col   = 0;    // tile column within the sheet (16px units)
    short row   = 0;    // tile row within the sheet
};

struct VillageMap
{
    static constexpr int kTileSize   = 16;   // source pixels per tile
    static constexpr int kDefaultCols = 60;
    static constexpr int kDefaultRows = 40;

    int cols = kDefaultCols;
    int rows = kDefaultRows;

    std::vector<VillageMapCell> ground;
    std::vector<VillageMapCell> objects;
    std::vector<VillageMapCell> overhead;
    std::vector<unsigned char>  solid;     // 1 = blocks movement

    VillageMap() { Resize(kDefaultCols, kDefaultRows); }

    int  Index(int cellCol, int cellRow) const { return cellRow * cols + cellCol; }
    bool InBounds(int cellCol, int cellRow) const
    { return cellCol >= 0 && cellCol < cols && cellRow >= 0 && cellRow < rows; }

    // Clears the map to empty at the given dimensions.
    void Resize(int newCols, int newRows);

    bool Save(const std::string& mapName) const;   // villagemap_<name>.txt
    bool Load(const std::string& mapName);         // false if no file exists

    // Which visual layer to draw (solid is an overlay, handled by callers).
    enum class Layer { Ground = 0, Objects = 1, Overhead = 2 };
    const std::vector<VillageMapCell>& CellsFor(Layer layer) const;
    std::vector<VillageMapCell>&       CellsFor(Layer layer);

    // Draws one layer with per-cell culling. originScreen = screen position of
    // map cell (0,0); scale = screen pixels per source pixel (16px tile → 16*scale).
    // sheets/sheetCount = the loaded tilesheet textures the cells reference.
    void DrawLayer(Layer layer, const Texture2D* sheets, int sheetCount,
                   Vector2 originScreen, float scale, Color tint,
                   float screenW, float screenH) const;
};
