/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWLOOPCLOSERTASK_H
#define CWLOOPCLOSERTASK_H

//Qt includes
#include <QMultiHash>

//Our includes
#include "cwTask.h"
#include "cwStation.h"
#include "cwShot.h"
class cwCavingRegion;
class cwCave;
class cwSurveyChunk;

class cwLoopCloserTask : public cwTask
{
    Q_OBJECT
public:
    explicit cwLoopCloserTask(QObject *parent = 0);

    //Inputs
    void setRegion(cwCavingRegion* region);

    //

signals:

public slots:

protected:
    void runTask();

private:
    class cwEdgeSurveyChunk {
    public:
        void setStations(QList<cwStation> stations) { Stations = stations; }
        QList<cwStation> stations() const { return Stations; }

        void setShots(QList<cwShot> shots) { Shots = shots; }
        QList<cwShot> shot() const { return Shots; }

        cwEdgeSurveyChunk* split(QString stationName);

    private:
        QList<cwStation> Stations;
        QList<cwShot> Shots;
    };

    /**
     * @brief The cwEdgeProcessor class
     *
     * This class process edgeSurveyChunks and splits them into groups of shots and station that
     * have no intersection.  Intersection only happen at the end of
     */
    class cwMainEdgeProcessor {
    public:

       QList<cwEdgeSurveyChunk *> mainEdges(cwCave* cave);

    private:
        void addEdgeChunk(cwEdgeSurveyChunk *chunk);
        void addStationInEdgeChunk(cwEdgeSurveyChunk* chunk);
        void splitOnStation(cwStation station);

        QSet<cwEdgeSurveyChunk *> ResultingEdges;
        QMultiHash<QString, cwEdgeSurveyChunk*> Lookup;
    };

    class cwLoopDetector {
    public:
        void process(QList<cwEdgeSurveyChunk*> edges);

        QList<cwEdgeSurveyChunk*> loopEdges() const;
        QList<cwEdgeSurveyChunk*> legEdges() const;

    private:
        QList<cwEdgeSurveyChunk*> LoopEdges;
        QList<cwEdgeSurveyChunk*> LegEdges;
    };

    cwCavingRegion* Region;

    void processCave(cwCave* cave);
    void findLegsAndLoops(cwCave* cave);

    void printEdges(QList<cwEdgeSurveyChunk*> edges) const;
    void checkBasicEdges(QList<cwEdgeSurveyChunk*> edges) const;



//    void addEdgeChunk(cwEdgeSurveyChunk *chunk,
//                      QList<cwEdgeSurveyChunk *> &resultingEdges,
//                      QMultiHash<QString, cwEdgeSurveyChunk*> &chunkLoopup);
//    void splitOnStation(cwStation station,
//                        QMultiHash<QString, cwEdgeSurveyChunk*> &chunkLoopup);

};

#endif // CWLOOPCLOSERTASK_H
