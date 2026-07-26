/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwFixStationModel.h"

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwCoordinateTransform.h"
#include "cwGeoPoint.h"
#include "cwGeoReference.h"
#include "cwSurveyNetwork.h"

namespace {

//! The input CS's domain verdict for a fix, once per row. A row that omits its
//! own CS falls back to the region's global CS, exactly as
//! cwFixStationValidator's Part-A check and cwCave::recomputeGridConvergence do
//! — otherwise the cave-level warning would fire with no cell to point at.
//! Unparseable or wholly absent CS defers (both components valid).
cwCoordinateTransform::DomainCheck domainCheck(const cwFixStation& fix,
                                               const QString& fallbackCS)
{
    QString cs = fix.inputCS().trimmed();
    if (cs.isEmpty()) {
        cs = fallbackCS.trimmed();
    }
    if (cs.isEmpty()) {
        return {};
    }
    const cwGeoPoint point(fix.easting(), fix.northing(), fix.elevation());
    return cwCoordinateTransform::domainCheck(cs, point);
}

//! Empty when the fix's coordinate is plausible for its CS, otherwise a one-line
//! explanation. Scoped to a single row: an absent or unparseable CS never flags,
//! so this only speaks up when the CS is known and the point is well outside its
//! declared area of use.
QString domainErrorMessage(const cwFixStation& fix, const QString& fallbackCS)
{
    const cwCoordinateTransform::DomainCheck check = domainCheck(fix, fallbackCS);
    if (check.eastingValid && check.northingValid) {
        return QString();
    }
    return cwFixStationModel::tr(
        "This coordinate is outside the valid range for its coordinate system "
        "— check for a transposed digit or the wrong CS/zone.");
}

}

cwFixStationModel::StationReference
cwFixStationModel::classifyStationReference(const QString& stationName,
                                            const cwSurveyNetwork& network)
{
    // Trim before matching, exactly as the survex export does
    // (cwSurvexExporterUtils::validateFixStations) — otherwise a stray trailing
    // space would report a station survex anchors just fine as missing.
    const QString trimmedName = stationName.trimmed();
    if (trimmedName.isEmpty()) {
        return StationReference::Empty;
    }
    // Nothing to check against — a cave whose survey network hasn't been
    // computed yet would flag every named fix, so defer instead.
    if (network.isEmpty()) {
        return StationReference::Ok;
    }
    // CaveWhere station names are case-insensitive (Compass isn't — see
    // CLAUDE.md); hasStation matches that way in one hash probe.
    return network.hasStation(trimmedName)
        ? StationReference::Ok
        : StationReference::Unknown;
}

bool cwFixStationModel::isErrorOnlyRoleChange(const QList<int>& roles)
{
    // Empty means "all roles" per Qt — a real edit, not error-only.
    if (roles.isEmpty()) {
        return false;
    }
    for (int role : roles) {
        switch (role) {
        case DomainErrorRole:
        case EastingDomainErrorRole:
        case NorthingDomainErrorRole:
        case StationErrorRole:
            break;
        default:
            return false;
        }
    }
    return true;
}

cwFixStationModel::cwFixStationModel(QObject* parent) :
    QAbstractListModel(parent)
{
}

cwFixStationModel::~cwFixStationModel() = default;

void cwFixStationModel::setCave(cwCave* cave)
{
    if (m_cave == cave) {
        return;
    }
    if (m_cave != nullptr) {
        // We only ever connect surveyNetworkChanged, so a blanket disconnect is
        // exactly our own wiring — without it a re-point would double-emit.
        disconnect(m_cave, nullptr, this, nullptr);
    }
    m_cave = cave;
    if (m_cave == nullptr) {
        return;
    }
    // A station appearing or disappearing in the survey flips StationErrorRole
    // for any row that named it, so refresh every row when the network changes.
    connect(m_cave, &cwCave::surveyNetworkChanged, this, [this] {
        refreshComputedErrors({StationErrorRole});
    });
}

void cwFixStationModel::refreshDomainErrors()
{
    refreshComputedErrors({DomainErrorRole, EastingDomainErrorRole, NorthingDomainErrorRole});
}

void cwFixStationModel::refreshComputedErrors(const QList<int>& roles)
{
    if (m_fixStations.isEmpty()) {
        return;
    }
    emit dataChanged(index(0), index(m_fixStations.size() - 1), roles);
}

QString cwFixStationModel::fallbackCS() const
{
    const cwCavingRegion* region = m_cave != nullptr ? m_cave->parentRegion() : nullptr;
    return region != nullptr ? region->geoReference()->globalCoordinateSystem() : QString();
}

QString cwFixStationModel::stationErrorMessage(const cwFixStation& fix) const
{
    if (m_cave == nullptr) {
        return QString();
    }
    switch (classifyStationReference(fix.stationName(), m_cave->network())) {
    case StationReference::Ok:
        return QString();
    case StationReference::Empty:
        return tr("This fix has no station name — enter the survey station it fixes.");
    case StationReference::Unknown:
        return tr("No survey station named \"%1\" in this cave — check the name.")
            .arg(fix.stationName().trimmed());
    }
    return QString();
}

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
    case DomainErrorRole:        return domainErrorMessage(fix, fallbackCS());
    case EastingDomainErrorRole:  return !domainCheck(fix, fallbackCS()).eastingValid;
    case NorthingDomainErrorRole: return !domainCheck(fix, fallbackCS()).northingValid;
    case StationErrorRole:       return stationErrorMessage(fix);
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
        QList<int> changedRoles{role};
        // The domain check reads the easting, northing, and input CS, so a
        // change to any of those can turn the row's domain roles on or off.
        if (role == EastingRole || role == NorthingRole || role == InputCSRole) {
            changedRoles.append(DomainErrorRole);
            changedRoles.append(EastingDomainErrorRole);
            changedRoles.append(NorthingDomainErrorRole);
        }
        // The station reference is judged from the station name.
        if (role == StationNameRole) {
            changedRoles.append(StationErrorRole);
        }
        emit dataChanged(index, index, changedRoles);
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
        {DomainErrorRole,        "domainError"},
        {EastingDomainErrorRole,  "eastingDomainError"},
        {NorthingDomainErrorRole, "northingDomainError"},
        {StationErrorRole,       "stationError"}
    };
}

void cwFixStationModel::addFixStation()
{
    appendFixStation(cwFixStation());
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
