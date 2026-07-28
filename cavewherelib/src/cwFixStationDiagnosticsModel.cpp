/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwFixStationDiagnosticsModel.h"

//Our includes
#include "cwCave.h"
#include "cwCoordinateTransform.h"
#include "cwFixStation.h"
#include "cwFixStationDiagnostics.h"
#include "cwFixStationModel.h"

using cwFixStationDiagnostics::StationReference;

namespace {

//! Empty when the fix's coordinate is plausible for its CS, otherwise a one-line
//! explanation. Scoped to a single row: an absent or unparseable CS never flags,
//! so this only speaks up when the CS is known and the point is well outside its
//! declared area of use.
QString domainErrorMessage(const cwFixStation& fix)
{
    if (cwFixStationDiagnostics::isDomainValid(fix)) {
        return QString();
    }
    return cwFixStationDiagnosticsModel::tr(
        "This coordinate is outside the valid range for its coordinate system "
        "— check for a transposed digit or the wrong CS/zone.");
}

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

QString cwFixStationDiagnosticsModel::stationErrorMessage(const cwFixStation& fix) const
{
    if (m_cave == nullptr) {
        return QString();
    }
    switch (cwFixStationDiagnostics::classifyStationReference(fix.stationName(),
                                                             m_cave->network())) {
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
    case DomainErrorRole:  return domainErrorMessage(*fix);
    case StationErrorRole: return stationErrorMessage(*fix);
    default:               break;
    }

    // Both coordinate flags come from one verdict rather than one each. data() is
    // still entered once per role, so a row showing all three domain roles asks
    // three times; this only keeps that from being four. The check is cached per
    // (CS, coordinate) inside cwCoordinateTransform, so the repeats are hits.
    const cwCoordinateTransform::DomainCheck check =
        cwFixStationDiagnostics::domainCheck(*fix);
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
