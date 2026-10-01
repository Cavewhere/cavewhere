/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwCaveExporterTask.h"
#include "cwCave.h"

//Qt includes
#include <QThread>

cwCaveExporterTask::cwCaveExporterTask(QObject* parent) :
    cwExporterTask(parent)
{
}

/**
  \brief Sets the data for the task

  If the task is already running, then this does nothing
  */
void cwCaveExporterTask::setData(const cwCaveData &cave) {
    if(!isRunning()) {
        Cave = cave;
    } else {
        qWarning("Can't set cave data when cave exporter is already running");
    }
}

/**
  \brief Starts running the export cave task
  */
void cwCaveExporterTask::runTask() {
    if(checkData() && openOutputFile()) {
        bool good = writeCave(*OutputStream.data(), Cave);
        closeOutputFile();

        if(!good) {
            stop();
        }
    } else {
        stop();
    }
    done();
}




/**
  \brief Updates the progress of the cave task
  */
void cwCaveExporterTask::UpdateProgress(int /*tripProgress*/) {

}

/**
  \brief Checks if the cave has trips in it before running
  */
bool cwCaveExporterTask::checkData() {
    return checkData(Cave);
}

/**
  \brief Checks if the cave has trips in it, at any depth, before running
  */
bool cwCaveExporterTask::checkData(const cwCaveData& cave) {
    bool hasTrips = !cave.trips.isEmpty();
    walkCaveDataTree(cave.nodes, [&hasTrips](const cwCaveData& node, const QStringList&) {
        hasTrips = hasTrips || !node.trips.isEmpty();
    });

    if(!hasTrips) {
        Errors.append(QString("No trips to do loop closure in %1").arg(cave.name));
        return false;
    }
    return true;
}
