/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

// Cavewhere
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwExternalCenterline.h"
#include "cwExternalCenterlineManager.h"
#include "cwExternalSourceSettings.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwGeoReference.h"
#include "cwLinePlotManager.h"
#include "cwLinePlotTask.h"
#include "cwNote.h"
#include "cwNoteStation.h"
#include "cwRenderLinePlot.h"
#include "cwScrap.h"
#include "cwShot.h"
#include "cwSurveyNoteModel.h"
#include "cwStation.h"
#include "cwStationPositionLookup.h"
#include "cwSurveyChunk.h"
#include "cwSurvexExporterRegion.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"

// Test helpers
#include "BoulderFixtureHelper.h"
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
    manager.externalCenterlineManager()->setTripAttachmentDirs(tripDirs);
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

TEST_CASE("Trip-attached centerline emits renderable line geometry",
          "[Attach][Geometry]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    const QString attachDir = seedAttachment(tempSubdir(tempRoot, QStringLiteral("geom-attach")),
                                             fixturePath(QStringLiteral("survex_simple.svx")));

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));
    cwTrip* attached = addEmptyTrip(cave, QStringLiteral("Attached"));
    attached->setExternalCenterline(cwExternalCenterline(QStringLiteral("survex_simple.svx")));

    // linePlot must outlive manager: the manager holds a raw pointer to it and
    // its destructor drain can push geometry, so declare it first.
    cwRenderLinePlot linePlot;
    cwLinePlotManager manager;
    manager.setRenderLinePlot(&linePlot);

    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(attached->id(), attachDir);
    manager.externalCenterlineManager()->setTripAttachmentDirs(tripDirs);
    manager.setRegion(&region);
    manager.waitToFinish();

    REQUIRE_FALSE(manager.hasSolveError());

    // The external stations solved and reached the cave lookup — this half
    // works today (it is why the trip's station-list panel populates).
    const cwStationPositionLookup& lookup = cave->stationPositionLookup();
    const QString tripPrefix = QStringLiteral("trip_%1").arg(attached->id().toString(QUuid::Id128));
    const QString a1Key = tripPrefix + QStringLiteral(".simple.a1");
    const QString a2Key = tripPrefix + QStringLiteral(".simple.a2");
    const QString a3Key = tripPrefix + QStringLiteral(".simple.a3");
    REQUIRE(lookup.hasPosition(a1Key));
    REQUIRE(lookup.hasPosition(a2Key));
    REQUIRE(lookup.hasPosition(a3Key));

    // The attached trip has no survey chunk, so its geometry is emitted from the
    // solved network (cwLinePlotGeometry::emitNetworkScopeGeometry). Assert the
    // external centerline reaches the render object as line-list vertices.
    const QVector<QVector3D> points = linePlot.points();

    // Two shots (a1->a2, a2->a3) => 4 non-indexed line-list vertices.
    CHECK(points.size() == 4);

    // Every solved external station appears as a geometry vertex, in the same
    // world-origin-relative space as the lookup.
    const auto containsPoint = [&](const QVector3D& p) {
        for (const QVector3D& v : points) {
            if ((v - p).lengthSquared() < 1e-6f) {
                return true;
            }
        }
        return false;
    };
    CHECK(containsPoint(lookup.position(a1Key)));
    CHECK(containsPoint(lookup.position(a2Key)));
    CHECK(containsPoint(lookup.position(a3Key)));
}

// B4 / Layer 1 — recompute trigger. A note+scrap on an externally attached trip,
// anchored to an external station by its scope-stripped panel name ("simple.a1").
// After a solve the manager should announce the scrap as changed so cwScrapManager
// remorphs it, but the changed-scrap set is keyed on the bare note-station name
// while cavern reports the scoped name ("TRIP_<HEX>.SIMPLE.A1"), so the scrap is
// never flagged. [!shouldfail]: expected red until B4 is fixed (plan §16).
TEST_CASE("Externally attached scrap is flagged for remorph after a solve",
          "[Attach][Scrap][!shouldfail]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    const QString attachDir = seedAttachment(tempSubdir(tempRoot, QStringLiteral("scrap-attach")),
                                             fixturePath(QStringLiteral("survex_simple.svx")));

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));
    cwTrip* attached = addEmptyTrip(cave, QStringLiteral("Attached"));
    attached->setExternalCenterline(cwExternalCenterline(QStringLiteral("survex_simple.svx")));

    cwNote* note = new cwNote(attached->notes());
    attached->notes()->addNotes({note});
    cwScrap* scrap = new cwScrap();
    note->addScrap(scrap);

    cwNoteStation noteStation;
    noteStation.setName(QStringLiteral("simple.a1"));
    noteStation.setPositionOnNote(QPointF(0.5, 0.5));
    scrap->addStation(noteStation);

    // changedScraps must outlive manager: the lambda captures it by reference
    // and the manager's destructor drain can still emit, so declare it first.
    QList<cwScrap*> changedScraps;

    cwLinePlotManager manager;
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(attached->id(), attachDir);
    manager.externalCenterlineManager()->setTripAttachmentDirs(tripDirs);

    QObject::connect(&manager, &cwLinePlotManager::stationPositionInScrapsChanged,
                     [&changedScraps](const QList<cwScrap*>& scraps) {
                         changedScraps.append(scraps);
                     });

    manager.setRegion(&region);
    manager.waitToFinish();

    REQUIRE_FALSE(manager.hasSolveError());

    CHECK(changedScraps.contains(scrap));
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
    manager.externalCenterlineManager()->setCaveAttachmentDirs(caveDirs);
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
    manager.externalCenterlineManager()->setTripAttachmentDirs(tripDirs);
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

