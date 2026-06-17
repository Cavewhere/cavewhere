/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSymbologyPaletteListModel.h"
#include "cwSymbologyPaletteManager.h"
#include "cwSymbologyPalette.h"

cwSymbologyPaletteListModel::cwSymbologyPaletteListModel(QObject *parent) :
    QAbstractListModel(parent)
{
}

void cwSymbologyPaletteListModel::setManager(cwSymbologyPaletteManager *manager)
{
    if (m_manager == manager) {
        return;
    }

    disconnect(m_managerConnection);
    m_manager = manager;
    if (m_manager != nullptr) {
        m_managerConnection = connect(m_manager, &cwSymbologyPaletteManager::palettesChanged,
                                      this, &cwSymbologyPaletteListModel::refresh);
    }

    refresh();
    emit managerChanged();
}

void cwSymbologyPaletteListModel::refresh()
{
    beginResetModel();
    m_palettes = m_manager ? m_manager->palettes() : QList<cwSymbologyPalette *>();
    endResetModel();
}

int cwSymbologyPaletteListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_palettes.size());
}

QVariant cwSymbologyPaletteListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_palettes.size()) {
        return QVariant();
    }

    const cwSymbologyPalette *palette = m_palettes.at(index.row());
    switch (role) {
    case NameRole:
        return palette->name();
    case IdRole:
        return palette->id();
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> cwSymbologyPaletteListModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {IdRole, "id"}
    };
}
