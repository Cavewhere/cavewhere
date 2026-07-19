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
#include "cwLinePlotManager.h"
#include "cwLinePlotTask.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwSurveyChunk.h"
#include "cwTrip.h"

// Test helpers
#include "ExternalCenterlineTestHelpers.h"
#include "LoadProjectHelper.h"

// Qt
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>
#include <QUuid>

namespace {

cwTrip* addTripWithShot(cwCave* cave, const QString& name)
{
    cwTrip* trip = addEmptyTrip(cave, name);
    cwSurveyChunk* chunk = new cwSurveyChunk();
    trip->addChunk(chunk);
    cwShot shot;
    shot.setDistance(cwDistanceReading(QStringLiteral("10.0")));
    shot.setCompass(cwCompassReading(QStringLiteral("0.0")));
    shot.setClino(cwClinoReading(QStringLiteral("0.0")));
    chunk->appendShot(cwStation(QStringLiteral("N1")), cwStation(QStringLiteral("N2")), shot);
    return trip;
}

} // namespace

TEST_CASE("Native region has zero attached rows and a native driver source",
          "[LinePlotManager][AttachedCenterlinesModel]")
{
    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Native"));
    addTripWithShot(cave, QStringLiteral("NativeTrip"));

    cwLinePlotManager manager;
    manager.setRegion(&region);
    manager.waitToFinish();

    REQUIRE_FALSE(manager.hasSolveError());
    CHECK(manager.attachedCenterlinesModel()->rowCount() == 0);

    // The driver text is the native *begin cave_<uuid> form the line-plot
    // exporter has emitted since Phase 1 commit 7.
    const QString driver = manager.driverSource();
    REQUIRE_FALSE(driver.isEmpty());
    CHECK(driver.contains(QStringLiteral("*begin %1")
                          .arg(cwLinePlotTask::cavernCaveNameFor(cave->id()))));
    CHECK_FALSE(driver.contains(QStringLiteral("*include")));
}

TEST_CASE("Attaching one trip yields one row and an *include in the driver source",
          "[LinePlotManager][AttachedCenterlinesModel]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    const QString attachDir = tempSubdir(tempRoot, QStringLiteral("one-trip"));
    seedAttachment(attachDir, fixturePath(QStringLiteral("survex_simple.svx")));

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));
    cwTrip* attached = addAttachedTrip(cave, QStringLiteral("Attached"));

    cwLinePlotManager manager;
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(attached->id(), attachDir);
    manager.setTripAttachmentDirs(tripDirs);
    manager.setRegion(&region);
    manager.waitToFinish();

    REQUIRE_FALSE(manager.hasSolveError());

    const cwAttachedCenterlinesModel* model = manager.attachedCenterlinesModel();
    REQUIRE(model->rowCount() == 1);
    CHECK(roleAt(model, 0, cwAttachedCenterlinesModel::OwnerNameRole).toString()
          == QStringLiteral("Attached"));
    CHECK(roleAt(model, 0, cwAttachedCenterlinesModel::OwnerKindRole).toString()
          == QStringLiteral("Trip"));
    CHECK(roleAt(model, 0, cwAttachedCenterlinesModel::EntryFileRole).toString()
          == QStringLiteral("survex_simple.svx"));
    // survex_simple.svx has no *include chain: the closure is the entry alone.
    CHECK(roleAt(model, 0, cwAttachedCenterlinesModel::DepCountRole).toInt() == 1);
    CHECK(roleAt(model, 0, cwAttachedCenterlinesModel::WarningCountRole).toInt() == 0);
    CHECK(roleAt(model, 0, cwAttachedCenterlinesModel::LastSolvedRole)
          .toDateTime().isValid());

    // The solve's driver carries the absolute forward-slash quoted
    // *include line per Phase 1 commit 8's format.
    const QString expectedAbs = QFileInfo(QDir(attachDir)
        .filePath(QStringLiteral("survex_simple.svx"))).absoluteFilePath();
    CHECK(manager.driverSource().contains(QStringLiteral("*include \"%1\"").arg(expectedAbs)));
}

