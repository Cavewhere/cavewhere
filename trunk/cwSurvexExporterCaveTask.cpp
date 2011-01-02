//Our includes
#include "cwSurvexExporterCaveTask.h"
#include "cwSurvexExporterTripTask.h"
#include "cwCave.h"
#include "cwTrip.h"

cwSurvexExporterCaveTask::cwSurvexExporterCaveTask(QObject *parent) :
    cwSurvexExporterTask(parent)
{
    Cave = new cwCave(this);
    TripExporter = new cwSurvexExporterTripTask(this);
    TripExporter->setParentSurvexExporter(this);

    connect(TripExporter, SIGNAL(progressed(int)), SLOT(UpdateProgress(int)));
}

/**
  \brief Sets the data for the task

  If the task is already running, then this does nothing
  */
void cwSurvexExporterCaveTask::setData(const cwCave& cave) {
    if(!isRunning()) {
        *Cave = cave;
    } else {
        qWarning("Can't set cave data when cave exporter is already running");
    }
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

/**
  \brief Starts running the export cave task
  */
void cwSurvexExporterCaveTask::runTask() {
    openOutputFile();
    writeCave(OutputStream, Cave);
    closeOutputFile();
    done();
}

/**
  \brief Updates the progress of the cave task
  */
void cwSurvexExporterCaveTask::UpdateProgress(int /*tripProgress*/) {

}
