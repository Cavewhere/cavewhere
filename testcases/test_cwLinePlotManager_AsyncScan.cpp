/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Cavewhere
#include "cwAttachedCenterlinesModel.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwExternalCenterline.h"
#include "cwExternalSourceSettings.h"
#include "cwLinePlotManager.h"
#include "cwTrip.h"

// Tests
#include "ExternalCenterlineTestHelpers.h"
#include "LoadProjectHelper.h"
#include "cwSignalSpy.h"

// Qt
#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QString>
#include <QTemporaryDir>
#include <QThread>
#include <QUuid>

// Std
#include <functional>
#include <memory>

namespace {

// How long the union-gate test blocks (without processing events) after
// editing the source, so the watcher thread's queued fileChanged
// delivery is in the event queue before processing resumes. kqueue
// latency is typically single-digit milliseconds; 500 leaves valgrind
// headroom.
constexpr int kWatcherPostWindowMs = 500;

} // namespace

TEST_CASE("Attach populates the watch set only after the async scan applies",
          "[LinePlotManager][AsyncScan]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    const QString attachDir = tempSubdir(tempRoot, QStringLiteral("async-attach"));
    const QString copyPath = seedAttachment(attachDir,
                                            fixturePath(QStringLiteral("survex_simple.svx")));

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Async"));
    cwTrip* attached = addAttachedTrip(cave, QStringLiteral("Attached"));

    cwLinePlotManager manager;
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(attached->id(), attachDir);
    manager.setTripAttachmentDirs(tripDirs);
    manager.setRegion(&region);

    // Everything above ran synchronously and the scan is deferred through
    // the event loop, so nothing has applied yet — no main-thread I/O
    // happened during the recompute call itself.
    CHECK(manager.watchedFiles().isEmpty());
    CHECK(manager.attachedCenterlinesModel()->rowCount() == 0);

    // Draining yields the same result the synchronous recompute produced.
    manager.waitToFinish();
    CHECK(manager.watchedFiles().contains(QFileInfo(copyPath).canonicalFilePath()));
    const cwAttachedCenterlinesModel* model = manager.attachedCenterlinesModel();
    REQUIRE(model->rowCount() == 1);
    CHECK(roleAt(model, 0, cwAttachedCenterlinesModel::DepCountRole).toInt() == 1);
    CHECK_FALSE(manager.fileOwnsDeclination(attached->id()));
}

TEST_CASE("Setter burst coalesces into one scan and only the newest result applies",
          "[LinePlotManager][AsyncScan]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    const QString dirA = tempSubdir(tempRoot, QStringLiteral("burst-a"));
    const QString dirB = tempSubdir(tempRoot, QStringLiteral("burst-b"));
    const QString copyA = seedAttachment(dirA, fixturePath(QStringLiteral("survex_simple.svx")));
    const QString copyB = seedAttachment(dirB, fixturePath(QStringLiteral("survex_simple.svx")));

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Burst"));
    cwTrip* attached = addAttachedTrip(cave, QStringLiteral("Attached"));

    // Settle the initial scan first (no dirs yet — the watch set stays
    // empty, so watchedFilesChanged has not fired).
    cwLinePlotManager manager;
    manager.setRegion(&region);
    manager.waitToFinish();
    REQUIRE(manager.watchedFiles().isEmpty());

    cwSignalSpy watchedSpy(&manager, &cwLinePlotManager::watchedFilesChanged);

    // Two synchronous restarts in one event-loop turn: the restarter
    // coalesces them into a single scan, so the dirA state never applies.
    QHash<QUuid, QString> dirsA;
    dirsA.insert(attached->id(), dirA);
    manager.setTripAttachmentDirs(dirsA);
    QHash<QUuid, QString> dirsB;
    dirsB.insert(attached->id(), dirB);
    manager.setTripAttachmentDirs(dirsB);

    manager.waitToFinish();

    CHECK(watchedSpy.count() == 1);
    CHECK(manager.watchedFiles().contains(QFileInfo(copyB).canonicalFilePath()));
    CHECK_FALSE(manager.watchedFiles().contains(QFileInfo(copyA).canonicalFilePath()));
}

