/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// B8 (plans/EXTERNAL_FILE_PHASE2.html §16): editing an in-project attachment
// outside CaveWhere fires the watcher and re-solves, but queues no save job —
// and a queued save job is the only other thing that reaches
// cwProject::modified(). The edit therefore has to mark the project modified
// on its own account, while CaveWhere's own on-disk work (discard, sync,
// Save As) must not, since those look identical to the watcher.

// Catch
#include <catch2/catch_test_macros.hpp>

// Cavewhere
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwExternalCenterline.h"
#include "cwExternalCenterlineManager.h"
#include "cwExternalSourceSettings.h"
#include "cwFutureManagerModel.h"
#include "cwLinePlotManager.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSaveLoad.h"
#include "cwTrip.h"

// Tests
#include "ExternalCenterlineTestHelpers.h"

// Qt
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QTemporaryDir>
#include <QTest>

// Std
#include <memory>

namespace {

const QByteArray kOriginalEntry =
    "*begin Entry\n"
    "*fix E1 0 0 0\n"
    "*data normal from to tape compass clino\n"
    "E1 E2 10.0 0 0\n"
    "*end Entry\n";

// Content the solve can read, so the edit is an ordinary one rather than a
// broken file.
const QByteArray kEditedEntry =
    "*begin Entry\n"
    "*fix E1 0 0 0\n"
    "*data normal from to tape compass clino\n"
    "E1 E2 10.0 0 0\n"
    "E2 E3 12.0 90 0\n"
    "*end Entry\n";

struct ModifiedFixture {
    QTemporaryDir tempDir;
    std::unique_ptr<cwRootData> rootData;
    cwProject* project = nullptr;
    cwTrip* trip = nullptr;
    QString entryPath;

    cwSaveLoad* saveLoad() const { return project->saveLoad(); }
};

void drainSave(ModifiedFixture& fixture)
{
    fixture.project->waitSaveToFinish();
    fixture.rootData->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();
}

// One cave, one trip, one attachment declared — the model half of every
// fixture here, before anything reaches disk.
void buildAttachedTrip(ModifiedFixture& fixture)
{
    REQUIRE(fixture.tempDir.isValid());

    fixture.rootData = std::make_unique<cwRootData>();
    // commitAll needs an account, or saveAs never creates a HEAD commit and
    // discardChanges later fails with "revspec 'HEAD' not found".
    fixture.rootData->account()->setName(QStringLiteral("Modified Tester"));
    fixture.rootData->account()->setEmail(QStringLiteral("modified.tester@example.com"));

    fixture.project = fixture.rootData->project();

    cwCave* cave = addEmptyCave(*fixture.project->cavingRegion(),
                                QStringLiteral("Modified"));
    fixture.trip = addAttachedTrip(cave,
                                   QStringLiteral("Attached"),
                                   QStringLiteral("entry.svx"));
}

// Writes the attachment's entry file into the dir the data root implies.
// Called before the manager arms the watcher, so this seeding is not itself
// an edit the test would then have to account for.
void seedEntryFile(ModifiedFixture& fixture)
{
    const QString attachmentDir =
        fixture.saveLoad()->externalCenterlineDir(fixture.trip).absolutePath();
    REQUIRE(QDir().mkpath(attachmentDir));
    fixture.entryPath = QDir(attachmentDir).filePath(QStringLiteral("entry.svx"));
    overwriteFile(fixture.entryPath, kOriginalEntry);
}

// A saved project holding one trip whose attachment entry file is on disk
// and watched, sitting at a clean modified() baseline. `extension` picks the
// project format: "cwproj" for the Git-backed directory, "cw" for the
// bundled archive, whose working tree is only re-zipped on save.
std::unique_ptr<ModifiedFixture> makeFixture(cwLinePlotManager& manager,
                                             const QString& extension)
{
    auto fixture = std::make_unique<ModifiedFixture>();
    buildAttachedTrip(*fixture);

    const QString projectPath = QDir(fixture->tempDir.path())
        .filePath(QStringLiteral("modified-%1.%2")
                      .arg(QCoreApplication::applicationPid())
                      .arg(extension));
    REQUIRE(fixture->project->saveAs(projectPath));
    drainSave(*fixture);

    seedEntryFile(*fixture);

    manager.externalCenterlineManager()->setSaveLoad(fixture->saveLoad());
    manager.setRegion(fixture->project->cavingRegion());
    manager.waitToFinish();

    // The scan writes the harvested station names onto the trip, which is a
    // model mutation like any other — so the clean baseline has to be taken
    // after a save that follows the first scan, not after saveAs alone.
    fixture->project->save();
    drainSave(*fixture);
    REQUIRE_FALSE(fixture->project->modified());

    return fixture;
}

} // namespace

