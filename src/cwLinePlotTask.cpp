//Our includes
#include "cwLinePlotTask.h"
#include "cwSurvexExporterRegionTask.h"
#include "cwCavernTask.h"
#include "cwCavingRegion.h"
#include "cwPlotSauceTask.h"
#include "cwPlotSauceXMLTask.h"
#include "cwLinePlotGeometryTask.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwNote.h"
#include "cwScrap.h"
#include "cwSurveyNoteModel.h"
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
    //connect(PlotSauceParseTask, SIGNAL(stationPosition(QString,QVector3D)), SLOT(updateStationPositionForCaves(QString,QVector3D)));

    CenterlineGeometryTask = new cwLinePlotGeometryTask();
    CenterlineGeometryTask->setParentTask(this);

    connect(CenterlineGeometryTask, SIGNAL(finished()), SLOT(linePlotTaskComplete()));
    connect(CenterlineGeometryTask, SIGNAL(stopped()), SLOT(done()));

}



/**
  \brief Set's the data for the line plot task
  */
void cwLinePlotTask::setData(const cwCavingRegion& region) {
    if(!isReady()) {
        qWarning() << "Can't set cave data for LinePlotTask, while it's running";
        return;
    }

    //Copy over all region data
    *Region = region;

    //Populate the original pointers
    RegionOriginalPointers = RegionDataPtrs(region);

}

/**
  \brief Called when plot task starts running

  1. Export the region or part of the region of interest into survex file
  2. Run the survex program
  3. Read the 3d file data
  4. Update the survey data
  */
void cwLinePlotTask::runTask() {
    //  qDebug() << "\n-------------------------------------";
    if(!isRunning()) {
        done();
        return;
    }

    qDebug() << "Running line plot task";

    //Change all the cave names, such that survex can handle them correctly
    encodeCaveNames();

    //Initilize the cave station lookup, from previous run
    initializeCaveStationLookups();

    int numCaves = Region->caveCount();
    CaveStationLookups.clear();
    CaveStationLookups.reserve(numCaves);
    CaveStationLookups.resize(numCaves);

    Time.start();
    exportData();
}

/**
  \brief Exports the data to
  */
void cwLinePlotTask::exportData() {
    if(!isRunning()) {
        done();
        return;
    }

    //  qDebug() << "Status" << status();
    SurvexExporter->setData(*Region);
    SurvexExporter->start();
}

/**
  This once the data has been exported this runs
  cavern on the data
  */
void cwLinePlotTask::runCavern() {
    if(!isRunning()) {
        done();
        return;
    }

    //qDebug() << "Running cavern on " << SurvexFile->fileName() << "Status" << status();
    CavernTask->start();
}

/**
  Once cavern is done running the data, this converts the 3d data
  into compress xml file
  */
void cwLinePlotTask::convertToXML() {
    if(!isRunning()) {
        done();
        return;
    }

    //  qDebug() << "Covert 3d to xml" << "Status" << status() << CavernTask->output3dFileName();
    PlotSauceTask->setSurvex3DFile(CavernTask->output3dFileName());
    PlotSauceTask->start();
}

void cwLinePlotTask::readXML() {
    if(!isRunning()) {
        done();
        return;
    }

    //  qDebug() << "Reading xml" << "Status" << status() << PlotSauceTask->outputXMLFile();
    PlotSauceParseTask->setPlotSauceXMLFile(PlotSauceTask->outputXMLFile());
    PlotSauceParseTask->start();
}

/**
  \brief This starts the lineplot geometry task

  This will generate the centerline geometry for the data
  */
void cwLinePlotTask::generateCenterlineGeometry() {
    if(!isRunning()) {
        done();
        return;
    }

    //Go through all the stations in the plot sauce parse and assign them
    //to caves
    updateStationPositionForCaves(PlotSauceParseTask->stationPositions());

    //Clear all the stations from the parser
    PlotSauceParseTask->clearStationPositions();

    //  qDebug() << "Generating centerline geometry" << status();
    CenterlineGeometryTask->setRegion(Region);
    CenterlineGeometryTask->start();
}

/**
  \brief This alerts all the listeners that the data is done
  */
void cwLinePlotTask::linePlotTaskComplete() {

    //Copy all the from the CenterLineGemoetryTask into the results
    Result.StationPositions = CenterlineGeometryTask->pointData();
    Result.LinePlotIndexData = CenterlineGeometryTask->indexData();

    // qDebug() << "Finished running linePlotTask:" << Time.elapsed() << "ms";
    done();
}

