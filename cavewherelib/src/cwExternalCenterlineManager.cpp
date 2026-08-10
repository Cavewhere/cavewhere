/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwExternalCenterlineManager.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwConcurrent.h"
#include "cwExternalCenterline.h"
#include "cwExternalCenterlineAttach.h"
#include "cwExternalCenterlineScanner.h"
#include "cwExternalCenterlineSync.h"
#include "cwExternalStationHarvest.h"
#include "cwSaveLoad.h"
#include "cwSurveyChunkSignaler.h"
#include "cwTrip.h"
#include "asyncfuture.h"

//Qt includes
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFuture>

//Std includes
#include <algorithm>
#include <tuple>

namespace {

// The ownerKind an OwnerScanInput carries for a trip-level attachment. Only
// those are harvested: a cave-level attachment has no trip to hold the names,
// and nothing consumes them yet. Compare-side spelling only — comparing a
// QString against this allocates nothing, whereas passing it to appendOwner's
// QString parameter would convert on every call.
constexpr QLatin1StringView kTripOwnerKind("Trip");

// How many offending paths the containment message names before it
// summarizes the rest. One is the overwhelmingly common case; the cap
// keeps a pathological file from filling the banner.
constexpr int kMaxNamedEscapingPaths = 3;

// The subset of `dependencies` that resolves outside `boundary`, and so
// cannot travel with the project. An empty boundary disables the check —
// see OwnerScanInput::dataRootDir.
QStringList escapingDependencies(const QStringList& dependencies, const QString& boundary)
{
    if (boundary.isEmpty()) {
        return QStringList();
    }

    QStringList escaping;
    for (const QString& dependency : dependencies) {
        if (!cwExternalCenterlineSync::isContainedIn(dependency, boundary)) {
            escaping.append(dependency);
        }
    }
    return escaping;
}

// The reason a non-empty `escaping` list keeps the owner out of the solve.
QString containmentErrorFor(const QStringList& escaping)
{
    QString named = escaping.mid(0, kMaxNamedEscapingPaths).join(QStringLiteral(", "));
    if (escaping.size() > kMaxNamedEscapingPaths) {
        named += QStringLiteral(" and %1 more")
                     .arg(escaping.size() - kMaxNamedEscapingPaths);
    }

    return QStringLiteral(
        "Left out of the survey: this file includes data stored outside the "
        "project folder (%1). Only files inside the project travel with it, so "
        "copy them in, or point the *include at a copy inside the project.")
        .arg(named);
}

// Deterministic presentation order: cave display name, then trip
// display name (cave-level owners sort ahead of their trips via the
// empty trip key), with ownerId as a stable tiebreak for duplicates.
void sortAttachedRows(QVector<cwAttachedCenterlinesModel::Row>& rows)
{
    std::stable_sort(rows.begin(), rows.end(),
                     [](const cwAttachedCenterlinesModel::Row& left,
                        const cwAttachedCenterlinesModel::Row& right) {
        const QString leftTripKey = left.ownerKind == QStringLiteral("Cave")
            ? QString() : left.ownerName;
        const QString rightTripKey = right.ownerKind == QStringLiteral("Cave")
            ? QString() : right.ownerName;
        return std::tie(left.caveName, leftTripKey, left.ownerId)
             < std::tie(right.caveName, rightTripKey, right.ownerId);
    });
}

} // namespace

