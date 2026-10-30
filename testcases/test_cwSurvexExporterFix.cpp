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
#include <QTextStream>

// Our includes
#include "cwScopeLabels.h"
#include "cwSurvexExporterCaveTask.h"
#include "cwSurvexExporterRegion.h"
#include "cwSurvexExporterUtils.h"
#include "cwCavingRegionData.h"
#include "cwCoordinateTransform.h"
#include "cwEntrance.h"
#include "cwFixStation.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwSurvexCS.h"
#include "LoadProjectHelper.h"

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
    trip.id = QUuid::createUuid();
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
    cave.id = QUuid::createUuid();
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

cwEntrance makeEntrance(const QString& stationName)
{
    cwEntrance entrance;
    entrance.setStationName(stationName);
    return entrance;
}

//! The region exported with \a options, read back as text. Concurrent test
//! processes each need their own output file.
QString writeRegionToString(const cwCavingRegionData& region,
                            const cwSurvexExporterRegion::Options& options)
{
    const QString path = QDir::temp().filePath(
        QStringLiteral("cwSurvexExporterFix-%1.svx").arg(QCoreApplication::applicationPid()));

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

//! The region exported the way the export menu does it, read back as text.
QString writeRegionToString(const cwCavingRegionData& region,
                            cwSurvexExporterRegion::OutputCSPolicy policy =
                                cwSurvexExporterRegion::OutputCSPolicy::Shareable)
{
    cwSurvexExporterRegion::Options options;
    options.outputCSPolicy = policy;
    return writeRegionToString(region, options);
}

//! A derived node of a sourced subtree: \a cavernLabel is the naming level
//! cavern opens for it, and \a sourcePath its place inside the source.
cwCaveData derivedNode(const cwCaveData& root, const QString& name,
                       const QString& cavernLabel, const QString& sourcePath)
{
    cwCaveData node;
    node.id = QUuid::createUuid();
    node.name = name;
    node.sourceId = root.sourceId;
    node.sourcePath = sourcePath;
    node.cavernLabel = cavernLabel;
    return node;
}

//! survex_nested_entrance.svx attached as a source root, with the harvest's
//! stations: outer nests inner, which nests side. The caller nests the
//! derived nodes the scan builds for the blocks.
cwCaveData nestedEntranceRoot()
{
    cwCaveData root;
    root.id = QUuid::createUuid();
    root.name = QStringLiteral("Nested");
    root.sourceId = QUuid::createUuid();
    root.externalCenterline = cwExternalCenterline(QStringLiteral("survex_nested_entrance.svx"));
    root.externalStations = {QStringLiteral("outer.o1"), QStringLiteral("outer.o2"),
                             QStringLiteral("outer.inner.i1"), QStringLiteral("outer.inner.i2"),
                             QStringLiteral("outer.inner.side.s1"),
                             QStringLiteral("outer.inner.side.s2")};
    return root;
}

cwSurvexExporterRegion::Options attachedOptions(const cwCaveData& root)
{
    cwSurvexExporterRegion::Options options;
    options.caveAttachmentDirs.insert(
        root.id, testcasesDatasetSourcePath(QStringLiteral("external-centerlines")));
    options.externalFixedStations.insert(root.id, {QStringLiteral("outer.o1")});
    return options;
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

TEST_CASE("Fix validation judges duplicates by the station each name resolves to",
          "[cwSurvexExporterFix][CasePairs]") {
    // P2 and p2 are a case pair, two stations; q1 has no pair.
    const QSet<QString> stationKeys = {QStringLiteral("P2"), QStringLiteral("p2"),
                                       QStringLiteral("q1")};

    SECTION("Fixes on both members of a pair are two fixes") {
        const QList<cwFixStation> fixes = {
            makeFix("P2", QStringLiteral("EPSG:32616"), 100, 200, 0),
            makeFix("p2", QStringLiteral("EPSG:32616"), 100, 230, 0),
            makeFix("Q1", QStringLiteral("EPSG:32616"), 300, 400, 0),
        };
        QStringList errors;
        const QList<cwFixStation> kept =
            cwSurvexExporterUtils::validateFixStations(fixes, stationKeys, errors);
        INFO(errors.join('\n').toStdString());
        CHECK(errors.isEmpty());
        REQUIRE(kept.size() == 3);
        CHECK(kept.at(0).stationName() == QStringLiteral("P2"));
        CHECK(kept.at(1).stationName() == QStringLiteral("p2"));
    }

    SECTION("A name typed in another case of one member still duplicates it") {
        const QList<cwFixStation> fixes = {
            makeFix("q1", QStringLiteral("EPSG:32616"), 100, 200, 0),
            makeFix("Q1", QStringLiteral("EPSG:32616"), 300, 400, 0),
        };
        QStringList errors;
        const QList<cwFixStation> kept =
            cwSurvexExporterUtils::validateFixStations(fixes, stationKeys, errors);
        CHECK(kept.size() == 1);
        CHECK(errors == QStringList({QStringLiteral("Duplicate fix on station: \"Q1\"")}));
    }

    SECTION("A spelling that matches a pair with no lowercase member only by case is unknown") {
        const QSet<QString> mixedPair = {QStringLiteral("BB2"), QStringLiteral("Bb2")};
        QStringList errors;
        const QList<cwFixStation> kept = cwSurvexExporterUtils::validateFixStations(
            {makeFix("bB2", QStringLiteral("EPSG:32616"), 100, 200, 0)}, mixedPair, errors);
        CHECK(kept.isEmpty());
        CHECK(errors == QStringList({QStringLiteral("Fix references unknown station: \"bB2\"")}));
    }
}

TEST_CASE("stationKeysBySpelling keys a case pair by its spelling and the rest by canonical key",
          "[cwSurvexExporterFix][CasePairs]") {
    const QStringList spellings = {QStringLiteral("P2"), QStringLiteral("p2"),
                                   QStringLiteral("Q1"), QStringLiteral("xy.P3")};

    const QHash<QString, QString> keys =
        cwSurvexExporterUtils::stationKeysBySpelling(QStringLiteral("Trip1."), spellings);
    CHECK(keys.size() == spellings.size());
    CHECK(keys.value(QStringLiteral("P2")).toStdString() == "trip1.P2");
    CHECK(keys.value(QStringLiteral("p2")).toStdString() == "trip1.p2");
    CHECK(keys.value(QStringLiteral("Q1")).toStdString() == "trip1.q1");
    CHECK(keys.value(QStringLiteral("xy.P3")).toStdString() == "trip1.xy.p3");

    const QSet<QString> scoped =
        cwSurvexExporterUtils::scopedStationKeys(QStringLiteral("Trip1."), spellings);
    CHECK(scoped
          == QSet<QString>({QStringLiteral("trip1.P2"), QStringLiteral("trip1.p2"),
                            QStringLiteral("trip1.q1"), QStringLiteral("trip1.xy.p3")}));
}

TEST_CASE("keepsCase is true for a name with a capital", "[cwSurvexExporterFix][CasePairs]") {
    CHECK(cwSurvexExporterUtils::keepsCase(QStringLiteral("K1")));
    CHECK(cwSurvexExporterUtils::keepsCase(QStringLiteral("XY.P2")));
    CHECK(cwSurvexExporterUtils::keepsCase(QStringLiteral("cave.Foo.p2")));
    CHECK_FALSE(cwSurvexExporterUtils::keepsCase(QStringLiteral("k1")));
    CHECK_FALSE(cwSurvexExporterUtils::keepsCase(QStringLiteral("xy.p2")));
    CHECK_FALSE(cwSurvexExporterUtils::keepsCase(QStringLiteral("12")));
}

namespace {

//! What writeFixStations() writes for \a fixes, or for \a fallback when
//! \a fixes is empty.
QString writtenFixes(const QList<cwFixStation>& fixes, const QString& fallback = QString(),
                     const QString& globalCS = QString())
{
    cwSurvexCS::SidecarWriter sidecars;
    cwSurvexExporterUtils::CsScope scope(sidecars);
    QString text;
    QTextStream stream(&text);
    cwSurvexExporterUtils::writeFixStations(stream, fixes, fallback, globalCS, scope);
    stream.flush();
    return text;
}

//! The lines of \a text, trimmed, with blank ones dropped.
QStringList linesOf(const QString& text)
{
    QStringList lines;
    for (const QString& line : text.split(QLatin1Char('\n'))) {
        if (!line.trimmed().isEmpty()) {
            lines.append(line.trimmed());
        }
    }
    return lines;
}

} // namespace

TEST_CASE("A fix on a station spelled with a capital is written under *case preserve",
          "[cwSurvexExporterFix][CasePairs]") {
    const QString utm = QStringLiteral("EPSG:32616");

    SECTION("Its *cs comes before the block, and a lowercase fix after it is written plainly") {
        const QString text = writtenFixes({makeFix("XY.P2", utm, 500000, 4000000, 100),
                                           makeFix("XY.P3", utm, 500010, 4000000, 100),
                                           makeFix("k1", utm, 500020, 4000000, 100)});
        INFO(text.toStdString());
        const QStringList lines = linesOf(text);
        REQUIRE(lines.size() == 10);
        CHECK(lines.at(0).startsWith(QStringLiteral("*cs ")));
        CHECK(lines.at(1) == QStringLiteral("*begin"));
        CHECK(lines.at(2) == QStringLiteral("*case preserve"));
        CHECK(lines.at(3).startsWith(QStringLiteral("*fix XY.P2 500000.")));
        CHECK(lines.at(4) == QStringLiteral("*end"));
        // Cavern drops a *cs at its block's *end, so the next fix needs none of
        // its own: the system in scope outside the block is still the same.
        CHECK(lines.at(5) == QStringLiteral("*begin"));
        CHECK(lines.at(6) == QStringLiteral("*case preserve"));
        CHECK(lines.at(7).startsWith(QStringLiteral("*fix XY.P3 500010.")));
        CHECK(lines.at(8) == QStringLiteral("*end"));
        CHECK(lines.at(9).startsWith(QStringLiteral("*fix k1 500020.")));
    }

    SECTION("The fallback fix keeps its capital the same way") {
        const QStringList lines = linesOf(writtenFixes({}, QStringLiteral("K1")));
        INFO(lines.join('\n').toStdString());
        CHECK(lines
              == QStringList({QStringLiteral("*begin"), QStringLiteral("*case preserve"),
                              QStringLiteral("*fix K1 0 0 0"), QStringLiteral("*end")}));
    }

    SECTION("A lowercase fallback is written plainly") {
        CHECK(linesOf(writtenFixes({}, QStringLiteral("k1")))
              == QStringList({QStringLiteral("*fix k1 0 0 0")}));
    }
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

TEST_CASE("Fix validation drops a fix whose system is a PROJ operation",
          "[cwSurvexExporterFix][issue708]") {
    // A pipeline is an operation, not a coordinate system: it has no area of
    // use to check the coordinate against, and survex can't take it as *cs.
    // The fix is dropped like any other unusable one, and the message names
    // the system so the user can find the row.
    const QString pipeline =
        QStringLiteral("+proj=pipeline +step +proj=unitconvert +xy_in=m +xy_out=m");
    const QSet<QString> stationKeys = {cwStation::canonicalKey(QStringLiteral("a1")),
                                       cwStation::canonicalKey(QStringLiteral("a2"))};

    const cwFixStation pipelineFix = makeFix("a1", pipeline, 500000.0, 4000000.0, 100.0);
    REQUIRE(pipelineFix.state() == cwFixStation::Valid);

    const QList<cwFixStation> fixes = {
        pipelineFix,
        makeFix("a2", QStringLiteral("EPSG:32616"), 500100.0, 4000050.0, 110.0)
    };

    QStringList errors;
    const QList<cwFixStation> kept =
        cwSurvexExporterUtils::validateFixStations(fixes, stationKeys, errors);

    INFO(errors.join('\n').toStdString());
    REQUIRE(kept.size() == 1);
    CHECK(kept.first().stationName() == QStringLiteral("a2"));
    REQUIRE(errors.size() == 1);
    CHECK(errors.first().contains(QStringLiteral("a1")));
    CHECK(errors.first().contains(pipeline));
}

TEST_CASE("Survex export writes no *cs for a fix whose system is a PROJ operation",
          "[cwSurvexExporterFix][issue708]") {
    // Written out, the pipeline became *cs CUSTOM "+proj=pipeline ..." and
    // the solve failed on it; the good fix beside it carries the cave alone.
    cwCavingRegionData region;

    cwCaveData cave = makeCave(QStringLiteral("Operation"),
                               {QStringLiteral("a1"), QStringLiteral("a2")});
    cave.fixStations.append(makeFix("a1",
                                    QStringLiteral("+proj=pipeline +step +proj=unitconvert "
                                                   "+xy_in=m +xy_out=m"),
                                    500000.0, 4000000.0, 100.0));
    cave.fixStations.append(makeFix("a2", QStringLiteral("EPSG:32616"),
                                    500100.0, 4000050.0, 110.0));
    region.caves.append(cave);

    const QString output = writeRegionToString(region);
    INFO(output.toStdString());
    CHECK_FALSE(output.contains(QStringLiteral("+proj=pipeline")));
    CHECK_FALSE(output.contains(QStringLiteral("*fix a1")));
    CHECK(output.contains(QStringLiteral("*cs EPSG:32616")));
    CHECK(output.contains(QStringLiteral("*fix a2 500100.000000000 4000050.000000000 110.000000000")));
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

TEST_CASE("Line-plot export with no frame writes no *cs for a fix that has one",
          "[cwSurvexExporterFix][issue660]") {
    // A project whose only fix is a typo has no frame (#660), so there is no
    // *cs out, and cavern refuses any *cs without one: before the *fix it
    // reports "The input projection is set but the output projection isn't",
    // after it "fixed before CS command first used". *declination auto would
    // write the fix's *cs after the fallback *fix, so it has to sit out too.
    cwCavingRegionData region;
    REQUIRE(region.geoReference.localCoordinateSystem.isEmpty());

    cwCaveData cave = makeCave(QStringLiteral("Typo"),
                               {QStringLiteral("a1"), QStringLiteral("a2")});
    cave.trips.first().calibrations.setAutoDeclination(true);

    SECTION("a fix on a surveyed station") {
        cave.fixStations.append(makeFix("a1", QStringLiteral("EPSG:32613"),
                                        1478000.0, 4430000.0, 1655.0));
    }
    SECTION("a fix whose station name the survey doesn't have") {
        cave.fixStations.append(makeFix("zz9", QStringLiteral("EPSG:32613"),
                                        1478000.0, 4430000.0, 1655.0));
    }
    region.caves.append(cave);

    const QString output = writeRegionToString(
        region, cwSurvexExporterRegion::OutputCSPolicy::WorkingFrame);
    INFO(output.toStdString());
    CHECK_FALSE(output.contains(QStringLiteral("*cs")));
    CHECK_FALSE(output.contains(QStringLiteral("*declination auto")));
    CHECK(output.contains(QStringLiteral("*fix a1 0 0 0")));
}

TEST_CASE("Survex export writes *entrance for a native entrance after the node's fixes",
          "[cwSurvexExporterFix][SurvexEntrance]") {
    cwCavingRegionData region;
    cwCaveData cave = makeCave(QStringLiteral("Marked"),
                               {QStringLiteral("a1"), QStringLiteral("a2"), QStringLiteral("a3")});
    cave.fixStations.append(makeFix("a1", QStringLiteral("EPSG:32616"),
                                    500000.0, 4000000.0, 100.0));
    cave.entrances.append(makeEntrance(QStringLiteral("a2")));
    region.caves.append(cave);

    const QString output = writeRegionToString(region);
    INFO(output.toStdString());

    const int fixIdx = output.indexOf(QStringLiteral("*fix a1"));
    REQUIRE(fixIdx >= 0);
    const int entranceIdx = output.indexOf(QStringLiteral("*entrance a2\n"));
    REQUIRE(entranceIdx > fixIdx);
    const int endIdx = output.indexOf(QStringLiteral("*end "), entranceIdx);
    CHECK(endIdx > entranceIdx);
    CHECK(output.count(QStringLiteral("*entrance")) == 1);
}

TEST_CASE("Survex export skips an entrance on a station in no shot",
          "[cwSurvexExporterFix][SurvexEntrance]") {
    // Cavern warns about an *entrance on a station no leg uses, so the
    // driver writes only the entrances the data places.
    cwCavingRegionData region;
    cwCaveData cave = makeCave(QStringLiteral("Haunt"),
                               {QStringLiteral("a1"), QStringLiteral("a2")});
    cave.entrances.append(makeEntrance(QStringLiteral("ghost")));
    cave.entrances.append(makeEntrance(QStringLiteral("a2")));
    region.caves.append(cave);

    const QString output = writeRegionToString(region);
    INFO(output.toStdString());
    CHECK_FALSE(output.contains(QStringLiteral("ghost")));
    CHECK(output.contains(QStringLiteral("*entrance a2\n")));
}

TEST_CASE("Survex export writes one *entrance for two entrances that differ only in case",
          "[cwSurvexExporterFix][SurvexEntrance]") {
    cwCavingRegionData region;
    cwCaveData cave = makeCave(QStringLiteral("Twice"),
                               {QStringLiteral("a1"), QStringLiteral("a2")});
    cave.entrances.append(makeEntrance(QStringLiteral("a2")));
    cave.entrances.append(makeEntrance(QStringLiteral("A2")));
    region.caves.append(cave);

    const QString output = writeRegionToString(region);
    INFO(output.toStdString());
    CHECK(output.count(QStringLiteral("*entrance a2\n"), Qt::CaseInsensitive) == 1);
}

TEST_CASE("Survex export writes an attached cave's entrance before the file's *include",
          "[cwSurvexExporterFix][SurvexEntrance]") {
    // The node's own fixes and entrances name the file's stations in the
    // node's block, ahead of the *include that reads them.
    const QString fixtureDir = testcasesDatasetSourcePath(QStringLiteral("external-centerlines"));
    cwCaveData cave;
    cave.id = QUuid::createUuid();
    cave.name = QStringLiteral("Attached");
    cave.externalCenterline = cwExternalCenterline(QStringLiteral("survex_entrance.svx"));
    cave.externalStations = {QStringLiteral("doghill.d1"), QStringLiteral("doghill.d2"),
                             QStringLiteral("doghill.d3"), QStringLiteral("doghill.d4")};
    cave.entrances.append(makeEntrance(QStringLiteral("doghill.d2")));
    cave.entrances.append(makeEntrance(QStringLiteral("doghill.ghost")));

    cwCavingRegionData region;
    region.caves.append(cave);

    cwSurvexExporterRegion::Options options;
    options.caveAttachmentDirs.insert(cave.id, fixtureDir);
    options.externalFixedStations.insert(cave.id, {QStringLiteral("doghill.d1"),
                                                   QStringLiteral("doghill.d4")});

    const QString output = writeRegionToString(region, options);
    INFO(output.toStdString());
    const int entranceIdx = output.indexOf(QStringLiteral("*entrance doghill.d2\n"));
    REQUIRE(entranceIdx >= 0);
    const int includeIdx = output.indexOf(QStringLiteral("*include"));
    REQUIRE(includeIdx >= 0);
    CHECK(entranceIdx < includeIdx);
    CHECK_FALSE(output.contains(QStringLiteral("ghost")));
}

TEST_CASE("Survex export writes an entrance on a nested block's node in the source root's block",
          "[cwSurvexExporterFix][SurvexEntrance]") {
    // The survey editor stores an entrance on the node that holds the
    // station. The root's block is the only one the sourced subtree writes,
    // so the entrance is named there by the node's cavern path.
    cwCaveData root = nestedEntranceRoot();
    cwCaveData outer = derivedNode(root, QStringLiteral("outer"), QStringLiteral("outer"),
                                   QStringLiteral("outer"));
    cwCaveData inner = derivedNode(root, QStringLiteral("inner"), QStringLiteral("inner"),
                                   QStringLiteral("outer/inner"));
    inner.entrances.append(makeEntrance(QStringLiteral("i1")));

    SECTION("an entrance on the innermost node") {
    }

    SECTION("the same station marked on two nodes, in two spellings") {
        outer.entrances.append(makeEntrance(QStringLiteral("inner.I1")));
    }

    const QUuid innerId = inner.id;
    outer.nodes.append(inner);
    root.nodes.append(outer);
    cwCavingRegionData region;
    region.caves.append(root);

    const QString output = writeRegionToString(region, attachedOptions(root));
    INFO(output.toStdString());
    const int entranceIdx = output.indexOf(QStringLiteral("*entrance outer.inner.i1\n"),
                                           0, Qt::CaseInsensitive);
    REQUIRE(entranceIdx >= 0);
    const int includeIdx = output.indexOf(QStringLiteral("*include"));
    REQUIRE(includeIdx >= 0);
    CHECK(entranceIdx < includeIdx);
    CHECK(output.count(QStringLiteral("*entrance outer.inner.i1\n"), Qt::CaseInsensitive) == 1);

    // The plan maps inner's entry to the station its *entrance names, in
    // region scope.
    const cwScopeLabels labels(region);
    const cwSurvexExporterCaveTask::DriverTree tree(region.caves, labels);
    const cwSurvexExporterCaveTask::AnchorPlan plan(tree, attachedOptions(root).externalFixedStations);
    CHECK(plan.entranceKeys().value(innerId).value(QStringLiteral("i1")).toStdString()
          == cwStation::canonicalKey(labels.prefix(root.id) + QStringLiteral("outer.inner.i1")).toStdString());
}

TEST_CASE("Survex export writes a nested block's entrance as a case-preserving file spells it",
          "[cwSurvexExporterFix][SurvexEntrance]") {
    // A file under *case preserve keeps its station's case, so the root's
    // block names it in the file's spelling, whichever spelling the nodes use.
    cwCaveData root = nestedEntranceRoot();
    root.externalStations.replace(root.externalStations.indexOf(QStringLiteral("outer.inner.i1")),
                                  QStringLiteral("outer.inner.I1"));
    cwCaveData outer = derivedNode(root, QStringLiteral("outer"), QStringLiteral("outer"),
                                   QStringLiteral("outer"));
    cwCaveData inner = derivedNode(root, QStringLiteral("inner"), QStringLiteral("inner"),
                                   QStringLiteral("outer/inner"));
    inner.entrances.append(makeEntrance(QStringLiteral("i1")));
    outer.entrances.append(makeEntrance(QStringLiteral("inner.i1")));
    // A native trip's station is read under the block's *case lower.
    cwTripData dig = tripWithStations({QStringLiteral("D1"), QStringLiteral("D2")});
    dig.name = QStringLiteral("Dig");
    outer.trips.append(dig);
    outer.entrances.append(makeEntrance(QStringLiteral("D2")));
    outer.nodes.append(inner);
    root.nodes.append(outer);
    cwCavingRegionData region;
    region.caves.append(root);

    const QString output = writeRegionToString(region, attachedOptions(root));
    INFO(output.toStdString());
    CHECK(output.count(QStringLiteral("*case preserve\n*entrance outer.inner.I1\n*end\n")) == 1);
    CHECK(output.count(QStringLiteral("*entrance outer.inner.i1\n"), Qt::CaseInsensitive) == 1);
    CHECK(output.count(QStringLiteral("*entrance dig.d2\n")) == 1);
    CHECK(output.count(QStringLiteral("*case preserve")) == 1);
}

TEST_CASE("Survex export writes a native entrance in the node's spelling, under *case lower",
          "[cwSurvexExporterFix][SurvexEntrance]") {
    cwCavingRegionData region;
    cwCaveData cave = makeCave(QStringLiteral("Upper"), {QStringLiteral("A1"), QStringLiteral("A2")});
    cave.entrances.append(makeEntrance(QStringLiteral("A2")));
    region.caves.append(cave);

    const QString output = writeRegionToString(region);
    INFO(output.toStdString());
    CHECK(output.count(QStringLiteral("*entrance A2\n")) == 1);
    CHECK_FALSE(output.contains(QStringLiteral("*case preserve")));
}

TEST_CASE("Survex export writes an entrance on a native trip's station under a derived node",
          "[cwSurvexExporterFix][SurvexEntrance]") {
    // The survey editor marks the station by its name in the trip, and the
    // root's block writes the trip under its label.
    cwCaveData root = nestedEntranceRoot();
    cwCaveData outer = derivedNode(root, QStringLiteral("outer"), QStringLiteral("outer"),
                                   QStringLiteral("outer"));
    cwTripData dig = tripWithStations({QStringLiteral("D1"), QStringLiteral("D2")});
    dig.name = QStringLiteral("Dig");
    outer.trips.append(dig);
    outer.entrances.append(makeEntrance(QStringLiteral("D2")));
    root.nodes.append(outer);
    cwCavingRegionData region;
    region.caves.append(root);

    const QString output = writeRegionToString(region, attachedOptions(root));
    INFO(output.toStdString());
    CHECK(output.count(QStringLiteral("*entrance dig.d2\n"), Qt::CaseInsensitive) == 1);
}
