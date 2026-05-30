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
#include "cwLinePlotTask.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwStationPositionLookup.h"
#include "cwSurveyChunk.h"
#include "cwSurvexExporterRegion.h"
#include "cwTrip.h"

// Test helpers
#include "LoadProjectHelper.h"

// Qt
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QString>
#include <QTemporaryDir>
#include <QUuid>

namespace {

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

// Copies `source` to <attachDir>/<basename(source)> so the worker's
// *include path resolves at solve time. Returns the attachment dir.
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
    return attachDir;
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

cwTrip* addTripWithShot(cwCave* cave,
                       const QString& name,
                       const QString& fromName,
                       const QString& toName,
                       double distance)
{
    cwTrip* trip = addEmptyTrip(cave, name);
    cwSurveyChunk* chunk = new cwSurveyChunk();
    trip->addChunk(chunk);
    cwShot shot;
    shot.setDistance(cwDistanceReading(QString::number(distance)));
    shot.setCompass(cwCompassReading(QStringLiteral("0.0")));
    shot.setClino(cwClinoReading(QStringLiteral("0.0")));
    chunk->appendShot(cwStation(fromName), cwStation(toName), shot);
    return trip;
}

// Reads the driver .svx the exporter writes when invoked with the
// supplied options. The exporter has no inspection hook today, so the
// test calls it directly against a known output path - this is the
// same code path the worker exercises but produces a file we can read
// back for the [Attach][Trip] driver-content assertion.
QString writeDriverFor(const cwCavingRegion& region,
                       const cwSurvexExporterRegion::Options& options,
                       const QString& outputPath)
{
    const auto result = cwSurvexExporterRegion::exportRegion(region.data(), outputPath, options);
    REQUIRE_FALSE(result.hasError());
    QFile file(outputPath);
    REQUIRE(file.open(QFile::ReadOnly));
    return QString::fromUtf8(file.readAll());
}

// Helper to assert no lookup entry has the cave UUID prefix (i.e. the
// cave is genuinely empty in the solved lookup). Used by [Attach][Error].
bool lookupHasAnyStationFor(const cwStationPositionLookup& lookup)
{
    return !lookup.positions().isEmpty();
}

} // namespace

TEST_CASE("Driver *include emits absolute forward-slash quoted path for trip attachment",
          "[Attach][Trip]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    const QString attachDir = seedAttachment(tempSubdir(tempRoot, QStringLiteral("trip-attach")),
                                             fixturePath(QStringLiteral("survex_simple.svx")));

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));
    cwTrip* attached = addEmptyTrip(cave, QStringLiteral("Attached"));
    attached->setExternalCenterline(cwExternalCenterline(QStringLiteral("survex_simple.svx")));

    cwSurvexExporterRegion::Options options;
    options.tripAttachmentDirs.insert(attached->id(), attachDir);

    // Worker rewrites the cave name to cave_<uuid> via encodeCaveNames
    // before calling exportRegion; mirror that here so the driver
    // content assertion reflects the production line-plot path.
    cwCavingRegionData snapshot = region.data();
    REQUIRE(snapshot.caves.size() == 1);
    snapshot.caves[0].name = cwLinePlotTask::cavernCaveNameFor(cave->id());

    const QString driverPath = QDir(tempRoot.path()).absoluteFilePath(QStringLiteral("driver.svx"));
    const auto result = cwSurvexExporterRegion::exportRegion(snapshot, driverPath, options);
    REQUIRE_FALSE(result.hasError());

    QFile file(driverPath);
    REQUIRE(file.open(QFile::ReadOnly));
    const QString driver = QString::fromUtf8(file.readAll());

    // The trip wrapper uses trip_<uuid> with QUuid::Id128 layout
    // (32 lower-case hex, no hyphens, no braces).
    const QString tripLabel = QStringLiteral("trip_%1").arg(attached->id().toString(QUuid::Id128));
    CHECK(driver.contains(QStringLiteral("*begin %1").arg(tripLabel)));
    CHECK(driver.contains(QStringLiteral("*end %1").arg(tripLabel)));

    // Absolute, forward-slash, quoted *include path joining the
    // attachment dir and the project-relative entry file. The expected
    // path is the canonical absolute form of <attachDir>/survex_simple.svx.
    const QString expectedAbs = QFileInfo(QDir(attachDir).filePath(QStringLiteral("survex_simple.svx"))).absoluteFilePath();
    const QString expectedLine = QStringLiteral("*include \"%1\"").arg(expectedAbs);
    CHECK(driver.contains(expectedLine));
    // Forward-slash invariant: no back-slash should appear in the
    // include path even on Windows. The expected path uses
    // QFileInfo::absoluteFilePath() which already returns forward
    // slashes on every platform.
    CHECK_FALSE(driver.contains(QStringLiteral("*include \"") + QString(expectedAbs).replace('/', '\\')));
}

