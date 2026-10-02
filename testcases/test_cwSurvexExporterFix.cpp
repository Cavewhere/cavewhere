/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Qt
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSet>

// Our includes
#include "cwSurvexExporterRegion.h"
#include "cwSurvexExporterUtils.h"
#include "cwCavingRegionData.h"
#include "cwCoordinateTransform.h"
#include "cwFixStation.h"
#include "cwStation.h"
#include "cwShot.h"

namespace {

cwStation makeStation(const QString& name)
{
    cwStation s;
    s.setName(name);
    return s;
}

//! A trip holding \a stationNames and nothing else worth writing — its shots
//! carry no readings, so the trip contributes no data lines to compete with
//! the *cs / *fix block under test.
cwTripData tripWithStations(const QStringList& stationNames)
{
    cwTripData trip;
    trip.name = QStringLiteral("Trip1");
    trip.calibrations.setBackSights(false);

    cwSurveyChunkData chunk;
    for (const QString& name : stationNames) {
        chunk.stations.append(makeStation(name));
    }
    for (int i = 1; i < stationNames.size(); i++) {
        chunk.shots.append(cwShot());
    }
    trip.chunks.append(chunk);
    return trip;
}

cwCaveData makeCave(const QString& name, const QStringList& stationNames)
{
    cwCaveData cave;
    cave.name = name;
    cave.trips.append(tripWithStations(stationNames));
    return cave;
}

cwFixStation makeFix(const QString& name, const QString& cs, double e, double n, double el)
{
    cwFixStation f;
    f.setStationName(name);
    f.setInputCS(cs);
    // One call, so the numbers survive even when cs is blank — set one at a
    // time they would collapse to 0 and the fix would be about nothing.
    f.setCoordinate(e, n, el);
    return f;
}

//! The region exported the way the export menu does it, read back as text.
//! Concurrent test processes each need their own output file.
QString writeRegionToString(const cwCavingRegionData& region)
{
    const QString path = QDir::temp().filePath(
        QStringLiteral("cwSurvexExporterFix-%1.svx").arg(QCoreApplication::applicationPid()));

    cwSurvexExporterRegion::Options options;
    options.outputCSPolicy = cwSurvexExporterRegion::OutputCSPolicy::Shareable;
    const auto result = cwSurvexExporterRegion::exportRegion(region, path, options);
    INFO(result.errorMessage().toStdString());
    REQUIRE_FALSE(result.hasError());

    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    const QString text = QString::fromUtf8(file.readAll());
    file.close();
    QFile::remove(path);
    return text;
}

} // namespace

TEST_CASE("Survex export emits no *cs out when no fix has an inputCS",
          "[cwSurvexExporterFix]") {
    cwCavingRegionData region;
    region.caves.append(makeCave(QStringLiteral("TestCave"),
                                 {QStringLiteral("a1"), QStringLiteral("a2")}));

    const QString output = writeRegionToString(region);
    CHECK_FALSE(output.contains(QStringLiteral("*cs out")));
}

