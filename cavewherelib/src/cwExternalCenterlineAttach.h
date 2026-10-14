/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWEXTERNALCENTERLINEATTACH_H
#define CWEXTERNALCENTERLINEATTACH_H

//Our includes
#include "cwExternalCenterline.h"
#include "cwExternalCenterlineScanner.h"
#include "cwGlobals.h"
#include <Monad/Result.h>

//Qt includes
#include <QFuture>
#include <QPointer>
#include <QString>
#include <QStringList>

//Std includes
#include <atomic>
#include <memory>

class cwSurveyNode;
class cwTrip;
class cwSaveLoad;
class cwExternalSourceSettings;

/**
 * Attach orchestrator: composes the Phase 1 primitives (scanner,
 * reconcile, data-model mutation) plus the cwExternalSourceSettings
 * breadcrumb write into one cancellable, testable entry point.
 * Free functions, no QObject, no member state - see
 * plans/EXTERNAL_FILE_PHASE2.html section 6.
 *
 * Sequencing: scan first (on a worker thread via QtConcurrent), and
 * only on a successful scan mutate the data model and reconcile the
 * dependency closure into the trip's attachment dir. Cancelling the
 * returned future is honored until the scan lands on the main thread;
 * after the filesystem mutation starts the attach runs to completion.
 *
 * Lifetime: continuations are contextualized on `saveLoad`, so if the
 * project retires mid-attach the chain is dropped and the returned
 * future settles as canceled without a result (dropping the last
 * reference to a Deferred cancels it) - observe it with AsyncFuture
 * .context() and never call .result() on a canceled future.
 */
namespace cwExternalCenterlineAttach {

struct AttachReport {
    cwExternalCenterlineScanner::ScanResult scan; //!< Copy of the scanner result
    cwExternalCenterline persisted;               //!< Entry file that landed on the owner
    QStringList warnings;                         //!< Scanner warnings + reconcile plan warnings

    /**
     * The metadata fields actually applied to the trip - each field is
     * seeded only when the file provides it AND the trip's existing
     * value is still the default, so re-attach over user-edited
     * metadata never clobbers (master plan section 8.8 q4/q7). Fields
     * that were skipped stay empty here even when scan.seededMetadata
     * carries them. Empty for a source root.
     */
    cwExternalCenterlineScanner::SeededTripMetadata metadata;