cwExternalCenterlineManager::cwExternalCenterlineManager(QObject* parent) :
    QObject(parent),
    m_scanRestarter(this)
{
    m_attachedCenterlinesModel = new cwAttachedCenterlinesModel(this);

    // Watches the in-project dependencies of every attachment. fileChanged
    // hands the event off to onWatchedFileChanged, which dirties the
    // project and re-solves.
    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::fileChanged,
            this, &cwExternalCenterlineManager::onWatchedFileChanged);

    // Renames re-sort the attached-centerlines rows immediately from
    // cached scan counts — no disk I/O, no waiting for a full recompute.
    m_signaler = new cwSurveyChunkSignaler(this);
    m_signaler->addConnectionToCaves(SIGNAL(nameChanged()), this, SLOT(rebuildAttachedRowsFromNames()));
    m_signaler->addConnectionToTrips(SIGNAL(nameChanged()), this, SLOT(rebuildAttachedRowsFromNames()));

    // Any attach/detach — the wrappers here, undo/redo, or protobuf load
    // populating a trip after insertion — re-derives the watch set, dir
    // maps, rows, and missing-source probe. Trip-list churn matters too:
    // a cave inserted at load (or a trip restored by undo) can carry an
    // already-set externalCenterline that never fires the change signal
    // post-connect.
    m_signaler->addConnectionToCaves(SIGNAL(externalCenterlineChanged()), this, SLOT(recomputeWatchSetAndProbeSources()));
    m_signaler->addConnectionToCaves(SIGNAL(insertedTrips(int,int)), this, SLOT(recomputeWatchSetAndProbeSources()));
    m_signaler->addConnectionToCaves(SIGNAL(removedTrips(int,int)), this, SLOT(recomputeWatchSetAndProbeSources()));
    m_signaler->addConnectionToTrips(SIGNAL(externalCenterlineChanged()), this, SLOT(recomputeWatchSetAndProbeSources()));

    // Registering the scan with the future manager lets tests drain it
    // via futureManagerModel()->waitForFinished().
    m_scanRestarter.onFutureChanged([this]() {
        if (m_futureManagerToken.isValid()) {
            m_futureManagerToken.addJob({ QFuture<void>(m_scanRestarter.future()), QStringLiteral("External centerline scan") });
        }
    });
}

cwExternalCenterlineManager::~cwExternalCenterlineManager()
{
    // Cancel only, never drain (CLAUDE.md): the worker holds value
    // snapshots, the apply is context-guarded, and the restarter's alive
    // flag drops any queued start — nothing dangles without a pump.
    cancelScan();
}

void cwExternalCenterlineManager::cancelScan()
{
    m_scanRestarter.future().cancel();
}

void cwExternalCenterlineManager::setRegion(cwCavingRegion* region)
{
    if (!m_region.isNull()) {
        disconnect(m_region.data(), &cwCavingRegion::insertedCaves,
                   this, &cwExternalCenterlineManager::recomputeWatchSetAndProbeSources);
        disconnect(m_region.data(), &cwCavingRegion::removedCaves,
                   this, &cwExternalCenterlineManager::recomputeWatchSetAndProbeSources);
    }
    m_region = region;
    m_signaler->setRegion(region);
    if (!m_region.isNull()) {
        // Cave-list churn (project load, undo/redo) can bring in owners
        // whose externalCenterline was set before the signaler connected
        // — the per-object change signal never fires, so the list-level
        // signal drives the recompute.
        connect(m_region.data(), &cwCavingRegion::insertedCaves,
                this, &cwExternalCenterlineManager::recomputeWatchSetAndProbeSources);
        connect(m_region.data(), &cwCavingRegion::removedCaves,
                this, &cwExternalCenterlineManager::recomputeWatchSetAndProbeSources);
    }

    if (m_region.isNull()) {
        // Supersede any scan still in flight for the previous region —
        // its apply would otherwise reinstall that region's watch set and
        // model rows. The null-region snapshot produces an empty result,
        // clearing both.
        recomputeWatchSetAndProbeSources();
        return;
    }

    // The consumer's initial solve chains behind the scan's apply via
    // solveNeeded() — buildInput reads the declination flags, so it must
    // not race the worker.
    if (m_region->hasCaves()) {
        m_solveOnScanApply = true;
    }
    recomputeWatchSetAndProbeSources();
}

void cwExternalCenterlineManager::setFutureManagerToken(cwFutureManagerToken token)
{
    m_futureManagerToken = token;
}

