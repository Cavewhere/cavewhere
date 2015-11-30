/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwFindUnconnectedSurveyChunksTask.h"
#include "cwSurveyChunk.h"
#include "cwCave.h"
#include "cwTrip.h"

cwFindUnconnectedSurveyChunksTask::cwFindUnconnectedSurveyChunksTask()
{

}

/**
 * @brief cwFindUnconnectedSurveyChunksTask::setCave
 * @param cave
 */
void cwFindUnconnectedSurveyChunksTask::setCave(cwCave *cave)
{
    Cave = cave;
}

/**
 * @brief cwFindUnconnectedSurveyChunksTask::results
 * @return
 */
QList<cwFindUnconnectedSurveyChunksTask::Result> cwFindUnconnectedSurveyChunksTask::results() const
{
    return Results;
}

/**
 * @brief cwFindUnconnectedSurveyChunksTask::runTask
 */
void cwFindUnconnectedSurveyChunksTask::runTask()
{

    //Do a full refresh
    Connected.clear();
    Unconnected.clear();
    ChunkConnectedTo.clear();
    ChunkToStationSet.clear();
    Results.clear();

    //Index the cwSurveyChunk to station name sets
    indexChunkToStations();

    //Update the database which chunks are connected to other chunks
    updateConnectedTo();

    //Add the first survey chunk in the first trip as Connected
    //This will recursively find all the other connected trips, the connected set
    //will be populated after this call
    addFirstChunkAsConnected();

    //update the unconnect chunks, this should be empty if there's no unconnected chunks
    updateResults();

    done();
}

/**
 * @brief cwSurveyChunkConnectedToCaveTask::indexAllStationsToChunk
 */
void cwFindUnconnectedSurveyChunksTask::indexChunkToStations()
{
    foreach(cwTrip* trip, Cave->trips()) {
        foreach(cwSurveyChunk* chunk, trip->chunks()) {
            ChunkToStationSet.insert(chunk, chunkToStationNameSet(chunk));
        }
    }
}


/**
 * @brief cwSurveyChunkConnectedToCaveTask::isChunksConnect
 * @param chunk1
 * @param chunk2
 * @return True if the chunk1 and chunk2 are connected
 *
 * Make sure chunk1 and chunk2 are added to ChunkToStationSet
 *
 * Returns false if chunk1 and chunk2 are equal to each other
 */
bool cwFindUnconnectedSurveyChunksTask::isChunksConnect(cwSurveyChunk *chunk1, cwSurveyChunk *chunk2) const
{
    if(chunk1 == chunk2) { return false; }
    Q_ASSERT(ChunkToStationSet.contains(chunk1) && ChunkToStationSet.contains(chunk2));

    QSet<QString> set1 = ChunkToStationSet.value(chunk1);
    QSet<QString> set2 = ChunkToStationSet.value(chunk2);

    for(auto iter = set1.begin(); iter != set1.end(); iter++) {
        if(set2.contains(*iter)) {
            return true;
        }
    }
    return false;
}

/**
 * @brief cwSurveyChunkConnectedToCaveTask::chunkToStationNameSet
 * @param chunk
 * @return
 */
QSet<QString> cwFindUnconnectedSurveyChunksTask::chunkToStationNameSet(cwSurveyChunk *chunk) const
{
    QSet<QString> stationNameSet;
    for(int i = 0; i < chunk->stationCount(); i++) {
        QString name = chunk->data(cwSurveyChunk::StationNameRole, i).toString().toUpper();
        if(!name.isEmpty()) {
            stationNameSet.insert(name);
        }
    }
    return stationNameSet;
}

/**
 * @brief cwSurveyChunkConnectedToCaveTask::addConnectedTo
 * @param chunk1
 * @param chunk2
 *
 * Attempts to index chunk1 and chunk2 if they're connected and aren't already connect. If chunk1
 * and chunk2 aren't connected, this function does nothing. If the chunk1 and chunk2 are already
 * indexed as connected, this function does nothing.
 */
