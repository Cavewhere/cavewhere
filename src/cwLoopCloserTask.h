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
        QList<cwEdgeSurveyChunk*> splitChunkLocally(cwEdgeSurveyChunk* chunk);

        QSet<cwEdgeSurveyChunk *> ResultingEdges;
        QMultiHash<QString, cwEdgeSurveyChunk*> Lookup;
    };

    class cwLoop {
    public:
        void setEdges(QList<cwEdgeSurveyChunk*> edges) { Edges = edges; }
        QList<cwEdgeSurveyChunk*> edges() const { return Edges; }

    private:
        QList<cwEdgeSurveyChunk*> Edges;
    };

    class cwLoopDetector {
    public:
        void process(QList<cwEdgeSurveyChunk*> edges);

        QList<cwLoop> loops() const { return Loops; }
        QList<cwEdgeSurveyChunk*> legEdges() const;

    private:
        //Input
        QList<cwEdgeSurveyChunk*> Edges;

        //Processing Data
        QMultiHash<QString, cwEdgeSurveyChunk*> EdgeLookup; //Maps first and last station to edgesurveychunk
        QSet<cwEdgeSurveyChunk*> VisitedEdges; //All the edgeSurveyChunks that have been visited already
        QSet<QString> VisitedStations; //All the stations that have been processed

        //Output
        QList<cwLoop> Loops;
        QList<cwEdgeSurveyChunk*> LegEdges;

        void createEdgeLookup();
        void findLoops(QString station, QList<cwEdgeSurveyChunk*> path = QList<cwEdgeSurveyChunk*>()); //Recusive function, depth first search
        void printEdges(QList<cwLoopCloserTask::cwEdgeSurveyChunk*> edges);
    };

    cwCavingRegion* Region;

    void processCave(cwCave* cave);
    void findLegsAndLoops(cwCave* cave);

//    static void printEdges(QList<cwEdgeSurveyChunk*> edges) const;
    void checkBasicEdges(QList<cwEdgeSurveyChunk*> edges) const;

    void printLoops(QList<cwLoop> loops) const;
    void printEdges(QList<cwLoopCloserTask::cwEdgeSurveyChunk*> edges) const;


//    void addEdgeChunk(cwEdgeSurveyChunk *chunk,
//                      QList<cwEdgeSurveyChunk *> &resultingEdges,
//                      QMultiHash<QString, cwEdgeSurveyChunk*> &chunkLoopup);
//    void splitOnStation(cwStation station,
//                        QMultiHash<QString, cwEdgeSurveyChunk*> &chunkLoopup);

};

#endif // CWLOOPCLOSERTASK_H