void cwExternalCenterlineManager::setCaveAttachmentDirs(QHash<QUuid, QString> dirs)
{
    m_caveAttachmentDirs = std::move(dirs);
    recomputeWatchSetAndProbeSources();
}

void cwExternalCenterlineManager::setTripAttachmentDirs(QHash<QUuid, QString> dirs)
{
    m_tripAttachmentDirs = std::move(dirs);
    recomputeWatchSetAndProbeSources();
}

void cwExternalCenterlineManager::setExternalSourceSettings(cwExternalSourceSettings* settings)
{
    if (m_externalSourceSettings == settings) {
        return;
    }
    if (!m_externalSourceSettings.isNull()) {
        disconnect(m_externalSourceSettings.data(), &cwExternalSourceSettings::externalCenterlineSourcesChanged,
                   this, &cwExternalCenterlineManager::recomputeWatchSetAndProbeSources);
    }
    m_externalSourceSettings = settings;
    if (!m_externalSourceSettings.isNull()) {
        connect(m_externalSourceSettings.data(), &cwExternalSourceSettings::externalCenterlineSourcesChanged,
                this, &cwExternalCenterlineManager::recomputeWatchSetAndProbeSources);
    }
    recomputeWatchSetAndProbeSources();
}

void cwExternalCenterlineManager::setSaveLoad(cwSaveLoad* saveLoad)
{
    if (m_saveLoad == saveLoad) {
        return;
    }
    if (!m_saveLoad.isNull()) {
        disconnect(m_saveLoad.data(), &cwSaveLoad::dataRootChanged,
                   this, &cwExternalCenterlineManager::recomputeWatchSetAndProbeSources);
    }
    m_saveLoad = saveLoad;
    if (!m_saveLoad.isNull()) {
        // Every attachment dir is derived from the data root, so moving it
        // — a temporary project's first Save As does exactly that — leaves
        // the whole watch set naming files that are no longer there. The
        // recompute re-derives the dirs and re-arms on the new paths;
        // without it an attachment stops being watched for the rest of the
        // session, and the edits this class exists to notice go unseen.
        connect(m_saveLoad.data(), &cwSaveLoad::dataRootChanged,
                this, &cwExternalCenterlineManager::recomputeWatchSetAndProbeSources);
    }
    recomputeWatchSetAndProbeSources();
}

QStringList cwExternalCenterlineManager::watchedFiles() const
{
    return m_watchedFiles;
}

void cwExternalCenterlineManager::markSolved(const QDateTime& when)
{
    m_attachedCenterlinesModel->markSolved(when);
}

void cwExternalCenterlineManager::waitToFinish()
{
    AsyncFuture::waitForFinished(m_scanRestarter.future());
}

void cwExternalCenterlineManager::recomputeWatchSetAndProbeSources()
{
    m_scanRestarter.restart([this]() {
        // Stage 1 (main thread, no I/O): snapshot the per-owner value
        // inputs.
        refreshAttachmentDirsFromSaveLoad();
        const QVector<OwnerScanInput> owners = collectOwnerSnapshots();

        // Stage 2 (worker thread): the per-owner scans. The scanner is
        // stateless; `owners` crosses by value.
        auto future = cwConcurrent::run([owners]() {
            return scanOwners(owners);
        });

        // Stage 3 (main thread): install the result wholesale. A result
        // superseded by a newer restart never lands (the restarter
        // cancels this future), and a result outliving this object is
        // dropped by the context auto-disconnect.
        AsyncFuture::observe(future).context(this, [this](ExternalScanResult result) {
            applyScanResult(std::move(result));
        });

        return future;
    });
}

