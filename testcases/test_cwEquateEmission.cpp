/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Commits 4 & 5 of plans/EXTERNAL_FILE_EQUATES_AND_SCOPING.html: cave-level
// and region-level *equate emission.
//
// A cwCave equate renders as a bare *equate line at cave scope inside the
// driver, with cave-relative operands — a NativeCave handle as its tail, a
// Trip handle as "<scopePrefix><tail>". The solve then merges the tied
// stations to one coordinate, which is the payoff: a native station and an
// externally-attached station drawn coincident.
//
// A cwCavingRegion equate renders at region scope, after both *begin cave_<hex>
// siblings close, with fully-qualified operands ("cave_<hex>." prepended to the
// same cave-relative rendering). A cross-cave tie draws stations in two
// different caves coincident across the cave boundary.

// Catch
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

// Cavewhere
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwCavernNaming.h"
#include "cwEquate.h"
#include "cwEquateModel.h"
#include "cwExternalCenterline.h"
#include "cwExternalCenterlineManager.h"
#include "cwLinePlotManager.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwStationHandle.h"
#include "cwStationPositionLookup.h"
#include "cwSurveyChunk.h"
#include "cwSurvexExporterRegion.h"
#include "cwTrip.h"

// Test helpers
#include "ExternalCenterlineTestHelpers.h"

// Qt
#include <QDir>
#include <QFile>
#include <QHash>
#include <QString>
#include <QTemporaryDir>
#include <QUuid>

namespace {

// A one-shot native trip (station `fromName` -> `toName`, level, along the
// given `compass` bearing). It carries no fix, so the exporter fallback-fixes
// its first station at the origin; the toName station is derived from the leg,
// so a cross-cave tie on it closes a loop rather than double-fixing a station.
cwTrip* addNativeTripWithShot(cwCave* cave,
                              const QString& name,
                              const QString& fromName,
                              const QString& toName,
                              double distance,
                              const QString& compass = QStringLiteral("0.0"))
{
    cwTrip* trip = addEmptyTrip(cave, name);
    cwSurveyChunk* chunk = new cwSurveyChunk();
    trip->addChunk(chunk);
    cwShot shot;
    shot.setDistance(cwDistanceReading(QString::number(distance)));
    shot.setCompass(cwCompassReading(compass));
    shot.setClino(cwClinoReading(QStringLiteral("0.0")));
    chunk->appendShot(cwStation(fromName), cwStation(toName), shot);
    return trip;
}

cwStationHandle nativeHandle(const cwCave* cave, const QString& tail)
{
    return cwStationHandle(cwStationHandle::NativeCave, cave->id(), tail);
}

cwStationHandle tripHandle(const cwTrip* trip, const QString& tail)
{
    return cwStationHandle(cwStationHandle::Trip, trip->id(), tail);
}

QString tripScopePrefix(const cwTrip* trip)
{
    return cwCavernNaming::tripScopePrefix(trip->id());
}

// Runs the driver export the worker would run (cave name rewritten to
// cave_<hex> via encodeCaveNames) and returns the emitted .svx text.
QString driverTextFor(const cwCavingRegion& region,
                      const cwCave* cave,
                      const cwSurvexExporterRegion::Options& options,
                      const QString& outputPath)
{
    cwCavingRegionData snapshot = region.data();
    REQUIRE(snapshot.caves.size() == 1);
    snapshot.caves[0].name = cwCavernNaming::caveName(cave->id());

    const auto result = cwSurvexExporterRegion::exportRegion(snapshot, outputPath, options);
    REQUIRE_FALSE(result.hasError());

    QFile file(outputPath);
    REQUIRE(file.open(QFile::ReadOnly));
    return QString::fromUtf8(file.readAll());
}

// The region driver the worker would run: every cave name rewritten to
// cave_<hex> (encodeCaveNames), so region-scope operands are fully qualified.
// Returns the emitted .svx text.
QString regionDriverText(const cwCavingRegion& region,
                         const cwSurvexExporterRegion::Options& options,
                         const QString& outputPath)
{
    cwCavingRegionData snapshot = region.data();
    for (cwCaveData& cave : snapshot.caves) {
        cave.name = cwCavernNaming::caveName(cave.id);
    }

    const auto result = cwSurvexExporterRegion::exportRegion(snapshot, outputPath, options);
    REQUIRE_FALSE(result.hasError());

    QFile file(outputPath);
    REQUIRE(file.open(QFile::ReadOnly));
    return QString::fromUtf8(file.readAll());
}

} // namespace

