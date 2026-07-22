/**************************************************************************
**
**    Copyright (C) 2026
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

// Our
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwExternalCenterlineAttach.h"
#include "cwExternalCenterlineManager.h"
#include "cwExternalSourceSettings.h"
#include "cwFutureManagerModel.h"
#include "cwLinePlotManager.h"
#include "cwCavernNaming.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSaveLoad.h"
#include "cwSignalSpy.h"
#include "cwTeam.h"
#include "cwTeamMember.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"
#include "ExternalCenterlineTestHelpers.h"
#include "LoadProjectHelper.h"

// AsyncFuture
#include <asyncfuture.h>

// Qt
#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QThreadPool>

// Std
#include <memory>

namespace {

// Plenty of headroom for the scan worker + save-job queue under
// valgrind / busy CI; the actual attach finishes in milliseconds.
constexpr int kAttachWaitMs = 10000;
// Bounded wait used to prove nothing happens after a cancel - long
// enough for the discarded scan continuation to have landed.
constexpr int kCancelSettleMs = 500;

struct SavedProjectFixture {
    QTemporaryDir tempDir;
    std::unique_ptr<cwRootData> rootData;
    cwProject* project = nullptr;
    cwCave* cave = nullptr;
    cwTrip* trip = nullptr;

    cwSaveLoad* saveLoad() const { return project->saveLoad(); }
    cwExternalSourceSettings* settings() const { return rootData->externalSourceSettings(); }
};

std::unique_ptr<SavedProjectFixture> makeSavedProject(const QString& projectFileBase)
{
    auto fixture = std::make_unique<SavedProjectFixture>();
    REQUIRE(fixture->tempDir.isValid());

    fixture->rootData = std::make_unique<cwRootData>();
    fixture->project = fixture->rootData->project();

    auto region = fixture->project->cavingRegion();
    region->addCave();
    fixture->cave = region->cave(0);
    fixture->cave->setName(QStringLiteral("AttachCave"));
    fixture->cave->addTrip();
    fixture->trip = fixture->cave->trip(0);
    fixture->trip->setName(QStringLiteral("AttachTrip"));

    const QString projectPath =
        QDir(fixture->tempDir.path()).filePath(projectFileBase + QStringLiteral(".cwproj"));
    REQUIRE(fixture->project->saveAs(projectPath));
    fixture->project->waitSaveToFinish();
    // Drain the queued fileSaved delivery so modified() is settled false
    // before tests take a baseline (same idiom as test_cwLazLayerSaveLoad).
    fixture->rootData->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();
    return fixture;
}

QString datasetExternalCenterlinePath(const QString& fileName)
{
    // In-source path (no copy) so the scanner can see sibling fixture
    // files; the attach copies into the project without touching it.
    return testcasesDatasetSourcePath(
        QStringLiteral("external-centerlines/%1").arg(fileName));
}

// Spins the event loop for `ms` wall-clock milliseconds. Used to give
// a discarded continuation every chance to run before asserting that
// it did NOT mutate anything.
void settleEventLoop(int ms)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
}

Monad::Result<cwExternalCenterlineAttach::AttachReport> runAttach(
    SavedProjectFixture* fixture, const QString& sourceFile)
{
    auto future = cwExternalCenterlineAttach::attach(
        fixture->trip, sourceFile, fixture->saveLoad(), fixture->settings());
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    fixture->project->waitSaveToFinish();
    return future.result();
}

} // namespace

TEST_CASE("attach copies the closure, sets the trip centerline, and remembers the source",
          "[Attach][Orchestrator]")
{
    auto fixture = makeSavedProject(QStringLiteral("attach-happy"));
    REQUIRE_FALSE(fixture->project->modified());

    const QString source = datasetExternalCenterlinePath(QStringLiteral("survex_simple.svx"));

    // Set-model-on-success: the model may only flip once, after the
    // copies are verified on disk - there is never a window where the
    // trip reads as attached while the attachment dir is still filling.
    const QDir attachmentDir = fixture->saveLoad()->externalCenterlineDir(fixture->trip);
    int flipCount = 0;
    bool entryOnDiskAtFlip = false;
    QObject::connect(fixture->trip, &cwTrip::externalCenterlineChanged,
                     fixture->trip, [&]() {
        ++flipCount;
        entryOnDiskAtFlip = QFileInfo::exists(
            attachmentDir.absoluteFilePath(QStringLiteral("survex_simple.svx")));
    });

    const auto result = runAttach(fixture.get(), source);

    REQUIRE_FALSE(result.hasError());
    const auto report = result.value();
    CHECK(report.scan.dependencies.size() == 1);
    CHECK(report.persisted.entryFile() == QStringLiteral("survex_simple.svx"));
    CHECK(report.warnings.isEmpty());

    CHECK(fixture->trip->externalCenterline().entryFile()
          == QStringLiteral("survex_simple.svx"));
    CHECK(flipCount == 1);
    CHECK(entryOnDiskAtFlip);

    CHECK(QFileInfo::exists(attachmentDir.absoluteFilePath(QStringLiteral("survex_simple.svx"))));

    // Source memory is always written (direction change: no live-link toggle).
    CHECK(fixture->settings()->sourcePathFor(fixture->trip->id()) == source);

    // Attaching is a project mutation.
    CHECK(fixture->project->modified());
}

TEST_CASE("attach persists the trip centerline through a save/load round trip",
          "[Attach][Orchestrator]")
{
    auto fixture = makeSavedProject(QStringLiteral("attach-roundtrip"));
    const QString projectPath = fixture->project->filename();

    const QString source = datasetExternalCenterlinePath(QStringLiteral("survex_simple.svx"));
    const auto result = runAttach(fixture.get(), source);
    REQUIRE_FALSE(result.hasError());
    fixture->project->waitSaveToFinish();

    auto root2 = std::make_unique<cwRootData>();
    root2->project()->loadFile(projectPath);
    root2->project()->waitLoadToFinish();

    auto region2 = root2->project()->cavingRegion();
    REQUIRE(region2->caveCount() == 1);
    REQUIRE(region2->cave(0)->tripCount() == 1);
    CHECK(region2->cave(0)->trip(0)->externalCenterline().entryFile()
          == QStringLiteral("survex_simple.svx"));
}

TEST_CASE("attach writes the source entry through the shared per-machine store",
          "[Attach][Orchestrator]")
{
    // Commit 2 proved the store persists; this proves production
    // writes actually route through it (the first real writer).
    auto fixture = makeSavedProject(QStringLiteral("attach-store"));

    const QString source = datasetExternalCenterlinePath(QStringLiteral("survex_simple.svx"));
    const auto result = runAttach(fixture.get(), source);
    REQUIRE_FALSE(result.hasError());

    const cwExternalSourceSettings fresh;
    CHECK(fresh.sourcePathFor(fixture->trip->id()) == source);
}

TEST_CASE("attach seeds date, team, and file-owned declination onto a fresh trip",
          "[Attach][Orchestrator]")
{
    auto fixture = makeSavedProject(QStringLiteral("attach-seed"));

    // Re-stamp the default date right before attaching: the seeding
    // predicate compares against QDate::currentDate() at attach time,
    // so a midnight rollover between fixture construction and attach
    // would otherwise read the ctor default as a user-set date.
    fixture->trip->setDate(QDateTime(QDate::currentDate(), QTime()));

    const QString source =
        datasetExternalCenterlinePath(QStringLiteral("survex_with_metadata.svx"));
    const auto result = runAttach(fixture.get(), source);

    REQUIRE_FALSE(result.hasError());
    const auto report = result.value();

    CHECK(fixture->trip->date() == QDateTime(QDate(2025, 6, 1), QTime()));
    REQUIRE(report.metadata.date.has_value());
    CHECK(report.metadata.date.value() == QDate(2025, 6, 1));

    const auto members = fixture->trip->team()->teamMembers();
    REQUIRE(members.size() == 2);
    CHECK(members.at(0).name() == QStringLiteral("Alice"));
    CHECK(members.at(1).name() == QStringLiteral("Bob"));

    // *calibrate declination 7.2 - the file owns it, stored via
    // setImportedDeclination (manual value, auto off).
    CHECK(fixture->trip->calibrations()->declinationManual() == Catch::Approx(7.2));
    CHECK_FALSE(fixture->trip->calibrations()->autoDeclination());
    REQUIRE(report.metadata.declination.has_value());
    CHECK(report.metadata.declination.value() == Catch::Approx(7.2));
}

TEST_CASE("re-attach never clobbers user-edited trip metadata",
          "[Attach][Orchestrator]")
{
    auto fixture = makeSavedProject(QStringLiteral("attach-noclobber"));

    const QDateTime userDate(QDate(2020, 1, 15), QTime());
    fixture->trip->setDate(userDate);
    fixture->trip->team()->addTeamMember(
        cwTeamMember(QStringLiteral("Carol"), QStringList()));
    fixture->trip->calibrations()->setAutoDeclination(false);
    fixture->trip->calibrations()->setDeclinationManual(3.0);
    fixture->project->waitSaveToFinish();

    const QString source =
        datasetExternalCenterlinePath(QStringLiteral("survex_with_metadata.svx"));
    const auto result = runAttach(fixture.get(), source);

    REQUIRE_FALSE(result.hasError());
    const auto report = result.value();

    // The seeded values are silently ignored; report.metadata carries
    // only what was applied - here, nothing.
    CHECK(fixture->trip->date() == userDate);
    REQUIRE(fixture->trip->team()->teamMembers().size() == 1);
    CHECK(fixture->trip->team()->teamMembers().first().name() == QStringLiteral("Carol"));
    CHECK(fixture->trip->calibrations()->declinationManual() == Catch::Approx(3.0));
    CHECK_FALSE(report.metadata.date.has_value());
    CHECK(report.metadata.team.isEmpty());
    CHECK_FALSE(report.metadata.declination.has_value());
    CHECK_FALSE(report.metadata.declinationIsAuto);
}

TEST_CASE("re-attach over an attached trip replaces the entry and GCs the old closure",
          "[Attach][Orchestrator]")
{
    auto fixture = makeSavedProject(QStringLiteral("attach-reattach"));

    const QString first = datasetExternalCenterlinePath(QStringLiteral("survex_simple.svx"));
    REQUIRE_FALSE(runAttach(fixture.get(), first).hasError());

    const QDir attachmentDir = fixture->saveLoad()->externalCenterlineDir(fixture->trip);
    REQUIRE(QFileInfo::exists(
        attachmentDir.absoluteFilePath(QStringLiteral("survex_simple.svx"))));

    const QString second =
        datasetExternalCenterlinePath(QStringLiteral("survex_no_metadata.svx"));
    const auto result = runAttach(fixture.get(), second);

    REQUIRE_FALSE(result.hasError());
    CHECK(fixture->trip->externalCenterline().entryFile()
          == QStringLiteral("survex_no_metadata.svx"));
    CHECK(QFileInfo::exists(
        attachmentDir.absoluteFilePath(QStringLiteral("survex_no_metadata.svx"))));
    // The old closure is garbage-collected by the reconcile.
    CHECK_FALSE(QFileInfo::exists(
        attachmentDir.absoluteFilePath(QStringLiteral("survex_simple.svx"))));
    // Source memory upserted to the new pick.
    CHECK(fixture->settings()->sourcePathFor(fixture->trip->id()) == second);
}

TEST_CASE("failed re-attach leaves the prior attachment model untouched",
          "[Attach][Orchestrator]")
{
#ifdef Q_OS_WIN
    SKIP("Directory permission enforcement is not supported on Windows NTFS");
#endif
    auto fixture = makeSavedProject(QStringLiteral("attach-reattach-fail"));

    const QString first = datasetExternalCenterlinePath(QStringLiteral("survex_simple.svx"));
    REQUIRE_FALSE(runAttach(fixture.get(), first).hasError());

    // Simulate a partial re-attach having GC'd the old closure: remove
    // the prior entry's copy, then make the dir read-only so the next
    // attach's copy fails. The model flips only on success, so even a
    // now-dangling prior attachment is left alone (set-model-on-success,
    // decided 2026-07-19) - a failed re-attach never mutates the trip.
    const QString attachmentDir =
        fixture->saveLoad()->externalCenterlineDir(fixture->trip).absolutePath();
    REQUIRE(QFile::remove(
        QDir(attachmentDir).absoluteFilePath(QStringLiteral("survex_simple.svx"))));
    REQUIRE(QFile::setPermissions(attachmentDir,
                                  QFile::ReadOwner | QFile::ExeOwner));

    const QString second =
        datasetExternalCenterlinePath(QStringLiteral("survex_no_metadata.svx"));
    auto future = cwExternalCenterlineAttach::attach(
        fixture->trip, second, fixture->saveLoad(), fixture->settings());
    const bool finished = AsyncFuture::waitForFinished(future, kAttachWaitMs);
    QFile::setPermissions(attachmentDir,
                          QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    REQUIRE(finished);

    CHECK(future.result().hasError());
    CHECK(fixture->trip->externalCenterline().entryFile()
          == QStringLiteral("survex_simple.svx"));
    // The prior source memory is preserved for a retry.
    CHECK(fixture->settings()->sourcePathFor(fixture->trip->id()) == first);
}

TEST_CASE("attach fails cleanly when the source file does not exist",
          "[Attach][Orchestrator]")
{
    auto fixture = makeSavedProject(QStringLiteral("attach-scan-fail"));
    REQUIRE_FALSE(fixture->project->modified());

    const QString missing =
        QDir(fixture->tempDir.path()).filePath(QStringLiteral("does_not_exist.svx"));
    auto future = cwExternalCenterlineAttach::attach(
        fixture->trip, missing, fixture->saveLoad(), fixture->settings());
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));

    CHECK(future.result().hasError());
    CHECK(fixture->trip->externalCenterline().isEmpty());
    CHECK_FALSE(fixture->saveLoad()->externalCenterlineDir(fixture->trip).exists());
    CHECK(fixture->settings()->sourcePathFor(fixture->trip->id()).isEmpty());
    CHECK_FALSE(fixture->project->modified());
}

TEST_CASE("attach never sets the trip centerline when reconcile cannot write",
          "[Attach][Orchestrator]")
{
#ifdef Q_OS_WIN
    SKIP("Directory permission enforcement is not supported on Windows NTFS");
#endif
    auto fixture = makeSavedProject(QStringLiteral("attach-reconcile-fail"));
    REQUIRE_FALSE(fixture->project->modified());

    // Pre-create the attachment dir read-only so the copy job fails.
    const QString attachmentDir =
        fixture->saveLoad()->externalCenterlineDir(fixture->trip).absolutePath();
    REQUIRE(QDir().mkpath(attachmentDir));
    REQUIRE(QFile::setPermissions(attachmentDir,
                                  QFile::ReadOwner | QFile::ExeOwner));
    const auto restorePermissions = [attachmentDir]() {
        QFile::setPermissions(attachmentDir,
                              QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    };

    const QString source = datasetExternalCenterlinePath(QStringLiteral("survex_simple.svx"));
    auto future = cwExternalCenterlineAttach::attach(
        fixture->trip, source, fixture->saveLoad(), fixture->settings());
    const bool finished = AsyncFuture::waitForFinished(future, kAttachWaitMs);
    restorePermissions();
    REQUIRE(finished);

    CHECK(future.result().hasError());
    // Never set: the model flips only after the reconcile verify
    // passes, so the trip stays Native and no source entry is written.
    CHECK(fixture->trip->externalCenterline().isEmpty());
    CHECK(fixture->settings()->sourcePathFor(fixture->trip->id()).isEmpty());
    // Pinned by the phase 2 plan: a failed attach leaves modified()
    // true - the copy jobs flipped the bit at enqueue and partial
    // files may have hit disk.
    CHECK(fixture->project->modified());
}

TEST_CASE("cancelling the attach before the scan lands leaves everything untouched",
          "[Attach][Orchestrator]")
{
    auto fixture = makeSavedProject(QStringLiteral("attach-cancel"));
    REQUIRE_FALSE(fixture->project->modified());

    const QString source = datasetExternalCenterlinePath(QStringLiteral("survex_simple.svx"));
    auto future = cwExternalCenterlineAttach::attach(
        fixture->trip, source, fixture->saveLoad(), fixture->settings());
    future.cancel();

    // The scan worker finishes regardless; its continuation must see
    // the cancel and drop the result without mutating anything. Wait
    // for the worker pool to drain first so the settle loop provably
    // delivers the discarded continuation (otherwise a slow-CI cold
    // pool start would make the assertions pass vacuously).
    REQUIRE(QThreadPool::globalInstance()->waitForDone(kAttachWaitMs));
    settleEventLoop(kCancelSettleMs);

    CHECK(future.isCanceled());
    CHECK(fixture->trip->externalCenterline().isEmpty());
    CHECK_FALSE(fixture->saveLoad()->externalCenterlineDir(fixture->trip).exists());
    CHECK(fixture->settings()->sourcePathFor(fixture->trip->id()).isEmpty());
    CHECK_FALSE(fixture->project->modified());
}

TEST_CASE("detach clears the centerline, removes the attachment dir, and forgets the source",
          "[Attach][Orchestrator]")
{
    auto fixture = makeSavedProject(QStringLiteral("detach-attached"));

    const QString source = datasetExternalCenterlinePath(QStringLiteral("survex_simple.svx"));
    const auto attachResult = runAttach(fixture.get(), source);
    REQUIRE_FALSE(attachResult.hasError());

    const QString attachmentDir =
        fixture->saveLoad()->externalCenterlineDir(fixture->trip).absolutePath();
    REQUIRE(QDir(attachmentDir).exists());

    auto future = cwExternalCenterlineAttach::detach(
        fixture->trip, fixture->saveLoad(), fixture->settings());
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    fixture->project->waitSaveToFinish();

    REQUIRE_FALSE(future.result().hasError());
    CHECK(fixture->trip->externalCenterline().isEmpty());
    CHECK_FALSE(QDir(attachmentDir).exists());
    CHECK(fixture->settings()->sourcePathFor(fixture->trip->id()).isEmpty());
    CHECK_FALSE(fixture->settings()->hasSource(fixture->trip->id()));

    // Second detach on the now-Native trip: idempotent, still Ok.
    auto again = cwExternalCenterlineAttach::detach(
        fixture->trip, fixture->saveLoad(), fixture->settings());
    REQUIRE(AsyncFuture::waitForFinished(again, kAttachWaitMs));
    CHECK_FALSE(again.result().hasError());
}

TEST_CASE("detach on a Native trip is a no-op that completes Ok",
          "[Attach][Orchestrator]")
{
    auto fixture = makeSavedProject(QStringLiteral("detach-native"));
    REQUIRE_FALSE(fixture->project->modified());

    // Plant a stray source entry: detach on a Native trip must still
    // clear it (the documented contract), even with nothing attached.
    fixture->settings()->setSourcePath(fixture->trip->id(),
                                       QStringLiteral("/stale/orphan.svx"));

    auto future = cwExternalCenterlineAttach::detach(
        fixture->trip, fixture->saveLoad(), fixture->settings());
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));

    REQUIRE_FALSE(future.result().hasError());
    CHECK(fixture->trip->externalCenterline().isEmpty());
    CHECK_FALSE(fixture->settings()->hasSource(fixture->trip->id()));
    CHECK_FALSE(fixture->project->modified());
}

TEST_CASE("attach refuses null inputs with a clear error",
          "[Attach][Orchestrator]")
{
    auto fixture = makeSavedProject(QStringLiteral("attach-guards"));
    const QString source = datasetExternalCenterlinePath(QStringLiteral("survex_simple.svx"));

    {
        auto future = cwExternalCenterlineAttach::attach(
            nullptr, source, fixture->saveLoad(), fixture->settings());
        REQUIRE(future.isFinished());
        CHECK(future.result().hasError());
    }
    {
        auto future = cwExternalCenterlineAttach::attach(
            fixture->trip, source, nullptr, fixture->settings());
        REQUIRE(future.isFinished());
        CHECK(future.result().hasError());
    }
    {
        auto future = cwExternalCenterlineAttach::attach(
            fixture->trip, source, fixture->saveLoad(), nullptr);
        REQUIRE(future.isFinished());
        CHECK(future.result().hasError());
    }
    // A never-saved project used to be refused here as well. It is now
    // a supported case - a temporary project already has a real root
    // dir, and Save As carries the attachment with it. See
    // "[Attach][Temporary]" in test_cwExternalCenterlineAttachTemporary.
}

// ---------------------------------------------------------------------
// cwExternalCenterlineManager operation surface (commit 9): the
// per-owner operation token, the attach/detach wrappers, and the
// saveLoad-derived attachment-dir maps. These run through cwRootData's
// production wiring - the manager under test is the one the QML
// surface binds to.
// ---------------------------------------------------------------------

namespace {

cwExternalCenterlineManager* managerOf(SavedProjectFixture* fixture)
{
    return fixture->rootData->externalCenterlineManager();
}

// Drains the scan-then-solve pipeline plus the save queue so the maps,
// rows, and watch set reflect every queued recompute.
void drainPipelines(SavedProjectFixture* fixture)
{
    fixture->project->waitSaveToFinish();
    fixture->rootData->linePlotManager()->waitToFinish();
    fixture->rootData->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();
}

} // namespace

TEST_CASE("manager attach holds the owner token, refuses re-entry, and derives the dir map",
          "[Attach][Manager]")
{
    auto fixture = makeSavedProject(QStringLiteral("manager-attach"));
    auto manager = managerOf(fixture.get());
    const QString source = datasetExternalCenterlinePath(QStringLiteral("survex_simple.svx"));
    const QUuid ownerId = fixture->trip->id();

    cwSignalSpy busySpy(manager, &cwExternalCenterlineManager::ownerBusyChanged);

    auto future = fixture->rootData->attachTripCenterline(fixture->trip, source);
    CHECK(manager->isOwnerBusy(ownerId));

    // Re-entry while the attach drains is refused without touching the
    // token (no extra ownerBusyChanged emissions).
    auto refused = fixture->rootData->attachTripCenterline(fixture->trip, source);
    REQUIRE(refused.isFinished());
    CHECK(refused.result().hasError());
    CHECK(refused.result().errorMessage().contains(QStringLiteral("in progress")));

    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    REQUIRE_FALSE(future.result().hasError());
    CHECK_FALSE(manager->isOwnerBusy(ownerId));
    CHECK(busySpy.count() == 2); // begin + end; the refused call emitted nothing

    // The recompute snapshot derives the attachment-dir map from the
    // save/load pipeline - the next solve emits the trip's *include.
    drainPipelines(fixture.get());
    const auto inputs = manager->solveInputs();
    REQUIRE(inputs.tripAttachmentDirs.contains(ownerId));
    CHECK(inputs.tripAttachmentDirs.value(ownerId)
          == fixture->saveLoad()->externalCenterlineDir(fixture->trip).absolutePath());
}

TEST_CASE("manager detach drops the settings entry and dir map synchronously",
          "[Attach][Manager]")
{
    auto fixture = makeSavedProject(QStringLiteral("manager-detach"));
    auto manager = managerOf(fixture.get());
    const QString source = datasetExternalCenterlinePath(QStringLiteral("survex_simple.svx"));
    const QUuid ownerId = fixture->trip->id();

    auto attachFuture = manager->attachCenterline(fixture->trip, source);
    REQUIRE(AsyncFuture::waitForFinished(attachFuture, kAttachWaitMs));
    REQUIRE_FALSE(attachFuture.result().hasError());
    drainPipelines(fixture.get());
    REQUIRE(manager->solveInputs().tripAttachmentDirs.contains(ownerId));

    const QString attachmentDir =
        fixture->saveLoad()->externalCenterlineDir(fixture->trip).absolutePath();

    auto detachFuture = manager->detachCenterline(fixture->trip);

    // The queued-invoke hole (commit-7 review): everything a late
    // updateFromSource would consult is already gone, synchronously -
    // before the remove-tree drains.
    CHECK(fixture->settings()->sourcePathFor(ownerId).isEmpty());
    CHECK_FALSE(manager->solveInputs().tripAttachmentDirs.contains(ownerId));
    CHECK(fixture->trip->externalCenterline().isEmpty());
    CHECK(manager->isOwnerBusy(ownerId));

    // A late updateFromSource during the drain is refused by the busy
    // token; after the drain it hits the empty sourcePath/attachmentDir
    // guard. Either way nothing is resurrected.
    manager->updateFromSource(ownerId);

    REQUIRE(AsyncFuture::waitForFinished(detachFuture, kAttachWaitMs));
    REQUIRE_FALSE(detachFuture.result().hasError());
    CHECK_FALSE(manager->isOwnerBusy(ownerId));

    manager->updateFromSource(ownerId);
    drainPipelines(fixture.get());
    CHECK_FALSE(QDir(attachmentDir).exists());
}

TEST_CASE("manager attach and detach complete through the QML report bridge",
          "[Attach][Manager]")
{
    auto fixture = makeSavedProject(QStringLiteral("manager-report"));
    auto manager = managerOf(fixture.get());
    const QString source = datasetExternalCenterlinePath(QStringLiteral("survex_simple.svx"));
    const QUuid ownerId = fixture->trip->id();

    cwSignalSpy attachSpy(manager, &cwExternalCenterlineManager::attachCompleted);
    cwSignalSpy detachSpy(manager, &cwExternalCenterlineManager::detachCompleted);

    auto future = fixture->rootData->attachTripCenterline(fixture->trip, source);

    // The refused re-entry completes through the same bridge, as a
    // failure report. The emission is deferred — never re-entrant
    // inside the call — so nothing has fired yet when the refused
    // future returns already-finished.
    auto refused = fixture->rootData->attachTripCenterline(fixture->trip, source);
    REQUIRE(refused.isFinished());
    CHECK(attachSpy.count() == 0);

    // Draining the first attach delivers both reports in call order:
    // the queued refusal, then the in-flight attach's success.
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    REQUIRE(attachSpy.count() == 2);
    const auto refusedReport =
        attachSpy.takeFirst().at(0).value<cwExternalCenterlineReport>();
    CHECK_FALSE(refusedReport.success());
    CHECK_FALSE(refusedReport.canceled());
    CHECK(refusedReport.ownerId() == ownerId);
    CHECK(refusedReport.errorMessage().contains(QStringLiteral("in progress")));
    const auto attachReport =
        attachSpy.takeFirst().at(0).value<cwExternalCenterlineReport>();
    CHECK(attachReport.success());
    CHECK_FALSE(attachReport.canceled());
    CHECK(attachReport.ownerId() == ownerId);
    CHECK(attachReport.entryFile() == QStringLiteral("survex_simple.svx"));
    CHECK(attachReport.errorMessage().isEmpty());
    // The busy token is already released when the report lands — the
    // signal documents "emitted after the token drops".
    CHECK_FALSE(manager->isOwnerBusy(ownerId));

    auto detachFuture = manager->detachCenterline(fixture->trip);
    REQUIRE(AsyncFuture::waitForFinished(detachFuture, kAttachWaitMs));
    REQUIRE(detachSpy.count() == 1);
    const auto detachReport =
        detachSpy.takeFirst().at(0).value<cwExternalCenterlineReport>();
    CHECK(detachReport.success());
    CHECK(detachReport.ownerId() == ownerId);
    CHECK(detachReport.errorMessage().isEmpty());

    drainPipelines(fixture.get());
}

TEST_CASE("cancelAttach cancels an in-flight attach before the scan lands",
          "[Attach][Manager]")
{
    auto fixture = makeSavedProject(QStringLiteral("manager-cancel-attach"));
    auto manager = managerOf(fixture.get());
    const QString source = datasetExternalCenterlinePath(QStringLiteral("survex_simple.svx"));
    const QUuid ownerId = fixture->trip->id();

    cwSignalSpy attachSpy(manager, &cwExternalCenterlineManager::attachCompleted);

    auto future = manager->attachCenterline(fixture->trip, source);
    // Same stack as the call - the scan continuation cannot have
    // landed yet, so the flag is provably set before its single
    // consult point.
    manager->cancelAttach(ownerId);
    // Unlike cancelling the future, cancelAttach leaves the busy
    // token held until the outcome is decided at scan-landing.
    CHECK(manager->isOwnerBusy(ownerId));

    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    CHECK(future.isCanceled());

    settleEventLoop(kCancelSettleMs);
    REQUIRE(attachSpy.count() == 1);
    const auto report = attachSpy.takeFirst().at(0).value<cwExternalCenterlineReport>();
    CHECK(report.canceled());
    CHECK_FALSE(report.success());
    CHECK(report.ownerId() == ownerId);
    CHECK(report.errorMessage().isEmpty());

    CHECK_FALSE(manager->isOwnerBusy(ownerId));
    CHECK(fixture->trip->externalCenterline().isEmpty());
    CHECK(fixture->settings()->sourcePathFor(ownerId).isEmpty());

    // The owner is immediately reusable - a fresh attach succeeds.
    auto retry = manager->attachCenterline(fixture->trip, source);
    REQUIRE(AsyncFuture::waitForFinished(retry, kAttachWaitMs));
    REQUIRE_FALSE(retry.result().hasError());
    drainPipelines(fixture.get());
}

TEST_CASE("cancelAttach after the scan lands is a structural no-op",
          "[Attach][Manager]")
{
    auto fixture = makeSavedProject(QStringLiteral("manager-cancel-late"));
    auto manager = managerOf(fixture.get());
    const QString source = datasetExternalCenterlinePath(QStringLiteral("survex_simple.svx"));
    const QUuid ownerId = fixture->trip->id();

    cwSignalSpy attachSpy(manager, &cwExternalCenterlineManager::attachCompleted);

    // externalCenterlineChanged fires inside the reconcile continuation
    // - strictly after the cancel flag's single consult point and
    // before the attach future completes. Cancelling from there is the
    // latest deterministic moment: if the flag were ever consulted a
    // second time (cancelling mid-filesystem-write, exactly what the
    // header promises can't happen), this attach would report canceled
    // instead of success.
    QObject::connect(fixture->trip, &cwTrip::externalCenterlineChanged,
                     fixture->trip, [&]() {
        manager->cancelAttach(ownerId);
    });

    auto future = manager->attachCenterline(fixture->trip, source);
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    CHECK_FALSE(future.isCanceled());
    REQUIRE_FALSE(future.result().hasError());

    settleEventLoop(kCancelSettleMs);
    REQUIRE(attachSpy.count() == 1);
    const auto report = attachSpy.takeFirst().at(0).value<cwExternalCenterlineReport>();
    CHECK(report.success());
    CHECK_FALSE(report.canceled());
    CHECK(fixture->trip->externalCenterline().entryFile()
          == QStringLiteral("survex_simple.svx"));
    CHECK_FALSE(manager->isOwnerBusy(ownerId));
    drainPipelines(fixture.get());
}

TEST_CASE("cancelAttach is a no-op for idle owners and non-attach operations",
          "[Attach][Manager]")
{
    auto fixture = makeSavedProject(QStringLiteral("manager-cancel-noop"));
    auto manager = managerOf(fixture.get());
    const QString source = datasetExternalCenterlinePath(QStringLiteral("survex_simple.svx"));
    const QUuid ownerId = fixture->trip->id();

    cwSignalSpy attachSpy(manager, &cwExternalCenterlineManager::attachCompleted);
    cwSignalSpy detachSpy(manager, &cwExternalCenterlineManager::detachCompleted);

    // Idle owner: nothing to cancel, nothing reported.
    manager->cancelAttach(ownerId);

    auto attachFuture = manager->attachCenterline(fixture->trip, source);
    REQUIRE(AsyncFuture::waitForFinished(attachFuture, kAttachWaitMs));
    REQUIRE_FALSE(attachFuture.result().hasError());

    // cancelAttach never touches a detach in flight.
    auto detachFuture = manager->detachCenterline(fixture->trip);
    manager->cancelAttach(ownerId);
    REQUIRE(AsyncFuture::waitForFinished(detachFuture, kAttachWaitMs));
    CHECK_FALSE(detachFuture.isCanceled());
    REQUIRE_FALSE(detachFuture.result().hasError());

    settleEventLoop(kCancelSettleMs);
    CHECK(attachSpy.count() == 1); // only the successful attach reported
    REQUIRE(detachSpy.count() == 1);
    const auto detachReport =
        detachSpy.takeFirst().at(0).value<cwExternalCenterlineReport>();
    CHECK(detachReport.success());
    CHECK_FALSE(detachReport.canceled());
    drainPipelines(fixture.get());
}

TEST_CASE("scopePrefixForTrip derives the driver's qualified-station prefix",
          "[Attach][Manager][ScopeStations]")
{
    auto fixture = makeSavedProject(QStringLiteral("manager-scope-prefix"));
    auto manager = managerOf(fixture.get());

    const QString prefix = manager->scopePrefixForTrip(fixture->trip);
    CHECK(prefix == cwCavernNaming::caveName(fixture->cave->id())
                    + QLatin1Char('.')
                    + cwCavernNaming::tripName(fixture->trip->id())
                    + QLatin1Char('.'));
    // Shape contract independent of the helpers: what
    // cwScopeStationListModel::scopePrefix expects.
    CHECK(QRegularExpression(
              QStringLiteral("^cave_[0-9a-f]{32}\\.trip_[0-9a-f]{32}\\.$"))
              .match(prefix).hasMatch());

    CHECK(manager->scopePrefixForTrip(nullptr).isEmpty());

    // A trip outside any cave has no qualified prefix.
    cwTrip orphan;
    CHECK(manager->scopePrefixForTrip(&orphan).isEmpty());
}

TEST_CASE("attachment dirs derive from the save/load pipeline at load time",
          "[Attach][Manager]")
{
    auto fixture = makeSavedProject(QStringLiteral("manager-load-derive"));
    const QString source = datasetExternalCenterlinePath(QStringLiteral("survex_simple.svx"));
    const auto attachResult = runAttach(fixture.get(), source);
    REQUIRE_FALSE(attachResult.hasError());
    const QString projectPath = fixture->project->filename();
    const QUuid ownerId = fixture->trip->id();
    fixture->rootData->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();

    // A fresh session opening the project: no attach ran here, so the
    // dir map, rows, and watch set can only come from the load-time
    // derivation (insertedCaves -> recompute -> saveLoad-derived dirs).
    auto freshRoot = std::make_unique<cwRootData>();
    freshRoot->project()->loadFile(projectPath);
    freshRoot->project()->waitLoadToFinish();
    auto freshManager = freshRoot->externalCenterlineManager();

    REQUIRE(tryWait(kWatcherWaitMs, [&] {
        QCoreApplication::processEvents(QEventLoop::AllEvents, kInnerPollEventsMs);
        return freshManager->solveInputs().tripAttachmentDirs.contains(ownerId);
    }));
    const QString derivedDir = freshManager->solveInputs().tripAttachmentDirs.value(ownerId);
    CHECK(derivedDir.endsWith(QStringLiteral("external-centerline")));
    CHECK(QFileInfo::exists(QDir(derivedDir).filePath(QStringLiteral("survex_simple.svx"))));

    REQUIRE(tryWait(kWatcherWaitMs, [&] {
        QCoreApplication::processEvents(QEventLoop::AllEvents, kInnerPollEventsMs);
        return freshManager->attachedCenterlinesModel()->rowCount() == 1;
    }));

    freshRoot->linePlotManager()->waitToFinish();
    freshRoot->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();
}
