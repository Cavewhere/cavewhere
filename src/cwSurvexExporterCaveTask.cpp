//Our includes
#include "cwSurvexExporterCaveTask.h"
#include "cwSurvexExporterTripTask.h"
#include "cwCave.h"
#include "cwTrip.h"

cwSurvexExporterCaveTask::cwSurvexExporterCaveTask(QObject *parent) :
    cwCaveExporterTask(parent)
{
    TripExporter = new cwSurvexExporterTripTask(this);
    TripExporter->setParentSurvexExporter(this);

    connect(TripExporter, SIGNAL(progressed(int)), SIGNAL(progressed(int)));
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

    stream << "*begin " << caveName << " ;" << cave->name() << endl << endl;

    //Haven't done anything
    TotalProgress = 0;

    //Go throug all the trips and save them
    for(int i = 0; i < cave->tripCount(); i++) {
        cwTrip* trip = cave->trip(i);
        TripExporter->writeTrip(stream, trip);
        TotalProgress += trip->numberOfStations();
        stream << endl;
    }

    stream << "*end ; End of " << cave->name() << endl;

    return true;
}


