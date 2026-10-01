/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#include "cwSurveyChunkSignaler.h"
#include "cwCavingRegion.h"
#include "cwSurveyNode.h"
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
*
* Supports switching regions and clearing with nullptr: the previous
* region's connections (structural and user-added, down to chunks and
* calibrations) are severed first, so a retired region's signals can no
* longer reach this signaler or its receivers.
*/
void cwSurveyChunkSignaler::setRegion(cwCavingRegion* region) {
    if(Region == region) {
        return;
    }

    if(!Region.isNull()) {
        disconnectNode(Region->rootNode());
    }

    Region = region;

    if(!Region.isNull()) {
        connectNode(Region->rootNode());
    }

    emit regionChanged();
}

/**
 * @brief cwSurveyChunkSignaler::addConnectionToCaves
 * @param signal
 * @param reciever
 * @param slot
 *
 * This adds a connection dynamically to every survey node in the region at
 * every depth: caves, and the nodes nested under them. A node added later, at
 * any depth, gets the connection too.
 */
void cwSurveyChunkSignaler::addConnectionToCaves(const char *signal, QObject *reciever, const char *slot)
{
    Connection connection(signal, reciever, slot);

    Q_ASSERT(!CaveConnections.contains(connection));

    if(!Region.isNull()) {
        for(cwSurveyNode* node : Region->rootNode()->allNodes()) {
            connection.connect(node);
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
 * This adds a connection dynamically to all the trips in the region, at every
 * depth. A trip added later, or a node added with trips, gets the connection too.
 */
void cwSurveyChunkSignaler::addConnectionToTrips(const char *signal,
                                                 QObject *reciever,
                                                 const char *slot)
{
    Connection connection(signal, reciever, slot);

    Q_ASSERT(!TripConnections.contains(connection));

    if(!Region.isNull()) {
        for(cwTrip* trip : Region->rootNode()->allTrips()) {
            connection.connect(trip);
        }
    }

    TripConnections.append(connection);
}

void cwSurveyChunkSignaler::addConnectionToTripCalibrations(const char *signal, QObject *reciever, const char *slot)
{
    Connection connection(signal, reciever, slot);

    Q_ASSERT(!TripCalibrationConnections.contains(connection));

    if(!Region.isNull()) {
        for(cwTrip* trip : Region->rootNode()->allTrips()) {
            connection.connect(trip->calibrations());
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
 * This adds a connection dynamically to all the cwSurveyChunk in the region. If more cwSurveyChunk, trips, or nodes are added to the
 * region the connection created for each additional cwSurveyChunk.
 */
void cwSurveyChunkSignaler::addConnectionToChunks(const char *signal, QObject *reciever, const char *slot)
{
    Connection connection(signal, reciever, slot);

    Q_ASSERT(!ChunkConnections.contains(connection));

    if(!Region.isNull()) {
        for(cwTrip* trip : Region->rootNode()->allTrips()) {
            for(cwSurveyChunk* chunk : trip->chunks()) {
                connection.connect(chunk);
            }
        }
    }

    ChunkConnections.append(connection);
}

/**
  \brief Connects \a node, its trips, and every node below it.

  The region's root takes the structural connections only: the user-added node
  connections are for caves and the nodes under them, and the root is neither.
  */
void cwSurveyChunkSignaler::connectNode(cwSurveyNode* node) {
    connect(node, &cwSurveyNode::insertedNodes, this, &cwSurveyChunkSignaler::connectAddedNodes);
    connect(node, &cwSurveyNode::beginRemoveNodes, this, &cwSurveyChunkSignaler::disconnectRemovedNodes);
    connect(node, &cwSurveyNode::insertedTrips, this, &cwSurveyChunkSignaler::connectAddedTrips);
    connect(node, &cwSurveyNode::beginRemoveTrips, this, &cwSurveyChunkSignaler::disconnectRemovedTrips);
    if(!node->isRoot()) {
        connectAll(node, CaveConnections); //Connect to all user added connections
    }

    const QList<cwTrip*> trips = node->trips();
    for(cwTrip* trip : trips) {
        connectTrip(trip);
    }

    const QList<cwSurveyNode*> children = node->childNodes();
    for(cwSurveyNode* child : children) {
        connectNode(child);
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
    for(int i = 0; i < trip->chunkCount(); i++) {
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
 * @brief cwSurveyChunkSignaler::disconnectNode
 * @param node
 *
 * The inverse of connectNode: disconnects \a node, its trips, and every node
 * below it.
 */
void cwSurveyChunkSignaler::disconnectNode(cwSurveyNode *node)
{
    disconnect(node, nullptr, this, nullptr);
    if(!node->isRoot()) {
        disconnectAll(node, CaveConnections);
    }

    const QList<cwTrip*> trips = node->trips();
    for(cwTrip* trip : trips) {
        disconnectTrip(trip);
    }

    const QList<cwSurveyNode*> children = node->childNodes();
    for(cwSurveyNode* child : children) {
        disconnectNode(child);
    }
}

/**
 * @brief cwSurveyChunkSignaler::disconnectTrip
 * @param trip
 */
void cwSurveyChunkSignaler::disconnectTrip(cwTrip *trip)
{
    disconnect(trip, nullptr, this, nullptr);
    disconnectAll(trip, TripConnections);
    disconnectAll(trip->calibrations(), TripCalibrationConnections);

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
    disconnect(chunk, nullptr, this, nullptr);
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
    for(const Connection& connection : connections) {
        connection.connect(sender);
    }
}

void cwSurveyChunkSignaler::disconnectAll(QObject *sender, const QList<cwSurveyChunkSignaler::Connection> &connections) const
{
    for(const Connection& connection : connections) {
        connection.disconnect(sender);
    }
}

/**
 * @brief cwSurveyChunkSignaler::connectAddedNodes
 * @param beginIndex
 * @param endIndex
 */
void cwSurveyChunkSignaler::connectAddedNodes(int beginIndex, int endIndex)
{
    Q_ASSERT(qobject_cast<cwSurveyNode*>(sender()) != nullptr);
    const cwSurveyNode* parentNode = static_cast<cwSurveyNode*>(sender());

    for(int i = beginIndex; i <= endIndex; i++) {
        connectNode(parentNode->childNode(i));
    }
}

/**
 * @brief cwSurveyChunkSignaler::connectAddedTrips
 * @param beginIndex
 * @param endIndex
 */
void cwSurveyChunkSignaler::connectAddedTrips(int beginIndex, int endIndex)
{
    Q_ASSERT(qobject_cast<cwSurveyNode*>(sender()) != nullptr);
    const cwSurveyNode* node = static_cast<cwSurveyNode*>(sender());

    for(int i = beginIndex; i <= endIndex; i++) {
        connectTrip(node->trip(i));
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
 * @brief cwSurveyChunkSignaler::disconnectRemovedNodes
 * @param beginIndex
 * @param endIndex
 */
void cwSurveyChunkSignaler::disconnectRemovedNodes(int beginIndex, int endIndex)
{
    Q_ASSERT(qobject_cast<cwSurveyNode*>(sender()) != nullptr);
    const cwSurveyNode* parentNode = static_cast<cwSurveyNode*>(sender());

    for(int i = beginIndex; i <= endIndex; i++) {
        disconnectNode(parentNode->childNode(i));
    }
}

/**
 * @brief cwSurveyChunkSignaler::disconnectRemovedTrips
 * @param beginIndex
 * @param endIndex
 */
void cwSurveyChunkSignaler::disconnectRemovedTrips(int beginIndex, int endIndex)
{
    Q_ASSERT(qobject_cast<cwSurveyNode*>(sender()) != nullptr);
    const cwSurveyNode* node = static_cast<cwSurveyNode*>(sender());

    for(int i = beginIndex; i <= endIndex; i++) {
        disconnectTrip(node->trip(i));
    }
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
    QObject::connect(sender, Signal.constData(), Reciever, Slot.constData(), Qt::UniqueConnection);
}

void cwSurveyChunkSignaler::Connection::disconnect(QObject *sender) const
{
    QObject::disconnect(sender, Signal.constData(), Reciever, Slot.constData());
}
