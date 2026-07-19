/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Cavewhere
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwExternalCenterline.h"
#include "cwFutureManagerModel.h"
#include "cwLinePlotManager.h"
#include "cwExternalSourceSettings.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSaveLoad.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwStationPositionLookup.h"
#include "cwSurveyChunk.h"
#include "cwTrip.h"

// Tests
#include "ExternalCenterlineTestHelpers.h"
#include "LoadProjectHelper.h"
#include "cwSignalSpy.h"

// Qt
#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>

// Std
#include <functional>
#include <memory>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QString>
#include <QTemporaryDir>
#include <QUuid>

namespace {

// survex_simple.svx + an appended extra shot pulling the chain out to a4.
// The new station name is asserted against the cave's position lookup
// after the watcher fires.
// Mirrors the survex_simple.svx layout (self-fixing at A1 inside *begin
// Simple) and extends it with one more shot to A4. The watcher test
// verifies A4 appears in the lookup after the edit lands.
QByteArray simpleSvxWithExtraShot()
{
    return QByteArrayLiteral(
        "; extended for watcher test\n"
        "*begin Simple\n"
        "*fix A1 0 0 0\n"
        "*data normal from to tape compass clino\n"
        "A1 A2 10.0 0 0\n"
        "A2 A3 8.5 90 0\n"
        "A3 A4 5.0 0 0\n"
        "*end Simple\n");
}

} // namespace

TEST_CASE("Watcher set contains the in-project copy paths after attach",
          "[Attach][Watcher]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    const QString attachDir = tempSubdir(tempRoot, QStringLiteral("watcher-trip"));
    const QString copyPath = seedAttachment(attachDir,
                                            fixturePath(QStringLiteral("survex_simple.svx")));

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Watch"));
    cwTrip* attached = addEmptyTrip(cave, QStringLiteral("Attached"));
    attached->setExternalCenterline(cwExternalCenterline(QStringLiteral("survex_simple.svx")));

    cwLinePlotManager manager;
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(attached->id(), attachDir);
    manager.setTripAttachmentDirs(tripDirs);
    manager.setRegion(&region);
    manager.waitToFinish();

    // The expected path is the canonical absolute form of the in-project
    // copy - this is what the scanner emits and what recompute installs.
    const QString expected = QFileInfo(copyPath).canonicalFilePath();
    INFO("expected watch path: " << expected.toStdString());
    REQUIRE_FALSE(expected.isEmpty());
    CHECK(manager.watchedFiles().contains(expected));
    // Watch set on a project-side-only attach has no source-side entries
    // until live-link is configured.
    CHECK(manager.missingSourceOwners().isEmpty());
}

TEST_CASE("Editing the in-project copy triggers a re-run that picks up the new station",
          "[Attach][Watcher]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    const QString attachDir = tempSubdir(tempRoot, QStringLiteral("watcher-edit"));
    const QString copyPath = seedAttachment(attachDir,
                                            fixturePath(QStringLiteral("survex_simple.svx")));

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Watch"));
    cwTrip* attached = addEmptyTrip(cave, QStringLiteral("Attached"));
    attached->setExternalCenterline(cwExternalCenterline(QStringLiteral("survex_simple.svx")));

    cwLinePlotManager manager;
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(attached->id(), attachDir);
    manager.setTripAttachmentDirs(tripDirs);
    manager.setRegion(&region);
    manager.waitToFinish();

    const QString tripPrefix = QStringLiteral("trip_%1").arg(attached->id().toString(QUuid::Id128));
    const QString a3Key = tripPrefix + QStringLiteral(".simple.a3");
    const QString a4Key = tripPrefix + QStringLiteral(".simple.a4");
    REQUIRE(cave->stationPositionLookup().hasPosition(a3Key));
    REQUIRE_FALSE(cave->stationPositionLookup().hasPosition(a4Key));

    // Now overwrite the in-project copy with content that adds an a4
    // station. The watcher should fire (project-side path), recompute the
    // watch set, and trigger rerunSurvex; the next solve adds a4 to the
    // lookup.
    overwriteFile(copyPath, simpleSvxWithExtraShot());

    REQUIRE(tryWait(kWatcherWaitMs, [&] {
        QCoreApplication::processEvents(QEventLoop::AllEvents, kInnerPollEventsMs);
        return cave->stationPositionLookup().hasPosition(a4Key);
    }));
    CHECK(cave->stationPositionLookup().hasPosition(a4Key));
}