TEST_CASE("buildInput injects resolved declination only for CaveWhere-owned external trips",
          "[Attach][Declination]")
{
    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));

    cwTrip* caveWhereOwned = addEmptyTrip(cave, QStringLiteral("CaveWhereOwned"));
    caveWhereOwned->setExternalCenterline(cwExternalCenterline(QStringLiteral("survex_no_metadata.svx")));
    caveWhereOwned->calibrations()->setAutoDeclination(false);
    caveWhereOwned->calibrations()->setDeclinationManual(5.5);

    cwTrip* fileOwned = addEmptyTrip(cave, QStringLiteral("FileOwned"));
    fileOwned->setExternalCenterline(cwExternalCenterline(QStringLiteral("survex_with_metadata.svx")));
    fileOwned->calibrations()->setAutoDeclination(false);
    fileOwned->calibrations()->setDeclinationManual(3.3);

    // Native trips never appear in the injection map, even when the flag
    // map (wrongly) claims their file doesn't own declination.
    cwTrip* nativeTrip = addTripWithShot(cave,
                                         QStringLiteral("Native"),
                                         QStringLiteral("N1"),
                                         QStringLiteral("N2"),
                                         15.0);

    QHash<QUuid, bool> fileOwnsDeclination;
    fileOwnsDeclination.insert(caveWhereOwned->id(), false);
    fileOwnsDeclination.insert(fileOwned->id(), true);
    fileOwnsDeclination.insert(nativeTrip->id(), false);

    const auto input = cwLinePlotTask::buildInput(&region, { {}, {}, fileOwnsDeclination });

    REQUIRE(input.tripInjectedDeclinations.size() == 1);
    CHECK(input.tripInjectedDeclinations.value(caveWhereOwned->id()) == 5.5);
}

TEST_CASE("buildInput rides the IGRF-resolved declination for auto external trips",
          "[Attach][Declination]")
{
    cwCavingRegion region;
    region.geoReference()->setGlobalCoordinateSystem(QStringLiteral("EPSG:32613"));
    cwCave* cave = addEmptyCave(region, QStringLiteral("Boulder"));

    cwFixStation fix;
    fix.setStationName(QStringLiteral("a1"));
    fix.setInputCS(QStringLiteral("EPSG:32613"));
    fix.setEasting(478000.0);
    fix.setNorthing(4430000.0);
    fix.setElevation(1655.0);
    cave->fixStations()->appendFixStation(fix);

    cwTrip* attached = addEmptyTrip(cave, QStringLiteral("Attached"));
    attached->setExternalCenterline(cwExternalCenterline(QStringLiteral("survex_no_metadata.svx")));
    attached->setDate(makeUtcDate(2024, 6, 1));
    REQUIRE(attached->calibrations()->autoDeclination());
    REQUIRE(attached->calibrations()->autoDeclinationAvailable());

    QHash<QUuid, bool> fileOwnsDeclination;
    fileOwnsDeclination.insert(attached->id(), false);

    const auto input = cwLinePlotTask::buildInput(&region, { {}, {}, fileOwnsDeclination });

    // The injected value is the trip's resolved declination — IGRF here,
    // since auto is on and a dated trip + fixed cave make it available.
    const double resolved = attached->calibrations()->declination();
    CHECK(resolved != 0.0);
    CHECK(input.tripInjectedDeclinations.value(attached->id()) == resolved);
}

