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
#include "cwSurveyChunkSignaler.h"
#include "cwErrorModel.h"
#include "cwErrorListModel.h"


cwLinePlotManager::cwLinePlotManager(QObject *parent) :
    QObject(parent)
{
    Region = nullptr;
    GLLinePlot = nullptr;

    SurveySignaler = new cwSurveyChunkSignaler(this);

    SurveySignaler->addConnectionToCaves(SIGNAL(insertedTrips(int,int)), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToCaves(SIGNAL(removedTrips(int,int)), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToCaves(SIGNAL(nameChanged()), this, SLOT(runSurvex()));

    SurveySignaler->addConnectionToTrips(SIGNAL(chunksInserted(int,int)), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToTrips(SIGNAL(chunksRemoved(int,int)), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToTrips(SIGNAL(nameChanged()), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToTripCalibrations(SIGNAL(calibrationsChanged()), this, SLOT(runSurvex()));

    SurveySignaler->addConnectionToChunks(SIGNAL(shotsAdded(int,int)), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToChunks(SIGNAL(shotsRemoved(int,int)), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToChunks(SIGNAL(stationsAdded(int,int)), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToChunks(SIGNAL(stationsRemoved(int,int)), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToChunks(SIGNAL(dataChanged(cwSurveyChunk::DataRole,int)), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToChunks(SIGNAL(calibrationsChanged()), this, SLOT(runSurvex()));
    SurveySignaler->addConnectionToChunkCalibrations(SIGNAL(calibrationsChanged()), this, SLOT(runSurvex()));

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
    connect(Region, SIGNAL(insertedCaves(int,int)), SLOT(runSurvex()));
    connect(Region, SIGNAL(removedCaves(int,int)), SLOT(runSurvex()));

    SurveySignaler->setRegion(Region);

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

        if(cave->isStationPositionLookupStale()) {
            caveIsStale = true;
        }
    }

    if(caveIsStale) {
        runSurvex();
    }
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
 * @brief cwLinePlotManager::updateUnconnectedChunkErrors
 *
 * This will clear all the survey chunk errors and add survey chunk error's that exist. Currently
 * the only errors that is added to the whole survey chunk, are unconnected survey chunk error.
 */
void cwLinePlotManager::updateUnconnectedChunkErrors(cwCave* cave,
                                                     const cwLinePlotTask::LinePlotCaveData& caveData)
{

    //Append unconnected errors
    if(caveData.unconnectedChunkError().size() > 0) {
        foreach(auto errorResult, caveData.unconnectedChunkError()) {
            cwErrorModel* model = cave->trip(errorResult.TripIndex)->chunk(errorResult.SurveyChunkIndex)->errorModel();
            model->errors()->append(errorResult.Error);
            UnconnectedChunks.append(model->errors());
        }
    }
}

/**
 * @brief cwLinePlotManager::clearUnconnectedChunkErrors
 *
 * This goes throught all clears all the connected cwSurveyChunk
 */
void cwLinePlotManager::clearUnconnectedChunkErrors()
{
    foreach(auto errorList, UnconnectedChunks) {
        errorList->clear();
    }
    UnconnectedChunks.clear();
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

    //Clear all the unconnected chunk errors from the previous run
    clearUnconnectedChunkErrors();

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

        updateUnconnectedChunkErrors(cave, caveData);

        if(caveData.hasStationPositionsChanged()) {
            cave->setStationPositionLookup(caveData.stationPositions());
            cave->setSurveyNetwork(caveData.network());
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