TEST_CASE("Cave equate emits a bare *equate line at cave scope", "[Equate][Emission]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    // seedAttachment returns the copied file path, not the dir; the
    // *include resolves against <attachDir>/<basename>, so register the
    // dir (see the coincidence case below for the same pattern).
    const QString attachDir = tempSubdir(tempRoot, QStringLiteral("emit-attach"));
    seedAttachment(attachDir, fixturePath(QStringLiteral("survex_simple.svx")));

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));
    addNativeTripWithShot(cave, QStringLiteral("Native"),
                          QStringLiteral("1"), QStringLiteral("2"), 10.0);
    cwTrip* attached = addAttachedTrip(cave, QStringLiteral("Attached"));

    // A cross-scope tie (native 1 == the external centerline's simple.a1)
    // and a within-native tie (1 == 2), so both operand renderings are
    // exercised in one driver.
    cave->equates()->appendEquate(cwEquate({nativeHandle(cave, QStringLiteral("1")),
                                            tripHandle(attached, QStringLiteral("simple.a1"))}));
    cave->equates()->appendEquate(cwEquate({nativeHandle(cave, QStringLiteral("1")),
                                            nativeHandle(cave, QStringLiteral("2"))}));

    cwSurvexExporterRegion::Options options;
    options.tripAttachmentDirs.insert(attached->id(), attachDir);

    const QString driverPath = QDir(tempRoot.path()).absoluteFilePath(QStringLiteral("driver.svx"));
    const QString driver = driverTextFor(region, cave, options, driverPath);

    const QString crossScopeLine =
        QStringLiteral("*equate 1 %1simple.a1").arg(tripScopePrefix(attached));
    const QString nativeLine = QStringLiteral("*equate 1 2");
    CHECK(driver.contains(crossScopeLine));
    CHECK(driver.contains(nativeLine));

    // The ties must sit inside the cave block: after the trip wrapper that
    // declares the external stations, before the cave's *end. Otherwise a
    // cave-relative operand names a station that is out of scope.
    const QString caveEnd = QStringLiteral("*end %1").arg(cwCavernNaming::caveName(cave->id()));
    const int tripBeginIndex = driver.indexOf(QStringLiteral("*begin trip_"));
    const int equateIndex = driver.indexOf(QStringLiteral("*equate"));
    const int caveEndIndex = driver.indexOf(caveEnd);
    REQUIRE(tripBeginIndex >= 0);
    REQUIRE(equateIndex >= 0);
    REQUIRE(caveEndIndex >= 0);
    CHECK(tripBeginIndex < equateIndex);
    CHECK(equateIndex < caveEndIndex);
}

