/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwCollisionRectGrid.h"

//Std includes
#include <algorithm>

namespace {
constexpr int kEmptyChain = -1;

// Cell index of `coordinate` along one axis, clamped to [0, cellCount - 1].
// 64-bit math keeps extreme QRect coordinates from overflowing.
int clampedCell(int coordinate, int origin, int cellLength, int cellCount)
{
    const qint64 offset = qint64(coordinate) - qint64(origin);
    const qint64 cell = offset >= 0 ? offset / cellLength : -1;
    return int(std::clamp<qint64>(cell, 0, cellCount - 1));
}

int cellCountFor(int length, int cellLength)
{
    return int((qint64(length) + cellLength - 1) / cellLength);
}

// Requested cell length, grown so `length` spans at most kMaxCellsPerAxis cells.
int cellLengthFor(int length, int requestedCellLength)
{
    const int cappedCellLength = cellCountFor(std::max(0, length), cwCollisionRectGrid::kMaxCellsPerAxis);
    return std::max({cwCollisionRectGrid::kMinCellSize, requestedCellLength, cappedCellLength});
}
}

cwCollisionRectGrid::cwCollisionRectGrid()
{
    reset(QRect(), QSize(kMinCellSize, kMinCellSize));
}

void cwCollisionRectGrid::reset(const QRect& bounds, const QSize& cellSize)
{
    m_bounds = bounds;
    m_cellSize = QSize(cellLengthFor(bounds.width(), cellSize.width()),
                       cellLengthFor(bounds.height(), cellSize.height()));

    if (bounds.isEmpty()) {
        m_columns = 1;
        m_rows = 1;
    } else {
        m_columns = cellCountFor(bounds.width(), m_cellSize.width());
        m_rows = cellCountFor(bounds.height(), m_cellSize.height());
    }

    clear();
}

void cwCollisionRectGrid::clear()
{
    m_cellHeads.assign(size_t(m_columns) * size_t(m_rows), kEmptyChain);
    m_entries.clear();
    m_rectCount = 0;
}

bool cwCollisionRectGrid::addRect(const QRect& rect)
{
    if (rect.isEmpty()) {
        return true;
    }

    const int firstColumn = clampedCell(rect.left(), m_bounds.left(), m_cellSize.width(), m_columns);
    const int lastColumn = clampedCell(rect.right(), m_bounds.left(), m_cellSize.width(), m_columns);
    const int firstRow = clampedCell(rect.top(), m_bounds.top(), m_cellSize.height(), m_rows);
    const int lastRow = clampedCell(rect.bottom(), m_bounds.top(), m_cellSize.height(), m_rows);

    // Visits each covered cell's chain head; stops early when `visit` returns false.
    const auto visitCoveredCells = [&](auto&& visit) {
        for (int row = firstRow; row <= lastRow; ++row) {
            const size_t rowStart = size_t(row) * size_t(m_columns);
            for (int column = firstColumn; column <= lastColumn; ++column) {
                if (!visit(m_cellHeads[rowStart + column])) {
                    return false;
                }
            }
        }
        return true;
    };

    const bool isClear = visitCoveredCells([&](const int& head) {
        for (int entry = head; entry != kEmptyChain; entry = m_entries[entry].next) {
            if (m_entries[entry].rect.intersects(rect)) {
                return false;
            }
        }
        return true;
    });
    if (!isClear) {
        return false;
    }

    visitCoveredCells([&](int& head) {
        m_entries.push_back(Entry{rect, head});
        head = int(m_entries.size()) - 1;
        return true;
    });

    ++m_rectCount;
    return true;
}

int cwCollisionRectGrid::rectCount() const
{
    return m_rectCount;
}

QRect cwCollisionRectGrid::bounds() const
{
    return m_bounds;
}

QSize cwCollisionRectGrid::cellSize() const
{
    return m_cellSize;
}
