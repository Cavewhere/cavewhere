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

    connect(TripExporter, SIGNAL(progressed(int)), SLOT(UpdateProgress(int)));
}

/**
  \brief Writes the cave data to the stream
  */
void cwSurvexExporterCaveTask::writeCave(QTextStream& stream, cwCave* cave) {

    stream << "*begin " << cave->name() << endl << endl;

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
}