TEST_CASE("Editing an in-project attachment marks the project modified",
          "[ExternalCenterline][Modified]")
{
    QString extension = QStringLiteral("cwproj");
    auto expectedType = cwProject::GitFileType;

    SECTION("directory .cwproj") { }

    SECTION("bundled .cw") {
        // The format that loses the edit outright: the working tree is a
        // scratch copy only re-zipped on save, so an unprompted quit drops it.
        extension = QStringLiteral("cw");
        expectedType = cwProject::BundledGitFileType;
    }

    cwLinePlotManager manager;
    auto fixture = makeFixture(manager, extension);
    REQUIRE(fixture->project->fileType() == expectedType);

    overwriteFile(fixture->entryPath, kEditedEntry);

    CHECK(tryWait(kWatcherWaitMs, [&] {
        QCoreApplication::processEvents(QEventLoop::AllEvents, kInnerPollEventsMs);
        return fixture->project->modified();
    }));
}

TEST_CASE("Editing the source outside the project leaves the project unmodified",
          "[ExternalCenterline][Modified]")
{
    // The asymmetry the fix has to keep: the source is the user's own file
    // outside the project, and nothing that travels with the project changed
    // when it did. It raises the stale flag and waits for an Update.
    cwLinePlotManager manager;
    auto fixture = makeFixture(manager, QStringLiteral("cwproj"));

    const QString sourcePath =
        QDir(fixture->tempDir.path()).filePath(QStringLiteral("source-entry.svx"));
    overwriteFile(sourcePath, kOriginalEntry);

    auto* settings = fixture->rootData->externalSourceSettings();
    settings->setSourcePath(fixture->trip->id(), sourcePath);
    auto* external = manager.externalCenterlineManager();
    external->setExternalSourceSettings(settings);
    manager.waitToFinish();

    overwriteFile(sourcePath, kEditedEntry);

    REQUIRE(tryWait(kWatcherWaitMs, [&] {
        QCoreApplication::processEvents(QEventLoop::AllEvents, kInnerPollEventsMs);
        return external->staleSourceOwners().contains(fixture->trip->id());
    }));
    CHECK_FALSE(fixture->project->modified());

    settings->setSourcePath(fixture->trip->id(), QString());
}

TEST_CASE("Discarding changes does not re-dirty the project through the watcher",
          "[ExternalCenterline][Modified]")
{
    // git reset --hard rewrites the same watched files a user's editor
    // would. Those events arrive after discardCompleted has already cleared
    // the dirty bit, so an unguarded emit would undo the discard's own work.
    cwLinePlotManager manager;
    auto fixture = makeFixture(manager, QStringLiteral("cwproj"));
    REQUIRE(fixture->project->fileType() == cwProject::GitFileType);

    // Station counts the harvest reports for each of the two entry files.
    // The difference is the observable that says the watcher event was
    // delivered and the rescan applied, rather than asserting into a race.
    constexpr int kOriginalStationCount = 2;
    constexpr int kEditedStationCount = 3;
    const auto waitForStationCount = [&](int count) {
        return tryWait(kWatcherWaitMs, [&] {
            QCoreApplication::processEvents(QEventLoop::AllEvents, kInnerPollEventsMs);
            return fixture->trip->externalStations().size() == count;
        });
    };

    REQUIRE(fixture->trip->externalStations().size() == kOriginalStationCount);

    overwriteFile(fixture->entryPath, kEditedEntry);
    REQUIRE(waitForStationCount(kEditedStationCount));
    REQUIRE(fixture->project->modified());

    // Drain the metadata write the harvest enqueued, so its deferred
    // completes before discardChanges replaces the pending-jobs deferred
    // (same hazard as the cwLazLayer discard test).
    drainSave(*fixture);

    fixture->project->discardChanges();
    fixture->project->waitForDiscardToFinish();
    QCoreApplication::processEvents();

    // The reset puts the original entry back and the harvest drops the leg
    // again — which is the manager telling us it processed the watcher event
    // the discard produced. Only then is the dirty bit worth reading.
    REQUIRE(waitForStationCount(kOriginalStationCount));
    CHECK(fileContents(fixture->entryPath) == kOriginalEntry);
    CHECK_FALSE(fixture->project->modified());
}

