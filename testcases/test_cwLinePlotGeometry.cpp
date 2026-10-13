/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Cavewhere includes
#include "cwLinePlotGeometry.h"
#include "SurveyTreeTestHelper.h"
#include "ExternalCenterlineTestHelpers.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwExternalCenterline.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwStationPositionLookup.h"
#include "cwSurveyNetwork.h"

//Qt includes
#include <QVector3D>

namespace {

cwShot makeShot(const QString& dist, const QString& compass, const QString& clino)
{
    cwShot s;
    s.setDistance(cwDistanceReading(dist));
    s.setCompass(cwCompassReading(compass));
    s.setClino(cwClinoReading(clino));
    return s;
}

// Counts how many vertices sit at exactly `position`.
int countPositions(const QVector<QVector3D>& points, const QVector3D& position)
{
    int count = 0;
    for (const QVector3D& p : points) {
        if (p == position) {
            count++;
        }
    }
    return count;
}

// Returns the index of the trip whose vertex range covers vertex `vertexIndex`,
// or -1 if none does.
int tripForVertex(const QVector<cwLinePlotGeometry::VertexRange>& ranges, int vertexIndex)
{
    for (int i = 0; i < ranges.size(); ++i) {
        const auto& r = ranges.at(i);
        if (vertexIndex >= r.start && vertexIndex < r.start + r.count) {
            return i;
        }
    }
    return -1;
}

void makeSourceRoot(cwSurveyNode* node)
{
    node->setSourceId(QUuid::createUuid());
}

cwCave* addDerivedNode(cwSurveyNode* root, const QString& name, const QString& sourcePath,
                       const QString& cavernLabel)
{
    auto* node = new cwCave();
    node->setName(name);
    node->setSourceId(root->sourceId());
    node->setSourcePath(sourcePath);
    node->setCavernLabel(cavernLabel);
    root->addNode(node);
    return node;
}

cwTrip* addDerivedTrip(cwSurveyNode* node, const QString& name, const QString& sourcePath,
                       const QStringList& stations)
{
    auto* trip = new cwTrip();
    trip->setName(name);
    trip->setSourcePath(sourcePath);
    trip->setExternalStations(stations);
    node->addTrip(trip);
    return trip;
}

} // namespace

TEST_CASE("cwLinePlotGeometry de-shares vertices per shot", "[cwLinePlotGeometry]")
{
    // Two trips share the tie-in station a2. With per-shot de-share each shot
    // owns its own two endpoint vertices, so a2 appears twice — once per shot —
    // and each trip occupies a contiguous vertex range.
    cwCavingRegion region;

    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Cave 1"));
    region.addCave(cave);

    cwTrip* trip1 = new cwTrip();
    cwSurveyChunk* chunk1 = new cwSurveyChunk();
    trip1->addChunk(chunk1);
    chunk1->appendShot(cwStation("a1"), cwStation("a2"), makeShot("10.0", "0.0", "0.0"));
    cave->addTrip(trip1);

    cwTrip* trip2 = new cwTrip();
    cwSurveyChunk* chunk2 = new cwSurveyChunk();
    trip2->addChunk(chunk2);
    chunk2->appendShot(cwStation("a2"), cwStation("a3"), makeShot("10.0", "90.0", "0.0"));
    cave->addTrip(trip2);

    // Hand-set the solved positions so generate() has geometry to build.
    cwStationPositionLookup lookup;
    lookup.setPosition("a1", QVector3D(0.0f, 0.0f, 0.0f));
    lookup.setPosition("a2", QVector3D(0.0f, 10.0f, 0.0f));
    lookup.setPosition("a3", QVector3D(10.0f, 10.0f, 0.0f));
    SurveyTreeTestHelper::setScopedPositions(cave, lookup);

    const auto result = cwLinePlotGeometry::generate(region.data(), region.stationPositions());
    REQUIRE_FALSE(result.hasError());
    const cwLinePlotGeometry::Result geometry = result.value();

    SECTION("each shot emits two vertices; the shared station is duplicated") {
        // shot a1->a2 = [a1, a2], shot a2->a3 = [a2, a3] -> 4 vertices, a2 twice.
        CHECK(geometry.points.size() == 4);
        CHECK(countPositions(geometry.points, QVector3D(0.0f, 10.0f, 0.0f)) == 2);
    }

    SECTION("each trip occupies a contiguous vertex range") {
        REQUIRE(geometry.tripVertexRanges.size() == 2);
        REQUIRE(geometry.tripUuids.size() == 2);

        CHECK(geometry.tripVertexRanges.at(0).start == 0);
        CHECK(geometry.tripVertexRanges.at(0).count == 2);
        CHECK(geometry.tripVertexRanges.at(1).start == 2);
        CHECK(geometry.tripVertexRanges.at(1).count == 2);

        // Ranges cover every vertex with no gaps or overlap.
        for (int i = 0; i < geometry.points.size(); ++i) {
            CHECK(tripForVertex(geometry.tripVertexRanges, i) >= 0);
        }
    }

    SECTION("the range->uuid table is keyed by identity, not list position") {
        REQUIRE(geometry.tripUuids.size() == 2);
        CHECK(geometry.tripUuids.at(0) == trip1->id());
        CHECK(geometry.tripUuids.at(1) == trip2->id());

        // The vertices for a1 (trip1-only) and a3 (trip2-only) must fall in the
        // range whose UUID resolves to the right trip.
        for (int i = 0; i < geometry.points.size(); ++i) {
            const int tripIndex = tripForVertex(geometry.tripVertexRanges, i);
            REQUIRE(tripIndex >= 0);
            if (geometry.points.at(i) == QVector3D(0.0f, 0.0f, 0.0f)) {
                CHECK(geometry.tripUuids.at(tripIndex) == trip1->id());
            } else if (geometry.points.at(i) == QVector3D(10.0f, 10.0f, 0.0f)) {
                CHECK(geometry.tripUuids.at(tripIndex) == trip2->id());
            }
        }
    }

    SECTION("each native trip is as long as its chunks") {
        CHECK(tripLengthOf(geometry, trip1) == Catch::Approx(10.0));
        CHECK(tripLengthOf(geometry, trip2) == Catch::Approx(10.0));
    }
}

