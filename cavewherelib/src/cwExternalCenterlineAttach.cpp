/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwExternalCenterlineAttach.h"

//Our includes
#include "cwCave.h"
#include "cwExternalCenterlineSync.h"
#include "cwExternalSourceSettings.h"
#include "cwSaveLoad.h"
#include "cwTeam.h"
#include "cwTeamMember.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"

//AsyncFuture
#include <asyncfuture.h>

//Qt includes
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QtConcurrent>

namespace {

using cwExternalCenterlineAttach::AttachReport;
using cwExternalCenterlineScanner::ScanResult;
using cwExternalCenterlineScanner::SeededTripMetadata;

bool isDefaultTripDate(const QDateTime& date)
{
    // cwTrip's constructor defaults the date to today-at-midnight, so
    // "the user never set it" reads as invalid or still today's default.
    // A trip created on an earlier day and attached later keeps its date.
    return !date.isValid() || date == QDateTime(QDate::currentDate(), QTime());
}

bool isDefaultDeclination(const cwTripCalibration* calibration)
{
    return calibration->autoDeclination() && calibration->declinationManual() == 0.0;
}

/**
 * Applies the scanner's metadata to the trip, field by field, only
 * where the trip's existing value is still the default. Returns the
 * subset that was actually applied.
 */
SeededTripMetadata seedTripMetadata(cwTrip* trip, const SeededTripMetadata& metadata)
{
    SeededTripMetadata applied;

    if (metadata.date.has_value() && isDefaultTripDate(trip->date())) {
        trip->setDate(QDateTime(metadata.date.value(), QTime()));
        applied.date = metadata.date;
    }

    if (!metadata.team.isEmpty() && trip->team()->rowCount() == 0) {
        for (const QString& name : metadata.team) {
            trip->team()->addTeamMember(cwTeamMember(name, QStringList()));
        }
        applied.team = metadata.team;
    }

    cwTripCalibration* calibration = trip->calibrations();
    if (metadata.fileOwnsDeclination() && isDefaultDeclination(calibration)) {
        if (metadata.declination.has_value()) {
            calibration->setImportedDeclination(metadata.declination.value());
            applied.declination = metadata.declination;
        } else {
            // *declination auto: cavern computes IGRF itself, so
            // CaveWhere's own auto machinery stands down (master plan
            // section 8.8 q7).
            calibration->setAutoDeclination(false);
            applied.declinationIsAuto = true;
        }
    }

    return applied;
}

} // namespace

