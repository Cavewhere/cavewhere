#ifndef CWLINEPLOTMANAGER_H
#define CWLINEPLOTMANAGER_H

//Our includes
class cwCavingRegion;
class cwCave;
class cwTrip;
class cwSurveyChunk;
class cwShot;
class cwStation;
class cwLinePlotTask;

//Qt includes
#include <QObject>
#include <QThread>

class cwLinePlotManager : public QObject
{
    Q_OBJECT
public:
    explicit cwLinePlotManager(QObject *parent = 0);

    void setRegion(cwCavingRegion* region);

signals:

public slots:

private:
    cwCavingRegion* Region; //The main

    cwLinePlotTask* LinePlotTask;
    QThread* LinePlotThread;

    bool Rerun;

    void connectCaves(cwCavingRegion* region);
    void connectCave(cwCave* cave);
    void connectTrips(cwCave* cave);
    void connectTrip(cwTrip* trip);
    void connectChunks(cwTrip* trip);
    void connectChunk(cwSurveyChunk* chunk);
    void connectShots(cwSurveyChunk* chunk);
    void connectShot(cwShot* shot);
    void connectStations(cwSurveyChunk* chunk);
    void connectStation(cwStation* station);

private slots:
    void regionDestroyed(QObject* region);
    void runSurvex();
    void reRunSurvex();

    void connectAddedCaves(int beginIndex, int endIndex);
    void connectAddedTrips(int beginIndex, int endIndex);
    void connectAddedChunks(int beginIndex, int endIndex);
    void connectAddedStations(int beginIndex, int endIndex);
    void connectAddedShots(int beginIndex, int endIndex);

};

#endif // CWLINEPLOTMANAGER_H
