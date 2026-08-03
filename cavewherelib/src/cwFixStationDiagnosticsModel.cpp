/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwFixStationDiagnosticsModel.h"

//Our includes
#include "cwCave.h"
#include "cwCoordinateText.h"
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

//! Empty when the row's coordinate can be read, otherwise why it can't. See
//! CoordinateErrorRole for why this and the domain message can't both speak.
QString coordinateErrorMessage(const cwFixStation& fix)
{
    switch (fix.state()) {
    case cwFixStation::Empty:
    case cwFixStation::Valid:
        return QString();
    case cwFixStation::NoSystem:
        return cwFixStationDiagnosticsModel::tr(
            "This coordinate has no coordinate system, so there is no way to tell "
            "what its numbers mean — choose the system they were measured in.");
    case cwFixStation::Unreadable:
        //The reason comes from the same parser that refused the text, rather
        //than being restated here where it would drift. Metric stands in for the
        //project's unit system exactly as cwFixStation does when it reads the
        //stored coordinate: a stored elevation spells its own unit out, so there
        //is nothing left for a unit system to resolve, and this is what makes
        //the message the same one the row was judged by.
        return cwFixStationDiagnosticsModel::tr("This coordinate can't be read: %1")
            .arg(cwCoordinateText::parse(fix.coordinate(),
                                         cwUnits::Metric,
                                         cwCoordinateText::axisOrderFor(fix.inputCS()))
                     .errorMessage());
    }
    return QString();
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
    // The domain verdict defers on any state but Valid, so it moves with the
    // state as well as with the numbers — and the state is a function of the
    // text. A text edit that leaves the components where they were still
    // changes it: editing an unreadable row to "0, 0, 0m" moves nothing (all
    // three were already 0) but turns a row with no domain verdict into one
    // that has, and could have, an out-of-domain origin.
    if (roles.contains(cwFixStationModel::EastingRole)
        || roles.contains(cwFixStationModel::NorthingRole)
        || roles.contains(cwFixStationModel::CoordinateTextRole)
        || roles.contains(cwFixStationModel::InputCSRole)) {
        derived.append(DomainErrorRole);
        derived.append(EastingDomainErrorRole);
        derived.append(NorthingDomainErrorRole);
    }
    // What the row's coordinate amounts to is a function of the text and the CS
    // together — naming a system turns unreadable text into a coordinate without
    // touching a character of it, and a component edit rewrites the text.
    if (roles.contains(cwFixStationModel::CoordinateTextRole)
        || roles.contains(cwFixStationModel::InputCSRole)) {
        derived.append(CoordinateErrorRole);
        derived.append(CoordinateOrderUnknownRole);
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
    case CoordinateErrorRole:
    case CoordinateOrderUnknownRole:
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
    case DomainErrorRole:            return domainErrorMessage(*fix);
    case CoordinateErrorRole:        return coordinateErrorMessage(*fix);
    case CoordinateOrderUnknownRole: return fix->state() == cwFixStation::NoSystem;
    case StationErrorRole:           return stationErrorMessage(*fix);
    default:                         break;
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
    names.insert(CoordinateErrorRole, "coordinateError");
    names.insert(CoordinateOrderUnknownRole, "coordinateOrderUnknown");
    names.insert(StationErrorRole, "stationError");
    return names;
}
