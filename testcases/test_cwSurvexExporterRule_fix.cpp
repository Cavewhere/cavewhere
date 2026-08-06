/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Qt
#include <QBuffer>
#include <QTextStream>

// Our includes
#include "cwSurvexExporterRule.h"
#include "cwSurveyDataArtifact.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwCoordinateTransform.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwErrorModel.h"
#include "cwErrorListModel.h"
#include "cwDistanceReading.h"
#include "cwCompassReading.h"
#include "cwClinoReading.h"

namespace {

cwStation makeStation(const QString& name)
{
    cwStation s;
    s.setName(name);
    return s;
}

cwSurveyChunk* makeChunk(const QStringList& stationNames)
{
    auto chunk = new cwSurveyChunk();
    for (const QString& n : stationNames) {
        chunk->appendNewShot();
        const int last = chunk->stationCount() - 1;
        if (last < 0) {
            continue;
        }
        cwStation s = makeStation(n);
        chunk->setStation(s, last);
    }
    return chunk;
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

QString writeRegionToString(const cwSurveyDataArtifact::Region& region)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    REQUIRE(buffer.open(QIODevice::WriteOnly));
    QTextStream stream(&bytes);
    auto result = cwSurvexExporterRule::writeRegion(stream, region);
    REQUIRE(!result.hasError());
    stream.flush();
    return QString::fromUtf8(bytes);
}

} // namespace

TEST_CASE("cwSurvexExporterRule emits no *cs out when no fix has an inputCS",
          "[cwSurvexExporterRule_fix]") {
    cwSurveyDataArtifact::Region region;

    cwSurveyDataArtifact::Cave cave;
    cave.name = QStringLiteral("TestCave");
    region.caves.append(cave);

    const QString output = writeRegionToString(region);
    CHECK_FALSE(output.contains(QStringLiteral("*cs out")));
}

