/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwExternalCenterlineAttach.h"

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwExternalCenterlineSync.h"
#include "cwExternalSourceSettings.h"
#include "cwSaveLoad.h"
#include "cwTeam.h"
#include "cwTeamMember.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"
#include "cwUnits.h"

//AsyncFuture
#include <asyncfuture.h>

//Qt includes
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QSet>
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


/**
 * The owner an attach or detach is running for - exactly one of a cave
 * or a trip. Everything the pipeline needs from its owner goes through
 * here, so the scan -> reconcile -> verify -> set-model body is written
 * once and the two owner kinds differ only where they genuinely do:
 * the guards in front, and what the success continuation seeds.
 */
class OwnerTarget
{
public:
    static OwnerTarget forTrip(cwTrip* trip)
    {
        OwnerTarget owner;
        owner.m_kind = Kind::Trip;
        owner.m_trip = trip;
        return owner;
    }

    static OwnerTarget forCave(cwCave* cave)
    {
        OwnerTarget owner;
        owner.m_kind = Kind::Cave;
        owner.m_cave = cave;
        return owner;
    }

    bool isCave() const { return m_kind == Kind::Cave; }

    cwTrip* trip() const { return m_trip.data(); }
    cwCave* cave() const { return m_cave.data(); }

    bool isAlive() const
    {
        return isCave() ? !m_cave.isNull() : !m_trip.isNull();
    }

    QUuid id() const
    {
        return isCave() ? m_cave->id() : m_trip->id();
    }

    cwExternalCenterline externalCenterline() const
    {
        return isCave() ? m_cave->externalCenterline() : m_trip->externalCenterline();
    }

    void setExternalCenterline(const cwExternalCenterline& centerline) const
    {
        if (isCave()) {
            m_cave->setExternalCenterline(centerline);
        } else {
            m_trip->setExternalCenterline(centerline);
        }
    }

    /**
     * True while the owner is still in the data model. A trip removed
     * while the scan ran is kept alive by the undo stack with its
     * parentCave still set (see the deliberately-commented
     * setParentCave(nullptr) in cwCave's remove command), so QPointer
     * liveness alone is not enough; a cave answers the same question
     * against its region.
     */
    bool membershipIntact() const
    {
        if (isCave()) {
            const cwCavingRegion* region = m_cave->parentRegion();
            return region != nullptr && region->caves().contains(m_cave.data());
        }
        const cwCave* parentCave = m_trip->parentCave();
        return parentCave != nullptr && parentCave->trips().contains(m_trip.data());
    }

    QString membershipLostError() const
    {
        return isCave()
            ? QStringLiteral("attach: cave was removed from its region mid-attach")
            : QStringLiteral("attach: trip was removed from its cave mid-attach");
    }

    /**
     * True when the owner sits where an attachment dir can be derived -
     * a trip in a cave, a cave in a region. A detach of an owner that
     * has already left the model clears the data model alone.
     */
    bool hasResolvableAttachmentDir() const
    {
        return isCave() ? m_cave->parentRegion() != nullptr
                        : m_trip->parentCave() != nullptr;
    }

    //! Where the closure is mirrored - pure path math, no disk I/O.
    QString attachmentDir(cwSaveLoad* saveLoad) const
    {
        return isCave() ? saveLoad->externalCenterlineDir(m_cave.data()).absolutePath()
                        : saveLoad->externalCenterlineDir(m_trip.data()).absolutePath();
    }

private:
    enum class Kind { Trip, Cave };

    Kind m_kind = Kind::Trip;
    QPointer<cwTrip> m_trip;
    QPointer<cwCave> m_cave;
};

/**
 * Brings the cave's Scope trips in line with the blocks the scan found:
 * a block that already has a trip windowing it keeps that trip, a block
 * with no trip gets one, and a trip whose block is gone is left alone
 * (its station list simply goes empty). Fresh attach is the degenerate
 * case where no Scope trip exists yet, so attach and replace run the
 * same reconcile. Returns the trips it created, in block order.
 */
