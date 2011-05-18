//Our includes
#include "cwLinePlotTask.h"
#include "cwSurvexExporterRegionTask.h"
#include "cwCavernTask.h"
#include "cwCavingRegion.h"
#include "cwPlotSauceTask.h"
#include "cwPlotSauceXMLTask.h"

//Qt includes
#include <QDebug>
#include <QTime>

cwLinePlotTask::cwLinePlotTask(QObject *parent) :
    cwTask(parent)
{
    Region = new cwCavingRegion(NULL, this);

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

    connect(CavernTask, SIGNAL(finished()), SLOT(convertToXML()));
    connect(CavernTask, SIGNAL(stopped()), SLOT(done()));

    PlotSauceTask = new cwPlotSauceTask();
    PlotSauceTask->setParentTask(this);

    connect(PlotSauceTask, SIGNAL(finished()), SLOT(readXML()));
    connect(PlotSauceTask, SIGNAL(stopped()), SLOT(done()));

    PlotSauceParseTask = new cwPlotSauceXMLTask();
    PlotSauceParseTask->setParentTask(this);

    connect(PlotSauceParseTask, SIGNAL(finished()), SLOT(linePlotTaskComplete()));
    connect(PlotSauceParseTask, SIGNAL(stopped()), SLOT(done()));
    connect(PlotSauceParseTask, SIGNAL(stationPosition(QString,QVector3D)), SLOT(setStationPosition(QString,QVector3D)));
}

/**
  \brief Set's the data for the line plot task
  */
void cwLinePlotTask::setData(cwCavingRegion region) {
    if(status() == Running) {
        qWarning() << "Can't set cave data for LinePlotTask, while it's running";
        return;
    }

    *Region = region;
    Region->setParent(this);
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
    Time.start();
    exportData();
}

/**
  \brief Exports the data to
  */
void cwLinePlotTask::exportData() {
    qDebug() << "Status" << status();
    SurvexExporter->setData(*Region);
    SurvexExporter->start();
}

/**
  This once the data has been exported this runs
  cavern on the data
  */
void cwLinePlotTask::runCavern() {
    qDebug() << "Running cavern on " << SurvexFile->fileName() << "Status" << status();;
    CavernTask->start();
}

/**
  Once cavern is done running the data, this converts the 3d data
  into compress xml file
  */
void cwLinePlotTask::convertToXML() {
    qDebug() << "Covert 3d to xml" << "Status" << status();
    PlotSauceTask->setSurvex3DFile(CavernTask->output3dFileName());
    PlotSauceTask->start();
}

void cwLinePlotTask::readXML() {
    qDebug() << "Reading xml" << "Status" << status();;
    PlotSauceParseTask->setPlotSauceXMLFile(PlotSauceTask->outputXMLFile());
    PlotSauceParseTask->start();
}

/**
  \brief This alerts all the listeners that the data is done
  */
void cwLinePlotTask::linePlotTaskComplete() {
    qDebug() << "Finished running linePlotTask:" << Time.elapsed() << "ms";
    done();
}

void cwLinePlotTask::setStationPosition(QString /*name*/, QVector3D /*position*/) {
   // qDebug() << "Station:" << name << "Position:" << position;
}
