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
#include <QString>
#include <QStringList>

//Std includes
#include <atomic>
#include <memory>

class cwCave;
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

/**
 * One Scope trip the cave-level attach created: a trip that owns no
 * chunks and no file, and whose stations come from filtering the
 * cave's solved network by its stationPrefix.
 */
struct ScopeTripDescription {
    QString name;          //!< The trip's name after uniqueTripName dedup
    QString stationPrefix; //!< Dotted path of the block the trip windows into
};

struct AttachReport {
    cwExternalCenterlineScanner::ScanResult scan; //!< Copy of the scanner result
    cwExternalCenterline persisted;               //!< Entry file that landed on the trip
    QStringList warnings;                         //!< Scanner warnings + reconcile plan warnings

    /**
     * The metadata fields actually applied to the trip - each field is
     * seeded only when the file provides it AND the trip's existing
     * value is still the default, so re-attach over user-edited
     * metadata never clobbers (master plan section 8.8 q4/q7). Fields
     * that were skipped stay empty here even when scan.seededMetadata
     * carries them.
     */
    cwExternalCenterlineScanner::SeededTripMetadata metadata;

    /**
     * The Scope trips a cave-level attach created, in the scan's
     * document order. Empty for a trip attach, and empty for the blocks
     * a replace kept - a trip that already windows a block is left
     * exactly as it is.
     */
    QList<ScopeTripDescription> createdScopeTrips;
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
 * Cave-level attach: the same scan -> reconcile -> verify -> set model
 * pipeline as the trip overload, run against
 * saveLoad->externalCenterlineDir(cave), plus Scope-trip
 * auto-creation - one trip per scanned block that has at least one
 * station of its own, named from the block and carrying the block's
 * dotted path as its stationPrefix
 * (plans/EXTERNAL_FILE_PHASE3.html section 3.4). The created trips are
 * listed in AttachReport::createdScopeTrips.
 *
 * Refuses a cave that already holds trips while owning no attachment:
 * the cave-level verb is for a cave created for this file, and an
 * attached cave's exporter skips the trip loop, so native trips would
 * silently stop reaching the driver. Attaching over a cave that is
 * already attached is a replace - its trips are the Scope trips the
 * previous attach created - and reconciles against the new block set:
 * blocks that still have a trip keep it, new blocks get new trips, and
 * a trip whose block is gone stays in place with an empty station list
 * (section 5 q5).
 *
 * Metadata seeding stays trip-only; a cave has no date, team, or
 * calibration to seed.
 */
CAVEWHERE_LIB_EXPORT QFuture<Monad::Result<AttachReport>> attach(
    cwCave* cave,
    const QString& sourceFile,
    cwSaveLoad* saveLoad,
    cwExternalSourceSettings* externalSourceSettings,
    std::shared_ptr<std::atomic_bool> cancelFlag = nullptr);

/**
 * Cave-level detach: remove every Scope trip the attach created that
 * the user never put chunks in, then clear the cave's
 * externalCenterline, remove the attachment dir, and drop the
 * breadcrumb - the trip detach's tail, with the cascade in front. A
 * Scope trip the user gave chunks to survives, carrying its data into
 * the now-Native cave.
 *
 * The cave itself survives as an empty Native cave with its name
 * (section 5 q7); removing the cave is the separate remove-cave flow.
 */
CAVEWHERE_LIB_EXPORT QFuture<Monad::ResultBase> detach(
    cwCave* cave,
    cwSaveLoad* saveLoad,
    cwExternalSourceSettings* externalSourceSettings);

} // namespace cwExternalCenterlineAttach

#endif // CWEXTERNALCENTERLINEATTACH_H