TEST_CASE("Structurally invalid or out-of-cave equates emit nothing", "[Equate][Emission]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));
    addNativeTripWithShot(cave, QStringLiteral("Native"),
                          QStringLiteral("1"), QStringLiteral("2"), 10.0);

    // A Trip handle naming a trip that is not in this cave cannot be
    // rendered cave-relative, so the whole equate is dropped rather than
    // emitting a name cavern can't resolve.
    const QUuid strangerTripId = QUuid::createUuid();
    cave->equates()->appendEquate(
        cwEquate({nativeHandle(cave, QStringLiteral("1")),
                  cwStationHandle(cwStationHandle::Trip, strangerTripId, QStringLiteral("x"))}));

    // A station tied only to itself is structurally invalid (one distinct
    // endpoint), so writeEquates drops it at the isValid() guard before any
    // operand is rendered.
    cave->equates()->appendEquate(
        cwEquate({nativeHandle(cave, QStringLiteral("1")),
                  nativeHandle(cave, QStringLiteral("1"))}));

    cwSurvexExporterRegion::Options options;
    const QString driverPath = QDir(tempRoot.path()).absoluteFilePath(QStringLiteral("driver.svx"));
    const QString driver = driverTextFor(region, cave, options, driverPath);

    CHECK_FALSE(driver.contains(QStringLiteral("*equate")));
}

TEST_CASE("A cave equate draws a native and an external station coincident",
          "[Equate][Emission]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    // seedAttachment copies the fixture into attachDir; the *include path is
    // <attachDir>/<basename>, so the attachment dir — not the copied file —
    // is what the worker resolves against.
    const QString attachDir = tempSubdir(tempRoot, QStringLiteral("coincident-attach"));
    seedAttachment(attachDir, fixturePath(QStringLiteral("survex_simple.svx")));

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));
    addNativeTripWithShot(cave, QStringLiteral("Native"),
                          QStringLiteral("1"), QStringLiteral("2"), 10.0);
    cwTrip* attached = addAttachedTrip(cave, QStringLiteral("Attached"));

    // Tie native "1" to the external centerline's simple.a1. Both survey
    // runs carry their own fix (the native cave via the exporter's
    // first-station fallback, the external file via its own *fix A1), so
    // the equate is what identifies the two stations — a broken operand
    // would make cavern report simple.a1 as undefined instead of solving.
    cave->equates()->appendEquate(cwEquate({nativeHandle(cave, QStringLiteral("1")),
                                            tripHandle(attached, QStringLiteral("simple.a1"))}));

    cwLinePlotManager manager;
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(attached->id(), attachDir);
    manager.externalCenterlineManager()->setTripAttachmentDirs(tripDirs);
    manager.setRegion(&region);
    manager.waitToFinish();

    INFO("solve error: " << manager.solveErrorMessage().toStdString());
    INFO("driver:\n" << manager.driverSource().toStdString());
    REQUIRE_FALSE(manager.hasSolveError());

    const cwStationPositionLookup& lookup = cave->stationPositionLookup();
    const QString externalKey = tripScopePrefix(attached) + QStringLiteral("simple.a1");

    REQUIRE(lookup.hasPosition(QStringLiteral("1")));
    REQUIRE(lookup.hasPosition(externalKey));

    const QVector3D nativePos = lookup.position(QStringLiteral("1"));
    const QVector3D externalPos = lookup.position(externalKey);
    CHECK(nativePos.x() == Catch::Approx(externalPos.x()).margin(0.001));
    CHECK(nativePos.y() == Catch::Approx(externalPos.y()).margin(0.001));
    CHECK(nativePos.z() == Catch::Approx(externalPos.z()).margin(0.001));

    // The native run stayed intact through the tie: "2" is 10 m from "1".
    REQUIRE(lookup.hasPosition(QStringLiteral("2")));
    const QVector3D twoPos = lookup.position(QStringLiteral("2"));
    CHECK((twoPos - nativePos).length() == Catch::Approx(10.0).margin(0.01));
}

