#include "cwBaseNoteLiDARStationInteraction.h"

cwBaseNoteLiDARStationInteraction::cwBaseNoteLiDARStationInteraction(QQuickItem *parent) :
    cwInteraction(parent)
{

}

void cwBaseNoteLiDARStationInteraction::addPoint(QVector3D position, cwNoteLiDAR *noteLiDAR)
{
    cwNoteLiDARStation station;
    station.setPositionOnNote(position);
    station.setName("Station Name");
    noteLiDAR->addStation(station);
}
