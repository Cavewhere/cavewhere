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
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSaveLoad.h"
#include "cwSignalSpy.h"
#include "cwStationPositionLookup.h"
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
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QThreadPool>

// Std
#include <memory>

namespace {

std::unique_ptr<SavedProjectFixture> makeNewProject()
{
    return makeNewProject(QStringLiteral("AttachCave"), QStringLiteral("AttachTrip"));
}

std::unique_ptr<SavedProjectFixture> makeSavedProject(
    const QString& projectFileBase,
    const QString& extension = QStringLiteral(".cwproj"))
{
    return makeSavedProject(projectFileBase,
                            QStringLiteral("AttachCave"), QStringLiteral("AttachTrip"),
                            extension);
}

QString datasetExternalCenterlinePath(const QString& fileName)
{
    // In-source path (no copy) so the scanner can see sibling fixture
    // files; the attach copies into the project without touching it.
    return testcasesDatasetSourcePath(
        QStringLiteral("external-centerlines/%1").arg(fileName));
}

// Wide margin so a hand-planted edit reads as newer than the source
// whatever the filesystem's mtime granularity.
constexpr int kEditIsNewerSeconds = 3600;

QByteArray readWholeFile(const QString& path)
{
    QFile file(path);
    REQUIRE(file.open(QFile::ReadOnly));
    return file.readAll();
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
    CHECK(fixture->settings()->breadcrumbPath(fixture->trip->id()) == source);

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
    CHECK(fresh.breadcrumbPath(fixture->trip->id()) == source);
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
    CHECK(fixture->settings()->breadcrumbPath(fixture->trip->id()) == second);
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
    CHECK(fixture->settings()->breadcrumbPath(fixture->trip->id()) == first);
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
    CHECK(fixture->settings()->breadcrumbPath(fixture->trip->id()).isEmpty());
    CHECK_FALSE(fixture->project->modified());
}

TEST_CASE("attach refuses a source whose *include cannot be mirrored under the attachment",
          "[Attach][Orchestrator]")
{
    auto fixture = makeSavedProject(QStringLiteral("attach-omitted-dep"));

    // The entry sits one level down from the file it includes, so the
    // dependency's path relative to the entry's directory climbs out of it
    // — computePlan cannot place it under the attachment dir and omits it.
    const QString innerDir = QDir(fixture->tempDir.path()).filePath(QStringLiteral("src/inner"));
    REQUIRE(QDir().mkpath(innerDir));
    const QString sharedPath =
        QDir(fixture->tempDir.path()).filePath(QStringLiteral("src/shared.svx"));
    overwriteFile(sharedPath, QByteArrayLiteral("*begin Shared\n*end Shared\n"));

    const QString entryPath = QDir(innerDir).filePath(QStringLiteral("entry.svx"));
    overwriteFile(entryPath,
                  QByteArrayLiteral("*begin Entry\n*include \"../shared.svx\"\n*end Entry\n"));

    const auto result = runAttach(fixture.get(), entryPath);

    // Copying only part of the closure would persist an attachment whose
    // entry file *includes a file nothing brought into the project — broken
    // the moment it is made. Refusing is the whole point of the promotion.
    REQUIRE(result.hasError());
    CHECK(result.errorMessage().contains(QStringLiteral("shared.svx")));

    // The model is untouched, so the trip is exactly as it was.
    CHECK(fixture->trip->externalCenterline().isEmpty());
    CHECK(fixture->settings()->breadcrumbPath(fixture->trip->id()).isEmpty());
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
    CHECK(fixture->settings()->breadcrumbPath(fixture->trip->id()).isEmpty());
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
    settleEventLoop(kNothingHappensSettleMs);

    CHECK(future.isCanceled());
    CHECK(fixture->trip->externalCenterline().isEmpty());
    CHECK_FALSE(fixture->saveLoad()->externalCenterlineDir(fixture->trip).exists());
    CHECK(fixture->settings()->breadcrumbPath(fixture->trip->id()).isEmpty());
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
    CHECK(fixture->settings()->breadcrumbPath(fixture->trip->id()).isEmpty());
    CHECK_FALSE(fixture->settings()->hasBreadcrumb(fixture->trip->id()));

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
    fixture->settings()->setBreadcrumbPath(fixture->trip->id(),
                                           QStringLiteral("/stale/orphan.svx"));

    auto future = cwExternalCenterlineAttach::detach(
        fixture->trip, fixture->saveLoad(), fixture->settings());
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));

    REQUIRE_FALSE(future.result().hasError());
    CHECK(fixture->trip->externalCenterline().isEmpty());
    CHECK_FALSE(fixture->settings()->hasBreadcrumb(fixture->trip->id()));
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