TEST_CASE("Detach mid-solve does not crash and empties the watch set",
          "[Attach][Watcher]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    const QString attachDir = tempSubdir(tempRoot, QStringLiteral("watcher-detach"));
    seedAttachment(attachDir, fixturePath(QStringLiteral("survex_simple.svx")));

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Detach"));
    cwTrip* attached = addEmptyTrip(cave, QStringLiteral("Attached"));
    attached->setExternalCenterline(cwExternalCenterline(QStringLiteral("survex_simple.svx")));

    cwLinePlotManager manager;
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(attached->id(), attachDir);
    manager.setTripAttachmentDirs(tripDirs);
    manager.setRegion(&region);

    // Wait for the initial async scan to install the watch set — its
    // apply chains the solve, so the solve is now in (or about to be in)
    // flight. Coalescing means detaching before this point would fold
    // into the initial scan and never populate the set at all.
    REQUIRE(tryWait(kWatcherWaitMs, [&] { return !manager.watchedFiles().isEmpty(); }));

    cwSignalSpy watcherSpy(&manager, &cwLinePlotManager::watchedFilesChanged);

    // Do NOT wait for the solve - this models the "detach mid-solve"
    // condition. Detach by clearing the trip's externalCenterline and
    // dropping the trip-attachment dir entry; the setter kicks off the
    // async recompute that rebuilds the watcher.
    attached->setExternalCenterline(cwExternalCenterline());
    manager.setTripAttachmentDirs(QHash<QUuid, QString>());

    // The recompute empties the watch set once its apply lands. Verify
    // the signal fired and the public accessor is empty - no need to
    // wait for the in-flight solve to finish or fail.
    REQUIRE(tryWait(kWatcherWaitMs, [&] { return manager.watchedFiles().isEmpty(); }));
    CHECK(manager.watchedFiles().isEmpty());
    CHECK(watcherSpy.count() >= 1);

    // Draining the in-flight solve must not crash or leave the manager
    // in an error state.
    manager.waitToFinish();
    CHECK(cave != nullptr); // sentinel: just make sure we got here
}

TEST_CASE("Startup probe detects a missing live-link source path",
          "[Attach][Watcher][LiveLink]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    const QString attachDir = tempSubdir(tempRoot, QStringLiteral("probe-attach"));
    seedAttachment(attachDir, fixturePath(QStringLiteral("survex_simple.svx")));

    // Plant a sourcePath in cwExternalSourceSettings that *does not* exist on disk -
    // simulates the user moving the original outside CaveWhere while the
    // project was closed. Per §8.8 question 16 the missing-source state
    // must surface at startup, not just on a watcher event.
    const QString missingSource =
        QDir(tempRoot.path()).absoluteFilePath(QStringLiteral("not-there.svx"));
    REQUIRE_FALSE(QFile::exists(missingSource));

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Probe"));
    cwTrip* attached = addEmptyTrip(cave, QStringLiteral("Attached"));
    attached->setExternalCenterline(cwExternalCenterline(QStringLiteral("survex_simple.svx")));

    cwExternalSourceSettings settings;
    settings.setSourcePath(attached->id(), missingSource);

    cwLinePlotManager manager;
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(attached->id(), attachDir);
    manager.setTripAttachmentDirs(tripDirs);
    manager.setExternalSourceSettings(&settings);

    cwSignalSpy missingSpy(&manager, &cwLinePlotManager::missingSourceOwnersChanged);

    // Now bind the region. setRegion's recompute walks the region against
    // the already-configured local settings and surfaces the missing
    // source without needing a QFileSystemWatcher event.
    manager.setRegion(&region);
    manager.waitToFinish();

    const QList<QUuid> missing = manager.missingSourceOwners();
    CHECK(missing.size() == 1);
    CHECK(missing.contains(attached->id()));
    CHECK(missingSpy.count() >= 1);
}

