/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCOLLISIONRECTGRID_H
#define CWCOLLISIONRECTGRID_H

//Qt includes
#include <QRect>
#include <QSize>

//Std includes
#include <vector>

//Our includes
#include "CaveWhereLibExport.h"

/**
  \brief Uniform-grid collision detector for screen-space label rects.

  Each placed rect is linked into every grid cell it covers, so an insert only
  tests the rects that share a cell with it. The per-insert cost depends on the
  local label density, independent of how many rects are already placed.
  */
class CAVEWHERE_LIB_EXPORT cwCollisionRectGrid
{
public:
    cwCollisionRectGrid();

    // Empties the grid and covers `bounds` with cells of `cellSize`, reusing
    // storage. Cell sizes are clamped to kMinCellSize and grown so each axis
    // has at most kMaxCellsPerAxis cells; an empty `bounds` gives a single cell.
    void reset(const QRect& bounds, const QSize& cellSize);

    // Empties the grid, keeping bounds, cell size and storage.
    void clear();

    // Records `rect` and returns true when it intersects no placed rect
    // (QRect::intersects). Rects outside `bounds` map to the edge cells. An
    // empty rect returns true and blocks nothing.
    bool addRect(const QRect& rect);

    // Records `rect` without testing it, so it may overlap placed rects; later
    // addRect calls collide with it. An empty rect is ignored.
    void insertRect(const QRect& rect);

    // Rects recorded since the last reset or clear.
    int rectCount() const;
    QRect bounds() const;
    QSize cellSize() const;

    static constexpr int kMinCellSize = 1;
    static constexpr int kMaxCellsPerAxis = 1024;

private:
    struct CellRange {
        int firstColumn;
        int lastColumn;
        int firstRow;
        int lastRow;
    };

    struct Entry {
        QRect rect;
        int next;   // index of the next entry in the same cell, or -1
    };

    QRect m_bounds;
    QSize m_cellSize;
    int m_columns = 0;
    int m_rows = 0;
    int m_rectCount = 0;
    std::vector<int> m_cellHeads;   // columns * rows, -1 when empty
    std::vector<Entry> m_entries;   // one per cell a rect covers

    CellRange coveredCells(const QRect& rect) const;
    void link(const QRect& rect, const CellRange& cells);
};

#endif // CWCOLLISIONRECTGRID_H