TEST_CASE("An Update leaves nothing behind to save once it has been saved",
          "[ExternalCenterline][Modified]")
{
    // updateFromSource writes into the project through cwSaveLoad, which marks
    // the project modified as it queues the copy — so by the time the copy
    // lands the dirty bit has already done its job. The watcher is armed on
    // that same destination and reads CaveWhere's own write exactly as it
    // reads a user's edit, so unless the copy opens the self-write window on
    // its way out, its event dirties the project a second time. Arriving after
    // the save that followed the Update, that leaves the project asking to be
    // saved again with nothing left to write.
    //
    // Whether the event lands before or after that save is the OS's business,
    // so in principle this samples the echo rather than pinning it. In
    // practice the event has always come after: removing the guard fails this
    // on every run, while the temporary-project test below catches its own
    // subject roughly half the time.
    cwLinePlotManager manager;
    auto fixture = makeFixture(manager, QStringLiteral("cwproj"));

    // The source carries the entry file's name so the copy lands on the
    // watched destination rather than beside it.
    const QString sourceDir =
        QDir(fixture->tempDir.path()).filePath(QStringLiteral("source"));
    REQUIRE(QDir().mkpath(sourceDir));
    const QString sourcePath = QDir(sourceDir).filePath(QStringLiteral("entry.svx"));
    overwriteFile(sourcePath, kEditedEntry);

    auto* settings = fixture->rootData->externalSourceSettings();
    settings->setSourcePath(fixture->trip->id(), sourcePath);
    auto* external = manager.externalCenterlineManager();
    external->setExternalSourceSettings(settings);
    manager.waitToFinish();

    external->updateFromSource(fixture->trip->id());

    // The extra leg arrives with the copied bytes, so the harvest reporting
    // three stations is the reconcile having reached disk.
    constexpr int kEditedStationCount = 3;
    REQUIRE(tryWait(kWatcherWaitMs, [&] {
        QCoreApplication::processEvents(QEventLoop::AllEvents, kInnerPollEventsMs);
        return fixture->trip->externalStations().size() == kEditedStationCount;
    }));
    CHECK(fileContents(fixture->entryPath) == kEditedEntry);

    fixture->project->save();
    drainSave(*fixture);
    REQUIRE_FALSE(fixture->project->modified());

    // Long enough for the copy's watcher event to have been delivered and
    // handled, so a surviving echo has had its chance to land.
    constexpr int kEchoSettleMs = 1000;
    QTest::qWait(kEchoSettleMs);
    CHECK_FALSE(fixture->project->modified());

    settings->setSourcePath(fixture->trip->id(), QString());
}