TEST_CASE("cwLinePlotGeometry duplicates a station shared by consecutive shots", "[cwLinePlotGeometry]")
{
    // A station reused by consecutive chunks of the same trip is duplicated —
    // per-shot de-share gives every shot its own endpoints.
    cwCavingRegion region;

    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Cave 1"));
    region.addCave(cave);

    cwTrip* trip = new cwTrip();
    cwSurveyChunk* chunk1 = new cwSurveyChunk();
    trip->addChunk(chunk1);
    chunk1->appendShot(cwStation("a1"), cwStation("a2"), makeShot("10.0", "0.0", "0.0"));

    cwSurveyChunk* chunk2 = new cwSurveyChunk();
    trip->addChunk(chunk2);
    chunk2->appendShot(cwStation("a2"), cwStation("a3"), makeShot("10.0", "90.0", "0.0"));
    cave->addTrip(trip);

    cwStationPositionLookup lookup;
    lookup.setPosition("a1", QVector3D(0.0f, 0.0f, 0.0f));
    lookup.setPosition("a2", QVector3D(0.0f, 10.0f, 0.0f));
    lookup.setPosition("a3", QVector3D(10.0f, 10.0f, 0.0f));
    SurveyTreeTestHelper::setScopedPositions(cave, lookup);

    const auto result = cwLinePlotGeometry::generate(region.data(), region.stationPositions());
    REQUIRE_FALSE(result.hasError());
    const cwLinePlotGeometry::Result geometry = result.value();

    // shot a1->a2 = [a1, a2], shot a2->a3 = [a2, a3] -> 4 vertices, a2 twice.
    CHECK(geometry.points.size() == 4);
    CHECK(countPositions(geometry.points, QVector3D(0.0f, 10.0f, 0.0f)) == 2);

    // All vertices belong to the one trip's single range.
    REQUIRE(geometry.tripUuids.size() == 1);
    REQUIRE(geometry.tripVertexRanges.size() == 1);
    CHECK(geometry.tripVertexRanges.at(0).start == 0);
    CHECK(geometry.tripVertexRanges.at(0).count == 4);
}

