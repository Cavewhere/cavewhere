/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwExternalCenterlineManager.h"
#include "cwCave.h"
#include "cwCavernRunner.h"
#include "cwCavingRegion.h"
#include "cwConcurrent.h"
#include "cwErrorListModel.h"
#include "cwExternalCenterline.h"
#include "cwExternalCenterlineAttach.h"
#include "cwExternalCenterlineScanner.h"
#include "cwExternalCenterlineSync.h"
#include "cwExternalStationHarvest.h"
#include "cwGeoReference.h"
#include "cwSaveLoad.h"
#include "cwSurveyChunkSignaler.h"
#include "cwSurveyNoteModel.h"
#include "cwSurveyTreeModel.h"
#include "cwTrip.h"
#include "asyncfuture.h"

//Qt includes
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFuture>
#include <QGuiApplication>
#include <QWindow>

//Std includes
#include <algorithm>
#include <functional>
#include <tuple>

namespace {

// The ownerKind an OwnerScanInput carries for a trip-level attachment; a
// node owner carries its cwSurveyNodeKind name. Only trips are harvested
// onto the owner itself. Compare-side spelling
// only — comparing a QString against this allocates nothing, whereas passing
// it to appendOwner's QString parameter would convert on every call.
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

// How `absolutePath` reads to someone looking at the project. The project
// copy is the only file the user has now, so it is named the way it sits in
// the project folder. An empty boundary (no saveLoad, scan-only tests) leaves
// the file name, which is still the shortest true name for it.
QString projectRelativePath(const QString& absolutePath, const QString& boundary)
{
    if (boundary.isEmpty()) {
        return QFileInfo(absolutePath).fileName();
    }
    return QDir(boundary).relativeFilePath(absolutePath);
}

// `message` as the project names its files: cavern echoes back the
// machine-specific in-project path it was handed, so it is renamed the way
// projectRelativePath names the same file for the missing-copy banner. Both
// separator forms are rewritten, since a path can arrive natively spelled, and
// the trailing separator keeps a sibling that merely shares the prefix intact.
// Keying on the data root, which holds still between runs, is what keeps an
// unchanged failure producing an identical message.
QString withProjectRelativePaths(const QString& message, const QString& boundary)
{
    if (boundary.isEmpty()) {
        return message;
    }

    const QString root = QDir::cleanPath(boundary);
    const QString prefix = root.endsWith(QLatin1Char('/'))
        ? root : root + QLatin1Char('/');
    const QString nativePrefix = QDir::toNativeSeparators(prefix);

    QString rewritten = message;
    rewritten.replace(prefix, QString());
    if (nativePrefix != prefix) {
        rewritten.replace(nativePrefix, QString());
    }
    return rewritten;
}

// The AttachedFileReadWarnings row for an owner whose scan raised
// `warnings`. The scanner names files canonically, so the data root is
// canonicalized before the paths are made project-relative.
cwError readWarningsFor(const QString& entryFile, const QStringList& warnings,
                        const QString& dataRootDir)
{
    const QString fileName = QFileInfo(entryFile).fileName();
    const QString message = warnings.size() == 1
        ? QStringLiteral("%1 was read with 1 warning.").arg(fileName)
        : QStringLiteral("%1 was read with %2 warnings.")
              .arg(fileName, QString::number(warnings.size()));

    const QString boundary = QFileInfo(dataRootDir).canonicalFilePath();
    QStringList lines;
    lines.reserve(warnings.size());
    for (const QString& warning : warnings) {
        lines.append(withProjectRelativePaths(warning, boundary));
    }

    cwError error(message, cwError::Warning);
    error.setDetail(lines.join(QLatin1Char('\n')));
    return error;
}

// Installs an owner's read-warnings row on its own error model; an empty
// `warning` removes the row, which is how a clean rescan clears it.
void setReadWarnings(cwErrorModel* errorModel, const cwError& warning, const QUuid& ownerId)
{
    if (errorModel != nullptr) {
        errorModel->errors()->setTypedWarning(cwErrorTypeId::AttachedFileReadWarnings,
                                              warning.message(), warning.detail(), ownerId);
    }
}

// True when every file in `stored` is on disk with the size and
// last-modified time it had when the fingerprint was taken — the quiet
// path, which reads no file (plans/EXTERNAL_SOURCE_CHANGE_NOTIFY.html §2).
bool statsMatchDisk(const cwExternalSourceSettings::SourceFingerprint& stored)
{
    for (const auto& record : stored.files) {
        const QFileInfo info(record.path);
        if (info.size() != record.size
            || info.lastModified().toMSecsSinceEpoch() != record.lastModified) {
            return false;
        }
    }
    return true;
}

// True when two fingerprints of the same files describe the same
// contents. Only the hashes are compared: the stats are exactly what a
// content-preserving rewrite (a git checkout, an editor saving unchanged
// text) moves, and they are what the caller goes on to refresh.
bool contentsMatch(const cwExternalSourceSettings::SourceFingerprint& left,
                   const cwExternalSourceSettings::SourceFingerprint& right)
{
    return std::equal(left.files.cbegin(), left.files.cend(),
                      right.files.cbegin(), right.files.cend(),
                      [](const cwExternalSourceSettings::SourceFileFingerprint& leftFile,
                         const cwExternalSourceSettings::SourceFileFingerprint& rightFile) {
        return leftFile.sha256 == rightFile.sha256;
    });
}

// An opaque token for the contents `fingerprint` describes: the file
// hashes in order, which moves whenever any of those files is edited
// again. What the app banner remembers when the user dismisses it, so a
// second edit of the same source is told apart from the first
// (plans/EXTERNAL_SOURCE_CHANGE_NOTIFY.html §4).
QString revisionOf(const cwExternalSourceSettings::SourceFingerprint& fingerprint)
{
    QStringList hashes;
    hashes.reserve(fingerprint.files.size());
    for (const auto& record : fingerprint.files) {
        hashes.append(record.sha256);
    }
    return hashes.join(QLatin1Char('-'));
}

QStringList fingerprintedPaths(const cwExternalSourceSettings::SourceFingerprint& fingerprint)
{
    QStringList paths;
    paths.reserve(fingerprint.files.size());
    for (const auto& record : fingerprint.files) {
        paths.append(record.path);
    }
    return paths;
}

// The spelling a path takes in a watch set: symlinks resolved and ".."
// collapsed, so two spellings of one file are one entry (the lesson from
// the live-link retirement's commit 3). A path that is gone has no
// canonical form, so it keeps its absolute spelling.
QString canonicalWatchPath(const QString& path)
{
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? info.absoluteFilePath() : canonical;
}

QStringList sortedList(const QSet<QString>& paths)
{
    QStringList list(paths.cbegin(), paths.cend());
    std::sort(list.begin(), list.end());
    return list;
}

// Adds the watch-set spelling of every file in `fingerprint` that is on
// disk here to `files`, and every parent directory that is on disk to
// `directories`. The parent is collected whether or not the file is there,
// so a source deleted and written again is noticed both ways. When the
// parent directory itself is gone, watching stops at that point and the
// focus re-sweep is what notices the folder and its file returning
// (plans/EXTERNAL_SOURCE_CHANGE_NOTIFY.html §2).
void collectSourceWatchPaths(const cwExternalSourceSettings::SourceFingerprint& fingerprint,
                             QSet<QString>& files,
                             QSet<QString>& directories)
{
    for (const auto& record : fingerprint.files) {
        const QFileInfo info(record.path);
        const QString directory = canonicalWatchPath(info.absolutePath());
        if (QFileInfo::exists(directory)) {
            directories.insert(directory);
        }
        if (info.exists()) {
            files.insert(canonicalWatchPath(info.absoluteFilePath()));
        }
    }
}

// Deterministic presentation order: node display name, then trip
// display name (node owners sort ahead of their node's trips via the
// empty trip key), with ownerId as a stable tiebreak for duplicates.
void sortAttachedRows(QVector<cwAttachedCenterlinesModel::Row>& rows)
{
    std::stable_sort(rows.begin(), rows.end(),
                     [](const cwAttachedCenterlinesModel::Row& left,
                        const cwAttachedCenterlinesModel::Row& right) {
        const QString leftTripKey = left.ownerKind == kTripOwnerKind
            ? left.ownerName : QString();
        const QString rightTripKey = right.ownerKind == kTripOwnerKind
            ? right.ownerName : QString();
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
    m_sourceStatusModel = new cwExternalSourceStatusModel(this);

    // Watches the in-project dependencies of every attachment. fileChanged
    // hands the event off to onWatchedFileChanged, which dirties the
    // project and re-solves.
    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::fileChanged,
            this, &cwExternalCenterlineManager::onWatchedFileChanged);

    // Watches the files outside the project the attachments were copied
    // from. Notification only: an event here recomputes statuses and
    // re-arms, and stops there.
    m_sourceWatcher = new QFileSystemWatcher(this);
    connect(m_sourceWatcher, &QFileSystemWatcher::fileChanged,
            this, &cwExternalCenterlineManager::onSourceFileChanged);
    connect(m_sourceWatcher, &QFileSystemWatcher::directoryChanged,
            this, &cwExternalCenterlineManager::onSourceDirectoryChanged);

    // The "edited it in another app and came back" moment: regaining focus
    // re-sweeps at one stat per fingerprinted file, which also covers
    // anything the platform watcher missed
    // (plans/EXTERNAL_SOURCE_CHANGE_NOTIFY.html §2). Guarded so a host with
    // no GUI — a console tool linking this library — simply has no such
    // moment.
    if (auto* guiApplication = qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        connect(guiApplication, &QGuiApplication::focusWindowChanged,
                this, [this](QWindow* window) {
            if (window != nullptr) {
                sweepSources();
            }
        });
    }

    // Renames re-sort the attached-centerlines rows immediately from
    // cached scan counts — no disk I/O, no waiting for a full recompute.
    m_signaler = new cwSurveyChunkSignaler(this);
    m_signaler->addConnectionToCaves(SIGNAL(nameChanged()), this, SLOT(rebuildAttachedRowsFromNames()));
    m_signaler->addConnectionToTrips(SIGNAL(nameChanged()), this, SLOT(rebuildAttachedRowsFromNames()));
    // A trip's file fixes are listed under its scope, so a scope move
    // renames those rows without waiting for the next scan. The node's
    // tripScopeLabelsChanged() fires once per scope move; each trip's
    // scopeChanged() would fire on every sibling at every trip insert.
    // A trip's own centerline change moves its scope too.
    m_signaler->addConnectionToCaves(SIGNAL(tripScopeLabelsChanged()), this, SLOT(refreshSenderAttachedFixes()));
    m_signaler->addConnectionToTrips(SIGNAL(externalCenterlineChanged()), this, SLOT(refreshSenderAttachedFixes()));
    // The rows name the system chosen for the file's bare fixes.
    m_signaler->addConnectionToCaves(SIGNAL(externalCenterlineChanged()), this, SLOT(refreshSenderAttachedFixes()));

    // Any attach/detach — the wrappers here, undo/redo, or protobuf load
    // populating a trip after insertion — re-derives the watch set, dir
    // maps, and rows. Trip-list churn matters too:
    // a cave inserted at load (or a trip restored by undo) can carry an
    // already-set externalCenterline that never fires the change signal
    // post-connect.
    m_signaler->addConnectionToCaves(SIGNAL(externalCenterlineChanged()), this, SLOT(recomputeWatchSet()));
    m_signaler->addConnectionToCaves(SIGNAL(insertedTrips(int,int)), this, SLOT(recomputeWatchSet()));
    m_signaler->addConnectionToCaves(SIGNAL(removedTrips(int,int)), this, SLOT(recomputeWatchSet()));
    // The same for a node arriving or leaving below a cave, which the
    // region's cave-list signals never hear.
    m_signaler->addConnectionToCaves(SIGNAL(insertedNodes(int,int)), this, SLOT(recomputeWatchSet()));
    m_signaler->addConnectionToCaves(SIGNAL(removedNodes(int,int)), this, SLOT(recomputeWatchSet()));
    m_signaler->addConnectionToTrips(SIGNAL(externalCenterlineChanged()), this, SLOT(recomputeWatchSet()));

    // Registering the scan with the future manager lets tests drain it
    // via futureManagerModel()->waitForFinished().
    m_scanRestarter.onFutureChanged([this]() {
        if (m_futureManagerToken.isValid()) {
            m_futureManagerToken.addJob({ QFuture<void>(m_scanRestarter.future()), QStringLiteral("External centerline scan") });
        }

        // The outer future tracks the apply, so it settles after the apply
        // has emitted its signals. A newer run in its place keeps the scan
        // going.
        const auto settle = [this]() {
            if (m_scanRestarter.future().isFinished()) {
                setScanning(false);
            }
        };
        AsyncFuture::observe(m_scanRestarter.future()).context(this, settle, settle);
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
                   this, &cwExternalCenterlineManager::recomputeWatchSet);
        disconnect(m_region.data(), &cwCavingRegion::removedCaves,
                   this, &cwExternalCenterlineManager::recomputeWatchSet);
        disconnect(m_region.data(), &cwCavingRegion::ownersDeleted,
                   this, &cwExternalCenterlineManager::clearBreadcrumbsForDeletedOwners);
        disconnect(m_region->geoReference(), &cwGeoReference::localProjectionChanged,
                   this, &cwExternalCenterlineManager::refreshAllAttachedFixes);
    }
    m_region = region;
    m_signaler->setRegion(region);
    if (!m_region.isNull()) {
        // Cave-list churn (project load, undo/redo) can bring in owners
        // whose externalCenterline was set before the signaler connected
        // — the per-object change signal never fires, so the list-level
        // signal drives the recompute.
        connect(m_region.data(), &cwCavingRegion::insertedCaves,
                this, &cwExternalCenterlineManager::recomputeWatchSet);
        connect(m_region.data(), &cwCavingRegion::removedCaves,
                this, &cwExternalCenterlineManager::recomputeWatchSet);
        // removedCaves also fires on close, load, and moves; ownersDeleted
        // fires only on the delete verbs, which is what the store needs.
        connect(m_region.data(), &cwCavingRegion::ownersDeleted,
                this, &cwExternalCenterlineManager::clearBreadcrumbsForDeletedOwners);
        // Gaining or losing a frame changes what a bare fix is read in.
        connect(m_region->geoReference(), &cwGeoReference::localProjectionChanged,
                this, &cwExternalCenterlineManager::refreshAllAttachedFixes);
    }

    if (m_region.isNull()) {
        // Supersede any scan still in flight for the previous region —
        // its apply would otherwise reinstall that region's watch set and
        // model rows. The null-region snapshot produces an empty result,
        // clearing both.
        recomputeWatchSet();
        return;
    }

    // The consumer's initial solve chains behind the scan's apply via
    // solveNeeded() — buildInput reads the declination flags, so it must
    // not race the worker.
    if (m_region->hasCaves()) {
        m_solveOnScanApply = true;
    }
    recomputeWatchSet();
}

void cwExternalCenterlineManager::setFutureManagerToken(cwFutureManagerToken token)
{
    m_futureManagerToken = token;
}

void cwExternalCenterlineManager::setCaveAttachmentDirs(QHash<QUuid, QString> dirs)
{
    m_caveAttachmentDirs = std::move(dirs);
    recomputeWatchSet();
}

void cwExternalCenterlineManager::setTripAttachmentDirs(QHash<QUuid, QString> dirs)
{
    m_tripAttachmentDirs = std::move(dirs);
    recomputeWatchSet();
}

cwLinePlotTask::ExternalCenterlineInputs cwExternalCenterlineManager::solveInputs() const
{
    // Both states name a file the solve cannot read, so both keep the
    // owner's *include off the driver. A missing copy is the costlier of
    // the two to leave in: cavern fatals on an *include it cannot open,
    // which loses the whole region its plot rather than this one survey.
    QSet<QUuid> excluded(m_containmentErrors.keyBegin(), m_containmentErrors.keyEnd());
    excluded.unite(QSet<QUuid>(m_missingCopies.keyBegin(), m_missingCopies.keyEnd()));

    return { m_caveAttachmentDirs,
             m_tripAttachmentDirs,
             m_fileOwnsDeclination,
             std::move(excluded),
             bareFixedStations(),
             m_ownerFixedStations,
             bareFixOutputSystems(),
             m_ownerDefaultedDatums };
}

void cwExternalCenterlineManager::setExternalSourceSettings(cwExternalSourceSettings* settings)
{
    if (m_externalSourceSettings == settings) {
        return;
    }

    // No recompute: the scan reads the in-project copies alone, so a
    // breadcrumb changing under it cannot change what any of them says.
    // The source statuses are another matter — a re-stamped breadcrumb
    // names a different file, or the same file in a new state, so the
    // watch set and the statuses follow the store.
    if (!m_externalSourceSettings.isNull()) {
        disconnect(m_externalSourceSettings.data(), &cwExternalSourceSettings::breadcrumbsChanged,
                   this, &cwExternalCenterlineManager::sweepSources);
    }
    m_externalSourceSettings = settings;
    if (!m_externalSourceSettings.isNull()) {
        connect(m_externalSourceSettings.data(), &cwExternalSourceSettings::breadcrumbsChanged,
                this, &cwExternalCenterlineManager::sweepSources);
    }
    sweepSources();
}

void cwExternalCenterlineManager::setSaveLoad(cwSaveLoad* saveLoad)
{
    if (m_saveLoad == saveLoad) {
        return;
    }
    if (!m_saveLoad.isNull()) {
        disconnect(m_saveLoad.data(), &cwSaveLoad::dataRootChanged,
                   this, &cwExternalCenterlineManager::recomputeWatchSet);
        disconnect(m_saveLoad.data(), &cwSaveLoad::objectPathReady,
                   this, &cwExternalCenterlineManager::onOwnerPathReady);
    }
    m_saveLoad = saveLoad;
    if (!m_saveLoad.isNull()) {
        connect(m_saveLoad.data(), &cwSaveLoad::objectPathReady,
                this, &cwExternalCenterlineManager::onOwnerPathReady);

        // Every attachment dir is derived from the data root, so moving it
        // — a temporary project's first Save As does exactly that — leaves
        // the whole watch set naming files that are no longer there. The
        // recompute re-derives the dirs and re-arms on the new paths;
        // without it an attachment stops being watched for the rest of the
        // session, and the edits this class exists to notice go unseen.
        connect(m_saveLoad.data(), &cwSaveLoad::dataRootChanged,
                this, &cwExternalCenterlineManager::recomputeWatchSet);
    }
    recomputeWatchSet();
}

QStringList cwExternalCenterlineManager::watchedFiles() const
{
    return m_watchedFiles;
}

QStringList cwExternalCenterlineManager::watchedSourceFiles() const
{
    return m_watchedSourceFiles;
}

void cwExternalCenterlineManager::markSolved(const QDateTime& when)
{
    m_attachedCenterlinesModel->markSolved(when);
}

void cwExternalCenterlineManager::waitToFinish()
{
    AsyncFuture::waitForFinished(m_scanRestarter.future());
}

void cwExternalCenterlineManager::setScanning(bool scanning)
{
    if (m_scanning == scanning) {
        return;
    }
    m_scanning = scanning;
    emit scanningChanged();
}

void cwExternalCenterlineManager::recomputeWatchSet()
{
    setScanning(true);
    m_scanRestarter.restart([this]() {
        // Stage 1 (main thread, no I/O): snapshot the per-owner value
        // inputs.
        if (refreshAttachmentDirsFromSaveLoad()) {
            // These maps are where the driver's *include paths come from,
            // so a dir that moved (a rename, or a Save As relocating the
            // data root) leaves the last solve reading a path that no
            // longer holds the file. Cavern fatals on an *include it
            // cannot open, which costs the whole region its plot until
            // something re-solves.
            m_solveOnScanApply = true;
        }
        const QVector<OwnerScanInput> owners = collectOwnerSnapshots();

        // Stage 2 (worker thread): the per-owner scans. The scanner is
        // stateless; `owners` crosses by value. The promise overload is
        // what lets the worker see the restarter's cancel and abandon a
        // superseded scan mid-batch.
        auto future = cwConcurrent::run([owners](QPromise<ExternalScanResult>& promise) {
            scanOwners(promise, owners);
        });

        // Stage 3 (main thread): install the result wholesale. A result
        // superseded by a newer restart never lands (the restarter
        // cancels this future), and a result outliving this object is
        // dropped by the context auto-disconnect. Canceling the returned
        // apply future cancels the worker too.
        return AsyncFuture::observe(future).context(this, [this](ExternalScanResult result) {
            applyScanResult(std::move(result));
        }).future();
    });
}

void cwExternalCenterlineManager::rescanAttachments()
{
    // The solve is requested too: a file that appeared (or changed) while
    // nothing watched it changes what the driver emits, and only the solve
    // shows that.
    m_solveOnScanApply = true;
    recomputeWatchSet();
}

bool cwExternalCenterlineManager::refreshAttachmentDirsFromSaveLoad()
{
    if (m_saveLoad.isNull() || m_region.isNull()) {
        return false;
    }
    QHash<QUuid, QString> caveDirs;
    QHash<QUuid, QString> tripDirs;

    // An owner that is only now entering the maps got here through an
    // attach, which requests its own solve; a moved dir is the case
    // nothing else notices.
    bool trackedDirMoved = false;
    const auto insertDir = [&trackedDirMoved](QHash<QUuid, QString>& dirs,
                                              const QHash<QUuid, QString>& previous,
                                              const QUuid& id,
                                              const QString& dir) {
        const auto previousDir = previous.constFind(id);
        if (previousDir != previous.constEnd() && *previousDir != dir) {
            trackedDirMoved = true;
        }
        dirs.insert(id, dir);
    };

    // Every node at every depth is a cwCave while the tree migration runs
    // (cwCave.h), so a Section's attachment lives where a cave's does.
    for (cwSurveyNode* node : m_region->rootNode()->allNodes()) {
        const auto* cave = qobject_cast<const cwCave*>(node);
        if (cave != nullptr && !cave->externalCenterline().isEmpty()) {
            insertDir(caveDirs, m_caveAttachmentDirs, cave->id(),
                      m_saveLoad->externalCenterlineDir(cave).absolutePath());
        }
        for (const cwTrip* trip : node->trips()) {
            if (!trip->externalCenterline().isEmpty()) {
                insertDir(tripDirs, m_tripAttachmentDirs, trip->id(),
                          m_saveLoad->externalCenterlineDir(trip).absolutePath());
            }
        }
    }
    m_caveAttachmentDirs = std::move(caveDirs);
    m_tripAttachmentDirs = std::move(tripDirs);
    return trackedDirMoved;
}

void cwExternalCenterlineManager::onOwnerPathReady(QObject* object)
{
    // Every other object cwSaveLoad reports (notes, sketches, LAZ layers)
    // sits outside the attachment tree. Whether this cave or trip owns an
    // attachment is left to the recompute's own map diff, so the rule
    // lives in one place.
    if (qobject_cast<cwCave*>(object) || qobject_cast<cwTrip*>(object)) {
        recomputeWatchSet();
    }
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

    const auto appendOwner = [&](const QUuid& ownerId,
                                 const QString& attachmentDir,
                                 const cwExternalCenterline& centerline,
                                 const QString& caveName,
                                 const QString& ownerName,
                                 const QString& ownerKind,
                                 const QString& kindLabel) {
        if (centerline.isEmpty()) {
            return;
        }
        owners.append(OwnerScanInput { ownerId, caveName, ownerName, ownerKind,
                                       centerline.entryFile(), attachmentDir, dataRootDir,
                                       kindLabel, centerline.fixCoordinateSystem() });
    };

    for (const cwSurveyNode* node : m_region->rootNode()->allNodes()) {
        if (!node->externalCenterline().isEmpty()) {
            appendOwner(node->id(),
                        m_caveAttachmentDirs.value(node->id()),
                        node->externalCenterline(),
                        node->name(),
                        node->name(),
                        QString::fromLatin1(QMetaEnum::fromType<cwSurveyNodeKind::Kind>()
                                                .valueToKey(static_cast<int>(node->kind()))),
                        cwSurveyTreeModel::kindLabel(node));
        }
        for (const cwTrip* trip : node->trips()) {
            if (!trip->externalCenterline().isEmpty()) {
                appendOwner(trip->id(),
                            m_tripAttachmentDirs.value(trip->id()),
                            trip->externalCenterline(),
                            node->name(),
                            trip->name(),
                            QStringLiteral("Trip"),
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
    row.kindLabel = owner.kindLabel;
    row.entryFile = owner.entryFile;
    return row;
}

void cwExternalCenterlineManager::scanOwners(QPromise<ExternalScanResult>& promise,
                                             const QVector<OwnerScanInput>& owners)
{
    ExternalScanResult result;

    // Per-owner scan of the in-project copy — the only file the project
    // reads, and so the only one worth scanning or watching. Each owner
    // contributes one row to the attached-centerlines model, counted from
    // the scan when it resolves.
    for (const OwnerScanInput& owner : owners) {
        // A superseded scan stops here. The harvest below runs cavern,
        // which serializes every caller on one global mutex, so each
        // remaining owner of an already-obsolete scan delays the next
        // scan's harvests and the solve behind them. Returning without a
        // result is safe: the future is canceled, so the observe/context
        // apply takes its canceled branch, and the restarter either drops
        // the delivery as generation-stale (a supersede) or cancels the
        // outer future (cancelScan) — it never reads a result.
        if (promise.isCanceled()) {
            return;
        }

        cwAttachedCenterlinesModel::Row row = rowFromOwner(owner);
        if (!owner.fixCoordinateSystem.isEmpty()) {
            result.ownerFixSystems.insert(owner.ownerId, owner.fixCoordinateSystem);
        }

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
                    // The row carries the reason too, since it is the only
                    // surface a cave-level owner has — cwTrip's banner reads
                    // externalStationsError, cwCave has no counterpart.
                    const QString containmentError = containmentErrorFor(escaping);
                    result.containmentErrors.insert(owner.ownerId, containmentError);
                    row.error = containmentError;
                } else {
                    // Station names, from cavern reading this one attachment on its
                    // own — the region solve can't supply them for exactly the
                    // attachments that need tying in or placing, since it drops a
                    // survey nothing fixes. A cave attachment's names are what its
                    // node's own fix stations are checked against. One cavern run
                    // per owner, cave or trip.
                    const QString bareFixSystem =
                        !scan.hasError() && scan.value().hasFixWithoutCoordinateSystem()
                            ? scan.value().outputCoordinateSystem
                            : QString();
                    const auto harvest =
                        cwExternalStationHarvest::harvestWithFixes(projectEntry, bareFixSystem);
                    if (harvest.hasError()) {
                        const QString harvestError =
                            withProjectRelativePaths(harvest.errorMessage(), owner.dataRootDir);
                        if (owner.ownerKind == kTripOwnerKind) {
                            result.tripHarvestErrors.insert(owner.ownerId, harvestError);
                        } else {
                            // The row is a cave owner's only surface (see above),
                            // and without names every fix on the cave's file
                            // stations is dropped as unknown. The row keeps one
                            // line; the region solve, which normally fails on
                            // the same error, carries the full log.
                            row.error = cwCavernRunner::firstErrorLine(harvestError);
                        }
                    } else {
                        result.ownerStations.insert(owner.ownerId, harvest.value().names);
                        if (!harvest.value().fixedNames.isEmpty()) {
                            result.ownerFixedStations.insert(owner.ownerId,
                                                             harvest.value().fixedNames);
                        }
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
                    if (owner.ownerKind != kTripOwnerKind) {
                        // The rows units.json was written from, read off this
                        // very scan of the copy so they keep the file's
                        // spelling: cavern reads a Compass or Walls name as
                        // written, and a tie has to name it the same way.
                        const auto format = cwExternalCenterlineScanner::formatFor(owner.entryFile);
                        const bool preserveCase = format == cwExternalCenterlineScanner::Format::Compass
                                                  || format == cwExternalCenterlineScanner::Format::Walls;
                        result.sourceViews.insert(
                            owner.ownerId,
                            cwExternalUnitManifest::solveView(scan.value().rows, preserveCase));
                    } else {
                        result.tripDeclinations.insert(
                            owner.ownerId,
                            cwExternalCenterlineScanner::attachmentDeclination(scan.value()));
                    }
                    for (const QString& dep : scan.value().dependencies) {
                        if (!escaping.contains(dep)) {
                            result.watchedFiles.append(dep);
                        }
                    }
                    result.fileOwnsDeclination.insert(
                        owner.ownerId, scan.value().seededMetadata.fileOwnsDeclination());
                    if (!scan.value().fixes.isEmpty()) {
                        // Kept relative to the attachment's folder, the way
                        // the Fix Stations page names the file. The scan
                        // names files canonically, so the folder is too.
                        const QDir attachmentDir(
                            QFileInfo(owner.attachmentDir).canonicalFilePath());
                        QList<cwExternalCenterlineScanner::ScannedFix> fixes = scan.value().fixes;
                        for (cwExternalCenterlineScanner::ScannedFix& fix : fixes) {
                            fix.file = attachmentDir.relativeFilePath(fix.file);
                        }
                        result.ownerFixes.insert(owner.ownerId, fixes);
                    }
                    if (!scan.value().defaultedDatums.isEmpty()) {
                        result.ownerDefaultedDatums.insert(owner.ownerId,
                                                           scan.value().defaultedDatums);
                    }
                    if (!scan.value().outputCoordinateSystem.isEmpty()) {
                        result.ownerOutputSystems.insert(owner.ownerId,
                                                         scan.value().outputCoordinateSystem);
                    }
                    row.depCount = scan.value().dependencies.size();
                    row.warningCount = scan.value().warnings.size();
                    if (!scan.value().warnings.isEmpty()) {
                        result.ownerReadWarnings.insert(
                            owner.ownerId,
                            readWarningsFor(owner.entryFile, scan.value().warnings,
                                            owner.dataRootDir));
                    }
                }
            } else {
                // The one file the project reads is gone. Nothing below can
                // run against it, so this is also the only account the owner
                // gets of why its stations and errors went quiet.
                result.missingCopies.insert(
                    owner.ownerId,
                    projectRelativePath(projectEntry, owner.dataRootDir));
            }
        }

        result.rows.append(row);
    }

    // Stable order so equality comparisons against m_watchedFiles are
    // deterministic, and the StringList tests can compare by index.
    result.watchedFiles =
        sortedList(QSet<QString>(result.watchedFiles.cbegin(), result.watchedFiles.cend()));
    result.existingWatchedFiles.reserve(result.watchedFiles.size());
    for (const QString& path : std::as_const(result.watchedFiles)) {
        if (QFileInfo::exists(path)) {
            result.existingWatchedFiles.append(path);
        }
    }
    sortAttachedRows(result.rows);

    promise.addResult(std::move(result));
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

    // Request the solve behind the flag swap so the consumer's buildInput
    // never reads half-applied declination state; an owner entering or
    // leaving the map is a change too (its *include just appeared or
    // vanished).
    const bool missingCopiesChangedNow = result.missingCopies != m_missingCopies;
    const bool solveNow = m_solveOnScanApply
        || result.fileOwnsDeclination != m_fileOwnsDeclination
        || result.ownerFixes != m_ownerFixes
        || result.ownerFixedStations != m_ownerFixedStations
        || result.ownerDefaultedDatums != m_ownerDefaultedDatums
        || result.ownerOutputSystems != m_ownerOutputSystems
        || result.ownerFixSystems != m_ownerFixSystems
        || result.containmentErrors != m_containmentErrors
        || missingCopiesChangedNow;
    m_solveOnScanApply = false;
    m_fileOwnsDeclination = std::move(result.fileOwnsDeclination);
    m_ownerFixes = std::move(result.ownerFixes);
    m_ownerFixedStations = std::move(result.ownerFixedStations);
    m_ownerDefaultedDatums = std::move(result.ownerDefaultedDatums);
    m_ownerOutputSystems = std::move(result.ownerOutputSystems);
    m_ownerFixSystems = std::move(result.ownerFixSystems);
    // An owner entering or leaving the excluded set changes the driver the
    // same way a declination flag does — its *include just vanished or
    // reappeared — so both halves of that set swap ahead of the solve
    // request.
    m_containmentErrors = result.containmentErrors;
    m_missingCopies = std::move(result.missingCopies);

    // Ahead of the solve request, so the solve that follows snapshots the
    // fresh names rather than the previous scan's.
    applyHarvest(result);

    m_lastScanRows = result.rows;
    m_attachedCenterlinesModel->setRows(std::move(result.rows));

    // Every path that re-reads the attachments comes through here —
    // project open among them — so the source sweep rides along rather
    // than needing a trigger list of its own.
    sweepSources();

    // Both emissions land after every member swap, so a handler — the trip
    // panel re-reading missingCopyPath(), the consumer's buildInput —
    // never sees a half-applied scan.
    if (missingCopiesChangedNow) {
        emit missingCopiesChanged();
    }

    if (solveNow) {
        emit solveNeeded();
    }
}

void cwExternalCenterlineManager::applyHarvest(const ExternalScanResult& result)
{
    if (m_region.isNull()) {
        return;
    }

    // A derived node's cavernLabel and a derived trip's cavernLabel and unit set
    // come from its source root's manifest; the member views read them all, so
    // a change drops them ahead of the solve.
    bool memberViewsMoved = false;
    for (cwSurveyNode* node : m_region->rootNode()->allNodes()) {
        node->setExternalStations(result.ownerStations.value(node->id()));
        setReadWarnings(node->errorModel(), result.ownerReadWarnings.value(node->id()),
                        node->id());

        const cwSurveyNode* sourceRoot = node->sourceRoot();
        const cwExternalUnitManifest::SolveView sourceView =
            sourceRoot != nullptr ? result.sourceViews.value(sourceRoot->id())
                                  : cwExternalUnitManifest::SolveView();
        const QString cavernLabel = node->isSourced() && !node->isSourceRoot()
            ? sourceView.cavernLabels.value(node->sourcePath())
            : QString();
        if (node->cavernLabel() != cavernLabel) {
            node->setCavernLabel(cavernLabel);
            memberViewsMoved = true;
        }

        for (cwTrip* trip : node->trips()) {
            if (trip->isDerived()) {
                const QStringList unitStations = sourceView.unitStations.value(trip->sourcePath());
                const QString tripLabel = sourceView.tripCavernLabels.value(trip->sourcePath());
                memberViewsMoved = memberViewsMoved || trip->externalStations() != unitStations
                                   || trip->cavernLabel() != tripLabel;
                trip->setCavernLabel(tripLabel);
                trip->setExternalStations(unitStations);
                trip->setExternalStationsError(QString());
                trip->setFileDeclination(sourceView.unitDeclinations.value(trip->sourcePath()));
                continue;
            }
            setReadWarnings(trip->errorModel(), result.ownerReadWarnings.value(trip->id()),
                            trip->id());
            trip->setExternalStations(result.ownerStations.value(trip->id()));
            trip->setFileDeclination(result.tripDeclinations.value(trip->id()));
            // A containment failure skipped the harvest, so the two are
            // never both present; naming it first keeps that explicit.
            const QString containmentError = result.containmentErrors.value(trip->id());
            trip->setExternalStationsError(
                containmentError.isEmpty()
                    ? result.tripHarvestErrors.value(trip->id())
                    : containmentError);
        }

        refreshAttachedFixes(node);
    }

    if (memberViewsMoved) {
        m_region->invalidateMemberViews();
    }
}

void cwExternalCenterlineManager::refreshAttachedFixes(cwSurveyNode* node)
{
    QList<cwAttachedFix> attachedFixes;
    QHash<QUuid, QStringList> fileFixedStations;
    // Named the way a fix on the owner names the station: a trip's file
    // stations sit under its scopePrefix(), as in fileFixedStations(). A fix
    // listed on a derived node keeps the owner's name for it.
    using ScannedFix = cwExternalCenterlineScanner::ScannedFix;
    const auto ownerFixes = [&](const QUuid& ownerId, const QString& scope,
                                const cwExternalCenterline& centerline,
                                const std::function<bool(const ScannedFix&)>& holds) {
        QList<cwAttachedFix> listed;
        const QString fileName = QFileInfo(centerline.entryFile()).fileName();
        const ReadInSystem readIn = readInSystemFor(ownerId, centerline);
        for (const ScannedFix& fix : m_ownerFixes.value(ownerId)) {
            if (!holds(fix)) {
                continue;
            }
            const bool bare = !fix.hasCoordinateSystem();
            listed.append({scope + fix.station, fix.coordinate, fix.coordinateSystem,
                           fileName, fix.file, fix.line,
                           bare ? readIn.system : QString(),
                           fix.reference,
                           bare && readIn.fileOutput,
                           ownerId});
        }
        return listed;
    };
    const auto addFileFixedStations = [&](const QUuid& ownerId) {
        const auto fixed = m_ownerFixedStations.constFind(ownerId);
        if (fixed != m_ownerFixedStations.constEnd()) {
            fileFixedStations.insert(ownerId, fixed.value());
        }
    };

    // A fix lives on the derived trip whose leaf Survex block holds it, else on
    // the derived node whose file holds it; the owner keeps the ones none of
    // its derived records take (the entry file's own, a hidden node's).
    const bool ownsAttachment = !node->externalCenterline().isEmpty();
    const cwSurveyNode* owner = ownsAttachment ? node
                                               : (node->isSourced() ? node->sourceRoot() : nullptr);
    const bool ownerAttaches = owner != nullptr && !owner->externalCenterline().isEmpty();
    // A fix's trip is a direct child of the node its nodePath names, so only
    // this node's derived trips can take one of the fixes it is offered.
    QSet<QString> tripPaths;
    for (const cwTrip* trip : node->trips()) {
        if (trip->isDerived()) {
            tripPaths.insert(trip->sourcePath());
        }
    }
    const auto takenByTrip = [&tripPaths](const ScannedFix& fix) {
        return tripPaths.contains(fix.tripPath);
    };
    const auto fixesFromOwner = [&](const std::function<bool(const ScannedFix&)>& holds) {
        return ownerFixes(owner->id(), QString(), owner->externalCenterline(), holds);
    };
    if (ownsAttachment) {
        const QSet<QString> derivedPaths = derivedNodePaths(node);
        attachedFixes.append(fixesFromOwner([&](const ScannedFix& fix) {
            return !derivedPaths.contains(fix.nodePath) && !takenByTrip(fix);
        }));
        addFileFixedStations(node->id());
    } else if (ownerAttaches) {
        const QString sourcePath = node->sourcePath();
        attachedFixes.append(fixesFromOwner([&](const ScannedFix& fix) {
            return fix.nodePath == sourcePath && !takenByTrip(fix);
        }));
    }
    for (cwTrip* trip : node->trips()) {
        if (trip->isDerived()) {
            const QString tripPath = trip->sourcePath();
            trip->setAttachedFixes(ownerAttaches
                ? fixesFromOwner([&tripPath](const ScannedFix& fix) { return fix.tripPath == tripPath; })
                : QList<cwAttachedFix>());
            continue;
        }
        attachedFixes.append(ownerFixes(trip->id(), trip->scopePrefix(), trip->externalCenterline(),
                                        [](const ScannedFix&) { return true; }));
        addFileFixedStations(trip->id());
    }

    node->setAttachedFixes(attachedFixes);
    node->setFileFixedStations(fileFixedStations);
}

QSet<QString> cwExternalCenterlineManager::derivedNodePaths(const cwSurveyNode* owner)
{
    QSet<QString> paths;
    if (!owner->isSourceRoot()) {
        return paths;
    }
    for (const cwSurveyNode* descendant : owner->allNodes()) {
        if (descendant->isSourced() && descendant->sourceRoot() == owner) {
            paths.insert(descendant->sourcePath());
        }
    }
    return paths;
}

void cwExternalCenterlineManager::refreshAllAttachedFixes()
{
    if (m_region.isNull()) {
        return;
    }
    for (cwSurveyNode* node : m_region->rootNode()->allNodes()) {
        refreshAttachedFixes(node);
    }
}

cwExternalCenterlineManager::ReadInSystem cwExternalCenterlineManager::readInSystemFor(
    const QUuid& ownerId, const cwExternalCenterline& centerline) const
{
    const bool georeferenced =
        !m_region.isNull() && m_region->geoReference()->hasCoordinateSystem();
    const QString fileOutput = georeferenced ? QString() : m_ownerOutputSystems.value(ownerId);
    // Like the driver, which names the chosen system only where some *cs out
    // is in force for it.
    if (!centerline.fixCoordinateSystem().isEmpty() && (georeferenced || !fileOutput.isEmpty())) {
        return {centerline.fixCoordinateSystem(), false};
    }
    return {fileOutput, !fileOutput.isEmpty()};
}

void cwExternalCenterlineManager::refreshSenderAttachedFixes()
{
    if (const auto* trip = qobject_cast<cwTrip*>(sender())) {
        if (trip->parentNode() != nullptr) {
            refreshAttachedFixes(trip->parentNode());
        }
        return;
    }
    // A node's attachment reaches the derived nodes under it, which list the
    // fixes their files hold.
    if (auto* node = qobject_cast<cwSurveyNode*>(sender())) {
        refreshAttachedFixes(node);
        for (cwSurveyNode* descendant : node->allNodes()) {
            refreshAttachedFixes(descendant);
        }
    }
}

QHash<QUuid, QString> cwExternalCenterlineManager::bareFixOutputSystems() const
{
    QHash<QUuid, QString> systems;
    const QHash<QUuid, QStringList> bare = bareFixedStations();
    for (auto it = bare.constBegin(); it != bare.constEnd(); ++it) {
        const QString system = m_ownerOutputSystems.value(it.key());
        if (!system.isEmpty()) {
            systems.insert(it.key(), system);
        }
    }
    return systems;
}

QHash<QUuid, QStringList> cwExternalCenterlineManager::bareFixedStations() const
{
    QHash<QUuid, QStringList> stations;
    for (auto it = m_ownerFixes.constBegin(); it != m_ownerFixes.constEnd(); ++it) {
        QStringList bare;
        for (const cwExternalCenterlineScanner::ScannedFix& fix : it.value()) {
            if (!fix.hasCoordinateSystem()) {
                bare.append(fix.station);
            }
        }
        if (!bare.isEmpty()) {
            stations.insert(it.key(), bare);
        }
    }
    return stations;
}

cwExternalSourceStatusModel::Row
cwExternalCenterlineManager::sourceStatusFor(
    const QUuid& ownerId, const cwExternalSourceSettings::SourceFingerprint& stored)
{
    using Status = cwExternalSourceStatusModel::Status;

    cwExternalSourceStatusModel::Row row;
    row.ownerId = ownerId;
    row.status = Status::NoBreadcrumb;
    row.sourcePath = m_externalSourceSettings->breadcrumbPath(ownerId);

    if (row.sourcePath.isEmpty()) {
        return row;
    }

    // An entry recorded before fingerprints existed has nothing to
    // compare against, and stays quiet: the feature never nags about data
    // older than itself (plans/EXTERNAL_SOURCE_CHANGE_NOTIFY.html §5 N1).
    if (stored.isEmpty()) {
        return row;
    }

    // Missing is decided from the entry file alone. A fingerprinted
    // dependency that is gone reads as Changed instead, which is what it
    // usually is: dropping an *include and deleting the file it named is
    // an ordinary edit, and re-copying from the entry file picks up the
    // smaller dependency set. Only a gone entry file leaves nothing to
    // copy from, which is what Update-all skips
    // (plans/EXTERNAL_SOURCE_CHANGE_NOTIFY.html §4).
    if (!QFileInfo::exists(row.sourcePath)) {
        row.status = Status::SourceMissing;
        return row;
    }

    if (statsMatchDisk(stored)) {
        row.status = Status::UpToDate;
        return row;
    }

    // A stat-level difference is only a question; the hash answers it.
    const auto current =
        cwExternalSourceSettings::computeFingerprint(fingerprintedPaths(stored));
    if (contentsMatch(stored, current)) {
        // Identical contents under fresh stats — record the new stats so
        // the next check takes the pure-stat path. Nothing about the
        // project changed, so nothing is emitted and nothing is dirtied.
        m_externalSourceSettings->refreshFingerprintStats(ownerId, current);
        row.status = Status::UpToDate;
        return row;
    }

    row.status = Status::Changed;
    row.sourceRevision = revisionOf(current);
    return row;
}

void cwExternalCenterlineManager::sweepSources()
{
    QVector<cwExternalSourceStatusModel::Row> rows;
    QSet<QString> files;
    QSet<QString> directories;

    // Without the store nothing remembers where anything came from, so no
    // attachment has a source to compare against or to watch.
    if (!m_externalSourceSettings.isNull()) {
        // The owners the last scan published are exactly the attachments,
        // so the sweep reads them instead of walking the region again.
        rows.reserve(m_lastScanRows.size());
        for (const cwAttachedCenterlinesModel::Row& attached : std::as_const(m_lastScanRows)) {
            // One read of the stored fingerprint answers both halves of the
            // sweep: it is what the status is decided against, and it names
            // the files to watch.
            const auto stored = m_externalSourceSettings->fingerprint(attached.ownerId);
            rows.append(sourceStatusFor(attached.ownerId, stored));
            collectSourceWatchPaths(stored, files, directories);
        }
    }

    m_sourceStatusModel->setRows(std::move(rows));

    // Swaps one half of m_sourceWatcher's table over to `desired` when the
    // watcher is holding something else. The comparison is against what
    // the watcher reports it actually holds — `armedNow` — rather than
    // what a past sweep meant to arm, so a path an addPaths call quietly
    // refused is tried again on the next sweep.
    const auto armPaths = [this](const QStringList& desired,
                                 const QStringList& armedNow,
                                 QStringList& intended) {
        intended = desired;

        const QStringList armed =
            sortedList(QSet<QString>(armedNow.cbegin(), armedNow.cend()));
        if (desired == armed) {
            return;
        }
        if (!armed.isEmpty()) {
            m_sourceWatcher->removePaths(armed);
        }
        if (!desired.isEmpty()) {
            m_sourceWatcher->addPaths(desired);
        }
    };

    armPaths(sortedList(files), m_sourceWatcher->files(), m_watchedSourceFiles);
    armPaths(sortedList(directories), m_sourceWatcher->directories(),
             m_watchedSourceDirectories);
}

void cwExternalCenterlineManager::clearBreadcrumbsForDeletedOwners(const QList<QUuid>& ownerIds)
{
    if (m_externalSourceSettings.isNull()) {
        return;
    }

    for (const QUuid& ownerId : ownerIds) {
        // Silent for an owner that never had one: clearBreadcrumb returns
        // before it emits when the store holds neither key.
        m_externalSourceSettings->clearBreadcrumb(ownerId);
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
            row.error = cached->error;
        }
        rows.append(row);
    }
    sortAttachedRows(rows);
    m_lastScanRows = rows;
    m_attachedCenterlinesModel->setRows(std::move(rows));
}

void cwExternalCenterlineManager::rearmWatcher(QFileSystemWatcher* watcher, const QString& path)
{
    // macOS atomic-replace (write-to-temp, rename-over) drops the path from
    // the watcher's internal table after the fileChanged event; Linux
    // inotify behaves similarly when the inode is replaced. Re-add so the
    // next edit still notifies us. No-op when the path is already armed.
    if (!watcher->files().contains(path) && QFileInfo::exists(path)) {
        watcher->addPath(path);
    }
}

void cwExternalCenterlineManager::onSourceFileChanged(const QString& path)
{
    rearmWatcher(m_sourceWatcher, path);
    sweepSources();
}

void cwExternalCenterlineManager::onSourceDirectoryChanged()
{
    sweepSources();
}

void cwExternalCenterlineManager::onWatchedFileChanged(const QString& path)
{
    rearmWatcher(m_watcher, path);

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
    recomputeWatchSet();
}

QFuture<Monad::Result<cwExternalCenterlineAttach::AttachReport>>
cwExternalCenterlineManager::runAttachOperation(const QUuid& ownerId,
                                                const QString& ownerNoun,
                                                const AttachStarter& start)
{
    using ReportResult = Monad::Result<cwExternalCenterlineAttach::AttachReport>;

    if (isOwnerBusy(ownerId)) {
        return refuseOperation<ReportResult>(
            &cwExternalCenterlineManager::attachCompleted, ownerId,
            QStringLiteral("attach: another operation for this %1 is still in progress")
                .arg(ownerNoun));
    }

    auto cancelFlag = std::make_shared<std::atomic_bool>(false);
    auto guard = std::make_shared<OperationGuard>(
        this, ownerId, OperationKind::Attach, cancelFlag,
        &cwExternalCenterlineManager::attachCompleted);
    auto future = start(std::move(cancelFlag));
    // The canceled path (project retired mid-attach, or cancelAttach
    // landing before the scan) must still release the token.
    AsyncFuture::observe(future).context(this,
            [this, guard, ownerId](const ReportResult& result) {
        if (!result.hasError()) {
            // This is the only recompute after an attach lands — the
            // settings write triggers nothing — and its apply must also
            // request the solve that picks up the new owner's *include.
            m_solveOnScanApply = true;
            recomputeWatchSet();
            const auto& attachReport = result.value();
            const QUuid sourceRootId =
                attachReport.root.isNull() ? QUuid() : attachReport.root->id();
            guard->finish(cwExternalCenterlineReport::succeeded(
                ownerId, attachReport.persisted.entryFile(), attachReport.warnings,
                sourceRootId));
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

QFuture<Monad::ResultBase>
cwExternalCenterlineManager::runDetachOperation(const QUuid& ownerId,
                                                const QString& ownerNoun,
                                                QHash<QUuid, QString>& attachmentDirs,
                                                const std::function<QFuture<Monad::ResultBase>()>& start)
{
    using Monad::ResultBase;

    if (isOwnerBusy(ownerId)) {
        return refuseOperation<ResultBase>(
            &cwExternalCenterlineManager::detachCompleted, ownerId,
            QStringLiteral("detach: another operation for this %1 is still in progress")
                .arg(ownerNoun));
    }

    auto guard = std::make_shared<OperationGuard>(
        this, ownerId, OperationKind::Detach, nullptr,
        &cwExternalCenterlineManager::detachCompleted);

    // Queued-invoke hole (commit-7 review): anything already queued behind
    // this call would otherwise still see the owner's attachment dir and
    // could write into it. Drop the map entry in the same synchronous block
    // as the operation's settings and model clears, so a late caller finds
    // the owner unattached rather than half-detached.
    attachmentDirs.remove(ownerId);

    auto future = start();
    AsyncFuture::observe(future).context(this,
            [this, guard, ownerId](const ResultBase& result) {
        // Recompute once the remove has drained, so the scan sees the dir
        // gone, and request the solve that drops the owner's *include.
        m_solveOnScanApply = true;
        recomputeWatchSet();
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

QFuture<Monad::Result<cwExternalCenterlineAttach::AttachReport>>
cwExternalCenterlineManager::attachCenterline(cwTrip* trip, const QString& sourcePath)
{
    using ReportResult = Monad::Result<cwExternalCenterlineAttach::AttachReport>;

    if (trip == nullptr) {
        return refuseOperation<ReportResult>(
            &cwExternalCenterlineManager::attachCompleted, QUuid(),
            QStringLiteral("attach: trip is null"));
    }
    return runAttachOperation(trip->id(), QStringLiteral("trip"),
                              [this, trip, sourcePath](std::shared_ptr<std::atomic_bool> cancelFlag) {
        return cwExternalCenterlineAttach::attach(
            trip, sourcePath, m_saveLoad.data(), m_externalSourceSettings.data(),
            std::move(cancelFlag));
    });
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

    // Attach already is replace for a trip: its reconcile copies the new
    // closure into the owner's existing attachment dir and GCs whatever the
    // new entry file stops referencing. It also owns the null-owner and
    // busy-owner refusals, so the nothing-attached guard above is the only
    // one replace adds.
    return attachCenterline(trip, sourcePath);
}

QFuture<Monad::Result<cwExternalCenterlineAttach::AttachReport>>
cwExternalCenterlineManager::attachSource(cwSurveyNode* target, const QString& sourcePath)
{
    using ReportResult = Monad::Result<cwExternalCenterlineAttach::AttachReport>;

    if (target == nullptr) {
        return refuseOperation<ReportResult>(
            &cwExternalCenterlineManager::attachCompleted, QUuid(),
            QStringLiteral("attach: target node is null"));
    }
    return runAttachOperation(target->id(), QStringLiteral("node"),
                              [this, target, sourcePath](std::shared_ptr<std::atomic_bool> cancelFlag) {
        return cwExternalCenterlineAttach::attach(
            target, sourcePath, m_saveLoad.data(), m_externalSourceSettings.data(),
            std::move(cancelFlag));
    });
}

QFuture<Monad::Result<cwExternalCenterlineAttach::AttachReport>>
cwExternalCenterlineManager::replaceSource(cwSurveyNode* root, const QString& sourcePath)
{
    using ReportResult = Monad::Result<cwExternalCenterlineAttach::AttachReport>;

    if (root == nullptr) {
        return refuseOperation<ReportResult>(
            &cwExternalCenterlineManager::attachCompleted, QUuid(),
            QStringLiteral("replace: source root is null"));
    }
    if (root->externalCenterline().isEmpty()) {
        return refuseOperation<ReportResult>(
            &cwExternalCenterlineManager::attachCompleted, root->id(),
            QStringLiteral("replace: this node has no attached survey file to replace"));
    }
    return runAttachOperation(root->id(), QStringLiteral("source root"),
                              [this, root, sourcePath](std::shared_ptr<std::atomic_bool> cancelFlag) {
        return cwExternalCenterlineAttach::replace(
            root, sourcePath, m_saveLoad.data(), m_externalSourceSettings.data(),
            std::move(cancelFlag));
    });
}

QFuture<Monad::ResultBase> cwExternalCenterlineManager::deleteSourceRoot(cwSurveyNode* root)
{
    using Monad::ResultBase;

    if (root == nullptr) {
        return refuseOperation<ResultBase>(
            &cwExternalCenterlineManager::detachCompleted, QUuid(),
            QStringLiteral("delete: source root is null"));
    }
    return runDetachOperation(root->id(), QStringLiteral("source root"), m_caveAttachmentDirs,
                              [this, root]() {
        return cwExternalCenterlineAttach::deleteSourceRoot(
            root, m_saveLoad.data(), m_externalSourceSettings.data());
    });
}

template<typename OwnerT>
QString cwExternalCenterlineManager::reloadSourcePathForOwner(const OwnerT* owner) const
{
    if (owner == nullptr || m_externalSourceSettings.isNull()
        || owner->externalCenterline().isEmpty()) {
        return QString();
    }

    const QUuid ownerId = owner->id();
    const QString sourcePath = m_externalSourceSettings->breadcrumbPath(ownerId);
    if (sourcePath.isEmpty() || !QFileInfo::exists(sourcePath)) {
        return QString();
    }

    // A breadcrumb pointing anywhere inside the project's data root names
    // one of CaveWhere's own copies — the owner's copy itself in the usual
    // case — and copying a file over itself is no reload at all. The data
    // root is also the boundary the attach guard refuses at, so the Reload
    // button and the guard tell the same story.
    if (!m_saveLoad.isNull()
        && cwExternalCenterlineSync::isContainedIn(
               sourcePath, m_saveLoad->dataRootDir().absolutePath())) {
        return QString();
    }

    return sourcePath;
}

bool cwExternalCenterlineManager::canReloadFromSource(cwTrip* trip) const
{
    return !reloadSourcePathForOwner(trip).isEmpty();
}

bool cwExternalCenterlineManager::canReloadFromSource(cwSurveyNode* root) const
{
    return !reloadSourcePathForOwner(root).isEmpty();
}

template<typename OwnerT, typename ReplaceFn>
QFuture<Monad::Result<cwExternalCenterlineAttach::AttachReport>>
cwExternalCenterlineManager::reloadFromSourceForOwner(OwnerT* owner,
                                                      const QString& ownerNoun,
                                                      ReplaceFn replace)
{
    using ReportResult = Monad::Result<cwExternalCenterlineAttach::AttachReport>;

    if (owner == nullptr) {
        return refuseOperation<ReportResult>(
            &cwExternalCenterlineManager::attachCompleted, QUuid(),
            QStringLiteral("reload: %1 is null").arg(ownerNoun));
    }

    const QUuid ownerId = owner->id();
    if (isOwnerBusy(ownerId)) {
        return refuseOperation<ReportResult>(
            &cwExternalCenterlineManager::attachCompleted, ownerId,
            QStringLiteral("reload: another operation for this %1 is still in progress")
                .arg(ownerNoun));
    }

    const QString sourcePath = reloadSourcePathForOwner(owner);
    if (sourcePath.isEmpty()) {
        return refuseOperation<ReportResult>(
            &cwExternalCenterlineManager::attachCompleted, ownerId,
            QStringLiteral("reload: this machine has no source file to copy from"));
    }

    return replace(owner, sourcePath);
}

QFuture<Monad::Result<cwExternalCenterlineAttach::AttachReport>>
cwExternalCenterlineManager::reloadFromSource(cwTrip* trip)
{
    return reloadFromSourceForOwner(trip, QStringLiteral("trip"),
                                    [this](cwTrip* owner, const QString& path) {
        return replaceCenterline(owner, path);
    });
}

QFuture<Monad::Result<cwExternalCenterlineAttach::AttachReport>>
cwExternalCenterlineManager::reloadFromSource(cwSurveyNode* root)
{
    return reloadFromSourceForOwner(root, QStringLiteral("source root"),
                                    [this](cwSurveyNode* owner, const QString& path) {
        return replaceSource(owner, path);
    });
}

cwTrip* cwExternalCenterlineManager::tripForOwner(const QUuid& ownerId) const
{
    if (m_region.isNull()) {
        return nullptr;
    }

    for (cwTrip* trip : m_region->rootNode()->allTrips()) {
        if (trip->id() == ownerId) {
            return trip;
        }
    }
    return nullptr;
}

cwSurveyNode* cwExternalCenterlineManager::nodeForOwner(const QUuid& ownerId) const
{
    if (m_region.isNull()) {
        return nullptr;
    }

    for (cwSurveyNode* node : m_region->rootNode()->allNodes()) {
        if (node->id() == ownerId) {
            return node;
        }
    }
    return nullptr;
}

void cwExternalCenterlineManager::updateFromSource(const QUuid& ownerId)
{
    if (cwTrip* trip = tripForOwner(ownerId)) {
        reloadFromSource(trip);
        return;
    }
    if (cwSurveyNode* root = nodeForOwner(ownerId)) {
        reloadFromSource(root);
    }
}

void cwExternalCenterlineManager::updateAllChangedSources()
{
    for (const cwAttachedCenterlinesModel::Row& attached : std::as_const(m_lastScanRows)) {
        if (m_sourceStatusModel->statusFor(attached.ownerId)
            == cwExternalSourceStatusModel::Status::Changed) {
            updateFromSource(attached.ownerId);
        }
    }
}

QList<cwExternalSourceAttentionRow> cwExternalCenterlineManager::sourcesNeedingAttention() const
{
    using Status = cwExternalSourceStatusModel::Status;

    QList<cwExternalSourceAttentionRow> sources;
    for (const cwAttachedCenterlinesModel::Row& attached : std::as_const(m_lastScanRows)) {
        const auto statusRow = m_sourceStatusModel->rowFor(attached.ownerId);
        if (statusRow.status != Status::Changed && statusRow.status != Status::SourceMissing) {
            continue;
        }

        // A source root stands for its whole subtree, so its row counts the
        // nodes the attachment built and the notes on every trip under it.
        QString ownerName = attached.ownerName;
        int noteCount = 0;
        if (attached.ownerKind == kTripOwnerKind) {
            if (const cwTrip* trip = tripForOwner(attached.ownerId)) {
                noteCount = trip->notes()->rowCount();
            }
        } else if (const cwSurveyNode* root = nodeForOwner(attached.ownerId)) {
            const int nodeCount = root->allNodes().size() + 1;
            if (nodeCount > 1) {
                ownerName = tr("%1 (%n nodes)", nullptr, nodeCount).arg(ownerName);
            }
            for (const cwTrip* trip : root->allTrips()) {
                noteCount += trip->notes()->rowCount();
            }
        }

        sources.append(cwExternalSourceAttentionRow(
            attached.ownerId.toString(QUuid::WithoutBraces),
            ownerName,
            attached.ownerKind,
            attached.caveName,
            statusRow.sourcePath,
            statusRow.sourceRevision,
            statusRow.status,
            noteCount));
    }
    return sources;
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
    return runDetachOperation(trip->id(), QStringLiteral("trip"), m_tripAttachmentDirs,
                              [this, trip]() {
        return cwExternalCenterlineAttach::detach(
            trip, m_saveLoad.data(), m_externalSourceSettings.data());
    });
}
