/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// The N2 gate (plans/EXTERNAL_SOURCE_CHANGE_NOTIFY.html §5): one status
// model says, for every attachment this machine remembers a source for,
// whether that source has moved on since it was copied - and a project
// open sweeps them all.

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwExternalCenterline.h"
#include "cwExternalCenterlineAttach.h"
#include "cwExternalCenterlineManager.h"
#include "cwExternalSourceSettings.h"
#include "cwExternalSourceStatusModel.h"
#include "cwFutureManagerModel.h"
#include "cwLinePlotManager.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSaveLoad.h"
#include "cwSurveyNode.h"
#include "cwTrip.h"
#include "ExternalCenterlineTestHelpers.h"

// AsyncFuture
#include <asyncfuture.h>

// Qt
#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QUuid>
#include <QWindow>

// Std
#include <memory>

namespace {

using Status = cwExternalSourceStatusModel::Status;

// Wide margin so a planted edit reads as a different mtime whatever the
// filesystem's granularity, which can be a full second.
constexpr int kEditIsNewerSeconds = 3600;

const QByteArray kOneShot("A1 A2 5.0 0 0\n");
const QByteArray kTwoShots("A1 A2 5.0 0 0\nA2 A3 7.25 90 0\n");
// Same byte count as kOneShot, different contents.
const QByteArray kOneShotSameSize("A1 A2 6.0 0 0\n");

// A one-file Survex source outside the project, written with a chosen
// mtime so an edit is unambiguous. Returns the path.
QString writeSource(const QString& path,
                    const QByteArray& shots,
                    const QDateTime& mtime = QDateTime::currentDateTimeUtc())
{
    return writeFileWithMtime(path,
                              QByteArray("*begin Alpha\n"
                                         "*data normal from to tape compass clino\n")
                                  + shots + "*end Alpha\n",
                              mtime);
}

QString sourceDirOf(const SavedProjectFixture* fixture)
{
    const QString dir = QDir(fixture->tempDir.path()).absoluteFilePath(
        QStringLiteral("source"));
    REQUIRE(QDir().mkpath(dir));
    return dir;
}

QString sourcePathIn(const QString& dir, const QString& name)
{
    return QDir(dir).absoluteFilePath(name);
}

cwExternalSourceStatusModel* statusModelOf(const SavedProjectFixture* fixture)
{
    return fixture->rootData->externalCenterlineManager()->sourceStatusModel();
}

// Re-reads every attachment (the same pass a project open runs) and waits
// for it to land.
void sweep(SavedProjectFixture* fixture)
{
    fixture->rootData->externalCenterlineManager()->rescanAttachments();
    drainPipelines(fixture);
}

std::unique_ptr<SavedProjectFixture> makeStatusProject(const QString& projectFileBase)
{
    return makeSavedProject(projectFileBase,
                            QStringLiteral("StatusCave"),
                            QStringLiteral("StatusTrip"));
}

// Saves and drains so the next assertion starts from an unmodified
// project.
void saveAndSettle(SavedProjectFixture* fixture)
{
    REQUIRE(fixture->project->save());
    drainPipelines(fixture);
    REQUIRE_FALSE(fixture->project->modified());
}

const cwExternalSourceRecord::SourceFileFingerprint&
onlyRecord(const cwExternalSourceRecord::SourceFingerprint& fingerprint)
{
    REQUIRE(fingerprint.files.size() == 1);
    return fingerprint.files.at(0);
}

} // namespace

