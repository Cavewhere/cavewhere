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
#include "cwExternalSourceStatusModel.h"
#include "cwFutureManagerToken.h"
#include "cwGlobals.h"
#include "cwLinePlotTask.h"

//Qt includes
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QPromise>
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
 * maps and the attached-centerlines model, watches the in-project copies,
 * and runs the async scan pipeline. cwLinePlotManager consumes it — it
 * reads the dirs and declination flags for each solve's buildInput and
 * runs a solve whenever solveNeeded() fires.
 *
 * The in-project copy is the only file the subsystem ever reads for the
 * solve. The file an attachment was picked from is a breadcrumb held
 * elsewhere (cwExternalSourceSettings) and is never scanned or copied from
 * on the manager's own initiative — see
 * plans/EXTERNAL_FILE_LIVE_LINK_RETIREMENT.html. A second watcher does
 * observe those sources, purely to tell the user their source moved on:
 * a source-side event only recomputes sourceStatusModel() rows, so it
 * copies nothing, solves nothing, and leaves the project unmodified
 * (plans/EXTERNAL_SOURCE_CHANGE_NOTIFY.html §5 N3).
 */
class CAVEWHERE_LIB_EXPORT cwExternalCenterlineManager : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ExternalCenterlineManager)
    QML_UNCREATABLE("ExternalCenterlineManager is created by cwLinePlotManager")

    Q_PROPERTY(cwAttachedCenterlinesModel* attachedCenterlinesModel READ attachedCenterlinesModel CONSTANT FINAL)
    Q_PROPERTY(cwExternalSourceStatusModel* sourceStatusModel READ sourceStatusModel CONSTANT FINAL)

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
    // The excluded set unions the two states that make an *include
    // unusable: an escaping dependency and a missing in-project copy.
    cwLinePlotTask::ExternalCenterlineInputs solveInputs() const;

    // Per-machine record of the file each attachment was last picked from,
    // owned by cwRootData. The manager holds it only to hand to attach and
    // replace, which write the breadcrumb as they land; nothing in the scan
    // pipeline reads it. Null by default, which leaves attach with nowhere
    // to record the pick.
    void setExternalSourceSettings(cwExternalSourceSettings* settings);
    cwExternalSourceSettings* externalSourceSettings() const { return m_externalSourceSettings; }

    // Optional cwSaveLoad: the source of the attachment dirs every recompute
    // derives, and the job queue attach and replace reconcile through.
    void setSaveLoad(cwSaveLoad* saveLoad);

    // The current set of paths the QFileSystemWatcher is intended to watch
    // (snapshot of the most recent recompute). Returned sorted, in a form
    // tests can tryVerify against. In-project attachment-dir dependencies
    // only: a remembered source lives outside the project and is left
    // alone.
    QStringList watchedFiles() const;

    // The remembered source files this session watches for change
    // notification — the fingerprinted dependency set of every attachment,
    // canonicalized, and only the files that are on disk here. Their parent
    // directories are watched too, which is how a deleted source is noticed
    // coming back; those are left out of this list. Sorted, so a caller can
    // compare it against an expected set.
    QStringList watchedSourceFiles() const;

    // Trip-level attach/detach through the cwExternalCenterlineAttach
    // orchestrator, using the wired saveLoad and settings store. Both
    // refuse a busy owner (completed error future) so two filesystem
    // operations for one owner can never interleave — the same
    // protection the UI gets from isOwnerBusy, enforced for every
    // caller. Detach drops the owner's settings entry and attachment-dir
    // map entry synchronously, so anything queued behind it finds the
    // owner already unattached rather than resurrecting files into the
    // detached owner's dir.
    QFuture<Monad::Result<cwExternalCenterlineAttach::AttachReport>>
    attachCenterline(cwTrip* trip, const QString& sourcePath);
    QFuture<Monad::ResultBase> detachCenterline(cwTrip* trip);

    // Points an already-attached trip at a freshly picked file: the same
    // scan → reconcile → GC → re-solve pass as attach, run against the
    // owner's existing attachment dir, so the closure is swapped in one
    // operation (plans/EXTERNAL_FILE_LIVE_LINK_RETIREMENT.html §5.1).
    // Deliberately not detach-then-attach: detach clears the settings
    // entry and the attachment-dir map synchronously, leaving a window
    // where an interleaved scan sees the owner as unattached. Reports
    // through attachCompleted, since to the bridge a replace is an
    // attach over an occupied owner.
    QFuture<Monad::Result<cwExternalCenterlineAttach::AttachReport>>
    replaceCenterline(cwTrip* trip, const QString& sourcePath);

    // Re-copies the file this owner was last attached (or replaced) from
    // into the project, through replaceCenterline — one scan → reconcile →
    // GC → re-solve pass that brings the in-project copy back in line with
    // its origin. The source is the breadcrumb this manager looks up
    // itself, so QML asks for the verb rather than naming a path. Refuses a
    // null trip, a busy owner, and an owner with no reloadable source, each
    // through the attach bridge like every other refusal.
    Q_INVOKABLE QFuture<Monad::Result<cwExternalCenterlineAttach::AttachReport>>
    reloadFromSource(cwTrip* trip);

    // True when reloadFromSource has a file to copy: this machine
    // remembers where the copy came from, that file is on disk here, and it
    // lies outside the owner's attachment dir (a source inside the project
    // already is the copy). A machine that received the project through Git
    // reads false, which is what keeps the verb off the menu there. Read at
    // the UI's own refresh points — the answer follows the disk, so it has
    // no change signal.
    Q_INVOKABLE bool canReloadFromSource(cwTrip* trip) const;

    // Requests cancellation of ownerId's in-flight attachCenterline.
    // Honored only until the attach's internal scan lands - the flag
    // is consulted exactly once, at that point, so a later call is a
    // structural no-op and the attach runs to completion (the §5 q14
    // rule). The busy token is held until either outcome reports, so
    // cancel can never free the owner while reconcile is writing.
    // Never touches a detach in flight; a
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

    // True while an attach / replace / detach for ownerId has not yet
    // drained. QML binds this one query for the Reload/Replace
    // affordances; ownerBusyChanged notifies.
    Q_INVOKABLE bool isOwnerBusy(const QUuid& ownerId) const
    {
        return m_activeOperations.contains(ownerId);
    }

    // True when the owner's external file carries its own declination
    // (master plan §8.8 q7) — the driver then injects nothing and the
    // trip panel shows the value read-only. Captured from the most recent
    // scan of the in-project copy; owners that were never scanned
    // successfully default to file-owned so an unknown state never injects
    // a declination. QML re-evaluates on solveNeeded() — the scan apply
    // emits it whenever the flags change.
    Q_INVOKABLE bool fileOwnsDeclination(const QUuid& ownerId) const
    {
        return m_fileOwnsDeclination.value(ownerId, true);
    }

    // Project-relative path of ownerId's in-project centerline copy when
    // that file is gone from disk; empty while the copy is there. The copy
    // is the only file the project reads, so its absence is what the trip
    // panel banners, offering Replace… as the way back, and what keeps the
    // owner's *include off the driver so the rest of the region still
    // plots. Derived at every recompute, which is what clears it when the
    // file returns.
    //
    // A missing *member* of the closure is left to cavern: the harvest
    // reads the entry file and names the include it could not open, which
    // the file-error banner shows. Only the entry file's absence stops
    // that from happening at all.
    Q_INVOKABLE QString missingCopyPath(const QUuid& ownerId) const
    {
        return m_missingCopies.value(ownerId);
    }

    // Absolute path of the in-project directory holding ownerId's copies
    // of its external files. Every entryFile is relative to this, so the
    // pair names a file on disk. Empty for an owner with no attachment.
    Q_INVOKABLE QString attachmentDir(const QUuid& ownerId) const
    {
        return m_tripAttachmentDirs.value(ownerId,
                                          m_caveAttachmentDirs.value(ownerId));
    }

    // Re-reads every attachment from disk right now — watch set,
    // declination flags, harvest, and the missing-copy report — and
    // requests the solve behind the apply. The panel's Reload runs this so
    // a copy restored (or replaced) behind the app's back is noticed; a
    // watched edit gets here on its own through the watcher.
    Q_INVOKABLE void rescanAttachments();

    // One row per attached external centerline, rebuilt on every watch-set
    // recompute. Always non-null; owned by this object.
    cwAttachedCenterlinesModel* attachedCenterlinesModel() const { return m_attachedCenterlinesModel; }

    // How each attachment's remembered source compares with the copy in
    // the project — the one status source every surface reads
    // (plans/EXTERNAL_SOURCE_CHANGE_NOTIFY.html §3). Swept whenever the
    // attachments are re-read from disk, which is what gives a project
    // its statuses at open. Always non-null; owned by this object.
    cwExternalSourceStatusModel* sourceStatusModel() const { return m_sourceStatusModel; }

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

    // Emitted after a scan apply has fully installed its result (member
    // swap included) when the apply carried a solve request or the
    // declination flags changed. The consumer runs its solve here — the
    // ordering guarantees buildInput never reads half-applied flags.
    void solveNeeded();

    // Emitted whenever the set of owners whose in-project copy is missing
    // changes, including the paths it names. QML re-reads
    // missingCopyPath() here.
    void missingCopiesChanged();

    // Emitted whenever isOwnerBusy(ownerId) flips for ownerId — an
    // attach/replace/detach started or drained.
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
        // Containment boundary for this owner's in-project dependencies
        // (cwSaveLoad::dataRootDir). Empty when no saveLoad is wired, which
        // disables the check — scan-only tests have no project on disk to
        // be contained by.
        QString dataRootDir;
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
        QHash<QUuid, bool> fileOwnsDeclination;
        // Station names harvested from each trip owner's in-project entry
        // file, and cavern's complaint when that harvest failed. An owner
        // appears in at most one of them; an owner in neither had no
        // in-project entry to read.
        QHash<QUuid, QStringList> tripStations;
        QHash<QUuid, QString> tripHarvestErrors;
        // Owners (cave or trip) whose in-project entry file depends on a
        // path outside the project's data root, with the reason. Such an
        // owner is dropped from the solve entirely — see B7 in
        // plans/EXTERNAL_FILE_PHASE2.html. Its dependencies are also kept
        // out of the watch set, and it is never harvested, because the
        // harvest runs cavern over the entry file and cavern would read
        // the escaping path.
        QHash<QUuid, QString> containmentErrors;
        // Owners whose in-project entry file was gone when the worker
        // looked, mapped to the project-relative path of the file that
        // should be there (the file name alone when no saveLoad gives the
        // scan a project root).
        QHash<QUuid, QString> missingCopies;
        QVector<cwAttachedCenterlinesModel::Row> rows;
    };

    QPointer<cwCavingRegion> m_region;

    AsyncFuture::Restarter<ExternalScanResult> m_scanRestarter;
    cwFutureManagerToken m_futureManagerToken;

    cwAttachedCenterlinesModel* m_attachedCenterlinesModel;
    cwExternalSourceStatusModel* m_sourceStatusModel;

    // Rename wiring only: cave/trip nameChanged re-sorts the model rows
    // from cached counts. Survey-data connections that drive the solve
    // live on the consumer's own signaler.
    cwSurveyChunkSignaler* m_signaler;

    QHash<QUuid, QString> m_caveAttachmentDirs;
    QHash<QUuid, QString> m_tripAttachmentDirs;

    QPointer<cwExternalSourceSettings> m_externalSourceSettings;
    QPointer<cwSaveLoad> m_saveLoad;

    QFileSystemWatcher* m_watcher = nullptr;

    // Notification-only watcher over the files outside the project that
    // the attachments were copied from. Kept apart from m_watcher because
    // the two mean opposite things: an in-project event is an edit the
    // project owns, while a source-side event only updates a status the
    // user may act on (plans/EXTERNAL_SOURCE_CHANGE_NOTIFY.html §2).
    QFileSystemWatcher* m_sourceWatcher = nullptr;

    // The full intended watch set — the in-project dependencies of every
    // attachment — sorted and deduplicated. Compared against on every
    // recompute to figure out which addPath/removePath calls to send.
    // Paths that do not exist on disk are kept here too — they re-arm the
    // next time recompute runs and they appear.
    QStringList m_watchedFiles;

    // The source files and directories the last sweep armed
    // m_sourceWatcher on, sorted and deduplicated. Each sweep re-arms by
    // comparing this intent against what the watcher reports it holds, so
    // a path the watcher dropped or refused comes back.
    QStringList m_watchedSourceFiles;
    QStringList m_watchedSourceDirectories;

    // Sticky "request a solve when the next scan applies" flag, set by
    // paths that used to do recompute-then-solve synchronously. The
    // apply consumes it after the member swap and emits solveNeeded().
    bool m_solveOnScanApply = false;

    // Rows from the most recent scan apply; the rename rebuild reuses
    // their dep/warning counts so it never touches the disk.
    QVector<cwAttachedCenterlinesModel::Row> m_lastScanRows;

    // Per-owner operation registry: one entry while an attach/replace/detach
    // drains, guarding against interleaved per-owner filesystem operations
    // (commit-5 review's in-flight-token item, generalized at commit 9, made
    // a registry for cancelAttach at commit 12). The kind keeps cancelAttach
    // off detach operations; the cancelFlag is the attach orchestrator's
    // cancel seam. A replace registers as an Attach — to everything
    // downstream it is an attach over an occupied owner.
    enum class OperationKind { Attach, Detach };

    struct ActiveOperation {
        OperationKind kind;
        std::shared_ptr<std::atomic_bool> cancelFlag; // Attach only
    };

    QHash<QUuid, ActiveOperation> m_activeOperations;

    // Per-owner reason its in-project entry file reaches outside the
    // project's data root, from the most recent recompute. Membership is
    // what excludes the owner from the solve (solveInputs) and, for a trip,
    // what the file-error banner shows. Rebuilt wholesale on every
    // recompute, so fixing the file clears it.
    QHash<QUuid, QString> m_containmentErrors;

    // Per-owner project-relative path of an in-project copy that is gone
    // from disk, from the most recent recompute; read via
    // missingCopyPath(). Membership also drops the owner from the solve
    // (solveInputs), since cavern fatals on an *include it cannot open and
    // would cost the whole region its plot. Rebuilt wholesale, so restoring
    // the file clears it.
    QHash<QUuid, QString> m_missingCopies;

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

        // Token release without a report - for the destructor backstop and
        // any operation constructed with no report signal.
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

    // The whole synchronous refusal: queue the failure report on the
    // operation's completion signal and hand the caller an already
    // failed future carrying the same message. Every verb refuses this
    // way, so the shape lives here rather than in each of them.
    template<typename ResultT, typename Signal>
    QFuture<ResultT> refuseOperation(Signal signal,
                                     const QUuid& ownerId,
                                     const QString& error)
    {
        emitReportDeferred(signal, cwExternalCenterlineReport::failed(ownerId, error));
        return AsyncFuture::completed(ResultT(error));
    }

    // The file reloadFromSource would copy for `trip`, or an empty string
    // when this machine has none — the one place the eligibility rules
    // live, shared by the verb and canReloadFromSource.
    QString reloadSourcePath(cwTrip* trip) const;

    // Rebuilds both attachment-dir maps wholesale from the region walk
    // via cwSaveLoad::externalCenterlineDir (pure path math, no disk
    // I/O). Runs at every recompute snapshot when a saveLoad is wired,
    // so the maps track attach/detach/load without per-path bookkeeping;
    // the setters remain the seam for scan-only tests with no saveLoad.
    // Returns true when an owner already in the maps came back with a
    // different dir — the files the driver already *includes moved, so
    // the caller owes them a solve that re-reads them at the new path.
    bool refreshAttachmentDirsFromSaveLoad();

    // Stage 1: one value-type input per attached owner with a non-empty
    // entry file. Pure region/member reads, no filesystem access.
    QVector<OwnerScanInput> collectOwnerSnapshots() const;

    // Stage 2, runs on the worker thread: scans each owner's in-project
    // entry file. Static and pure — touches no member state, only the
    // filesystem and the promise. It produces one result when it runs to
    // completion, and none when `promise` is canceled partway through: a
    // superseded scan abandons its remaining owners instead of paying for
    // harvests nothing will read.
    static void scanOwners(QPromise<ExternalScanResult>& promise,
                           const QVector<OwnerScanInput>& owners);

    // Identity fields of a model row from an owner snapshot (counts and
    // lastSolved are filled by the caller).
    static cwAttachedCenterlinesModel::Row rowFromOwner(const OwnerScanInput& owner);

    // Stage 3, back on the main thread: watcher diff, member swaps,
    // signal emissions, model rows, and the solveNeeded() emission when
    // one was requested or the declination flags changed.
    void applyScanResult(ExternalScanResult result);

    // Pushes the scan's harvested station names and harvest errors onto the
    // live trips. Walks the whole region rather than the result's keys, so a
    // trip that was detached (or whose entry file vanished) since the last
    // scan is cleared instead of keeping names it no longer owns.
    void applyHarvestToTrips(const ExternalScanResult& result);

    // How ownerId's remembered source, fingerprinted as `stored` when it
    // was copied, compares with that source on disk now. Reads the disk,
    // and silently refreshes the stored stats when a stat-level difference
    // turns out to be identical content, so the fast path goes quiet again.
    cwExternalSourceStatusModel::Row
    sourceStatusFor(const QUuid& ownerId,
                    const cwExternalSourceSettings::SourceFingerprint& stored);

    // The whole answer to a source-side trigger — a watcher event, the
    // window regaining focus, a breadcrumb re-stamped: re-check every
    // remembered source into sourceStatusModel(), and arm m_sourceWatcher
    // on the fingerprinted files that are on disk here plus their parent
    // directories. It reads files outside the project and writes rows in
    // sourceStatusModel(); it copies nothing, requests no solve, and never
    // marks the project modified. Runs behind every scan apply too, so a
    // project open — which re-reads the attachments — sweeps the sources
    // with it. Quiet-path cost is one stat per fingerprinted file; only a
    // file whose size or mtime moved is read and hashed.
    void sweepSources();

    // Re-arms `watcher` for `path` (the file that just fired) so we
    // continue to receive change events for it; on macOS the watcher
    // implicitly drops a path after an atomic-write replace.
    static void rearmWatcher(QFileSystemWatcher* watcher, const QString& path);