TEST_CASE("Rename re-sorts and renames rows synchronously from cached counts",
          "[LinePlotManager][AsyncScan]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    const QString alphaDir = tempSubdir(tempRoot, QStringLiteral("rename-alpha"));
    const QString zuluDir = tempSubdir(tempRoot, QStringLiteral("rename-zulu"));
    const QString alphaCopy = seedAttachment(alphaDir,
                                             fixturePath(QStringLiteral("survex_simple.svx")));
    const QString zuluCopy = seedAttachment(zuluDir,
                                            fixturePath(QStringLiteral("survex_simple.svx")));

    cwCavingRegion region;
    cwCave* alphaCave = addEmptyCave(region, QStringLiteral("Alpha"));
    cwTrip* alphaTrip = addAttachedTrip(alphaCave, QStringLiteral("TripA"));
    cwCave* zuluCave = addEmptyCave(region, QStringLiteral("Zulu"));
    cwTrip* zuluTrip = addAttachedTrip(zuluCave, QStringLiteral("TripZ"));
    Q_UNUSED(zuluCave);

    cwLinePlotManager manager;
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(alphaTrip->id(), alphaDir);
    tripDirs.insert(zuluTrip->id(), zuluDir);
    manager.setTripAttachmentDirs(tripDirs);
    manager.setRegion(&region);
    manager.waitToFinish();

    const cwAttachedCenterlinesModel* model = manager.attachedCenterlinesModel();
    REQUIRE(model->rowCount() == 2);
    REQUIRE(roleAt(model, 0, cwAttachedCenterlinesModel::OwnerNameRole).toString()
            == QStringLiteral("TripA"));
    REQUIRE(roleAt(model, 0, cwAttachedCenterlinesModel::DepCountRole).toInt() == 1);

    // Deleting the in-project copies makes a full recompute distinguishable
    // from the rows-only rebuild: a rescan would find no entry file and
    // report depCount 0. No events are processed between here and the
    // assertions, so the watcher can't sneak a recompute in.
    REQUIRE(QFile::remove(alphaCopy));
    REQUIRE(QFile::remove(zuluCopy));

    // Renaming "Alpha" past "Zulu" flips the cave-major sort. The rebuild
    // runs synchronously inside setName — no event-loop spin, no disk I/O.
    alphaCave->setName(QStringLiteral("Zz"));

    REQUIRE(model->rowCount() == 2);
    CHECK(roleAt(model, 0, cwAttachedCenterlinesModel::OwnerNameRole).toString()
          == QStringLiteral("TripZ"));
    CHECK(roleAt(model, 1, cwAttachedCenterlinesModel::OwnerNameRole).toString()
          == QStringLiteral("TripA"));
    // Counts came from the cache, not a rescan of the deleted files.
    CHECK(roleAt(model, 0, cwAttachedCenterlinesModel::DepCountRole).toInt() == 1);
    CHECK(roleAt(model, 1, cwAttachedCenterlinesModel::DepCountRole).toInt() == 1);

    manager.waitToFinish();
}

TEST_CASE("Teardown while a scan is in flight is safe",
          "[LinePlotManager][AsyncScan]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    const QString attachDir = tempSubdir(tempRoot, QStringLiteral("teardown"));
    seedAttachment(attachDir, fixturePath(QStringLiteral("survex_simple.svx")));

    SECTION("region destroyed before the scan runs") {
        auto region = std::make_unique<cwCavingRegion>();
        cwCave* cave = addEmptyCave(*region, QStringLiteral("Doomed"));
        cwTrip* attached = addAttachedTrip(cave, QStringLiteral("Attached"));

        cwLinePlotManager manager;
        QHash<QUuid, QString> tripDirs;
        tripDirs.insert(attached->id(), attachDir);
        manager.setTripAttachmentDirs(tripDirs);
        manager.setRegion(region.get());

        // The scan is queued but has not snapshotted yet; killing the
        // region now means the snapshot sees a null region and the apply
        // installs an empty result.
        region.reset();
        manager.waitToFinish();

        CHECK(manager.watchedFiles().isEmpty());
        CHECK(manager.attachedCenterlinesModel()->rowCount() == 0);
    }

    SECTION("setRegion(nullptr) supersedes the previous region's scan state") {
        cwCavingRegion region;
        cwCave* cave = addEmptyCave(region, QStringLiteral("Cleared"));
        cwTrip* attached = addAttachedTrip(cave, QStringLiteral("Attached"));

        cwLinePlotManager manager;
        QHash<QUuid, QString> tripDirs;
        tripDirs.insert(attached->id(), attachDir);
        manager.setTripAttachmentDirs(tripDirs);
        manager.setRegion(&region);
        manager.waitToFinish();
        REQUIRE_FALSE(manager.watchedFiles().isEmpty());
        REQUIRE(manager.attachedCenterlinesModel()->rowCount() == 1);

        // Nulling the region restarts the scan (superseding any in-flight
        // worker) with an empty snapshot, so the old region's watch set
        // and rows cannot outlive it.
        manager.setRegion(nullptr);
        manager.waitToFinish();

        CHECK(manager.watchedFiles().isEmpty());
        CHECK(manager.attachedCenterlinesModel()->rowCount() == 0);
    }

    SECTION("manager destroyed while the scan is queued or running") {
        cwCavingRegion region;
        cwCave* cave = addEmptyCave(region, QStringLiteral("Survivor"));
        cwTrip* attached = addAttachedTrip(cave, QStringLiteral("Attached"));

        auto manager = std::make_unique<cwLinePlotManager>();
        QHash<QUuid, QString> tripDirs;
        tripDirs.insert(attached->id(), attachDir);
        manager->setTripAttachmentDirs(tripDirs);
        manager->setRegion(&region);

        // Destructor cancels both restarters; the worker's result (if it
        // ever ran) is dropped by the context auto-disconnect.
        manager.reset();
        CHECK(true); // sentinel: reached teardown without crashing
    }
}