TEST_CASE("cwLinePlotGeometry measures a cave that resolved nothing as zero",
          "[cwLinePlotGeometry]")
{
    // A derived trip whose unit holds no network station (a broken attach, a
    // renamed block) leaves the root with no geometry at all. Length and depth
    // still have to be real numbers: they travel straight to cave->length() and
    // cave->depth(), which the cave page renders.
    cwCavingRegion region;

    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("cave1"));
    region.addCave(cave);

    makeSourceRoot(cave);
    addDerivedTrip(cave, QStringLiteral("missing"), QStringLiteral("MISSING"),
                   {QStringLiteral("m1"), QStringLiteral("m2")});

    const auto result = cwLinePlotGeometry::generate(region.data(), region.stationPositions(), cwSurveyNetwork());
    REQUIRE_FALSE(result.hasError());
    const cwLinePlotGeometry::Result geometry = result.value();

    CHECK(geometry.points.isEmpty());
    REQUIRE(geometry.nodeLengthAndDepths.size() == 1);
    const cwLinePlotGeometry::LengthAndDepth measured = geometry.nodeLengthAndDepths.value(region.cave(0)->id());
    CHECK(measured.length() == 0.0);
    CHECK(measured.depth() == 0.0);
}

TEST_CASE("cwLinePlotGeometry draws each unit trip by two-endpoint membership",
          "[cwLinePlotGeometry]")
{
    // The compass_project shape: two surveys of one Compass file share A3, and
    // cavern names every station below the file's level. A unit draws the legs
    // whose two ends it holds, so the shared station's legs split between the
    // surveys and none goes to both.
    cwCavingRegion region;

    cwCave* root = new cwCave();
    root->setName(QStringLiteral("project"));
    root->setExternalCenterline(cwExternalCenterline(QStringLiteral("PROJECT.MAK")));
    region.addCave(root);
    makeSourceRoot(root);

    cwCave* alpha = addDerivedNode(root, QStringLiteral("Alpha"), QStringLiteral("ALPHA.DAT"),
                                   QStringLiteral("alpha"));
    cwTrip* first = addDerivedTrip(alpha, QStringLiteral("ALPHA1"), QStringLiteral("ALPHA.DAT/ALPHA1"),
                                   {QStringLiteral("a1"), QStringLiteral("a2"), QStringLiteral("a3")});
    cwTrip* second = addDerivedTrip(alpha, QStringLiteral("ALPHA2"), QStringLiteral("ALPHA.DAT/ALPHA2"),
                                    {QStringLiteral("a3"), QStringLiteral("a4")});

    const QVector3D a1Position(0.0f, 0.0f, 0.0f);
    const QVector3D a2Position(0.0f, 10.0f, 0.0f);
    const QVector3D a3Position(10.0f, 10.0f, 0.0f);
    const QVector3D a4Position(10.0f, 0.0f, -5.0f);

    cwStationPositionLookup lookup;
    lookup.setPosition(QStringLiteral("alpha.a1"), a1Position);
    lookup.setPosition(QStringLiteral("alpha.a2"), a2Position);
    lookup.setPosition(QStringLiteral("alpha.a3"), a3Position);
    lookup.setPosition(QStringLiteral("alpha.a4"), a4Position);
    SurveyTreeTestHelper::setScopedPositions(root, lookup);

    cwSurveyNetwork network;
    network.addShot(QStringLiteral("project.alpha.A1"), QStringLiteral("project.alpha.A2"));
    network.addShot(QStringLiteral("project.alpha.A2"), QStringLiteral("project.alpha.A3"));
    network.addShot(QStringLiteral("project.alpha.A3"), QStringLiteral("project.alpha.A4"));

    const auto result = cwLinePlotGeometry::generate(region.data(), region.stationPositions(), network);
    REQUIRE_FALSE(result.hasError());
    const cwLinePlotGeometry::Result geometry = result.value();

    const qsizetype firstIndex = geometry.tripUuids.indexOf(first->id());
    const qsizetype secondIndex = geometry.tripUuids.indexOf(second->id());
    REQUIRE(firstIndex >= 0);
    REQUIRE(secondIndex >= 0);
    CHECK(geometry.tripVertexRanges.at(firstIndex).count == 4);
    CHECK(geometry.tripVertexRanges.at(secondIndex).count == 2);
    CHECK(countPositions(geometry.points, a3Position) == 2);

    // The derived node and the root both fold the three legs.
    const double expectedLength = 10.0 + 10.0 + QVector3D(a4Position - a3Position).length();
    CHECK(geometry.nodeLengthAndDepths.value(alpha->id()).length() == Catch::Approx(expectedLength));
    CHECK(geometry.nodeLengthAndDepths.value(root->id()).length() == Catch::Approx(expectedLength));
    CHECK(geometry.nodeLengthAndDepths.value(root->id()).depth() == Catch::Approx(5.0));

    // Each unit trip is as long as the legs it draws.
    CHECK(tripLengthOf(geometry, first) == Catch::Approx(20.0));
    CHECK(tripLengthOf(geometry, second) == Catch::Approx(QVector3D(a4Position - a3Position).length()));
}

