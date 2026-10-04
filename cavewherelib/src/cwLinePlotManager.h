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
class cwSurveyNode;
class cwTrip;
class cwSurveyChunk;
class cwShot;
class cwScrap;
class cwStationReference;
class cwRenderLinePlot;
class cwSurveyChunkSignaler;
class cwEquateModel;
class cwErrorListModel;
class cwExternalCenterlineManager;
class cwKeywordItem;
class cwKeywordItemModel;
class cwKeywordModel;
class cwLinePlotTripVisibility;

#include "cwFloatingSurveyModel.h"
#include "cwKeywordItemRegistry.h"
#include "cwLinePlotTask.h"
#include "cwSurveyNetwork.h"
#include "cwSurveyNetworkSource.h"
#include "cwGlobals.h"
#include "cwFutureManagerToken.h"
#include "cwUpdatable.h"

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

class CAVEWHERE_LIB_EXPORT cwLinePlotManager : public QObject, public cwUpdatableBase
{
    Q_OBJECT
    QML_NAMED_ELEMENT(LinePlotManager)

    Q_PROPERTY(cwFloatingSurveyModel* floatingSurveyModel READ floatingSurveyModel CONSTANT FINAL)
    Q_PROPERTY(cwSurveyNetworkSource* surveyNetworkSource READ surveyNetworkSource CONSTANT)
    Q_PROPERTY(bool hasSolveError READ hasSolveError NOTIFY cavernOutputChanged FINAL)
    Q_PROPERTY(QString solveErrorMessage READ solveErrorMessage NOTIFY cavernOutputChanged FINAL)
    Q_PROPERTY(QString cavernLog READ cavernLog NOTIFY cavernOutputChanged FINAL)
    Q_PROPERTY(QString loopClosureStats READ loopClosureStats NOTIFY cavernOutputChanged FINAL)
    Q_PROPERTY(QString driverSource READ driverSource NOTIFY cavernOutputChanged FINAL)
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
    // cwRootData exposes it to QML as externalCenterlineManager.
    cwExternalCenterlineManager* externalCenterlineManager() const { return m_externalCenterlineManager; }

    void setRegion(cwCavingRegion* region);
    Q_INVOKABLE void setRenderLinePlot(cwRenderLinePlot* linePlot);
    void setFutureManagerToken(cwFutureManagerToken token);

    // Registers keyword items per trip — Type="Line Plot" for the centerline
    // and Type="Splays" for the splay tail — so both participate in keyword
    // visibility filtering (type / trip / year / date / cave / caver). Items
    // are (re)created on each solve in updateLinePlot(). Mirrors the scrap and
    // note managers.
    void setKeywordItemModel(cwKeywordItemModel* keywordItemModel);

    // Region-wide survey network source, updated whenever the line-plot
    // pipeline completes. Shared across every consumer (sketches today; future
    // 2D views). Always non-null after construction; its future may be
    // unstarted until the first line plot finishes.
    cwSurveyNetworkSource* surveyNetworkSource() const { return m_surveyNetworkSource; }

    // Region-wide qualified survey network ("<caveLabel>.<tripLabel>.<station>"
    // keys) parsed from cavern's .3d output on the most recent solve. Empty
    // until the first solve completes. A plain snapshot accessor with no change
    // signal of its own: surveyNetworkSource() is the channel consumers watch,
    // and a trip's solved stations are pulsed by cwTrip::solvedStationsChanged.
    cwSurveyNetwork regionNetwork() const { return m_lastPublishedNetwork; }

    // Every survey the most recent run found floating — not joined to the rest
    // of its cave — from both of the passes that can find one: the pre-solve
    // native-chunk check and the post-solve topology check. One list so a
    // consumer never has to know which pass answered, which is what lets a
    // single banner speak for both. Empty until the first run completes.
    QList<cwFindFloatingSurveys::Result> floatingSurveys() const { return m_floatingSurveys; }

