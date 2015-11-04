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
class cwSurveyChunkSignaler;

//Qt includes
#include <QObject>
#include <QThread>
#include <QPointer>

class cwLinePlotManager : public QObject
{
    Q_OBJECT
public:
    explicit cwLinePlotManager(QObject *parent = 0);
    ~cwLinePlotManager();

    void setRegion(cwCavingRegion* region);
    Q_INVOKABLE void setGLLinePlot(cwGLLinePlot* linePlot);

    void waitToFinish();

signals:
    void stationPositionInCavesChanged(QList<cwCave*>);
    void stationPositionInTripsChanged(QList<cwTrip*>);
    void stationPositionInScrapsChanged(QList<cwScrap*>);

public slots:

private:
    QPointer<cwCavingRegion> Region; //The main

    cwLinePlotTask* LinePlotTask;
    QThread* LinePlotThread;

    cwGLLinePlot* GLLinePlot;

    cwSurveyChunkSignaler* SurveySignaler;

    void connectCaves(cwCavingRegion* region);

    void validateResultsData(cwLinePlotTask::LinePlotResultData& results);

    void setCaveStationLookupAsStale(bool isStale);

private slots:
    void runSurvex();

    void updateLinePlot();
};

#endif // CWLINEPLOTMANAGER_H
