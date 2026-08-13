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
#include "cwExternalCenterlineManager.h"
#include "cwLinePlotManager.h"
#include "cwTrip.h"

// Tests
#include "ExternalCenterlineTestHelpers.h"
#include "LoadProjectHelper.h"
#include "cwSignalSpy.h"

// Qt
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QString>
#include <QTemporaryDir>
#include <QUuid>

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
    manager.externalCenterlineManager()->setTripAttachmentDirs(tripDirs);
    manager.setRegion(&region);

    // Everything above ran synchronously and the scan is deferred through
    // the event loop, so nothing has applied yet — no main-thread I/O
    // happened during the recompute call itself.
    CHECK(manager.externalCenterlineManager()->watchedFiles().isEmpty());
    CHECK(manager.externalCenterlineManager()->attachedCenterlinesModel()->rowCount() == 0);

    // Draining yields the same result the synchronous recompute produced.
    manager.waitToFinish();
    CHECK(manager.externalCenterlineManager()->watchedFiles().contains(QFileInfo(copyPath).canonicalFilePath()));
    const cwAttachedCenterlinesModel* model = manager.externalCenterlineManager()->attachedCenterlinesModel();
    REQUIRE(model->rowCount() == 1);
    CHECK(roleAt(model, 0, cwAttachedCenterlinesModel::DepCountRole).toInt() == 1);
    CHECK_FALSE(manager.externalCenterlineManager()->fileOwnsDeclination(attached->id()));
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
    REQUIRE(manager.externalCenterlineManager()->watchedFiles().isEmpty());

    cwSignalSpy watchedSpy(manager.externalCenterlineManager(), &cwExternalCenterlineManager::watchedFilesChanged);

    // Two synchronous restarts in one event-loop turn: the restarter
    // coalesces them into a single scan, so the dirA state never applies.
    QHash<QUuid, QString> dirsA;
    dirsA.insert(attached->id(), dirA);
    manager.externalCenterlineManager()->setTripAttachmentDirs(dirsA);
    QHash<QUuid, QString> dirsB;
    dirsB.insert(attached->id(), dirB);
    manager.externalCenterlineManager()->setTripAttachmentDirs(dirsB);

    manager.waitToFinish();

    CHECK(watchedSpy.count() == 1);
    CHECK(manager.externalCenterlineManager()->watchedFiles().contains(QFileInfo(copyB).canonicalFilePath()));
    CHECK_FALSE(manager.externalCenterlineManager()->watchedFiles().contains(QFileInfo(copyA).canonicalFilePath()));
}