    //! The source root a node-level attach created or refreshed; null for a
    //! trip attach.
    QPointer<cwSurveyNode> root;
};

/**
 * Trip-level attach: scan sourceFile -> set the trip's
 * externalCenterline -> reconcile the closure into
 * saveLoad->externalCenterlineDir(trip) -> seed trip metadata ->
 * remember sourceFile (stored absolute) in externalSourceSettings
 * (always - the in-project copy is the source of truth and sourceFile
 * is where updates come from; see the Phase 2 direction change).
 *
 * Set-model-on-success: the trip's externalCenterline flips only after
 * the reconcile verify passes, so any failure - scan, cancel, or
 * reconcile - leaves the data model and settings exactly as they were.
 * Partial files may remain in the attachment dir and the project stays
 * modified; the next attach or detach cleans them up.
 *
 * cancelFlag is cwExternalCenterlineManager::cancelAttach's seam. It
 * is consulted exactly once, when the scan lands on the main thread:
 * set by then, the returned future settles as canceled and nothing
 * has mutated; set later, it has no effect and the attach runs to
 * completion (the q14 rule, made structural). Unlike cancelling the
 * returned future directly, the flag never settles the future early,
 * so a manager observing it keeps its busy token until the outcome
 * is actually decided.
 */
CAVEWHERE_LIB_EXPORT QFuture<Monad::Result<AttachReport>> attach(
    cwTrip* trip,
    const QString& sourceFile,
    cwSaveLoad* saveLoad,
    cwExternalSourceSettings* externalSourceSettings,
    std::shared_ptr<std::atomic_bool> cancelFlag = nullptr);

/**
 * Trip-level detach: clear the trip's externalCenterline, remove the
 * attachment dir through the cwSaveLoad job queue, and drop the
 * cwExternalSourceSettings breadcrumb. Idempotent - detaching a Native
 * trip only clears any stray breadcrumb and completes Ok without
 * touching the filesystem or the modified bit. If the attachment dir
 * cannot be removed the future completes with an error naming the
 * stranded directory; the model and settings are cleared regardless.
 */
CAVEWHERE_LIB_EXPORT QFuture<Monad::ResultBase> detach(
    cwTrip* trip,
    cwSaveLoad* saveLoad,
    cwExternalSourceSettings* externalSourceSettings);

/**
 * Source attach: scan sourceFile, then create a read-only source root under
 * target (its kind from the scan's root row, a fresh source id, the copied
 * entry file as its external centerline), copy the closure into
 * saveLoad->externalCenterlineDir(root), write units.json beside the copy, add
 * the root's cwExternalSource row, and build the subtree with
 * reconcileSubtree. A lone .dat, .srv, or .svx is a one-node subtree: a
 * CompassFile, WallsBook, or SurvexFile root holding its units.
 *
 * The root exists only from the moment the scan lands; a copy that fails
 * removes it again. AttachReport::root names it on success. cancelFlag works
 * as for the trip overload.
 */
CAVEWHERE_LIB_EXPORT QFuture<Monad::Result<AttachReport>> attach(
    cwSurveyNode* target,
    const QString& sourceFile,
    cwSaveLoad* saveLoad,
    cwExternalSourceSettings* externalSourceSettings,
    std::shared_ptr<std::atomic_bool> cancelFlag = nullptr);

/**
 * Replace and Reload on a source root: re-copy sourceFile's closure into the
 * root's attachment dir (overwriting the copy and removing what the new
 * closure drops), re-write units.json, and reconcile the subtree against the
 * fresh scan. A node with an external centerline and no source record yet
 * becomes a root here.
 */
CAVEWHERE_LIB_EXPORT QFuture<Monad::Result<AttachReport>> replace(
    cwSurveyNode* root,
    const QString& sourceFile,
    cwSaveLoad* saveLoad,
    cwExternalSourceSettings* externalSourceSettings,
    std::shared_ptr<std::atomic_bool> cancelFlag = nullptr);

/**
 * Brings the derived nodes and trips under \a root in line with \a rows, the
 * scan's pre-order tree:
 *  - a node row matches the derived node with its sourcePath, or creates one;
 *  - a unit row matches the derived trip with unitSourcePath(row), or creates
 *    one, its date seeded from the row (a dateless row takes its nearest dated
 *    ancestor's). A node-and-unit row's trip sits under the row's node; a
 *    unit-only row's (a leaf Survex block, a Compass survey, a Walls .srv)
 *    under its parent's node, so a leaf Survex block is one trip;
 *  - a missing file's row is skipped, and every row below it;
 *  - a derived trip no row matches is removed when it holds no native content
 *    (chunks, notes, LiDAR scans, sketches); a derived node no row matches is
 *    removed when its subtree holds none, and otherwise stays and turns native;
 *  - native nodes and trips (no source path) are never touched.
 * New names go through uniqueChildName / uniqueTripName.
 */
CAVEWHERE_LIB_EXPORT void reconcileSubtree(cwSurveyNode* root,
                                           const QList<cwScanRow>& rows);

/**
 * The source path a derived trip carries for a unit row: the row's own path
 * for a unit-only row (a leaf Survex block's trip takes the block's path), and
 * the row's path plus "/#own" for a node that is also a unit (a Survex root,
 * or a Survex block that nests another and has shots of its own), so the
 * trip's path names it apart from its node's - an empty root path included.
 */
CAVEWHERE_LIB_EXPORT QString unitSourcePath(const cwScanRow& row);

/**
 * Removes a source root through the ordinary node delete (removeNode, so
 * nodesDeleted clears the breadcrumbs, and the node's directory Remove job
 * takes the attachment dir inside it), drops its cwExternalSource row and its
 * breadcrumb. The future reports an error when the attachment dir is still on
 * disk once the job queue drains.
 */
CAVEWHERE_LIB_EXPORT QFuture<Monad::ResultBase> deleteSourceRoot(
    cwSurveyNode* root,
    cwSaveLoad* saveLoad,
    cwExternalSourceSettings* externalSourceSettings);

} // namespace cwExternalCenterlineAttach

#endif // CWEXTERNALCENTERLINEATTACH_H
