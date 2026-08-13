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
#include "cwFutureManagerModel.h"
#include "cwLinePlotManager.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSaveLoad.h"
#include "cwTrip.h"

// Tests
#include "ExternalCenterlineTestHelpers.h"

// Qt
#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QUuid>

// Std
#include <memory>

namespace {

const QStringList kHangingStations = {QStringLiteral("hanging.h1"),
                                      QStringLiteral("hanging.h2"),
                                      QStringLiteral("hanging.h3")};

// A saved project holding one trip with an attachment entry file, written
// with `entryContents`. Saving is what gives the manager a data root, and the
// data root is what the harvest error's paths are rewritten against.
struct ProjectFixture {
    QTemporaryDir tempDir;
    std::unique_ptr<cwRootData> rootData;
    cwProject* project = nullptr;
    cwTrip* trip = nullptr;
    QString dataRootDir;
    QString entryPath;
};

std::unique_ptr<ProjectFixture> makeProjectFixture(const QByteArray& entryContents)
{
    auto fixture = std::make_unique<ProjectFixture>();
    REQUIRE(fixture->tempDir.isValid());

    fixture->rootData = std::make_unique<cwRootData>();
    fixture->project = fixture->rootData->project();

    auto region = fixture->project->cavingRegion();
    region->addCave();
    cwCave* cave = region->cave(0);
    cave->setName(QStringLiteral("Harvest"));
    cave->addTrip();
    fixture->trip = cave->trip(0);
    fixture->trip->setName(QStringLiteral("Attached"));
    fixture->trip->setExternalCenterline(cwExternalCenterline(QStringLiteral("entry.svx")));

    const QString projectPath =
        QDir(fixture->tempDir.path()).filePath(QStringLiteral("harvest.cwproj"));
    REQUIRE(fixture->project->saveAs(projectPath));
    fixture->project->waitSaveToFinish();

    cwSaveLoad* saveLoad = fixture->project->saveLoad();
    fixture->dataRootDir = saveLoad->dataRootDir().absolutePath();
    const QString attachmentDir =
        saveLoad->externalCenterlineDir(fixture->trip).absolutePath();
    REQUIRE(QDir().mkpath(attachmentDir));
    fixture->entryPath = QDir(attachmentDir).filePath(QStringLiteral("entry.svx"));
    overwriteFile(fixture->entryPath, entryContents);

    fixture->rootData->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();
    return fixture;
}

void scanFixture(cwLinePlotManager& manager, ProjectFixture& fixture)
{
    manager.externalCenterlineManager()->setSaveLoad(fixture.project->saveLoad());
    manager.setRegion(fixture.project->cavingRegion());
    manager.waitToFinish();
}

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

TEST_CASE("A harvest error names the attachment the way the project does",
          "[LinePlotManager][StationHarvest]")
{
    auto fixture = makeProjectFixture(fileContents(fixturePath(QStringLiteral("broken.svx"))));

    cwLinePlotManager manager;
    scanFixture(manager, *fixture);

    const QString error = fixture->trip->externalStationsError();
    INFO(error.toStdString());
    REQUIRE_FALSE(error.isEmpty());

    // The same name the missing-copy banner gives the file, so two
    // collaborators looking at the same broken attachment read the same text.
    const QString relativeEntry =
        QDir(fixture->dataRootDir).relativeFilePath(fixture->entryPath);
    CHECK(relativeEntry == QStringLiteral("Harvest/trips/Attached/external-centerline/entry.svx"));
    CHECK(error.contains(relativeEntry));
    CHECK_FALSE(error.contains(fixture->dataRootDir));
}

TEST_CASE("A harvest error keeps an absolute path from outside the data root",
          "[LinePlotManager][StationHarvest]")
{
    auto fixture = makeProjectFixture(QByteArray());

    // A directory whose name starts with the data root's, which is what a
    // prefix match without the separator would wrongly rewrite.
    const QString siblingDir = fixture->dataRootDir + QStringLiteral("-elsewhere");
    REQUIRE(QDir().mkpath(siblingDir));
    const QString missingInclude =
        QDir(siblingDir).filePath(QStringLiteral("absent.svx"));
    REQUIRE_FALSE(QFile::exists(missingInclude));

    overwriteFile(fixture->entryPath,
                  QByteArray("*include \"") + missingInclude.toUtf8() + "\"\n");

    cwLinePlotManager manager;
    scanFixture(manager, *fixture);

    const QString error = fixture->trip->externalStationsError();
    INFO(error.toStdString());
    REQUIRE_FALSE(error.isEmpty());

    // Outside the project, so the absolute spelling is the only true one.
    CHECK(error.contains(missingInclude));
}