void cwExternalCenterlineManager::refreshAttachmentDirsFromSaveLoad()
{
    if (m_saveLoad.isNull() || m_region.isNull()) {
        return;
    }
    QHash<QUuid, QString> caveDirs;
    QHash<QUuid, QString> tripDirs;
    for (cwCave* cave : m_region->caves()) {
        if (!cave->externalCenterline().isEmpty()) {
            caveDirs.insert(cave->id(),
                            m_saveLoad->externalCenterlineDir(cave).absolutePath());
        }
        for (cwTrip* trip : cave->trips()) {
            if (!trip->externalCenterline().isEmpty()) {
                tripDirs.insert(trip->id(),
                                m_saveLoad->externalCenterlineDir(trip).absolutePath());
            }
        }
    }
    m_caveAttachmentDirs = std::move(caveDirs);
    m_tripAttachmentDirs = std::move(tripDirs);
}

QVector<cwExternalCenterlineManager::OwnerScanInput> cwExternalCenterlineManager::collectOwnerSnapshots() const
{
    QVector<OwnerScanInput> owners;
    if (m_region.isNull()) {
        return owners;
    }

    // One path-math read for the whole batch; empty without a saveLoad,
    // which leaves the containment check disabled for scan-only tests.
    const QString dataRootDir =
        m_saveLoad.isNull() ? QString() : m_saveLoad->dataRootDir().absolutePath();

    // One QSettings read for the whole batch. Per-owner sourcePathFor()
    // calls would each construct a QSettings, and this runs on the main
    // thread on every recompute.
    QHash<QUuid, QString> sourcePaths;
    if (!m_externalSourceSettings.isNull()) {
        const auto sources = m_externalSourceSettings->externalCenterlineSources();
        sourcePaths.reserve(sources.size());
        for (const auto& source : sources) {
            sourcePaths.insert(source.ownerId, source.sourcePath);
        }
    }

    const auto appendOwner = [&](const QUuid& ownerId,
                                 const QString& attachmentDir,
                                 const QString& entryFile,
                                 const QString& caveName,
                                 const QString& ownerName,
                                 const QString& ownerKind) {
        if (entryFile.isEmpty()) {
            return;
        }
        owners.append(OwnerScanInput { ownerId, caveName, ownerName, ownerKind,
                                       entryFile, attachmentDir,
                                       sourcePaths.value(ownerId),
                                       dataRootDir });
    };

    for (cwCave* cave : m_region->caves()) {
        if (!cave->externalCenterline().isEmpty()) {
            appendOwner(cave->id(),
                        m_caveAttachmentDirs.value(cave->id()),
                        cave->externalCenterline().entryFile(),
                        cave->name(),
                        cave->name(),
                        QStringLiteral("Cave"));
        }
        for (cwTrip* trip : cave->trips()) {
            if (!trip->externalCenterline().isEmpty()) {
                appendOwner(trip->id(),
                            m_tripAttachmentDirs.value(trip->id()),
                            trip->externalCenterline().entryFile(),
                            cave->name(),
                            trip->name(),
                            QStringLiteral("Trip"));
            }
        }
    }
    return owners;
}

cwAttachedCenterlinesModel::Row cwExternalCenterlineManager::rowFromOwner(const OwnerScanInput& owner)
{
    cwAttachedCenterlinesModel::Row row;
    row.ownerId = owner.ownerId;
    row.caveName = owner.caveName;
    row.ownerName = owner.ownerName;
    row.ownerKind = owner.ownerKind;
    row.entryFile = owner.entryFile;
    return row;
}