TEST_CASE("the sweep at project open reports a source edited while the project was closed",
          "[ExternalSourceStatus]")
{
    auto fixture = makeStatusProject(QStringLiteral("status-open-sweep"));
    const QString sourcePath =
        writeSource(sourcePathIn(sourceDirOf(fixture.get()), QStringLiteral("entry.svx")),
                    kOneShot);

    attachThroughManager(fixture.get(), fixture->trip, sourcePath);
    drainPipelines(fixture.get());
    saveAndSettle(fixture.get());

    const QUuid tripId = fixture->trip->id();
    CHECK(statusModelOf(fixture.get())->statusFor(tripId) == Status::UpToDate);

    const QString projectPath = fixture->project->filename();

    // The evening of edits the app never saw.
    writeSource(sourcePath, kTwoShots,
                QDateTime::currentDateTimeUtc().addSecs(kEditIsNewerSeconds));

    auto freshRoot = std::make_unique<cwRootData>();
    freshRoot->project()->loadFile(projectPath);
    freshRoot->project()->waitLoadToFinish();
    freshRoot->linePlotManager()->waitToFinish();
    freshRoot->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();
    waitForSourceSweep(freshRoot->externalCenterlineManager());

    auto* statuses = freshRoot->externalCenterlineManager()->sourceStatusModel();
    REQUIRE(statuses->rowCount() == 1);
    CHECK(roleAt(statuses, 0, cwExternalSourceStatusModel::OwnerIdRole).toUuid() == tripId);
    CHECK(roleAt(statuses, 0, cwExternalSourceStatusModel::SourcePathRole).toString()
          == QFileInfo(sourcePath).absoluteFilePath());
    CHECK(statuses->statusFor(tripId) == Status::Changed);
    CHECK(statuses->sourcePathFor(tripId) == QFileInfo(sourcePath).absoluteFilePath());
}

TEST_CASE("a source that is gone reads as missing rather than changed",
          "[ExternalSourceStatus]")
{
    auto fixture = makeStatusProject(QStringLiteral("status-missing-source"));
    const QString sourcePath =
        writeSource(sourcePathIn(sourceDirOf(fixture.get()), QStringLiteral("entry.svx")),
                    kOneShot);

    attachThroughManager(fixture.get(), fixture->trip, sourcePath);
    drainPipelines(fixture.get());
    REQUIRE(statusModelOf(fixture.get())->statusFor(fixture->trip->id())
            == Status::UpToDate);

    REQUIRE(QFile::remove(sourcePath));
    sweep(fixture.get());

    CHECK(statusModelOf(fixture.get())->statusFor(fixture->trip->id())
          == Status::SourceMissing);
    // The path is still reported: it is what the user is being told about.
    CHECK(statusModelOf(fixture.get())->sourcePathFor(fixture->trip->id())
          == QFileInfo(sourcePath).absoluteFilePath());
}

TEST_CASE("an attachment this machine remembers no source for stays quiet",
          "[ExternalSourceStatus]")
{
    auto fixture = makeStatusProject(QStringLiteral("status-no-breadcrumb"));
    const QString sourcePath =
        writeSource(sourcePathIn(sourceDirOf(fixture.get()), QStringLiteral("entry.svx")),
                    kOneShot);

    attachThroughManager(fixture.get(), fixture->trip, sourcePath);
    drainPipelines(fixture.get());
    const QUuid tripId = fixture->trip->id();

    SECTION("no breadcrumb at all - the collaborator who got the project through Git")
    {
        fixture->settings()->clearBreadcrumb(tripId);
        sweep(fixture.get());

        REQUIRE(statusModelOf(fixture.get())->rowCount() == 1);
        CHECK(statusModelOf(fixture.get())->statusFor(tripId) == Status::NoBreadcrumb);
        CHECK(statusModelOf(fixture.get())->sourcePathFor(tripId).isEmpty());
    }

    SECTION("an entry written before fingerprints existed, whose source has since changed")
    {
        // The path-only writer is what an older CaveWhere recorded.
        fixture->settings()->setBreadcrumbPath(tripId, sourcePath);
        REQUIRE(fixture->settings()->fingerprint(tripId).isEmpty());

        writeSource(sourcePath, kTwoShots,
                    QDateTime::currentDateTimeUtc().addSecs(kEditIsNewerSeconds));
        sweep(fixture.get());

        // Never nag on data older than the feature.
        CHECK(statusModelOf(fixture.get())->statusFor(tripId) == Status::NoBreadcrumb);
        CHECK(statusModelOf(fixture.get())->sourcePathFor(tripId)
              == QFileInfo(sourcePath).absoluteFilePath());
    }
}