namespace {

struct LiveLinkFixture {
    QTemporaryDir tempDir;
    std::unique_ptr<cwRootData> rootData;
    cwProject* project = nullptr;
    cwCave* cave = nullptr;
    cwTrip* trip = nullptr;
    QString sourcePath;
    QString attachmentDir;
};

std::unique_ptr<LiveLinkFixture> makeLiveLinkFixture()
{
    auto fixture = std::make_unique<LiveLinkFixture>();
    REQUIRE(fixture->tempDir.isValid());

    fixture->rootData = std::make_unique<cwRootData>();
    fixture->project = fixture->rootData->project();

    auto region = fixture->project->cavingRegion();
    region->addCave();
    fixture->cave = region->cave(0);
    fixture->cave->setName(QStringLiteral("LiveLink"));
    fixture->cave->addTrip();
    fixture->trip = fixture->cave->trip(0);
    fixture->trip->setName(QStringLiteral("LiveTrip"));

    // Set the centerline before saveAs so the fixture settles with
    // modified() == false — the [Attach][Stale] tests baseline against
    // that before asserting updateFromSource flips it.
    fixture->trip->setExternalCenterline(cwExternalCenterline(QStringLiteral("source.svx")));

    const QString projectPath = QDir(fixture->tempDir.path())
                                    .filePath(QStringLiteral("livelink.cwproj"));
    REQUIRE(fixture->project->saveAs(projectPath));
    fixture->project->waitSaveToFinish();

    // Pick the source path next to the project temp dir, then write the
    // simple fixture contents to it. Live-link source lives outside the
    // attachment dir.
    fixture->sourcePath = QDir(fixture->tempDir.path())
                              .filePath(QStringLiteral("source.svx"));
    {
        QFile src(fixturePath(QStringLiteral("survex_simple.svx")));
        REQUIRE(src.open(QFile::ReadOnly));
        const QByteArray content = src.readAll();
        QFile dest(fixture->sourcePath);
        REQUIRE(dest.open(QFile::WriteOnly | QFile::Truncate));
        REQUIRE(dest.write(content) == content.size());
    }

    // Pre-reconcile: copy source into the attachment dir so the project
    // side has a coherent starting state (mirrors the
    // "[Attach picker → reconcile]" flow that would run for real).
    // Direct QFile ops, so the copy doesn't touch the modified bit.
    fixture->attachmentDir =
        fixture->project->saveLoad()->externalCenterlineDir(fixture->trip).absolutePath();
    REQUIRE(QDir().mkpath(fixture->attachmentDir));
    const QString destFile =
        QDir(fixture->attachmentDir).filePath(QStringLiteral("source.svx"));
    REQUIRE(QFile::copy(fixture->sourcePath, destFile));

    // Drain the queued fileSaved delivery so modified() is settled false
    // before tests take a baseline (same idiom as the attach tests).
    fixture->rootData->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();
    return fixture;
}

// Spins the event loop for `ms` wall-clock milliseconds, giving any
// (wrongly) queued reconcile/solve continuation every chance to run
// before the test asserts nothing happened.
void settleEventLoop(int ms)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, kInnerPollEventsMs);
    }
}

} // namespace

