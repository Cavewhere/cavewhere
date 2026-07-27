/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwFixStationModel.h"

//Our includes
#include "cwCoordinateText.h"
#include "cwStation.h"

namespace {

//! The key a fix's station name matches on. See cwFixStationModel::indexOf().
QString fixKey(const QString& stationName)
{
    return cwStation::canonicalKey(stationName.trimmed());
}

}

cwFixStationModel::cwFixStationModel(QObject* parent) :
    QAbstractListModel(parent)
{
    //Derived from the model's own change signals rather than emitted by each
    //mutator: a future insert, remove or bulk edit can't forget to fire it, and
    //it can't run ahead of the dataChanged that carries the new name.
    connect(this, &QAbstractItemModel::rowsInserted, this, &cwFixStationModel::fixedStationsChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &cwFixStationModel::fixedStationsChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &cwFixStationModel::fixedStationsChanged);
    connect(this, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex&, const QModelIndex&, const QList<int>& roles)
    {
        //Empty roles means "every role", which includes the name.
        if(roles.isEmpty() || roles.contains(StationNameRole)) {
            emit fixedStationsChanged();
        }
    });
}

cwFixStationModel::~cwFixStationModel() = default;

int cwFixStationModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return m_fixStations.size();
}

QModelIndex cwFixStationModel::index(int row, int column, const QModelIndex& parent) const
{
    return QAbstractListModel::index(row, column, parent);
}

QVariant cwFixStationModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_fixStations.size()) {
        return QVariant();
    }

    const cwFixStation& fix = m_fixStations.at(index.row());
    switch (role) {
    case StationNameRole:        return fix.stationName();
    case InputCSRole:            return fix.inputCS();
    case EastingRole:            return fix.easting();
    case NorthingRole:           return fix.northing();
    case ElevationRole:          return fix.elevation();
    case HorizontalVarianceRole: return fix.horizontalVariance();
    case VerticalVarianceRole:   return fix.verticalVariance();
    case IdRole:                 return fix.id();
    default:                     return QVariant();
    }
}

bool cwFixStationModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_fixStations.size()) {
        return false;
    }

    cwFixStation& fix = m_fixStations[index.row()];
    bool changed = false;

    switch (role) {
    case StationNameRole: {
        const QString s = value.toString();
        if (fix.stationName() != s) {
            fix.setStationName(s);
            changed = true;
        }
        break;
    }
    case InputCSRole: {
        const QString s = value.toString();
        if (fix.inputCS() != s) {
            fix.setInputCS(s);
            changed = true;
        }
        break;
    }
    case EastingRole: {
        const double v = value.toDouble();
        if (fix.easting() != v) {
            fix.setEasting(v);
            changed = true;
        }
        break;
    }
    case NorthingRole: {
        const double v = value.toDouble();
        if (fix.northing() != v) {
            fix.setNorthing(v);
            changed = true;
        }
        break;
    }
    case ElevationRole: {
        const double v = value.toDouble();
        if (fix.elevation() != v) {
            fix.setElevation(v);
            changed = true;
        }
        break;
    }
    case HorizontalVarianceRole: {
        const double v = value.toDouble();
        if (fix.horizontalVariance() != v) {
            fix.setHorizontalVariance(v);
            changed = true;
        }
        break;
    }
    case VerticalVarianceRole: {
        const double v = value.toDouble();
        if (fix.verticalVariance() != v) {
            fix.setVerticalVariance(v);
            changed = true;
        }
        break;
    }
    case IdRole: {
        const QUuid id = value.toUuid();
        if (fix.id() != id) {
            fix.setId(id);
            changed = true;
        }
        break;
    }
    default:
        return false;
    }

    if (changed) {
        emit dataChanged(index, index, {role});
    }
    return changed;
}

QHash<int, QByteArray> cwFixStationModel::roleNames() const
{
    return {
        {StationNameRole,        "stationName"},
        {InputCSRole,            "inputCS"},
        {EastingRole,            "easting"},
        {NorthingRole,           "northing"},
        {ElevationRole,          "elevation"},
        {HorizontalVarianceRole, "horizontalVariance"},
        {VerticalVarianceRole,   "verticalVariance"},
        {IdRole,                 "id"}
    };
}

