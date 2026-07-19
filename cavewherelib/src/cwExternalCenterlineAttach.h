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

class cwTrip;
class cwSaveLoad;
class cwExternalSourceSettings;

/**
 * Attach orchestrator: composes the Phase 1 primitives (scanner,
 * reconcile, data-model mutation) plus the cwExternalSourceSettings
 * source-memory write into one cancellable, testable entry point.
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
 */
CAVEWHERE_LIB_EXPORT QFuture<Monad::Result<AttachReport>> attach(
    cwTrip* trip,
    const QString& sourceFile,
    cwSaveLoad* saveLoad,
    cwExternalSourceSettings* externalSourceSettings);

/**
 * Trip-level detach: clear the trip's externalCenterline, remove the
 * attachment dir through the cwSaveLoad job queue, and drop the
 * cwExternalSourceSettings entry. Idempotent - detaching a Native
 * trip only clears any stray settings entry and completes Ok without
 * touching the filesystem or the modified bit. If the attachment dir
 * cannot be removed the future completes with an error naming the
 * stranded directory; the model and settings are cleared regardless.
 */
CAVEWHERE_LIB_EXPORT QFuture<Monad::ResultBase> detach(
    cwTrip* trip,
    cwSaveLoad* saveLoad,
    cwExternalSourceSettings* externalSourceSettings);

} // namespace cwExternalCenterlineAttach

#endif // CWEXTERNALCENTERLINEATTACH_H