QList<cwExternalCenterlineAttach::ScopeTripDescription> reconcileScopeTrips(
    cwCave* cave,
    const QList<cwScanBlock>& blocks)
{
    QSet<QString> windowedPrefixes;
    const QList<cwTrip*> existingTrips = cave->trips();
    for (const cwTrip* trip : existingTrips) {
        if (!trip->stationPrefix().isEmpty()) {
            windowedPrefixes.insert(trip->stationPrefix());
        }
    }

    QList<cwExternalCenterlineAttach::ScopeTripDescription> created;
    for (const cwScanBlock& block : blocks) {
        // A block with no stations of its own has nothing for a trip to
        // window (section 5 q3); the dialog still shows it in the tree.
        if (block.stationCount < 1 || windowedPrefixes.contains(block.path)) {
            continue;
        }

        cwTrip* trip = new cwTrip();
        // Seed the survey-entry unit the way cwCave does for a UI-created
        // trip, so chunks the user later adds to this Scope trip read in
        // the project's unit.
        trip->calibrations()->setDistanceUnit(cwUnits::surveyUnit(cave->unitSystem()));
        // uniqueTripName is consulted per block: blocks that share a leaf
        // name dedupe against the trips this same loop already added.
        trip->setName(cave->uniqueTripName(block.name()));
        trip->setStationPrefix(block.path);
        cave->addTrip(trip);

        windowedPrefixes.insert(block.path);
        created.append({trip->name(), block.path});
    }
    return created;
}

/**
 * Removes the Scope trips a cave-level detach leaves nothing behind
 * for: a trip that windows a block and holds no chunks of its own. A
 * Scope trip the user put chunks in is real survey data and stays.
 *
 * removeTrip() is the boundary on purpose - it emits tripsDeleted(),
 * which is what clears each trip's breadcrumb and wakes the manager.
 * With no undo stack it also destroys the trip outright, so nothing
 * here reads the pointer afterward.
 */
void removeEmptyScopeTrips(cwCave* cave)
{
    for (int i = cave->tripCount() - 1; i >= 0; --i) {
        const cwTrip* trip = cave->trip(i);
        if (!trip->stationPrefix().isEmpty() && trip->chunkCount() == 0) {
            cave->removeTrip(i);
        }
    }
}


/**
 * The dependency guards every verb shares, prefixed with the verb's own
 * name, as an error message or an empty string. Owner-specific guards
 * bracket this one: the null-owner check runs before it, the owner's
 * placement and freshness checks after, so every overload refuses in the
 * same order.
 */
QString dependencyError(const QString& verb,
                        cwSaveLoad* saveLoad,
                        cwExternalSourceSettings* externalSourceSettings)
{
    if (saveLoad == nullptr) {
        return QStringLiteral("%1: saveLoad is null").arg(verb);
    }
    if (externalSourceSettings == nullptr) {
        return QStringLiteral("%1: externalSourceSettings is null").arg(verb);
    }
    return QString();
}

/**
 * The whole attach pipeline, owner-generic: scan on a worker, then on
 * the main thread reconcile the closure into the owner's attachment
 * dir, verify it, and only then set the model, seed what the owner kind
 * asks for, and stamp the breadcrumb. The public overloads run their
 * owner-specific guards and hand the owner in here.
 */
