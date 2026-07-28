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

TEST_CASE("cwSurvexExporterRule emits *cs out at region level when globalCS is set",
          "[cwSurvexExporterRule_fix]") {
    cwSurveyDataArtifact::Region region;
    region.globalCoordinateSystem =QStringLiteral("EPSG:32616");

    cwSurveyDataArtifact::Cave cave;
    cave.name = QStringLiteral("TestCave");
    region.caves.append(cave);

    const QString output = writeRegionToString(region);
    INFO(output.toStdString());
    CHECK(output.contains(QStringLiteral("*cs out EPSG:32616")));
}

TEST_CASE("cwSurvexExporterRule emits no *cs out when globalCS is empty and no fix has inputCS",
          "[cwSurvexExporterRule_fix]") {
    cwSurveyDataArtifact::Region region;

    cwSurveyDataArtifact::Cave cave;
    cave.name = QStringLiteral("TestCave");
    region.caves.append(cave);

    const QString output = writeRegionToString(region);
    CHECK_FALSE(output.contains(QStringLiteral("*cs out")));
}

TEST_CASE("cwSurvexExporterRule derives *cs out from the first fix's inputCS when globalCS is empty",
          "[cwSurvexExporterRule_fix]") {
    // Reproduces the user's nimbus.cwproj scenario: the picker never set
    // a region globalCS, but a cave has a fix carrying its own inputCS.
    // Cavern rejects *cs without *cs out — so the exporter must fall back
    // to the fix's CS for *cs out, otherwise the line plot fails with
    // "input projection is set but output projection isn't".

    SECTION("single fix — fix.inputCS becomes *cs out") {
        cwSurveyDataArtifact::Region region;
        // region.globalCS intentionally left empty.

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

    SECTION("a geographic fix CS is no fallback — cavern refuses it for output") {
        // "Coordinate system unsuitable for output" (survex/src/commands.c:2672)
        // fails the entire solve, so emitting one is worse than emitting none.
        // New fix-station rows start on WGS84, making this the ordinary shape of
        // a project whose global CS was never set; cwFixStationValidator's
        // needsOutputCS prompt is what asks the user to pick a projected one.
        cwSurveyDataArtifact::Region region;

        cwSurveyDataArtifact::Cave cave;
        cave.name = QStringLiteral("Geographic");
        cave.fixStations.append(makeFix("a1", QStringLiteral("EPSG:4326"),
                                        -115.59902, 46.12113, 300.0));
        region.caves.append(cave);

        const QString output = writeRegionToString(region);
        INFO(output.toStdString());
        CHECK_FALSE(output.contains(QStringLiteral("*cs out")));
    }

    SECTION("a projected fix behind a geographic one is still found") {
        // The premise for the section above: geographic is skipped, not a stop.
        cwSurveyDataArtifact::Region region;

        cwSurveyDataArtifact::Cave cave;
        cave.name = QStringLiteral("Mixed");
        cave.fixStations.append(makeFix("a1", QStringLiteral("EPSG:4326"),
                                        -115.59902, 46.12113, 300.0));
        cave.fixStations.append(makeFix("a2", QStringLiteral("EPSG:32616"),
                                        500000.0, 4000000.0, 0.0));
        region.caves.append(cave);

        const QString output = writeRegionToString(region);
        INFO(output.toStdString());
        CHECK(output.contains(QStringLiteral("*cs out EPSG:32616")));
    }

    SECTION("a survex keyword cavern won't output is skipped too") {
        // LONG-LAT and JTSK are survex's own spellings, and cavern refuses both
        // for output (ok_for_output == NO, survex/src/commands.c:2521-2540).
        // PROJ can't create either, and isGeographic() answers false for
        // anything it fails to create — so before this they read as projected
        // and were picked for *cs out, failing the solve even though a usable
        // CS sat right behind them. An svx import stores *cs verbatim, so this
        // is what round-tripping such a file looks like.
        // The premise: neither is something PROJ can answer for, so
        // isGeographic() alone can never rule them out.
        REQUIRE_FALSE(cwCoordinateTransform::isValidCS(QStringLiteral("LONG-LAT")));
        REQUIRE_FALSE(cwCoordinateTransform::isValidCS(QStringLiteral("JTSK")));

        cwSurveyDataArtifact::Region region;

        cwSurveyDataArtifact::Cave cave;
        cave.name = QStringLiteral("Keyword");
        cave.fixStations.append(makeFix("a1", QStringLiteral("LONG-LAT"),
                                        -115.59902, 46.12113, 300.0));
        cave.fixStations.append(makeFix("a2", QStringLiteral("JTSK"),
                                        -700000.0, -1000000.0, 400.0));
        cave.fixStations.append(makeFix("a3", QStringLiteral("EPSG:32616"),
                                        500000.0, 4000000.0, 0.0));
        region.caves.append(cave);

        const QString output = writeRegionToString(region);
        INFO(output.toStdString());
        CHECK(output.contains(QStringLiteral("*cs out EPSG:32616")));
        CHECK_FALSE(output.contains(QStringLiteral("*cs out LONG-LAT")));
        CHECK_FALSE(output.contains(QStringLiteral("*cs out JTSK")));
    }

    SECTION("a survex keyword cavern will output is still a fallback") {
        // The premise for the section above: PROJ failing to parse a CS is not
        // itself a reason to skip it. UTM16N, S-MERC, OSGB and EUR79Z30 are all
        // unknown to PROJ and all fine for cavern's output, so a blanket
        // "PROJ must understand it" gate would leave these exports with no
        // *cs out at all — the very failure the fallback exists to prevent.
        REQUIRE_FALSE(cwCoordinateTransform::isValidCS(QStringLiteral("UTM16N")));

        cwSurveyDataArtifact::Region region;

        cwSurveyDataArtifact::Cave cave;
        cave.name = QStringLiteral("Keyword");
        cave.fixStations.append(makeFix("a1", QStringLiteral("UTM16N"),
                                        500000.0, 4000000.0, 0.0));
        region.caves.append(cave);

        const QString output = writeRegionToString(region);
        INFO(output.toStdString());
        CHECK(output.contains(QStringLiteral("*cs out UTM16N")));
    }

    SECTION("explicit globalCS overrides any fix inputCS") {
        cwSurveyDataArtifact::Region region;
        region.globalCoordinateSystem =QStringLiteral("EPSG:32616");

        cwSurveyDataArtifact::Cave cave;
        cave.name = QStringLiteral("Override");
        // Fix uses a different CS — globalCS should still win for *cs out.
        cave.fixStations.append(makeFix("a1", QStringLiteral("EPSG:32617"),
                                        500000.0, 4000000.0, 0.0));
        region.caves.append(cave);

        const QString output = writeRegionToString(region);
        INFO(output.toStdString());
        CHECK(output.contains(QStringLiteral("*cs out EPSG:32616")));
        CHECK_FALSE(output.contains(QStringLiteral("*cs out EPSG:32617")));
    }
}

TEST_CASE("cwSurvexExporterRule emits *cs and *fix per fix station",
          "[cwSurvexExporterRule_fix]") {
    cwSurveyDataArtifact::Region region;
    region.globalCoordinateSystem =QStringLiteral("EPSG:32616");

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
    auto buildRegion = [](const QString& globalCS) {
        cwSurveyDataArtifact::Region region;
        region.globalCoordinateSystem =globalCS;

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

        region.caves.append(cave);
        return region;
    };

    SECTION("no globalCS — pre-CS legacy behavior, no *cs emitted") {
        const QString output = writeRegionToString(buildRegion(QString()));
        INFO(output.toStdString());
        CHECK(output.contains(QStringLiteral("*fix a1 0 0 0")));
        CHECK_FALSE(output.contains(QStringLiteral("\n*cs ")));
    }

    SECTION("globalCS set — emit *cs <globalCS> before fallback so survex accepts the *fix under *cs out") {
        const QString output = writeRegionToString(buildRegion(QStringLiteral("EPSG:32616")));
        INFO(output.toStdString());
        const int csIdx  = output.indexOf(QStringLiteral("*cs EPSG:32616"));
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
    CHECK(output.contains(QStringLiteral("*fix a1 500000.000000 4000000.000000 100.000000")));
    CHECK_FALSE(output.contains(QStringLiteral("a2")));
}
