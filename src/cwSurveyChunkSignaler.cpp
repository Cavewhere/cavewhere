/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#include "cwSurveyChunkSignaler.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwTripCalibration.h"

cwSurveyChunkSignaler::cwSurveyChunkSignaler(QObject *parent) : QObject(parent)
{

}

/**
* @brief cwSurveyChunkSignaler::region
* @return
*/
cwCavingRegion* cwSurveyChunkSignaler::region() const {
    return Region;
}

/**
* @brief cwSurveyChunkSignaler::setRegion
* @param region
*/
void cwSurveyChunkSignaler::setRegion(cwCavingRegion* region) {
    if(Region != region) {
        Region = region;

        //Connect all signal from the region
        connect(Region.data(), &cwCavingRegion::insertedCaves, this, &cwSurveyChunkSignaler::connectAddedCaves);
        connect(Region.data(), &cwCavingRegion::beginRemoveCaves, this, &cwSurveyChunkSignaler::disconnectRemovedCaves);

        //Connect all sub data
        connectCaves(Region);

        emit regionChanged();
    }
}

/**
 * @brief cwSurveyChunkSignaler::addConnectionToCaves
 * @param signal
 * @param reciever
 * @param slot
 *
 * This adds a connection dynamically to all the caves in the region. If more caves are added to the
 * region the connection is added to the newly added cave.
 */
void cwSurveyChunkSignaler::addConnectionToCaves(const char *signal, QObject *reciever, const char *slot)
{
    Connection connection(signal, reciever, slot);

    Q_ASSERT(!CaveConnections.contains(connection));

    if(!Region.isNull()) {
        foreach(cwCave* cave, Region->caves()) {
            connection.connect(cave);
        }
    }

    CaveConnections.append(connection);
}

/**
 * @brief cwSurveyChunkSignaler::addConnectionToTrips
 * @param signal
 * @param reciever
 * @param slot
 *
 * This adds a connection dynamically to all the trips in the region. If more trips or caves are added to the
 * region the connection created for each additional trip.
 */
void cwSurveyChunkSignaler::addConnectionToTrips(const char *signal,
                                                 QObject *reciever,
                                                 const char *slot)
{
    Connection connection(signal, reciever, slot);

    Q_ASSERT(!TripConnections.contains(connection));

    if(!Region.isNull()) {
        foreach(cwCave* cave, Region->caves()) {
            foreach(cwTrip* trip, cave->trips()) {
                connection.connect(trip);
            }
        }
    }

    TripConnections.append(connection);
}

void cwSurveyChunkSignaler::addConnectionToTripCalibrations(const char *signal, QObject *reciever, const char *slot)
{
    Connection connection(signal, reciever, slot);

    Q_ASSERT(!TripCalibrationConnections.contains(connection));

    if(!Region.isNull()) {
        foreach(cwCave* cave, Region->caves()) {
            foreach(cwTrip* trip, cave->trips()) {
                connection.connect(trip->calibrations());
            }
        }
    }

    TripCalibrationConnections.append(connection);
}

/**
 * @brief cwSurveyChunkSignaler::addConnectionToChunks
 * @param signal
 * @param reciever
 * @param slot
 *
 * This adds a connection dynamically to all the cwSurveyChunk in the region. If more cwSurveyChunk, trips, or caves are added to the
 * region the connection created for each additional cwSurveyChunk.
 */
void cwSurveyChunkSignaler::addConnectionToChunks(const char *signal, QObject *reciever, const char *slot)
{
    Connection connection(signal, reciever, slot);

    Q_ASSERT(!ChunkConnections.contains(connection));

    if(!Region.isNull()) {
        foreach(cwCave* cave, Region->caves()) {
            foreach(cwTrip* trip, cave->trips()) {
                foreach(cwSurveyChunk* chunk, trip->chunks()) {
                    connection.connect(chunk);
                }
            }
        }
    }

    ChunkConnections.append(connection);
}

