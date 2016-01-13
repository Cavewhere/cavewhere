/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

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
#include "cwSurveyChunk.h"
#include "cwDebug.h"
#include "cwLength.h"
#include "cwStationValidator.h"
#include "cwErrorModel.h"

//Qt includes
#include <QDebug>
#include <QTime>
#include <QThread>

//Std includes
#include <math.h>

/**
 * @brief cwLinePlotTask::LinePlotCaveData::setDepth
 * @param depth - This is the cave depth calculated by the task
 */
cwLinePlotTask::LinePlotCaveData::LinePlotCaveData() :
    DepthLengthChanged(false),
    Depth(0.0),
    Length(0.0),
    StationPostionsChanged(false)
{
}


cwLinePlotTask::cwLinePlotTask(QObject *parent) :
    cwTask(parent)
{
    Region = new cwCavingRegion();

    SurvexFile = new QTemporaryFile(this);
    SurvexFile->open();
    SurvexFile->setAutoRemove(false);
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

    UnconnectedSurveyChunkTask = new cwFindUnconnectedSurveyChunksTask();
    UnconnectedSurveyChunkTask->setParentTask(this);
}

cwLinePlotTask::~cwLinePlotTask()
{
    delete Region;
}

/**
  \brief Set's the data for the line plot task
  */
void cwLinePlotTask::setData(const cwCavingRegion& region) {
    if(!isReady()) {
        qWarning() << "Can't set cave data for LinePlotTask, while it's running";
        return;
    }

    //Move region to the current thread
    QMetaObject::invokeMethod(this,
                              "moveCaveRegionToThread",
                              Qt::BlockingQueuedConnection,
                              Q_ARG(QThread*, QThread::currentThread()));

    //Copy over all region data
    *Region = region;  

    //Move region back to task thread
    moveCaveRegionToThread(thread());

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
//     qDebug() << "\n-------------------------------------";
    if(!isRunning()) {
        done();
        return;
    }

//    qDebug() << "Running line plot task";

    //Clear the previous results
    Result.clear();

    try {

        //Check for errors
        checkForErrors();

        //Change all the cave names, such that survex can handle them correctly
        encodeCaveNames();

        //Initilize the cave station lookup, from previous run
        initializeCaveStationLookups();

        Time.start();
        exportData();

    } catch(QString) {
        done();
        return;
    }
}

/**
  \brief Exports the data to
  */
void cwLinePlotTask::exportData() {
    if(!isRunning()) {
        done();
        return;
    }

//    qDebug() << "Running export data status:" << status();
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

//    qDebug() << "Running cavern on " << SurvexFile->fileName() << "Status" << status();
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

//    qDebug() << "Covert 3d to xml" << "Status" << status() << CavernTask->output3dFileName();
    PlotSauceTask->setSurvex3DFile(CavernTask->output3dFileName());
    PlotSauceTask->start();
}

void cwLinePlotTask::readXML() {
    if(!isRunning()) {
        done();
        return;
    }

//    qDebug() << "Reading xml" << "Status" << status() << PlotSauceTask->outputXMLFile();
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

//    qDebug() << "Generating centerline geometry" << status();
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

    //Update the depth and length of the cave
    updateDepthLength();

//    qDebug() << "Finished running linePlotTask:" << Time.elapsed() << "ms";
    done();
}

/**
 * @brief cwLinePlotTask::updateStationPositionForCaves
 * @param stationPostions
 */
void cwLinePlotTask::updateStationPositionForCaves(const cwStationPositionLookup& stationPostions) {

    //Index all the stations for quick lookup
    indexStations();

    //Splite up stationPostions for each indiviual cave
    QVector<cwStationPositionLookup> caveStationLookups = splitLookupByCave(stationPostions);

    //Update all the lookups that are part of this class
    updateInteralCaveStationLookups(caveStationLookups);

    //Update all cave station position models
    updateExteralCaveStationLookups();
}

/**
 * @brief cwLinePlotTask::updateDepthLength
 */
void cwLinePlotTask::updateDepthLength()
{
    QVector<cwLinePlotGeometryTask::LengthAndDepth> caveLengthAndDepth = CenterlineGeometryTask->cavesLengthAndDepths();
    Q_ASSERT(Region->caveCount() == caveLengthAndDepth.size());

    //Update all cave station position models
    for(int i = 0; i < Region->caveCount(); i++) {
        //Get the region's caves
        cwLinePlotGeometryTask::LengthAndDepth lengthAndDepth = caveLengthAndDepth.at(i);

        LinePlotCaveData& caveData = createLinePlotCaveDataAt(i);
        caveData.setLength(lengthAndDepth.length());
        caveData.setDepth(lengthAndDepth.depth());
    }
}

/**
 * @brief cwLinePlotTask::checkForErrors
 */