TEST_CASE("cwLinePlotGeometry draws a leg two units both hold in each of them",
          "[cwLinePlotGeometry]")
{
    // A Walls station in two books: when both units hold both ends of a leg,
    // the leg draws in both trips (§7.2 step 8), and every node still counts it
    // once.
    cwCavingRegion region;

    cwCave* root = new cwCave();
    root->setName(QStringLiteral("kaua"));
    root->setExternalCenterline(cwExternalCenterline(QStringLiteral("KAUA.WPJ")));
    region.addCave(root);
    makeSourceRoot(root);

    cwTrip* first = addDerivedTrip(root, QStringLiteral("D01"), QStringLiteral("SURVEYS/D01"),
                                   {QStringLiteral("d1"), QStringLiteral("d2")});
    cwTrip* second = addDerivedTrip(root, QStringLiteral("D02"), QStringLiteral("SURVEYS/D02"),
                                    {QStringLiteral("d1"), QStringLiteral("d2"), QStringLiteral("d3")});

    cwStationPositionLookup lookup;
    lookup.setPosition(QStringLiteral("d1"), QVector3D(0.0f, 0.0f, 0.0f));
    lookup.setPosition(QStringLiteral("d2"), QVector3D(0.0f, 10.0f, 0.0f));
    lookup.setPosition(QStringLiteral("d3"), QVector3D(0.0f, 20.0f, 0.0f));
    SurveyTreeTestHelper::setScopedPositions(root, lookup);

    cwSurveyNetwork network;
    network.addShot(QStringLiteral("kaua.d1"), QStringLiteral("kaua.d2"));
    network.addShot(QStringLiteral("kaua.d2"), QStringLiteral("kaua.d3"));

    const auto result = cwLinePlotGeometry::generate(region.data(), region.stationPositions(), network);
    REQUIRE_FALSE(result.hasError());
    const cwLinePlotGeometry::Result geometry = result.value();

    CHECK(geometry.tripVertexRanges.at(geometry.tripUuids.indexOf(first->id())).count == 2);
    CHECK(geometry.tripVertexRanges.at(geometry.tripUuids.indexOf(second->id())).count == 4);
    CHECK(geometry.nodeLengthAndDepths.value(root->id()).length() == Catch::Approx(20.0));

    // The shared leg counts in each trip that draws it, while the node counts it once.
    CHECK(tripLengthOf(geometry, first) == Catch::Approx(10.0));
    CHECK(tripLengthOf(geometry, second) == Catch::Approx(20.0));
}

