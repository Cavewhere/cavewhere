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
#include "cwCavingRegion.h"
#include "cwCave.h"
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
    cave->setStationPositionLookup(lookup);

    const auto result = cwLinePlotGeometry::generate(region.data());
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
    cave->setStationPositionLookup(lookup);

    const auto result = cwLinePlotGeometry::generate(region.data());
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

TEST_CASE("cwLinePlotGeometry windows a Scope trip whose prefix carries authored case",
          "[cwLinePlotGeometry]")
{
    // A Scope trip stores its stationPrefix exactly as the survey file authored
    // the *begin block ("48H-Feng"), while cavern lowercases every label it
    // writes to the .3d — so the solved network keys are lowercase. The
    // scope-membership filter has to bridge the two, or the trip emits no line
    // geometry at all (stations still get labels, which is what made this look
    // like a rendering bug rather than a matching one).
    cwCavingRegion region;

    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("cave1")); //sanitizes to itself: cavePrefix is "cave1."
    region.addCave(cave);

    cwTrip* scopeTrip = new cwTrip();
    scopeTrip->setName(QStringLiteral("48H-Feng"));
    scopeTrip->setStationPrefix(QStringLiteral("48H-Feng")); //what reconcileScopeTrips stores
    cave->addTrip(scopeTrip);

    const QVector3D f1Position(0.0f, 0.0f, 0.0f);
    const QVector3D f2Position(0.0f, 10.0f, 0.0f);
    const QVector3D l1Position(5.0f, 10.0f, 0.0f);

    // Cave-local lookup keys — the network's cave prefix is stripped before the
    // position lookup.
    cwStationPositionLookup lookup;
    lookup.setPosition(QStringLiteral("48h-feng.f1"), f1Position);
    lookup.setPosition(QStringLiteral("48h-feng.f2"), f2Position);
    lookup.setPosition(QStringLiteral("48h-feng.lower.l1"), l1Position);
    cave->setStationPositionLookup(lookup);

    // Cavern-shaped network keys: region-wide and lowercase.
    cwSurveyNetwork network;
    network.addShot(QStringLiteral("cave1.48h-feng.f1"), QStringLiteral("cave1.48h-feng.f2"));
    network.addShot(QStringLiteral("cave1.48h-feng.f2"), QStringLiteral("cave1.48h-feng.lower.l1"));

    const auto result = cwLinePlotGeometry::generate(region.data(), network);
    REQUIRE_FALSE(result.hasError());
    const cwLinePlotGeometry::Result geometry = result.value();

    // Find the range by trip identity rather than list position.
    REQUIRE(geometry.tripUuids.size() == geometry.tripVertexRanges.size());
    const qsizetype tripIndex = geometry.tripUuids.indexOf(scopeTrip->id());
    REQUIRE(tripIndex >= 0);

    const cwLinePlotGeometry::VertexRange range = geometry.tripVertexRanges.at(tripIndex);
    CHECK(range.count > 0);
    CHECK(range.count % 2 == 0);

    // Every emitted vertex resolved through the cave lookup.
    for (int i = range.start; i < range.start + range.count; ++i) {
        const QVector3D point = geometry.points.at(i);
        INFO("vertex index: " << i);
        CHECK((point == f1Position || point == f2Position || point == l1Position));
    }

    // Both legs are drawn, each with its own endpoints.
    CHECK(countPositions(geometry.points, f1Position) == 1);
    CHECK(countPositions(geometry.points, l1Position) == 1);
    CHECK(countPositions(geometry.points, f2Position) == 2);
}

