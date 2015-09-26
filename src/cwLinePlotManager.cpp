/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwLinePlotManager.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwShot.h"
#include "cwSurveyChunk.h"
#include "cwLinePlotTask.h"
#include "cwGLLinePlot.h"
#include "cwTripCalibration.h"
#include "cwSurveyNoteModel.h"
#include "cwScrap.h"
#include "cwDebug.h"
#include "cwLength.h"

cwLinePlotManager::cwLinePlotManager(QObject *parent) :
    QObject(parent)
{
    Region = nullptr;
    GLLinePlot = nullptr;

    LinePlotThread = new QThread(this);
    LinePlotThread->start();

    LinePlotTask = new cwLinePlotTask();
    LinePlotTask->setThread(LinePlotThread);
    connect(LinePlotTask, SIGNAL(shouldRerun()), SLOT(runSurvex())); //So the task is rerun
    connect(LinePlotTask, SIGNAL(finished()), SLOT(updateLinePlot()));
}

cwLinePlotManager::~cwLinePlotManager() {
    LinePlotTask->stop();

    QMetaObject::invokeMethod(LinePlotThread, "quit");
    LinePlotThread->wait();

    delete LinePlotTask;
}

/**
  \brief Sets the region that this manager will listen to
  */
void cwLinePlotManager::setRegion(cwCavingRegion* region) {
    Region = region;
    if(Region == nullptr) { return; }

    //Connect all signal from the region
    connect(Region, SIGNAL(destroyed(QObject*)), SLOT(regionDestroyed(QObject*)));
    connect(Region, SIGNAL(insertedCaves(int,int)), SLOT(runSurvex()));
    connect(Region, SIGNAL(insertedCaves(int,int)), SLOT(connectAddedCaves(int,int)));
    connect(Region, SIGNAL(removedCaves(int,int)), SLOT(runSurvex()));

    //Connect all sub data
    connectCaves(Region);

    runSurvex();
}

void cwLinePlotManager::setGLLinePlot(cwGLLinePlot* linePlot) {
    GLLinePlot = linePlot;
    updateLinePlot();
}

/**
 * @brief cwLinePlotManager::waitToFinish
 *
 * Will cause the LinePlotManager to block until the underlying task is finished. This is useful
 * for unit testing.
 */
void cwLinePlotManager::waitToFinish()
{
    LinePlotTask->waitToFinish();
}

/**
  \brief Connects all the caves in the region to this object
  */
void cwLinePlotManager::connectCaves(cwCavingRegion* region) {
    bool caveIsStale = false;

    for(int i = 0; i < region->caveCount(); i++) {
        cwCave* cave = region->cave(i);
        connectCave(cave);

        if(cave->isStationPositionLookupStale()) {
            caveIsStale = true;
        }
    }

    if(caveIsStale) {
        runSurvex();
    }
}

/**
  \brief Connects a cave
  */
void cwLinePlotManager::connectCave(cwCave* cave) {
    connect(cave, SIGNAL(insertedTrips(int,int)), SLOT(runSurvex()));
    connect(cave, SIGNAL(insertedTrips(int,int)), SLOT(connectAddedTrips(int,int)));
    connect(cave, SIGNAL(removedTrips(int,int)), SLOT(runSurvex()));
    connect(cave, SIGNAL(nameChanged()), SLOT(runSurvex()));
    connectTrips(cave);
}

/**
  \brief Connects all the trips in the cave to this object
  */
void cwLinePlotManager::connectTrips(cwCave* cave) {
    for(int i = 0; i < cave->tripCount(); i++) {
        cwTrip* trip = cave->trip(i);
        connectTrip(trip);
    }
}

/**
  \brief Connects a trip
  */
void cwLinePlotManager::connectTrip(cwTrip* trip) {
    connect(trip, SIGNAL(chunksInserted(int,int)), SLOT(runSurvex()));
    connect(trip, SIGNAL(chunksInserted(int,int)), SLOT(connectAddedChunks(int,int)));
    connect(trip, SIGNAL(chunksRemoved(int,int)), SLOT(runSurvex()));
    connect(trip, SIGNAL(nameChanged()), SLOT(runSurvex()));
    connect(trip->calibrations(), SIGNAL(calibrationsChanged()), SLOT(runSurvex()));
    connectChunks(trip);
}

/**
  \brief Connects all the trips in the chunk to this object
  */
void cwLinePlotManager::connectChunks(cwTrip* trip) {
    for(int i = 0; i < trip->numberOfChunks(); i++) {
        cwSurveyChunk* chunk = trip->chunk(i);
        connectChunk(chunk);
    }
}

/**
  \brief Connects as chunk
  */
void cwLinePlotManager::connectChunk(cwSurveyChunk* chunk) {
    connect(chunk, SIGNAL(shotsAdded(int,int)), SLOT(runSurvex()));
    connect(chunk, SIGNAL(shotsRemoved(int,int)), SLOT(runSurvex()));
    connect(chunk, SIGNAL(stationsAdded(int,int)), SLOT(runSurvex()));
    connect(chunk, SIGNAL(stationsRemoved(int,int)), SLOT(runSurvex()));
    connect(chunk, SIGNAL(dataChanged(cwSurveyChunk::DataRole,int)), SLOT(runSurvex()));
}

/**
 * @brief cwLinePlotManager::validateResultsData
 * @param results
 *
 * This goes through the results and removes
 */