TEST_CASE("a touched but identical source stays up to date and refreshes its stored stats",
          "[ExternalSourceStatus]")
{
    auto fixture = makeStatusProject(QStringLiteral("status-touched-identical"));
    const QString sourcePath =
        writeSource(sourcePathIn(sourceDirOf(fixture.get()), QStringLiteral("entry.svx")),
                    kOneShot);

    attachThroughManager(fixture.get(), fixture->trip, sourcePath);
    drainPipelines(fixture.get());
    saveAndSettle(fixture.get());

    const QUuid tripId = fixture->trip->id();
    const auto stamped = fixture->settings()->fingerprint(tripId);
    REQUIRE_FALSE(stamped.isEmpty());

    // The git-checkout case: identical bytes, fresh mtime.
    const QDateTime touched =
        QDateTime::currentDateTimeUtc().addSecs(kEditIsNewerSeconds);
    writeSource(sourcePath, kOneShot, touched);
    REQUIRE(onlyRecord(stamped).lastModified != touched.toMSecsSinceEpoch());

    QSignalSpy breadcrumbSpy(fixture->settings(),
                             &cwExternalSourceSettings::breadcrumbsChanged);
    QSignalSpy statusSpy(statusModelOf(fixture.get()),
                         &cwExternalSourceStatusModel::statusesChanged);
    QSignalSpy resetSpy(statusModelOf(fixture.get()),
                        &cwExternalSourceStatusModel::modelReset);
    sweep(fixture.get());

    CHECK(statusModelOf(fixture.get())->statusFor(tripId) == Status::UpToDate);

    // The refresh is bookkeeping about files nobody changed: it neither
    // dirties the project nor announces itself as a breadcrumb change,
    // and the status model - which every banner and list binds to -
    // stays as quiet as it was, with no reset for them to churn on.
    CHECK(breadcrumbSpy.count() == 0);
    CHECK(statusSpy.count() == 0);
    CHECK(resetSpy.count() == 0);
    CHECK_FALSE(fixture->project->modified());

    // The stats now describe the file as it sits, so the next check takes
    // the pure-stat path instead of reading the file again.
    const auto refreshed = [&fixture, tripId]() {
        const auto reopened = reopenFromDisk(fixture.get());
        return reopened->externalSourceSettings()->fingerprint(tripId);
    }();
    const QFileInfo sourceInfo(sourcePath);
    CHECK(onlyRecord(refreshed).lastModified == sourceInfo.lastModified().toMSecsSinceEpoch());
    CHECK(onlyRecord(refreshed).size == sourceInfo.size());
    CHECK(onlyRecord(refreshed).sha256 == onlyRecord(stamped).sha256);

    // Different bytes at the same size and mtime - the fast path's
    // documented blind spot, and so proof that the second check settles
    // on stats alone and reads no file.
    writeSource(sourcePath, kOneShotSameSize, touched);
    REQUIRE(QFileInfo(sourcePath).size() == onlyRecord(refreshed).size);
    sweep(fixture.get());
    CHECK(statusModelOf(fixture.get())->statusFor(tripId) == Status::UpToDate);
}

TEST_CASE("cave-level attachments get their own status row",
          "[ExternalSourceStatus]")
{
    auto fixture = makeStatusProject(QStringLiteral("status-cave-owner"));
    const QString sourceDir = sourceDirOf(fixture.get());

    const QString tripSource =
        writeSource(sourcePathIn(sourceDir, QStringLiteral("trip.svx")), kOneShot);
    attachThroughManager(fixture.get(), fixture->trip, tripSource);
    drainPipelines(fixture.get());

    // Cave-level attachment: the copy path is trip-only today, so the
    // cave's attachment and its breadcrumb are seeded the way a load
    // would leave them.
    const QString caveSource =
        writeSource(sourcePathIn(sourceDir, QStringLiteral("cave.svx")), kOneShot);
    fixture->cave->setExternalCenterline(cwExternalCenterline(QStringLiteral("cave.svx")));
    fixture->settings()->setBreadcrumb(
        fixture->cave->id(), caveSource,
        cwExternalSourceRecord::computeFingerprint({caveSource}));
    sweep(fixture.get());

    auto* statuses = statusModelOf(fixture.get());
    REQUIRE(statuses->rowCount() == 2);
    CHECK(statuses->statusFor(fixture->cave->id()) == Status::UpToDate);
    CHECK(statuses->statusFor(fixture->trip->id()) == Status::UpToDate);

    writeSource(caveSource, kTwoShots,
                QDateTime::currentDateTimeUtc().addSecs(kEditIsNewerSeconds));
    sweep(fixture.get());

    CHECK(statuses->statusFor(fixture->cave->id()) == Status::Changed);
    CHECK(statuses->sourcePathFor(fixture->cave->id())
          == QFileInfo(caveSource).absoluteFilePath());
    // One owner's edit leaves the other owner alone.
    CHECK(statuses->statusFor(fixture->trip->id()) == Status::UpToDate);
}

