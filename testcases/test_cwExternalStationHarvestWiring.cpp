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
#include "cwExternalCenterlineManager.h"
#include "cwLinePlotManager.h"
#include "cwTrip.h"

// Tests
#include "ExternalCenterlineTestHelpers.h"

// Qt
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QUuid>

namespace {

const QStringList kHangingStations = {QStringLiteral("hanging.h1"),
                                      QStringLiteral("hanging.h2"),
                                      QStringLiteral("hanging.h3")};

} // namespace

TEST_CASE("The scan harvests an attached trip's station names",
          "[LinePlotManager][StationHarvest]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    const QString attachDir = tempSubdir(tempRoot, QStringLiteral("harvest-names"));
    seedAttachment(attachDir, fixturePath(QStringLiteral("survex_hanging.svx")));

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Harvest"));
    cwTrip* native = addNativeTripWithShot(cave, QStringLiteral("Native"),
                                           QStringLiteral("A1"), QStringLiteral("A2"));
    cwTrip* attached = addAttachedTrip(cave, QStringLiteral("Attached"),
                                       QStringLiteral("survex_hanging.svx"));

    cwLinePlotManager manager;
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(attached->id(), attachDir);
    manager.externalCenterlineManager()->setTripAttachmentDirs(tripDirs);
    manager.setRegion(&region);
    manager.waitToFinish();

    // Nothing fixes survex_hanging.svx and nothing ties it in, so the region
    // solve dropped it — these names exist only because the scan ran cavern
    // over the attachment on its own.
    CHECK(attached->externalStations() == kHangingStations);
    CHECK(attached->externalStationsError().isEmpty());
    CHECK(attached->solvedStations().isEmpty());

    // A native trip has no file to read, so it never gains names this way.
    CHECK(native->externalStations().isEmpty());
    CHECK(native->externalStationsError().isEmpty());

    // Detaching leaves the trip out of the next scan's result entirely. The
    // apply has to clear it anyway — names it no longer owns would keep
    // showing up in the banner and the tie suggester.
    attached->setExternalCenterline(cwExternalCenterline());
    manager.waitToFinish();
    CHECK(attached->externalStations().isEmpty());
}

TEST_CASE("A broken attachment reports cavern's error instead of names",
          "[LinePlotManager][StationHarvest]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    const QString attachDir = tempSubdir(tempRoot, QStringLiteral("harvest-broken"));
    const QString copyPath = seedAttachment(attachDir, fixturePath(QStringLiteral("broken.svx")));

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Broken"));
    addNativeTripWithShot(cave, QStringLiteral("Native"),
                          QStringLiteral("A1"), QStringLiteral("A2"));
    cwTrip* attached = addAttachedTrip(cave, QStringLiteral("Attached"),
                                       QStringLiteral("broken.svx"));

    cwLinePlotManager manager;
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(attached->id(), attachDir);
    manager.externalCenterlineManager()->setTripAttachmentDirs(tripDirs);
    manager.setRegion(&region);
    manager.waitToFinish();

    // The whole point of harvesting per file: the region solve fails as one
    // run with one log, while this names the file that broke it.
    CHECK(attached->externalStations().isEmpty());
    INFO(attached->externalStationsError().toStdString());
    CHECK_FALSE(attached->externalStationsError().isEmpty());
    CHECK(attached->externalStationsError().contains(QStringLiteral("broken.svx")));

    REQUIRE(manager.externalCenterlineManager()->watchedFiles()
            .contains(QFileInfo(copyPath).canonicalFilePath()));

    // Fixing the file in place is the way out of the error, and the watcher
    // recompute is what has to notice.
    overwriteFile(copyPath, fileContents(fixturePath(QStringLiteral("survex_hanging.svx"))));

    REQUIRE(tryWait(kWatcherWaitMs, [&] {
        return !attached->externalStations().isEmpty();
    }));
    manager.waitToFinish();

    CHECK(attached->externalStations() == kHangingStations);
    CHECK(attached->externalStationsError().isEmpty());
}