void cwLinePlotManager::validateResultsData(cwLinePlotTask::LinePlotResultData &results)
{
    QMap<cwCave*, cwLinePlotTask::LinePlotCaveData> validCaves;
    QSet<cwTrip*> validTrips;
    QSet<cwScrap*> validScraps;

    //Update all the positions for all the caves
    foreach(cwCave* cave, Region->caves()) {
        if(results.caveData().contains(cave)) {

            //Add this cave to the valid, needs to be updated list
            validCaves.insert(cave, results.caveData().value(cave));

            foreach(cwTrip* trip, cave->trips()) {
                if(results.trips().contains(trip)) {

                    //Add this trip to the valid trips
                    validTrips.insert(trip);

                    foreach(cwNote* note, trip->notes()->notes()) {
                        foreach(cwScrap* scrap, note->scraps()) {
                            if(results.scraps().contains(scrap)) {

                                //Add this scrap to the valid scrap
                                validScraps.insert(scrap);
                            }
                        }
                    }
                }
            }
        }
    }

    results.setCaveData(validCaves);
    results.setTrip(validTrips);
    results.setScraps(validScraps);
}

/**
 * @brief cwLinePlotManager::markCaveStationsAsStale
 *
 * This will go through all the caves in the region and mark them as stale
 * This is useful, to make sure that the cave data is upto date. If the user
 * closes cavewhere before the line plot is re-processed.
 */
void cwLinePlotManager::setCaveStationLookupAsStale(bool isStale)
{
    foreach(cwCave* cave, Region->caves()) {
        cave->setStationPositionLookupStale(isStale);
    }
}

/**
  \brief Called when the region adds more caves
  */
void cwLinePlotManager::connectAddedCaves(int beginIndex, int endIndex) {
    for(int i = beginIndex; i <= endIndex; i++) {
        cwCave* cave = Region->cave(i);
        connectCave(cave);
    }
}

/**
  \brief Called when the cave adds more trips
  */
void cwLinePlotManager::connectAddedTrips(int beginIndex, int endIndex) {
    Q_ASSERT(qobject_cast<cwCave*>(sender()) != nullptr);
    cwCave* cave = static_cast<cwCave*>(sender());
    for(int i = beginIndex; i <= endIndex; i++) {
        cwTrip* trip = cave->trip(i);
        connectTrip(trip);
    }
}

/**
  \brief Called when the trip adds more chunks
  */
void cwLinePlotManager::connectAddedChunks(int beginIndex, int endIndex) {
    Q_ASSERT(qobject_cast<cwTrip*>(sender()) != nullptr);
    cwTrip* trip = static_cast<cwTrip*>(sender());
    for(int i = beginIndex; i <= endIndex; i++) {
        cwSurveyChunk* chunk = trip->chunk(i);
        connectChunk(chunk);
    }
}

/**
  \brief The region is going to be destroyed

  Prevent a stall pointer
  */
void cwLinePlotManager::regionDestroyed(QObject* region) {
    if(region == Region) {
        Region = nullptr;
    }
}

/**
  \brief Run the line plot task
  */
void cwLinePlotManager::runSurvex() {
    if(Region != nullptr) {
        if(LinePlotTask->isReady()) {
//            qDebug() << "Running the task";
            setCaveStationLookupAsStale(true);
            LinePlotTask->setData(*Region);
            LinePlotTask->start();
        } else {
            //Restart the survex
//            qDebug() << "Restart plot task";
            LinePlotTask->restart();
        }
    }
}

/**
  \brief Updates the line plot, and all the station positions for the
  line region
  */
void cwLinePlotManager::updateLinePlot() {
    if(!LinePlotTask->isReady()) { return; }
    if(Region == nullptr) { return; }

    cwLinePlotTask::LinePlotResultData resultData = LinePlotTask->linePlotData();

    //Validate all the objects in resultData, remove any that were delete before the task was over
    validateResultsData(resultData); //Modifies resultData inplace

    //Update all the positions for all the caves that need to be updated
    //Also update the length and depth information
    QMapIterator<cwCave*, cwLinePlotTask::LinePlotCaveData> iter(resultData.caveData());
    while(iter.hasNext()) {
        iter.next();
        cwCave* cave = iter.key();
        cwLinePlotTask::LinePlotCaveData caveData = iter.value();

        if(caveData.hasStationPositionsChanged()) {
            cave->setStationPositionLookup(caveData.stationPositions());
        }

        if(caveData.hasDepthLengthChanged()) {
            //Update the cave's depth and length
            double length = cwUnits::convert(caveData.length(), cwUnits::Meters, (cwUnits::LengthUnit)cave->length()->unit());
            double depth = cwUnits::convert(caveData.depth(), cwUnits::Meters, (cwUnits::LengthUnit)cave->depth()->unit());

            cave->length()->setValue(length);
            cave->depth()->setValue(depth);
        }
    }

    //Update the 3D plot
    if(GLLinePlot != nullptr) {
        GLLinePlot->setPoints(resultData.stationPositions());
        GLLinePlot->setIndexes(resultData.linePlotIndexData());
    }

    //Mark all caves as up todate
    setCaveStationLookupAsStale(false);

    emit stationPositionInCavesChanged(resultData.caveData().keys());
    emit stationPositionInTripsChanged(resultData.trips().toList());
    emit stationPositionInScrapsChanged(resultData.scraps().toList());
}

