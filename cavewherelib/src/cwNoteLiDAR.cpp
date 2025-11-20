//Our includes
#include "cwNoteLiDAR.h"
#include "cwTrip.h"
#include "cwCave.h"
#include "cwNoteLiDARTransformation.h"
#include "cwNoteTransformCalculator.h"

//Qt includes
#include <QMetaType>

cwNoteLiDAR::cwNoteLiDAR(QObject* parent)
    : QAbstractListModel(parent),
    m_noteTransformation(new cwNoteLiDARTransformation(this))
{
    connect(m_noteTransformation, &cwNoteLiDARTransformation::matrixChanged, this, [this]() {
        QList<int> roles = {ScenePositionRole};
        emit dataChanged(index(0), index(rowCount() - 1), roles);
        updateNoteTransformion();
    });

    connect(m_noteTransformation, &cwNoteLiDARTransformation::upChanged, this, [this]() {
        QList<int> roles = {UpPositionRole};
        emit dataChanged(index(0), index(rowCount() - 1), roles);
    });

    connect(this, &cwNoteLiDAR::autoCalculateNorthChanged, this, [this]() {
        updateNoteTransformion();
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

    updateNoteTransformion();
}

void cwNoteLiDAR::removeStation(int stationId) {
    if (stationId < 0 || stationId >= m_stations.size()) {
        return;
    }
    beginRemoveRows(QModelIndex(), stationId, stationId);
    m_stations.removeAt(stationId);
    endRemoveRows();
    emit countChanged();

    updateNoteTransformion();
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

cwNoteLiDARData cwNoteLiDAR::data() const
{
    return {
        m_name.value(),
        m_filename,
        m_stations,
        m_noteTransformation->data(),
        m_autoCalculateNorth
    };
}

void cwNoteLiDAR::setData(const cwNoteLiDARData &data)
{
    setName(data.name);
    setFilename(data.filename);
    setStations(data.stations);
    setAutoCalculateNorth(data.autoCalculateNorth); //This should be set before m_noteTransformation because auto calucaltion
    m_noteTransformation->setData(data.transfrom);
}

/**
  \brief Sets the parent trip for this note
  */
void cwNoteLiDAR::setParentTrip(cwTrip* trip) {
    if(m_parentTrip != trip) {
        m_parentTrip = trip;
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

    const cwNoteLiDARStation& station = m_stations.at(row);
    switch (role) {
    case NameRole: {
        return station.name();
    }
    case PositionOnNoteRole: {
        return station.positionOnNote();
    }
    case ScenePositionRole: {
        //We might want to cache the matrix inside of m_noteTransformation
        return m_noteTransformation->matrix().map(station.positionOnNote());
    }
    case UpPositionRole: {
        return m_noteTransformation->up() * station.positionOnNote();
    }
    case StationRole: {
        return QVariant::fromValue(station);
    }
    default: {
        return {};
    }
    }
}

bool cwNoteLiDAR::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid()) {
        return false;
    }
    const int row = index.row();
    cwNoteLiDARStation& station = m_stations[row];

    switch (role) {
    case NameRole: {
        station.setName(value.toString());
        emit dataChanged(index, index, {NameRole});
        updateNoteTransformion();
        return true;
    }
    case PositionOnNoteRole: {
        station.setPositionOnNote(value.value<QVector3D>());
        emit dataChanged(index, index, {PositionOnNoteRole, ScenePositionRole, UpPositionRole});
        updateNoteTransformion();
        return true;
    }
    case StationRole: {
        return false;
    }
    default: {
        return false;
    }
    }
    return false;
}


QHash<int, QByteArray> cwNoteLiDAR::roleNames() const
{
    return {
        { NameRole, QByteArrayLiteral("name") },
        { PositionOnNoteRole, QByteArrayLiteral("positionOnNote") },
        { ScenePositionRole, QByteArrayLiteral("scenePosition") },
        { UpPositionRole, QByteArrayLiteral("upPosition") },
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

void cwNoteLiDAR::updateNoteTransformion()
{
    if(!m_autoCalculateNorth) {
        return;
    }

    if(parentTrip() == nullptr) {
        return;
    }

    if(parentCave() == nullptr) {
        return;
    }

    if(m_stations.size() < 2) {
        return;
    }

    QMatrix4x4 upMatrix;
    upMatrix.rotate(m_noteTransformation->up());

    cwNoteTransformCalculator::ProfileTransform profileTransform {
                                                                 cwScrapType::LiDAR,
                                                                 QMatrix4x4(),
                                                                 QMatrix4x4(),
                                                                 QMatrix4x4(), //View matrix on the real survey data
                                                                 upMatrix, //Up matrix
                                                                 parentCave()->stationPositionLookup()};



    auto shotStations = cwNoteTransformCalculator::noteShots(stations(), parentCave()->network());
    auto transform = cwNoteTransformCalculator::projectedAverageTransform(shotStations, profileTransform);
    m_noteTransformation->setNorthUp(transform.north);
}


cwNoteLiDARTransformation *cwNoteLiDAR::noteTransformation() const
{
    return m_noteTransformation;
}
