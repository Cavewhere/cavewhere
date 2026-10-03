/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwAttachedFixModel.h"
#include "cwFixStationDiagnosticsModel.h"
#include "cwFixStationModel.h"

//Std includes
#include <algorithm>
#include <iterator>
#include <limits>

cwAttachedFixModel::cwAttachedFixModel(QObject* parent) :
    QAbstractListModel(parent)
{
}

int cwAttachedFixModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : count();
}

QModelIndex cwAttachedFixModel::index(int row, int column, const QModelIndex& parent) const
{
    return QAbstractListModel::index(row, column, parent);
}

QVariant cwAttachedFixModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_fixes.size()) {
        return QVariant();
    }

    const cwAttachedFix& fix = m_fixes.at(index.row());
    switch (role) {
    case cwFixStationModel::StationNameRole:
        return fix.station;
    case cwFixStationModel::InputCSRole:
        return fix.coordinateSystem;
    case cwFixStationModel::CoordinateTextRole:
        return fix.coordinate;
    // The scan keeps the coordinate as the file writes it and reads no
    // components out of it, so those roles say "no value".
    case cwFixStationModel::EastingRole:
    case cwFixStationModel::NorthingRole:
    case cwFixStationModel::ElevationRole:
    case cwFixStationModel::HorizontalVarianceRole:
    case cwFixStationModel::VerticalVarianceRole:
        return std::numeric_limits<double>::quiet_NaN();
    case SourceFileRole:
        return fix.fileName;
    case ReadOnlyRole:
        return true;
    case cwFixStationDiagnosticsModel::DomainErrorRole:
    case cwFixStationDiagnosticsModel::CoordinateErrorRole:
    case cwFixStationDiagnosticsModel::StationErrorRole:
        return QString();
    case cwFixStationDiagnosticsModel::EastingDomainErrorRole:
    case cwFixStationDiagnosticsModel::NorthingDomainErrorRole:
    case cwFixStationDiagnosticsModel::CoordinateOrderUnknownRole:
    case cwFixStationDiagnosticsModel::DatumEnabledRole:
        return false;
    case cwFixStationDiagnosticsModel::AvailableDatumsRole:
        return QStringList();
    default:
        return QVariant();
    }
}

Qt::ItemFlags cwAttachedFixModel::flags(const QModelIndex& index) const
{
    return QAbstractListModel::flags(index) & ~Qt::ItemIsEditable;
}

QHash<int, QByteArray> cwAttachedFixModel::roleNames() const
{
    return {
        {cwFixStationModel::StationNameRole,        "stationName"},
        {cwFixStationModel::InputCSRole,            "inputCS"},
        {cwFixStationModel::EastingRole,            "easting"},
        {cwFixStationModel::NorthingRole,           "northing"},
        {cwFixStationModel::ElevationRole,          "elevation"},
        {cwFixStationModel::HorizontalVarianceRole, "horizontalVariance"},
        {cwFixStationModel::VerticalVarianceRole,   "verticalVariance"},
        {cwFixStationModel::CoordinateTextRole,     "coordinateText"},
        {SourceFileRole,                            "sourceFile"},
        {ReadOnlyRole,                              "readOnly"},
        {cwFixStationDiagnosticsModel::DomainErrorRole,            "domainError"},
        {cwFixStationDiagnosticsModel::EastingDomainErrorRole,     "eastingDomainError"},
        {cwFixStationDiagnosticsModel::NorthingDomainErrorRole,    "northingDomainError"},
        {cwFixStationDiagnosticsModel::CoordinateErrorRole,        "coordinateError"},
        {cwFixStationDiagnosticsModel::CoordinateOrderUnknownRole, "coordinateOrderUnknown"},
        {cwFixStationDiagnosticsModel::StationErrorRole,           "stationError"},
        {cwFixStationDiagnosticsModel::AvailableDatumsRole,        "availableDatums"},
        {cwFixStationDiagnosticsModel::DatumEnabledRole,           "datumEnabled"}
    };
}

void cwAttachedFixModel::setAttachedFixes(const QList<cwAttachedFix>& fixes)
{
    if (m_fixes == fixes) {
        return;
    }

    const int oldCount = count();
    beginResetModel();
    m_fixes = fixes;
    endResetModel();
    if (count() != oldCount) {
        emit countChanged();
    }
}

int cwAttachedFixModel::firstFixWithoutCoordinateSystem() const
{
    const auto bare = std::find_if(m_fixes.cbegin(), m_fixes.cend(), [](const cwAttachedFix& fix) {
        return fix.coordinateSystem.isEmpty();
    });
    return bare == m_fixes.cend() ? -1 : static_cast<int>(std::distance(m_fixes.cbegin(), bare));
}
