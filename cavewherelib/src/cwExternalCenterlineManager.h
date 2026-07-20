/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWEXTERNALCENTERLINEMANAGER_H
#define CWEXTERNALCENTERLINEMANAGER_H

//Our includes
class cwCavingRegion;
class cwSaveLoad;
class cwSurveyChunkSignaler;
class cwTrip;
#include "cwAttachedCenterlinesModel.h"
#include "cwExternalCenterlineAttach.h"
#include "cwExternalCenterlineReport.h"
#include "cwExternalSourceSettings.h"
#include "cwFutureManagerToken.h"
#include "cwGlobals.h"
#include "cwLinePlotTask.h"

//Qt includes
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QSet>
#include <QStringList>
#include <QUuid>

//Std includes
#include <atomic>
#include <memory>

QT_FORWARD_DECLARE_CLASS(QFileSystemWatcher)

//Async includes
#include <asyncfuture.h>

/**
 * Self-contained external-centerline subsystem: owns the attachment-dir
 * maps and the attached-centerlines model, watches the in-project copies
 * and live-link sources, runs the async scan/probe pipeline, and
 * surfaces missing/stale owners. cwLinePlotManager consumes it — it
 * reads the dirs and declination flags for each solve's buildInput and
 * runs a solve whenever solveNeeded() fires.
 */
class CAVEWHERE_LIB_EXPORT cwExternalCenterlineManager : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ExternalCenterlineManager)
    QML_UNCREATABLE("ExternalCenterlineManager is created by cwLinePlotManager")

    Q_PROPERTY(cwAttachedCenterlinesModel* attachedCenterlinesModel READ attachedCenterlinesModel CONSTANT FINAL)
    Q_PROPERTY(QList<QUuid> missingSourceOwners READ missingSourceOwners NOTIFY missingSourceOwnersChanged FINAL)
    Q_PROPERTY(QList<QUuid> staleSourceOwners READ staleSourceOwners NOTIFY staleSourceOwnersChanged FINAL)

