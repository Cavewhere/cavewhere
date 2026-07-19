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
#include "cwExternalCenterlineScanner.h"
#include "cwExternalCenterlineSync.h"
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

    // Single watcher for both in-project attachment-dir dependencies and
    // live-link source-side dependencies. fileChanged hands the event off
    // to onWatchedFileChanged which decides between a plain re-solve and
    // the stale-flag flow.
    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::fileChanged,
            this, &cwExternalCenterlineManager::onWatchedFileChanged);

    // Renames re-sort the attached-centerlines rows immediately from
    // cached scan counts — no disk I/O, no waiting for a full recompute.
    m_signaler = new cwSurveyChunkSignaler(this);
    m_signaler->addConnectionToCaves(SIGNAL(nameChanged()), this, SLOT(rebuildAttachedRowsFromNames()));
    m_signaler->addConnectionToTrips(SIGNAL(nameChanged()), this, SLOT(rebuildAttachedRowsFromNames()));

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
    m_region = region;
    m_signaler->setRegion(region);

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
    m_saveLoad = saveLoad;
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
        // inputs. Clearing the since-snapshot set here — not at restart
        // time — matters: the restarter defers this lambda through the
        // event loop, and "since the snapshot" means since this instant.
        m_staleRaisedSinceSnapshot.clear();
        const QVector<OwnerScanInput> owners = collectOwnerSnapshots();

        // Stage 2 (worker thread): every scan and freshness probe. The
        // scanner and planner are stateless; `owners` crosses by value.
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