TEST_CASE("Survex export derives *cs out from the first fix's inputCS",
          "[cwSurvexExporterFix]") {
    // An exported .svx is for somebody else to read, so its *cs out names a
    // system they can paste somewhere — derived from the fixes, never the
    // project's own frame, which is a local projection meaningful only here.
    // Cavern also rejects *cs without *cs out, so there has to be one.

    SECTION("single fix — fix.inputCS becomes *cs out") {
        cwCavingRegionData region;

        cwCaveData cave = makeCave(QStringLiteral("Nimbus"),
                                   {QStringLiteral("a0"), QStringLiteral("a1")});
        cave.fixStations.append(makeFix("a0", QStringLiteral("EPSG:6653"),
                                        288777.04, 5474149.93, 380.1));
        region.caves.append(cave);

        const QString output = writeRegionToString(region);
        INFO(output.toStdString());

        const int csOutIdx = output.indexOf(QStringLiteral("*cs out EPSG:6653"));
        const int fixIdx   = output.indexOf(QStringLiteral("*fix a0"));
        REQUIRE(csOutIdx >= 0);
        REQUIRE(fixIdx > csOutIdx);
    }

    SECTION("multiple fixes — first non-empty inputCS wins") {
        cwCavingRegionData region;

        cwCaveData caveA = makeCave(QStringLiteral("CaveA"),
                                    {QStringLiteral("a1"), QStringLiteral("a2")});
        caveA.fixStations.append(makeFix("a1", QStringLiteral("EPSG:32616"),
                                         500000.0, 4000000.0, 100.0));
        region.caves.append(caveA);

        cwCaveData caveB = makeCave(QStringLiteral("CaveB"),
                                    {QStringLiteral("b1"), QStringLiteral("b2")});
        caveB.fixStations.append(makeFix("b1", QStringLiteral("EPSG:32617"),
                                         500000.0, 4000000.0, 200.0));
        region.caves.append(caveB);

        const QString output = writeRegionToString(region);
        INFO(output.toStdString());
        CHECK(output.contains(QStringLiteral("*cs out EPSG:32616")));
        CHECK_FALSE(output.contains(QStringLiteral("*cs out EPSG:32617")));
    }

    SECTION("fix with empty inputCS is skipped when picking *cs out") {
        cwCavingRegionData region;

        cwCaveData cave = makeCave(QStringLiteral("Mixed"),
                                   {QStringLiteral("a1"), QStringLiteral("a2")});
        // First fix has no inputCS — picker should skip it and use the
        // next fix that does carry one.
        cave.fixStations.append(makeFix("a1", QString(),
                                        100.0, 200.0, 0.0));
        cave.fixStations.append(makeFix("a2", QStringLiteral("EPSG:32616"),
                                        500000.0, 4000000.0, 0.0));
        region.caves.append(cave);

        const QString output = writeRegionToString(region);
        INFO(output.toStdString());
        CHECK(output.contains(QStringLiteral("*cs out EPSG:32616")));
    }

    SECTION("a geographic fix contributes the UTM zone containing it") {
        // Cavern refuses a geographic *cs out outright ("Coordinate system
        // unsuitable for output", survex/src/commands.c:2672), and new rows
        // start on WGS84 — so the zone containing the fix stands in for one.
        cwCavingRegionData region;

        cwCaveData cave = makeCave(QStringLiteral("Geographic"),
                                   {QStringLiteral("a1"), QStringLiteral("a2")});
        cave.fixStations.append(makeFix("a1", QStringLiteral("EPSG:4326"),
                                        -115.59902, 46.12113, 300.0));
        region.caves.append(cave);

        const QString output = writeRegionToString(region);
        INFO(output.toStdString());
        CHECK(output.contains(QStringLiteral("*cs out EPSG:32611")));
        CHECK_FALSE(output.contains(QStringLiteral("*cs out EPSG:4326")));
    }

    SECTION("a system PROJ can't read leaves the choice to the next fix") {
        // Importers translate their format's spelling into PROJ's before a fix
        // is built, so a system PROJ can't read is one nothing can place —
        // there is no reading of it that cavern would accept either. Offering
        // it as *cs out anyway let a typo shadow a perfectly good fix sitting
        // right behind it, and the whole solve failed on the strength of the
        // first row.
        REQUIRE_FALSE(cwCoordinateTransform::isValidCS(QStringLiteral("UTM 16 N")));

        cwCavingRegionData region;

        cwCaveData cave = makeCave(QStringLiteral("Keyword"),
                                   {QStringLiteral("a1"), QStringLiteral("a2")});
        cave.fixStations.append(makeFix("a1", QStringLiteral("UTM 16 N"),
                                        500000.0, 4000000.0, 0.0));
        cave.fixStations.append(makeFix("a2", QStringLiteral("EPSG:32616"),
                                        500000.0, 4000000.0, 0.0));
        region.caves.append(cave);

        const QString output = writeRegionToString(region);
        INFO(output.toStdString());
        CHECK(output.contains(QStringLiteral("*cs out EPSG:32616")));
        CHECK_FALSE(output.contains(QStringLiteral("*cs out UTM 16 N")));
    }

}

TEST_CASE("Survex export emits *cs and *fix per fix station",
          "[cwSurvexExporterFix]") {
    cwCavingRegionData region;

    cwCaveData cave = makeCave(QStringLiteral("Multi"),
                               {QStringLiteral("a1"), QStringLiteral("a2"), QStringLiteral("b1")});
    cave.fixStations.append(makeFix("a1", QStringLiteral("EPSG:32616"),
                                    500000.0, 4000000.0, 100.0));
    cave.fixStations.append(makeFix("a2", QStringLiteral("EPSG:32616"),
                                    500100.0, 4000050.0, 110.0));
    cave.fixStations.append(makeFix("b1", QStringLiteral("EPSG:4326"),
                                    -85.0, 36.0, 200.0));
    region.caves.append(cave);

    const QString output = writeRegionToString(region);
    INFO(output.toStdString());

    // *cs out at region level
    CHECK(output.contains(QStringLiteral("*cs out EPSG:32616")));

    // First *cs/*fix block: EPSG:32616 with two fixes
    const int csUtmIdx = output.indexOf(QStringLiteral("*cs EPSG:32616"));
    REQUIRE(csUtmIdx >= 0);
    const int fixA1Idx = output.indexOf(QStringLiteral("*fix a1"));
    REQUIRE(fixA1Idx > csUtmIdx);
    const int fixA2Idx = output.indexOf(QStringLiteral("*fix a2"));
    REQUIRE(fixA2Idx > fixA1Idx);

    // Second *cs/*fix block: EPSG:4326 with one fix; *cs only re-emitted
    // when the inputCS changes.
    const int csGeoIdx = output.indexOf(QStringLiteral("*cs EPSG:4326"));
    REQUIRE(csGeoIdx > fixA2Idx);
    const int fixB1Idx = output.indexOf(QStringLiteral("*fix b1"));
    REQUIRE(fixB1Idx > csGeoIdx);

    // No legacy zero-fix sneaks in when explicit fixes exist.
    CHECK_FALSE(output.contains(QStringLiteral(" 0 0 0")));
}

