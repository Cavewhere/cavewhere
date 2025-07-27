/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurvexExporterCaveTask.h"
#include "cwSurvexExporterTripTask.h"
#include "cwTrip.h"

cwSurvexExporterCaveTask::cwSurvexExporterCaveTask(QObject *parent) :
    cwCaveExporterTask(parent)
{
    TripExporter = new cwSurvexExporterTripTask(this);
    TripExporter->setParentSurvexExporter(this);

//    connect(TripExporter, SIGNAL(progressed(int)), SIGNAL(progressed(int)));
}

/**
  \brief Writes the cave data to the stream
  */
bool cwSurvexExporterCaveTask::writeCave(QTextStream& stream, const cwCaveData& cave) {

    if(!checkData(cave)) {
        if(isRunning()) {
            stop();
        }
        return false;
    }

    QString caveName = QString(cave.name).remove(" ");

    stream << "*begin " << caveName << " ;" << cave.name << Qt::endl << Qt::endl;

    //Add fix station to tie the cave down
    fixFirstStation(stream, cave);

    //Haven't done anything
    TotalProgress = 0;

    //Go throug all the trips and save them
    for(int i = 0; i < cave.trips.size(); i++) {
        const cwTripData& tripData = cave.trips.at(i);
        auto trip = std::make_unique<cwTrip>();
        trip->setData(tripData);
        TripExporter->writeTrip(stream, trip.get());
        TotalProgress += trip->numberOfStations();
        stream << Qt::endl;
    }

    stream << "*end ; End of " << cave.name << Qt::endl;

    return true;
}

/**
 * @brief cwSurvexExporterCaveTask::fixFirstStation
 * @param stream
 * @param cave
 *
 * This fixes the first station in the cave, if the cave has any stations.
 */
void cwSurvexExporterCaveTask::fixFirstStation(QTextStream &stream, const cwCaveData &cave)
{
    if(!cave.trips.isEmpty()) {
        const cwTripData& firstTrip = cave.trips.first();
        if(!firstTrip.chunks.isEmpty()) {
            const cwSurveyChunkData& firstChunk = firstTrip.chunks.first();
            if(!firstChunk.stations.isEmpty()) {
                cwStation station = firstChunk.stations.first();

                stream << "*fix " << station.name() << " " << 0 << " " << 0 << " " << 0 << Qt::endl;
            }
        }
    }
}