TEST_CASE("cwLinePlotGeometry gives each leg of nested Scope trips one owner",
          "[cwLinePlotGeometry]")
{
    // A nested *begin block's stations carry their parent's prefix too, so a
    // parent Scope trip string-matches every station of its children. Left
    // alone, parent and child each draw the child's legs: doubled render
    // segments, two trips toggling the same leg, and a cave length that counts
    // nested passage twice.
    cwCavingRegion region;

    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("cave1")); //sanitizes to itself: cavePrefix is "cave1."
    region.addCave(cave);

    cwTrip* parentTrip = new cwTrip();
    parentTrip->setName(QStringLiteral("doghill"));
    parentTrip->setStationPrefix(QStringLiteral("doghill"));
    cave->addTrip(parentTrip);

    cwTrip* childTrip = new cwTrip();
    childTrip->setName(QStringLiteral("big-passage"));
    childTrip->setStationPrefix(QStringLiteral("doghill.big-passage"));
    cave->addTrip(childTrip);

    const QVector3D d1Position(0.0f, 0.0f, 0.0f);
    const QVector3D d2Position(0.0f, 10.0f, 0.0f);
    const QVector3D p1Position(0.0f, 12.0f, 0.0f);
    const QVector3D p2Position(5.0f, 12.0f, 0.0f);

    cwStationPositionLookup lookup;
    lookup.setPosition(QStringLiteral("doghill.d1"), d1Position);
    lookup.setPosition(QStringLiteral("doghill.d2"), d2Position);
    lookup.setPosition(QStringLiteral("doghill.big-passage.p1"), p1Position);
    lookup.setPosition(QStringLiteral("doghill.big-passage.p2"), p2Position);
    cave->setStationPositionLookup(lookup);

    // 10 m inside the parent, a 2 m tie into the child block, 5 m inside the
    // child: 17 m of passage in all.
    cwSurveyNetwork network;
    network.addShot(QStringLiteral("cave1.doghill.d1"), QStringLiteral("cave1.doghill.d2"));
    network.addShot(QStringLiteral("cave1.doghill.d2"),
                    QStringLiteral("cave1.doghill.big-passage.p1"));
    network.addShot(QStringLiteral("cave1.doghill.big-passage.p1"),
                    QStringLiteral("cave1.doghill.big-passage.p2"));

    const auto result = cwLinePlotGeometry::generate(region.data(), network);
    REQUIRE_FALSE(result.hasError());
    const cwLinePlotGeometry::Result geometry = result.value();

    REQUIRE(geometry.tripUuids.size() == geometry.tripVertexRanges.size());
    const qsizetype parentIndex = geometry.tripUuids.indexOf(parentTrip->id());
    const qsizetype childIndex = geometry.tripUuids.indexOf(childTrip->id());
    REQUIRE(parentIndex >= 0);
    REQUIRE(childIndex >= 0);

    SECTION("the innermost scope owns a nested block's legs") {
        // Parent draws d1-d2 and the d2-p1 tie; the child draws p1-p2 alone.
        CHECK(geometry.tripVertexRanges.at(parentIndex).count == 4);
        CHECK(geometry.tripVertexRanges.at(childIndex).count == 2);
    }

    SECTION("three legs are drawn, the tie among them exactly once") {
        CHECK(geometry.points.size() == 6);
        // p1 is an endpoint of the tie and of the child's own leg — twice, and
        // four times if the tie were drawn from both sides of the boundary.
        CHECK(countPositions(geometry.points, p1Position) == 2);
        CHECK(countPositions(geometry.points, d1Position) == 1);
        CHECK(countPositions(geometry.points, p2Position) == 1);
    }

    SECTION("cave length counts each leg once") {
        REQUIRE(geometry.cavesLengthAndDepths.size() == 1);
        CHECK(geometry.cavesLengthAndDepths.at(0).length() == Catch::Approx(17.0));
        CHECK(geometry.cavesLengthAndDepths.at(0).depth() == Catch::Approx(0.0));
    }
}

TEST_CASE("cwLinePlotGeometry measures a cave that resolved nothing as zero",
          "[cwLinePlotGeometry]")
{
    // A Scope trip whose prefix selects no network station (a broken attach, a
    // renamed block) leaves the cave with no geometry at all. Length and depth
    // still have to be real numbers: they travel straight to cave->length() and
    // cave->depth(), which the cave page renders.
    cwCavingRegion region;

    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("cave1"));
    region.addCave(cave);

    cwTrip* scopeTrip = new cwTrip();
    scopeTrip->setName(QStringLiteral("missing"));
    scopeTrip->setStationPrefix(QStringLiteral("missing"));
    cave->addTrip(scopeTrip);

    const auto result = cwLinePlotGeometry::generate(region.data(), cwSurveyNetwork());
    REQUIRE_FALSE(result.hasError());
    const cwLinePlotGeometry::Result geometry = result.value();

    CHECK(geometry.points.isEmpty());
    REQUIRE(geometry.cavesLengthAndDepths.size() == 1);
    CHECK(geometry.cavesLengthAndDepths.at(0).length() == 0.0);
    CHECK(geometry.cavesLengthAndDepths.at(0).depth() == 0.0);
}
