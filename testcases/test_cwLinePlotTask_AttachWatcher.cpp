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
#include "cwLinePlotManager.h"
#include "cwLocalSettings.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSaveLoad.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwStationPositionLookup.h"
#include "cwSurveyChunk.h"
#include "cwTrip.h"

// Tests
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

// Watcher events flow through OS notifications and are delivered on the
// next event-loop spin. Tests poll the predicate, processing events each
// iteration, until either the predicate is satisfied or the budget runs
// out. Picked well above the actual settle time so a busy CI under
// valgrind still has headroom; the watcher itself fires within ~10ms.
constexpr int kWatcherWaitMs = 5000;

// Per-iteration event-loop budget inside tryWait. Long enough for one
// AsyncFuture continuation to run; short enough that the predicate
// re-evaluates promptly on flaky-CI scheduling jitter.
constexpr int kPollEventsBudgetMs = 50;

// Inside a tryWait predicate we also pump the loop once more so a freshly
// queued continuation that the outer pump just missed can land before we
// re-evaluate. Smaller than kPollEventsBudgetMs because we only need to
// drain whatever is already queued, not wait for new work.
constexpr int kInnerPollEventsMs = 10;

bool tryWait(int timeoutMs, std::function<bool()> predicate)
{
    QElapsedTimer timer;
    timer.start();
    while (!predicate()) {
        if (timer.elapsed() >= timeoutMs) {
            return predicate(); // one last evaluation under the deadline
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, kPollEventsBudgetMs);
    }
    return true;
}

QString fixturePath(const QString& name)
{
    return testcasesDatasetSourcePath(QStringLiteral("external-centerlines/%1").arg(name));
}

QString tempSubdir(const QTemporaryDir& root, const QString& name)
{
    const QDir baseDir(root.path());
    REQUIRE(baseDir.mkpath(name));
    return baseDir.absoluteFilePath(name);
}

// Copies `source` to <attachDir>/<basename(source)> with the watcher
// pattern from the existing [Attach][...] tests. Returns the destination
// path so the test can later edit it directly.
QString seedAttachment(const QString& attachDir, const QString& source)
{
    const QString basename = QFileInfo(source).fileName();
    const QString dest = QDir(attachDir).absoluteFilePath(basename);
    REQUIRE_FALSE(source.isEmpty());
    REQUIRE(QFile::exists(source));
    if (QFile::exists(dest)) {
        QFile::remove(dest);
    }
    REQUIRE(QFile::copy(source, dest));
    return dest;
}

// Atomically overwrite a watched file. QFileSystemWatcher on macOS treats
// some editors' rename-over patterns differently from in-place writes, so
// we do an in-place open-truncate-write (the same shape as a user editing
// the file in a regular editor that saves in place).
void overwriteFile(const QString& path, const QByteArray& content)
{
    QFile f(path);
    REQUIRE(f.open(QFile::WriteOnly | QFile::Truncate));
    REQUIRE(f.write(content) == content.size());
    f.close();
}

cwCave* addEmptyCave(cwCavingRegion& region, const QString& name)
{
    cwCave* cave = new cwCave();
    cave->setName(name);
    region.addCave(cave);
    return cave;
}