TEST_CASE("Trip-attached centerline resolves stations under cave_<uuid>.trip_<uuid>.*",
          "[Attach][Trip]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    const QString attachDir = seedAttachment(tempSubdir(tempRoot, QStringLiteral("trip-attach")),
                                             fixturePath(QStringLiteral("survex_simple.svx")));

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));
    cwTrip* attached = addEmptyTrip(cave, QStringLiteral("Attached"));
    attached->setExternalCenterline(cwExternalCenterline(QStringLiteral("survex_simple.svx")));

    cwLinePlotManager manager;
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(attached->id(), attachDir);
    manager.setTripAttachmentDirs(tripDirs);
    manager.setRegion(&region);
    manager.waitToFinish();

    REQUIRE_FALSE(manager.hasSolveError());

    // survex_simple.svx defines *begin Simple with stations a1, a2, a3.
    // After cavern's tolower default and our prefix wrapping, the
    // lookup keys are "trip_<uuid>.simple.a1" etc.
    const cwStationPositionLookup& lookup = cave->stationPositionLookup();
    const QString tripPrefix = QStringLiteral("trip_%1").arg(attached->id().toString(QUuid::Id128));

    const QStringList expectedTails = {QStringLiteral("simple.a1"),
                                       QStringLiteral("simple.a2"),
                                       QStringLiteral("simple.a3")};
    for (const QString& tail : expectedTails) {
        const QString key = tripPrefix + QStringLiteral(".") + tail;
        INFO("expected key: " << key.toStdString());
        CHECK(lookup.hasPosition(key));
    }
}

TEST_CASE("Cave-attached centerline skips trip loop and resolves under cave_<uuid>.*",
          "[Attach][Cave]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    const QString attachDir = tempSubdir(tempRoot, QStringLiteral("cave-attach"));
    // survex_nested.svx pulls in entrance.svx which pulls in
    // passage.svx. Copy all three to the attachment dir so the
    // include chain resolves at solve time.
    for (const QString& f : {QStringLiteral("survex_nested.svx"),
                             QStringLiteral("entrance.svx"),
                             QStringLiteral("passage.svx")}) {
        seedAttachment(attachDir, fixturePath(f));
    }

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Nested"));
    cave->setExternalCenterline(cwExternalCenterline(QStringLiteral("survex_nested.svx")));

    cwLinePlotManager manager;
    QHash<QUuid, QString> caveDirs;
    caveDirs.insert(cave->id(), attachDir);
    manager.setCaveAttachmentDirs(caveDirs);
    manager.setRegion(&region);
    manager.waitToFinish();

    REQUIRE_FALSE(manager.hasSolveError());

    const cwStationPositionLookup& lookup = cave->stationPositionLookup();
    // cave-level attachment unwraps directly inside cave_<uuid>, so the
    // lookup keys reflect the file's own *begin nesting:
    //   nested.entrance.e1 / e2 (from entrance.svx)
    //   nested.entrance.passage.p2 / p3 (from passage.svx, which joins
    //     entrance via ..E2 so the whole closure is one connected
    //     component).
    const QStringList expected = {QStringLiteral("nested.entrance.e1"),
                                  QStringLiteral("nested.entrance.e2"),
                                  QStringLiteral("nested.entrance.passage.p1"),
                                  QStringLiteral("nested.entrance.passage.p2"),
                                  QStringLiteral("nested.entrance.passage.p3")};
    for (const QString& key : expected) {
        INFO("expected key: " << key.toStdString());
        CHECK(lookup.hasPosition(key));
    }
}

TEST_CASE("Mixed cave: native trip and attached trip both resolve",
          "[Attach][Mixed]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    const QString attachDir = seedAttachment(tempSubdir(tempRoot, QStringLiteral("mixed-attach")),
                                             fixturePath(QStringLiteral("survex_simple.svx")));

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Mixed"));
    cwTrip* nativeTrip = addTripWithShot(cave,
                                         QStringLiteral("Native"),
                                         QStringLiteral("N1"),
                                         QStringLiteral("N2"),
                                         15.0);
    Q_UNUSED(nativeTrip);
    cwTrip* attached = addEmptyTrip(cave, QStringLiteral("Attached"));
    attached->setExternalCenterline(cwExternalCenterline(QStringLiteral("survex_simple.svx")));

    cwLinePlotManager manager;
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(attached->id(), attachDir);
    manager.setTripAttachmentDirs(tripDirs);
    manager.setRegion(&region);
    manager.waitToFinish();

    REQUIRE_FALSE(manager.hasSolveError());

    const cwStationPositionLookup& lookup = cave->stationPositionLookup();
    // Native trip stations come from cave_<uuid>.N1 / N2 (cavern lowercases)
    // and the lookup strips the cave prefix.
    CHECK(lookup.hasPosition(QStringLiteral("n1")));
    CHECK(lookup.hasPosition(QStringLiteral("n2")));

    // Attached trip stations come through the trip_<uuid>.simple.* prefix.
    const QString tripPrefix = QStringLiteral("trip_%1").arg(attached->id().toString(QUuid::Id128));
    CHECK(lookup.hasPosition(tripPrefix + QStringLiteral(".simple.a1")));
    CHECK(lookup.hasPosition(tripPrefix + QStringLiteral(".simple.a3")));
}

TEST_CASE("Broken external centerline surfaces SolveError with Step::Cavern",
          "[Attach][Error]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    const QString attachDir = seedAttachment(tempSubdir(tempRoot, QStringLiteral("err-attach")),
                                             fixturePath(QStringLiteral("broken.svx")));

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Broken"));
    cwTrip* attached = addEmptyTrip(cave, QStringLiteral("Attached"));
    attached->setExternalCenterline(cwExternalCenterline(QStringLiteral("broken.svx")));

    cwLinePlotManager manager;
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(attached->id(), attachDir);
    manager.setTripAttachmentDirs(tripDirs);
    manager.setRegion(&region);
    manager.waitToFinish();

    REQUIRE(manager.hasSolveError());
    // Cavern's parser rejection lands on Step::Cavern (the .svx file
    // parsed cleanly from our exporter; cavern itself refused the
    // included file). The cavernLog property carries the diagnostic
    // text so CavernOutputPage can surface it.
    CHECK_FALSE(manager.cavernLog().isEmpty());
    CHECK(lookupHasAnyStationFor(cave->stationPositionLookup()) == false);
}
