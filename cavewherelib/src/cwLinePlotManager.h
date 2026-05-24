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
#include "cwSurveyNetwork.h"
#include "cwSurveyNetworkArtifact.h"
#include "cwGlobals.h"
#include "cwFutureManagerToken.h"

//Qt includes
#include <QObject>
#include <QPointer>
#include <QQmlEngine>

//Async includes
#include <asyncfuture.h>

//Std includes
#include <optional>

class CAVEWHERE_LIB_EXPORT cwLinePlotManager : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(LinePlotManager)

    Q_PROPERTY(bool automaticUpdate READ automaticUpdate WRITE setAutomaticUpdate NOTIFY automaticUpdateChanged)
    Q_PROPERTY(cwSurveyNetworkArtifact* surveyNetworkArtifact READ surveyNetworkArtifact CONSTANT)
    Q_PROPERTY(bool hasSolveError READ hasSolveError NOTIFY solveErrorChanged FINAL)
    Q_PROPERTY(QString solveErrorMessage READ solveErrorMessage NOTIFY solveErrorChanged FINAL)
    Q_PROPERTY(QString cavernLog READ cavernLog NOTIFY solveErrorChanged FINAL)
    Q_PROPERTY(QString loopClosureStats READ loopClosureStats NOTIFY solveErrorChanged FINAL)

public:
    explicit cwLinePlotManager(QObject *parent = 0);
    ~cwLinePlotManager();

    bool hasSolveError() const { return m_lastSolveError.has_value(); }
    QString solveErrorMessage() const { return m_lastSolveError ? m_lastSolveError->message : QString(); }
    QString cavernLog() const { return m_lastSolveError ? m_lastSolveError->cavernLog : QString(); }
    QString loopClosureStats() const { return m_lastSolveError ? m_lastSolveError->loopClosureStats : QString(); }

    void setRegion(cwCavingRegion* region);
    Q_INVOKABLE void setRenderLinePlot(cwRenderLinePlot* linePlot);
    void setFutureManagerToken(cwFutureManagerToken token);

    bool automaticUpdate() const;
    void setAutomaticUpdate(bool automaticUpdate);

    // Region-wide survey network artifact, updated whenever the line-plot
    // pipeline completes. Shared across every consumer (sketches today; future
    // 2D views). Always non-null after construction; its future may be
    // unstarted until the first line plot finishes.
    cwSurveyNetworkArtifact* surveyNetworkArtifact() const { return m_surveyNetworkArtifact; }

    void waitToFinish();

signals:
    void stationPositionInCavesChanged(QList<cwCave*>);
    void stationPositionInTripsChanged(QList<cwTrip*>);
    void stationPositionInScrapsChanged(QList<cwScrap*>);
    void automaticUpdateChanged();
    void solveErrorChanged();

public slots:

private:
    QPointer<cwCavingRegion> Region; //The main
    QList<QPointer<cwErrorListModel>> UnconnectedChunks; //Current unconnected chunks

    AsyncFuture::Restarter<cwLinePlotTask::LinePlotResultData> m_restarter;
    cwFutureManagerToken m_futureManagerToken;
    cwRenderLinePlot* m_linePlot;

    cwSurveyChunkSignaler* SurveySignaler;

    cwSurveyNetworkArtifact* m_surveyNetworkArtifact;
    cwSurveyNetwork m_lastPublishedNetwork;

    std::optional<cwLinePlotTask::SolveError> m_lastSolveError;

    bool AutomaticUpdate = true;

    void connectCaves(cwCavingRegion* region);
    void connectFixStations(cwCave* cave);

    void validateResultsData(cwLinePlotTask::LinePlotResultData& results);

    void setCaveStationLookupAsStale(bool isStale);
    void updateUnconnectedChunkErrors(cwCave *cave, const cwLinePlotTask::LinePlotCaveData& caveData);
    void clearUnconnectedChunkErrors();

    void updateLinePlot(cwLinePlotTask::LinePlotResultData results);

private slots:
    void rerunSurvex();
    void runSurvex();
};

//This needs to be here for moc to generate correctly and we can forward declare cwRenderLinePlot
#include "cwRenderLinePlot.h"

inline bool cwLinePlotManager::automaticUpdate() const {
    return AutomaticUpdate;
}

#endif // CWLINEPLOTMANAGER_H
