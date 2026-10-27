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

    //Pre-fill with the trip's first known station (scope-relative name) to save
    //typing; fall back to a placeholder when the record knows of none. Same
    //accessor the scope autocomplete reads, so the field and the dropdown
    //offering to complete it always agree.
    cwTrip* trip = noteLiDAR != nullptr ? noteLiDAR->parentTrip() : nullptr;
    const QList<cwTrip::KnownStation> known =
        trip != nullptr ? trip->knownStations() : QList<cwTrip::KnownStation>();
    station.setName(known.isEmpty() ? QStringLiteral("Station Name") : known.first().name);

    noteLiDAR->addStation(station);
}