/**
 * @brief cwSurveyChunkSignaler::connectCaves
 * @param region
 *
 * Connects all the caves in the region
 */
void cwSurveyChunkSignaler::connectCaves(cwCavingRegion *region)
{
    foreach(cwCave* cave, region->caves()) {
        connectCave(cave);
    }
}

/**
  \brief Connects a cave
  */
void cwSurveyChunkSignaler::connectCave(cwCave* cave) {
    connect(cave, &cwCave::insertedTrips, this, &cwSurveyChunkSignaler::connectAddedTrips);
    connect(cave, &cwCave::beginRemoveTrips, this, &cwSurveyChunkSignaler::disconnectRemovedTrips);
    connectAll(cave, CaveConnections); //Connect to all user added connections
    connectTrips(cave);
}

/**
  \brief Connects all the trips in the cave to this object
  */
void cwSurveyChunkSignaler::connectTrips(cwCave* cave) {
    for(int i = 0; i < cave->tripCount(); i++) {
        cwTrip* trip = cave->trip(i);
        connectTrip(trip);
    }
}

/**
  \brief Connects a trip
  */
void cwSurveyChunkSignaler::connectTrip(cwTrip* trip) {
    connect(trip, &cwTrip::chunksInserted, this, &cwSurveyChunkSignaler::connectAddedChunks);
    connect(trip, &cwTrip::chunksAboutToBeRemoved, this, &cwSurveyChunkSignaler::disconnectRemovedChunks);
    connectAll(trip, TripConnections); //Connect to all user added connections
    connectAll(trip->calibrations(), TripCalibrationConnections);
    connectChunks(trip);
}

/**
  \brief Connects all the trips in the chunk to this object
  */
void cwSurveyChunkSignaler::connectChunks(cwTrip* trip) {
    for(int i = 0; i < trip->numberOfChunks(); i++) {
        cwSurveyChunk* chunk = trip->chunk(i);
        connectChunk(chunk);
    }
}

/**
  \brief Connects as chunk
  */
void cwSurveyChunkSignaler::connectChunk(cwSurveyChunk* chunk) {
    connectAll(chunk, ChunkConnections); //Connect to all user added connections
}

/**
 * @brief cwSurveyChunkSignaler::disconnectCave
 * @param cave
 */
void cwSurveyChunkSignaler::disconnectCave(cwCave *cave)
{

    disconnectAll(cave, CaveConnections);

    if(cave->hasTrips()) {
        disconnectTrips(cave, 0, cave->tripCount() - 1);
    }
}

/**
 * @brief cwSurveyChunkSignaler::disconnectTrips
 * @param cave
 * @param beginIndex
 * @param endIndex
 */
void cwSurveyChunkSignaler::disconnectTrips(cwCave *cave, int beginIndex, int endIndex)
{
    for(int i = beginIndex; i <= endIndex; i++) {
        cwTrip* trip = cave->trip(i);
        disconnectTrip(trip);
    }
}

/**
 * @brief cwSurveyChunkSignaler::disconnectTrip
 * @param trip
 */
void cwSurveyChunkSignaler::disconnectTrip(cwTrip *trip)
{
    disconnectAll(trip, TripConnections);

    if(!trip->chunks().isEmpty()) {
        disconnectSurveyChunks(trip, 0, trip->chunks().size() - 1);
    }
}

/**
 * @brief cwSurveyChunkSignaler::disconnectSurveyChunks
 * @param trip
 * @param beginIndex
 * @param endIndex
 */
void cwSurveyChunkSignaler::disconnectSurveyChunks(cwTrip *trip, int beginIndex, int endIndex)
{
    for(int i = beginIndex; i <= endIndex; i++) {
        cwSurveyChunk* chunk = trip->chunk(i);
        disconnectSurveyChunk(chunk);
    }
}

/**
 * @brief cwSurveyChunkSignaler::disconnectSurveyChunk
 * @param chunk
 */
void cwSurveyChunkSignaler::disconnectSurveyChunk(cwSurveyChunk *chunk)
{
    disconnectAll(chunk, ChunkConnections);
}

