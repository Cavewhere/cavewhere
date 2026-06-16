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
class cwSaveLoad;
class cwKeywordItem;
class cwKeywordItemModel;
class cwLinePlotTripVisibility;
#include "cwLinePlotTask.h"
#include "cwLocalSettings.h"
#include "cwSurveyNetwork.h"
#include "cwSurveyNetworkArtifact.h"
#include "cwGlobals.h"
#include "cwFutureManagerToken.h"

//Qt includes
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QSet>
#include <QStringList>
#include <QUuid>

QT_FORWARD_DECLARE_CLASS(QFileSystemWatcher)

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

public:
    explicit cwLinePlotManager(QObject *parent = 0);
    ~cwLinePlotManager();

    bool hasSolveError() const { return m_lastSolveError.has_value(); }
    QString solveErrorMessage() const { return m_lastSolveError ? m_lastSolveError->message : QString(); }
    QString cavernLog() const { return m_lastCavernLog; }
    QString loopClosureStats() const { return m_lastLoopClosureStats; }

    void setRegion(cwCavingRegion* region);
    Q_INVOKABLE void setRenderLinePlot(cwRenderLinePlot* linePlot);
    void setFutureManagerToken(cwFutureManagerToken token);

    // Registers one keyword item per trip so the centerline participates in
    // keyword visibility filtering (trip / year / date / cave / caver). Items
    // are (re)created on each solve in updateLinePlot(). Mirrors the scrap and
    // note managers.
    void setKeywordItemModel(cwKeywordItemModel* keywordItemModel);

    // Per-owner attachment directories (abs paths on disk) consumed by
    // the line-plot driver's *include emission. Populated by the project
    // wiring (commit 9) from cwSaveLoad::externalCenterlineDir(owner); the
    // [Attach][...] tests call these setters directly. The maps are read
    // by every subsequent runSurvex() call and baked into the cwLinePlotTask
    // Input — change them while a solve is in flight and the in-flight
    // solve still sees the old map (the Input copy is already made), and
    // the next solve picks up the new one.
    void setCaveAttachmentDirs(QHash<QUuid, QString> dirs);
    void setTripAttachmentDirs(QHash<QUuid, QString> dirs);

    // Per-machine state for live-link attachments. The manager re-runs the
    // scanner over each entry whose ownerId appears here with a non-empty
    // sourcePath, and a source-side fileChanged event triggers a
    // reconcile-from-source through m_saveLoad before re-solving. Empty
    // by default, in which case every attachment is treated as import-mode
    // (no source-side watching).
    void setLocalSettings(cwLocalSettings settings);
    cwLocalSettings localSettings() const { return m_localSettings; }

    // Optional cwSaveLoad used to enqueue reconcile copy/remove jobs when
    // a live-link source changes on disk. Without it the source-side
    // watcher path still fires rerunSurvex but skips the reconcile; this
    // is the test mode used by the simpler [Attach][Watcher] cases.
    void setSaveLoad(cwSaveLoad* saveLoad);

    // The current set of paths the QFileSystemWatcher is intended to watch
    // (snapshot of the most recent recompute). Returned sorted, in a form
    // tests can tryVerify against. Includes both in-project attachment-dir
    // dependencies and live-link source-side dependencies.
    QStringList watchedFiles() const;

    // Owners (cave or trip id) whose local_settings.json sourcePath does
    // not exist on disk at the most recent recompute. Surfaces the
    // "Source not found" startup probe per §8.8 question 16 of the master
    // plan. Empty when no live-link attachment is configured.
    QList<QUuid> missingSourceOwners() const { return m_missingSourceOwners; }

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
    void cavernOutputChanged();

    // Emitted whenever the external-centerline watch set changes. Tests
    // use this with a SignalSpy to wait for attach / detach to settle
    // before asserting watchedFiles() contents.
    void watchedFilesChanged();

    // Emitted whenever the set of owners with a missing live-link source
    // changes. The set itself is read via missingSourceOwners().
    void missingSourceOwnersChanged();

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

    QHash<QUuid, QString> m_caveAttachmentDirs;
    QHash<QUuid, QString> m_tripAttachmentDirs;

    cwLocalSettings m_localSettings;
    QPointer<cwSaveLoad> m_saveLoad;

    QFileSystemWatcher* m_watcher = nullptr;

    // The full intended watch set (project-side + source-side deps), sorted
    // and deduplicated. Compared against on every recompute to figure out
    // which addPath/removePath calls to send. Paths that do not exist on
    // disk are kept here too — they re-arm the next time recompute runs and
    // they appear.
    QStringList m_watchedFiles;

    // Source path → owner id, for the source-side reconcile path. The
    // mere presence of a path in this map is the "this is a source-side
    // dep" signal that onWatchedFileChanged uses to route through
    // reconcile instead of running a plain solve. Whichever owner's
    // source-side scan first added a given path wins; duplicates between
    // owners are rare in v1 (one source per owner) and the reconcile
    // primitive is keyed by attachmentDir, not ownerId, so a
    // first-write-wins map is sufficient.
    QHash<QString, QUuid> m_sourceOwnerForPath;

    QList<QUuid> m_missingSourceOwners;

    // Per-trip centerline keyword visibility. One keyword item per trip, keyed
    // by the live cwTrip*. The visibility proxy (reachable via item->object())
    // owns the running id and pushes toggles straight to the render object, so
    // the manager keeps no flag array or running-id map of its own.
    QPointer<cwKeywordItemModel> m_keywordItemModel;
    QHash<cwTrip*, QPointer<cwKeywordItem>> m_tripKeywordEntries;

    bool AutomaticUpdate = true;

    void connectCaves(cwCavingRegion* region);
    void connectFixStations(cwCave* cave);

    void validateResultsData(cwLinePlotTask::LinePlotResultData& results);

    void setCaveStationLookupAsStale(bool isStale);
    void updateUnconnectedChunkErrors(cwCave *cave, const cwLinePlotTask::LinePlotCaveData& caveData);
    void clearUnconnectedChunkErrors();

    void updateLinePlot(cwLinePlotTask::LinePlotResultData results);

    // Re-attaches the per-trip keyword items to the freshly-solved geometry:
    // resolves each running id's UUID to a live cwTrip*, creates/removes keyword
    // items to match, re-binds each trip's visibility proxy to its new running
    // id, and re-seeds the render object's hidden trips. Identity (UUID) keyed,
    // so it is immune to list-order drift; trips deleted mid-solve simply fail
    // to resolve and are skipped.
    void reconcileTripKeywordItems(const QVector<QUuid>& tripUuids);
    void removeTripKeywordEntry(cwTrip* trip);

    // Tears down every keyword entry synchronously (for manager destruction /
    // model swap); removeTripKeywordEntry drops a single entry via deleteLater.
    void clearTripKeywordEntries();

    void publishResults(const cwLinePlotTask::LinePlotResultData& results);
    void publishCavernOutput(QString cavernLog,
                             QString loopClosureStats,
                             std::optional<cwLinePlotTask::SolveError> solveError);
    void publishPerCaveErrors(const cwLinePlotTask::LinePlotResultData& results);

    // Re-runs the scanner over every attachment (project-side, plus the
    // source side for live-link entries), rebuilds m_watchedFiles and
    // m_missingSourceOwners, and brings the QFileSystemWatcher in line by
    // diffing against the previous set. Cheap: scanner walks are
    // sub-millisecond per file and the test fixtures total ~3 files.
    void recomputeWatchSetAndProbeSources();

    // Re-arms the watcher for `path` (the file that just fired) so we
    // continue to receive change events for it; on macOS the watcher
    // implicitly drops a path after an atomic-write replace.
    void rearmWatcher(const QString& path);

private slots:
    void runSurvex();
    void onWatchedFileChanged(const QString& path);
};

//This needs to be here for moc to generate correctly and we can forward declare cwRenderLinePlot
#include "cwRenderLinePlot.h"

inline bool cwLinePlotManager::automaticUpdate() const {
    return AutomaticUpdate;
}

#endif // CWLINEPLOTMANAGER_H