public:
    explicit cwExternalCenterlineManager(QObject* parent = nullptr);
    ~cwExternalCenterlineManager();

    // Kicks off a recompute against the new region (superseding any scan
    // still in flight for the old one; a null region clears the watch set
    // and rows). When the region has caves, the consumer's initial solve
    // is requested through solveNeeded() after the scan applies —
    // buildInput reads the declination flags, so it must not race the
    // worker.
    void setRegion(cwCavingRegion* region);
    void setFutureManagerToken(cwFutureManagerToken token);

    // Per-owner attachment directories (abs paths on disk) consumed by
    // the line-plot driver's *include emission. When a saveLoad is wired
    // (production), the maps are derived wholesale from
    // cwSaveLoad::externalCenterlineDir(owner) at every recompute
    // snapshot; the setters are the seam for scan-only tests with no
    // saveLoad. The maps are read by every subsequent solve and baked
    // into the cwLinePlotTask Input — change them while a solve is in
    // flight and the in-flight solve still sees the old map (the Input
    // copy is already made), and the next solve picks up the new one.
    void setCaveAttachmentDirs(QHash<QUuid, QString> dirs);
    void setTripAttachmentDirs(QHash<QUuid, QString> dirs);

    // One atomic value snapshot of this subsystem's contribution to a
    // solve (dirs + declination flags), for the consumer's buildInput.
    cwLinePlotTask::ExternalCenterlineInputs solveInputs() const
    {
        return { m_caveAttachmentDirs, m_tripAttachmentDirs, m_fileOwnsDeclination };
    }

    // Per-machine state for live-link attachments, owned by cwRootData.
    // The manager re-runs the scanner over each entry whose ownerId
    // appears here with a non-empty sourcePath; a source-side
    // fileChanged event only flags the owner stale (staleSourceOwners)
    // — reconciling waits for an explicit updateFromSource call. The
    // manager observes the store's change signal and recomputes the
    // watch set on every mutation. Null by default, in which case every
    // attachment is treated as import-mode (no source-side watching).
    void setExternalSourceSettings(cwExternalSourceSettings* settings);
    cwExternalSourceSettings* externalSourceSettings() const { return m_externalSourceSettings; }

    // Optional cwSaveLoad used by updateFromSource to enqueue reconcile
    // copy/remove jobs. Without it updateFromSource is a no-op; staleness
    // detection (watcher + probe) works regardless.
    void setSaveLoad(cwSaveLoad* saveLoad);

    // The current set of paths the QFileSystemWatcher is intended to watch
    // (snapshot of the most recent recompute). Returned sorted, in a form
    // tests can tryVerify against. Includes both in-project attachment-dir
    // dependencies and live-link source-side dependencies.
    QStringList watchedFiles() const;

    // Owners (cave or trip id) whose cwExternalSourceSettings sourcePath does
    // not exist on disk at the most recent recompute. Surfaces the
    // "Source not found" startup probe per §8.8 question 16 of the master
    // plan. Empty when no live-link attachment is configured.
    QList<QUuid> missingSourceOwners() const { return m_missingSourceOwners; }

    // Owners whose remembered source has drifted from the in-project copy.
    // Staleness is probe-defined: an owner is stale exactly when the
    // reconcile plan (computePlan against the attachment dir) has pending
    // copies, so the Update affordance never advertises a copy that would
    // no-op. A source-side watcher event flags the owner immediately as a
    // fast path; every recompute re-derives the list wholesale from the
    // probe, clearing it once a fresh copy lands (normally via
    // updateFromSource). The recompute runs async — flags raised by
    // watcher events after its snapshot are unioned back in at apply so
    // the probe can't wipe an edit it never saw. Staleness is only ever
    // surfaced — the manager never reconciles on its own (see the
    // Phase 2 direction change).
    QList<QUuid> staleSourceOwners() const { return m_staleSourceOwners; }

    // User-triggered "Update" for a stale live-link owner: rescans the
    // remembered source, reconciles the closure into the owner's
    // attachment dir through cwSaveLoad, then recomputes the watch set
    // (clearing the stale flag) and requests a re-solve. No-op while any
    // operation for the same owner is already in flight, or when no
    // saveLoad is wired.
    Q_INVOKABLE void updateFromSource(const QUuid& ownerId);

    // Trip-level attach/detach through the cwExternalCenterlineAttach
    // orchestrator, using the wired saveLoad and settings store. Both
    // refuse a busy owner (completed error future) so two filesystem
    // operations for one owner can never interleave — the same
    // protection the UI gets from isOwnerBusy, enforced for every
    // caller. Detach drops the owner's settings entry and attachment-dir
    // map entry synchronously, so an updateFromSource invoke already
    // queued behind it hits the empty-guard and no-ops instead of
    // resurrecting files into the detached owner's dir.
    QFuture<Monad::Result<cwExternalCenterlineAttach::AttachReport>>
    attachCenterline(cwTrip* trip, const QString& sourcePath);
    QFuture<Monad::ResultBase> detachCenterline(cwTrip* trip);

    // Requests cancellation of ownerId's in-flight attachCenterline.
    // Honored only until the attach's internal scan lands - the flag
    // is consulted exactly once, at that point, so a later call is a
    // structural no-op and the attach runs to completion (the §5 q14
    // rule). The busy token is held until either outcome reports, so
    // cancel can never free the owner while reconcile is writing.
    // Never touches a detach or updateFromSource in flight; a
    // cancelled attach ends in one attachCompleted report with
    // canceled == true.
    //
    // This is the only safe cancel. Cancelling the QFuture returned by
    // attachCenterline directly does NOT get these guarantees: past
    // the scan-landing point the orchestrator ignores future-cancel
    // and keeps writing, while this manager's canceled observer fires
    // immediately - releasing the token and reporting canceled for an
    // attach that may still land. That seam predates cancelAttach and
    // remains for tests; production callers must not cancel the
    // returned future.
    Q_INVOKABLE void cancelAttach(const QUuid& ownerId);

    // True while an attach / detach / updateFromSource for ownerId has
    // not yet drained. QML binds this one query for the
    // Update/Detach/re-attach affordances; ownerBusyChanged notifies.
    Q_INVOKABLE bool isOwnerBusy(const QUuid& ownerId) const
    {
        return m_activeOperations.contains(ownerId);
    }

    // Per-owner missing-source query for QML. JS can't test QUuid
    // membership in missingSourceOwners (strict equality never matches
    // the wrapped values); bind this instead, re-evaluated on
    // missingSourceOwnersChanged.
    Q_INVOKABLE bool isSourceMissing(const QUuid& ownerId) const
    {
        return m_missingSourceOwners.contains(ownerId);
    }

    // True when the owner's external file carries its own declination
    // (master plan §8.8 q7) — the driver then injects nothing and the
    // trip panel shows the value read-only. Captured from the most recent
    // scan (source-side wins over project-side when both resolve); owners
    // that were never scanned successfully default to file-owned so an
    // unknown state never injects a declination. QML re-evaluates on
    // solveNeeded() — the scan apply emits it whenever the flags change.
    Q_INVOKABLE bool fileOwnsDeclination(const QUuid& ownerId) const
    {
        return m_fileOwnsDeclination.value(ownerId, true);
    }

    // The qualified-station prefix the line-plot driver gives an
    // attached trip's stations ("cave_<32hex>.trip_<32hex>."), for
    // cwScopeStationListModel::scopePrefix. Empty for a null trip or a
    // trip without a parent cave.
    Q_INVOKABLE QString scopePrefixForTrip(cwTrip* trip) const;

    // One row per attached external centerline, rebuilt on every watch-set
    // recompute. Always non-null; owned by this object.
    cwAttachedCenterlinesModel* attachedCenterlinesModel() const { return m_attachedCenterlinesModel; }

    // Stamps every row's lastSolved. Called once from the consumer's
    // solve-success path.
    void markSolved(const QDateTime& when);

    // Cancels any in-flight or queued scan; a canceled scan's apply never
    // runs, so no solveNeeded() can fire afterward. The consumer's
    // teardown calls this before draining its solve pipeline.
    void cancelScan();

    // Test helper: drains the scan pipeline (snapshot → worker → apply).
    // A solve the apply requested is the consumer's to drain.
    void waitToFinish();

