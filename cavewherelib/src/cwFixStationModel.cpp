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
    case CoordinateTextRole:     return fix.coordinateText();
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
    const bool hadCoordinateText = !fix.coordinateText().isEmpty();

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
        QList<int> roles = {role};
        //Writing a component drops the stored coordinate text (see
        //cwFixStation), so a view offering that text has to hear about it.
        if (hadCoordinateText && fix.coordinateText().isEmpty()) {
            roles.append(CoordinateTextRole);
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

    //Copied rather than referenced: writing the row below detaches the list
    //when it is shared, which would leave a reference into the old block.
    const cwFixStation current = m_fixStations.at(row);

    const auto result = cwCoordinateText::parse(text, units, order);
    if (result.hasError()) {
        return result.errorMessage();
    }

    const cwCoordinateText::Coordinate coordinate = result.value();
    const QString typed = cwCoordinateText::textToStore(text, coordinate, units);

    //The row's own string handed straight back — the editor opens on it, so this
    //is the commonest commit there is. Compared in stored form, which is what
    //makes re-typing "304" against a stored "304m" a no-op as well: the unit was
    //spelled out when the string was first kept, not added by an edit. Identity
    //has to include the axis order, though: once the row's CS flips between
    //geographic and projected the same text means a different coordinate, and
    //re-committing it byte-identical is exactly the gesture that corrects it.
    if (typed == current.coordinateText() && order == current.coordinateTextAxisOrder()) {
        return QString();
    }

    //Text that reads back as what this row already renders is the machine's
    //string rather than the user's, and a row that stores none renders exactly
    //that anyway — so keep none. This is also what makes opening a cell and
    //leaving it free: the numbers are left alone instead of being round-tripped
    //through a unit conversion, and m→ft→m is not an identity in IEEE
    //arithmetic (it lands one ulp out for about an eighth of all doubles).
    const bool rendersRowAsIs =
        (typed == cwCoordinateText::format(current.easting(), current.northing(),
                                           current.elevation(), units, order));

    //Two components say nothing about elevation — that is the shape a coordinate
    //copied from a map arrives in — so the one already on the fix stands. To
    //clear it the user types it: "46.2, -115.6, 0".
    const double elevation = coordinate.hasElevation ? coordinate.elevation
                                                     : current.elevation();

    const bool componentsChanged = !rendersRowAsIs
                                   && (current.easting() != coordinate.easting
                                       || current.northing() != coordinate.northing
                                       || current.elevation() != elevation);

    //The pair as cwFixStation will actually hold it — an empty text takes the
    //order back to the default with it — so this compares what would be stored
    //against what is stored, rather than restating that normalization here.
    const QString keptText = rendersRowAsIs ? QString() : typed;
    const cwCoordinateText::AxisOrder keptOrder =
        keptText.isEmpty() ? cwCoordinateText::EastingNorthing : order;
    const bool textChanged = keptText != current.coordinateText()
                             || keptOrder != current.coordinateTextAxisOrder();

    if (!componentsChanged && !textChanged) {
        return QString();
    }

    cwFixStation& fix = m_fixStations[row];
    QList<int> roles;
    if (componentsChanged) {
        fix.setEasting(coordinate.easting);
        fix.setNorthing(coordinate.northing);
        fix.setElevation(elevation);
        roles = {EastingRole, NorthingRole, ElevationRole};
    }

    //After the components, never before: each of those setters drops the stored
    //text, which is the point of them (see cwFixStation).
    fix.setCoordinateText(keptText, order);
    if (textChanged) {
        roles.append(CoordinateTextRole);
    }

    const QModelIndex changed = index(row);
    emit dataChanged(changed, changed, roles);
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
