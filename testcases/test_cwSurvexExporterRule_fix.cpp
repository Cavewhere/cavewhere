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
    f.setEasting(e);
    f.setNorthing(n);
    f.setElevation(el);
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
    region.globalCS = QStringLiteral("EPSG:32616");

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

    SECTION("explicit globalCS overrides any fix inputCS") {
        cwSurveyDataArtifact::Region region;
        region.globalCS = QStringLiteral("EPSG:32616");

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
    region.globalCS = QStringLiteral("EPSG:32616");

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
        region.globalCS = globalCS;

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