QFuture<Monad::Result<AttachReport>> attachOwner(
    const OwnerTarget& owner,
    const QString& sourceFile,
    cwSaveLoad* saveLoad,
    cwExternalSourceSettings* externalSourceSettings,
    std::shared_ptr<std::atomic_bool> cancelFlag)
{
    using ReportResult = Monad::Result<AttachReport>;

    // Everything under the project's data root is data CaveWhere itself
    // writes, so a "source" there names a copy of itself: the panel would
    // read "Copied from: <the copy>" and the source watcher would treat the
    // project's own writes as upstream changes. Refused before the scan
    // worker starts, so nothing on disk or in the model has moved.
    if (cwExternalCenterlineSync::isContainedIn(sourceFile,
                                                saveLoad->dataRootDir().absolutePath())) {
        return AsyncFuture::completed(ReportResult(
            QStringLiteral("attach: %1 is inside this project's data folder — it is "
                           "CaveWhere's own copy, not a source. Pick the original file "
                           "the data came from instead.")
                .arg(QFileInfo(sourceFile).absoluteFilePath())));
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

    const QPointer<cwSaveLoad> saveLoadPtr(saveLoad);
    const QPointer<cwExternalSourceSettings> settingsPtr(externalSourceSettings);

    auto scanFuture = QtConcurrent::run([sourceFile]() {
        return cwExternalCenterlineScanner::scan(sourceFile);
    });

    AsyncFuture::observe(scanFuture).context(saveLoad,
            [deferred, resultFuture, owner, saveLoadPtr, settingsPtr, sourceFile,
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
        if (!owner.isAlive() || settingsPtr.isNull() || saveLoadPtr.isNull()) {
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

        cwSaveLoad* saveLoad = saveLoadPtr.data();

        if (!owner.membershipIntact()) {
            deferred.complete(ReportResult(owner.membershipLostError()));
            return;
        }

        const QString attachmentDir = owner.attachmentDir(saveLoad);

        // Overwrite: the user picked this file, so its bytes are the ones
        // that belong in the project. Replace runs through here too, where
        // the destination may hold an edit the user made to the project's
        // copy — a same-size edit passes the up-to-date test and would
        // otherwise survive the swap the user just confirmed.
        auto reconcileFuture = cwExternalCenterlineSync::reconcile(
            saveLoad, scan, attachmentDir,
            cwExternalCenterlineSync::CopyPolicy::Overwrite);

        // Cancellation is deliberately not honored past this point -
        // the filesystem mutation has started, so the attach runs to
        // completion (success or failure).
        AsyncFuture::observe(reconcileFuture).context(saveLoad,
                [deferred, owner, settingsPtr, scan, attachmentDir, sourceFile]
                (const Monad::ResultBase& reconcileResult) mutable {
            if (!owner.isAlive() || settingsPtr.isNull()) {
                deferred.complete(ReportResult(
                    QStringLiteral("attach: owner was deleted mid-attach")));
                return;
            }

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
            // A dependency the plan omitted never becomes a pending copy, so
            // the loop above cannot see it. The entry file still *includes
            // it, though, which means the copy that landed references a file
            // nothing brought into the project — an attachment that is
            // broken the moment it is made. Fail rather than persist it.
            failures += verifyPlan.warnings;

            if (!failures.isEmpty()) {
                // The model was never touched, so a failed attach leaves
                // the owner exactly as it was. Partial files may remain
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
            // The copy mirrors the source layout from the plan's base, so
            // the entry is named by its path relative to that same base -
            // a bare filename for the common flat case, a subpath when the
            // closure reached above the entry's own directory. Reusing
            // verifyPlan's base keeps attach and the planner on one answer.
            owner.setExternalCenterline(cwExternalCenterline(
                QDir(verifyPlan.baseDir).relativeFilePath(scan.dependencies.first())));

            AttachReport report;
            report.scan = scan;
            report.persisted = owner.externalCenterline();
            // verifyPlan.warnings are all omissions, and any of those failed
            // the attach above, so only the scan's advisories reach here.
            report.warnings = scan.warnings;
            if (owner.isCave()) {
                // Before the future completes, so the manager's recompute
                // and the solve it chains already see the Scope trips.
                report.createdScopeTrips = reconcileScopeTrips(owner.cave(), scan.blocks);
            } else {
                report.metadata = seedTripMetadata(owner.trip(), scan.seededMetadata);
            }

            // Attach, Replace, and Reload all reach this one stamp, so
            // every copy records the fingerprint of the source's whole
            // dependency set alongside the breadcrumb.
            settingsPtr->setBreadcrumb(
                owner.id(),
                QFileInfo(sourceFile).absoluteFilePath(),
                cwExternalSourceSettings::computeFingerprint(scan.dependencies));

            deferred.complete(ReportResult(report));
        });
    });

    return resultFuture;
}

/**
 * The whole detach, owner-generic: drop the breadcrumb, cascade away a
 * cave's chunk-less Scope trips, clear the model, and remove the
 * attachment dir through the job queue.
 */
QFuture<Monad::ResultBase> detachOwner(const OwnerTarget& owner,
                                       cwSaveLoad* saveLoad,
                                       cwExternalSourceSettings* externalSourceSettings)
{
    using Monad::ResultBase;

    externalSourceSettings->clearBreadcrumb(owner.id());

    if (owner.externalCenterline().isEmpty()) {
        // Native owner: nothing to remove and no mutation to report, so
        // the modified bit stays untouched.
        return AsyncFuture::completed(ResultBase());
    }

    if (owner.isCave()) {
        removeEmptyScopeTrips(owner.cave());
    }

    if (!owner.hasResolvableAttachmentDir()) {
        // No attachment dir can be resolved; just clear the model.
        owner.setExternalCenterline(cwExternalCenterline());
        return AsyncFuture::completed(ResultBase());
    }

    const QString attachmentDir = owner.attachmentDir(saveLoad);
    owner.setExternalCenterline(cwExternalCenterline());
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
    const QString dependencyFailure =
        dependencyError(QStringLiteral("attach"), saveLoad, externalSourceSettings);
    if (!dependencyFailure.isEmpty()) {
        return AsyncFuture::completed(ReportResult(dependencyFailure));
    }
    if (trip->parentCave() == nullptr) {
        return AsyncFuture::completed(
            ReportResult(QStringLiteral("attach: trip is not part of a cave yet")));
    }

    return attachOwner(OwnerTarget::forTrip(trip), sourceFile, saveLoad,
                       externalSourceSettings, std::move(cancelFlag));
}

QFuture<Monad::Result<AttachReport>> attach(cwCave* cave,
                                            const QString& sourceFile,
                                            cwSaveLoad* saveLoad,
                                            cwExternalSourceSettings* externalSourceSettings,
                                            std::shared_ptr<std::atomic_bool> cancelFlag)
{
    using ReportResult = Monad::Result<AttachReport>;

    if (cave == nullptr) {
        return AsyncFuture::completed(
            ReportResult(QStringLiteral("attach: cave is null")));
    }
    const QString dependencyFailure =
        dependencyError(QStringLiteral("attach"), saveLoad, externalSourceSettings);
    if (!dependencyFailure.isEmpty()) {
        return AsyncFuture::completed(ReportResult(dependencyFailure));
    }
    if (cave->parentRegion() == nullptr) {
        return AsyncFuture::completed(
            ReportResult(QStringLiteral("attach: cave is not part of a region yet")));
    }

    // Fresh-cave guard (section 3.3): the exporter skips the trip loop for
    // an attached cave, so native trips under one would silently stop
    // reaching the driver. A cave that is already attached passes - its
    // trips are the Scope trips the previous attach created, and this call
    // is the replace that reconciles them.
    if (cave->hasTrips() && cave->externalCenterline().isEmpty()) {
        return AsyncFuture::completed(ReportResult(
            QStringLiteral("attach: cave already has trips — cave-level attach is "
                           "for a new cave")));
    }

    return attachOwner(OwnerTarget::forCave(cave), sourceFile, saveLoad,
                       externalSourceSettings, std::move(cancelFlag));
}

QFuture<Monad::ResultBase> detach(cwTrip* trip,
                                  cwSaveLoad* saveLoad,
                                  cwExternalSourceSettings* externalSourceSettings)
{
    using Monad::ResultBase;

    if (trip == nullptr) {
        return AsyncFuture::completed(ResultBase(QStringLiteral("detach: trip is null")));
    }
    const QString dependencyFailure =
        dependencyError(QStringLiteral("detach"), saveLoad, externalSourceSettings);
    if (!dependencyFailure.isEmpty()) {
        return AsyncFuture::completed(ResultBase(dependencyFailure));
    }

    return detachOwner(OwnerTarget::forTrip(trip), saveLoad, externalSourceSettings);
}

QFuture<Monad::ResultBase> detach(cwCave* cave,
                                  cwSaveLoad* saveLoad,
                                  cwExternalSourceSettings* externalSourceSettings)
{
    using Monad::ResultBase;

    if (cave == nullptr) {
        return AsyncFuture::completed(ResultBase(QStringLiteral("detach: cave is null")));
    }
    const QString dependencyFailure =
        dependencyError(QStringLiteral("detach"), saveLoad, externalSourceSettings);
    if (!dependencyFailure.isEmpty()) {
        return AsyncFuture::completed(ResultBase(dependencyFailure));
    }

    return detachOwner(OwnerTarget::forCave(cave), saveLoad, externalSourceSettings);
}

} // namespace cwExternalCenterlineAttach
