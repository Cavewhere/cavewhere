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
#include "cwExternalSourceSettings.h"
#include "cwFutureManagerModel.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSaveLoad.h"
#include "cwTeam.h"
#include "cwTeamMember.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"
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
    const auto result = runAttach(fixture.get(), source);

    REQUIRE_FALSE(result.hasError());
    const auto report = result.value();
    CHECK(report.scan.dependencies.size() == 1);
    CHECK(report.persisted.entryFile() == QStringLiteral("survex_simple.svx"));
    CHECK(report.warnings.isEmpty());

    CHECK(fixture->trip->externalCenterline().entryFile()
          == QStringLiteral("survex_simple.svx"));

    const QDir attachmentDir = fixture->saveLoad()->externalCenterlineDir(fixture->trip);
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

TEST_CASE("failed re-attach falls back to Native when the prior entry file is gone",
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
    // attach's copy fails. Rolling back to the prior attachment would
    // persist a dangling entry-file reference, so the orchestrator must
    // fall back to Native.
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
    CHECK(fixture->trip->externalCenterline().isEmpty());
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

TEST_CASE("attach rolls back the trip centerline when reconcile cannot write",
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
    // Rolled back: the trip stays Native and no source entry is written.
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

TEST_CASE("attach refuses null inputs and unsaved projects with a clear error",
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
    {
        // A never-saved project can't receive the copies - the job
        // queue no-ops on temporary projects.
        auto tempRoot = std::make_unique<cwRootData>();
        auto region = tempRoot->project()->cavingRegion();
        region->addCave();
        region->cave(0)->addTrip();
        auto future = cwExternalCenterlineAttach::attach(
            region->cave(0)->trip(0), source,
            tempRoot->project()->saveLoad(),
            tempRoot->externalSourceSettings());
        REQUIRE(future.isFinished());
        REQUIRE(future.result().hasError());
        // Pin which guard fired - a reconcile failure would also
        // produce hasError() but with a confusing message.
        CHECK(future.result().errorMessage().contains(QStringLiteral("save the project")));
    }
}
