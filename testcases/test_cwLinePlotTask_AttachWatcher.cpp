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
#include "cwExternalCenterlineManager.h"
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
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QString>
#include <QTemporaryDir>
#include <QUuid>

// Std
#include <memory>

namespace {

// Mirrors the survex_simple.svx layout (self-fixing at A1 inside *begin
// Simple) and extends it with one more shot to A4. The watcher tests
// assert A4 appears in the cave's position lookup once the edit lands.
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
    manager.externalCenterlineManager()->setTripAttachmentDirs(tripDirs);
    manager.setRegion(&region);
    manager.waitToFinish();

    // The expected path is the canonical absolute form of the in-project
    // copy - this is what the scanner emits and what recompute installs.
    const QString expected = QFileInfo(copyPath).canonicalFilePath();
    INFO("expected watch path: " << expected.toStdString());
    REQUIRE_FALSE(expected.isEmpty());
    CHECK(manager.externalCenterlineManager()->watchedFiles().contains(expected));
    // No remembered source, so nothing can be reported missing.
    CHECK(manager.externalCenterlineManager()->missingSourceOwners().isEmpty());
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
    manager.externalCenterlineManager()->setTripAttachmentDirs(tripDirs);
    manager.setRegion(&region);
    manager.waitToFinish();

    const QString tripPrefix = tripScopeLabel(attached);
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
    manager.externalCenterlineManager()->setTripAttachmentDirs(tripDirs);
    manager.setRegion(&region);

    // Wait for the initial async scan to install the watch set — its
    // apply chains the solve, so the solve is now in (or about to be in)
    // flight. Coalescing means detaching before this point would fold
    // into the initial scan and never populate the set at all.
    REQUIRE(tryWait(kWatcherWaitMs, [&] { return !manager.externalCenterlineManager()->watchedFiles().isEmpty(); }));

    cwSignalSpy watcherSpy(manager.externalCenterlineManager(), &cwExternalCenterlineManager::watchedFilesChanged);

    // Do NOT wait for the solve - this models the "detach mid-solve"
    // condition. Detach by clearing the trip's externalCenterline and
    // dropping the trip-attachment dir entry; the setter kicks off the
    // async recompute that rebuilds the watcher.
    attached->setExternalCenterline(cwExternalCenterline());
    manager.externalCenterlineManager()->setTripAttachmentDirs(QHash<QUuid, QString>());

    // The recompute empties the watch set once its apply lands. Verify
    // the signal fired and the public accessor is empty - no need to
    // wait for the in-flight solve to finish or fail.
    REQUIRE(tryWait(kWatcherWaitMs, [&] { return manager.externalCenterlineManager()->watchedFiles().isEmpty(); }));
    CHECK(manager.externalCenterlineManager()->watchedFiles().isEmpty());
    CHECK(watcherSpy.count() >= 1);

    // Draining the in-flight solve must not crash or leave the manager
    // in an error state.
    manager.waitToFinish();
    CHECK(cave != nullptr); // sentinel: just make sure we got here
}

TEST_CASE("Startup probe detects a missing remembered source path",
          "[Attach][Watcher]")
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
    manager.externalCenterlineManager()->setTripAttachmentDirs(tripDirs);
    manager.externalCenterlineManager()->setExternalSourceSettings(&settings);

    cwSignalSpy missingSpy(manager.externalCenterlineManager(), &cwExternalCenterlineManager::missingSourceOwnersChanged);

    // Now bind the region. setRegion's recompute walks the region against
    // the already-configured local settings and surfaces the missing
    // source without needing a QFileSystemWatcher event.
    manager.setRegion(&region);
    manager.waitToFinish();

    const QList<QUuid> missing = manager.externalCenterlineManager()->missingSourceOwners();
    CHECK(missing.size() == 1);
    CHECK(missing.contains(attached->id()));
    CHECK(missingSpy.count() >= 1);
}

