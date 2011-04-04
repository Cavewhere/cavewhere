//Our includes
#include "cwCaveExporterTask.h"
#include "cwCave.h"

cwCaveExporterTask::cwCaveExporterTask(QObject* parent) :
    cwExporterTask(parent)
{
    Cave = new cwCave(this);
}

/**
  \brief Sets the data for the task

  If the task is already running, then this does nothing
  */
void cwCaveExporterTask::setData(const cwCave& cave) {
    if(!isRunning()) {
        *Cave = cave;
    } else {
        qWarning("Can't set cave data when cave exporter is already running");
    }
}

/**
  \brief Starts running the export cave task
  */
void cwCaveExporterTask::runTask() {
    openOutputFile();
    writeCave(OutputStream, Cave);
    closeOutputFile();
    done();
}

/**
  \brief Updates the progress of the cave task
  */
void cwCaveExporterTask::UpdateProgress(int /*tripProgress*/) {

}
