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
class cwFindUnconnectedSurveyChunksTask : public cwTask
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

    void setCave(cwCave* cave);
    QList<Result> results() const;

protected:
    void runTask();

private:
    cwCave* Cave;

    QSet<cwSurveyChunk*> Connected; //Connect to the main cave
    QSet<cwSurveyChunk*> Unconnected; //Not connected
    QMultiHash<cwSurveyChunk*, cwSurveyChunk*> ChunkConnectedTo; //How the chunks are connect to each other

    QHash<cwSurveyChunk*, QSet<QString>> ChunkToStationSet; //Index of cwSurveyChunk to station names set

    QList<Result> Results;

    void indexChunkToStations();

    bool isChunksConnect(cwSurveyChunk* chunk1, cwSurveyChunk* chunk2) const;
    QSet<QString> chunkToStationNameSet(cwSurveyChunk* chunk) const;
    void addConnectedTo(cwSurveyChunk* chunk1, cwSurveyChunk* chunk2);
    void updateConnectedTo(cwSurveyChunk* chunk);
    void updateConnectedTo();
    void addFirstChunkAsConnected();
    void insertChunkToConnected(cwSurveyChunk* chunk);
    void updateResults();


};

#endif // CWFINDUNCONNECTEDSURVEYTASK_H