TEST_CASE("cwSurvexExporterRule derives *cs out from the first fix's inputCS",
          "[cwSurvexExporterRule_fix]") {
    // An exported .svx is for somebody else to read, so its *cs out names a
    // system they can paste somewhere — derived from the fixes, never the
    // project's own frame, which is a local projection meaningful only here.
    // Cavern also rejects *cs without *cs out, so there has to be one.

    SECTION("single fix — fix.inputCS becomes *cs out") {
        cwSurveyDataArtifact::Region region;

        cwSurveyDataArtifact::Cave cave;
        cave.name = QStringLiteral("Nimbus");
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
        cwSurveyDataArtifact::Region region;

        cwSurveyDataArtifact::Cave caveA;
        caveA.name = QStringLiteral("CaveA");
        caveA.fixStations.append(makeFix("a1", QStringLiteral("EPSG:32616"),
                                         500000.0, 4000000.0, 100.0));
        region.caves.append(caveA);

        cwSurveyDataArtifact::Cave caveB;
        caveB.name = QStringLiteral("CaveB");
        caveB.fixStations.append(makeFix("b1", QStringLiteral("EPSG:32617"),
                                         500000.0, 4000000.0, 200.0));
        region.caves.append(caveB);

        const QString output = writeRegionToString(region);
        INFO(output.toStdString());
        CHECK(output.contains(QStringLiteral("*cs out EPSG:32616")));
        CHECK_FALSE(output.contains(QStringLiteral("*cs out EPSG:32617")));
    }

    SECTION("fix with empty inputCS is skipped when picking *cs out") {
        cwSurveyDataArtifact::Region region;

        cwSurveyDataArtifact::Cave cave;
        cave.name = QStringLiteral("Mixed");
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
        cwSurveyDataArtifact::Region region;

        cwSurveyDataArtifact::Cave cave;
        cave.name = QStringLiteral("Geographic");
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

        cwSurveyDataArtifact::Region region;

        cwSurveyDataArtifact::Cave cave;
        cave.name = QStringLiteral("Keyword");
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

TEST_CASE("cwSurvexExporterRule emits *cs and *fix per fix station",
          "[cwSurvexExporterRule_fix]") {
    cwSurveyDataArtifact::Region region;

    cwSurveyDataArtifact::Cave cave;
    cave.name = QStringLiteral("Multi");

    // Trips/chunks aren't required by writeFixStations; cave.fixStations
    // are written verbatim (validation already ran at snapshot construction).
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

TEST_CASE("cwSurvexExporterRule falls back to *fix firstStation 0 0 0 with no fixes",
          "[cwSurvexExporterRule_fix]") {
    auto unfixedCave = []() {
        cwSurveyDataArtifact::Cave cave;
        cave.name = QStringLiteral("Legacy");

        cwSurveyDataArtifact::Trip trip;
        trip.name = QStringLiteral("Trip1");
        trip.calibration.setBackSights(false);
        cwSurveyDataArtifact::SurveyChunk chunk;
        chunk.stations.append(makeStation(QStringLiteral("a1")));
        chunk.stations.append(makeStation(QStringLiteral("a2")));
        cwShot shot;
        shot.setDistance(cwDistanceReading(QStringLiteral("10")));
        shot.setCompass(cwCompassReading(QStringLiteral("90")));
        shot.setClino(cwClinoReading(QStringLiteral("0")));
        chunk.shots.append(shot);
        trip.chunks.append(chunk);
        cave.trips.append(trip);
        return cave;
    };

    SECTION("nothing in the region has a CS — pre-CS legacy behavior, no *cs emitted") {
        cwSurveyDataArtifact::Region region;
        region.caves.append(unfixedCave());

        const QString output = writeRegionToString(region);
        INFO(output.toStdString());
        CHECK(output.contains(QStringLiteral("*fix a1 0 0 0")));
        CHECK_FALSE(output.contains(QStringLiteral("\n*cs ")));
    }

    SECTION("another cave's fix supplies *cs out — emit *cs before the fallback so survex accepts it") {
        cwSurveyDataArtifact::Region region;

        cwSurveyDataArtifact::Cave fixedCave;
        fixedCave.name = QStringLiteral("Fixed");
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

TEST_CASE("cwSurveyDataArtifact::Cave validates fixes and appends cwError",
          "[cwSurvexExporterRule_fix]") {
    // Build a real cwCavingRegion with one cave that has stations a1, a2 and
    // three fixes: one valid (a1), one referring to an unknown station, one
    // duplicating a1. Snapshot construction should keep only a1 and append
    // two cwError entries on the cave.
    cwCavingRegion region;
    auto cave = new cwCave(&region);
    cave->setName(QStringLiteral("T"));

    auto trip = new cwTrip();
    auto chunk = new cwSurveyChunk();
    chunk->appendNewShot();
    chunk->appendNewShot();
    chunk->setStation(makeStation("a1"), 0);
    chunk->setStation(makeStation("a2"), 1);
    trip->addChunk(chunk);
    cave->addTrip(trip);

    cave->fixStations()->appendFixStation(
        makeFix("a1", QStringLiteral("EPSG:32616"), 100, 200, 0));
    cave->fixStations()->appendFixStation(
        makeFix("ghost", QStringLiteral("EPSG:32616"), 300, 400, 0));
    cave->fixStations()->appendFixStation(
        makeFix("A1", QStringLiteral("EPSG:32616"), 500, 600, 0)); // dup of a1 (case-insensitive)

    region.addCave(cave);

    // Building the Region snapshot triggers per-cave validation.
    cwSurveyDataArtifact::Region snapshot(&region);

    REQUIRE(snapshot.caves.size() == 1);
    const auto& snapshotCave = snapshot.caves.at(0);
    REQUIRE(snapshotCave.fixStations.size() == 1);
    CHECK(snapshotCave.fixStations.first().stationName() == QStringLiteral("a1"));

    // Two errors: unknown station + duplicate.
    auto* errors = cave->errorModel()->errors();
    REQUIRE(errors->rowCount(QModelIndex()) == 2);
}

TEST_CASE("cwSurveyDataArtifact::Cave drops a fix whose coordinate can't be read",
          "[cwSurvexExporterRule_fix]") {
    // Only a Valid fix has components — every other state reads 0. Writing one
    // anyway would emit `*fix a1 0 0 0` and move the whole cave to the origin,
    // silently, so it is dropped with a reason instead. The row a2 keeps its
    // numbers as text; what it lacks is a system to read them under.
    cwCavingRegion region;
    auto cave = new cwCave(&region);
    cave->setName(QStringLiteral("T"));

    auto trip = new cwTrip();
    auto chunk = new cwSurveyChunk();
    chunk->appendNewShot();
    chunk->appendNewShot();
    chunk->setStation(makeStation("a1"), 0);
    chunk->setStation(makeStation("a2"), 1);
    trip->addChunk(chunk);
    cave->addTrip(trip);

    cave->fixStations()->appendFixStation(
        makeFix("a1", QStringLiteral("EPSG:32616"), 500000, 4000000, 100));

    const cwFixStation noSystem = makeFix("a2", QString(), 610016.792, 5615117.075, 304);
    REQUIRE(noSystem.state() == cwFixStation::NoSystem);
    REQUIRE_FALSE(noSystem.coordinate().isEmpty());
    cave->fixStations()->appendFixStation(noSystem);

    region.addCave(cave);

    cwSurveyDataArtifact::Region snapshot(&region);

    REQUIRE(snapshot.caves.size() == 1);
    const auto& snapshotCave = snapshot.caves.at(0);
    REQUIRE(snapshotCave.fixStations.size() == 1);
    CHECK(snapshotCave.fixStations.first().stationName() == QStringLiteral("a1"));

    auto* errors = cave->errorModel()->errors();
    REQUIRE(errors->rowCount(QModelIndex()) == 1);

    // And the station it dropped never reaches the file, at the origin or
    // anywhere else — the good fix is still written in full.
    cwSurveyDataArtifact::Region exported;
    exported.caves.append(snapshotCave);
    const QString output = writeRegionToString(exported);
    INFO(output.toStdString());
    CHECK(output.contains(QStringLiteral("*fix a1 500000.000000000 4000000.000000000 100.000000000")));
    CHECK_FALSE(output.contains(QStringLiteral("a2")));
}

TEST_CASE("cwSurvexExporterRule writes a geographic fix to the last digit the user typed",
          "[cwSurvexExporterRule_fix]") {
    // Degrees, so the decimals that are micrometers in UTM are centimeters
    // here. Cutting them at six put this cave's a1 ~5 cm off the LiDAR scan it
    // was picked from; the 7th below has to reach the file.
    cwSurveyDataArtifact::Region region;

    cwSurveyDataArtifact::Cave cave;
    cave.name = QStringLiteral("Iron Gorge");
    cave.fixStations.append(makeFix("a1", QStringLiteral("EPSG:4326"),
                                    -121.8305843, 51.1140816, 2198.010));
    region.caves.append(cave);

    const QString output = writeRegionToString(region);
    INFO(output.toStdString());
    CHECK(output.contains(QStringLiteral("*fix a1 -121.830584300 51.114081600 2198.010000000")));
}