TEST_CASE("Source edit during an in-flight scan survives the apply-time union",
          "[LinePlotManager][AsyncScan]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    // Live-link layout without a project: source outside, in-project copy
    // inside the attachment dir, both starting in sync.
    const QString sourcePath = QDir(tempRoot.path()).filePath(QStringLiteral("source.svx"));
    REQUIRE(QFile::copy(fixturePath(QStringLiteral("survex_simple.svx")), sourcePath));
    const QString attachDir = tempSubdir(tempRoot, QStringLiteral("union-attach"));
    const QString destPath = QDir(attachDir).filePath(QStringLiteral("source.svx"));
    REQUIRE(QFile::copy(sourcePath, destPath));

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Union"));
    cwTrip* trip = new cwTrip();
    trip->setName(QStringLiteral("Linked"));
    cave->addTrip(trip);
    trip->setExternalCenterline(cwExternalCenterline(QStringLiteral("source.svx")));

    cwExternalSourceSettings settings;
    settings.setSourcePath(trip->id(), sourcePath);

    cwLinePlotManager manager;
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(trip->id(), attachDir);
    manager.setTripAttachmentDirs(tripDirs);
    manager.setExternalSourceSettings(&settings);
    manager.setRegion(&region);
    manager.waitToFinish();
    REQUIRE(manager.staleSourceOwners().isEmpty());
    REQUIRE(manager.watchedFiles().contains(QFileInfo(sourcePath).canonicalFilePath()));

    // Kick off a scan FIRST — its queued snapshot runs before the watcher
    // event below can be delivered, so the flag the event raises lands in
    // the since-snapshot set while the worker probes the disk.
    manager.setTripAttachmentDirs(tripDirs);

    // Same-size edit with a backdated mtime: computePlan's size+mtime
    // heuristic reads this as "in sync", so the probe alone can never
    // flag the owner — only the apply-time union with the watcher flag
    // keeps it stale (the commit-8 merge-policy gate).
    const QByteArray original = fileContents(sourcePath);
    QByteArray flipped = original;
    flipped.replace("8.5", "8.6");
    REQUIRE(flipped != original);
    REQUIRE(flipped.size() == original.size());
    overwriteFile(sourcePath, flipped);
    {
        const QDateTime destTime = QFileInfo(destPath).lastModified();
        QFile sourceFile(sourcePath);
        REQUIRE(sourceFile.open(QFile::ReadWrite));
        REQUIRE(sourceFile.setFileTime(destTime.addSecs(-60),
                                       QFileDevice::FileModificationTime));
        sourceFile.close();
    }

    // Block WITHOUT processing events: the watcher thread posts its
    // fileChanged delivery into the main-thread queue during this sleep,
    // landing behind the already-queued scan start and ahead of the
    // worker's apply (which can only be posted after the scan start
    // executes). Processing then runs snapshot-clear → watcher flag →
    // apply — the exact ordering the union must survive. Without the
    // sleep the sub-millisecond worker usually applies before the OS
    // event arrives and the final check would pass on the fast path
    // alone, never exercising the union.
    QThread::msleep(kWatcherPostWindowMs);

    REQUIRE(tryWait(kWatcherWaitMs, [&] {
        return manager.staleSourceOwners().contains(trip->id());
    }));

    // Drain the scan and the chained solve: the apply must have unioned
    // the watcher-raised flag back in rather than trusting the probe.
    manager.waitToFinish();
    CHECK(manager.staleSourceOwners().contains(trip->id()));
    CHECK(manager.missingSourceOwners().isEmpty());
}