// nameTripFromFileAndNavigate's naming step: the entry file's base name,
// deduped against the cave.
void nameTripFromEntryFile(SavedProjectFixture* fixture)
{
    const QString baseName =
        QFileInfo(fixture->trip->externalCenterline().entryFile()).completeBaseName();
    fixture->trip->setName(fixture->cave->uniqueTripName(baseName));
}

// The copy is the only file the project reads, so "attached" means: the
// entry file sits where externalCenterlineDir points, the manager reports
// no missing copy, and the solve placed the station the file fixes.
void checkAttachmentIsReadable(SavedProjectFixture* fixture)
{
    auto manager = managerOf(fixture);
    const QDir attachmentDir = fixture->saveLoad()->externalCenterlineDir(fixture->trip);

    INFO("attachment dir: " << attachmentDir.absolutePath().toStdString());
    INFO("missing copy: " << manager->missingCopyPath(fixture->trip->id()).toStdString());
    CHECK(QFileInfo::exists(
        attachmentDir.absoluteFilePath(QStringLiteral("survex_simple.svx"))));
    CHECK(manager->missingCopyPath(fixture->trip->id()).isEmpty());
    CHECK_FALSE(manager->solveInputs().excludedExternalOwners.contains(fixture->trip->id()));

    // survex_simple.svx opens "*begin Simple" and fixes a1, so the station
    // lands under the trip's own scope. Without the *include the driver
    // emits nothing for this trip and the station never appears.
    INFO("solve error: "
         << fixture->rootData->linePlotManager()->solveErrorMessage().toStdString());
    CHECK_FALSE(fixture->rootData->linePlotManager()->hasSolveError());
    CHECK(fixture->cave->stationPositionLookup().hasPosition(
        tripScopeLabel(fixture->trip) + QStringLiteral(".simple.a1")));
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

TEST_CASE("manager attach solves a Compass .dat whose station names contain dots",
          "[Attach][Manager][Compass]")
{
    // B6 regression (plans/EXTERNAL_FILE_PHASE2.html section 16). A '.' inside
    // a station name flips cavern's .3d label separator to ':'
    // (find_output_separator in survex/src/commands.c). Before
    // cwSurvex3DFileReader normalized pimg->separator back to '.', the decode
    // split those labels on '.', matched no cave scope, and dropped every
    // station: the attach reported success and cavern solved cleanly, but the
    // trip had no solved stations and nothing drew. Compass permits dotted
    // names, so any real-world .dat like the B6 repro hit this; a plain-named
    // .dat kept '.' as separator and passed even without the fix.
    auto fixture = makeSavedProject(QStringLiteral("manager-attach-compass"));
    QTemporaryDir spacedDir;
    QString source = datasetExternalCenterlinePath(QStringLiteral("compass_dotted.dat"));

    SECTION("straight from the fixture") {
    }
    SECTION("through a copy whose filename contains spaces") {
        REQUIRE(spacedDir.isValid());
        const QString spaced = QDir(spacedDir.path())
            .absoluteFilePath(QStringLiteral("compass dotted - Copy.dat"));
        REQUIRE(QFile::copy(source, spaced));
        source = spaced;
    }

    attachThroughManager(fixture.get(), fixture->trip, source);
    drainPipelines(fixture.get());

    INFO("solve error: "
         << fixture->rootData->linePlotManager()->solveErrorMessage().toStdString());
    CHECK_FALSE(fixture->rootData->linePlotManager()->hasSolveError());

    // Both surveys of the multi-survey .dat land under the trip's scope,
    // dotted names intact.
    const cwStationPositionLookup lookup = fixture->cave->stationPositionLookup();
    const QString scope = tripScopeLabel(fixture->trip);
    CHECK(lookup.hasPosition(scope + QStringLiteral(".s1")));
    CHECK(lookup.hasPosition(scope + QStringLiteral(".sa1.1")));
    CHECK(lookup.hasPosition(scope + QStringLiteral(".sa1.2")));
    CHECK(lookup.hasPosition(scope + QStringLiteral(".sb2.1")));
}

TEST_CASE("manager detach drops the settings entry and dir map synchronously",
          "[Attach][Manager]")
{
    auto fixture = makeSavedProject(QStringLiteral("manager-detach"));
    auto manager = managerOf(fixture.get());
    const QString source = datasetExternalCenterlinePath(QStringLiteral("survex_simple.svx"));
    const QUuid ownerId = fixture->trip->id();

    attachThroughManager(fixture.get(), fixture->trip, source);
    drainPipelines(fixture.get());
    REQUIRE(manager->solveInputs().tripAttachmentDirs.contains(ownerId));

    const QString attachmentDir =
        fixture->saveLoad()->externalCenterlineDir(fixture->trip).absolutePath();

    auto detachFuture = manager->detachCenterline(fixture->trip);

    // The queued-invoke hole (commit-7 review): everything a caller queued
    // behind the detach would consult is already gone, synchronously -
    // before the remove-tree drains. The busy token covers the rest of the
    // drain, so there is no instant at which the owner looks attachable
    // while its files are still being removed.
    CHECK(fixture->settings()->breadcrumbPath(ownerId).isEmpty());
    CHECK_FALSE(manager->solveInputs().tripAttachmentDirs.contains(ownerId));
    CHECK(fixture->trip->externalCenterline().isEmpty());
    CHECK(manager->isOwnerBusy(ownerId));

    REQUIRE(AsyncFuture::waitForFinished(detachFuture, kAttachWaitMs));
    REQUIRE_FALSE(detachFuture.result().hasError());
    CHECK_FALSE(manager->isOwnerBusy(ownerId));

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

    settleEventLoop(kNothingHappensSettleMs);
    REQUIRE(attachSpy.count() == 1);
    const auto report = attachSpy.takeFirst().at(0).value<cwExternalCenterlineReport>();
    CHECK(report.canceled());
    CHECK_FALSE(report.success());
    CHECK(report.ownerId() == ownerId);
    CHECK(report.errorMessage().isEmpty());

    CHECK_FALSE(manager->isOwnerBusy(ownerId));
    CHECK(fixture->trip->externalCenterline().isEmpty());
    CHECK(fixture->settings()->breadcrumbPath(ownerId).isEmpty());

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

    settleEventLoop(kNothingHappensSettleMs);
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

    attachThroughManager(fixture.get(), fixture->trip, source);

    // cancelAttach never touches a detach in flight.
    auto detachFuture = manager->detachCenterline(fixture->trip);
    manager->cancelAttach(ownerId);
    REQUIRE(AsyncFuture::waitForFinished(detachFuture, kAttachWaitMs));
    CHECK_FALSE(detachFuture.isCanceled());
    REQUIRE_FALSE(detachFuture.result().hasError());

    settleEventLoop(kNothingHappensSettleMs);
    CHECK(attachSpy.count() == 1); // only the successful attach reported
    REQUIRE(detachSpy.count() == 1);
    const auto detachReport =
        detachSpy.takeFirst().at(0).value<cwExternalCenterlineReport>();
    CHECK(detachReport.success());
    CHECK_FALSE(detachReport.canceled());
    drainPipelines(fixture.get());
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

// ---------------------------------------------------------------------
// Replace verb (plans/EXTERNAL_FILE_LIVE_LINK_RETIREMENT.html commit 1):
// point an attached trip at a freshly picked file in one operation.
// ---------------------------------------------------------------------

TEST_CASE("replace swaps the closure, GCs the dropped deps, and re-solves",
          "[Attach][Replace]")
{
    // Both project formats: the bundled .cw keeps its tree extracted in a
    // temporary directory, so the attachment dir the replace reconciles
    // into is a different place than the .cwproj's on-disk tree.
    QString extension;
    SECTION("git-backed .cwproj") { extension = QStringLiteral(".cwproj"); }
    SECTION("bundled .cw") { extension = QStringLiteral(".cw"); }

    auto fixture = makeSavedProject(QStringLiteral("replace-swap"), extension);
    auto manager = managerOf(fixture.get());
    const QUuid ownerId = fixture->trip->id();

    // Three files deep: survex_nested.svx -> entrance.svx -> passage.svx.
    const QString nested = datasetExternalCenterlinePath(QStringLiteral("survex_nested.svx"));
    attachThroughManager(fixture.get(), fixture->trip, nested);
    drainPipelines(fixture.get());

    const QDir attachmentDir = fixture->saveLoad()->externalCenterlineDir(fixture->trip);
    REQUIRE(QFileInfo::exists(attachmentDir.absoluteFilePath(QStringLiteral("entrance.svx"))));
    REQUIRE(QFileInfo::exists(attachmentDir.absoluteFilePath(QStringLiteral("passage.svx"))));

    cwSignalSpy solveSpy(manager, &cwExternalCenterlineManager::solveNeeded);
    cwSignalSpy attachSpy(manager, &cwExternalCenterlineManager::attachCompleted);

    // The new pick references neither include, so both are the GC's job.
    const QString simple = datasetExternalCenterlinePath(QStringLiteral("survex_simple.svx"));
    auto replaceFuture = manager->replaceCenterline(fixture->trip, simple);
    REQUIRE(AsyncFuture::waitForFinished(replaceFuture, kAttachWaitMs));
    REQUIRE_FALSE(replaceFuture.result().hasError());
    drainPipelines(fixture.get());

    CHECK(fixture->trip->externalCenterline().entryFile()
          == QStringLiteral("survex_simple.svx"));
    CHECK(QFileInfo::exists(attachmentDir.absoluteFilePath(QStringLiteral("survex_simple.svx"))));
    CHECK_FALSE(QFileInfo::exists(
        attachmentDir.absoluteFilePath(QStringLiteral("survex_nested.svx"))));
    CHECK_FALSE(QFileInfo::exists(
        attachmentDir.absoluteFilePath(QStringLiteral("entrance.svx"))));
    CHECK_FALSE(QFileInfo::exists(
        attachmentDir.absoluteFilePath(QStringLiteral("passage.svx"))));

    // The pick is remembered like any other, and the plot is re-solved
    // off the new bytes rather than waiting for the next edit.
    CHECK(fixture->settings()->breadcrumbPath(ownerId) == simple);
    CHECK(solveSpy.count() > 0);

    // One report, through the attach bridge - a replace is an attach over
    // an occupied owner as far as a dialog is concerned.
    REQUIRE(attachSpy.count() == 1);
    const auto report = attachSpy.at(0).at(0).value<cwExternalCenterlineReport>();
    CHECK(report.success());
    CHECK(report.entryFile() == QStringLiteral("survex_simple.svx"));
    CHECK(report.ownerId() == ownerId);
}

TEST_CASE("replace overwrites an edit to the project copy that kept its size",
          "[Attach][Replace]")
{
    // ReplaceCenterlineDialog promises the current copies, edits included,
    // are replaced. An edit that keeps the byte count and moves the mtime
    // forward is exactly the shape the reconcile's up-to-date test reads as
    // already current, so it is the case that promise stands or falls on.
    auto fixture = makeSavedProject(QStringLiteral("replace-overwrites-edit"));
    auto manager = managerOf(fixture.get());

    const QString simple = datasetExternalCenterlinePath(QStringLiteral("survex_simple.svx"));
    attachThroughManager(fixture.get(), fixture->trip, simple);
    drainPipelines(fixture.get());

    const QDir attachmentDir = fixture->saveLoad()->externalCenterlineDir(fixture->trip);
    const QString copyPath =
        attachmentDir.absoluteFilePath(QStringLiteral("survex_simple.svx"));

    const QByteArray sourceBytes = readWholeFile(simple);
    QByteArray editedBytes = sourceBytes;
    editedBytes.replace(QByteArray("A2 A3 8.5"), QByteArray("A2 A3 9.5"));
    REQUIRE(editedBytes.size() == sourceBytes.size());
    REQUIRE(editedBytes != sourceBytes);

    writeFileWithMtime(copyPath, editedBytes,
                       QDateTime::currentDateTimeUtc().addSecs(kEditIsNewerSeconds));
    REQUIRE(readWholeFile(copyPath) == editedBytes);

    auto replaceFuture = manager->replaceCenterline(fixture->trip, simple);
    REQUIRE(AsyncFuture::waitForFinished(replaceFuture, kAttachWaitMs));
    REQUIRE_FALSE(replaceFuture.result().hasError());
    drainPipelines(fixture.get());

    CHECK(readWholeFile(copyPath) == sourceBytes);
}

TEST_CASE("replace refuses a trip with nothing attached", "[Attach][Replace]")
{
    auto fixture = makeSavedProject(QStringLiteral("replace-unattached"));
    auto manager = managerOf(fixture.get());
    const QString source = datasetExternalCenterlinePath(QStringLiteral("survex_simple.svx"));

    cwSignalSpy attachSpy(manager, &cwExternalCenterlineManager::attachCompleted);

    auto future = manager->replaceCenterline(fixture->trip, source);
    REQUIRE(future.isFinished());
    CHECK(future.result().hasError());
    CHECK(future.result().errorMessage().contains(QStringLiteral("no attached centerline")));

    // Nothing was attached on the way out: replace is a swap, not a
    // back-door attach.
    CHECK(fixture->trip->externalCenterline().isEmpty());
    CHECK_FALSE(manager->isOwnerBusy(fixture->trip->id()));

    // The refusal reports through the bridge, deferred like every other
    // synchronous refusal.
    CHECK(attachSpy.count() == 0);
    REQUIRE(attachSpy.wait(kAttachWaitMs));
    REQUIRE(attachSpy.count() == 1);
    const auto report = attachSpy.at(0).at(0).value<cwExternalCenterlineReport>();
    CHECK_FALSE(report.success());
    CHECK(report.errorMessage().contains(QStringLiteral("no attached centerline")));

    drainPipelines(fixture.get());
}

TEST_CASE("replace refuses a busy owner without disturbing the attachment",
          "[Attach][Replace]")
{
    auto fixture = makeSavedProject(QStringLiteral("replace-busy"));
    auto manager = managerOf(fixture.get());
    const QUuid ownerId = fixture->trip->id();
    const QString simple = datasetExternalCenterlinePath(QStringLiteral("survex_simple.svx"));

    attachThroughManager(fixture.get(), fixture->trip, simple);
    drainPipelines(fixture.get());

    cwSignalSpy busySpy(manager, &cwExternalCenterlineManager::ownerBusyChanged);

    const QString nested = datasetExternalCenterlinePath(QStringLiteral("survex_nested.svx"));
    auto replaceFuture = manager->replaceCenterline(fixture->trip, nested);
    REQUIRE(manager->isOwnerBusy(ownerId));

    const QString noMetadata =
        datasetExternalCenterlinePath(QStringLiteral("survex_no_metadata.svx"));
    auto refused = manager->replaceCenterline(fixture->trip, noMetadata);
    REQUIRE(refused.isFinished());
    CHECK(refused.result().hasError());
    CHECK(refused.result().errorMessage().contains(QStringLiteral("in progress")));

    REQUIRE(AsyncFuture::waitForFinished(replaceFuture, kAttachWaitMs));
    REQUIRE_FALSE(replaceFuture.result().hasError());
    drainPipelines(fixture.get());

    // The refused call never took the token, so the owner's busy window
    // is the first replace's alone, and the file that landed is the one
    // that replace ran with.
    CHECK(busySpy.count() == 2);
    CHECK_FALSE(manager->isOwnerBusy(ownerId));
    CHECK(fixture->trip->externalCenterline().entryFile()
          == QStringLiteral("survex_nested.svx"));
}

// ---------------------------------------------------------------------
// Reload verb (plans/EXTERNAL_FILE_PHASE2.html §16 B9b): re-copy the file
// the attachment was picked from, through the same overwrite path Replace
// uses, and only when this machine still has that file outside the project.
// ---------------------------------------------------------------------

namespace {

// Wide margin so a hand-planted edit reads as older than the project copy
// whatever the filesystem's mtime granularity.
constexpr int kEditIsOlderSeconds = 3600;

// A writable copy of `fixtureName` outside the project, so a test can edit
// the source the way a user editing it in Survex would.
QString externalSourceCopy(SavedProjectFixture* fixture, const QString& fixtureName)
{
    const QString sourcePath = QDir(fixture->tempDir.path())
                                   .filePath(QStringLiteral("source/") + fixtureName);
    return writeFileWithMtime(sourcePath,
                              readWholeFile(datasetExternalCenterlinePath(fixtureName)),
                              QDateTime::currentDateTimeUtc());
}

} // namespace

TEST_CASE("reload re-copies the remembered source over the project copy",
          "[Attach][Reload]")
{
    auto fixture = makeSavedProject(QStringLiteral("reload-from-source"));
    auto manager = managerOf(fixture.get());

    const QString sourcePath =
        externalSourceCopy(fixture.get(), QStringLiteral("survex_simple.svx"));
    attachThroughManager(fixture.get(), fixture->trip, sourcePath);
    drainPipelines(fixture.get());

    const QString copyPath = fixture->saveLoad()
        ->externalCenterlineDir(fixture->trip)
        .absoluteFilePath(QStringLiteral("survex_simple.svx"));
    const QByteArray originalBytes = readWholeFile(copyPath);

    // The source moves on after the copy was taken — the state Reload exists
    // for. Same byte count and an mtime the copy already beats: a conditional
    // copy reads this source as up to date and leaves it alone, so only an
    // unconditional overwrite lands the edit.
    QByteArray editedBytes = originalBytes;
    editedBytes.replace(QByteArray("A2 A3 8.5"), QByteArray("A2 A3 9.5"));
    REQUIRE(editedBytes.size() == originalBytes.size());
    REQUIRE(editedBytes != originalBytes);
    writeFileWithMtime(sourcePath, editedBytes,
                       QDateTime::currentDateTimeUtc().addSecs(-kEditIsOlderSeconds));

    CHECK(manager->canReloadFromSource(fixture->trip));

    cwSignalSpy solveSpy(manager, &cwExternalCenterlineManager::solveNeeded);
    cwSignalSpy attachSpy(manager, &cwExternalCenterlineManager::attachCompleted);

    auto reloadFuture = manager->reloadFromSource(fixture->trip);
    REQUIRE(AsyncFuture::waitForFinished(reloadFuture, kAttachWaitMs));
    REQUIRE_FALSE(reloadFuture.result().hasError());
    drainPipelines(fixture.get());

    CHECK(readWholeFile(copyPath) == editedBytes);
    CHECK(fixture->trip->externalCenterline().entryFile()
          == QStringLiteral("survex_simple.svx"));
    CHECK(solveSpy.count() > 0);

    // A reload is a replace, so it reports through the attach bridge like
    // every other operation the surfaces observe.
    REQUIRE(attachSpy.count() == 1);
    const auto report = attachSpy.at(0).at(0).value<cwExternalCenterlineReport>();
    CHECK(report.success());
    CHECK(report.ownerId() == fixture->trip->id());
}

TEST_CASE("reload refuses a busy owner", "[Attach][Reload]")
{
    auto fixture = makeSavedProject(QStringLiteral("reload-busy"));
    auto manager = managerOf(fixture.get());
    const QUuid ownerId = fixture->trip->id();

    const QString sourcePath =
        externalSourceCopy(fixture.get(), QStringLiteral("survex_simple.svx"));
    attachThroughManager(fixture.get(), fixture->trip, sourcePath);
    drainPipelines(fixture.get());

    const QString nested = datasetExternalCenterlinePath(QStringLiteral("survex_nested.svx"));
    auto replaceFuture = manager->replaceCenterline(fixture->trip, nested);
    REQUIRE(manager->isOwnerBusy(ownerId));

    auto refused = manager->reloadFromSource(fixture->trip);
    REQUIRE(refused.isFinished());
    CHECK(refused.result().hasError());
    CHECK(refused.result().errorMessage().contains(QStringLiteral("in progress")));

    REQUIRE(AsyncFuture::waitForFinished(replaceFuture, kAttachWaitMs));
    REQUIRE_FALSE(replaceFuture.result().hasError());
    drainPipelines(fixture.get());

    // The refusal took no token and touched no file: the replace is what
    // landed.
    CHECK_FALSE(manager->isOwnerBusy(ownerId));
    CHECK(fixture->trip->externalCenterline().entryFile()
          == QStringLiteral("survex_nested.svx"));
}

TEST_CASE("reload is offered only for a source this machine has outside the project",
          "[Attach][Reload]")
{
    auto fixture = makeSavedProject(QStringLiteral("reload-eligibility"));
    auto manager = managerOf(fixture.get());
    const QUuid ownerId = fixture->trip->id();

    const QString sourcePath =
        externalSourceCopy(fixture.get(), QStringLiteral("survex_simple.svx"));
    attachThroughManager(fixture.get(), fixture->trip, sourcePath);
    drainPipelines(fixture.get());
    REQUIRE(manager->canReloadFromSource(fixture->trip));

    SECTION("no breadcrumb: the machine never learned where the copy came from")
    {
        fixture->settings()->clearBreadcrumb(ownerId);
        CHECK_FALSE(manager->canReloadFromSource(fixture->trip));
    }

    SECTION("the remembered file is gone from this machine")
    {
        REQUIRE(QFile::remove(sourcePath));
        CHECK_FALSE(manager->canReloadFromSource(fixture->trip));
    }

    SECTION("the remembered file is the project copy itself")
    {
        const QString copyPath = fixture->saveLoad()
            ->externalCenterlineDir(fixture->trip)
            .absoluteFilePath(QStringLiteral("survex_simple.svx"));
        fixture->settings()->setBreadcrumbPath(ownerId, copyPath);
        CHECK_FALSE(manager->canReloadFromSource(fixture->trip));
    }

    // Whatever made it ineligible, asking anyway is refused through the
    // bridge rather than copying something unexpected.
    cwSignalSpy attachSpy(manager, &cwExternalCenterlineManager::attachCompleted);
    auto refused = manager->reloadFromSource(fixture->trip);
    REQUIRE(refused.isFinished());
    CHECK(refused.result().hasError());
    CHECK(refused.result().errorMessage().contains(QStringLiteral("no source file")));
    CHECK_FALSE(manager->isOwnerBusy(ownerId));

    REQUIRE(attachSpy.wait(kAttachWaitMs));
    const auto report = attachSpy.at(0).at(0).value<cwExternalCenterlineReport>();
    CHECK_FALSE(report.success());

    drainPipelines(fixture.get());
}

// ---------------------------------------------------------------------
// Missing in-project copy (plans/EXTERNAL_FILE_LIVE_LINK_RETIREMENT.html
// §7 q1): the copy is the only file the project reads, so its absence is
// the state worth surfacing and Replace is the way out.
// ---------------------------------------------------------------------

TEST_CASE("a deleted in-project copy is reported missing until the file returns",
          "[Attach][MissingCopy]")
{
    auto fixture = makeSavedProject(QStringLiteral("missing-copy-report"));
    auto manager = managerOf(fixture.get());
    const QUuid ownerId = fixture->trip->id();
    const QString source = datasetExternalCenterlinePath(QStringLiteral("survex_simple.svx"));

    attachThroughManager(fixture.get(), fixture->trip, source);
    drainPipelines(fixture.get());
    CHECK(manager->missingCopyPath(ownerId).isEmpty());

    const QString copyPath = fixture->saveLoad()
        ->externalCenterlineDir(fixture->trip)
        .absoluteFilePath(QStringLiteral("survex_simple.svx"));
    REQUIRE(QFileInfo::exists(copyPath));
    const QByteArray copyContents = fileContents(copyPath);

    cwSignalSpy missingSpy(manager, &cwExternalCenterlineManager::missingCopiesChanged);

    // Deleting a watched file fires the watcher, which recomputes on its
    // own - the same path a user emptying the folder in Finder takes.
    REQUIRE(QFile::remove(copyPath));
    REQUIRE(tryWait(kWatcherWaitMs, [&] {
        QCoreApplication::processEvents(QEventLoop::AllEvents, kInnerPollEventsMs);
        return !manager->missingCopyPath(ownerId).isEmpty();
    }));
    CHECK(missingSpy.count() >= 1);

    // Named the way it sits in the project: the copy is the file the user
    // has now, so a path relative to the project's data root.
    const QString relativePath = fixture->saveLoad()->dataRootDir().relativeFilePath(copyPath);
    CHECK(manager->missingCopyPath(ownerId) == relativePath);
    CHECK(relativePath.endsWith(QStringLiteral("survex_simple.svx")));
    CHECK_FALSE(relativePath.startsWith(QStringLiteral("..")));
    CHECK_FALSE(relativePath.startsWith(QStringLiteral("/")));

    // Restoring the file behind the app's back is only noticed by a fresh
    // scan - nothing watches a path that is not there.
    overwriteFile(copyPath, copyContents);
    manager->rescanAttachments();
    REQUIRE(tryWait(kWatcherWaitMs, [&] {
        QCoreApplication::processEvents(QEventLoop::AllEvents, kInnerPollEventsMs);
        return manager->missingCopyPath(ownerId).isEmpty();
    }));

    drainPipelines(fixture.get());
    CHECK(manager->missingCopyPath(ownerId).isEmpty());
}

TEST_CASE("replacing an attachment whose copy went missing clears the report",
          "[Attach][MissingCopy]")
{
    auto fixture = makeSavedProject(QStringLiteral("missing-copy-replace"));
    auto manager = managerOf(fixture.get());
    const QUuid ownerId = fixture->trip->id();
    const QString simple = datasetExternalCenterlinePath(QStringLiteral("survex_simple.svx"));

    attachThroughManager(fixture.get(), fixture->trip, simple);
    drainPipelines(fixture.get());

    const QString copyPath = fixture->saveLoad()
        ->externalCenterlineDir(fixture->trip)
        .absoluteFilePath(QStringLiteral("survex_simple.svx"));
    REQUIRE(QFile::remove(copyPath));
    REQUIRE(tryWait(kWatcherWaitMs, [&] {
        QCoreApplication::processEvents(QEventLoop::AllEvents, kInnerPollEventsMs);
        return !manager->missingCopyPath(ownerId).isEmpty();
    }));

    CHECK(manager->solveInputs().excludedExternalOwners.contains(ownerId));

    // The banner's own affordance: pick the file again. Replace copies a
    // fresh closure into the same attachment dir, so the owner has a file
    // to read again.
    const QString nested = datasetExternalCenterlinePath(QStringLiteral("survex_nested.svx"));
    auto replaceFuture = manager->replaceCenterline(fixture->trip, nested);
    REQUIRE(AsyncFuture::waitForFinished(replaceFuture, kAttachWaitMs));
    REQUIRE_FALSE(replaceFuture.result().hasError());
    drainPipelines(fixture.get());

    CHECK(manager->missingCopyPath(ownerId).isEmpty());
    // Replace is the whole fix the banner promises: the owner is back in the
    // solve, not just out of the report.
    CHECK_FALSE(manager->solveInputs().excludedExternalOwners.contains(ownerId));
    CHECK(fixture->trip->externalCenterline().entryFile()
          == QStringLiteral("survex_nested.svx"));
}

TEST_CASE("a missing copy costs its own survey, not the region's plot",
          "[Attach][MissingCopy]")
{
    auto fixture = makeSavedProject(QStringLiteral("missing-copy-solve"));
    auto manager = managerOf(fixture.get());
    cwLinePlotManager* linePlotManager = fixture->rootData->linePlotManager();
    const QString simple = datasetExternalCenterlinePath(QStringLiteral("survex_simple.svx"));

    fixture->cave->addTrip();
    cwTrip* survivor = fixture->cave->trip(1);
    survivor->setName(QStringLiteral("SurvivingTrip"));

    attachThroughManager(fixture.get(), fixture->trip, simple);
    auto survivorAttach = manager->attachCenterline(survivor, simple);
    REQUIRE(AsyncFuture::waitForFinished(survivorAttach, kAttachWaitMs));
    REQUIRE_FALSE(survivorAttach.result().hasError());
    drainPipelines(fixture.get());

    // survex_simple.svx opens "*begin Simple" and fixes a1, so each trip's
    // stations land under its own "*begin <tripLabel>" wrapper.
    const QString missingKey =
        tripScopeLabel(fixture->trip) + QStringLiteral(".simple.a1");
    const QString survivorKey =
        tripScopeLabel(survivor) + QStringLiteral(".simple.a1");

    INFO("solve error: " << linePlotManager->solveErrorMessage().toStdString());
    REQUIRE_FALSE(linePlotManager->hasSolveError());
    REQUIRE(fixture->cave->stationPositionLookup().hasPosition(missingKey));
    REQUIRE(fixture->cave->stationPositionLookup().hasPosition(survivorKey));

    const QString copyPath = fixture->saveLoad()
        ->externalCenterlineDir(fixture->trip)
        .absoluteFilePath(QStringLiteral("survex_simple.svx"));
    const QByteArray copyContents = fileContents(copyPath);
    REQUIRE(QFile::remove(copyPath));

    // The watcher notices the deletion and recomputes; the recompute's
    // solve request is what makes the driver drop the *include.
    const QUuid missingOwnerId = fixture->trip->id();
    REQUIRE(tryWait(kWatcherWaitMs, [&] {
        QCoreApplication::processEvents(QEventLoop::AllEvents, kInnerPollEventsMs);
        return manager->solveInputs().excludedExternalOwners.contains(missingOwnerId);
    }));
    drainPipelines(fixture.get());

    // Cavern fatals on an *include it cannot open, which would cost the
    // whole region its plot — every trip panel reading "Solve failed" for
    // one broken attachment the banner says Replace fixes.
    INFO("solve error: " << linePlotManager->solveErrorMessage().toStdString());
    CHECK_FALSE(linePlotManager->hasSolveError());
    CHECK_FALSE(manager->missingCopyPath(missingOwnerId).isEmpty());
    CHECK(fixture->cave->stationPositionLookup().hasPosition(survivorKey));
    CHECK_FALSE(fixture->cave->stationPositionLookup().hasPosition(missingKey));

    // Putting the file back is the other way out, and a fresh scan is what
    // notices it — nothing watches a path that is not there.
    overwriteFile(copyPath, copyContents);
    manager->rescanAttachments();
    REQUIRE(tryWait(kWatcherWaitMs, [&] {
        QCoreApplication::processEvents(QEventLoop::AllEvents, kInnerPollEventsMs);
        return !manager->solveInputs().excludedExternalOwners.contains(missingOwnerId);
    }));
    drainPipelines(fixture.get());

    CHECK(manager->missingCopyPath(missingOwnerId).isEmpty());
    INFO("solve error: " << linePlotManager->solveErrorMessage().toStdString());
    CHECK_FALSE(linePlotManager->hasSolveError());
    CHECK(fixture->cave->stationPositionLookup().hasPosition(missingKey));
    CHECK(fixture->cave->stationPositionLookup().hasPosition(survivorKey));
}

// ---------------------------------------------------------------------
// Add Trip -> "Add trip from survey file..." (master §8.7). CavePage
// creates the trip first, attaches to it, and only then names it after
// the picked file - so the attachment dir the copies landed in belongs
// to the trip's placeholder name, and the rename has to bring them
// along. Both cases below run that exact order.
// ---------------------------------------------------------------------

TEST_CASE("naming a trip from its file after attach keeps the copy the project reads",
          "[Attach][NewTripFromFile]")
{
    // The reported bug's setup is the unsaved project: a cave added to a
    // brand-new project, then a trip added through Add trip from survey
    // file. The saved project runs the same order from a durable home,
    // which separates "the rename lost the files" from "an unsaved
    // project never had a place to move them from".
    std::unique_ptr<SavedProjectFixture> fixture;
    SECTION("unsaved project") { fixture = makeNewProject(); }
    SECTION("saved project") {
        fixture = makeSavedProject(QStringLiteral("attach-name-from-file"));
    }

    const QString source = datasetExternalCenterlinePath(QStringLiteral("survex_simple.svx"));

    attachThroughManager(fixture.get(), fixture->trip, source);
    drainPipelines(fixture.get());

    // The copies landed under the placeholder name the dialog attached to.
    REQUIRE(QFileInfo::exists(fixture->saveLoad()
                                  ->externalCenterlineDir(fixture->trip)
                                  .absoluteFilePath(QStringLiteral("survex_simple.svx"))));

    nameTripFromEntryFile(fixture.get());
    drainPipelines(fixture.get());
    CHECK(fixture->trip->name() == QStringLiteral("survex_simple"));

    checkAttachmentIsReadable(fixture.get());
}

TEST_CASE("a scan racing the rename's move stops reporting the copy missing",
          "[Attach][NewTripFromFile]")
{
    // The banner half of the same defect. In the app the rename's move
    // fires the watcher, and that recompute can scan the new attachment
    // dir while the move is still queued - so the entry file is at
    // neither path and the owner is reported missing. The move's own
    // completion is what has to correct it; without that trigger the
    // banner stays up for the rest of the session.
    auto fixture = makeNewProject();
    const QString source = datasetExternalCenterlinePath(QStringLiteral("survex_simple.svx"));

    attachThroughManager(fixture.get(), fixture->trip, source);
    drainPipelines(fixture.get());

    auto manager = managerOf(fixture.get());
    nameTripFromEntryFile(fixture.get());
    manager->rescanAttachments(); // races the move, as the watcher's does
    drainPipelines(fixture.get());

    checkAttachmentIsReadable(fixture.get());
}