void cwLinePlotTask::checkForErrors()
{
    for(int i = 0; i < Region->caveCount(); i++) {
        cwCave* cave = Region->cave(i);
        UnconnectedSurveyChunkTask->setCave(cave);
        UnconnectedSurveyChunkTask->start();
        auto errorResults = UnconnectedSurveyChunkTask->results();
        if(errorResults.size() > 0) {
            LinePlotCaveData& caveData = createLinePlotCaveDataAt(i);
            caveData.setUnconnectedChunkError(errorResults);
        }
    }

    for(int i = 0; i < Region->caveCount(); i++) {
        cwCave* cave = Region->cave(i);
        if(cave->errorModel()->fatalCount() > 0) {
            throw QString("Found error in cave");
        }
    }

    if(!Result.Caves.isEmpty()) {
        throw QString("Found error in caves");
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
//        QString caveNameNoSpaces = cave->name().remove(" ");
        cave->setName(QString("%1").arg(i)); //.arg(caveNameNoSpaces));
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
//    cwCave* cave = Region->cave(caveIndex);  //Get the local copy of the cave

    //Add the cave to the results with empty station lookup
    addEmptyStationLookup(caveIndex);

    const StationTripScrapLookup& lookup = TripLookups[caveIndex];
    QList<int> tripIndexes = lookup.trips(stationName);
    QList<QPair<int, int> > scrapIndexes = lookup.scraps(stationName);

    foreach(int tripIndex, tripIndexes) {
        //Get the trip that has been updated
        //Warning, don't us externalTrip, it's not part of this thread
        cwTrip* externalTrip = RegionOriginalPointers.Caves.at(caveIndex).Trips.at(tripIndex).Trip;

        //Add the trip to the trips that have changes
        Result.Trips.insert(externalTrip);
    }

    for(int i = 0; i < scrapIndexes.size(); i++) {
        QPair<int, int> index = scrapIndexes.at(i);
        int tripIndex = index.first;
        int scrapIndex = index.second;

        //Get the trip that has been updated
        //Warning, don't us externalScrap, it's not part of this thread
        cwScrap* externalScrap = RegionOriginalPointers.Caves.at(caveIndex).Trips.at(tripIndex).Scraps.at(scrapIndex);
        cwTrip* externalTrip = RegionOriginalPointers.Caves.at(caveIndex).Trips.at(tripIndex).Trip;

        //Add the scrap to the scraps that have changed
        Result.Scraps.insert(externalScrap);
        Result.Trips.insert(externalTrip); //Need to add the parent trip, because it might not be added, because scrap may exist in a differnt trip than the data
    }
}

/**
 * @brief cwLinePlotTask::indexStations
 *
 * This will go through each cave and it's trips and scraps and map them by station name
 *
 * This will provide quick lookup for a cwTrip or cwScrap based the station name
 */
void cwLinePlotTask::indexStations()
{
    //Resize the number of trip lookups based on the number of caves
    TripLookups.resize(Region->caveCount());

    for(int i = 0; i < Region->caveCount() && isRunning(); i++) {
        TripLookups[i] = StationTripScrapLookup(Region->cave(i));
    }
}

/**
 * @brief cwLinePlotTask::createLinePlotCaveDataAt
 * Creates LinePlotCaveData at index (if it doesn't already exist) and returns it
 */
cwLinePlotTask::LinePlotCaveData &cwLinePlotTask::createLinePlotCaveDataAt(int index)
{
    //Get extrenal cave pointer
    cwCave* externalCave = RegionOriginalPointers.Caves.at(index).Cave;

    if(!Result.Caves.contains(externalCave)) {
        Result.Caves[externalCave] = LinePlotCaveData();
    }

    LinePlotCaveData& caveData = Result.Caves[externalCave];

    return caveData;
}

/**
 * @brief cwLinePlotTask::splitLookupByCave
 * @param stationPostions
 * @return
 */
QVector<cwStationPositionLookup> cwLinePlotTask::splitLookupByCave(const cwStationPositionLookup &stationPostions)
{
    double positionPrecision = 3; //position to 3 digits
    double positionFactor = pow(10.0, positionPrecision);

    QString stationPattern = cwStationValidator::validCharactersRegex().pattern();
    QRegExp regex(QString("(\\d+)\\.(%1)").arg(stationPattern));

    //This vector is populated with all the stations for each cave
    QVector<cwStationPositionLookup> caveStations;
    caveStations.resize(CaveStationLookups.size());

    QMapIterator<QString, QVector3D> iter(stationPostions.positions());
    while( iter.hasNext() ) {
        iter.next();

        QString name = iter.key();
        QVector3D position = iter.value();

        //Cut off positions to 3 digits
        position.setX(qRound(position.x() * positionFactor) / positionFactor);
        position.setY(qRound(position.y() * positionFactor) / positionFactor);
        position.setZ(qRound(position.z() * positionFactor) / positionFactor);

        if(regex.exactMatch(name)) {

            QString caveIndexString = regex.cap(1); //Extract the index
            QString stationName = regex.cap(2);//Extract the station
            QString caveName = caveIndexString;

            bool okay;
            int caveIndex = caveIndexString.toInt(&okay);

            if(!okay) {
                qDebug() << "Can't covent caveIndex is not an int:" << caveIndexString << LOCATION;
                return QVector<cwStationPositionLookup>();
            }

            //Make sure the index is good
            if(caveIndex < 0 || caveIndex >= Region->caveCount()) {
                qDebug() << "CaveIndex is bad:" << caveIndex << LOCATION;
                return QVector<cwStationPositionLookup>();
            }

            cwCave* cave = Region->cave(caveIndex);

            //Make sure the caveName is valid
            if(caveName.compare(cave->name(), Qt::CaseInsensitive) != 0) {
                qDebug() << "CaveName is invalid:" << caveName << "looking for" << cave->name() << LOCATION;
                return QVector<cwStationPositionLookup>();
            }

            cwStationPositionLookup& lookup = caveStations[caveIndex];
            lookup.setPosition(stationName, position);

        } else {
            qDebug() << "Couldn't match: " << name << "This is a bug!" << LOCATION;
        }
    }

    return caveStations;
}

/**
 * @brief cwLinePlotTask::updateInteralCaveStationLookups
 * @param caveStations
 *
 * Go through all the stations and compare the to there previous positions
 * If they have been updated then, this will add them to the station changed
 */
void cwLinePlotTask::updateInteralCaveStationLookups(QVector<cwStationPositionLookup> caveStations)
{
    //Go through all the stations and compare the to there previous positions
    //If they have been updated then, this will add them to the station changed
    for(int i = 0; i < caveStations.size(); i++) {
        cwStationPositionLookup& newLookup = caveStations[i];
        cwStationPositionLookup& oldLookup = CaveStationLookups[i];

        if(newLookup.positions().size() != oldLookup.positions().size()) {
            //This adds the station lookup as changed if the new looup has delete or added stations
            addEmptyStationLookup(i);
        }

        QMap<QString, QVector3D> newPositions = newLookup.positions();
        QMap<QString, QVector3D> oldPositions = oldLookup.positions();

        foreach(QString stationName, newPositions.keys()) {
            if(oldPositions.contains(stationName)) {
                //Compare new point with old point
                QVector3D newPoint = newPositions.value(stationName);
                QVector3D oldPoint = oldPositions.value(stationName);
                if(newPoint != oldPoint) {
                    setStationAsChanged(i, stationName);
                }
            } else {
                //New point
                setStationAsChanged(i, stationName);
            }
        }

        //Update the new station lookup with the new station lookup
        CaveStationLookups[i] = caveStations[i];
    }
}

/**
 * @brief cwLinePlotTask::updateExteralCaveStationLookups
 *
 * Update all cave station position models
 */
void cwLinePlotTask::updateExteralCaveStationLookups()
{
    //Update all cave station position models
    for(int i = 0; i < Region->caveCount(); i++) {
        //Get extrenal cave pointer
        cwCave* externalCave = RegionOriginalPointers.Caves.at(i).Cave;
        if(Result.Caves.contains(externalCave)) {

            LinePlotCaveData caveData;
            caveData.setStationPositions(CaveStationLookups[i]);

            //Overwrite existing entry
            Result.Caves.insert(externalCave, caveData);

            //Update the region's station lookup
            cwCave* cave = Region->cave(i);
            cave->setStationPositionLookup(CaveStationLookups[i]);
        }
    }
}

void cwLinePlotTask::moveCaveRegionToThread(QThread *thread)
{
    Region->moveToThread(thread);
}

/**
 * @brief cwLinePlotTask::addEmptyStationLookup
 * @param caveIndex
 *
 * Adds an emptry station lookup to the results.
 */
void cwLinePlotTask::addEmptyStationLookup(int caveIndex)
{
    cwCave* externalCave = RegionOriginalPointers.Caves.at(caveIndex).Cave;
    if(!Result.Caves.contains(externalCave)) {
        Result.Caves.insert(externalCave, LinePlotCaveData());
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

/**
* @brief cwLinePlotTask::LinePlotResultData::setCaveData
* @param caveData
*
* Clear the results
*/
void cwLinePlotTask::LinePlotResultData::clear()
{
    Caves.clear();
    Trips.clear();
    Scraps.clear();
    StationPositions.clear();
    LinePlotIndexData.clear();
}

/**
 * @brief cwLinePlotTask::StationTripScrapLookup::StationTripScrapLookup
 * @param cave
 *
 * This will go through the cave and index the scrap, and trip information by stationName
 */
cwLinePlotTask::StationTripScrapLookup::StationTripScrapLookup(cwCave *cave)
{
    for(int tripIndex = 0; tripIndex < cave->tripCount(); tripIndex++) {
        cwTrip* trip = cave->trip(tripIndex);
        foreach(cwSurveyChunk* surveyChunk, trip->chunks()) {
            foreach(cwStation station, surveyChunk->stations()) {
                //Add trip to the multi hash
                MapStationToTrip.insertMulti(station.name(), tripIndex);
            }
        }

        int scrapIndex = 0;
        foreach(cwNote* note, trip->notes()->notes()) {
            for(int i = 0; i < note->scraps().size(); i++) {
                cwScrap* scrap = note->scrap(i);

                foreach(cwNoteStation noteStation, scrap->stations()) {
                    MapStationToScrap.insertMulti(noteStation.name(), QPair<int, int>(tripIndex, scrapIndex));
                }

                scrapIndex++;
            }
        }
    }
}