TEST_CASE("an owner with no attachment has no status row",
          "[ExternalSourceStatus]")
{
    auto fixture = makeStatusProject(QStringLiteral("status-unattached"));
    drainPipelines(fixture.get());

    auto* statuses = statusModelOf(fixture.get());
    CHECK(statuses->rowCount() == 0);
    CHECK(statuses->statusFor(fixture->trip->id()) == Status::NoBreadcrumb);
}

namespace {

// The update verbs are void, so the test waits for the operations' own
// reports rather than assuming the copies already landed.
void waitForAttachReports(SavedProjectFixture* fixture, QSignalSpy& attachSpy,
                          int expectedCount)
{
    while (attachSpy.count() < expectedCount && attachSpy.wait(kAttachWaitMs)) {
    }
    REQUIRE(attachSpy.count() == expectedCount);
    drainPipelines(fixture);
}

} // namespace

TEST_CASE("updateFromSource re-copies a source root's changed source",
          "[ExternalSourceStatus]")
{
    auto fixture = makeStatusProject(QStringLiteral("status-cave-update"));
    const QString caveSource =
        writeSource(sourcePathIn(sourceDirOf(fixture.get()), QStringLiteral("cave.svx")),
                    kOneShot);

    cwCave* cave = attachSourceUnderRegion(fixture.get(), caveSource);
    drainPipelines(fixture.get());

    auto* statuses = statusModelOf(fixture.get());
    REQUIRE(statuses->statusFor(cave->id()) == Status::UpToDate);

    writeSource(caveSource, kTwoShots,
                QDateTime::currentDateTimeUtc().addSecs(kEditIsNewerSeconds));
    sweep(fixture.get());
    REQUIRE(statuses->statusFor(cave->id()) == Status::Changed);

    QSignalSpy attachSpy(fixture->rootData->externalCenterlineManager(),
                         &cwExternalCenterlineManager::attachCompleted);
    fixture->rootData->externalCenterlineManager()->updateFromSource(cave->id());
    waitForAttachReports(fixture.get(), attachSpy, 1);

    sweep(fixture.get());
    CHECK(statuses->statusFor(cave->id()) == Status::UpToDate);

    const QString copyPath = fixture->saveLoad()
        ->externalCenterlineDir(cave)
        .absoluteFilePath(QStringLiteral("cave.svx"));
    CHECK(fileContents(copyPath).contains(kTwoShots));
}

TEST_CASE("update all covers source-root rows alongside trip rows",
          "[ExternalSourceStatus]")
{
    auto fixture = makeStatusProject(QStringLiteral("status-update-all-mixed"));
    const QString sourceDir = sourceDirOf(fixture.get());

    const QString tripSource =
        writeSource(sourcePathIn(sourceDir, QStringLiteral("trip.svx")), kOneShot);
    attachThroughManager(fixture.get(), fixture->trip, tripSource);
    drainPipelines(fixture.get());

    const QString caveSource =
        writeSource(sourcePathIn(sourceDir, QStringLiteral("cave.svx")), kOneShot);
    cwCave* cave = attachSourceUnderRegion(fixture.get(), caveSource);
    drainPipelines(fixture.get());

    auto* statuses = statusModelOf(fixture.get());
    REQUIRE(statuses->statusFor(fixture->trip->id()) == Status::UpToDate);
    REQUIRE(statuses->statusFor(cave->id()) == Status::UpToDate);

    const QDateTime edited = QDateTime::currentDateTimeUtc().addSecs(kEditIsNewerSeconds);
    writeSource(tripSource, kTwoShots, edited);
    writeSource(caveSource, kTwoShots, edited);
    sweep(fixture.get());
    REQUIRE(statuses->statusFor(fixture->trip->id()) == Status::Changed);
    REQUIRE(statuses->statusFor(cave->id()) == Status::Changed);

    QSignalSpy attachSpy(fixture->rootData->externalCenterlineManager(),
                         &cwExternalCenterlineManager::attachCompleted);
    fixture->rootData->externalCenterlineManager()->updateAllChangedSources();
    waitForAttachReports(fixture.get(), attachSpy, 2);

    sweep(fixture.get());
    CHECK(statuses->statusFor(fixture->trip->id()) == Status::UpToDate);
    CHECK(statuses->statusFor(cave->id()) == Status::UpToDate);
}