    //! The same answer as rows a view can bind to, with each record's cave and
    //! trip resolved against the live region. Always non-null.
    cwFloatingSurveyModel* floatingSurveyModel() const { return m_floatingSurveyModel; }

    void waitToFinish();

signals:
    //! Carries every node whose solve result moved, at any depth.
    void stationPositionInCavesChanged(QList<cwSurveyNode*>);
    void stationPositionInTripsChanged(QList<cwTrip*>);
    void stationPositionInScrapsChanged(QList<cwScrap*>);
    void updateStateChanged();
    void cavernOutputChanged();
    void floatingSurveysChanged();

public slots:
    //Marks the line plot dirty without running anything. Public so a "Solve"
    //button can pair it with cwUpdateCoordinator::updateNow(this), where the
    //mark-then-drive split is explained.
    void markNeedsUpdate();

private:
    cwUpdatable::State doUpdateState() const override;
    QFuture<void> doRun() override;

    QPointer<cwCavingRegion> Region; //The main
    QList<QPointer<cwErrorListModel>> UnconnectedChunks; //Current unconnected chunks

    AsyncFuture::Restarter<cwLinePlotTask::LinePlotResultData> m_restarter;
    cwFutureManagerToken m_futureManagerToken;
    cwRenderLinePlot* m_linePlot;

    cwSurveyChunkSignaler* SurveySignaler;

    cwSurveyNetworkSource* m_surveyNetworkSource;
    cwSurveyNetwork m_lastPublishedNetwork;

    QList<cwFindFloatingSurveys::Result> m_floatingSurveys;
    cwFloatingSurveyModel* m_floatingSurveyModel;

    // The last post-solve answer for which stations cavern dropped, kept like
    // the floating surveys' external half so a run that stops before the
    // post-solve pass leaves it standing.
    QList<cwLinePlotTask::LinePlotResultData::HangingStations> m_hangingStations;

    std::optional<cwLinePlotTask::SolveError> m_lastSolveError;
    QString m_lastCavernLog;
    QString m_lastLoopClosureStats;
    QString m_lastDriverSource;
    double m_lastSolveDuration = -1.0;
    int m_lastStationCount = 0;
    int m_lastWarningCount = 0;

    cwExternalCenterlineManager* m_externalCenterlineManager = nullptr;

    // Per-trip line-plot keyword visibility, keyed by the live cwTrip* plus
    // the kind. Each trip carries a centerline item (Type="Line Plot") and,
    // when the trip has splay geometry, a splays item (Type="Splays"); the two
    // visibility proxies (reachable via item->object()) address disjoint
    // vertex ranges and push keyword toggles straight to the render object, so
    // the manager keeps no flag array or running-id map of its own. The
    // registry owns the add/remove/delete mechanics.
    enum class TripKeywordKind : quint8 { Centerline, Splays };
    cwKeywordItemRegistry<QPair<cwTrip*, TripKeywordKind>> m_keywordRegistry;

    // Trips with registered items, so a trip that is destroyed or leaves the
    // solved geometry releases its items and destroyed() connection exactly
    // once.
    QSet<cwTrip*> m_trackedTrips;

    bool m_needsUpdate = false;

    // Ends the run (Working -> Clean, or -> Dirty if re-edited mid-solve) and
    // emits updateStateChanged so the coordinator re-evaluates its staleness
    // aggregate. Finishing the run's future is what releases anyone waiting on
    // the solve, so every completion path has to reach here.
    void finishSolving();

    void rerunIfAnyNodeIsStale(cwCavingRegion* region);
    void connectFixStations(cwSurveyNode* node);

    //! Hooks the fix stations of every node in the tree. Safe to repeat: each
    //! connection is unique, so the subtreeChanged handler can call it after
    //! every insert without stacking re-solves.
    void connectNodeInputs();

