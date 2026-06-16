/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Cavewhere includes
#include "cwLinePlotGeometry.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwStationPositionLookup.h"

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
