//Our includes
#include "cwLinePlotManager.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwStationReference.h"
#include "cwShot.h"
#include "cwSurveyChunk.h"
#include "cwLinePlotTask.h"

cwLinePlotManager::cwLinePlotManager(QObject *parent) :
    QObject(parent)
{
    Region = NULL;

    LinePlotThread = new QThread(this);
    LinePlotThread->start();

    LinePlotTask = new cwLinePlotTask();
    LinePlotTask->setThread(LinePlotThread);
    connect(LinePlotTask, SIGNAL(shouldRerun()), SLOT(runSurvex())); //So the task is rerun
}

/**
  \brief Sets the region that this manager will listen to
  */
void cwLinePlotManager::setRegion(cwCavingRegion* region) {
    Region = region;
    if(Region == NULL) { return; }

    //Connect all signal from the region
    connect(Region, SIGNAL(destroyed(QObject*)), SLOT(regionDestroyed(QObject*)));
    connect(Region, SIGNAL(insertedCaves(int,int)), SLOT(runSurvex()));
    connect(Region, SIGNAL(insertedCaves(int,int)), SLOT(connectAddedCaves(int,int)));
    connect(Region, SIGNAL(removedCaves(int,int)), SLOT(runSurvex()));

    //Connect all sub data
    connectCaves(Region);
}

/**
  \brief Connects all the caves in the region to this object
  */
void cwLinePlotManager::connectCaves(cwCavingRegion* region) {
    for(int i = 0; i < region->caveCount(); i++) {
        cwCave* cave = region->cave(i);
        connectCave(cave);
    }
}

/**
  \brief Connects a cave
  */
void cwLinePlotManager::connectCave(cwCave* cave) {
    connect(cave, SIGNAL(insertedTrips(int,int)), SLOT(runSurvex()));
    connect(cave, SIGNAL(insertedTrips(int,int)), SLOT(connectAddedTrips(int,int)));
    connect(cave, SIGNAL(removedTrips(int,int)), SLOT(runSurvex()));
    connect(cave, SIGNAL(nameChanged(QString)), SLOT(runSurvex()));
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
    connect(trip, SIGNAL(nameChanged(QString)), SLOT(runSurvex()));
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
    connect(chunk, SIGNAL(ShotsAdded(int,int)), SLOT(runSurvex()));
    connect(chunk, SIGNAL(ShotsRemoved(int,int)), SLOT(runSurvex()));
    connect(chunk, SIGNAL(StationsAdded(int,int)), SLOT(runSurvex()));
    connect(chunk, SIGNAL(StationsRemoved(int,int)), SLOT(runSurvex()));
    connect(chunk, SIGNAL(ShotsAdded(int,int)), SLOT(connectAddedShots(int,int)));
    connect(chunk, SIGNAL(StationsAdded(int,int)), SLOT(connectAddedStations(int,int)));
    connectShots(chunk);
    connectStations(chunk);
}

/**
  \brief Connects all the shots in the trip
  */
void cwLinePlotManager::connectShots(cwSurveyChunk* chunk) {
    for(int i = 0; i < chunk->ShotCount(); i++) {
        cwShot* shot = chunk->Shot(i);
        connectShot(shot);
    }
}

/**
  \brief connects a shot
  */
void cwLinePlotManager::connectShot(cwShot* shot) {
    connect(shot, SIGNAL(DistanceChanged()), SLOT(runSurvex()));
    connect(shot, SIGNAL(CompassChanged()), SLOT(runSurvex()));
    connect(shot, SIGNAL(BackCompassChanged()), SLOT(runSurvex()));
    connect(shot, SIGNAL(ClinoChanged()), SLOT(runSurvex()));
    connect(shot, SIGNAL(BackClinoChanged()), SLOT(runSurvex()));
}

/**
  \brief Connects all the stations in the trip to this object
  */
void cwLinePlotManager::connectStations(cwSurveyChunk* chunk) {
    for(int i = 0; i < chunk->StationCount(); i++) {
        cwStationReference* station = chunk->Station(i);
        connectStation(station);
    }
}

/**
  \brief Connect to the station
  */
void cwLinePlotManager::connectStation(cwStationReference* station) {
    connect(station, SIGNAL(nameChanged()), SLOT(runSurvex()));
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
    cwTrip* trip = static_cast<cwTrip*>(sender());
    for(int i = beginIndex; i <= endIndex; i++) {
        cwSurveyChunk* chunk = trip->chunk(i);
        connectChunk(chunk);
    }
}

/**
  \brief Called when the chunks add more stations
  */
void cwLinePlotManager::connectAddedStations(int beginIndex, int endIndex) {
    cwSurveyChunk* chunk = static_cast<cwSurveyChunk*>(sender());
    for(int i = beginIndex; i <= endIndex; i++) {
        cwStationReference* station = chunk->Station(i);
        connectStation(station);
    }
}

/**
  \brief Called when the chunks add more shots
  */
void cwLinePlotManager::connectAddedShots(int beginIndex, int endIndex) {
    cwSurveyChunk* chunk = static_cast<cwSurveyChunk*>(sender());
    for(int i = beginIndex; i <= endIndex; i++) {
        cwShot* shot = chunk->Shot(i);
        connectShot(shot);
    }
}
/**
  \brief The region is going to be destroyed

  Prevent a stall pointer
  */
void cwLinePlotManager::regionDestroyed(QObject* region) {
    if(region == Region) {
        Region = NULL;
    }
}

/**
  \brief Run the line plot task
  */
void cwLinePlotManager::runSurvex() {
    if(Region != NULL) {
        //qDebug() << "----Run survex----" << LinePlotTask->status();
        if(LinePlotTask->status() == cwTask::Stopped) {
            qDebug() << "Running the task";
            //qDebug() << "\tSetting data!" << LinePlotTask->status();
            QMetaObject::invokeMethod(LinePlotTask, "setData", Qt::AutoConnection,
                                      Q_ARG(cwCavingRegion, *Region));
            LinePlotTask->start();
        } else {
            //Restart the survex
            qDebug() << "Restart plot task";
            LinePlotTask->restart();
        }
    }
}

/**
  \brief Called when the manager should be rerun
  */
 void cwLinePlotManager::reRunSurvex() {
     if(Rerun) {
         qDebug() << "***Rerun***";
         runSurvex();
     }
 }