void cwFixStationModel::addFixStation()
{
    appendFixStation(cwFixStation());
}

int cwFixStationModel::indexOf(const QString& stationName) const
{
    const QString key = fixKey(stationName);
    if (key.isEmpty()) {
        return -1;
    }
    for (int i = 0; i < m_fixStations.size(); i++) {
        if (fixKey(m_fixStations.at(i).stationName()) == key) {
            return i;
        }
    }
    return -1;
}

bool cwFixStationModel::isFixed(const QString& stationName) const
{
    return indexOf(stationName) >= 0;
}

int cwFixStationModel::addFixStation(const QString& stationName)
{
    const QString trimmedName = stationName.trimmed();
    if (trimmedName.isEmpty()) {
        return -1;
    }

    const int existingRow = indexOf(trimmedName);
    if (existingRow >= 0) {
        return existingRow;
    }

    cwFixStation fix;
    fix.setStationName(trimmedName);
    appendFixStation(fix);
    return m_fixStations.size() - 1;
}

QString cwFixStationModel::setCoordinateText(int row,
                                             const QString& text,
                                             cwUnits::UnitSystem units,
                                             cwCoordinateText::AxisOrder order)
{
    if (row < 0 || row >= m_fixStations.size()) {
        return QString();
    }

    //Text the field would have rendered anyway is a no-op, checked before the
    //numbers because the numbers can't see it: the elevation crosses a unit
    //conversion in each direction, and (meters/0.3048)*0.3048 differs from
    //meters for about an eighth of all doubles. Without this, opening an
    //imperial project's coordinate cell and leaving it untouched would rewrite
    //the fix, dirty the project and re-solve the line plot.
    const cwFixStation& current = m_fixStations.at(row);
    if (text == cwCoordinateText::format(current.easting(), current.northing(),
                                         current.elevation(), units, order)) {
        return QString();
    }

    const auto result = cwCoordinateText::parse(text, units, order);
    if (result.hasError()) {
        return result.errorMessage();
    }

    const cwCoordinateText::Coordinate coordinate = result.value();
    cwFixStation& fix = m_fixStations[row];

    //Two components say nothing about elevation — that is the shape a coordinate
    //copied from a map arrives in — so the one already on the fix stands. To
    //clear it the user types it: "46.2, -115.6, 0".
    const double elevation = coordinate.hasElevation ? coordinate.elevation : fix.elevation();

    if (fix.easting() == coordinate.easting
        && fix.northing() == coordinate.northing
        && fix.elevation() == elevation) {
        return QString();
    }

    fix.setEasting(coordinate.easting);
    fix.setNorthing(coordinate.northing);
    fix.setElevation(elevation);

    const QModelIndex changed = index(row);
    emit dataChanged(changed, changed, {EastingRole, NorthingRole, ElevationRole});
    return QString();
}

void cwFixStationModel::removeFixStation(const QString& stationName)
{
    removeAt(indexOf(stationName));
}

void cwFixStationModel::appendFixStation(const cwFixStation& fix)
{
    const int row = m_fixStations.size();
    beginInsertRows(QModelIndex(), row, row);
    m_fixStations.append(fix);
    endInsertRows();
    emit countChanged();
}

void cwFixStationModel::removeAt(int index)
{
    if (index < 0 || index >= m_fixStations.size()) {
        return;
    }
    beginRemoveRows(QModelIndex(), index, index);
    m_fixStations.removeAt(index);
    endRemoveRows();
    emit countChanged();
}

cwFixStation cwFixStationModel::fixStationAt(int index) const
{
    if (index < 0 || index >= m_fixStations.size()) {
        return cwFixStation();
    }
    return m_fixStations.at(index);
}

void cwFixStationModel::setFixStations(const QList<cwFixStation>& fixes)
{
    const bool sizeChanged = (m_fixStations.size() != fixes.size());
    beginResetModel();
    m_fixStations = fixes;
    endResetModel();
    if (sizeChanged) {
        emit countChanged();
    }
}