signals:
    // Emitted whenever the external-centerline watch set changes. Tests
    // use this with a SignalSpy to wait for attach / detach to settle
    // before asserting watchedFiles() contents.
    void watchedFilesChanged();

    // Emitted whenever the set of owners with a missing live-link source
    // changes. The set itself is read via missingSourceOwners().
    void missingSourceOwnersChanged();

    // Emitted whenever the set of owners with a stale live-link source
    // changes. The set itself is read via staleSourceOwners().
    void staleSourceOwnersChanged();

    // Emitted after a scan apply has fully installed its result (member
    // swap included) when the apply carried a solve request or the
    // declination flags changed. The consumer runs its solve here — the
    // ordering guarantees buildInput never reads half-applied flags.
    void solveNeeded();

    // Emitted whenever isOwnerBusy(ownerId) flips for ownerId — an
    // attach/detach/updateFromSource started or drained.
    void ownerBusyChanged(const QUuid& ownerId);

    // Completion bridge for QML (commit-11 decision): while the manager
    // is alive, every attachCenterline / detachCenterline call ends in
    // exactly one of these — success, error, refused-while-busy, or
    // canceled — so dialogs and panels never observe the QFuture.
    // Always delivered asynchronously (never re-entrantly from inside
    // the call). An operation that ran reports after its busy token is
    // released; a refused-while-busy failure arrives while the blocking
    // operation still holds the token, so a report does NOT imply the
    // owner is idle — bind isOwnerBusy for that.
    void attachCompleted(const cwExternalCenterlineReport& report);
    void detachCompleted(const cwExternalCenterlineReport& report);

