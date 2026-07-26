/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwFixStationDiagnosticsModel.h"

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwCoordinateTransform.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwGeoPoint.h"
#include "cwGeoReference.h"
#include "cwSurveyNetwork.h"
#include "cwSurvexExporterUtils.h"

namespace {

//! The input CS's domain verdict for a fix, once per row. Resolves the CS
//! through cwSurvexExporterUtils::resolveFixCS so a row's warning is judged
//! against the very CS the survex export would anchor it with — the cave-level
//! check in cwFixStationValidator and cwCave::recomputeGridConvergence resolve
//! it the same way. Unparseable or wholly absent CS defers (both valid).
cwCoordinateTransform::DomainCheck domainCheck(const cwFixStation& fix,
                                               const QString& fallbackCS)
{
    const QString cs = cwSurvexExporterUtils::resolveFixCS(fix, fallbackCS);
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
    return cwFixStationDiagnosticsModel::tr(
        "This coordinate is outside the valid range for its coordinate system "
        "— check for a transposed digit or the wrong CS/zone.");
}

}

cwFixStationDiagnosticsModel::StationReference
cwFixStationDiagnosticsModel::classifyStationReference(const QString& stationName,
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

cwFixStationDiagnosticsModel::cwFixStationDiagnosticsModel(cwCave* cave) :
    QIdentityProxyModel(cave),
    m_fixStations(cave->fixStations()),
    m_cave(cave)
{
    QIdentityProxyModel::setSourceModel(m_fixStations);

    connect(m_fixStations, &QAbstractItemModel::dataChanged,
            this, &cwFixStationDiagnosticsModel::augmentSourceChange);

    // A station appearing or disappearing in the survey flips StationErrorRole
    // for any row that named it, so refresh every row when the network changes.
    connect(m_cave, &cwCave::surveyNetworkChanged, this, [this] {
        refreshRoles({StationErrorRole});
    });
}

cwFixStationDiagnosticsModel::~cwFixStationDiagnosticsModel() = default;

void cwFixStationDiagnosticsModel::refreshDomainErrors()
{
    refreshRoles({DomainErrorRole, EastingDomainErrorRole, NorthingDomainErrorRole});
}

void cwFixStationDiagnosticsModel::augmentSourceChange(const QModelIndex& topLeft,
                                                       const QModelIndex& bottomRight,
                                                       const QList<int>& roles)
{
    // Empty means "every role" per Qt, and the base class already forwarded it
    // verbatim — that covers ours, so a second emit would only duplicate work.
    if (roles.isEmpty()) {
        return;
    }

    QList<int> derived;
    if (roles.contains(cwFixStationModel::EastingRole)
        || roles.contains(cwFixStationModel::NorthingRole)
        || roles.contains(cwFixStationModel::InputCSRole)) {
        derived.append(DomainErrorRole);
        derived.append(EastingDomainErrorRole);
        derived.append(NorthingDomainErrorRole);
    }
    if (roles.contains(cwFixStationModel::StationNameRole)) {
        derived.append(StationErrorRole);
    }
    if (derived.isEmpty()) {
        return;
    }
    emit dataChanged(mapFromSource(topLeft), mapFromSource(bottomRight), derived);
}

void cwFixStationDiagnosticsModel::refreshRoles(const QList<int>& roles)
{
    const int rows = rowCount();
    if (rows == 0) {
        return;
    }
    emit dataChanged(index(0, 0), index(rows - 1, 0), roles);
}

QString cwFixStationDiagnosticsModel::fallbackCS() const
{
    const cwCavingRegion* region = m_cave != nullptr ? m_cave->parentRegion() : nullptr;
    return region != nullptr ? region->geoReference()->globalCoordinateSystem() : QString();
}

QString cwFixStationDiagnosticsModel::stationErrorMessage(const cwFixStation& fix) const
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

const cwFixStation* cwFixStationDiagnosticsModel::fixAt(const QModelIndex& proxyIndex) const
{
    if (m_fixStations == nullptr || !proxyIndex.isValid()) {
        return nullptr;
    }
    const QList<cwFixStation>& fixes = m_fixStations->fixStations();
    const int row = proxyIndex.row();
    if (row < 0 || row >= fixes.size()) {
        return nullptr;
    }
    return &fixes.at(row);
}

QModelIndex cwFixStationDiagnosticsModel::index(int row, int column, const QModelIndex& parent) const
{
    return QIdentityProxyModel::index(row, column, parent);
}

QVariant cwFixStationDiagnosticsModel::data(const QModelIndex& index, int role) const
{
    switch (role) {
    case DomainErrorRole:
    case EastingDomainErrorRole:
    case NorthingDomainErrorRole:
    case StationErrorRole:
        break;
    default:
        return QIdentityProxyModel::data(index, role);
    }

    const cwFixStation* fix = fixAt(index);
    if (fix == nullptr) {
        return QVariant();
    }

    switch (role) {
    case DomainErrorRole:  return domainErrorMessage(*fix, fallbackCS());
    case StationErrorRole: return stationErrorMessage(*fix);
    default:               break;
    }

    // Both coordinate flags come from one verdict — the check resolves the CS
    // and round-trips the point through PROJ, so asking twice per row would
    // double that work for every visible row on every refresh.
    const cwCoordinateTransform::DomainCheck check = domainCheck(*fix, fallbackCS());
    return role == EastingDomainErrorRole ? !check.eastingValid : !check.northingValid;
}

QHash<int, QByteArray> cwFixStationDiagnosticsModel::roleNames() const
{
    QHash<int, QByteArray> names = QIdentityProxyModel::roleNames();
    names.insert(DomainErrorRole, "domainError");
    names.insert(EastingDomainErrorRole, "eastingDomainError");
    names.insert(NorthingDomainErrorRole, "northingDomainError");
    names.insert(StationErrorRole, "stationError");
    return names;
}
