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
#include <QVector3D>

//Our includes
#include "cwTask.h"
#include "cwStation.h"
#include "cwShot.h"
class cwCavingRegion;
class cwCave;
class cwSurveyChunk;

//Armadillo
#include <armadillo>

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

    class cwShotVector {
    public:

        enum ShotDirection {
            FrontSite,
            BackSite
        };

        void setVector(arma::mat vector);
        const arma::mat &vector() const;

        void setCovarienceMatrix(arma::mat covarienceMatrix);
        arma::mat covarienceMatrix() const;

        void setDirection(ShotDirection direction);
        ShotDirection direction() const;

        QString fromStation() const;
        void setFromStation(QString fromStation);

        QString toStation() const;
        void setToStation(QString toStation);

    private:
        arma::mat Vector; //3x1 matrix
        arma::mat CovarianceMatrix; //3x3 matrix
        ShotDirection Direction;

        QString FromStation;
        QString ToStation;
    };

    class cwEdgeSurveyChunk {
    public:
        void setStations(QList<cwStation> stations) { Stations = stations; }
        QList<cwStation> stations() const { return Stations; }

        void setShots(QList<cwShot> shots) { Shots = shots; }
        QList<cwShot> shots() const { return Shots; }

        cwEdgeSurveyChunk* split(QString stationName);

        void addShotVector(cwShotVector direction);
        QList<cwShotVector> shotVectors() const;


    private:
        QList<cwStation> Stations;
        QList<cwShot> Shots;

        //Data that's calculated by the loop closure algorithm
        QList<cwShotVector> ShotVectors;

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
        void setEdges(QSet<cwEdgeSurveyChunk*> edges) { Edges = edges; }
        QSet<cwEdgeSurveyChunk*> edges() const { return Edges; }

        bool operator ==(const cwLoop& other) const;
        bool operator !=(const cwLoop& other) const;
        bool operator <(const cwLoop& other) const;

    private:
        QSet<cwEdgeSurveyChunk*> Edges;
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
        QList<cwLoop> findLoops(QString station);
        void findLoopsHelper(QString station, QList<cwLoop> &resultLoops, QList<cwEdgeSurveyChunk*> path); //Recusive function, depth first search
        QList<cwLoop> findUniqueLoops(QList<cwLoop> initialLoops);
        QList<cwLoop> minimizeLoops(QList<cwLoop> loops);

        void printEdges(QList<cwLoopCloserTask::cwEdgeSurveyChunk*> edges) const;
        void printLoops(QList<cwLoop> loops) const;
        void printMatrix(QVector<char> matrix, int columns, int rows, QHash<int, cwEdgeSurveyChunk*> columnIndexToLoopEdges);
    };

    class EdgeMatrixRow : public QVector<bool> {
    };

    class cwShotProcessor {
    public:
        void process(QList<cwEdgeSurveyChunk*> edges);

    private:

        void processShot(cwEdgeSurveyChunk *chunk, int shotIndex);
        cwShotVector shotTransform(double distance, double azimith, double clino, cwClinoStates::State clinoState);
    };

    class cwLeastSquares {
    public:
        void process(QList<cwEdgeSurveyChunk*> edges);
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

inline QDebug operator<<(QDebug dbg, const arma::mat &c)
{
    dbg.nospace() << "mat[" << c.n_rows << ", " << c.n_cols << "] = \n[";
    for(unsigned int i = 0; i < c.n_rows; i++) {
        for(unsigned int j = 0; j < c.n_cols; j++) {
            dbg.nospace() << c(i, j);

            if(j < c.n_cols - 1) {
                dbg.nospace() << ", ";
            }
        }

        if(i < c.n_rows - 1) {
            dbg.nospace() << ";\n";
        } else {
            dbg.nospace() << ']';
        }
    }

    return dbg.space();
}

#endif // CWLOOPCLOSERTASK_H
