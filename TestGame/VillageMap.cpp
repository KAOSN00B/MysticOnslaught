#pragma warning(disable: 4996)   // fopen/fscanf are fine here; paths are internal

#include "VillageMap.h"

#include <cstdio>

void VillageMap::Resize(int newCols, int newRows)
{
    cols = (newCols < 1) ? 1 : newCols;
    rows = (newRows < 1) ? 1 : newRows;
    ground.assign((size_t)cols * rows, VillageMapCell{});
    objects.assign((size_t)cols * rows, VillageMapCell{});
    overhead.assign((size_t)cols * rows, VillageMapCell{});
    solid.assign((size_t)cols * rows, 0);
}

const std::vector<VillageMapCell>& VillageMap::CellsFor(Layer layer) const
{
    switch (layer)
    {
    case Layer::Objects:  return objects;
    case Layer::Overhead: return overhead;
    default:              return ground;
    }
}

std::vector<VillageMapCell>& VillageMap::CellsFor(Layer layer)
{
    switch (layer)
    {
    case Layer::Objects:  return objects;
    case Layer::Overhead: return overhead;
    default:              return ground;
    }
}

bool VillageMap::Save(const std::string& mapName) const
{
    std::string path = "villagemap_" + mapName + ".txt";
    FILE* file = fopen(path.c_str(), "w");
    if (!file) return false;

    fprintf(file, "size %d %d\n", cols, rows);

    // One line per non-empty cell: <layerTag> col row sheet tileCol tileRow
    const char tags[3] = { 'g', 'o', 'h' };
    const std::vector<VillageMapCell>* layers[3] = { &ground, &objects, &overhead };
    for (int layerIdx = 0; layerIdx < 3; layerIdx++)
    {
        for (int r = 0; r < rows; r++)
            for (int c = 0; c < cols; c++)
            {
                const VillageMapCell& cell = (*layers[layerIdx])[Index(c, r)];
                if (cell.sheet < 0) continue;
                fprintf(file, "%c %d %d %d %d %d\n", tags[layerIdx],
                        c, r, cell.sheet, cell.col, cell.row);
            }
    }

    // Collision flags.
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
            if (solid[Index(c, r)])
                fprintf(file, "s %d %d\n", c, r);

    fclose(file);
    return true;
}

bool VillageMap::Load(const std::string& mapName)
{
    std::string path = "villagemap_" + mapName + ".txt";
    FILE* file = fopen(path.c_str(), "r");
    if (!file) return false;

    int fileCols = kDefaultCols, fileRows = kDefaultRows;
    if (fscanf(file, "size %d %d\n", &fileCols, &fileRows) != 2)
    {
        fclose(file);
        return false;
    }
    Resize(fileCols, fileRows);

    char tag;
    int c, r, sheet, tileCol, tileRow;
    while (fscanf(file, " %c", &tag) == 1)
    {
        if (tag == 's')
        {
            if (fscanf(file, "%d %d", &c, &r) != 2) break;
            if (InBounds(c, r)) solid[Index(c, r)] = 1;
            continue;
        }
        if (fscanf(file, "%d %d %d %d %d", &c, &r, &sheet, &tileCol, &tileRow) != 5)
            break;
        if (!InBounds(c, r)) continue;

        VillageMapCell cell;
        cell.sheet = (short)sheet;
        cell.col   = (short)tileCol;
        cell.row   = (short)tileRow;
        if      (tag == 'g') ground[Index(c, r)]   = cell;
        else if (tag == 'o') objects[Index(c, r)]  = cell;
        else if (tag == 'h') overhead[Index(c, r)] = cell;
    }

    fclose(file);
    return true;
}

void VillageMap::DrawLayer(Layer layer, const Texture2D* sheets, int sheetCount,
                           Vector2 originScreen, float scale, Color tint,
                           float screenW, float screenH) const
{
    const std::vector<VillageMapCell>& cells = CellsFor(layer);
    const float drawTile = kTileSize * scale;

    // Cull to the visible cell range so big maps stay cheap.
    int firstCol = (int)((0.f - originScreen.x) / drawTile) - 1;
    int firstRow = (int)((0.f - originScreen.y) / drawTile) - 1;
    int lastCol  = (int)((screenW - originScreen.x) / drawTile) + 1;
    int lastRow  = (int)((screenH - originScreen.y) / drawTile) + 1;
    if (firstCol < 0) firstCol = 0;
    if (firstRow < 0) firstRow = 0;
    if (lastCol >= cols) lastCol = cols - 1;
    if (lastRow >= rows) lastRow = rows - 1;

    for (int r = firstRow; r <= lastRow; r++)
    {
        for (int c = firstCol; c <= lastCol; c++)
        {
            const VillageMapCell& cell = cells[Index(c, r)];
            if (cell.sheet < 0 || cell.sheet >= sheetCount) continue;
            const Texture2D& sheet = sheets[cell.sheet];
            if (sheet.id == 0) continue;

            Rectangle src{ cell.col * (float)kTileSize, cell.row * (float)kTileSize,
                           (float)kTileSize, (float)kTileSize };
            Rectangle dst{ originScreen.x + c * drawTile, originScreen.y + r * drawTile,
                           drawTile, drawTile };
            DrawTexturePro(sheet, src, dst, Vector2{ 0.f, 0.f }, 0.f, tint);
        }
    }
}
