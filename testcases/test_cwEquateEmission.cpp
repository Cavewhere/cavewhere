/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Commit 4 of plans/EXTERNAL_FILE_EQUATES_AND_SCOPING.html: cave-level
// *equate emission. A cwCave equate renders as a bare *equate line at cave
// scope inside the driver, with cave-relative operands — a NativeCave
// handle as its tail, a Trip handle as "<scopePrefix><tail>". The solve
// then merges the tied stations to one coordinate, which is the payoff:
// a native station and an externally-attached station drawn coincident.

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

// A one-shot native trip (station `fromName` -> `toName`, level, magnetic
// north). It carries no fix, so on its own it floats — an equate is what
// ties it to the fixed external component in the coincidence test.
cwTrip* addNativeTripWithShot(cwCave* cave,
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
