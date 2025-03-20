/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurvexExporterCaveTask.h"
#include "cwSurvexExporterTripTask.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"

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
bool cwSurvexExporterCaveTask::writeCave(QTextStream& stream, cwCave* cave) {

    if(!checkData(cave)) {
        if(isRunning()) {
            stop();
        }
        return false;
    }

    QString caveName = cave->name().remove(" ");

    stream << "*begin " << caveName << " ;" << cave->name() << Qt::endl << Qt::endl;

    //This fucks up shit in cavern
   // stream << "*sd compass 2.0 degrees" << Qt::endl;
   // stream << "*sd clino 2.0 degrees" << Qt::endl << Qt::endl;

    //Add fix station to tie the cave down
    fixFirstStation(stream, cave);

    //Haven't done anything
    TotalProgress = 0;

    //Go throug all the trips and save them
    for(int i = 0; i < cave->tripCount(); i++) {
        cwTrip* trip = cave->trip(i);
        TripExporter->writeTrip(stream, trip);
        TotalProgress += trip->numberOfStations();
        stream << Qt::endl;
    }

    stream << "*end ; End of " << cave->name() << Qt::endl;

    return true;
}

/**
 * @brief cwSurvexExporterCaveTask::fixFirstStation
 * @param stream
 * @param cave
 *
 * This fixes the first station in the cave, if the cave has any stations.
 */
void cwSurvexExporterCaveTask::fixFirstStation(QTextStream &stream, cwCave *cave)
{
    if(cave != nullptr) {
        if(!cave->trips().isEmpty()) {
            cwTrip* firstTrip = cave->trips().first();
            if(!firstTrip->chunks().isEmpty()) {
                cwSurveyChunk* firstChunk = firstTrip->chunks().first();
                if(!firstChunk->stations().isEmpty()) {
                    cwStation station = firstChunk->stations().first();

                    stream << "*fix " << station.name() << " " << 0 << " " << 0 << " " << 0 << Qt::endl;
                }
            }
        }
    }
}