TEST_CASE("cwLinePlotGeometry hands the whole-node window every leg no unit drew",
          "[cwLinePlotGeometry]")
{
    // The Walls and root-plus-block shape: the file root owns some stations and
    // a prefixed survey owns the rest, tied to each other by one leg. The
    // prefixed survey draws its own leg; the root's own unit, the whole-node
    // window, draws the root's leg and the tie, so each leg has one owner.
    cwCavingRegion region;

    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("cave1"));
    cave->setExternalCenterline(cwExternalCenterline(QStringLiteral("walls_prefixed.wpj")));
    region.addCave(cave);
    makeSourceRoot(cave);

    cwTrip* window = addDerivedTrip(cave, QStringLiteral("walls_prefixed"), cwTrip::ownUnitSourcePath(),
                                    {QStringLiteral("a1"), QStringLiteral("a2")});
    cwTrip* blockTrip = addDerivedTrip(cave, QStringLiteral("XY"), QStringLiteral("XY"),
                                       {QStringLiteral("xy.p1"), QStringLiteral("xy.p2")});

    const QVector3D a1Position(0.0f, 0.0f, 0.0f);
    const QVector3D a2Position(0.0f, 10.0f, 0.0f);
    const QVector3D p1Position(0.0f, 14.0f, 0.0f);
    const QVector3D p2Position(12.0f, 14.0f, 0.0f);

    cwStationPositionLookup lookup;
    lookup.setPosition(QStringLiteral("a1"), a1Position);
    lookup.setPosition(QStringLiteral("a2"), a2Position);
    lookup.setPosition(QStringLiteral("xy.p1"), p1Position);
    lookup.setPosition(QStringLiteral("xy.p2"), p2Position);
    SurveyTreeTestHelper::setScopedPositions(cave, lookup);

    // 10 m at the root, a 4 m tie into the prefixed survey, 12 m inside it.
    // Walls keeps the authored case of a prefix.
    cwSurveyNetwork network;
    network.addShot(QStringLiteral("cave1.a1"), QStringLiteral("cave1.a2"));
    network.addShot(QStringLiteral("cave1.a2"), QStringLiteral("cave1.XY.p1"));
    network.addShot(QStringLiteral("cave1.XY.p1"), QStringLiteral("cave1.XY.p2"));

    const auto result = cwLinePlotGeometry::generate(region.data(), region.stationPositions(), network);
    REQUIRE_FALSE(result.hasError());
    const cwLinePlotGeometry::Result geometry = result.value();

    REQUIRE(geometry.tripUuids.size() == geometry.tripVertexRanges.size());
    const qsizetype windowIndex = geometry.tripUuids.indexOf(window->id());
    const qsizetype blockIndex = geometry.tripUuids.indexOf(blockTrip->id());
    REQUIRE(windowIndex >= 0);
    REQUIRE(blockIndex >= 0);

    // The root window draws a1-a2 and the a2-p1 tie; the block draws p1-p2.
    CHECK(geometry.tripVertexRanges.at(windowIndex).count == 4);
    CHECK(geometry.tripVertexRanges.at(blockIndex).count == 2);

    // The tie leg is drawn once: three legs, six vertices, p1 shared.
    CHECK(geometry.points.size() == 6);
    CHECK(countPositions(geometry.points, p1Position) == 2);
    CHECK(countPositions(geometry.points, a1Position) == 1);
    CHECK(countPositions(geometry.points, p2Position) == 1);

    // And the root length counts each leg once.
    REQUIRE(geometry.nodeLengthAndDepths.size() == 1);
    const cwLinePlotGeometry::LengthAndDepth measured = geometry.nodeLengthAndDepths.value(region.cave(0)->id());
    CHECK(measured.length() == Catch::Approx(26.0));
    CHECK(measured.depth() == Catch::Approx(0.0));

    // The window is as long as the root's leg and the tie; the block as its own leg.
    CHECK(tripLengthOf(geometry, window) == Catch::Approx(14.0));
    CHECK(tripLengthOf(geometry, blockTrip) == Catch::Approx(12.0));
}

TEST_CASE("cwLinePlotGeometry draws a native trip inside a sourced subtree from its chunks",
          "[cwLinePlotGeometry]")
{
    // The trip is written in the root's block under its label, so its
    // stations resolve below the root's label path, then the label.
    cwCavingRegion region;

    cwCave* root = new cwCave();
    root->setName(QStringLiteral("project"));
    root->setExternalCenterline(cwExternalCenterline(QStringLiteral("PROJECT.MAK")));
    region.addCave(root);
    makeSourceRoot(root);
    cwCave* alpha = addDerivedNode(root, QStringLiteral("Alpha"), QStringLiteral("ALPHA.DAT"),
                                   QStringLiteral("alpha"));

    cwTrip* native = new cwTrip();
    native->setName(QStringLiteral("Dig"));
    alpha->addTrip(native);
    cwSurveyChunk* chunk = new cwSurveyChunk();
    native->addChunk(chunk);
    chunk->appendShot(cwStation(QStringLiteral("n1")), cwStation(QStringLiteral("n2")),
                      makeShot(QStringLiteral("10"), QStringLiteral("0"), QStringLiteral("0")));
    REQUIRE(native->scopePrefix() == QStringLiteral("dig."));

    cwStationPositionLookup lookup;
    lookup.setPosition(QStringLiteral("dig.n1"), QVector3D(0.0f, 0.0f, 0.0f));
    lookup.setPosition(QStringLiteral("dig.n2"), QVector3D(0.0f, 10.0f, 0.0f));
    SurveyTreeTestHelper::setScopedPositions(root, lookup);

    cwSurveyNetwork network;
    network.addShot(QStringLiteral("project.dig.n1"), QStringLiteral("project.dig.n2"));

    const auto result = cwLinePlotGeometry::generate(region.data(), region.stationPositions(), network);
    REQUIRE_FALSE(result.hasError());
    const cwLinePlotGeometry::Result geometry = result.value();

    CHECK(geometry.tripVertexRanges.at(geometry.tripUuids.indexOf(native->id())).count == 2);
    CHECK(geometry.nodeLengthAndDepths.value(alpha->id()).length() == Catch::Approx(10.0));
    CHECK(geometry.nodeLengthAndDepths.value(root->id()).length() == Catch::Approx(10.0));
    CHECK(tripLengthOf(geometry, native) == Catch::Approx(10.0));
}