TEST_CASE("Live-link source edit flags the owner stale and never reconciles on its own",
          "[Attach][Stale]")
{
    auto fixture = makeLiveLinkFixture();

    cwExternalSourceSettings settings;
    settings.setSourcePath(fixture->trip->id(), fixture->sourcePath);

    cwLinePlotManager manager;
    manager.setSaveLoad(fixture->project->saveLoad());
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(fixture->trip->id(), fixture->attachmentDir);
    manager.setTripAttachmentDirs(tripDirs);
    manager.setExternalSourceSettings(&settings);
    manager.setRegion(fixture->project->cavingRegion());
    manager.waitToFinish();
    fixture->project->waitSaveToFinish();
    fixture->rootData->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();
    REQUIRE_FALSE(fixture->project->modified());

    // Source and in-project copy start in sync, so the recompute probe
    // must not report staleness at bind time.
    CHECK(manager.staleSourceOwners().isEmpty());

    const QString tripPrefix =
        QStringLiteral("trip_%1").arg(fixture->trip->id().toString(QUuid::Id128));
    const QString a3Key = tripPrefix + QStringLiteral(".simple.a3");
    const QString a4Key = tripPrefix + QStringLiteral(".simple.a4");
    REQUIRE(fixture->cave->stationPositionLookup().hasPosition(a3Key));
    REQUIRE_FALSE(fixture->cave->stationPositionLookup().hasPosition(a4Key));

    // Watch set should include both the in-project copy and the
    // source-side path - tryWait because the recompute is async and the
    // initial solve's continuations may still be draining.
    REQUIRE(tryWait(kWatcherWaitMs, [&] {
        const QStringList watched = manager.watchedFiles();
        const QString srcCanonical = QFileInfo(fixture->sourcePath).canonicalFilePath();
        const QString destCanonical = QFileInfo(QDir(fixture->attachmentDir)
                                                    .filePath(QStringLiteral("source.svx")))
                                          .canonicalFilePath();
        return watched.contains(srcCanonical) && watched.contains(destCanonical);
    }));

    cwSignalSpy staleSpy(&manager, &cwLinePlotManager::staleSourceOwnersChanged);

    // Edit the source side (not the project copy). Direction change: the
    // watcher event only flags the owner stale — no reconcile, no re-solve.
    overwriteFile(fixture->sourcePath, simpleSvxWithExtraShot());

    REQUIRE(tryWait(kWatcherWaitMs, [&] {
        return manager.staleSourceOwners().contains(fixture->trip->id());
    }));
    CHECK(staleSpy.count() >= 1);

    // Give any (wrongly) queued reconcile or solve continuation a chance
    // to run, then drain the save queue and solve pipeline so the
    // negative checks below can't false-pass on cross-thread latency.
    settleEventLoop(500);
    fixture->project->waitSaveToFinish();
    manager.waitToFinish();

    // Deterministic no-reconcile detector: enqueueing a reconcile copy
    // emits localMutationOccurred synchronously at enqueue time, so a
    // wrongly-triggered reconcile shows up here as modified() == true
    // regardless of save-thread timing.
    CHECK_FALSE(fixture->project->modified());
    CHECK(fileContents(QDir(fixture->attachmentDir).filePath(QStringLiteral("source.svx")))
          == fileContents(fixturePath(QStringLiteral("survex_simple.svx"))));
    CHECK_FALSE(fixture->cave->stationPositionLookup().hasPosition(a4Key));
}