TEST_CASE("Rows sort by cave display name then trip display name",
          "[LinePlotManager][AttachedCenterlinesModel]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    const QString zuluDir = tempSubdir(tempRoot, QStringLiteral("zulu"));
    seedAttachment(zuluDir, fixturePath(QStringLiteral("survex_simple.svx")));
    const QString alphaDir = tempSubdir(tempRoot, QStringLiteral("alpha"));
    seedAttachment(alphaDir, fixturePath(QStringLiteral("survex_simple.svx")));

    // Insert the later-sorting cave first so the model's order can't
    // accidentally mirror region insertion order.
    cwCavingRegion region;
    cwCave* zuluCave = addEmptyCave(region, QStringLiteral("Zulu"));
    cwTrip* zuluTrip = addAttachedTrip(zuluCave, QStringLiteral("Alpha trip"));
    cwCave* alphaCave = addEmptyCave(region, QStringLiteral("Alpha"));
    cwTrip* alphaTrip = addAttachedTrip(alphaCave, QStringLiteral("Zeta trip"));
    Q_UNUSED(alphaCave);

    cwLinePlotManager manager;
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(zuluTrip->id(), zuluDir);
    tripDirs.insert(alphaTrip->id(), alphaDir);
    manager.setTripAttachmentDirs(tripDirs);
    manager.setRegion(&region);
    manager.waitToFinish();

    const cwAttachedCenterlinesModel* model = manager.attachedCenterlinesModel();
    REQUIRE(model->rowCount() == 2);
    // Cave name is the major key: Alpha's "Zeta trip" precedes Zulu's
    // "Alpha trip" even though the trip names sort the other way.
    CHECK(roleAt(model, 0, cwAttachedCenterlinesModel::OwnerNameRole).toString()
          == QStringLiteral("Zeta trip"));
    CHECK(roleAt(model, 1, cwAttachedCenterlinesModel::OwnerNameRole).toString()
          == QStringLiteral("Alpha trip"));
}

TEST_CASE("Detach removes the owner's row via rowsRemoved with correct indices",
          "[LinePlotManager][AttachedCenterlinesModel]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    const QString firstDir = tempSubdir(tempRoot, QStringLiteral("first"));
    seedAttachment(firstDir, fixturePath(QStringLiteral("survex_simple.svx")));
    const QString secondDir = tempSubdir(tempRoot, QStringLiteral("second"));
    seedAttachment(secondDir, fixturePath(QStringLiteral("survex_simple.svx")));

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Detach"));
    cwTrip* first = addAttachedTrip(cave, QStringLiteral("First"));
    cwTrip* second = addAttachedTrip(cave, QStringLiteral("Second"));

    cwLinePlotManager manager;
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(first->id(), firstDir);
    tripDirs.insert(second->id(), secondDir);
    manager.setTripAttachmentDirs(tripDirs);
    manager.setRegion(&region);
    manager.waitToFinish();

    cwAttachedCenterlinesModel* model = manager.attachedCenterlinesModel();
    REQUIRE(model->rowCount() == 2);
    REQUIRE(roleAt(model, 0, cwAttachedCenterlinesModel::OwnerNameRole).toString()
            == QStringLiteral("First"));

    QSignalSpy removedSpy(model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy resetSpy(model, &QAbstractItemModel::modelReset);

    // Detach "First": clear its centerline and drop its attachment-dir
    // entry — the setter drives the (async) recompute that rebuilds the
    // rows, so drain it before asserting on the model signals.
    first->setExternalCenterline(cwExternalCenterline());
    QHash<QUuid, QString> remainingDirs;
    remainingDirs.insert(second->id(), secondDir);
    manager.setTripAttachmentDirs(remainingDirs);
    manager.waitToFinish();

    REQUIRE(removedSpy.count() == 1);
    CHECK(removedSpy.at(0).at(1).toInt() == 0); // first
    CHECK(removedSpy.at(0).at(2).toInt() == 0); // last
    CHECK(resetSpy.count() == 0);
    REQUIRE(model->rowCount() == 1);
    CHECK(roleAt(model, 0, cwAttachedCenterlinesModel::OwnerNameRole).toString()
          == QStringLiteral("Second"));
}