cwExternalCenterlineManager::ExternalScanResult cwExternalCenterlineManager::scanOwners(
    const QVector<OwnerScanInput>& owners)
{
    ExternalScanResult result;

    // Per-owner scan of the in-project copy — the only file the project
    // reads, and so the only one worth scanning or watching. A remembered
    // source is checked for existence and otherwise left alone. Each owner
    // contributes one row to the attached-centerlines model, counted from
    // the scan when it resolves.
    for (const OwnerScanInput& owner : owners) {
        cwAttachedCenterlinesModel::Row row = rowFromOwner(owner);

        if (!owner.attachmentDir.isEmpty()) {
            const QString projectEntry = QDir(owner.attachmentDir).filePath(owner.entryFile);
            if (QFileInfo::exists(projectEntry)) {
                const auto scan = cwExternalCenterlineScanner::scan(projectEntry);

                // Containment is decided before anything reads the file's
                // dependencies, and a failure keeps the owner out of the
                // solve. It also skips the harvest below, which runs cavern
                // over the entry file — cavern follows the very *include
                // being rejected.
                const QStringList escaping = scan.hasError()
                    ? QStringList()
                    : escapingDependencies(scan.value().dependencies, owner.dataRootDir);

                if (!escaping.isEmpty()) {
                    result.containmentErrors.insert(owner.ownerId,
                                                    containmentErrorFor(escaping));
                } else if (owner.ownerKind == kTripOwnerKind) {
                    // Station names, from cavern reading this one attachment on its
                    // own — the region solve can't supply them for exactly the
                    // attachments that need tying in, since it drops a survey
                    // nothing fixes.
                    const auto harvest = cwExternalStationHarvest::harvest(projectEntry);
                    if (harvest.hasError()) {
                        result.tripHarvestErrors.insert(owner.ownerId, harvest.errorMessage());
                    } else {
                        result.tripStations.insert(owner.ownerId, harvest.value());
                    }
                }

                // Everything below describes the file rather than deciding
                // whether to solve it, so an excluded owner still reports
                // it. The watch set keeps the dependencies that live inside
                // the project — the entry file among them, which is the file
                // the containment message asks the user to edit, and so the
                // only way the exclusion ever clears itself. The escaping
                // paths stay unwatched: the project cannot use them.
                if (!scan.hasError()) {
                    for (const QString& dep : scan.value().dependencies) {
                        if (!escaping.contains(dep)) {
                            result.watchedFiles.append(dep);
                        }
                    }
                    result.fileOwnsDeclination.insert(
                        owner.ownerId, scan.value().seededMetadata.fileOwnsDeclination());
                    row.depCount = scan.value().dependencies.size();
                    row.warningCount = scan.value().warnings.size();
                }
            }
        }

        // Existence only — the in-project copy is the file the project reads.
        if (!owner.sourcePath.isEmpty() && !QFileInfo::exists(owner.sourcePath)) {
            result.missingSourceOwners.append(owner.ownerId);
        }
        result.rows.append(row);
    }

    // Stable order so equality comparisons against m_watchedFiles are
    // deterministic, and the StringList tests can compare by index.
    const QSet<QString> dedup(result.watchedFiles.cbegin(), result.watchedFiles.cend());
    result.watchedFiles = QStringList(dedup.cbegin(), dedup.cend());
    std::sort(result.watchedFiles.begin(), result.watchedFiles.end());
    result.existingWatchedFiles.reserve(result.watchedFiles.size());
    for (const QString& path : std::as_const(result.watchedFiles)) {
        if (QFileInfo::exists(path)) {
            result.existingWatchedFiles.append(path);
        }
    }
    std::sort(result.missingSourceOwners.begin(), result.missingSourceOwners.end());
    sortAttachedRows(result.rows);

    return result;
}