// SB.3 (plans/SOURCE_BREADCRUMB_LOCAL_STORE_PLAN.md): the source sweep reads
// and hashes on a worker, one pass per burst of triggers, and a result that
// a breadcrumb change or a deletion overtook while it was in flight never
// lands.

namespace {

using SourceFingerprint = cwExternalSourceRecord::SourceFingerprint;

// Far enough from any real mtime that the stored stats miss the disk and
// the sweep has to hash the files to settle it.
constexpr qint64 kStaleStatShiftMs = 60000;

// Re-stamps in the burst case; each one is a distinct record, so each one
// emits breadcrumbsChanged.
constexpr int kBurstRestamps = 3;

// One pass for the burst, plus at most the one queued re-run.
constexpr int kMaxPassesPerBurst = 2;

constexpr int kGrowthBooks = 2;
constexpr int kGrowthSmallSurveysPerBook = 130;
constexpr int kGrowthLargeSurveysPerBook = 520;
constexpr int kGrowthShotsPerSurvey = 3;
constexpr int kGrowthFixEvery = 25;
constexpr double kMaxSweepGrowthRatio = 2.0;

// A thousand-file Walls attach under Debug + ASAN on a loaded machine.
constexpr int kLargeAttachWaitMs = 300000;

SourceFingerprint withStaleStats(SourceFingerprint fingerprint, qint64 shiftMs = kStaleStatShiftMs)
{
    for (auto& record : fingerprint.files) {
        record.lastModified -= shiftMs;
    }
    return fingerprint;
}

// Re-stamps ownerId's record with stats that miss the disk, which both
// triggers a sweep and sends it down the hash path. The files themselves
// are untouched, so the hash finds them identical.
void stageHashPath(cwExternalSourceSettings* settings, const QUuid& ownerId)
{
    settings->setBreadcrumb(ownerId, settings->breadcrumbPath(ownerId),
                            withStaleStats(settings->fingerprint(ownerId)));
}

// The same, with hold's FIFO added to the record, so the sweep's hash
// stays in flight until the hold lets go.
void stageHeldHashPath(cwExternalSourceSettings* settings, const QUuid& ownerId,
                       const SourceReadHold& hold)
{
    auto fingerprint = withStaleStats(settings->fingerprint(ownerId));
    fingerprint.files.append(hold.staleRecord());
    settings->setBreadcrumb(ownerId, settings->breadcrumbPath(ownerId), fingerprint);
}

QStringList canonicalPathsOf(const SourceFingerprint& fingerprint)
{
    QStringList paths;
    for (const auto& record : fingerprint.files) {
        paths.append(QFileInfo(record.path).canonicalFilePath());
    }
    return paths;
}

QStringList pathsOf(const SourceFingerprint& fingerprint)
{
    QStringList paths;
    for (const auto& record : fingerprint.files) {
        paths.append(record.path);
    }
    return paths;
}

QString canonicalPathOf(const QString& path)
{
    return QFileInfo(path).canonicalFilePath();
}

// The longest main-thread gap from staging a hash-path sweep over a sourced
// Walls root of kGrowthBooks books of surveysPerBook surveys until that
// sweep has installed its stat refresh.
qint64 hashSweepStall(int surveysPerBook)
{
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString entry = writeWallsProject(tempSubdir(sourceDir, QStringLiteral("walls")),
                                            kGrowthBooks, surveysPerBook, kGrowthShotsPerSurvey,
                                            kGrowthFixEvery);
    auto fixture = makeStatusProject(QStringLiteral("status-sweep-growth-%1").arg(surveysPerBook));
    drainPipelines(fixture.get());

    auto future = managerOf(fixture.get())
                      ->attachSource(fixture->project->cavingRegion()->rootNode(), entry);
    REQUIRE(AsyncFuture::waitForFinished(future, kLargeAttachWaitMs));
    INFO("attach: " << future.result().errorMessage().toStdString());
    REQUIRE_FALSE(future.result().hasError());
    const QUuid rootId = future.result().value().root->id();
    drainPipelines(fixture.get());

    cwExternalSourceSettings* settings = fixture->settings();
    const cwExternalCenterlineManager* manager = managerOf(fixture.get());
    const SourceFingerprint settled = settings->fingerprint(rootId);
    REQUIRE(settled.files.size() == kGrowthBooks * surveysPerBook + 1);
    REQUIRE(statusModelOf(fixture.get())->statusFor(rootId) == Status::UpToDate);

    const qint64 stall = longestMainThreadStallUntil(
        [&]() { stageHashPath(settings, rootId); },
        [&]() { return !manager->isSweepingSources() && settings->fingerprint(rootId) == settled; });
    CHECK(statusModelOf(fixture.get())->statusFor(rootId) == Status::UpToDate);
    drainPipelines(fixture.get());
    return stall;
}

} // namespace