TEST_CASE("updateFromSource lands the copy, re-solves, clears the flag, and marks modified",
          "[Attach][Stale]")
{
    auto fixture = makeLiveLinkFixture();

    cwExternalSourceSettings settings;
    settings.setSourcePath(fixture->trip->id(), fixture->sourcePath);

    cwLinePlotManager manager;
    manager.setSaveLoad(fixture->project->saveLoad());
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(fixture->trip->id(), fixture->attachmentDir);
    manager.setTripAttachmentDirs(tripDirs);
    manager.setExternalSourceSettings(&settings);
    manager.setRegion(fixture->project->cavingRegion());
    manager.waitToFinish();
    fixture->project->waitSaveToFinish();
    fixture->rootData->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();
    REQUIRE_FALSE(fixture->project->modified());

    const QString tripPrefix =
        QStringLiteral("trip_%1").arg(fixture->trip->id().toString(QUuid::Id128));
    const QString a4Key = tripPrefix + QStringLiteral(".simple.a4");
    REQUIRE_FALSE(fixture->cave->stationPositionLookup().hasPosition(a4Key));

    overwriteFile(fixture->sourcePath, simpleSvxWithExtraShot());
    REQUIRE(tryWait(kWatcherWaitMs, [&] {
        return manager.staleSourceOwners().contains(fixture->trip->id());
    }));

    // One copy enqueue per accepted update — the spy pins the in-flight
    // refusal below, since a second (wrongly accepted) update would plan
    // against the not-yet-copied dest and enqueue a second copy.
    cwSignalSpy mutationSpy(fixture->project->saveLoad(),
                            &cwSaveLoad::localMutationOccurred);

    manager.updateFromSource(fixture->trip->id());
    // Per-owner in-flight guard: the reconcile hasn't drained yet, and a
    // second call while it runs is refused rather than interleaved.
    CHECK(manager.isUpdateFromSourceInFlight(fixture->trip->id()));
    manager.updateFromSource(fixture->trip->id());

    REQUIRE(tryWait(kWatcherWaitMs, [&] {
        QCoreApplication::processEvents(QEventLoop::AllEvents, kInnerPollEventsMs);
        return fixture->cave->stationPositionLookup().hasPosition(a4Key);
    }));

    // The reconcile copied source-side content into the attachment dir;
    // verify by content comparison so we know the file mutation made it
    // through cwSaveLoad's job queue rather than just hitting an in-memory
    // cache.
    CHECK(fileContents(QDir(fixture->attachmentDir).filePath(QStringLiteral("source.svx")))
          == simpleSvxWithExtraShot());

    // Flag clears once the recompute re-probes the fresh copy; the copy
    // job flipped the project's modified bit; the guard released.
    REQUIRE(tryWait(kWatcherWaitMs, [&] {
        return manager.staleSourceOwners().isEmpty();
    }));
    CHECK_FALSE(manager.isUpdateFromSourceInFlight(fixture->trip->id()));
    CHECK(fixture->project->modified());
    CHECK(mutationSpy.count() == 1);
}

TEST_CASE("Open-time probe seeds staleness for a source that drifted while closed",
          "[Attach][Stale]")
{
    auto fixture = makeLiveLinkFixture();

    // The source drifts before the manager ever sees the project —
    // simulates editing the original while CaveWhere was closed.
    overwriteFile(fixture->sourcePath, simpleSvxWithExtraShot());

    cwExternalSourceSettings settings;
    settings.setSourcePath(fixture->trip->id(), fixture->sourcePath);

    cwLinePlotManager manager;
    manager.setSaveLoad(fixture->project->saveLoad());
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(fixture->trip->id(), fixture->attachmentDir);
    manager.setTripAttachmentDirs(tripDirs);
    manager.setExternalSourceSettings(&settings);
    manager.setRegion(fixture->project->cavingRegion());

    // setRegion's async recompute probes freshness — no watcher event is
    // needed to light the stale banner at open time. Drain the scan (and
    // its chained solve) before checking.
    manager.waitToFinish();

    CHECK(manager.staleSourceOwners().contains(fixture->trip->id()));
    CHECK(manager.missingSourceOwners().isEmpty());
}

TEST_CASE("Deleting the live-link source reports missing, not stale",
          "[Attach][Stale]")
{
    auto fixture = makeLiveLinkFixture();

    cwExternalSourceSettings settings;
    settings.setSourcePath(fixture->trip->id(), fixture->sourcePath);

    cwLinePlotManager manager;
    manager.setSaveLoad(fixture->project->saveLoad());
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(fixture->trip->id(), fixture->attachmentDir);
    manager.setTripAttachmentDirs(tripDirs);
    manager.setExternalSourceSettings(&settings);
    manager.setRegion(fixture->project->cavingRegion());
    manager.waitToFinish();

    REQUIRE(manager.missingSourceOwners().isEmpty());
    REQUIRE(tryWait(kWatcherWaitMs, [&] {
        return manager.watchedFiles()
            .contains(QFileInfo(fixture->sourcePath).canonicalFilePath());
    }));

    // Removing the source must reclassify the owner as missing — a stale
    // flag would offer an Update for a file that no longer exists.
    REQUIRE(QFile::remove(fixture->sourcePath));

    REQUIRE(tryWait(kWatcherWaitMs, [&] {
        return manager.missingSourceOwners().contains(fixture->trip->id());
    }));
    CHECK(manager.staleSourceOwners().isEmpty());

    manager.waitToFinish();
}
