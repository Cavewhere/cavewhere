/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWEXTERNALCENTERLINESYNC_H
#define CWEXTERNALCENTERLINESYNC_H

//Our includes
#include "cwExternalCenterlineScanner.h"
#include "cwGlobals.h"
#include <Monad/Result.h>

//Qt includes
#include <QFuture>
#include <QList>
#include <QString>
#include <QStringList>

//std
#include <utility>

class cwSaveLoad;

/**
 * Reconcile pass that copies an external-centerline dependency
 * closure into the project's attachment dir and garbage-collects
 * files that are no longer reachable from the entry file.
 *
 * The reconcile primitive splits into two layers:
 *
 *   1. computePlan(scan, attachmentDir) is pure-ish (reads the
 *      filesystem to inspect existing destination files but writes
 *      nothing). It walks the scanner's dependency list, decides
 *      which files need a copy (missing, size mismatch, or stale
 *      mtime), and finds files inside attachmentDir that are not
 *      part of the new closure. Tests exercise this layer directly
 *      because they can assert exactly what would be enqueued
 *      without running the cwSaveLoad job queue.
 *
 *   2. reconcile(saveLoad, scan, attachmentDir) wraps the planner.
 *      It calls computePlan, enqueues each copy and remove through
 *      the cwSaveLoad job queue (so all filesystem mutations run on
 *      the project's serialised save thread), and returns a future
 *      that completes when the queue drains.
 *
 * See plans/EXTERNAL_FILE_INTEGRATION_PLAN.html §5.2 and
 * plans/EXTERNAL_FILE_PHASE1.html §14 (commit 6) for the contract.
 */
namespace cwExternalCenterlineSync {

/**
 * What a reconcile does with a destination file that already looks
 * like its source.
 *
 * - SkipUpToDate: keep a destination of the same size whose mtime is
 *   the source's or newer. This is the cheap, idempotent pass.
 * - Overwrite: copy every dependency, so the source's bytes win.
 *   Attach and Replace use this: the user picked a file and expects
 *   its contents in the project, and an edit to the project's copy
 *   that keeps the byte size (a "5.2" turned into "5.3") reads as
 *   up to date under the SkipUpToDate test, which would silently
 *   keep the edit across the swap.
 */
enum class CopyPolicy {
    SkipUpToDate,
    Overwrite
};

/**
 * The set of operations reconcile would submit to cwSaveLoad for a
 * given (ScanResult, attachmentDir) pair. Returned by computePlan
 * and consumed by reconcile.
 *
 * - copies: absolute (source, destination) pairs where the
 *   destination is missing, has a different size, or has an older
 *   mtime than the source. Sources are scanner-canonical paths;
 *   destinations are inside attachmentDir.
 * - removes: absolute paths inside attachmentDir that exist on
 *   disk but are not part of the new dependency closure.
 * - expectedFiles: every dest path that would exist after
 *   reconcile completes (the union of copies' destinations plus
 *   destinations whose source matches and was skipped). Phase 2's
 *   QFileSystemWatcher wiring uses this as its watch set.
 * - warnings: dependency entries that could not be planned (e.g.
 *   a dep whose relative path escapes attachmentDir via ../). The
 *   scan itself was not failed; the dep is simply omitted.
 */
struct ReconcilePlan {
    QList<std::pair<QString, QString>> copies;
    QStringList removes;
    QStringList expectedFiles;
    QStringList warnings;

    bool operator==(const ReconcilePlan& other) const
    {
        return copies == other.copies
            && removes == other.removes
            && expectedFiles == other.expectedFiles
            && warnings == other.warnings;
    }
    bool operator!=(const ReconcilePlan& other) const { return !(*this == other); }
};

/**
 * Builds the reconcile plan for `scan` against `attachmentDir`.
 *
 * The plan layout under attachmentDir mirrors the source layout
 * relative to the entry file's directory (scan.dependencies[0]),
 * so the entry's *include paths still resolve after the copy
 * lands in the project. Dependencies whose relative path escapes
 * attachmentDir (cross-format includes pointing at siblings of
 * the entry's dir) are dropped from the plan with a warning.
 *
 * Under CopyPolicy::SkipUpToDate, existing files inside
 * attachmentDir that match a source by size and have a
 * same-or-newer mtime are kept (no copy enqueued); under
 * CopyPolicy::Overwrite every dependency is planned as a copy.
 * Files inside attachmentDir not part of the closure are slated
 * for removal either way.
 *
 * computePlan does not mutate the filesystem. It returns an empty
 * plan when scan.dependencies is empty (the caller's scan failed
 * upstream).
 */
CAVEWHERE_LIB_EXPORT ReconcilePlan computePlan(
    const cwExternalCenterlineScanner::ScanResult& scan,
    const QString& attachmentDir,
    CopyPolicy copyPolicy = CopyPolicy::SkipUpToDate);

/**
 * Runs the reconcile plan through the cwSaveLoad job queue.
 *
 * Enqueues one copy job per plan.copies entry (an overwriting copy
 * under CopyPolicy::Overwrite, an if-newer copy otherwise) and one
 * removeFile job per plan.removes entry; returns a future that
 * completes when the project's queued jobs drain (matching the
 * existing saveFlush primitive). Temporary / unsaved projects are
 * reconciled like any other - they already have a real root dir, and
 * Save As carries the attachment dir with it.
 *
 * Errors raised by individual filesystem jobs surface through the
 * existing cwSaveLoad error-collection channel; the returned
 * future itself completes with success once the queue drains.
 */
CAVEWHERE_LIB_EXPORT QFuture<Monad::ResultBase> reconcile(
    cwSaveLoad* saveLoad,
    const cwExternalCenterlineScanner::ScanResult& scan,
    const QString& attachmentDir,
    CopyPolicy copyPolicy = CopyPolicy::SkipUpToDate);

/**
 * True when `path` resolves at or inside `boundaryDir`.
 *
 * This is the portability test for an in-project attachment: a
 * dependency that resolves inside the project's data root exists on
 * every machine that has the project, and one that resolves outside it
 * exists only on the machine that authored it. It is a different
 * question from the one computePlan asks — computePlan tests whether a
 * dependency can be *written* under the attachment dir, and works on a
 * relative path — so the two deliberately do not share a predicate.
 *
 * Both sides are canonicalized before comparison. That matters in two
 * directions: a symlinked ancestor cannot be used to step out of the
 * boundary, and a project living under a symlinked path (a QTemporaryDir
 * under macOS's /tmp -> /private/tmp) still matches the scanner's
 * canonical dependency paths. A path that does not exist is compared
 * with its nearest existing ancestor canonicalized, so a missing file
 * still answers the question its location asks.
 *
 * Returns false when either argument is empty.
 */
CAVEWHERE_LIB_EXPORT bool isContainedIn(const QString& path, const QString& boundaryDir);

} // namespace cwExternalCenterlineSync

#endif // CWEXTERNALCENTERLINESYNC_H
