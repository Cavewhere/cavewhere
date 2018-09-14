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
    Cave = new cwCave();
    Cave->moveToThread(nullptr);
}

cwCaveExporterTask::~cwCaveExporterTask()
{
    Cave->deleteLater();
}

/**
  \brief Sets the data for the task

  If the task is already running, then this does nothing
  */
void cwCaveExporterTask::setData(const cwCave& cave) {
    if(!isRunning()) {
        Cave->moveToThread(QThread::currentThread());
        *Cave = cave;
        Cave->moveToThread(nullptr);
    } else {
        qWarning("Can't set cave data when cave exporter is already running");
    }
}

/**
  \brief Starts running the export cave task
  */
void cwCaveExporterTask::runTask() {
    Cave->moveToThread(QThread::currentThread());
    if(checkData() && openOutputFile()) {
        bool good = writeCave(*OutputStream.data(), Cave);
        closeOutputFile();

        if(!good) {
            stop();
        }
    } else {
        stop();
    }
    Cave->moveToThread(nullptr);
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
  \brief Checks if the cave has trips in it before running
  */
bool cwCaveExporterTask::checkData(cwCave* cave) {
    if(!cave->hasTrips()) {
        Errors.append(QString("No trips to do loop closure in %1").arg(cave->name()));
        return false;
    }
    return true;
}