void cwExternalCenterlineManager::applyScanResult(ExternalScanResult result)
{
    if (result.watchedFiles != m_watchedFiles) {
        // Wholesale-replace the watcher's table from our intent set. Files
        // that don't exist on disk are kept in m_watchedFiles (so a later
        // recompute can pick them up if they appear) but skipped on the
        // addPaths call - QFileSystemWatcher emits a warning otherwise.
        // The existence filter ran on the worker so the apply stays free
        // of file I/O.
        const QStringList currentlyWatched = m_watcher->files();
        if (!currentlyWatched.isEmpty()) {
            m_watcher->removePaths(currentlyWatched);
        }
        if (!result.existingWatchedFiles.isEmpty()) {
            m_watcher->addPaths(result.existingWatchedFiles);
        }
        m_watchedFiles = std::move(result.watchedFiles);
        emit watchedFilesChanged();
    }

    if (result.missingSourceOwners != m_missingSourceOwners) {
        m_missingSourceOwners = std::move(result.missingSourceOwners);
        emit missingSourceOwnersChanged();
    }

    // Request the solve behind the flag swap so the consumer's buildInput
    // never reads half-applied declination state; an owner entering or
    // leaving the map is a change too (its *include just appeared or
    // vanished).
    const bool solveNow = m_solveOnScanApply
        || result.fileOwnsDeclination != m_fileOwnsDeclination
        || result.containmentErrors != m_containmentErrors;
    m_solveOnScanApply = false;
    m_fileOwnsDeclination = std::move(result.fileOwnsDeclination);
    // An owner entering or leaving the excluded set changes the driver the
    // same way a declination flag does — its *include just vanished or
    // reappeared — so the swap lands ahead of the solve request.
    m_containmentErrors = result.containmentErrors;

    // Ahead of the solve request, so the solve that follows snapshots the
    // fresh names rather than the previous scan's.
    applyHarvestToTrips(result);

    m_lastScanRows = result.rows;
    m_attachedCenterlinesModel->setRows(std::move(result.rows));

    if (solveNow) {
        emit solveNeeded();
    }
}

void cwExternalCenterlineManager::applyHarvestToTrips(const ExternalScanResult& result)
{
    if (m_region.isNull()) {
        return;
    }

    for (cwCave* cave : m_region->caves()) {
        for (cwTrip* trip : cave->trips()) {
            trip->setExternalStations(result.tripStations.value(trip->id()));
            // A containment failure skipped the harvest, so the two are
            // never both present; naming it first keeps that explicit.
            const QString containmentError = result.containmentErrors.value(trip->id());
            trip->setExternalStationsError(
                containmentError.isEmpty()
                    ? result.tripHarvestErrors.value(trip->id())
                    : containmentError);
        }
    }
}

void cwExternalCenterlineManager::rebuildAttachedRowsFromNames()
{
    QVector<cwAttachedCenterlinesModel::Row> rows;
    const QVector<OwnerScanInput> owners = collectOwnerSnapshots();
    rows.reserve(owners.size());
    for (const OwnerScanInput& owner : owners) {
        cwAttachedCenterlinesModel::Row row = rowFromOwner(owner);
        // Counts come from the last scan; an owner attached since then
        // shows zeros until its scan applies. lastSolved is carried by
        // the model itself.
        const auto cached = std::find_if(m_lastScanRows.cbegin(), m_lastScanRows.cend(),
                                         [&owner](const cwAttachedCenterlinesModel::Row& candidate) {
            return candidate.ownerId == owner.ownerId;
        });
        if (cached != m_lastScanRows.cend()) {
            row.depCount = cached->depCount;
            row.warningCount = cached->warningCount;
        }
        rows.append(row);
    }
    sortAttachedRows(rows);
    m_lastScanRows = rows;
    m_attachedCenterlinesModel->setRows(std::move(rows));
}

QString cwExternalCenterlineManager::sourcePathForOwner(const QUuid& ownerId) const
{
    return m_externalSourceSettings.isNull() ? QString() : m_externalSourceSettings->sourcePathFor(ownerId);
}

void cwExternalCenterlineManager::rearmWatcher(const QString& path)
{
    // macOS atomic-replace (write-to-temp, rename-over) drops the path from
    // the watcher's internal table after the fileChanged event; Linux
    // inotify behaves similarly when the inode is replaced. Re-add so the
    // next edit still notifies us. No-op when the path is already armed.
    if (!m_watcher->files().contains(path) && QFileInfo::exists(path)) {
        m_watcher->addPath(path);
    }
}

