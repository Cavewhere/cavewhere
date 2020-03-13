/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwUsedStationTaskManager.h"
#include "cwUsedStationsTask.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"

cwUsedStationTaskManager::cwUsedStationTaskManager(QObject *parent) :
    QObject(parent),
    Cave(nullptr)
{
    Task = new cwUsedStationsTask();
    connect(Task, SIGNAL(shouldRerun()), SLOT(calculateUsedStations()));
    connect(Task, SIGNAL(usedStations(QList<QString>)), SLOT(setUsedStations(QList<QString>)));
}

cwUsedStationTaskManager::~cwUsedStationTaskManager() {
    Task->stop();
    Task->waitToFinish(cwTask::IgnoreRestart);
    delete Task;
}

/**
  If listen to cave changes is set to true,
  */
void cwUsedStationTaskManager::setListenToChanges(bool listen) {
    if(ListenToChanges != listen) {
        ListenToChanges = listen;

        if(ListenToChanges) {
            if(cave() != nullptr) {
                connectCave(cave());
            } else if(trip() != nullptr) {
                connectTrip(trip());
            }
            calculateUsedStations();
        } else {
            if(cave() != nullptr) {
                disconnectCave(cave());
            } else if(trip() != nullptr) {
                disconnectTrip(trip());
            }
        }
    }
}

/**
  Sets the cave for the manager
  */
void cwUsedStationTaskManager::setCave(cwCave* cave) {
    Q_ASSERT(trip() == nullptr);
    if(Cave != cave) {
        if(Cave != nullptr) {
            disconnectCave(Cave);
        }

        Cave = cave;

        if(Cave != nullptr) {
            connectCave(Cave);
            calculateUsedStations();
        }

        emit caveChanged();
    }
}

/**
  This starts the running the used station task on an external thread
  */
void cwUsedStationTaskManager::calculateUsedStations() {
    if(Task->isReady()) {
        QList<cwStation> stations;
        if(!Cave.isNull()) {
            stations = Cave->stations();
        } else if(!Trip.isNull()) {
            stations = Trip->stations();
        }


        if(!stations.isEmpty()) {
            QMetaObject::invokeMethod(Task,
                                      "setSettings",
                                      Qt::AutoConnection,
                                      Q_ARG(cwUsedStationsTask::Settings, TaskSettings));
            QMetaObject::invokeMethod(Task, //Object
                                      "setStationNames", //Function
                                      Qt::AutoConnection, //Connection to the function
                                      Q_ARG(QList<cwStation>, stations)); //Arguments to the function

            Task->start();
        }
    } else {
        Task->restart();
    }
}

/**
  Called when the task has completed, stations are set to this object
  */
void cwUsedStationTaskManager::setUsedStations(QList<QString> stations) {
    UsedStations = stations;
    emit usedStationsChanged();
}


/**
 * @brief cwUsedStationTaskManager::connectAddedTrips
 * @param parent
 * @param begin
 * @param end
 *
 * Connections the trips that where added
 */
void cwUsedStationTaskManager::connectAddedTrips(QModelIndex parent, int begin, int end)
{
    Q_UNUSED(parent);
    Q_ASSERT(dynamic_cast<cwCave*>(sender()) != nullptr);
    cwCave* cave = static_cast<cwCave*>(sender());

    for(int i = begin; i <= end; i++) {
        cwTrip* trip = cave->trip(i);
        connectTrip(trip);
    }
}

/**
 * @brief cwUsedStationTaskManager::connectAddedChunks
 * @param begin
 * @param end
 *
 * Connections the chunks that were added
 */
void cwUsedStationTaskManager::connectAddedChunks(int begin, int end)
{
    Q_ASSERT(dynamic_cast<cwTrip*>(sender()) != nullptr);
    cwTrip* trip = static_cast<cwTrip*>(sender());

    for(int i = begin; i <= end; i++) {
        cwSurveyChunk* chunk = trip->chunk(i);
        connectChunk(chunk);
    }

}

