/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwFixStationModel.h"

//Our includes
#include "cwCoordinateText.h"
#include "cwCoordinateTransform.h"
#include "cwStation.h"

namespace {

//! A row created through the UI, which always starts on a coordinate system.
//! Without one there is no axis order, so a fix's numbers can't be said to mean
//! anything (#625) — a new row picks the system that needs no further choice
//! rather than starting blank. Deliberately not cwFixStation's own default: a
//! row that really does arrive blank, from an old file or a hand edit, has to
//! stay blank so it can be flagged instead of silently reinterpreted.
cwFixStation newFixStation()
{
    cwFixStation fix;
    fix.setInputCS(cwCoordinateTransform::Wgs84);
    return fix;
}

//! The key a fix's station name matches on. See cwFixStationModel::indexOf().
QString fixKey(const QString& stationName)
{
    return cwStation::canonicalKey(stationName.trimmed());
}

//! The roles whose value differs between \a before and \a after.
//!
//! Every one of these is a view of the same coordinate string, so one write
//! moves several: setting a component spells the coordinate back out, and
//! changing the CS re-reads it under the other axis order. Comparing the two
//! fixes is what keeps the emission honest without each mutator having to know
//! what else it disturbed.
QList<int> movedCoordinateRoles(const cwFixStation& before, const cwFixStation& after)
{
    QList<int> roles;
    if (after.coordinate() != before.coordinate()) {
        roles.append(cwFixStationModel::CoordinateTextRole);
    }
    if (after.easting() != before.easting()) {
        roles.append(cwFixStationModel::EastingRole);
    }
    if (after.northing() != before.northing()) {
        roles.append(cwFixStationModel::NorthingRole);
    }
    if (after.elevation() != before.elevation()) {
        roles.append(cwFixStationModel::ElevationRole);
    }
    return roles;
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
    case CoordinateTextRole:     return fix.coordinate();
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
    //Shares the row's data block until the write below detaches it, so this is
    //a cheap snapshot rather than a copy of the fix.
    const cwFixStation before = fix;

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
        QList<int> roles = movedCoordinateRoles(before, fix);
        if (!roles.contains(role)) {
            roles.prepend(role);
        }
        emit dataChanged(index, index, roles);
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
        {IdRole,                 "id"},
        {CoordinateTextRole,     "coordinateText"}
    };
}

void cwFixStationModel::addFixStation()
{
    appendFixStation(newFixStation());
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

    cwFixStation fix = newFixStation();
    fix.setStationName(trimmedName);
    appendFixStation(fix);
    return m_fixStations.size() - 1;
}

QString cwFixStationModel::setCoordinateText(int row,
                                             const QString& text,
                                             cwUnits::UnitSystem units)
{
    if (row < 0 || row >= m_fixStations.size()) {
        return QString();
    }

    //Copied rather than referenced: writing the row below detaches the list
    //when it is shared, which would leave a reference into the old block.
    const cwFixStation before = m_fixStations.at(row);
    const cwCoordinateText::AxisOrder order = cwCoordinateText::axisOrderFor(before.inputCS());

    //Parsed here as well as inside the fix, because this is the surface that
    //gets to refuse: a coordinate that can't be read never becomes one. What
    //the fix does with text it can't read is a different question, answered on
    //the load path, where the text is already the user's (see cwFixStation).
    const auto result = cwCoordinateText::parse(text, units, order);
    if (result.hasError()) {
        return result.errorMessage();
    }

    //Normalized before it is stored, which is what makes re-typing "304"
    //against a stored "304m" a no-op: the unit was spelled out when the string
    //was first kept, not added by an edit.
    const QString typed = cwCoordinateText::textToStore(text, result.value(), units);
    if (typed == before.coordinate()) {
        return QString();
    }

    //A row with no coordinate still renders — as three zeros — and both editors
    //open on that rendering, so opening the cell and leaving it offers
    //"0, 0, 0m" straight back. Taking it would turn "not entered" into "entered
    //at the origin", which is the distinction the load path goes out of its way
    //to keep, and would dirty the project and re-solve for a gesture that typed
    //nothing.
    if (before.state() == cwFixStation::Empty
            && typed == cwCoordinateText::format(0.0, 0.0, 0.0, units, order)) {
        return QString();
    }

    m_fixStations[row].setCoordinate(typed);

    const QModelIndex changed = index(row);
    emit dataChanged(changed, changed, movedCoordinateRoles(before, m_fixStations.at(row)));
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