void cwExternalCenterlineManager::onWatchedFileChanged(const QString& path)
{
    rearmWatcher(path);

    // Every watched file travels with the project, so every event here
    // is an edit the project owns and the project is modified. Nothing
    // on this path queues a save job, which is the only other way the
    // dirty bit gets set — so without this a bundled .cw drops the edit
    // on quit-without-save, never having asked.
    //
    // The containment test is what handles a moved data root: the watcher
    // stays armed on the old locations and does sometimes report them on the
    // way out, and those paths are outside the project as a matter of fact
    // rather than of timing. cwSaveLoad filters its own in-place rewrites.
    if (!m_saveLoad.isNull()
        && cwExternalCenterlineSync::isContainedIn(
               path, m_saveLoad->dataRootDir().absolutePath())) {
        m_saveLoad->reportProjectFileChangedOnDisk();
    }

    // Recompute the dep set in case the edit added or removed an *include
    // that needs to enter/leave the watch set, with the re-solve chained
    // behind the apply so it sees the fresh declination flags.
    m_solveOnScanApply = true;
    recomputeWatchSetAndProbeSources();
}

void cwExternalCenterlineManager::updateFromSource(const QUuid& ownerId)
{
    if (isOwnerBusy(ownerId) || m_saveLoad.isNull()) {
        return;
    }
    const QString sourcePath = sourcePathForOwner(ownerId);
    QString attachmentDir = m_caveAttachmentDirs.value(ownerId);
    if (attachmentDir.isEmpty()) {
        attachmentDir = m_tripAttachmentDirs.value(ownerId);
    }
    if (sourcePath.isEmpty()
        || !QFileInfo::exists(sourcePath)
        || attachmentDir.isEmpty()) {
        // Source went missing or we don't know where to reconcile to.
        // Recompute so missingSourceOwners reflects the current disk state.
        recomputeWatchSetAndProbeSources();
        return;
    }
    const auto scan = cwExternalCenterlineScanner::scan(sourcePath);
    if (scan.hasError()) {
        // Unreadable source: nothing sensible to reconcile.
        return;
    }
    auto guard = std::make_shared<OperationGuard>(
        this, ownerId, OperationKind::Update, nullptr, nullptr);
    auto future = cwExternalCenterlineSync::reconcile(
        m_saveLoad.data(), scan.value(), attachmentDir);
    // Reconcile drains through the cwSaveLoad job queue; the returned
    // future completes after every copy/remove has hit disk, which is
    // the moment a solve can see the fresh bytes in the in-project
    // copy. The recompute installs any newly-added *include target in the
    // watch set before requesting the solve. The canceled path (project
    // retired mid-drain) must still release the per-owner token or the
    // owner stays busy for the rest of the session.
    AsyncFuture::observe(future).context(this,
            [this, guard, ownerId](Monad::ResultBase) {
        guard->release();
        m_solveOnScanApply = true;
        recomputeWatchSetAndProbeSources();
    },
            [guard]() {
        guard->release();
    });
}

QFuture<Monad::Result<cwExternalCenterlineAttach::AttachReport>>
cwExternalCenterlineManager::attachCenterline(cwTrip* trip, const QString& sourcePath)
{
    using ReportResult = Monad::Result<cwExternalCenterlineAttach::AttachReport>;

    if (trip == nullptr) {
        return refuseOperation<ReportResult>(
            &cwExternalCenterlineManager::attachCompleted, QUuid(),
            QStringLiteral("attach: trip is null"));
    }
    const QUuid ownerId = trip->id();
    if (isOwnerBusy(ownerId)) {
        return refuseOperation<ReportResult>(
            &cwExternalCenterlineManager::attachCompleted, ownerId,
            QStringLiteral("attach: another operation for this trip is still in progress"));
    }

    auto cancelFlag = std::make_shared<std::atomic_bool>(false);
    auto guard = std::make_shared<OperationGuard>(
        this, ownerId, OperationKind::Attach, cancelFlag,
        &cwExternalCenterlineManager::attachCompleted);
    auto future = cwExternalCenterlineAttach::attach(
        trip, sourcePath, m_saveLoad.data(), m_externalSourceSettings.data(),
        std::move(cancelFlag));
    // The canceled path (project retired mid-attach, or cancelAttach
    // landing before the scan) must still release the token.
    AsyncFuture::observe(future).context(this,
            [this, guard, ownerId](const ReportResult& result) {
        if (!result.hasError()) {
            // The settings write already queued a recompute; make sure
            // its apply also requests the solve that picks up the new
            // owner's *include.
            m_solveOnScanApply = true;
            recomputeWatchSetAndProbeSources();
            const auto& attachReport = result.value();
            guard->finish(cwExternalCenterlineReport::succeeded(
                ownerId, attachReport.persisted.entryFile(), attachReport.warnings));
        } else {
            guard->finish(cwExternalCenterlineReport::failed(
                ownerId, result.errorMessage()));
        }
    },
            [guard, ownerId]() {
        guard->finish(cwExternalCenterlineReport::wasCanceled(ownerId));
    });
    return future;
}

