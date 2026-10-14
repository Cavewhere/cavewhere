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
#include "cwExternalSourceModel.h"
#include "cwExternalSourceSettings.h"
#include "cwExternalUnitManifest.h"
#include "cwSaveLoad.h"
#include "cwSurveyNoteLiDARModel.h"
#include "cwSurveyNoteModel.h"
#include "cwSurveyNoteSketchModel.h"
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
#include <QHash>
#include <QList>
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


/**
 * The owner an attach or detach is running for - exactly one of a source root
 * or a trip. Everything the pipeline needs from its owner goes through here,
 * so the scan -> reconcile -> verify -> set-model body is written once and the
 * two owner kinds differ only where they genuinely do: the guards in front,
 * and what the success continuation builds or seeds.
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

    //! Every node below the region's root is a cwCave while the tree migration
    //! runs (cwCave.h), which is the type cwSaveLoad derives a dir from.
    static OwnerTarget forNode(cwSurveyNode* node)
    {
        OwnerTarget owner;
        owner.m_kind = Kind::Node;
        owner.m_node = qobject_cast<cwCave*>(node);
        return owner;
    }

    bool isNode() const { return m_kind == Kind::Node; }

    cwTrip* trip() const { return m_trip.data(); }
    cwCave* node() const { return m_node.data(); }

    bool isAlive() const
    {
        return isNode() ? !m_node.isNull() : !m_trip.isNull();
    }

    QUuid id() const
    {
        return isNode() ? m_node->id() : m_trip->id();
    }

    cwExternalCenterline externalCenterline() const
    {
        return isNode() ? m_node->externalCenterline() : m_trip->externalCenterline();
    }

    void setExternalCenterline(const cwExternalCenterline& centerline) const
    {
        if (isNode()) {
            m_node->setExternalCenterline(centerline);
        } else {
            m_trip->setExternalCenterline(centerline);
        }
    }

    /**
     * True while the owner is still in the data model. A trip removed
     * while the scan ran is kept alive by the undo stack with its
     * parentCave still set (see the deliberately-commented
     * setParentCave(nullptr) in cwCave's remove command), so QPointer
     * liveness alone is not enough; a node answers the same question
     * against its region.
     */
    bool membershipIntact() const
    {
        if (isNode()) {
            const cwCavingRegion* region = m_node->parentRegion();
            return region != nullptr
                   && region->rootNode()->allNodes().contains(m_node.data());
        }
        const cwCave* parentCave = m_trip->parentCave();
        return parentCave != nullptr && parentCave->trips().contains(m_trip.data());
    }

    QString membershipLostError() const
    {
        return isNode()
            ? QStringLiteral("attach: node was removed from its region mid-attach")
            : QStringLiteral("attach: trip was removed from its cave mid-attach");
    }

    //! Where the closure is mirrored - pure path math, no disk I/O.
    QString attachmentDir(cwSaveLoad* saveLoad) const
    {
        return isNode() ? saveLoad->externalCenterlineDir(m_node.data()).absolutePath()
                        : saveLoad->externalCenterlineDir(m_trip.data()).absolutePath();
    }

private:
    enum class Kind { Trip, Node };

    Kind m_kind = Kind::Trip;
    QPointer<cwTrip> m_trip;
    QPointer<cwCave> m_node;
};

/**
 * True when \a trip carries survey data the user entered: chunks, or notes
 * of any kind (scanned pages and the scraps on them, LiDAR scans,
 * sketches). A window holding any of it outlives its block: the source
 * derives the window, and what the user put in it is theirs.
 */
bool holdsNativeContent(const cwTrip* trip)
{
    return trip->chunkCount() > 0
           || trip->notes()->hasNotes()
           || trip->notesLiDAR()->hasNotes()
           || trip->notesSketch()->hasNotes();
}

//! The row of \a sourceId in \a region's source list, or -1.
int sourceRowOf(const cwCavingRegion* region, const QUuid& sourceId)
{
    if (region == nullptr || sourceId.isNull()) {
        return -1;
    }
    return region->externalSources()->indexOf(sourceId);
}

/**
 * Makes \a root the read-only source root its scan describes: a fresh source id
 * when it has none yet, the scan root's kind, and a cwExternalSource row.
 */
