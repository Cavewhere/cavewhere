//Our includes
#include "cwLinePlotTask.h"
#include "cwSurvexExporterRegionTask.h"
#include "cwCavernTask.h"
#include "cwCavingRegion.h"
#include "cwPlotSauceTask.h"
#include "cwPlotSauceXMLTask.h"
#include "cwLinePlotGeometryTask.h"
#include "cwCave.h"
#include "cwDebug.h"

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

    connect(CavernTask, SIGNAL(finished()), SLOT(convertToXML()));
    connect(CavernTask, SIGNAL(stopped()), SLOT(done()));

    PlotSauceTask = new cwPlotSauceTask();
    PlotSauceTask->setParentTask(this);

    connect(PlotSauceTask, SIGNAL(finished()), SLOT(readXML()));
    connect(PlotSauceTask, SIGNAL(stopped()), SLOT(done()));

    PlotSauceParseTask = new cwPlotSauceXMLTask();
    PlotSauceParseTask->setParentTask(this);

    connect(PlotSauceParseTask, SIGNAL(finished()), SLOT(generateCenterlineGeometry()));
    connect(PlotSauceParseTask, SIGNAL(stopped()), SLOT(done()));
    connect(PlotSauceParseTask, SIGNAL(stationPosition(QString,QVector3D)), SLOT(setStationPosition(QString,QVector3D)));

    CenterlineGeometryTask = new cwLinePlotGeometryTask();
    CenterlineGeometryTask->setParentTask(this);

    connect(PlotSauceParseTask, SIGNAL(finished()), SLOT(linePlotTaskComplete()));
    connect(PlotSauceParseTask, SIGNAL(stopped()), SLOT(done()));

}

/**
  \brief Set's the data for the line plot task
  */
void cwLinePlotTask::setData(const cwCavingRegion& region) {
    if(!isReady()) {
        qWarning() << "Can't set cave data for LinePlotTask, while it's running";
        return;
    }

    Region->setParent(this);
    *Region = region;


    //Incode the the cave's index into the cave's name
    for(int i = 0; i < Region->caveCount(); i++) {
        cwCave* cave = Region->cave(i);
        QString caveNameNoSpaces = cave->name().remove(" ");
        cave->setName(QString("%1-%2").arg(i).arg(caveNameNoSpaces));
    }
}

/**
  \brief Called when plot task starts running

  1. Export the region or part of the region of interest into survex file
  2. Run the survex program
  3. Read the 3d file data
  4. Update the survey data
  */
void cwLinePlotTask::runTask() {
    qDebug() << "\n-------------------------------------";
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
    qDebug() << "Covert 3d to xml" << "Status" << status() << CavernTask->output3dFileName();
    PlotSauceTask->setSurvex3DFile(CavernTask->output3dFileName());
    PlotSauceTask->start();
}

void cwLinePlotTask::readXML() {
    qDebug() << "Reading xml" << "Status" << status();
    PlotSauceParseTask->setPlotSauceXMLFile(PlotSauceTask->outputXMLFile());
    PlotSauceParseTask->start();
}

/**
  \brief This starts the lineplot geometry task

  This will generate the centerline geometry for the data
  */
void cwLinePlotTask::generateCenterlineGeometry() {
    qDebug() << "Generating centerline geometry" << status();
    CenterlineGeometryTask->setRegion(Region);
    CenterlineGeometryTask->start();
}

/**
  \brief This alerts all the listeners that the data is done
  */
void cwLinePlotTask::linePlotTaskComplete() {
    qDebug() << "Finished running linePlotTask:" << Time.elapsed() << "ms";
    done();
}

void cwLinePlotTask::setStationPosition(QString name, QVector3D position) {

   QString caveIndexString = name.section('-', 0, 0); //Extract the index
   QString caveNameAndStation = name.section('-', 1, -1); //Extract the rest
   QString stationName = caveNameAndStation.section(".", 1, 1); //Extract the station
   QString caveName = name.section(".", 0, 0);

   bool okay;
   int caveIndex = caveIndexString.toInt(&okay);

   if(!okay) {
       qDebug() << "Can't covent caveIndex is not an int:" << caveIndexString << LOCATION;
       return;
   }

   //Make sure the index is good
   if(caveIndex < 0 || caveIndex >= Region->caveCount()) {
       qDebug() << "CaveIndex is bad:" << caveIndex << LOCATION;
       return;
   }

   cwCave* cave = Region->cave(caveIndex);

   //Make sure the caveName is valid
   if(caveName.compare(cave->name(), Qt::CaseInsensitive) != 0) {
       qDebug() << "CaveName is invalid:" << caveName << "looking for" << cave->name() << LOCATION;
       return;
   }

   QWeakPointer<cwStation> station = cave->station(stationName);
   QSharedPointer<cwStation> sharedStation = station.toStrongRef();
   if(sharedStation.isNull()) {
       qDebug() << "Couldn't find station:" << stationName << "in cave" << cave->name();
       return;
   }

   sharedStation->setPosition(position);

  // qDebug() << "Station:" << name << "Position:" << position;
}