QFuture<Monad::Result<cwExternalCenterlineAttach::AttachReport>>
cwExternalCenterlineManager::replaceCenterline(cwTrip* trip, const QString& sourcePath)
{
    using ReportResult = Monad::Result<cwExternalCenterlineAttach::AttachReport>;

    if (trip != nullptr && trip->externalCenterline().isEmpty()) {
        return refuseOperation<ReportResult>(
            &cwExternalCenterlineManager::attachCompleted, trip->id(),
            QStringLiteral("replace: this trip has no attached centerline to replace"));
    }

    // Attach already is replace: its reconcile copies the new closure
    // into the owner's existing attachment dir and GCs whatever the new
    // entry file stops referencing. It also owns the null-trip and
    // busy-owner refusals, so the nothing-attached guard above is the
    // only one replace adds.
    return attachCenterline(trip, sourcePath);
}

void cwExternalCenterlineManager::cancelAttach(const QUuid& ownerId)
{
    const auto it = m_activeOperations.constFind(ownerId);
    if (it == m_activeOperations.constEnd()
        || it->kind != OperationKind::Attach
        || it->cancelFlag == nullptr) {
        return;
    }
    it->cancelFlag->store(true);
}

QFuture<Monad::ResultBase> cwExternalCenterlineManager::detachCenterline(cwTrip* trip)
{
    using Monad::ResultBase;

    if (trip == nullptr) {
        return refuseOperation<ResultBase>(
            &cwExternalCenterlineManager::detachCompleted, QUuid(),
            QStringLiteral("detach: trip is null"));
    }
    const QUuid ownerId = trip->id();
    if (isOwnerBusy(ownerId)) {
        return refuseOperation<ResultBase>(
            &cwExternalCenterlineManager::detachCompleted, ownerId,
            QStringLiteral("detach: another operation for this trip is still in progress"));
    }

    auto guard = std::make_shared<OperationGuard>(
        this, ownerId, OperationKind::Detach, nullptr,
        &cwExternalCenterlineManager::detachCompleted);

    // Queued-invoke hole (commit-7 review): an updateFromSource invoke
    // already queued behind this call would otherwise still see the
    // owner's attachment dir and resurrect files into it. Drop the map
    // entry in the same synchronous block as detach()'s settings and
    // model clears so the late invoke hits the empty-guard and no-ops.
    m_tripAttachmentDirs.remove(ownerId);

    auto future = cwExternalCenterlineAttach::detach(
        trip, m_saveLoad.data(), m_externalSourceSettings.data());
    AsyncFuture::observe(future).context(this,
            [this, guard, ownerId](const ResultBase& result) {
        // Re-probe after the remove-tree drains (the settings clear
        // already queued one recompute; this one sees the dir gone) and
        // request the solve that drops the owner's *include.
        m_solveOnScanApply = true;
        recomputeWatchSetAndProbeSources();
        if (!result.hasError()) {
            guard->finish(cwExternalCenterlineReport::succeeded(ownerId));
        } else {
            guard->finish(cwExternalCenterlineReport::failed(
                ownerId, result.errorMessage()));
        }
    },
            [guard, ownerId]() {
        guard->finish(cwExternalCenterlineReport::wasCanceled(ownerId));
    });
    return future;
}