TEST_CASE("A region equate emits a fully-qualified *equate at region scope",
          "[Equate][Emission]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    cwCave* caveA = addEmptyCave(region, QStringLiteral("Alpha"));
    addNativeTripWithShot(caveA, QStringLiteral("Native"),
                          QStringLiteral("1"), QStringLiteral("2"), 10.0);
    cwCave* caveB = addEmptyCave(region, QStringLiteral("Beta"));
    addNativeTripWithShot(caveB, QStringLiteral("Native"),
                          QStringLiteral("x"), QStringLiteral("y"), 10.0);

    // A cross-cave tie: Alpha's native "2" == Beta's native "y". Neither cave
    // knows the other's namespace, so each operand must carry its own
    // cave_<hex>. qualifier for the region-scope *equate to resolve it.
    region.equates()->appendEquate(cwEquate({nativeHandle(caveA, QStringLiteral("2")),
                                             nativeHandle(caveB, QStringLiteral("y"))}));

    cwSurvexExporterRegion::Options options;
    const QString driverPath = QDir(tempRoot.path()).absoluteFilePath(QStringLiteral("driver.svx"));
    const QString driver = regionDriverText(region, options, driverPath);

    const QString equateLine =
        QStringLiteral("*equate ")
        + cwCavernNaming::caveScopePrefix(caveA->id()) + QStringLiteral("2 ")
        + cwCavernNaming::caveScopePrefix(caveB->id()) + QStringLiteral("y");
    CHECK(driver.contains(equateLine));

    // The tie must sit at region scope: after BOTH cave blocks close (so both
    // qualified operands are declared) and before the region's outer *end.
    const QString caveAEnd = QStringLiteral("*end %1").arg(cwCavernNaming::caveName(caveA->id()));
    const QString caveBEnd = QStringLiteral("*end %1").arg(cwCavernNaming::caveName(caveB->id()));
    const int caveAEndIndex = driver.indexOf(caveAEnd);
    const int caveBEndIndex = driver.indexOf(caveBEnd);
    const int equateIndex = driver.indexOf(QStringLiteral("*equate"));
    const int outerEndIndex = driver.lastIndexOf(QStringLiteral("*end"));
    REQUIRE(caveAEndIndex >= 0);
    REQUIRE(caveBEndIndex >= 0);
    REQUIRE(equateIndex >= 0);
    CHECK(caveAEndIndex < equateIndex);
    CHECK(caveBEndIndex < equateIndex);
    CHECK(equateIndex < outerEndIndex);
}

TEST_CASE("A region equate draws stations in two caves coincident",
          "[Equate][Emission]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    cwCave* caveA = addEmptyCave(region, QStringLiteral("Alpha"));
    addNativeTripWithShot(caveA, QStringLiteral("Native"),
                          QStringLiteral("1"), QStringLiteral("2"), 10.0);
    cwCave* caveB = addEmptyCave(region, QStringLiteral("Beta"));
    // Beta's leg runs east while Alpha's runs north, so before the tie the two
    // derived stations sit 10 m apart on different axes — the tie is what makes
    // them coincident, not an accident of both landing at the origin.
    addNativeTripWithShot(caveB, QStringLiteral("Native"),
                          QStringLiteral("x"), QStringLiteral("y"), 10.0,
                          QStringLiteral("90.0"));

    region.equates()->appendEquate(cwEquate({nativeHandle(caveA, QStringLiteral("2")),
                                             nativeHandle(caveB, QStringLiteral("y"))}));

    cwLinePlotManager manager;
    manager.setRegion(&region);
    manager.waitToFinish();

    INFO("solve error: " << manager.solveErrorMessage().toStdString());
    INFO("driver:\n" << manager.driverSource().toStdString());
    REQUIRE_FALSE(manager.hasSolveError());

    // The tie crosses the cave boundary: each qualified operand decodes back
    // into its own cave's lookup (splitLookupByCave), so the coincidence check
    // reads "2" from Alpha and "y" from Beta.
    const cwStationPositionLookup& lookupA = caveA->stationPositionLookup();
    const cwStationPositionLookup& lookupB = caveB->stationPositionLookup();
    REQUIRE(lookupA.hasPosition(QStringLiteral("2")));
    REQUIRE(lookupB.hasPosition(QStringLiteral("y")));

    const QVector3D posA = lookupA.position(QStringLiteral("2"));
    const QVector3D posB = lookupB.position(QStringLiteral("y"));
    CHECK(posA.x() == Catch::Approx(posB.x()).margin(0.001));
    CHECK(posA.y() == Catch::Approx(posB.y()).margin(0.001));
    CHECK(posA.z() == Catch::Approx(posB.z()).margin(0.001));
}