private:
    // Value-type inputs and outputs of the async recompute pipeline.
    // One OwnerScanInput per attached owner, snapshotted on the main
    // thread; no live pointers cross the thread boundary (same rule as
    // cwLinePlotTask::buildInput).
    struct OwnerScanInput {
        QUuid ownerId;
        QString caveName;
        QString ownerName;
        QString ownerKind;
        QString entryFile;
        QString attachmentDir;
        QString sourcePath;
    };

    // Everything the worker derives from an OwnerScanInput batch; the
    // main-thread apply installs it wholesale (watcher diff, member
    // swaps, signal emissions, model rows).
    struct ExternalScanResult {
        QStringList watchedFiles;
        // Subset of watchedFiles that existed when the worker looked —
        // the apply feeds these to addPaths (QFileSystemWatcher warns on
        // missing files) without re-statting on the main thread.
        QStringList existingWatchedFiles;
        QHash<QString, QUuid> sourceOwnerForPath;
        QList<QUuid> missingSourceOwners;
        QList<QUuid> staleSourceOwners;
        QHash<QUuid, bool> fileOwnsDeclination;
        QVector<cwAttachedCenterlinesModel::Row> rows;
    };

    QPointer<cwCavingRegion> m_region;

    AsyncFuture::Restarter<ExternalScanResult> m_scanRestarter;
    cwFutureManagerToken m_futureManagerToken;

    cwAttachedCenterlinesModel* m_attachedCenterlinesModel;

    // Rename wiring only: cave/trip nameChanged re-sorts the model rows
    // from cached counts. Survey-data connections that drive the solve
    // live on the consumer's own signaler.
    cwSurveyChunkSignaler* m_signaler;

    QHash<QUuid, QString> m_caveAttachmentDirs;
    QHash<QUuid, QString> m_tripAttachmentDirs;

    QPointer<cwExternalSourceSettings> m_externalSourceSettings;
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
    QList<QUuid> m_staleSourceOwners;

    // Watcher flags raised after the current scan's snapshot was taken.
    // The worker's probe result was computed without them, so the apply
    // unions this set into the probe-derived stale list instead of
    // overwriting wholesale (commit-8 merge policy). Cleared at each
    // snapshot — that instant is what "since" means.
    QSet<QUuid> m_staleRaisedSinceSnapshot;

    // Sticky "request a solve when the next scan applies" flag, set by
    // paths that used to do recompute-then-solve synchronously. The
    // apply consumes it after the member swap and emits solveNeeded().
    bool m_solveOnScanApply = false;

    // Rows from the most recent scan apply; the rename rebuild reuses
    // their dep/warning counts so it never touches the disk.
    QVector<cwAttachedCenterlinesModel::Row> m_lastScanRows;

    // Per-owner operation registry: one entry while an
    // attach/detach/updateFromSource drains, guarding against
    // interleaved per-owner filesystem operations (commit-5 review's
    // in-flight-token item, generalized at commit 9, made a registry
    // for cancelAttach at commit 12). The kind keeps cancelAttach off
    // detach/update operations; the cancelFlag is the attach
    // orchestrator's cancel seam.
    enum class OperationKind { Attach, Detach, Update };

    struct ActiveOperation {
        OperationKind kind = OperationKind::Update;
        std::shared_ptr<std::atomic_bool> cancelFlag; // Attach only
    };

    QHash<QUuid, ActiveOperation> m_activeOperations;

    // Per-owner file-owns-declination flag from the most recent recompute;
    // read via fileOwnsDeclination() and baked into each solve's Input by
    // the consumer. Rebuilt wholesale on every recompute.
    QHash<QUuid, bool> m_fileOwnsDeclination;

    // RAII completion guard for one owner operation, shared (via
    // shared_ptr) by the operation's completion and canceled
    // callbacks. The constructor takes the busy token (registry
    // insert + ownerBusyChanged); finish() releases it and emits the
    // completion report - idempotent, so exactly-one report per
    // operation is structural rather than per-call-site discipline.
    // The destructor is the backstop for a chain torn down without
    // either callback running: it releases the token but never emits
    // (in practice only reachable while the manager itself is dying,
    // where the bridge contract no longer applies).
    class OperationGuard {
    public:
        using ReportSignal =
            void (cwExternalCenterlineManager::*)(const cwExternalCenterlineReport&);

        OperationGuard(cwExternalCenterlineManager* manager,
                       const QUuid& ownerId,
                       OperationKind kind,
                       std::shared_ptr<std::atomic_bool> cancelFlag,
                       ReportSignal signal)
            : m_manager(manager)
            , m_ownerId(ownerId)
            , m_signal(signal)
        {
            manager->m_activeOperations.insert(
                ownerId, ActiveOperation { kind, std::move(cancelFlag) });
            emit manager->ownerBusyChanged(ownerId);
        }

        OperationGuard(const OperationGuard&) = delete;
        OperationGuard& operator=(const OperationGuard&) = delete;

        ~OperationGuard()
        {
            release();
        }

        // Releases the token and emits the completion report. Safe to
        // call at most once per outcome path; a second call (or a call
        // after the manager died) does nothing.
        void finish(const cwExternalCenterlineReport& report)
        {
            if (m_released) {
                return;
            }
            release();
            // Re-read the QPointer after release(): it emitted
            // ownerBusyChanged synchronously, and a slot there could
            // (in principle) tear the manager down - a pre-release
            // snapshot would then emit through a dangling pointer.
            if (!m_manager.isNull() && m_signal != nullptr) {
                emit (m_manager.data()->*m_signal)(report);
            }
        }

        // Token release without a report - for operations with no
        // report signal (updateFromSource) and the destructor backstop.
        void release()
        {
            if (m_released) {
                return;
            }
            m_released = true;
            if (m_manager.isNull()) {
                return;
            }
            m_manager->m_activeOperations.remove(m_ownerId);
            emit m_manager->ownerBusyChanged(m_ownerId);
        }

    private:
        QPointer<cwExternalCenterlineManager> m_manager;
        QUuid m_ownerId;
        ReportSignal m_signal;
        bool m_released = false;
    };

    // Queues a completion-report emission for the synchronous refusal
    // paths (null trip, refused-while-busy). Emitting directly there
    // would run handlers re-entrantly inside attachCenterline /
    // detachCenterline — a handler that retries on failure would
    // recurse on the same stack — and would fire before the caller
    // even has the returned future. Deferring keeps the bridge
    // contract uniform: the report always arrives after the call
    // returns.
    template<typename Signal>
    void emitReportDeferred(Signal signal, cwExternalCenterlineReport report)
    {
        QMetaObject::invokeMethod(this, [this, signal, report = std::move(report)]() {
            emit (this->*signal)(report);
        }, Qt::QueuedConnection);
    }

    // Rebuilds both attachment-dir maps wholesale from the region walk
    // via cwSaveLoad::externalCenterlineDir (pure path math, no disk
    // I/O). Runs at every recompute snapshot when a saveLoad is wired,
    // so the maps track attach/detach/load without per-path bookkeeping;
    // the setters remain the seam for scan-only tests with no saveLoad.
    void refreshAttachmentDirsFromSaveLoad();

    // Stage 1: one value-type input per attached owner with a non-empty
    // entry file. Pure region/member reads, no filesystem access.
    QVector<OwnerScanInput> collectOwnerSnapshots() const;

    // Stage 2, runs on the worker thread: project-side scan, source-side
    // scan, and computePlan freshness probe per owner. Static and pure —
    // touches no member state, only the filesystem.
    static ExternalScanResult scanOwners(const QVector<OwnerScanInput>& owners);

    // Identity fields of a model row from an owner snapshot (counts and
    // lastSolved are filled by the caller).
    static cwAttachedCenterlinesModel::Row rowFromOwner(const OwnerScanInput& owner);

    // Stage 3, back on the main thread: watcher diff, member swaps,
    // signal emissions, model rows, stale-flag union, and the
    // solveNeeded() emission when one was requested or the declination
    // flags changed.
    void applyScanResult(ExternalScanResult result);

    // Re-arms the watcher for `path` (the file that just fired) so we
    // continue to receive change events for it; on macOS the watcher
    // implicitly drops a path after an atomic-write replace.
    void rearmWatcher(const QString& path);

    // ownerId's live-link source path, or empty when no store is set -
    // no store means every attachment is import mode.
    QString sourcePathForOwner(const QUuid& ownerId) const;

private slots:
    // Async three-stage recompute: snapshot per-owner value inputs on
    // the main thread (no I/O), run every scan and freshness probe on a
    // cwConcurrent worker, then apply the result wholesale on the main
    // thread via applyScanResult. Coalesced through m_scanRestarter, so
    // a settings flurry or editor save burst runs one scan and only the
    // newest result lands. Paths that also need a solve set
    // m_solveOnScanApply first — the apply emits solveNeeded() so the
    // consumer's solve chains behind the member swap. A slot so the
    // signaler's externalCenterlineChanged / trip-list connections can
    // route here.
    void recomputeWatchSetAndProbeSources();

    void onWatchedFileChanged(const QString& path);

    // Cave/trip rename: rebuild the model rows from fresh names plus the
    // cached per-owner scan counts (m_lastScanRows) — zero disk I/O and
    // no waiting on a full recompute, so the list re-sorts immediately.
    void rebuildAttachedRowsFromNames();
};

#endif // CWEXTERNALCENTERLINEMANAGER_H