    //! Re-solve when the region's equate list changes. An equate is a survey
    //! input like a shot or a fix — it is what joins two scopes cavern would
    //! otherwise leave in separate frames — so declaring one has to reach the
    //! plot the same way entering a shot does, or a tie the user just made
    //! stays invisible.
    void connectEquates(cwEquateModel* equates);

    void setCaveStationLookupAsStale(bool isStale);
    void updateUnconnectedChunkErrors(cwSurveyNode* node, const cwLinePlotTask::LinePlotCaveData& nodeData);
    void clearUnconnectedChunkErrors();

    void updateLinePlot(cwLinePlotTask::LinePlotResultData results);

    // Re-attaches the per-trip keyword items to the freshly-solved geometry:
    // resolves each running id's UUID to a live cwTrip*, creates/removes keyword
    // items to match, re-binds each trip's visibility proxy to its new vertex
    // span, and re-seeds the render object's hidden trips. Identity (UUID)
    // keyed, so it is immune to list-order drift; trips deleted mid-solve simply
    // fail to resolve and are skipped. tripVertexRanges and
    // tripSplayVertexRanges are parallel to tripUuids (all running-id indexed).
    void reconcileTripKeywordItems(const QVector<QUuid>& tripUuids,
                                   const QVector<cwLinePlotGeometry::VertexRange>& tripVertexRanges,
                                   const QVector<cwLinePlotGeometry::VertexRange>& tripSplayVertexRanges);
    void removeTripKeywordItems(cwTrip* trip);

    // Registry factory: builds one keyword item whose keywords extend
    // `keywordModel` (a trip-owned identity model) and whose object is a fresh
    // visibility proxy for `trip`. The registry adds it to the model.
    cwKeywordItem* makeTripKeywordItem(cwTrip* trip, cwKeywordModel* keywordModel);

    void publishResults(const cwLinePlotTask::LinePlotResultData& results);
    void publishCavernOutput(QString cavernLog,
                             QString loopClosureStats,
                             QString driverSource,
                             std::optional<cwLinePlotTask::SolveError> solveError,
                             double solveDuration,
                             int stationCount,
                             int warningCount);
    void publishPerCaveErrors(const cwLinePlotTask::LinePlotResultData& results);

    //! Decided with the same fact as *cs out — whether the project has a
    //! frame — so it is set as each solve starts. Each node gets one
    //! AttachedFixWithoutCS warning naming the attached files (its own or its
    //! trips') that fix a station with no input coordinate system, with those
    //! stations, and every other node has it cleared. It stays while the bare
    //! fix does: the driver drops a node fix on that station, and a node fix
    //! on another one leaves the file anchored twice.
    //!
    //! Beside it, an AttachedFileUnfixed warning names each attached file the
    //! driver fixes at the origin because the file fixes no station of its
    //! own and no node fix places it.
    void publishAttachedFixWarnings(const cwLinePlotTask::ExternalCenterlineInputs& inputs);
    void publishFloatingSurveys(QList<cwFindFloatingSurveys::Result> floatingSurveys,
                                bool externalScopesChecked);

    //! Writes one UnconnectedStations warning on each trip, and each node with
    //! an attached file, holding stations that are not tied to the cave, and
    //! clears it everywhere else. Reads the floating surveys and the hanging
    //! stations, the same solve result the floating-survey banner shows.
    void publishUnconnectedStationWarnings();

private slots:
    void runSurvex();
};

//This needs to be here for moc to generate correctly and we can forward declare cwRenderLinePlot
#include "cwRenderLinePlot.h"

inline cwUpdatable::State cwLinePlotManager::doUpdateState() const {
    // Dirty takes priority over Working: a survey edit that arrives mid-solve
    // isn't covered by the solve in flight, so it reports Dirty and the driver
    // runs it again. See cwUpdatable::State.
    if(m_needsUpdate) { return cwUpdatable::State::Dirty; }
    if(isRunning())   { return cwUpdatable::State::Working; }
    return cwUpdatable::State::Clean;
}

#endif // CWLINEPLOTMANAGER_H