TEST_CASE("Restart during an in-flight scan converges to the newest attachment dirs",
          "[LinePlotManager][AsyncScan]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    const QString dirA = tempSubdir(tempRoot, QStringLiteral("inflight-a"));
    const QString dirB = tempSubdir(tempRoot, QStringLiteral("inflight-b"));
    const QString copyA = seedAttachment(dirA, fixturePath(QStringLiteral("survex_simple.svx")));
    const QString copyB = seedAttachment(dirB, fixturePath(QStringLiteral("survex_simple.svx")));

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("InFlight"));
    cwTrip* attached = addAttachedTrip(cave, QStringLiteral("Attached"));

    cwLinePlotManager manager;
    manager.setRegion(&region);
    manager.waitToFinish();

    QHash<QUuid, QString> dirsA;
    dirsA.insert(attached->id(), dirA);
    manager.externalCenterlineManager()->setTripAttachmentDirs(dirsA);

    // Let the queued start fire so the worker is actually running, then
    // supersede it. The worker abandons its remaining owners as soon as it
    // sees the cancel; whatever it had built by then must stay invisible.
    QCoreApplication::processEvents();

    QHash<QUuid, QString> dirsB;
    dirsB.insert(attached->id(), dirB);
    manager.externalCenterlineManager()->setTripAttachmentDirs(dirsB);

    manager.waitToFinish();

    const cwExternalCenterlineManager* external = manager.externalCenterlineManager();
    CHECK(external->watchedFiles().contains(QFileInfo(copyB).canonicalFilePath()));
    CHECK_FALSE(external->watchedFiles().contains(QFileInfo(copyA).canonicalFilePath()));
    REQUIRE(external->attachedCenterlinesModel()->rowCount() == 1);
    CHECK(roleAt(external->attachedCenterlinesModel(), 0,
                 cwAttachedCenterlinesModel::DepCountRole).toInt() == 1);
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
    manager.externalCenterlineManager()->setTripAttachmentDirs(tripDirs);
    manager.setRegion(&region);
    manager.waitToFinish();

    const cwAttachedCenterlinesModel* model = manager.externalCenterlineManager()->attachedCenterlinesModel();
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
        manager.externalCenterlineManager()->setTripAttachmentDirs(tripDirs);
        manager.setRegion(region.get());

        // The scan is queued but has not snapshotted yet; killing the
        // region now means the snapshot sees a null region and the apply
        // installs an empty result.
        region.reset();
        manager.waitToFinish();

        CHECK(manager.externalCenterlineManager()->watchedFiles().isEmpty());
        CHECK(manager.externalCenterlineManager()->attachedCenterlinesModel()->rowCount() == 0);
    }

    SECTION("setRegion(nullptr) supersedes the previous region's scan state") {
        cwCavingRegion region;
        cwCave* cave = addEmptyCave(region, QStringLiteral("Cleared"));
        cwTrip* attached = addAttachedTrip(cave, QStringLiteral("Attached"));

        cwLinePlotManager manager;
        QHash<QUuid, QString> tripDirs;
        tripDirs.insert(attached->id(), attachDir);
        manager.externalCenterlineManager()->setTripAttachmentDirs(tripDirs);
        manager.setRegion(&region);
        manager.waitToFinish();
        REQUIRE_FALSE(manager.externalCenterlineManager()->watchedFiles().isEmpty());
        REQUIRE(manager.externalCenterlineManager()->attachedCenterlinesModel()->rowCount() == 1);

        // Nulling the region restarts the scan (superseding any in-flight
        // worker) with an empty snapshot, so the old region's watch set
        // and rows cannot outlive it.
        manager.setRegion(nullptr);
        manager.waitToFinish();

        CHECK(manager.externalCenterlineManager()->watchedFiles().isEmpty());
        CHECK(manager.externalCenterlineManager()->attachedCenterlinesModel()->rowCount() == 0);
    }

    SECTION("manager destroyed with a solve restart queued") {
        cwCavingRegion region;
        cwCave* cave = addEmptyCave(region, QStringLiteral("QueuedSolve"));
        cwTrip* attached = addAttachedTrip(cave, QStringLiteral("Attached"));

        auto manager = std::make_unique<cwLinePlotManager>();
        QHash<QUuid, QString> tripDirs;
        tripDirs.insert(attached->id(), attachDir);
        manager->externalCenterlineManager()->setTripAttachmentDirs(tripDirs);
        manager->setRegion(&region);
        manager->waitToFinish();

        // Queue a fresh solve restart through the event loop, then destroy
        // before it dispatches. The restarter's queued start executes during
        // the destructor's drain pump (only the m_alive flag guards it, and
        // that flag stays true for the whole destructor body), and its
        // buildInput reads the external subsystem's attachment dirs — which
        // must therefore still be alive at that point.
        manager->rerunSurvex();
        manager.reset();
        CHECK(true); // sentinel: reached teardown without crashing
    }

    SECTION("manager destroyed while the scan is queued or running") {
        cwCavingRegion region;
        cwCave* cave = addEmptyCave(region, QStringLiteral("Survivor"));
        cwTrip* attached = addAttachedTrip(cave, QStringLiteral("Attached"));

        auto manager = std::make_unique<cwLinePlotManager>();
        QHash<QUuid, QString> tripDirs;
        tripDirs.insert(attached->id(), attachDir);
        manager->externalCenterlineManager()->setTripAttachmentDirs(tripDirs);
        manager->setRegion(&region);

        // Destructor cancels both restarters; the worker's result (if it
        // ever ran) is dropped by the context auto-disconnect.
        manager.reset();
        CHECK(true); // sentinel: reached teardown without crashing
    }
}