TEST_CASE("Survex export falls back to *fix firstStation 0 0 0 with no fixes",
          "[cwSurvexExporterFix]") {
    auto unfixedCave = []() {
        return makeCave(QStringLiteral("Legacy"),
                        {QStringLiteral("a1"), QStringLiteral("a2")});
    };

    SECTION("nothing in the region has a CS — pre-CS legacy behavior, no *cs emitted") {
        cwCavingRegionData region;
        region.caves.append(unfixedCave());

        const QString output = writeRegionToString(region);
        INFO(output.toStdString());
        CHECK(output.contains(QStringLiteral("*fix a1 0 0 0")));
        CHECK_FALSE(output.contains(QStringLiteral("\n*cs ")));
    }

    SECTION("another cave's fix supplies *cs out — emit *cs before the fallback so survex accepts it") {
        cwCavingRegionData region;

        cwCaveData fixedCave = makeCave(QStringLiteral("Fixed"),
                                        {QStringLiteral("f1"), QStringLiteral("f2")});
        fixedCave.fixStations.append(makeFix("f1", QStringLiteral("EPSG:32616"),
                                             500000.0, 4000000.0, 0.0));
        region.caves.append(fixedCave);
        region.caves.append(unfixedCave());

        const QString output = writeRegionToString(region);
        INFO(output.toStdString());
        const int csIdx  = output.indexOf(QStringLiteral("*cs EPSG:32616"),
                                          output.indexOf(QStringLiteral("*fix f1")));
        const int fixIdx = output.indexOf(QStringLiteral("*fix a1 0 0 0"));
        REQUIRE(csIdx >= 0);
        REQUIRE(fixIdx > csIdx);
    }
}

TEST_CASE("Fix validation drops fixes on unknown and duplicated stations",
          "[cwSurvexExporterFix]") {
    // Stations a1, a2 and three fixes: one valid (a1), one referring to an
    // unknown station, one duplicating a1. Only a1 survives, and each drop
    // says why.
    const QSet<QString> stationNames = {cwStation::canonicalKey(QStringLiteral("a1")),
                                        cwStation::canonicalKey(QStringLiteral("a2"))};

    const QList<cwFixStation> fixes = {
        makeFix("a1", QStringLiteral("EPSG:32616"), 100, 200, 0),
        makeFix("ghost", QStringLiteral("EPSG:32616"), 300, 400, 0),
        makeFix("A1", QStringLiteral("EPSG:32616"), 500, 600, 0) // dup of a1 (case-insensitive)
    };

    QStringList errors;
    const QList<cwFixStation> kept =
        cwSurvexExporterUtils::validateFixStations(fixes, stationNames, errors);

    REQUIRE(kept.size() == 1);
    CHECK(kept.first().stationName() == QStringLiteral("a1"));
    INFO(errors.join('\n').toStdString());
    CHECK(errors.size() == 2);
}

TEST_CASE("Survex export drops a fix whose coordinate can't be read",
          "[cwSurvexExporterFix]") {
    // Only a Valid fix has components — every other state reads 0. Writing one
    // anyway would emit `*fix a1 0 0 0` and move the whole cave to the origin,
    // silently, so it is dropped instead. The row a2 keeps its numbers as
    // text; what it lacks is a system to read them under.
    cwCavingRegionData region;

    cwCaveData cave = makeCave(QStringLiteral("T"),
                               {QStringLiteral("a1"), QStringLiteral("a2")});
    cave.fixStations.append(makeFix("a1", QStringLiteral("EPSG:32616"),
                                    500000, 4000000, 100));

    const cwFixStation noSystem = makeFix("a2", QString(), 610016.792, 5615117.075, 304);
    REQUIRE(noSystem.state() == cwFixStation::NoSystem);
    REQUIRE_FALSE(noSystem.coordinate().isEmpty());
    cave.fixStations.append(noSystem);

    region.caves.append(cave);

    // The station it dropped never reaches the file, at the origin or
    // anywhere else — the good fix is still written in full.
    const QString output = writeRegionToString(region);
    INFO(output.toStdString());
    CHECK(output.contains(QStringLiteral("*fix a1 500000.000000000 4000000.000000000 100.000000000")));
    CHECK_FALSE(output.contains(QStringLiteral("a2")));
}

TEST_CASE("Survex export writes a geographic fix to the last digit the user typed",
          "[cwSurvexExporterFix]") {
    // Degrees, so the decimals that are micrometers in UTM are centimeters
    // here. Cutting them at six put this cave's a1 ~5 cm off the LiDAR scan it
    // was picked from; the 7th below has to reach the file.
    cwCavingRegionData region;

    cwCaveData cave = makeCave(QStringLiteral("Iron Gorge"),
                               {QStringLiteral("a1"), QStringLiteral("a2")});
    cave.fixStations.append(makeFix("a1", QStringLiteral("EPSG:4326"),
                                    -121.8305843, 51.1140816, 2198.010));
    region.caves.append(cave);

    const QString output = writeRegionToString(region);
    INFO(output.toStdString());
    CHECK(output.contains(QStringLiteral("*fix a1 -121.830584300 51.114081600 2198.010000000")));
}
