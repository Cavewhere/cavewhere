#include "cwBaseNoteLiDARStationInteraction.h"
#include "cwTrip.h"

cwBaseNoteLiDARStationInteraction::cwBaseNoteLiDARStationInteraction(QQuickItem *parent) :
    cwInteraction(parent)
{

}

void cwBaseNoteLiDARStationInteraction::addPoint(QVector3D position, cwNoteLiDAR *noteLiDAR)
{
    cwNoteLiDARStation station;
    station.setPositionOnNote(position);

    //Pre-fill with the trip's first solved station (scope-relative name) to
    //save typing; fall back to a placeholder when the trip is unsolved.
    cwTrip* trip = noteLiDAR != nullptr ? noteLiDAR->parentTrip() : nullptr;
    const QList<QPair<QString, QVector3D>> solved =
        trip != nullptr ? trip->solvedStations() : QList<QPair<QString, QVector3D>>();
    station.setName(solved.isEmpty() ? QStringLiteral("Station Name") : solved.first().first);

    noteLiDAR->addStation(station);
}
