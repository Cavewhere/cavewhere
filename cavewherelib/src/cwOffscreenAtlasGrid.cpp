//Our includes
#include "cwOffscreenAtlasGrid.h"

//Std includes
#include <algorithm>
#include <cmath>

namespace {
    constexpr int kColorBytesPerPixel = 4; // RGBA8, both atlas and scratch colour
}

QRect cwOffscreenAtlasGrid::tileRect(int index) const
{
    if (index < 0 || index >= capacity()) {
        return QRect();
    }
    const int col = index % columns;
    const int row = index / columns;
    return QRect(col * tileSize.width(), row * tileSize.height(),
                 tileSize.width(), tileSize.height());
}

cwOffscreenAtlasGrid cwOffscreenAtlasGrid::choose(QSize tileSize, int textureSizeMax,
                                                  qint64 byteBudget, int maxTilesPerFrame,
                                                  int depthBytesPerPixel)
{
    cwOffscreenAtlasGrid grid;
    grid.tileSize = tileSize;

    const int tileW = tileSize.width();
    const int tileH = tileSize.height();
    if (tileW <= 0 || tileH <= 0 || textureSizeMax <= 0 || maxTilesPerFrame <= 0
        || byteBudget <= 0) {
        return grid; // invalid inputs -> invalid grid (caller falls back)
    }

    // (a) Texture-dimension cap: how many tiles fit along each axis of one atlas.
    const int maxCols = textureSizeMax / tileW;
    const int maxRows = textureSizeMax / tileH;
    if (maxCols < 1 || maxRows < 1) {
        return grid; // a single tile exceeds the backend's max texture dimension
    }
    const qint64 textureCapacity = qint64(maxCols) * maxRows;

    // (b) Byte-budget cap: the colour-only RGBA8 atlas shares the budget with the one
    // reused scratch (RGBA8 colour + depth). Reserve the scratch, then see how many
    // atlas cells the remainder buys.
    const qint64 pixelsPerTile = qint64(tileW) * tileH;
    const qint64 scratchBytes = pixelsPerTile * (kColorBytesPerPixel + depthBytesPerPixel);
    const qint64 atlasBytesPerTile = pixelsPerTile * kColorBytesPerPixel;
    const qint64 budgetCapacity = (byteBudget - scratchBytes) / atlasBytesPerTile;
    if (budgetCapacity < 1) {
        return grid; // budget can't hold the scratch plus even one atlas cell
    }

    // The smallest of the three caps bounds the cell count. All three are >= 1 here (the
    // guards above returned otherwise), so cap >= 1.
    const qint64 cap = std::min({ textureCapacity, budgetCapacity, qint64(maxTilesPerFrame) });

    // Lay the cells out as square-ish as the per-axis texture caps allow, keeping the
    // atlas dimension (and so the single read-back) small. With cap >= 1 and maxCols/maxRows
    // >= 1, both come out >= 1: floor(sqrt(cap)) >= 1, and columns <= sqrt(cap) gives
    // cap / columns >= 1. columns*rows <= cap by construction (integer floor on rows).
    const int columns = std::min<qint64>(maxCols, qint64(std::floor(std::sqrt(double(cap)))));
    const int rows = std::min<qint64>(maxRows, cap / columns);

    grid.columns = columns;
    grid.rows = rows;
    return grid;
}