/**
 * @brief cwUsedStationTaskManager::disconnectRemovedTrips
 * @param parent
 * @param begin
 * @param end
 *
 * Disconnects the trips that were removed
 */
void cwUsedStationTaskManager::disconnectRemovedTrips(QModelIndex parent, int begin, int end)
{
    Q_UNUSED(parent);
    Q_ASSERT(dynamic_cast<cwCave*>(sender()) != nullptr);
    cwCave* cave = static_cast<cwCave*>(sender());

    for(int i = begin; i <= end; i++) {
        cwTrip* trip = cave->trip(i);
        disconnectTrip(trip);
    }
}

/**
 * @brief cwUsedStationTaskManager::disconnectRemovedChunks
 * @param begin
 * @param end
 *
 * Disconnects the chunks that were removed
 */
void cwUsedStationTaskManager::disconnectRemovedChunks(int begin, int end)
{
    Q_ASSERT(dynamic_cast<cwTrip*>(sender()) != nullptr);
    cwTrip* trip = static_cast<cwTrip*>(sender());

    for(int i = begin; i <= end; i++) {
        cwSurveyChunk* chunk = trip->chunk(i);
        disconnectChunk(chunk);
    }
}

/**
 * @brief cwUsedStationTaskManager::filterChunkDataChanged
 * @param role
 * @param index
 *
 * Runs calculateUsedStations if the role == StationNameRole ie. the name of a station has changed.
 * Index is unused.
 */
void cwUsedStationTaskManager::filterChunkDataChanged(cwSurveyChunk::DataRole role, int index)
{
    Q_UNUSED(index);
    if(role == cwSurveyChunk::StationNameRole) {
        calculateUsedStations();
    }
}

/**
  \brief Helper function to setListenToCaveChanges

  This hooks up the cave to the task or disconnect it, depending on the value of ListenToCaveCHanges.

  If ListenToCaveChanges is true it connects it, else it disconnects it.

  If Cave is null this function does nothing
  */
void cwUsedStationTaskManager::hookupCaveToTask() {
    if(Cave == nullptr) { return; }
    if(ListenToChanges) {
        connect(Cave, SIGNAL(stationAddedToCave(QString)), SLOT(calculateUsedStations()));
        connect(Cave, SIGNAL(stationRemovedFromCave(QString)), SLOT(calculateUsedStations()));
    } else {
        disconnect(Cave, 0, this, 0);
    }
}

/**
 * @brief cwUsedStationTaskManager::connectCave
 * @param cave
 *
 * Connects the cave to calculate used stations, when it changes
 */
void cwUsedStationTaskManager::connectCave(cwCave *cave)
{
    connect(cave, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(calculateUsedStations()));
    connect(cave, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(connectAddedTrips(QModelIndex,int,int)));
    connect(cave, SIGNAL(rowsRemoved(QModelIndex,int,int)), this, SLOT(calculateUsedStations()));

    foreach(cwTrip* trip, cave->trips()) {
        connectTrip(trip);
    }
}

/**
 * @brief cwUsedStationTaskManager::connectTrip
 * @param trip
 *
 * Connects the trip to calculate used stations, when it changes
 */
void cwUsedStationTaskManager::connectTrip(cwTrip *trip)
{
    connect(trip, SIGNAL(chunksInserted(int,int)), this, SLOT(calculateUsedStations()));
    connect(trip, SIGNAL(chunksInserted(int,int)), this, SLOT(connectAddedChunks(int,int)));
    connect(trip, SIGNAL(chunksRemoved(int,int)), this, SLOT(calculateUsedStations()));

    foreach(cwSurveyChunk* chunk, trip->chunks()) {
        connectChunk(chunk);
    }
}

/**
 * @brief cwUsedStationTaskManager::connectChunk
 * @param chunk
 *
 * Connects the chunk to calculate used stations, when it changes
 */