void cwLinePlotTask::updateStationPositionForCaves(const cwStationPositionLookup& stationPostions) {
    QMapIterator<QString, QVector3D> iter(stationPostions.positions());
    while( iter.hasNext() ) {
        iter.next();

        QString name = iter.key();
        QVector3D position = iter.value();

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

        cwStationPositionLookup& lookup = CaveStationLookups[caveIndex];
        if(lookup.hasPosition(stationName)) {
            QVector3D oldPosition = lookup.position(stationName);
            qDebug() << "OldPosition" << oldPosition << position << (oldPosition != position);
            if(oldPosition != position) {
                //Update the position
                lookup.setPosition(stationName, position);
                setStationAsChanged(caveIndex, stationName);
            }
        } else {
            //New station
            lookup.setPosition(stationName, position);
            setStationAsChanged(caveIndex, stationName);
        }
    }

    //Update all cave station position models
    for(int i = 0; i < Region->caveCount(); i++) {
        //Get extrenal cave pointer
        cwCave* externalCave = RegionOriginalPointers.Caves.at(i).Cave;
        if(Result.Caves.contains(externalCave)) {
            //Overwrite existing entry
            Result.Caves.insert(externalCave, CaveStationLookups[i]);

            //Update the region's station lookup
            cwCave* cave = Region->cave(i);
            cave->setStationPositionModel(CaveStationLookups[i]);
        }
    }
}

/**
 * @brief cwLinePlotTask::encodeCaveNames
 *
 * This encode's the cave's name. Allows survex to properly parse funny cavewhere cave names.
 */
void cwLinePlotTask::encodeCaveNames()
{
    //Encode the the cave's index into the cave's name
    for(int i = 0; i < Region->caveCount(); i++) {
        cwCave* cave = Region->cave(i);
        QString caveNameNoSpaces = cave->name().remove(" ");
        cave->setName(QString("%1-%2").arg(i).arg(caveNameNoSpaces));
    }
}

/**
 * @brief cwLinePlotTask::initializeCaveStationLookups
 */
void cwLinePlotTask::initializeCaveStationLookups()
{
    int numCaves = Region->caveCount();
    CaveStationLookups.reserve(numCaves);
    CaveStationLookups.resize(numCaves);

    //Go through all the caves and get the current station lookup
    //This will allows us to see which stations have change during the line plot task
    for(int i = 0; i < numCaves; i++) {
        cwCave* cave = Region->cave(i);
        CaveStationLookups[i] = cave->stationPositionLookup();
    }
}

/**
 * @brief cwLinePlotTask::setStationAsChanged
 * @param stationName
 *
 * This adds the station to the changed list, and also adds all pointers that referance
 * that station to the list as well
 */
void cwLinePlotTask::setStationAsChanged(int caveIndex, QString stationName)
{
    cwCave* cave = Region->cave(caveIndex);  //Get the local copy of the cave

    //Add the cave to the results with empty station lookup
    cwCave* externalCave = RegionOriginalPointers.Caves.at(caveIndex).Cave;
    Result.Caves.insert(externalCave, cwStationPositionLookup());

    for(int tripIndex = 0; tripIndex < cave->tripCount(); tripIndex++) {
        cwTrip* trip = cave->trip(tripIndex);
        if(trip->hasStation(stationName)) {

            //Get the trip that has been updated
            //Warning, don't us externalTrip, it's not part of this thread
            cwTrip* externalTrip = RegionOriginalPointers.Caves.at(caveIndex).Trips.at(tripIndex).Trip;

            //Add the trip to the trips that have changes
            Result.Trips.insert(externalTrip);
        }

        int scrapIndex = 0;
        foreach(cwNote* note, trip->notes()->notes()) {
            for(int i = 0; i < note->scraps().size(); i++) {
                cwScrap* scrap = note->scrap(i);
                if(scrap->hasStation(stationName)) {

                    //Get the trip that has been updated
                    //Warning, don't us externalScrap, it's not part of this thread
                    cwScrap* externalScrap = RegionOriginalPointers.Caves.at(caveIndex).Trips.at(tripIndex).Scraps.at(scrapIndex);

                    //Add the scrap to the scraps that have changed
                    Result.Scraps.insert(externalScrap);
                }

                scrapIndex++;
            }
        }
    }
}

/**
 * @brief cwLinePlotTask::TripDataPtrs::TripDataPtrs
 * @param trip
 *
 * Copies all the scrap pointers out of the trip.  This allows the LinePlotTask to show exactly
 * what has changed
 */
cwLinePlotTask::TripDataPtrs::TripDataPtrs(cwTrip *trip)
{
    Trip = trip;
    foreach(cwNote* note, trip->notes()->notes()) {
        foreach(cwScrap* scrap, note->scraps()) {
            Scraps.append(scrap);
        }
    }
}

/**
 * @brief cwLinePlotTask::CaveDataPtrs::CaveDataPtrs
 * @param cave
 *
 * Copies all the trip pointers out of the trip.  This allows the LinePlotTask to show exactly
 * what has changed
 */
cwLinePlotTask::CaveDataPtrs::CaveDataPtrs(cwCave *cave)
{
    Cave = cave;
    foreach(cwTrip* trip, cave->trips()) {
        Trips.append(cwLinePlotTask::TripDataPtrs(trip));
    }
}

/**
 * @brief cwLinePlotTask::CaveDataPtrs::CaveDataPtrs
 * @param cave
 *
 * Copies all the cave pointers out of the trip.  This allows the LinePlotTask to show exactly
 * what has changed
 */
cwLinePlotTask::RegionDataPtrs::RegionDataPtrs(const cwCavingRegion &region)
{
    foreach(cwCave* cave, region.caves()) {
        Caves.append(cwLinePlotTask::CaveDataPtrs(cave));
    }
}