TEST_CASE("Renaming the region keeps the attachment watched once the data root moves",
          "[ExternalCenterline][Modified]")
{
    // The other move: renaming the region renames the data root directory,
    // taking every attachment with it.
    //
    // cwSaveLoad updates its in-memory dataRoot and emits dataRootChanged
    // straight away, then only *queues* the directory move behind whatever
    // writes are already pending. On an idle project the move still wins,
    // which is why this goes unnoticed; on a busy one the recompute that
    // emit kicks off gets there first, derives its dirs from the new root
    // while the tree is still at the old path, finds no entry file, and
    // installs an empty watch set. Nothing re-triggers when the move finally
    // lands, so the attachment stays unwatched for the rest of the session:
    // the exact failure the dataRootChanged connection exists to prevent.
    cwLinePlotManager manager;
    auto fixture = makeFixture(manager, QStringLiteral("cwproj"));

    auto* external = manager.externalCenterlineManager();
    REQUIRE(external->watchedFiles().contains(
        QFileInfo(fixture->entryPath).canonicalFilePath()));

    // Add a trip, then rename — an ordinary pair of edits, and CaveWhere
    // drains nothing between them. That leaves the model busy in both ways
    // that matter when the rename lands: write jobs already queued ahead of
    // the directory move, and a recompute already in flight.
    cwCave* cave = fixture->project->cavingRegion()->cave(0);
    cave->addTrip();
    cave->trip(cave->tripCount() - 1)->setName(QStringLiteral("Second"));

    fixture->project->cavingRegion()->setName(QStringLiteral("Renamed Region"));
    drainSave(*fixture);

    // Where the entry file lands once the queued move has run. Waiting on the
    // file itself keeps the watch-set assertion below from reading before the
    // rename has happened at all.
    const QString movedEntry = QDir(
        fixture->saveLoad()->externalCenterlineDir(fixture->trip).absolutePath())
        .filePath(QStringLiteral("entry.svx"));
    REQUIRE(movedEntry != fixture->entryPath);
    REQUIRE(tryWait(kWatcherWaitMs, [&] {
        QCoreApplication::processEvents(QEventLoop::AllEvents, kInnerPollEventsMs);
        return QFileInfo::exists(movedEntry);
    }));

    CHECK(tryWait(kWatcherWaitMs, [&] {
        QCoreApplication::processEvents(QEventLoop::AllEvents, kInnerPollEventsMs);
        return external->watchedFiles().contains(
            QFileInfo(movedEntry).canonicalFilePath());
    }));
}

TEST_CASE("The first save of a temporary project does not re-dirty it through the watcher",
          "[ExternalCenterline][Modified]")
{
    // The one Save As that moves rather than copies: a temporary project's
    // first one relocates the whole root out from under every watched file.
    // The watcher reports those as changed on the way out — but only
    // sometimes, and whether it does is the OS's business. So this catches
    // an unfiltered handler on roughly half of runs rather than every one;
    // it is a sampling detector, not a pin.
    ModifiedFixture fixture;
    buildAttachedTrip(fixture);
    REQUIRE(fixture.project->isTemporaryProject());

    // Seeded and armed before the first save — the ordering is the point of
    // the test, and it is what puts the files under watch in time to see the
    // move at all.
    seedEntryFile(fixture);

    cwLinePlotManager manager;
    manager.externalCenterlineManager()->setSaveLoad(fixture.saveLoad());
    manager.setRegion(fixture.project->cavingRegion());
    manager.waitToFinish();
    REQUIRE(manager.externalCenterlineManager()->watchedFiles().contains(
        QFileInfo(fixture.entryPath).canonicalFilePath()));

    QTemporaryDir destinationRoot;
    REQUIRE(destinationRoot.isValid());
    const QString destination = QDir(destinationRoot.path())
        .filePath(QStringLiteral("moved-%1.cwproj").arg(QCoreApplication::applicationPid()));

    const QString entryBeforeMove = fixture.entryPath;
    REQUIRE(fixture.project->saveAs(destination));
    drainSave(fixture);
    REQUIRE_FALSE(QFile::exists(entryBeforeMove));

    // The move re-arms the watch set on the new location, which is the one
    // deterministic marker that the whole post-move recompute has run — the
    // stale events either arrived before it or never come at all.
    const QString movedEntry = QDir(
        fixture.saveLoad()->externalCenterlineDir(fixture.trip).absolutePath())
        .filePath(QStringLiteral("entry.svx"));
    REQUIRE(tryWait(kWatcherWaitMs, [&] {
        QCoreApplication::processEvents(QEventLoop::AllEvents, kInnerPollEventsMs);
        return manager.externalCenterlineManager()->watchedFiles().contains(
            QFileInfo(movedEntry).canonicalFilePath());
    }));

    // Long enough for a stale event and the recompute it schedules to have
    // landed, so "nothing happened" is not just a race the test won by
    // looking too early.
    constexpr int kNothingHappenedSettleMs = 1000;
    QTest::qWait(kNothingHappenedSettleMs);
    CHECK_FALSE(fixture.project->modified());
}