namespace cwExternalCenterlineAttach {

QFuture<Monad::Result<AttachReport>> attach(cwTrip* trip,
                                            const QString& sourceFile,
                                            cwSaveLoad* saveLoad,
                                            cwExternalSourceSettings* externalSourceSettings,
                                            std::shared_ptr<std::atomic_bool> cancelFlag)
{
    using ReportResult = Monad::Result<AttachReport>;

    if (trip == nullptr) {
        return AsyncFuture::completed(
            ReportResult(QStringLiteral("attach: trip is null")));
    }
    if (saveLoad == nullptr) {
        return AsyncFuture::completed(
            ReportResult(QStringLiteral("attach: saveLoad is null")));
    }
    if (externalSourceSettings == nullptr) {
        return AsyncFuture::completed(
            ReportResult(QStringLiteral("attach: externalSourceSettings is null")));
    }
    if (trip->parentCave() == nullptr) {
        return AsyncFuture::completed(
            ReportResult(QStringLiteral("attach: trip is not part of a cave yet")));
    }
    if (saveLoad->isTemporaryProject()) {
        // The reconcile job queue no-ops on unsaved projects, so the
        // copies would silently never land.
        return AsyncFuture::completed(
            ReportResult(QStringLiteral("attach: save the project before attaching an external file")));
    }

    // The Deferred is a cancellation firewall, not just a completion
    // handle. AsyncFuture propagates cancel() UPSTREAM through context
    // chains (execute()'s defer-watch and complete(QFuture)'s
    // pushCancel), so returning the observe(...).context(...) chain
    // directly would let the dialog's Cancel walk through the reconcile
    // stage into cwSaveLoad's shared m_pendingJobsDeferred future and
    // poison the project-wide job-drain state for every other observer.
    // (pendingJobsFinished() now AsyncFuture::shield()s that future as
    // the infrastructure-level backstop; the Deferred additionally keeps
    // attach's own chain from being torn down mid-mutation.)
    // The Deferred's future is connected to nothing upstream; the one
    // window where cancel is honored polls isCanceled() instead.
    AsyncFuture::Deferred<ReportResult> deferred;
    QFuture<ReportResult> resultFuture = deferred.future();

    const QPointer<cwTrip> tripPtr(trip);
    const QPointer<cwSaveLoad> saveLoadPtr(saveLoad);
    const QPointer<cwExternalSourceSettings> settingsPtr(externalSourceSettings);

    auto scanFuture = QtConcurrent::run([sourceFile]() {
        return cwExternalCenterlineScanner::scan(sourceFile);
    });

    AsyncFuture::observe(scanFuture).context(saveLoad,
            [deferred, resultFuture, tripPtr, saveLoadPtr, settingsPtr, sourceFile,
             cancelFlag = std::move(cancelFlag)]
            (const Monad::Result<ScanResult>& scanResult) mutable {
        if (resultFuture.isCanceled()) {
            // Cancelled before the scan landed - nothing has mutated.
            return;
        }
        if (cancelFlag != nullptr && cancelFlag->load()) {
            // cancelAttach landed while the scan ran. This is the
            // flag's single consult point - before any mutation - so
            // honoring it here settles the future as canceled with
            // the data model and filesystem untouched.
            deferred.cancel();
            return;
        }
        if (tripPtr.isNull() || settingsPtr.isNull() || saveLoadPtr.isNull()) {
            deferred.complete(ReportResult(QStringLiteral("attach: owner was deleted mid-attach")));
            return;
        }
        if (scanResult.hasError()) {
            deferred.complete(ReportResult(scanResult.errorMessage(),
                                           scanResult.errorCode()));
            return;
        }
        const ScanResult scan = scanResult.value();
        if (scan.dependencies.isEmpty()) {
            deferred.complete(ReportResult(
                QStringLiteral("attach: scan found no files for %1").arg(sourceFile)));
            return;
        }

        cwTrip* trip = tripPtr.data();
        cwSaveLoad* saveLoad = saveLoadPtr.data();

        // Re-check membership: a trip removed while the scan ran is kept
        // alive by the undo stack with its parentCave still set (see the
        // deliberately-commented setParentCave(nullptr) in cwCave's
        // remove command), so QPointer liveness alone is not enough.
        if (trip->parentCave() == nullptr
            || !trip->parentCave()->trips().contains(trip)) {
            deferred.complete(ReportResult(
                QStringLiteral("attach: trip was removed from its cave mid-attach")));
            return;
        }

        const QString attachmentDir = saveLoad->externalCenterlineDir(trip).absolutePath();

        auto reconcileFuture =
            cwExternalCenterlineSync::reconcile(saveLoad, scan, attachmentDir);

        // Cancellation is deliberately not honored past this point -
        // the filesystem mutation has started, so the attach runs to
        // completion (success or failure).
        AsyncFuture::observe(reconcileFuture).context(saveLoad,
                [deferred, tripPtr, settingsPtr, scan, attachmentDir, sourceFile]
                (const Monad::ResultBase& reconcileResult) mutable {
            if (tripPtr.isNull() || settingsPtr.isNull()) {
                deferred.complete(ReportResult(
                    QStringLiteral("attach: owner was deleted mid-attach")));
                return;
            }
            cwTrip* trip = tripPtr.data();

            // Verify with a fresh plan rather than bare existence: the
            // reconcile future completes Ok even when individual copy
            // jobs failed (errors go to the save-flush channel), and a
            // stale pre-existing destination would pass an exists()
            // check while holding old bytes. An empty copies list means
            // every destination is present AND current.
            const auto verifyPlan = cwExternalCenterlineSync::computePlan(scan, attachmentDir);

            QStringList failures;
            if (reconcileResult.hasError()) {
                failures.append(reconcileResult.errorMessage());
            }
            for (const auto& [copySource, copyDestination] : verifyPlan.copies) {
                failures.append(QStringLiteral("%1 was not copied to %2")
                                    .arg(copySource, copyDestination));
            }

            if (!failures.isEmpty()) {
                // The model was never touched, so a failed attach leaves
                // the trip exactly as it was. Partial files may remain
                // on disk (the next reconcile's GC problem) and the
                // project stays modified - the copy jobs already flipped
                // the bit at enqueue.
                deferred.complete(ReportResult(
                    QStringLiteral("attach: reconcile into the project failed:\n%1")
                        .arg(failures.join(QLatin1Char('\n')))));
                return;
            }

            // Set-model-on-success: flip the model only after the
            // copies are verified on disk, so a crash mid-attach can
            // never persist an attachment whose files were still in
            // flight.
            trip->setExternalCenterline(cwExternalCenterline(
                QFileInfo(scan.dependencies.first()).fileName()));

            AttachReport report;
            report.scan = scan;
            report.persisted = trip->externalCenterline();
            report.warnings = scan.warnings + verifyPlan.warnings;
            report.metadata = seedTripMetadata(trip, scan.seededMetadata);

            settingsPtr->setSourcePath(trip->id(),
                                       QFileInfo(sourceFile).absoluteFilePath());

            deferred.complete(ReportResult(report));
        });
    });

    return resultFuture;
}

QFuture<Monad::ResultBase> detach(cwTrip* trip,
                                  cwSaveLoad* saveLoad,
                                  cwExternalSourceSettings* externalSourceSettings)
{
    using Monad::ResultBase;

    if (trip == nullptr) {
        return AsyncFuture::completed(ResultBase(QStringLiteral("detach: trip is null")));
    }
    if (saveLoad == nullptr) {
        return AsyncFuture::completed(ResultBase(QStringLiteral("detach: saveLoad is null")));
    }
    if (externalSourceSettings == nullptr) {
        return AsyncFuture::completed(
            ResultBase(QStringLiteral("detach: externalSourceSettings is null")));
    }

    externalSourceSettings->clearSourcePath(trip->id());

    if (trip->externalCenterline().isEmpty()) {
        // Native trip: nothing to remove and no mutation to report, so
        // the modified bit stays untouched.
        return AsyncFuture::completed(ResultBase());
    }

    if (trip->parentCave() == nullptr) {
        // No attachment dir can be resolved; just clear the model.
        trip->setExternalCenterline(cwExternalCenterline());
        return AsyncFuture::completed(ResultBase());
    }

    const QString attachmentDir = saveLoad->externalCenterlineDir(trip).absolutePath();
    trip->setExternalCenterline(cwExternalCenterline());
    saveLoad->enqueueExternalCenterlineRemoveTree(attachmentDir);

    // The job queue reports Ok on drain even when the RemoveTree job
    // failed (job errors go to the save-flush channel), so verify the
    // dir is actually gone. The model and settings stay cleared either
    // way - the error only reports the stranded files.
    return AsyncFuture::observe(saveLoad->pendingJobsFinished())
        .context(saveLoad, [attachmentDir]() {
            if (QDir(attachmentDir).exists()) {
                return Monad::ResultBase(
                    QStringLiteral("detach: could not remove the attachment directory %1")
                        .arg(attachmentDir));
            }
            return Monad::ResultBase();
        })
        .future();
}

} // namespace cwExternalCenterlineAttach