TEST_CASE("a burst of sweep triggers in one event-loop turn runs one sweep pass",
          "[ExternalSourceStatus][Sweep]")
{
    auto fixture = makeStatusProject(QStringLiteral("status-sweep-burst"));
    const QString sourcePath =
        writeSource(sourcePathIn(sourceDirOf(fixture.get()), QStringLiteral("entry.svx")),
                    kOneShot);
    attachThroughManager(fixture.get(), fixture->trip, sourcePath);
    drainPipelines(fixture.get());

    const QUuid tripId = fixture->trip->id();
    cwExternalSourceSettings* settings = fixture->settings();
    const SourceFingerprint stamped = settings->fingerprint(tripId);
    REQUIRE_FALSE(stamped.isEmpty());

    QWindow window;
    QSignalSpy swept(managerOf(fixture.get()), &cwExternalCenterlineManager::sourcesSwept);

    // Every trigger in one turn: re-stamps (each emits breadcrumbsChanged)
    // and the window regaining focus.
    for (int i = 1; i <= kBurstRestamps; ++i) {
        settings->setBreadcrumb(tripId, sourcePath, withStaleStats(stamped, kStaleStatShiftMs * i));
    }
    emit qGuiApp->focusWindowChanged(&window);

    drainPipelines(fixture.get());
    settleEventLoop(kNothingHappensSettleMs);
    drainPipelines(fixture.get());

    INFO("sweep passes: " << swept.count());
    CHECK(swept.count() >= 1);
    CHECK(swept.count() <= kMaxPassesPerBurst);
    CHECK(statusModelOf(fixture.get())->statusFor(tripId) == Status::UpToDate);
    // The hash found the file as it was stamped and put the disk's stats back.
    CHECK(settings->fingerprint(tripId) == stamped);
}

TEST_CASE("a Replace that lands while a sweep is hashing keeps the replacement's record",
          "[ExternalSourceStatus][Sweep]")
{
    if (!SourceReadHold::kSupported) {
        SKIP("Holding a sweep's read needs a POSIX FIFO");
    }

    auto fixture = makeStatusProject(QStringLiteral("status-sweep-replace"));
    const QString sourceDir = sourceDirOf(fixture.get());
    const QString originalSource =
        writeSource(sourcePathIn(sourceDir, QStringLiteral("original.svx")), kOneShot);
    attachThroughManager(fixture.get(), fixture->trip, originalSource);
    const QString caveSource =
        writeSource(sourcePathIn(sourceDir, QStringLiteral("cave.svx")), kOneShot);
    cwCave* cave = attachSourceUnderRegion(fixture.get(), caveSource);
    drainPipelines(fixture.get());
    const QString replacementSource =
        writeSource(sourcePathIn(sourceDir, QStringLiteral("replacement.svx")), kTwoShots);

    const QUuid tripId = fixture->trip->id();
    cwExternalSourceSettings* settings = fixture->settings();
    cwExternalCenterlineManager* manager = managerOf(fixture.get());
    auto* statuses = statusModelOf(fixture.get());
    REQUIRE(statuses->statusFor(tripId) == Status::UpToDate);
    // The sweep hashes owners in row order, so the trip must come before the
    // held file: the stale pass then carries the trip's refresh for the
    // original file when the hold lets go.
    REQUIRE(statuses->rowCount() == 2);
    REQUIRE(roleAt(statuses, 0, cwExternalSourceStatusModel::OwnerIdRole).toUuid() == tripId);

    // The sweep snapshots the trip's original record with stats it will
    // refresh, then stays on the cave's held file.
    SourceReadHold hold;
    stageHashPath(settings, tripId);
    stageHeldHashPath(settings, cave->id(), hold);
    REQUIRE(hold.waitUntilHeld());

    auto future = manager->replaceCenterline(fixture->trip, replacementSource);
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    INFO("replace: " << future.result().errorMessage().toStdString());
    REQUIRE_FALSE(future.result().hasError());
    // The Replace stamped its record while the older sweep was still reading.
    REQUIRE(hold.isHolding());
    REQUIRE(canonicalPathsOf(settings->fingerprint(tripId))
            == QStringList { canonicalPathOf(replacementSource) });

    hold.release();
    drainPipelines(fixture.get());

    const SourceFingerprint record = settings->fingerprint(tripId);
    CHECK(canonicalPathOf(settings->breadcrumbPath(tripId)) == canonicalPathOf(replacementSource));
    CHECK(canonicalPathsOf(record) == QStringList { canonicalPathOf(replacementSource) });
    // Stats and hashes are the replacement's as it sits on disk.
    CHECK(record == cwExternalSourceRecord::computeFingerprint(pathsOf(record)));
    CHECK(statusModelOf(fixture.get())->statusFor(tripId) == Status::UpToDate);
    const QStringList watched = manager->watchedSourceFiles();
    CHECK(watched.contains(canonicalPathOf(replacementSource)));
    CHECK_FALSE(watched.contains(canonicalPathOf(originalSource)));
}

