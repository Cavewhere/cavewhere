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
class cwExternalCenterlineManager;
class cwKeywordItem;
class cwKeywordItemModel;
class cwLinePlotTripVisibility;
#include "cwLinePlotTask.h"
#include "cwSurveyNetwork.h"
#include "cwSurveyNetworkArtifact.h"
#include "cwGlobals.h"
#include "cwFutureManagerToken.h"

//Qt includes
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QUuid>

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
    Q_PROPERTY(bool hasSolveError READ hasSolveError NOTIFY cavernOutputChanged FINAL)
    Q_PROPERTY(QString solveErrorMessage READ solveErrorMessage NOTIFY cavernOutputChanged FINAL)
    Q_PROPERTY(QString cavernLog READ cavernLog NOTIFY cavernOutputChanged FINAL)
    Q_PROPERTY(QString loopClosureStats READ loopClosureStats NOTIFY cavernOutputChanged FINAL)
    Q_PROPERTY(QString driverSource READ driverSource NOTIFY cavernOutputChanged FINAL)
    Q_PROPERTY(cwSurveyNetwork regionNetwork READ regionNetwork NOTIFY regionNetworkChanged FINAL)
    Q_PROPERTY(double lastSolveDuration READ lastSolveDuration NOTIFY cavernOutputChanged FINAL)
    Q_PROPERTY(int lastSolveStationCount READ lastSolveStationCount NOTIFY cavernOutputChanged FINAL)
    Q_PROPERTY(int lastSolveWarningCount READ lastSolveWarningCount NOTIFY cavernOutputChanged FINAL)

public:
    explicit cwLinePlotManager(QObject *parent = 0);
    ~cwLinePlotManager();

    bool hasSolveError() const { return m_lastSolveError.has_value(); }
    QString solveErrorMessage() const { return m_lastSolveError ? m_lastSolveError->message : QString(); }
    QString cavernLog() const { return m_lastCavernLog; }
    QString loopClosureStats() const { return m_lastLoopClosureStats; }

    // The driver .svx text the worker generated for the most recent solve,
    // populated alongside cavernLog (present even when cavern itself
    // failed; empty when the export step failed or nothing was solvable).
    QString driverSource() const { return m_lastDriverSource; }

    // Live stats for the most recent solve, surfaced by CavernOutputPage's
    // status label. Duration is wall-clock seconds for the whole
    // pipeline; negative means no solve has run yet ("not yet run").
    double lastSolveDuration() const { return m_lastSolveDuration; }
    int lastSolveStationCount() const { return m_lastStationCount; }
    int lastSolveWarningCount() const { return m_lastWarningCount; }

    // The external-centerline subsystem (watcher, async scan pipeline,
    // attachment dirs, attached-centerlines model, stale/missing owners).
    // Owned by the manager: its solveNeeded() chains into runSurvex and
    // each solve's buildInput reads its dirs + declination flags.
    // cwRootData will expose this object directly to QML (commit 9).
    cwExternalCenterlineManager* externalCenterlineManager() const { return m_externalCenterlineManager; }

    void setRegion(cwCavingRegion* region);
    Q_INVOKABLE void setRenderLinePlot(cwRenderLinePlot* linePlot);
    void setFutureManagerToken(cwFutureManagerToken token);

    // Registers one keyword item per trip so the centerline participates in
    // keyword visibility filtering (trip / year / date / cave / caver). Items
    // are (re)created on each solve in updateLinePlot(). Mirrors the scrap and
    // note managers.
    void setKeywordItemModel(cwKeywordItemModel* keywordItemModel);

    bool automaticUpdate() const;
    void setAutomaticUpdate(bool automaticUpdate);

    // Region-wide survey network artifact, updated whenever the line-plot
    // pipeline completes. Shared across every consumer (sketches today; future
    // 2D views). Always non-null after construction; its future may be
    // unstarted until the first line plot finishes.
    cwSurveyNetworkArtifact* surveyNetworkArtifact() const { return m_surveyNetworkArtifact; }

    // Region-wide qualified survey network (cave_<uuid>.trip_<uuid>.<station>
    // keys) parsed from cavern's .3d output on the most recent solve. Empty
    // until the first solve completes. Bind cwScopeStationListModel::network
    // to this.
    cwSurveyNetwork regionNetwork() const { return m_lastPublishedNetwork; }

    void waitToFinish();

signals:
    void stationPositionInCavesChanged(QList<cwCave*>);
    void stationPositionInTripsChanged(QList<cwTrip*>);
    void stationPositionInScrapsChanged(QList<cwScrap*>);
    void automaticUpdateChanged();
    void cavernOutputChanged();

    void regionNetworkChanged();

public slots:
    void rerunSurvex();

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
    QString m_lastCavernLog;
    QString m_lastLoopClosureStats;
    QString m_lastDriverSource;
    double m_lastSolveDuration = -1.0;
    int m_lastStationCount = 0;
    int m_lastWarningCount = 0;

    cwExternalCenterlineManager* m_externalCenterlineManager = nullptr;

    // Per-trip centerline keyword visibility. One keyword item per trip, keyed
    // by the live cwTrip*. The visibility proxy (reachable via item->object())
    // owns the running id and pushes toggles straight to the render object, so
    // the manager keeps no flag array or running-id map of its own.
    QPointer<cwKeywordItemModel> m_keywordItemModel;
    QHash<cwTrip*, QPointer<cwKeywordItem>> m_tripKeywordEntries;

    bool AutomaticUpdate = true;

    void connectCaves(cwCavingRegion* region);
    void connectFixStations(cwCave* cave);

    void setCaveStationLookupAsStale(bool isStale);
    void updateUnconnectedChunkErrors(cwCave *cave, const cwLinePlotTask::LinePlotCaveData& caveData);
    void clearUnconnectedChunkErrors();

    void updateLinePlot(cwLinePlotTask::LinePlotResultData results);

    // Re-attaches the per-trip keyword items to the freshly-solved geometry:
    // resolves each running id's UUID to a live cwTrip*, creates/removes keyword
    // items to match, re-binds each trip's visibility proxy to its new vertex
    // span, and re-seeds the render object's hidden trips. Identity (UUID)
    // keyed, so it is immune to list-order drift; trips deleted mid-solve simply
    // fail to resolve and are skipped. tripVertexRanges is parallel to
    // tripUuids (both running-id indexed).
    void reconcileTripKeywordItems(const QVector<QUuid>& tripUuids,
                                   const QVector<cwLinePlotGeometry::VertexRange>& tripVertexRanges);
    void removeTripKeywordEntry(cwTrip* trip);

    // Tears down every keyword entry synchronously (for manager destruction /
    // model swap); removeTripKeywordEntry drops a single entry via deleteLater.
    void clearTripKeywordEntries();

    void publishResults(const cwLinePlotTask::LinePlotResultData& results);
    void publishCavernOutput(QString cavernLog,
                             QString loopClosureStats,
                             QString driverSource,
                             std::optional<cwLinePlotTask::SolveError> solveError,
                             double solveDuration,
                             int stationCount,
                             int warningCount);
    void publishPerCaveErrors(const cwLinePlotTask::LinePlotResultData& results);

private slots:
    void runSurvex();
};

//This needs to be here for moc to generate correctly and we can forward declare cwRenderLinePlot
#include "cwRenderLinePlot.h"

inline bool cwLinePlotManager::automaticUpdate() const {
    return AutomaticUpdate;
}

#endif // CWLINEPLOTMANAGER_H