/**
 * @brief cwSurveyChunkSignaler::connect
 * @param reciever
 * @param connections
 *
 * This creates connections from the sender, slot of the reciever, singal found in the connctions list. Each
 * Connection in the list is connect to the reciever.
 */
void cwSurveyChunkSignaler::connectAll(QObject *sender, const QList<cwSurveyChunkSignaler::Connection> &connections) const
{
    foreach(Connection connection, connections) {
        connection.connect(sender);
    }
}

void cwSurveyChunkSignaler::disconnectAll(QObject *sender, const QList<cwSurveyChunkSignaler::Connection> &connections) const
{
    foreach(Connection connection, connections) {
        connection.disconnect(sender);
    }
}

/**
 * @brief cwSurveyChunkSignaler::connectAddedCaves
 * @param beginIndex
 * @param endIndex
 */
void cwSurveyChunkSignaler::connectAddedCaves(int beginIndex, int endIndex)
{
    for(int i = beginIndex; i <= endIndex; i++) {
        cwCave* cave = Region->cave(i);
        connectCave(cave);
    }
}

/**
 * @brief cwSurveyChunkSignaler::connectAddedTrips
 * @param beginIndex
 * @param endIndex
 */
void cwSurveyChunkSignaler::connectAddedTrips(int beginIndex, int endIndex)
{
    Q_ASSERT(dynamic_cast<cwCave*>(sender()) != nullptr);
    cwCave* cave = static_cast<cwCave*>(sender());

    for(int i = beginIndex; i <= endIndex; i++) {
        cwTrip* trip = cave->trip(i);
        connectTrip(trip);
    }

}

/**
 * @brief cwSurveyChunkSignaler::connectAddedChunks
 * @param beginIndex
 * @param endIndex
 */
void cwSurveyChunkSignaler::connectAddedChunks(int beginIndex, int endIndex)
{
    Q_ASSERT(dynamic_cast<cwTrip*>(sender()) != nullptr);
    cwTrip* trip = static_cast<cwTrip*>(sender());

    for(int i = beginIndex; i <= endIndex; i++) {
        cwSurveyChunk* chunk = trip->chunk(i);
        connectChunk(chunk);
    }
}

/**
 * @brief cwSurveyChunkSignaler::disconnectRemovedCaves
 * @param beginIndex
 * @param endIndex
 */
void cwSurveyChunkSignaler::disconnectRemovedCaves(int beginIndex, int endIndex)
{
    for(int i = beginIndex; i <= endIndex; i++) {
        cwCave* cave = Region->cave(i);
        disconnectCave(cave);
    }
}

/**
 * @brief cwSurveyChunkSignaler::disconnectRemovedTrips
 * @param beginIndex
 * @param endIndex
 */
void cwSurveyChunkSignaler::disconnectRemovedTrips(int beginIndex, int endIndex)
{
    Q_ASSERT(dynamic_cast<cwCave*>(sender()) != nullptr);
    cwCave* cave = static_cast<cwCave*>(sender());
    disconnectTrips(cave, beginIndex, endIndex);
}

/**
 * @brief cwSurveyChunkSignaler::disconnectRemovedChunks
 * @param beginIndex
 * @param endIndex
 */
void cwSurveyChunkSignaler::disconnectRemovedChunks(int beginIndex, int endIndex)
{
    Q_ASSERT(dynamic_cast<cwTrip*>(sender()) != nullptr);
    cwTrip* trip = static_cast<cwTrip*>(sender());
    disconnectSurveyChunks(trip, beginIndex, endIndex);
}


void cwSurveyChunkSignaler::Connection::connect(QObject *sender) const
{
    QObject::connect(sender, Signal.constData(), Reciever, Slot.constData());
}

void cwSurveyChunkSignaler::Connection::disconnect(QObject *sender) const
{
    QObject::disconnect(sender, Signal.constData(), Reciever, Slot.constData());
}