void cwUsedStationTaskManager::connectChunk(cwSurveyChunk *chunk)
{
    connect(chunk, SIGNAL(stationsAdded(int,int)), this, SLOT(calculateUsedStations()));
    connect(chunk, SIGNAL(dataChanged(cwSurveyChunk::DataRole,int)),
            this, SLOT(filterChunkDataChanged(cwSurveyChunk::DataRole,int)));
    connect(chunk, SIGNAL(stationsRemoved(int,int)), this, SLOT(calculateUsedStations()));
}

/**
 * @brief cwUsedStationTaskManager::disconnectCave
 * @param cave
 *
 * Disconnects the cave from the manager
 */
void cwUsedStationTaskManager::disconnectCave(cwCave *cave)
{
    foreach(cwTrip* trip, cave->trips()) {
        disconnectTrip(trip);
    }
    disconnect(cave, 0, this, 0);
}

/**
 * @brief cwUsedStationTaskManager::disconnectTrip
 * @param trip
 * Disconnects the trip from the manager
 */
void cwUsedStationTaskManager::disconnectTrip(cwTrip *trip)
{
    foreach(cwSurveyChunk* chunk, trip->chunks()) {
        disconnectChunk(chunk);
    }
    disconnect(trip, 0, this, 0);
}

/**
 * @brief cwUsedStationTaskManager::disconnectChunk
 * @param chunk
 *
 * Disconnects the chunk from the manager
 */
void cwUsedStationTaskManager::disconnectChunk(cwSurveyChunk *chunk)
{
    disconnect(chunk, 0, this, 0);
}

/**
  Get's the cave for this task
  */
cwCave* cwUsedStationTaskManager::cave() {
    return Cave;
}

/**
* @brief cwUsedStationTaskManager::trip
* @return The current trip
*/
cwTrip* cwUsedStationTaskManager::trip() const {
    return Trip;
}

/**
* @brief cwUsedStationTaskManager::setTrip
* @param trip - Sets the current trip that the task manager will calculate use stations for
*
* Make sure the current cave isn't set. Call setCave(nullptr) before setting the trip
*/
void cwUsedStationTaskManager::setTrip(cwTrip* trip) {
    Q_ASSERT(cave() == nullptr);
    if(Trip != trip) {
        if(Trip != nullptr) {
            disconnectTrip(Trip);
        }

        Trip = trip;

        if(Trip != nullptr) {
            connectTrip(trip);
            calculateUsedStations();
        }

        emit tripChanged();
    }
}

/**
* @brief cwUsedStationTaskManager::setBold
* @param bold - If the results will have riched text bold elements. False results will be plained
* text
*/
void cwUsedStationTaskManager::setBold(bool bold) {
    if(this->bold() != bold) {
        TaskSettings.setBold(bold);
        calculateUsedStations();
        emit boldChanged();
    }
}

/**
* @brief cwUsedStationTaskManager::setAbbreviated
* @param abbreviated - If true, the manager will return abbreviated results.
*
* For example, if true:
*    A 40-50
*
* False:
*    Survey A, 40 to 60
*/
void cwUsedStationTaskManager::setAbbreviated(bool abbreviated) {
    if(this->abbreviated() != abbreviated) {
        TaskSettings.setAbbreviated(abbreviated);
        calculateUsedStations();
        emit abbreviatedChanged();
    }
}

/**
* @brief cwUsedStationTaskManager::setOnlyLargestRange
* @param onlyLargestRange - If true, the manager will only return one element in the results.
*
* The element will have the largest number of stations in that survey. This is useful, for finding
* the general majority stations for a survey. This isn't very useful when set to true for a cave.
*/
void cwUsedStationTaskManager::setOnlyLargestRange(bool onlyLargestRange) {
    if(this->onlyLargestRange() != onlyLargestRange) {
        TaskSettings.setOnlyLargestRange(onlyLargestRange);
        calculateUsedStations();
        emit onlyLargestRangeChanged();
    }
}