TEST_CASE("Driver emits injected declination inside the trip block before *include",
          "[Attach][Declination]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    const QString attachDir = seedAttachment(tempSubdir(tempRoot, QStringLiteral("decl-attach")),
                                             fixturePath(QStringLiteral("survex_no_metadata.svx")));

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));
    cwTrip* attached = addEmptyTrip(cave, QStringLiteral("Attached"));
    attached->setExternalCenterline(cwExternalCenterline(QStringLiteral("survex_no_metadata.svx")));

    cwCavingRegionData snapshot = region.data();
    REQUIRE(snapshot.caves.size() == 1);
    snapshot.caves[0].name = cwLinePlotTask::cavernCaveNameFor(cave->id());

    cwSurvexExporterRegion::Options options;
    options.tripAttachmentDirs.insert(attached->id(), attachDir);
    options.tripInjectedDeclinations.insert(attached->id(), 5.5);

    const QString driverPath = QDir(tempRoot.path()).absoluteFilePath(QStringLiteral("driver.svx"));
    auto result = cwSurvexExporterRegion::exportRegion(snapshot, driverPath, options);
    REQUIRE_FALSE(result.hasError());

    QFile file(driverPath);
    REQUIRE(file.open(QFile::ReadOnly));
    QString driver = QString::fromUtf8(file.readAll());
    file.close();

    // Stored +5.5 east-positive comes out with survex's negated calibrate
    // convention, same as native trips.
    const QString tripLabel = QStringLiteral("trip_%1").arg(attached->id().toString(QUuid::Id128));
    const qsizetype beginIndex = driver.indexOf(QStringLiteral("*begin %1").arg(tripLabel));
    const qsizetype declinationIndex = driver.indexOf(QStringLiteral("*calibrate DECLINATION -5.50"));
    const qsizetype includeIndex = driver.indexOf(QStringLiteral("*include"));
    const qsizetype endIndex = driver.indexOf(QStringLiteral("*end %1").arg(tripLabel));
    REQUIRE(beginIndex >= 0);
    REQUIRE(declinationIndex >= 0);
    REQUIRE(includeIndex >= 0);
    REQUIRE(endIndex >= 0);
    CHECK(beginIndex < declinationIndex);
    CHECK(declinationIndex < includeIndex);
    CHECK(includeIndex < endIndex);

    // Without an injection entry the trip block carries no declination at
    // all (the file-owned / unknown-ownership path).
    options.tripInjectedDeclinations.clear();
    result = cwSurvexExporterRegion::exportRegion(snapshot, driverPath, options);
    REQUIRE_FALSE(result.hasError());
    REQUIRE(file.open(QFile::ReadOnly));
    driver = QString::fromUtf8(file.readAll());
    CHECK_FALSE(driver.contains(QStringLiteral("*calibrate DECLINATION")));
}

TEST_CASE("Injected manual declination rotates externally attached stations",
          "[Attach][Declination]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    const QString attachDir = seedAttachment(tempSubdir(tempRoot, QStringLiteral("decl-rotate")),
                                             fixturePath(QStringLiteral("survex_no_metadata.svx")));

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Rotated"));
    cwTrip* attached = addEmptyTrip(cave, QStringLiteral("Attached"));
    attached->setExternalCenterline(cwExternalCenterline(QStringLiteral("survex_no_metadata.svx")));
    attached->calibrations()->setAutoDeclination(false);
    attached->calibrations()->setDeclinationManual(90.0);

    cwLinePlotManager manager;
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(attached->id(), attachDir);
    manager.externalCenterlineManager()->setTripAttachmentDirs(tripDirs);
    manager.setRegion(&region);
    manager.waitToFinish();

    REQUIRE_FALSE(manager.hasSolveError());

    // The scan of the in-project entry found no declination directive, so
    // CaveWhere owns it and the driver injected the manual 90.
    CHECK_FALSE(manager.externalCenterlineManager()->fileOwnsDeclination(attached->id()));

    // survex_no_metadata.svx shoots B1→B2 at compass 0 for 10 m with B1
    // fixed at the origin. Declination +90 east swings the true bearing
    // to 90°, so B2 lands 10 m east instead of 10 m north.
    const cwStationPositionLookup& lookup = cave->stationPositionLookup();
    const QString tripPrefix = QStringLiteral("trip_%1").arg(attached->id().toString(QUuid::Id128));
    const QString b2Key = tripPrefix + QStringLiteral(".nometadata.b2");
    REQUIRE(lookup.hasPosition(b2Key));
    const QVector3D b2 = lookup.position(b2Key);
    CHECK(b2.x() == Catch::Approx(10.0).margin(0.01));
    CHECK(b2.y() == Catch::Approx(0.0).margin(0.01));
}

