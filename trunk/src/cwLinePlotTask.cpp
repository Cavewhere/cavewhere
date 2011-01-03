//Our includes
#include "cwLinePlotTask.h"
#include "cwSurvexExporterRegionTask.h"
#include "cwCavernTask.h"
#include "cwCavingRegion.h"

//Qt includes
#include <QDebug>
#include <QTime>

cwLinePlotTask::cwLinePlotTask(QObject *parent) :
    cwTask(parent)
{
    Region = new cwCavingRegion(this);

    SurvexFile = new QTemporaryFile(this);
    SurvexFile->open();
    SurvexFile->close();

    SurvexExporter = new cwSurvexExporterRegionTask();
    SurvexExporter->setParentTask(this);
    SurvexExporter->setOutputFile(SurvexFile->fileName());

    connect(SurvexExporter, SIGNAL(finished()), SLOT(runCavern()));
    connect(SurvexExporter, SIGNAL(stopped()), SLOT(done()));

    CavernTask = new cwCavernTask();
    CavernTask->setParentTask(this);
    CavernTask->setSurvexFile(SurvexFile->fileName());

    connect(CavernTask, SIGNAL(finished()), SLOT(read3DData()));
    connect(CavernTask, SIGNAL(stopped()), SLOT(done()));
}

/**
  \brief Set's the data for the line plot task
  */
void cwLinePlotTask::setData(cwCavingRegion region) {
    if(status() == Running) {
        qWarning() << "Can't set cave data for LinePlotTask, while it's running";
        return;
    }

    QTime time;
    time.start();

    *Region = region;
    Region->setParent(this);

    //qDebug() << "Copy time: " << time.elapsed() << "ms";
}

/**
  \brief Called when plot task starts running

  1. Export the region or part of the region of interest into survex file
  2. Run the survex program
  3. Read the 3d file data
  4. Update the survey data
  */
void cwLinePlotTask::runTask() {
    //Can't run with null data
//    if(Region == NULL) {
//        qWarning() << "Can't run line plot task with no data"
//        stop();
//        done();
//        return;
//    }

    exportData();
}

/**
  \brief Exports the data to
  */
void cwLinePlotTask::exportData() {
    SurvexExporter->setData(*Region);
    SurvexExporter->start();
}

/**

  */
void cwLinePlotTask::runCavern() {
    qDebug() << "Running cavern on " << SurvexFile->fileName();
    CavernTask->start();
}

/**

  */
void cwLinePlotTask::read3DData() {
    qDebug() << "Reading in 3D data";
    done();
}

/**
  \brief This alerts all the listeners that the data is done
  */
void cwLinePlotTask::complete() {
    done();
}
