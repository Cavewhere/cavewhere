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
#include <utility>

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
        return fix.systemLabel();
    case cwFixStationModel::CoordinateTextRole:
        return fix.coordinate;
    case cwFixStationModel::EastingRole:
        return m_gridPositions.at(index.row()).x;
    case cwFixStationModel::NorthingRole:
        return m_gridPositions.at(index.row()).y;
    case cwFixStationModel::ElevationRole:
        return m_gridPositions.at(index.row()).z;
    // The file's standard errors stay in its text; the row has no variance.
    case cwFixStationModel::HorizontalVarianceRole:
    case cwFixStationModel::VerticalVarianceRole:
        return std::numeric_limits<double>::quiet_NaN();
    case SourceFileRole:
        return fix.fileName;
    case ReadOnlyRole:
        return true;
    case ReadInWarningRole:
        return fix.readInWarning();
    case ReferenceRole:
        return fix.reference;
    case ReadInSystemRole:
        return fix.readInSystem;
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
        {ReadInWarningRole,                         "readInWarning"},
        {ReferenceRole,                             "reference"},
        {ReadInSystemRole,                          "readInSystem"},
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

    // The same stations in the same order only changed what is said about
    // them (a new system chosen for the file's bare fixes, a renamed scope),
    // so the rows on screen stay and update in place.
    const auto sameStation = [](const cwAttachedFix& a, const cwAttachedFix& b) {
        return a.ownerId == b.ownerId && a.filePath == b.filePath && a.line == b.line;
    };
    if (std::equal(m_fixes.cbegin(), m_fixes.cend(), fixes.cbegin(), fixes.cend(), sameStation)) {
        m_fixes = fixes;
        readGridPositions();
        emit dataChanged(index(0), index(count() - 1));
        return;
    }

    const int oldCount = count();
    beginResetModel();
    m_fixes = fixes;
    readGridPositions();
    endResetModel();
    if (count() != oldCount) {
        emit countChanged();
    }
}

int cwAttachedFixModel::bareFixCount(const QUuid& ownerId) const
{
    return static_cast<int>(std::count_if(m_fixes.cbegin(), m_fixes.cend(),
                                          [&ownerId](const cwAttachedFix& fix) {
        return fix.ownerId == ownerId && fix.coordinateSystem.isEmpty();
    }));
}

int cwAttachedFixModel::firstFixWithoutReadInSystem() const
{
    const auto bare = std::find_if(m_fixes.cbegin(), m_fixes.cend(), [](const cwAttachedFix& fix) {
        return fix.coordinateSystem.isEmpty() && fix.readInSystem.isEmpty();
    });
    return bare == m_fixes.cend() ? -1 : static_cast<int>(std::distance(m_fixes.cbegin(), bare));
}

void cwAttachedFixModel::readGridPositions()
{
    constexpr double kNoValue = std::numeric_limits<double>::quiet_NaN();
    const cwGeoPoint noPosition(kNoValue, kNoValue, kNoValue);
    m_gridPositions.clear();
    m_gridPositions.reserve(m_fixes.size());
    for (const cwAttachedFix& fix : std::as_const(m_fixes)) {
        m_gridPositions.append(fix.gridPosition().value_or(noPosition));
    }
}
