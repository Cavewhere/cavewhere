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
 * Existing files inside attachmentDir that match a source by size
 * and have a same-or-newer mtime are kept (no copy enqueued).
 * Files inside attachmentDir not part of the closure are slated
 * for removal.
 *
 * computePlan does not mutate the filesystem. It returns an empty
 * plan when scan.dependencies is empty (the caller's scan failed
 * upstream).
 */
CAVEWHERE_LIB_EXPORT ReconcilePlan computePlan(
    const cwExternalCenterlineScanner::ScanResult& scan,
    const QString& attachmentDir);

/**
 * Runs the reconcile plan through the cwSaveLoad job queue.
 *
 * Enqueues one copyIfNewer job per plan.copies entry and one
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
    const QString& attachmentDir);

} // namespace cwExternalCenterlineSync

#endif // CWEXTERNALCENTERLINESYNC_H
