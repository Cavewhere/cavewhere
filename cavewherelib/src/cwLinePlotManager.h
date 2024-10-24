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
class cwRenderLinePlot;
class cwSurveyChunkSignaler;
class cwErrorListModel;
#include "cwLinePlotTask.h"
#include "cwGlobals.h"

//Qt includes
#include <QObject>
#include <QPointer>
#include <QQMlEngine>

class CAVEWHERE_LIB_EXPORT cwLinePlotManager : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(LinePlotManager)

    Q_PROPERTY(bool automaticUpdate READ automaticUpdate WRITE setAutomaticUpdate NOTIFY automaticUpdateChanged)

public:
    explicit cwLinePlotManager(QObject *parent = 0);
    ~cwLinePlotManager();

    void setRegion(cwCavingRegion* region);
    Q_INVOKABLE void setRenderLinePlot(cwRenderLinePlot* linePlot);

    bool automaticUpdate() const;
    void setAutomaticUpdate(bool automaticUpdate);

    void waitToFinish();

signals:
    void stationPositionInCavesChanged(QList<cwCave*>);
    void stationPositionInTripsChanged(QList<cwTrip*>);
    void stationPositionInScrapsChanged(QList<cwScrap*>);
    void automaticUpdateChanged();

public slots:

private:
    QPointer<cwCavingRegion> Region; //The main
    QList<QPointer<cwErrorListModel>> UnconnectedChunks; //Current unconnected chunks

    cwLinePlotTask* LinePlotTask;

    cwRenderLinePlot* m_linePlot;

    cwSurveyChunkSignaler* SurveySignaler;

    bool AutomaticUpdate = true;

    void connectCaves(cwCavingRegion* region);

    void validateResultsData(cwLinePlotTask::LinePlotResultData& results);

    void setCaveStationLookupAsStale(bool isStale);
    void updateUnconnectedChunkErrors(cwCave *cave, const cwLinePlotTask::LinePlotCaveData& caveData);
    void clearUnconnectedChunkErrors();

private slots:
    void rerunSurvex();
    void runSurvex();

    void updateLinePlot();
};

//This needs to be here for moc to generate correctly and we can forward declare cwRenderLinePlot
#include "cwRenderLinePlot.h"

inline bool cwLinePlotManager::automaticUpdate() const {
    return AutomaticUpdate;
}

#endif // CWLINEPLOTMANAGER_H