void cwFindUnconnectedSurveyChunksTask::addConnectedTo(cwSurveyChunk *chunk1, cwSurveyChunk *chunk2)
{
    if(isChunksConnect(chunk1, chunk2)) {
        if(!ChunkConnectedTo.contains(chunk1, chunk2)) {
            ChunkConnectedTo.insert(chunk1, chunk2);
            ChunkConnectedTo.insert(chunk2, chunk1);
        }
    }
}

/**
 * @brief cwSurveyChunkConnectedToCaveTask::updateConnectedTo
 * @param chunk
 *
 * Loops through all the chunks in the cave and tries to connect the with chunk.
 * The ChunkConnectedTo database will be update with the connected chunk pairs
 */
void cwFindUnconnectedSurveyChunksTask::updateConnectedTo(cwSurveyChunk *chunk)
{
    foreach(cwTrip* trip, Cave->trips()) {
        foreach(cwSurveyChunk* currentChunk, trip->chunks()) {
            addConnectedTo(chunk, currentChunk);
        }
    }
}

/**
 * @brief cwSurveyChunkConnectedToCaveTask::updateConnectedTo
 *
 * Loops through all the chunks in the cave and connects them to one another
 */
void cwFindUnconnectedSurveyChunksTask::updateConnectedTo()
{
    foreach(cwTrip* trip, Cave->trips()) {
        foreach(cwSurveyChunk* currentChunk, trip->chunks()) {
            updateConnectedTo(currentChunk);
        }
    }
}

/**
 * @brief cwSurveyChunkConnectedToCaveTask::addFirstChunkAsConnected
 */
void cwFindUnconnectedSurveyChunksTask::addFirstChunkAsConnected()
{
    foreach(cwTrip* trip, Cave->trips()) {
        foreach(cwSurveyChunk* chunk, trip->chunks()) {
            if(chunk->isValid()) {
                for(int i = 0; i < chunk->stationCount(); i++) {
                   QString stationName = chunk->data(cwSurveyChunk::StationNameRole, i).toString().toUpper();
                   if(!stationName.isEmpty()) {
                       //Found the first survey chunk that has a valid station name
                       insertChunkToConnected(chunk);
                       return;
                   }
                }
            }
        }
    }
}

/**
 * @brief cwSurveyChunkConnectedToCaveTask::insertChunkToConnected
 * @param chunk
 *
 * This is a recursive function that inserts the chunk into the connected list.
 * All other chunks that are connected tho chunk and it's "child" are also added
 * to the connected list. The connected list are the chunks that are connected to
 * the cave
 */
void cwFindUnconnectedSurveyChunksTask::insertChunkToConnected(cwSurveyChunk *chunk)
{
    if(!Connected.contains(chunk)) {
        Connected.insert(chunk);

        auto chunksConnetedToChunk = ChunkConnectedTo.values(chunk);
        foreach(cwSurveyChunk* currentChunk, chunksConnetedToChunk) {
            insertChunkToConnected(currentChunk);
        }
    }
}

/**
 * @brief cwSurveyChunkConnectedToCaveTask::updateUnconnected
 *
 * This update's a list of unconnected values
 */
void cwFindUnconnectedSurveyChunksTask::updateResults()
{
    Results.clear();

    cwError error;
    error.setMessage("Survey leg isn't connect to the cave");
    error.setType(cwError::Fatal);

    for(int t = 0; t < Cave->tripCount(); t++) {
        cwTrip* trip = Cave->trip(t);
        for(int c = 0; c < trip->numberOfChunks(); c++) {
            cwSurveyChunk* chunk = trip->chunk(c);

            if(!Connected.contains(chunk) && !chunk->isStationAndShotsEmpty()) {
                //Found an unconnect chunk, create a result
                Result result(t, c, error);
                Results.append(result);
            }
        }
    }
}