private slots:
    // Async three-stage recompute: snapshot per-owner value inputs on
    // the main thread (no I/O), run the per-owner scans on a
    // cwConcurrent worker, then apply the result wholesale on the main
    // thread via applyScanResult. Coalesced through m_scanRestarter, so
    // a settings flurry or editor save burst runs one scan and only the
    // newest result lands. Paths that also need a solve set
    // m_solveOnScanApply first — the apply emits solveNeeded() so the
    // consumer's solve chains behind the member swap. A slot so the
    // signaler's externalCenterlineChanged / trip-list connections can
    // route here.
    void recomputeWatchSet();

    void onWatchedFileChanged(const QString& path);

    // A remembered source changed on disk. Re-arms the path (an editor's
    // save-by-rename drops it) and re-checks the statuses. Nothing else:
    // the project copy stays exactly as it was until the user asks for an
    // update.
    void onSourceFileChanged(const QString& path);

    // A directory holding a remembered source changed — the event that
    // catches a source deleted, or written back after it was gone.
    void onSourceDirectoryChanged();

    // An object whose on-disk directory just finished moving — cwSaveLoad
    // emits this once a rename's move job has landed. Every attachment dir
    // is derived from that directory, so the maps naming the files (and
    // the *include the driver emits) are stale until the recompute this
    // schedules re-derives them. Waiting for the move to land is what
    // keeps the scan from reading the empty new path and reporting the
    // copy missing.
    void onOwnerPathReady(QObject* object);

    // Cave/trip rename: rebuild the model rows from fresh names plus the
    // cached per-owner scan counts (m_lastScanRows) — zero disk I/O and
    // no waiting on a full recompute, so the list re-sorts immediately.
    void rebuildAttachedRowsFromNames();
};

#endif // CWEXTERNALCENTERLINEMANAGER_H
