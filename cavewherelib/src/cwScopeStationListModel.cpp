/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwScopeStationListModel.h"
#include "cwStation.h"

//Std includes
#include <algorithm>

cwScopeStationListModel::cwScopeStationListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int cwScopeStationListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_rows.size();
}

QVariant cwScopeStationListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return QVariant();
    }

    const Row& row = m_rows.at(index.row());
    switch (role) {
    case StationNameRole:
        return row.displayName;
    case QualifiedNameRole:
        return row.qualifiedName;
    case PositionRole:
        return row.position;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> cwScopeStationListModel::roleNames() const
{
    return {
        { StationNameRole, "stationName" },
        { QualifiedNameRole, "qualifiedName" },
        { PositionRole, "stationPosition" }
    };
}

void cwScopeStationListModel::setScopePrefix(const QString& scopePrefix)
{
    if (m_scopePrefix == scopePrefix) {
        return;
    }
    m_scopePrefix = scopePrefix;
    emit scopePrefixChanged();
    rebuildRows();
}

void cwScopeStationListModel::setNetwork(const cwSurveyNetwork& network)
{
    if (m_network == network) {
        return;
    }
    m_network = network;
    emit networkChanged();
    rebuildRows();
}

void cwScopeStationListModel::rebuildRows()
{
    beginResetModel();
    m_rows.clear();

    if (!m_scopePrefix.isEmpty()) {
        const QString prefix = cwStation::canonicalKey(m_scopePrefix);
        const QStringList stations = m_network.stations();
        for (const QString& qualifiedName : stations) {
            if (qualifiedName.startsWith(prefix)) {
                m_rows.append({ qualifiedName.mid(prefix.size()),
                                qualifiedName,
                                m_network.position(qualifiedName) });
            }
        }

        std::stable_sort(m_rows.begin(), m_rows.end(),
                         [](const Row& left, const Row& right) {
            return left.qualifiedName < right.qualifiedName;
        });
    }

    endResetModel();
}
