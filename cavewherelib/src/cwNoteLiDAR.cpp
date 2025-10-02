//Our includes
#include "cwNoteLiDAR.h"
#include "cwTrip.h"
#include "cwCave.h"

#include <QMetaType>

cwNoteLiDAR::cwNoteLiDAR(QObject* parent)
    : QAbstractListModel(parent)
{
    connect(this, &cwNoteLiDAR::modelMatrixChanged, this, [this]() {
        QList<int> roles = {ScenePositionRole};
        emit dataChanged(index(0), index(rowCount() - 1), roles);
    });

    m_modelMatrix.setBinding([this]() {
        QMatrix4x4 matrix;

        //TODO: make this user define
        //Default rotation for up
        matrix.rotate(90.0, 1.0, 0.0, 0.0);

        // matrix.translate(m_translation);
        // auto rotation = m_rotation.value();
        // matrix.rotate(rotation.w(), rotation.x(), rotation.y(), rotation.z());
        return matrix;
    });
}

QString cwNoteLiDAR::filename() const {
    return m_filename;
}

void cwNoteLiDAR::setFilename(const QString& path) {
    if (m_filename == path) {
        return;
    }
    m_filename = path;
    emit filenameChanged();
}

void cwNoteLiDAR::addStation(const cwNoteLiDARStation& station) {
    const int insertRow = m_stations.size();
    beginInsertRows(QModelIndex(), insertRow, insertRow);
    m_stations.append(station);
    endInsertRows();
    emit countChanged();
}

void cwNoteLiDAR::removeStation(int stationId) {
    if (stationId < 0 || stationId >= m_stations.size()) {
        return;
    }
    beginRemoveRows(QModelIndex(), stationId, stationId);
    m_stations.removeAt(stationId);
    endRemoveRows();
    emit countChanged();
}

const QList<cwNoteLiDARStation>& cwNoteLiDAR::stations() const {
    return m_stations;
}

void cwNoteLiDAR::setStations(const QList<cwNoteLiDARStation>& stations) {
    if (m_stations == stations) {
        return;
    }
    beginResetModel();
    m_stations = stations;
    endResetModel();
    emit countChanged();
}

cwNoteLiDARStation cwNoteLiDAR::station(int stationId) const {
    if (stationId < 0 || stationId >= m_stations.size()) {
        return cwNoteLiDARStation(); // invalid (name empty)
    }
    return m_stations.at(stationId);
}

/**
  \brief Sets the parent trip for this note
  */
void cwNoteLiDAR::setParentTrip(cwTrip* trip) {
    if(m_parentTrip != trip) {
        m_parentTrip = trip;
        setParent(trip);
    }
}

cwCave *cwNoteLiDAR::parentCave() const
{
    return m_parentTrip != nullptr ? m_parentTrip->parentCave() : nullptr;
}

int cwNoteLiDAR::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_stations.size();
}

QVariant cwNoteLiDAR::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return {};
    }
    const int row = index.row();
    if (row < 0 || row >= m_stations.size()) {
        return {};
    }

    const cwNoteLiDARStation& station = m_stations.at(row);
    switch (role) {
    case NameRole: {
        return station.name();
    }
    case PositionOnNoteRole: {
        // QVector3D is a Qt metatype; it converts to QVariant cleanly
        return station.positionOnNote();
    }
    case ScenePositionRole: {
        qDebug() << "ModelMatrix:" << m_modelMatrix.value();
        qDebug() << "Scene Point:" <<  m_modelMatrix.value().map(station.positionOnNote()) << station.positionOnNote();
        return m_modelMatrix.value().map(station.positionOnNote());
    }
    case StationRole: {
        return QVariant::fromValue(station);
    }
    default: {
        return {};
    }
    }
}


QHash<int, QByteArray> cwNoteLiDAR::roleNames() const
{
    return {
        { NameRole, QByteArrayLiteral("name") },
        { PositionOnNoteRole, QByteArrayLiteral("positionOnNote") },
        { StationRole, QByteArrayLiteral("station") },

    };
}

int cwNoteLiDAR::clampIndex(int stationId) const
{
    if (stationId < 0) {
        return 0;
    }
    if (stationId >= m_stations.size()) {
        return m_stations.size() - 1;
    }
    return stationId;
}