TEST_CASE("A region equate ties three caves' stations coincident in one line",
          "[Equate][Emission]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    cwCave* caveA = addEmptyCave(region, QStringLiteral("Alpha"));
    addNativeTripWithShot(caveA, QStringLiteral("Native"),
                          QStringLiteral("1"), QStringLiteral("2"), 10.0);
    cwCave* caveB = addEmptyCave(region, QStringLiteral("Beta"));
    addNativeTripWithShot(caveB, QStringLiteral("Native"),
                          QStringLiteral("x"), QStringLiteral("y"), 10.0,
                          QStringLiteral("90.0"));
    cwCave* caveC = addEmptyCave(region, QStringLiteral("Gamma"));
    addNativeTripWithShot(caveC, QStringLiteral("Native"),
                          QStringLiteral("p"), QStringLiteral("q"), 10.0,
                          QStringLiteral("180.0"));

    // One region equate ties THREE stations, one per cave. The emission loop
    // must render all three qualified operands onto a single *equate line and
    // cavern must merge all three to one point. The three legs run north, east,
    // and south, so pre-tie the derived ends sit at distinct positions — only
    // the 3-way tie collapses them. Guards the N>2 operand path the deleted
    // cross-scope spike used to cover end-to-end.
    region.equates()->appendEquate(cwEquate({nativeHandle(caveA, QStringLiteral("2")),
                                             nativeHandle(caveB, QStringLiteral("y")),
                                             nativeHandle(caveC, QStringLiteral("q"))}));

    cwLinePlotManager manager;
    manager.setRegion(&region);
    manager.waitToFinish();

    INFO("solve error: " << manager.solveErrorMessage().toStdString());
    INFO("driver:\n" << manager.driverSource().toStdString());
    REQUIRE_FALSE(manager.hasSolveError());

    // All three operands land on one line, in cave order — a dropped or
    // reordered operand would fail here even if the solve still converged.
    const QString equateLine =
        QStringLiteral("*equate ")
        + cwCavernNaming::caveScopePrefix(caveA->id()) + QStringLiteral("2 ")
        + cwCavernNaming::caveScopePrefix(caveB->id()) + QStringLiteral("y ")
        + cwCavernNaming::caveScopePrefix(caveC->id()) + QStringLiteral("q");
    CHECK(manager.driverSource().contains(equateLine));

    // Each qualified operand decodes back into its own cave's lookup, all three
    // coincident.
    const cwStationPositionLookup& lookupA = caveA->stationPositionLookup();
    const cwStationPositionLookup& lookupB = caveB->stationPositionLookup();
    const cwStationPositionLookup& lookupC = caveC->stationPositionLookup();
    REQUIRE(lookupA.hasPosition(QStringLiteral("2")));
    REQUIRE(lookupB.hasPosition(QStringLiteral("y")));
    REQUIRE(lookupC.hasPosition(QStringLiteral("q")));

    const QVector3D posA = lookupA.position(QStringLiteral("2"));
    const QVector3D posB = lookupB.position(QStringLiteral("y"));
    const QVector3D posC = lookupC.position(QStringLiteral("q"));
    CHECK(posA.x() == Catch::Approx(posB.x()).margin(0.001));
    CHECK(posA.y() == Catch::Approx(posB.y()).margin(0.001));
    CHECK(posA.z() == Catch::Approx(posB.z()).margin(0.001));
    CHECK(posA.x() == Catch::Approx(posC.x()).margin(0.001));
    CHECK(posA.y() == Catch::Approx(posC.y()).margin(0.001));
    CHECK(posA.z() == Catch::Approx(posC.z()).margin(0.001));
}