TEST_CASE("an owner deleted while a sweep is hashing gets no status row",
          "[ExternalSourceStatus][Sweep]")
{
    if (!SourceReadHold::kSupported) {
        SKIP("Holding a sweep's read needs a POSIX FIFO");
    }

    auto fixture = makeStatusProject(QStringLiteral("status-sweep-delete"));
    const QString sourceDir = sourceDirOf(fixture.get());
    const QString tripSource =
        writeSource(sourcePathIn(sourceDir, QStringLiteral("trip.svx")), kOneShot);
    attachThroughManager(fixture.get(), fixture->trip, tripSource);
    const QString caveSource =
        writeSource(sourcePathIn(sourceDir, QStringLiteral("cave.svx")), kOneShot);
    cwCave* cave = attachSourceUnderRegion(fixture.get(), caveSource);
    drainPipelines(fixture.get());

    const QUuid tripId = fixture->trip->id();
    const QUuid caveId = cave->id();
    auto* statuses = statusModelOf(fixture.get());
    REQUIRE(statuses->rowCount() == 2);

    SourceReadHold hold;
    stageHeldHashPath(fixture->settings(), caveId, hold);
    REQUIRE(hold.waitUntilHeld());

    auto future = managerOf(fixture.get())->deleteSourceRoot(cave);
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    INFO("delete: " << future.result().errorMessage().toStdString());
    REQUIRE_FALSE(future.result().hasError());
    REQUIRE(hold.isHolding());

    // Every row set installed from here on, the held stale pass's included.
    int installsWithDeletedOwner = 0;
    QObject::connect(statuses, &cwExternalSourceStatusModel::statusesChanged, statuses,
                     [statuses, caveId, &installsWithDeletedOwner]() {
        for (int row = 0; row < statuses->rowCount(); ++row) {
            if (roleAt(statuses, row, cwExternalSourceStatusModel::OwnerIdRole).toUuid() == caveId) {
                ++installsWithDeletedOwner;
            }
        }
    });
    QSignalSpy swept(managerOf(fixture.get()), &cwExternalCenterlineManager::sourcesSwept);

    hold.release();
    drainPipelines(fixture.get());

    CHECK(swept.count() >= 1);
    CHECK(installsWithDeletedOwner == 0);
    REQUIRE(statuses->rowCount() == 1);
    CHECK(roleAt(statuses, 0, cwExternalSourceStatusModel::OwnerIdRole).toUuid() == tripId);
    CHECK(statuses->statusFor(tripId) == Status::UpToDate);
    CHECK_FALSE(fixture->settings()->hasBreadcrumb(caveId));
}

TEST_CASE("a hash-path sweep holds the main thread no longer as its source grows",
          "[ExternalSourceStatus][Sweep]")
{
    const qint64 small = hashSweepStall(kGrowthSmallSurveysPerBook);
    const qint64 large = hashSweepStall(kGrowthLargeSurveysPerBook);
    INFO("longest stall: " << small << " ms at " << kGrowthSmallSurveysPerBook
         << " surveys per book, " << large << " ms at " << kGrowthLargeSurveysPerBook);
    REQUIRE(small > 0);
    CHECK(static_cast<double>(large) / static_cast<double>(small) <= kMaxSweepGrowthRatio);
}