void ensureSourceRecord(cwSurveyNode* root, cwSurveyNodeKind::Kind kind)
{
    if (root->sourceId().isNull()) {
        root->setSourceId(QUuid::createUuid());
    }
    root->setSourcePath(QString());
    root->setReadOnly(true);
    root->setKind(kind);

    cwCavingRegion* region = root->parentRegion();
    if (region != nullptr && sourceRowOf(region, root->sourceId()) < 0) {
        region->externalSources()->appendSource(cwExternalSource(root->sourceId(), root->id()));
    }
}

/**
 * True when \a node's subtree holds anything the source does not derive: a
 * native trip, a derived trip holding native content, or a node that came from
 * elsewhere (a native Folder, another source's root).
 */
bool subtreeHoldsNativeContent(const cwSurveyNode* node, const QUuid& sourceId)
{
    bool holds = false;
    node->walk([&holds, &sourceId](const cwSurveyNode* visited) {
        if (visited->sourceId() != sourceId) {
            holds = true;
        }
        const QList<cwTrip*> trips = visited->trips();
        for (const cwTrip* trip : trips) {
            if (trip->sourcePath().isEmpty() || holdsNativeContent(trip)) {
                holds = true;
            }
        }
    });
    return holds;
}

//! Writes units.json for \a rows into \a attachmentDir, as a scan warning on failure.
QStringList writeManifest(const QString& attachmentDir, const QList<cwScanRow>& rows)
{
    const auto written = cwExternalUnitManifest::write(
        QDir(attachmentDir).absoluteFilePath(cwExternalUnitManifest::fileName()), rows);
    if (written.hasError()) {
        return { written.errorMessage() };
    }
    return {};
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
 * dir, verify it, and only then set the model, seed or build what the
 * owner kind asks for, and stamp the breadcrumb. The public overloads run
 * their owner-specific guards and hand the owner in here.
 *
 * A source attach has no owner yet: \a newRootParent names where the scan's
 * root node is created once the scan lands, and a copy that then fails
 * removes it again. Every other verb passes \a existingOwner and a null
 * \a newRootParent.
 */
QFuture<Monad::Result<AttachReport>> attachOwner(
    const OwnerTarget& existingOwner,
    cwSurveyNode* newRootParent,
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
    const QPointer<cwSurveyNode> newRootParentPtr(newRootParent);
    const bool createsRoot = newRootParent != nullptr;

    auto scanFuture = QtConcurrent::run([sourceFile]() {
        return cwExternalCenterlineScanner::scan(sourceFile);
    });

    AsyncFuture::observe(scanFuture).context(saveLoad,
            [deferred, resultFuture, existingOwner, newRootParentPtr, createsRoot,
             saveLoadPtr, settingsPtr, sourceFile, cancelFlag = std::move(cancelFlag)]
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
        const bool ownerGone = createsRoot ? newRootParentPtr.isNull() : !existingOwner.isAlive();
        if (ownerGone || settingsPtr.isNull() || saveLoadPtr.isNull()) {
            deferred.complete(ReportResult(QStringLiteral("attach: owner was deleted mid-attach")));
            return;
        }
        if (scanResult.hasError()) {
            deferred.complete(ReportResult(scanResult.errorMessage(),
                                           scanResult.errorCode()));
            return;
        }
        const ScanResult scan = scanResult.value();
        if (scan.dependencies.isEmpty() || scan.rows.isEmpty()) {
            deferred.complete(ReportResult(
                QStringLiteral("attach: scan found no files for %1").arg(sourceFile)));
            return;
        }

        cwSaveLoad* saveLoad = saveLoadPtr.data();

        OwnerTarget owner = existingOwner;
        if (createsRoot) {
            cwSurveyNode* parent = newRootParentPtr.data();
            if (parent->parentRegion() == nullptr) {
                deferred.complete(ReportResult(
                    QStringLiteral("attach: the target node left its region mid-attach")));
                return;
            }
            // The root takes the scan root's name, deduped among its siblings.
            const cwScanRow& rootRow = scan.rows.first();
            auto* root = new cwCave();
            root->setName(parent->uniqueChildName(rootRow.displayName));
            root->setKind(rootRow.kind);
            root->setReadOnly(true);
            root->setSourceId(QUuid::createUuid());
            parent->addNode(root);
            owner = OwnerTarget::forNode(root);
        } else if (!owner.membershipIntact()) {
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
                [deferred, owner, createsRoot, settingsPtr, scan, attachmentDir, sourceFile]
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
                // the bit at enqueue. A root this attach created goes again,
                // and its directory Remove job takes the partial copies.
                if (createsRoot) {
                    cwSurveyNode* parent = owner.node()->parentNode();
                    parent->removeNode(parent->indexOfNode(owner.node()));
                }
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
            // Replace and Reload rewrite the entry on the record the owner
            // already holds, so the system chosen for its bare fixes stays;
            // a first attach starts from an empty record, with none chosen.
            cwExternalCenterline centerline = owner.externalCenterline();
            centerline.setEntryFile(
                QDir(verifyPlan.baseDir).relativeFilePath(scan.dependencies.first()));
            owner.setExternalCenterline(centerline);

            AttachReport report;
            report.scan = scan;
            report.persisted = owner.externalCenterline();
            // verifyPlan.warnings are all omissions, and any of those failed
            // the attach above, so only the scan's advisories reach here.
            report.warnings = scan.warnings;
            if (owner.isNode()) {
                // Before the future completes, so the manager's recompute
                // and the solve it chains already see the subtree.
                cwSurveyNode* root = owner.node();
                ensureSourceRecord(root, scan.rows.first().kind);
                report.warnings += writeManifest(attachmentDir, scan.rows);
                cwExternalCenterlineAttach::reconcileSubtree(root, scan.rows);
                report.root = root;
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
 * Settles once saveLoad's job queue drains, with an error when \a attachmentDir
 * is still on disk. The queue reports Ok on drain even when a remove job
 * failed (job errors go to the save-flush channel), so the dir's absence is
 * what is checked.
 */
QFuture<Monad::ResultBase> attachmentDirRemoved(const QString& verb,
                                                cwSaveLoad* saveLoad,
                                                const QString& attachmentDir)
{
    return AsyncFuture::observe(saveLoad->pendingJobsFinished())
        .context(saveLoad, [verb, attachmentDir]() {
            if (QDir(attachmentDir).exists()) {
                return Monad::ResultBase(
                    QStringLiteral("%1: could not remove the attachment directory %2")
                        .arg(verb, attachmentDir));
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

    return attachOwner(OwnerTarget::forTrip(trip), nullptr, sourceFile, saveLoad,
                       externalSourceSettings, std::move(cancelFlag));
}

QFuture<Monad::Result<AttachReport>> attach(cwSurveyNode* target,
                                            const QString& sourceFile,
                                            cwSaveLoad* saveLoad,
                                            cwExternalSourceSettings* externalSourceSettings,
                                            std::shared_ptr<std::atomic_bool> cancelFlag)
{
    using ReportResult = Monad::Result<AttachReport>;

    if (target == nullptr) {
        return AsyncFuture::completed(
            ReportResult(QStringLiteral("attach: target node is null")));
    }
    const QString dependencyFailure =
        dependencyError(QStringLiteral("attach"), saveLoad, externalSourceSettings);
    if (!dependencyFailure.isEmpty()) {
        return AsyncFuture::completed(ReportResult(dependencyFailure));
    }
    if (target->parentRegion() == nullptr) {
        return AsyncFuture::completed(
            ReportResult(QStringLiteral("attach: target node is not part of a region yet")));
    }

    return attachOwner(OwnerTarget(), target, sourceFile, saveLoad,
                       externalSourceSettings, std::move(cancelFlag));
}

QFuture<Monad::Result<AttachReport>> replace(cwSurveyNode* root,
                                             const QString& sourceFile,
                                             cwSaveLoad* saveLoad,
                                             cwExternalSourceSettings* externalSourceSettings,
                                             std::shared_ptr<std::atomic_bool> cancelFlag)
{
    using ReportResult = Monad::Result<AttachReport>;

    if (root == nullptr) {
        return AsyncFuture::completed(
            ReportResult(QStringLiteral("replace: source root is null")));
    }
    const QString dependencyFailure =
        dependencyError(QStringLiteral("replace"), saveLoad, externalSourceSettings);
    if (!dependencyFailure.isEmpty()) {
        return AsyncFuture::completed(ReportResult(dependencyFailure));
    }
    if (root->parentRegion() == nullptr || qobject_cast<cwCave*>(root) == nullptr) {
        return AsyncFuture::completed(
            ReportResult(QStringLiteral("replace: source root is not part of a region")));
    }
    if (root->externalCenterline().isEmpty()) {
        return AsyncFuture::completed(
            ReportResult(QStringLiteral("replace: this node has no attached survey file")));
    }

    return attachOwner(OwnerTarget::forNode(root), nullptr, sourceFile, saveLoad,
                       externalSourceSettings, std::move(cancelFlag));
}

QString unitSourcePath(const cwScanRow& row)
{
    if (!row.isNode) {
        return row.sourcePath;
    }
    const QString ownUnit = cwTrip::ownUnitSourcePath();
    return row.sourcePath.isEmpty() ? ownUnit
                                    : row.sourcePath + QLatin1Char('/') + ownUnit;
}

void reconcileSubtree(cwSurveyNode* root, const QList<cwScanRow>& rows)
{
    if (root == nullptr || rows.isEmpty()) {
        return;
    }
    const QUuid sourceId = root->sourceId();

    // Every derived record under the root, by the path that names it in the
    // scan. A match is taken out, so what is left at the end is what the scan
    // no longer lists.
    QHash<QString, cwSurveyNode*> nodesByPath;
    QHash<QString, cwTrip*> tripsByPath;
    const QList<cwSurveyNode*> descendants = root->allNodes();
    for (cwSurveyNode* node : descendants) {
        if (node->sourceId() == sourceId && !node->sourcePath().isEmpty()) {
            nodesByPath.insert(node->sourcePath(), node);
        }
    }
    // A nested source root's derived trips belong to that root's reconcile.
    for (cwTrip* trip : root->allTrips()) {
        if (!trip->sourcePath().isEmpty() && trip->parentNode()->sourceRoot() == root) {
            tripsByPath.insert(trip->sourcePath(), trip);
        }
    }

    // The pre-order walk keeps one live node and one inherited date per depth;
    // a skipped node row skips every row below it.
    QList<cwSurveyNode*> nodeAtDepth;
    QList<QDate> dateAtDepth;
    int skipBelowDepth = -1;

    for (const cwScanRow& row : rows) {
        if (skipBelowDepth >= 0 && row.depth > skipBelowDepth) {
            continue;
        }
        skipBelowDepth = -1;

        const bool isRootRow = row.depth == 0;
        if (!isRootRow && row.missing) {
            if (row.isNode) {
                skipBelowDepth = row.depth;
            }
            continue;
        }

        nodeAtDepth.resize(row.depth + 1);
        dateAtDepth.resize(row.depth + 1);
        cwSurveyNode* parent = isRootRow ? nullptr : nodeAtDepth.at(row.depth - 1);
        const QDate inheritedDate = isRootRow ? QDate() : dateAtDepth.at(row.depth - 1);
        dateAtDepth[row.depth] = row.date.isValid() ? row.date : inheritedDate;

        cwSurveyNode* unitParent = parent;
        if (isRootRow) {
            nodeAtDepth[0] = root;
            unitParent = root;
        } else if (row.isNode) {
            cwSurveyNode* node = nodesByPath.take(row.sourcePath);
            if (node == nullptr) {
                auto* created = new cwCave();
                created->setName(parent->uniqueChildName(row.displayName));
                created->setKind(row.kind);
                created->setReadOnly(true);
                created->setSourceId(sourceId);
                created->setSourcePath(row.sourcePath);
                parent->addNode(created);
                node = created;
            }
            nodeAtDepth[row.depth] = node;
            unitParent = node;
        }

        if (!row.isUnit) {
            continue;
        }
        const QString tripPath = unitSourcePath(row);
        if (tripsByPath.take(tripPath) != nullptr) {
            continue;
        }
        auto* trip = new cwTrip();
        // Seed the survey-entry unit the way cwCave does for a UI-created
        // trip, so chunks the user later adds read in the project's unit.
        trip->calibrations()->setDistanceUnit(cwUnits::surveyUnit(root->unitSystem()));
        trip->setName(unitParent->uniqueTripName(row.displayName));
        trip->setSourcePath(tripPath);
        // Seeding is create-only: a trip that already exists was matched
        // above, so a reload never writes over a date the user set.
        const QDate unitDate = dateAtDepth.at(row.depth);
        if (unitDate.isValid()) {
            SeededTripMetadata metadata;
            metadata.date = unitDate;
            seedTripMetadata(trip, metadata);
        }
        unitParent->addTrip(trip);
    }

    // removeTrip() and removeNode() are the G1 boundary: they emit
    // tripsDeleted()/nodesDeleted(), and with no undo stack they destroy the
    // object outright, so nothing below reads a pointer it removed.
    for (cwTrip* trip : std::as_const(tripsByPath)) {
        if (holdsNativeContent(trip)) {
            continue;
        }
        cwSurveyNode* parent = trip->parentNode();
        parent->removeTrip(parent->indexOf(trip));
    }

    // allNodes() lists a parent before its children, so the reverse walk
    // visits the deepest first, and a node that turns native keeps its
    // ancestors too.
    for (auto it = descendants.crbegin(); it != descendants.crend(); ++it) {
        cwSurveyNode* node = nodesByPath.value((*it)->sourcePath());
        if (node != *it) {
            continue;
        }
        if (subtreeHoldsNativeContent(node, sourceId)) {
            node->setSourceId(QUuid());
            node->setSourcePath(QString());
            node->setReadOnly(false);
            continue;
        }
        cwSurveyNode* parent = node->parentNode();
        parent->removeNode(parent->indexOfNode(node));
    }
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

    externalSourceSettings->clearBreadcrumb(trip->id());

    if (trip->externalCenterline().isEmpty()) {
        // Native trip: nothing to remove and no mutation to report, so the
        // modified bit stays untouched.
        return AsyncFuture::completed(ResultBase());
    }

    if (trip->parentCave() == nullptr) {
        // No attachment dir can be resolved; just clear the model.
        trip->setExternalCenterline(cwExternalCenterline());
        return AsyncFuture::completed(ResultBase());
    }

    // The model and settings stay cleared whatever the remove reports - an
    // error only names the stranded files.
    const QString attachmentDir = saveLoad->externalCenterlineDir(trip).absolutePath();
    trip->setExternalCenterline(cwExternalCenterline());
    saveLoad->enqueueExternalCenterlineRemoveTree(attachmentDir);
    return attachmentDirRemoved(QStringLiteral("detach"), saveLoad, attachmentDir);
}

QFuture<Monad::ResultBase> deleteSourceRoot(cwSurveyNode* root,
                                            cwSaveLoad* saveLoad,
                                            cwExternalSourceSettings* externalSourceSettings)
{
    using Monad::ResultBase;

    if (root == nullptr) {
        return AsyncFuture::completed(ResultBase(QStringLiteral("delete: source root is null")));
    }
    const QString dependencyFailure =
        dependencyError(QStringLiteral("delete"), saveLoad, externalSourceSettings);
    if (!dependencyFailure.isEmpty()) {
        return AsyncFuture::completed(ResultBase(dependencyFailure));
    }
    cwSurveyNode* parent = root->parentNode();
    cwCavingRegion* region = root->parentRegion();
    if (parent == nullptr || region == nullptr || !root->isSourceRoot()) {
        return AsyncFuture::completed(
            ResultBase(QStringLiteral("delete: the node is not a source root in a region")));
    }

    const int sourceRow = sourceRowOf(region, root->sourceId());
    if (sourceRow >= 0) {
        region->externalSources()->removeAt(sourceRow);
    }
    externalSourceSettings->clearBreadcrumb(root->id());

    const QString attachmentDir =
        OwnerTarget::forNode(root).attachmentDir(saveLoad);
    parent->removeNode(parent->indexOfNode(root));

    // The node's directory Remove job takes the attachment dir with it.
    return attachmentDirRemoved(QStringLiteral("delete"), saveLoad, attachmentDir);
}

} // namespace cwExternalCenterlineAttach