QVector<cwExternalCenterlineManager::OwnerScanInput> cwExternalCenterlineManager::collectOwnerSnapshots() const
{
    QVector<OwnerScanInput> owners;
    if (m_region.isNull()) {
        return owners;
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
                                       sourcePathForOwner(ownerId) });
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

    // Per-owner scan: project-side (in-project copy) and source-side
    // (live-link). Both feed the same watch set; the source-side
    // mapping survives separately so onWatchedFileChanged can route
    // to the stale flag. Each owner also contributes one row to the
    // attached-centerlines model, counted from the project-side scan
    // when it resolves (the solve *includes the in-project bytes) and
    // the source-side scan otherwise.
    for (const OwnerScanInput& owner : owners) {
        cwAttachedCenterlinesModel::Row row = rowFromOwner(owner);
        bool rowCounted = false;

        if (!owner.attachmentDir.isEmpty()) {
            const QString projectEntry = QDir(owner.attachmentDir).filePath(owner.entryFile);
            if (QFileInfo::exists(projectEntry)) {
                const auto scan = cwExternalCenterlineScanner::scan(projectEntry);
                if (!scan.hasError()) {
                    for (const QString& dep : scan.value().dependencies) {
                        result.watchedFiles.append(dep);
                    }
                    result.fileOwnsDeclination.insert(
                        owner.ownerId, scan.value().seededMetadata.fileOwnsDeclination());
                    row.depCount = scan.value().dependencies.size();
                    row.warningCount = scan.value().warnings.size();
                    rowCounted = true;
                }
            }
        }
        if (!owner.sourcePath.isEmpty()) {
            if (!QFileInfo::exists(owner.sourcePath)) {
                result.missingSourceOwners.append(owner.ownerId);
            } else {
                const auto scan = cwExternalCenterlineScanner::scan(owner.sourcePath);
                if (!scan.hasError()) {
                    for (const QString& dep : scan.value().dependencies) {
                        result.watchedFiles.append(dep);
                        // First owner wins; collisions between two
                        // live-link owners pointing at the same source
                        // tree are not a v1 use case.
                        if (!result.sourceOwnerForPath.contains(dep)) {
                            result.sourceOwnerForPath.insert(dep, owner.ownerId);
                        }
                    }
                    // The project-side flag wins when both scans resolve:
                    // the solve *includes the in-project copy, so its scan
                    // describes the bytes actually exported. The source
                    // scan only fills in when no project entry exists yet;
                    // once reconcile copies the source over, the next
                    // recompute converges the flag to the new bytes.
                    if (!result.fileOwnsDeclination.contains(owner.ownerId)) {
                        result.fileOwnsDeclination.insert(
                            owner.ownerId, scan.value().seededMetadata.fileOwnsDeclination());
                    }
                    if (!rowCounted) {
                        row.depCount = scan.value().dependencies.size();
                        row.warningCount = scan.value().warnings.size();
                    }
                    // Open-time / recompute freshness probe: a pending
                    // copy in the reconcile plan means the source has
                    // drifted from the in-project bytes. computePlan
                    // only reads the filesystem, so the probe is free
                    // of side effects (Phase 2 direction change).
                    if (!owner.attachmentDir.isEmpty()
                        && !cwExternalCenterlineSync::computePlan(
                                scan.value(), owner.attachmentDir).copies.isEmpty()) {
                        result.staleSourceOwners.append(owner.ownerId);
                    }
                }
            }
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
    std::sort(result.staleSourceOwners.begin(), result.staleSourceOwners.end());
    sortAttachedRows(result.rows);

    return result;
}

void cwExternalCenterlineManager::applyScanResult(ExternalScanResult result)
{
    // Commit-8 merge policy: a watcher event that fired after the
    // snapshot raised a flag the probe was computed without — union it
    // back in rather than letting the wholesale rebuild wipe it.
    for (const QUuid& ownerId : std::as_const(m_staleRaisedSinceSnapshot)) {
        if (!result.staleSourceOwners.contains(ownerId)) {
            result.staleSourceOwners.append(ownerId);
        }
    }
    std::sort(result.staleSourceOwners.begin(), result.staleSourceOwners.end());

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
        m_sourceOwnerForPath = std::move(result.sourceOwnerForPath);
        emit watchedFilesChanged();
    } else {
        // Union unchanged but per-owner source mapping may have shifted.
        m_sourceOwnerForPath = std::move(result.sourceOwnerForPath);
    }

    if (result.missingSourceOwners != m_missingSourceOwners) {
        m_missingSourceOwners = std::move(result.missingSourceOwners);
        emit missingSourceOwnersChanged();
    }

    if (result.staleSourceOwners != m_staleSourceOwners) {
        m_staleSourceOwners = std::move(result.staleSourceOwners);
        emit staleSourceOwnersChanged();
    }

    // Request the solve behind the flag swap so the consumer's buildInput
    // never reads half-applied declination state; an owner entering or
    // leaving the map is a change too (its *include just appeared or
    // vanished).
    const bool solveNow = m_solveOnScanApply
        || result.fileOwnsDeclination != m_fileOwnsDeclination;
    m_solveOnScanApply = false;
    m_fileOwnsDeclination = std::move(result.fileOwnsDeclination);

    m_lastScanRows = result.rows;
    m_attachedCenterlinesModel->setRows(std::move(result.rows));

    if (solveNow) {
        emit solveNeeded();
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

    const auto sourceIt = m_sourceOwnerForPath.constFind(path);
    if (sourceIt != m_sourceOwnerForPath.constEnd()) {
        if (!QFileInfo::exists(path)) {
            // The source-side file vanished (deleted or renamed away) —
            // "stale" would offer an Update for a file that is gone.
            // Reclassify through the full probe so the owner lands in
            // missingSourceOwners instead. (An editor's atomic-save
            // replace has already re-created the path by the time the
            // event is delivered, so it takes the stale branch below.)
            recomputeWatchSetAndProbeSources();
            return;
        }
        // Direction change (see the header comment on
        // setExternalSourceSettings): a source-side edit only flags the
        // owner stale. It never touches the filesystem or re-solves —
        // the user triggers the reconcile via updateFromSource. Also
        // record it for the apply-time union: a scan in flight probed
        // the disk before this edit and must not clear the flag.
        const QUuid ownerId = sourceIt.value();
        m_staleRaisedSinceSnapshot.insert(ownerId);
        if (!m_staleSourceOwners.contains(ownerId)) {
            m_staleSourceOwners.append(ownerId);
            std::sort(m_staleSourceOwners.begin(), m_staleSourceOwners.end());
            emit staleSourceOwnersChanged();
        }
        return;
    }

    // Project-side change: recompute the dep set in case the edit added
    // or removed an *include that needs to enter/leave the watch set,
    // with the re-solve chained behind the apply so it sees the fresh
    // declination flags.
    m_solveOnScanApply = true;
    recomputeWatchSetAndProbeSources();
}

void cwExternalCenterlineManager::updateFromSource(const QUuid& ownerId)
{
    if (m_updateInFlightOwners.contains(ownerId) || m_saveLoad.isNull()) {
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
        // Unreadable source: nothing sensible to reconcile. The stale
        // flag is probe-owned — the next recompute re-derives it.
        return;
    }
    m_updateInFlightOwners.insert(ownerId);
    auto future = cwExternalCenterlineSync::reconcile(
        m_saveLoad.data(), scan.value(), attachmentDir);
    // Reconcile drains through the cwSaveLoad job queue; the returned
    // future completes after every copy/remove has hit disk, which is
    // the moment a solve can see the fresh bytes in the in-project
    // copy. The recompute re-probes freshness (clearing the stale flag)
    // and installs any newly-added *include target in the watch set
    // before requesting the solve. The canceled path (project retired
    // mid-drain) must still release the per-owner token or the Update
    // affordance stays stuck for the rest of the session.
    AsyncFuture::observe(future).context(this,
            [this, ownerId](Monad::ResultBase) {
        m_updateInFlightOwners.remove(ownerId);
        m_solveOnScanApply = true;
        recomputeWatchSetAndProbeSources();
    },
            [this, ownerId]() {
        m_updateInFlightOwners.remove(ownerId);
    });
}