TEST_CASE("Project-side scan flag wins over a diverged live-link source",
          "[Attach][Declination]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    // In-project copy carries no declination (CaveWhere owns it), but the
    // live-link source has since gained its own *calibrate declination and
    // no reconcile has run. The solve *includes the in-project bytes, so
    // the flag — and therefore the injection — must follow the project
    // copy, not the fresher source.
    const QString attachDir = seedAttachment(tempSubdir(tempRoot, QStringLiteral("decl-diverged")),
                                             fixturePath(QStringLiteral("survex_no_metadata.svx")));
    const QString sourceDir = seedAttachment(tempSubdir(tempRoot, QStringLiteral("decl-source")),
                                             fixturePath(QStringLiteral("survex_with_metadata.svx")));
    const QString sourcePath = QDir(sourceDir).absoluteFilePath(QStringLiteral("survex_with_metadata.svx"));

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Diverged"));
    cwTrip* attached = addEmptyTrip(cave, QStringLiteral("Attached"));
    attached->setExternalCenterline(cwExternalCenterline(QStringLiteral("survex_no_metadata.svx")));
    attached->calibrations()->setAutoDeclination(false);
    attached->calibrations()->setDeclinationManual(90.0);

    cwExternalSourceSettings settings;
    settings.setSourcePath(attached->id(), sourcePath);

    cwLinePlotManager manager;
    manager.externalCenterlineManager()->setExternalSourceSettings(&settings);
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(attached->id(), attachDir);
    manager.externalCenterlineManager()->setTripAttachmentDirs(tripDirs);
    manager.setRegion(&region);
    manager.waitToFinish();

    REQUIRE_FALSE(manager.hasSolveError());
    CHECK_FALSE(manager.externalCenterlineManager()->fileOwnsDeclination(attached->id()));

    // Injection applied → B2 swings east, exactly as in the undiverged case.
    const cwStationPositionLookup& lookup = cave->stationPositionLookup();
    const QString tripPrefix = QStringLiteral("trip_%1").arg(attached->id().toString(QUuid::Id128));
    const QString b2Key = tripPrefix + QStringLiteral(".nometadata.b2");
    REQUIRE(lookup.hasPosition(b2Key));
    const QVector3D b2 = lookup.position(b2Key);
    CHECK(b2.x() == Catch::Approx(10.0).margin(0.01));
    CHECK(b2.y() == Catch::Approx(0.0).margin(0.01));

    settings.clearSourcePath(attached->id());
}

TEST_CASE("File-owned declination wins over the trip's CaveWhere setting",
          "[Attach][Declination]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    const QString attachDir = seedAttachment(tempSubdir(tempRoot, QStringLiteral("decl-fileowned")),
                                             fixturePath(QStringLiteral("survex_with_metadata.svx")));

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("FileOwned"));
    cwTrip* attached = addEmptyTrip(cave, QStringLiteral("Attached"));
    attached->setExternalCenterline(cwExternalCenterline(QStringLiteral("survex_with_metadata.svx")));
    // Sentinel that must NOT take effect: the file's own
    // *calibrate declination 7.2 owns the trip's declination.
    attached->calibrations()->setAutoDeclination(false);
    attached->calibrations()->setDeclinationManual(90.0);

    cwLinePlotManager manager;
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(attached->id(), attachDir);
    manager.externalCenterlineManager()->setTripAttachmentDirs(tripDirs);
    manager.setRegion(&region);
    manager.waitToFinish();

    REQUIRE_FALSE(manager.hasSolveError());
    CHECK(manager.externalCenterlineManager()->fileOwnsDeclination(attached->id()));

    // A1→A2, compass 0, 10 m, A1 fixed at origin. The file's
    // *calibrate declination 7.2 means "subtract 7.2 from bearings"
    // (survex calibrate convention), so A2 resolves at bearing -7.2°:
    // x = 10·sin(-7.2°), y = 10·cos(-7.2°). The trip's 90° sentinel must
    // leave no trace.
    const cwStationPositionLookup& lookup = cave->stationPositionLookup();
    const QString tripPrefix = QStringLiteral("trip_%1").arg(attached->id().toString(QUuid::Id128));
    const QString a2Key = tripPrefix + QStringLiteral(".withmetadata.a2");
    REQUIRE(lookup.hasPosition(a2Key));
    const QVector3D a2 = lookup.position(a2Key);
    CHECK(a2.x() == Catch::Approx(-1.2533).margin(0.01));
    CHECK(a2.y() == Catch::Approx(9.9211).margin(0.01));
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
    manager.externalCenterlineManager()->setTripAttachmentDirs(tripDirs);
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
