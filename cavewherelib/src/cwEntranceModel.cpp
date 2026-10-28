/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwEntranceModel.h"

//Our includes
#include "cwStation.h"

namespace {

//! The key an entrance's station name matches on, the same one
//! cwFixStationModel matches fixes on.
QString entranceKey(const QString& stationName)
{
    return cwStation::canonicalKey(stationName.trimmed());
}

}

cwEntranceModel::cwEntranceModel(QObject* parent) :
    QAbstractListModel(parent)
{
    //Derived from the model's own change signals, as
    //cwFixStationModel::fixedStationsChanged is.
    connect(this, &QAbstractItemModel::rowsInserted, this, &cwEntranceModel::entranceStationsChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &cwEntranceModel::entranceStationsChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &cwEntranceModel::entranceStationsChanged);
    connect(this, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex&, const QModelIndex&, const QList<int>& roles)
    {
        //Empty roles means "every role", which includes the name.
        if (roles.isEmpty() || roles.contains(StationNameRole)) {
            emit entranceStationsChanged();
        }
    });
}

int cwEntranceModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return m_entrances.size();
}

QModelIndex cwEntranceModel::index(int row, int column, const QModelIndex& parent) const
{
    return QAbstractListModel::index(row, column, parent);
}

QVariant cwEntranceModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entrances.size()) {
        return QVariant();
    }

    const cwEntrance& entrance = m_entrances.at(index.row());
    switch (role) {
    case StationNameRole: return entrance.stationName();
    case LabelRole:       return entrance.label();
    case IdRole:          return entrance.id();
    default:              return QVariant();
    }
}

bool cwEntranceModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entrances.size()) {
        return false;
    }

    cwEntrance& entrance = m_entrances[index.row()];
    switch (role) {
    case StationNameRole: {
        const QString name = value.toString();
        if (entrance.stationName() == name) {
            return false;
        }
        entrance.setStationName(name);
        break;
    }
    case LabelRole: {
        const QString label = value.toString();
        if (entrance.label() == label) {
            return false;
        }
        entrance.setLabel(label);
        break;
    }
    case IdRole: {
        const QUuid id = value.toUuid();
        if (entrance.id() == id) {
            return false;
        }
        entrance.setId(id);
        break;
    }
    default:
        return false;
    }

    emit dataChanged(index, index, {role});
    return true;
}

Qt::ItemFlags cwEntranceModel::flags(const QModelIndex& index) const
{
    const Qt::ItemFlags base = QAbstractListModel::flags(index);
    return index.isValid() ? base | Qt::ItemIsEditable : base;
}

QHash<int, QByteArray> cwEntranceModel::roleNames() const
{
    return {
        {StationNameRole, "stationName"},
        {LabelRole,       "label"},
        {IdRole,          "id"}
    };
}

int cwEntranceModel::indexOf(const QString& stationName) const
{
    const QString key = entranceKey(stationName);
    if (key.isEmpty()) {
        return -1;
    }
    for (int i = 0; i < m_entrances.size(); i++) {
        if (entranceKey(m_entrances.at(i).stationName()) == key) {
            return i;
        }
    }
    return -1;
}

int cwEntranceModel::indexOf(const QUuid& entranceId) const
{
    if (entranceId.isNull()) {
        return -1;
    }
    for (int i = 0; i < m_entrances.size(); i++) {
        if (m_entrances.at(i).id() == entranceId) {
            return i;
        }
    }
    return -1;
}

bool cwEntranceModel::isEntrance(const QString& stationName) const
{
    return indexOf(stationName) >= 0;
}

int cwEntranceModel::addEntrance(const QString& stationName)
{
    const QString trimmedName = stationName.trimmed();
    if (trimmedName.isEmpty()) {
        return -1;
    }

    const int existingRow = indexOf(trimmedName);
    if (existingRow >= 0) {
        return existingRow;
    }

    cwEntrance entrance;
    entrance.setStationName(trimmedName);
    appendEntrance(entrance);
    return m_entrances.size() - 1;
}

void cwEntranceModel::removeEntrance(const QString& stationName)
{
    removeAt(indexOf(stationName));
}

void cwEntranceModel::removeAt(int row)
{
    if (row < 0 || row >= m_entrances.size()) {
        return;
    }
    beginRemoveRows(QModelIndex(), row, row);
    m_entrances.removeAt(row);
    endRemoveRows();
    emit countChanged();
}

cwEntrance cwEntranceModel::entranceAt(int row) const
{
    if (row < 0 || row >= m_entrances.size()) {
        return cwEntrance();
    }
    return m_entrances.at(row);
}

void cwEntranceModel::appendEntrance(const cwEntrance& entrance)
{
    insertEntrance(m_entrances.size(), entrance);
}

void cwEntranceModel::insertEntrance(int row, const cwEntrance& entrance)
{
    const int clampedRow = qBound(0, row, static_cast<int>(m_entrances.size()));
    beginInsertRows(QModelIndex(), clampedRow, clampedRow);
    m_entrances.insert(clampedRow, entrance);
    endInsertRows();
    emit countChanged();
}

void cwEntranceModel::setEntrances(const QList<cwEntrance>& entrances)
{
    const bool sizeChanged = m_entrances.size() != entrances.size();
    beginResetModel();
    m_entrances = entrances;
    endResetModel();
    if (sizeChanged) {
        emit countChanged();
    }
}

int cwEntranceModel::count() const
{
    return m_entrances.size();
}
