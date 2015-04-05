/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLINEPLOTMANAGER_H
#define CWLINEPLOTMANAGER_H

//Our includes
class cwCavingRegion;
class cwCave;
class cwTrip;
class cwSurveyChunk;
class cwShot;
class cwScrap;
class cwStationReference;
#include "cwLinePlotTask.h"
class cwGLLinePlot;

//Qt includes
#include <QObject>

class cwLinePlotManager : public QObject
{
    Q_OBJECT
public:
    explicit cwLinePlotManager(QObject *parent = 0);
    ~cwLinePlotManager();

    void setRegion(cwCavingRegion* region);
    Q_INVOKABLE void setGLLinePlot(cwGLLinePlot* linePlot);

signals:
    void stationPositionInCavesChanged(QList<cwCave*>);
    void stationPositionInTripsChanged(QList<cwTrip*>);
    void stationPositionInScrapsChanged(QList<cwScrap*>);

public slots:

private:
    cwCavingRegion* Region; //The main

    cwLinePlotTask* LinePlotTask;

    cwGLLinePlot* GLLinePlot;

    void connectCaves(cwCavingRegion* region);
    void connectCave(cwCave* cave);
    void connectTrips(cwCave* cave);
    void connectTrip(cwTrip* trip);
    void connectChunks(cwTrip* trip);
    void connectChunk(cwSurveyChunk* chunk);

    void validateResultsData(cwLinePlotTask::LinePlotResultData& results);

    void setCaveStationLookupAsStale(bool isStale);

private slots:
    void regionDestroyed(QObject* region);
    void runSurvex();

    void updateLinePlot();

    void connectAddedCaves(int beginIndex, int endIndex);
    void connectAddedTrips(int beginIndex, int endIndex);
    void connectAddedChunks(int beginIndex, int endIndex);

};

#endif // CWLINEPLOTMANAGER_H
