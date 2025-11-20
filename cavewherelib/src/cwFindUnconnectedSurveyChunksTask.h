/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWFINDUNCONNECTEDSURVEYTASK_H
#define CWFINDUNCONNECTEDSURVEYTASK_H

//Our includes
#include "cwTask.h"
#include "cwError.h"
#include "cwGlobals.h"
#include "cwCaveData.h"
class cwCave;
class cwSurveyChunk;

//Qt includes
#include <QHash>
#include <QMultiHash>
#include <QSet>

/**
 * @brief The cwSurveyChunkConnectedToCaveTask class
 *
 * This returns a list of unconnected survey chunks. A unconnected survey chunk is a survey leg
 * that is floating in the cave, and isn't connected to the rest of the cave
 */
class CAVEWHERE_LIB_EXPORT cwFindUnconnectedSurveyChunksTask : public cwTask
{
    Q_OBJECT

public:
    class Result {
    public:
        Result() : TripIndex(-1), SurveyChunkIndex(-1) {}
        Result(int tripIndex, int surveyChunkIndex, cwError error) : TripIndex(tripIndex), SurveyChunkIndex(surveyChunkIndex), Error(error) {}

        int TripIndex;
        int SurveyChunkIndex;
        cwError Error;
    };

    cwFindUnconnectedSurveyChunksTask();

    void setCave(const cwCaveData& cave);
    QList<Result> results() const;

protected:
    void runTask();

private:
    cwCaveData Cave;

    QSet<const cwSurveyChunk*> Connected; //Connect to the main cave
    QSet<const cwSurveyChunk*> Unconnected; //Not connected
    QMultiHash<const cwSurveyChunk*, const cwSurveyChunk*> ChunkConnectedTo; //How the chunks are connect to each other

    QHash<const cwSurveyChunk*, QSet<QString>> ChunkToStationSet; //Index of cwSurveyChunk to station names set

    QList<Result> Results;

    void indexChunkToStations(const cwCave* cave);

    bool isChunksConnect(const cwSurveyChunk *chunk1, const cwSurveyChunk *chunk2) const;
    QSet<QString> chunkToStationNameSet(const cwSurveyChunk* chunk) const;
    void addConnectedTo(const cwSurveyChunk* chunk1, const cwSurveyChunk* chunk2);
    void updateConnectedTo(const cwCave *cave, const cwSurveyChunk *chunk);
    void updateConnectedTo(const cwCave* cave);
    void addFirstChunkAsConnected(const cwCave* cave);
    void insertChunkToConnected(const cwSurveyChunk *chunk);
    void updateResults(const cwCave* cave);


};

#endif // CWFINDUNCONNECTEDSURVEYTASK_H
