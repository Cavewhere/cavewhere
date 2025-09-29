//Our includes
#include "cwNoteLiDAR.h"
#include "cwTrip.h"

#include <QMetaType>

cwNoteLiDAR::cwNoteLiDAR(QObject* parent)
    : QObject(parent)
{

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
    qDebug() << "Add station:" << station.name() << station.positionOnNote();
    m_stations.append(station);
    // emit stationsChanged();
}

void cwNoteLiDAR::removeStation(int stationId) {
    if (stationId < 0 || stationId >= m_stations.size()) {
        return;
    }
    m_stations.removeAt(stationId);
    // emit stationsChanged();
}

const QList<cwNoteLiDARStation>& cwNoteLiDAR::stations() const {
    return m_stations;
}

void cwNoteLiDAR::setStations(const QList<cwNoteLiDARStation>& stations) {
    if (m_stations == stations) {
        return;
    }
    m_stations = stations;
    // emit stationsChanged();
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
        setParentCave(trip->parentCave());
    }
}

void cwNoteLiDAR::setParentCave(cwCave *cave)
{
    if(m_parentCave != cave) {
        m_parentCave = cave;
    }
}