cwTrip* addEmptyTrip(cwCave* cave, const QString& name)
{
    cwTrip* trip = new cwTrip();
    trip->setName(name);
    cave->addTrip(trip);
    return trip;
}

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

    cwSignalSpy watcherSpy(&manager, &cwLinePlotManager::watchedFilesChanged);

    // Do NOT wait for the solve - this models the "detach mid-solve"
    // condition. Detach by clearing the trip's externalCenterline and
    // dropping the trip-attachment dir entry, then recompute via the
    // public setters so the watcher rebuild path runs immediately.
    attached->setExternalCenterline(cwExternalCenterline());
    manager.setTripAttachmentDirs(QHash<QUuid, QString>());

    // The recompute synchronously empties the watch set. Verify the
    // signal fired and the public accessor is empty - no need to wait
    // for the in-flight solve to finish or fail.
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

    // Plant a sourcePath in local_settings that *does not* exist on disk -
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

    cwLocalSettings settings;
    settings.setSourcePath(attached->id(), missingSource);

    cwLinePlotManager manager;
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(attached->id(), attachDir);
    manager.setTripAttachmentDirs(tripDirs);
    manager.setLocalSettings(settings);

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
    fixture->attachmentDir =
        fixture->project->saveLoad()->externalCenterlineDir(fixture->trip).absolutePath();
    REQUIRE(QDir().mkpath(fixture->attachmentDir));
    const QString destFile =
        QDir(fixture->attachmentDir).filePath(QStringLiteral("source.svx"));
    REQUIRE(QFile::copy(fixture->sourcePath, destFile));

    fixture->trip->setExternalCenterline(cwExternalCenterline(QStringLiteral("source.svx")));
    return fixture;
}

} // namespace

TEST_CASE("Live-link source edit fires reconcile then re-runs the solve",
          "[Attach][Watcher][LiveLink]")
{
    auto fixture = makeLiveLinkFixture();

    cwLocalSettings settings;
    settings.setSourcePath(fixture->trip->id(), fixture->sourcePath);

    cwLinePlotManager manager;
    manager.setSaveLoad(fixture->project->saveLoad());
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(fixture->trip->id(), fixture->attachmentDir);
    manager.setTripAttachmentDirs(tripDirs);
    manager.setLocalSettings(settings);
    manager.setRegion(fixture->project->cavingRegion());
    manager.waitToFinish();
    fixture->project->waitSaveToFinish();

    const QString tripPrefix =
        QStringLiteral("trip_%1").arg(fixture->trip->id().toString(QUuid::Id128));
    const QString a3Key = tripPrefix + QStringLiteral(".simple.a3");
    const QString a4Key = tripPrefix + QStringLiteral(".simple.a4");
    REQUIRE(fixture->cave->stationPositionLookup().hasPosition(a3Key));
    REQUIRE_FALSE(fixture->cave->stationPositionLookup().hasPosition(a4Key));

    // Watch set should include both the in-project copy and the
    // source-side path - tryWait because recompute runs synchronously on
    // setRegion but the test guards against any race with the initial
    // solve's continuations.
    REQUIRE(tryWait(kWatcherWaitMs, [&] {
        const QStringList watched = manager.watchedFiles();
        const QString srcCanonical = QFileInfo(fixture->sourcePath).canonicalFilePath();
        const QString destCanonical = QFileInfo(QDir(fixture->attachmentDir)
                                                    .filePath(QStringLiteral("source.svx")))
                                          .canonicalFilePath();
        return watched.contains(srcCanonical) && watched.contains(destCanonical);
    }));

    // Edit the source side (not the project copy). Reconcile should
    // bring the edit across into the attachment dir; then runSurvex
    // adds the new station.
    overwriteFile(fixture->sourcePath, simpleSvxWithExtraShot());

    REQUIRE(tryWait(kWatcherWaitMs, [&] {
        QCoreApplication::processEvents(QEventLoop::AllEvents, kInnerPollEventsMs);
        return fixture->cave->stationPositionLookup().hasPosition(a4Key);
    }));
    CHECK(fixture->cave->stationPositionLookup().hasPosition(a4Key));

    // The reconcile copied source-side content into the attachment dir;
    // verify by content comparison so we know the file mutation made it
    // through cwSaveLoad's job queue rather than just hitting an in-memory
    // cache.
    QFile destFile(QDir(fixture->attachmentDir).filePath(QStringLiteral("source.svx")));
    REQUIRE(destFile.open(QFile::ReadOnly));
    CHECK(destFile.readAll() == simpleSvxWithExtraShot());
}
