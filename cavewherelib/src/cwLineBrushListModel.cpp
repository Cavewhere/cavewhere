/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwLineBrushListModel.h"
#include "cwSymbologyPalette.h"

//Qt includes
#include <QHash>

//Std includes
#include <algorithm>

cwLineBrushListModel::cwLineBrushListModel(QObject *parent) :
    QAbstractListModel(parent)
{
}

void cwLineBrushListModel::setPalette(cwSymbologyPalette *palette)
{
    if (m_palette == palette) {
        return;
    }

    disconnect(m_paletteConnection);
    m_palette = palette;
    if (m_palette != nullptr) {
        // An in-place edit of the same palette object (setData) must re-skin
        // the picker; a manager reload that deletes the object instead arrives
        // as a fresh pointer through the resolvedPalette binding.
        m_paletteConnection = connect(m_palette, &cwSymbologyPalette::dataChanged,
                                      this, &cwLineBrushListModel::refresh);
    }

    refresh();
    emit paletteChanged();
}

void cwLineBrushListModel::refresh()
{
    beginResetModel();
    m_brushes = m_palette ? sortedForPicker(m_palette->lineBrushes()) : QVector<cwLineBrush>();
    endResetModel();
}

QVector<cwLineBrush> cwLineBrushListModel::sortedForPicker(QVector<cwLineBrush> brushes)
{
    // Each category sorts by the lowest zOrder among its members, so a category
    // whose brushes paint earliest (walls) leads the list.
    QHash<QString, int> categoryMinZOrder;
    for (const cwLineBrush &brush : brushes) {
        const auto it = categoryMinZOrder.find(brush.category);
        if (it == categoryMinZOrder.end()) {
            categoryMinZOrder.insert(brush.category, brush.zOrder);
        } else if (brush.zOrder < it.value()) {
            it.value() = brush.zOrder;
        }
    }

    std::stable_sort(brushes.begin(), brushes.end(),
                     [&categoryMinZOrder](const cwLineBrush &a, const cwLineBrush &b) {
        if (a.category != b.category) {
            // Every category was inserted from this same vector above, so the
            // lookups always hit.
            const int minA = categoryMinZOrder.value(a.category);
            const int minB = categoryMinZOrder.value(b.category);
            if (minA != minB) {
                return minA < minB;
            }
            // Same lowest zOrder in two categories — order the categories by
            // name so the result is deterministic across palette reloads.
            return QString::localeAwareCompare(a.category, b.category) < 0;
        }
        return QString::localeAwareCompare(a.displayName, b.displayName) < 0;
    });

    return brushes;
}

int cwLineBrushListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_brushes.size());
}

QVariant cwLineBrushListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_brushes.size()) {
        return QVariant();
    }

    const cwLineBrush &brush = m_brushes.at(index.row());
    switch (role) {
    case NameRole:
        return brush.name;
    case DisplayNameRole:
        return brush.displayName;
    case CategoryRole:
        return brush.category;
    case IsFirstInCategoryRole:
        return index.row() == 0
               || m_brushes.at(index.row() - 1).category != brush.category;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> cwLineBrushListModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {DisplayNameRole, "displayName"},
        {CategoryRole, "category"},
        {IsFirstInCategoryRole, "isFirstInCategory"}
    };
}