namespace {

struct RememberedSourceFixture {
    QTemporaryDir tempDir;
    std::unique_ptr<cwRootData> rootData;
    cwProject* project = nullptr;
    cwCave* cave = nullptr;
    cwTrip* trip = nullptr;
    QString sourcePath;
    QString attachmentDir;
};

std::unique_ptr<RememberedSourceFixture> makeRememberedSourceFixture()
{
    auto fixture = std::make_unique<RememberedSourceFixture>();
    REQUIRE(fixture->tempDir.isValid());

    fixture->rootData = std::make_unique<cwRootData>();
    fixture->project = fixture->rootData->project();

    // The centerline is set before saveAs so the fixture settles with
    // modified() == false, which the gate test baselines against.
    fixture->cave = addEmptyCave(*fixture->project->cavingRegion(),
                                 QStringLiteral("Remembered"));
    fixture->trip = addAttachedTrip(fixture->cave,
                                    QStringLiteral("SourceTrip"),
                                    QStringLiteral("source.svx"));

    const QString projectPath = QDir(fixture->tempDir.path())
                                    .filePath(QStringLiteral("remembered.cwproj"));
    REQUIRE(fixture->project->saveAs(projectPath));
    fixture->project->waitSaveToFinish();

    // Pick the source path next to the project temp dir, then write the
    // simple fixture contents to it. The remembered source lives outside
    // the attachment dir.
    fixture->sourcePath = QDir(fixture->tempDir.path())
                              .filePath(QStringLiteral("source.svx"));
    overwriteFile(fixture->sourcePath,
                  fileContents(fixturePath(QStringLiteral("survex_simple.svx"))));

    // Pre-reconcile: copy source into the attachment dir so the project
    // side has a coherent starting state (mirrors the
    // "[Attach picker → reconcile]" flow that would run for real).
    // Direct QFile ops, so the copy doesn't touch the modified bit.
    fixture->attachmentDir =
        fixture->project->saveLoad()->externalCenterlineDir(fixture->trip).absolutePath();
    REQUIRE(QDir().mkpath(fixture->attachmentDir));
    seedAttachment(fixture->attachmentDir, fixture->sourcePath);

    // Drain the queued fileSaved delivery so modified() is settled false
    // before tests take a baseline (same idiom as the attach tests).
    fixture->rootData->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();
    return fixture;
}

} // namespace

TEST_CASE("Editing a remembered source file changes nothing observable",
          "[Attach][Watcher]")
{
    // The retirement gate (plans/EXTERNAL_FILE_LIVE_LINK_RETIREMENT.html
    // §6, commit 2): the in-project copy is the only file the project
    // reads, so the file it was picked from is the user's own business.
    // Editing it must not be watched, must not re-solve, must not touch
    // the copy, and must not dirty the project.
    auto fixture = makeRememberedSourceFixture();

    cwExternalSourceSettings settings;
    settings.setSourcePath(fixture->trip->id(), fixture->sourcePath);

    cwLinePlotManager manager;
    manager.externalCenterlineManager()->setSaveLoad(fixture->project->saveLoad());
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(fixture->trip->id(), fixture->attachmentDir);
    manager.externalCenterlineManager()->setTripAttachmentDirs(tripDirs);
    manager.externalCenterlineManager()->setExternalSourceSettings(&settings);
    manager.setRegion(fixture->project->cavingRegion());
    manager.waitToFinish();
    fixture->project->waitSaveToFinish();
    fixture->rootData->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();
    REQUIRE_FALSE(fixture->project->modified());

    const QString tripPrefix = tripScopeLabel(fixture->trip);
    const QString a3Key = tripPrefix + QStringLiteral(".simple.a3");
    const QString a4Key = tripPrefix + QStringLiteral(".simple.a4");
    REQUIRE(fixture->cave->stationPositionLookup().hasPosition(a3Key));
    REQUIRE_FALSE(fixture->cave->stationPositionLookup().hasPosition(a4Key));

    const QString destPath =
        QDir(fixture->attachmentDir).filePath(QStringLiteral("source.svx"));
    const QString sourceCanonical = QFileInfo(fixture->sourcePath).canonicalFilePath();

    // The in-project copy is watched; the file it came from is not.
    // tryWait because the recompute is async and the initial solve's
    // continuations may still be draining.
    REQUIRE(tryWait(kWatcherWaitMs, [&] {
        return manager.externalCenterlineManager()->watchedFiles()
            .contains(QFileInfo(destPath).canonicalFilePath());
    }));
    const QStringList watchedBefore =
        manager.externalCenterlineManager()->watchedFiles();
    CHECK_FALSE(watchedBefore.contains(sourceCanonical));

    cwSignalSpy watchedSpy(manager.externalCenterlineManager(),
                           &cwExternalCenterlineManager::watchedFilesChanged);

    overwriteFile(fixture->sourcePath, simpleSvxWithExtraShot());

    // Nothing is expected to happen, so the test has to wait out the
    // window in which it would have: long enough for a watcher event to
    // have been delivered and any queued reconcile or solve to have run.
    settleEventLoop(kNothingHappensSettleMs);
    fixture->project->waitSaveToFinish();
    manager.waitToFinish();

    CHECK(watchedSpy.count() == 0);
    CHECK(manager.externalCenterlineManager()->watchedFiles() == watchedBefore);

    // Enqueueing a reconcile copy emits localMutationOccurred synchronously,
    // so a wrongly-triggered reconcile shows up as modified() regardless of
    // save-thread timing.
    CHECK_FALSE(fixture->project->modified());
    CHECK(fileContents(destPath) == fileContents(fixturePath(QStringLiteral("survex_simple.svx"))));
    CHECK_FALSE(fixture->cave->stationPositionLookup().hasPosition(a4Key));
}
