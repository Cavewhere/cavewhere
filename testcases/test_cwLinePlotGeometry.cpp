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

} // namespace

TEST_CASE("cwLinePlotGeometry de-shares vertices per trip", "[cwLinePlotGeometry]")
{
    // Two trips share the tie-in station a2. After de-sharing each trip owns its
    // own copy of a2, so a2 appears twice — once per trip — and every vertex
    // carries exactly one trip id.
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

    SECTION("per-vertex trip ids are parallel to points") {
        CHECK(geometry.tripIds.size() == geometry.points.size());
    }

    SECTION("the shared station is duplicated, one copy per trip") {
        // a1, a2(trip1), a2(trip2), a3
        CHECK(geometry.points.size() == 4);
        CHECK(countPositions(geometry.points, QVector3D(0.0f, 10.0f, 0.0f)) == 2);
    }

    SECTION("each trip gets a distinct dense id covering [0, tripCount)") {
        REQUIRE(geometry.tripUuids.size() == 2);
        QSet<quint32> ids(geometry.tripIds.constBegin(), geometry.tripIds.constEnd());
        CHECK(ids.size() == 2);
        for (quint32 id : ids) {
            CHECK(id < quint32(geometry.tripUuids.size()));
        }
    }

    SECTION("the index->uuid table is keyed by identity, not list position") {
        // The dense id is just an index into tripUuids; the UUID is the stable
        // key the manager resolves back to a live trip.
        REQUIRE(geometry.tripUuids.size() == 2);
        CHECK(geometry.tripUuids.at(0) == trip1->id());
        CHECK(geometry.tripUuids.at(1) == trip2->id());

        // The vertices for a1 (trip1-only) and a3 (trip2-only) must carry ids
        // whose UUID resolves to the right trip.
        for (int i = 0; i < geometry.points.size(); ++i) {
            const quint32 id = geometry.tripIds.at(i);
            if (geometry.points.at(i) == QVector3D(0.0f, 0.0f, 0.0f)) {
                CHECK(geometry.tripUuids.at(id) == trip1->id());
            } else if (geometry.points.at(i) == QVector3D(10.0f, 10.0f, 0.0f)) {
                CHECK(geometry.tripUuids.at(id) == trip2->id());
            }
        }
    }
}

TEST_CASE("cwLinePlotGeometry shares vertices within a trip", "[cwLinePlotGeometry]")
{
    // A station reused by consecutive chunks of the *same* trip is one vertex —
    // de-sharing only duplicates across trips.
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

    // a1, a2, a3 — a2 appears once (shared within the single trip).
    CHECK(geometry.points.size() == 3);
    CHECK(countPositions(geometry.points, QVector3D(0.0f, 10.0f, 0.0f)) == 1);
    CHECK(geometry.tripUuids.size() == 1);

    // All vertices belong to the one trip.
    QSet<quint32> ids(geometry.tripIds.constBegin(), geometry.tripIds.constEnd());
    CHECK(ids.size() == 1);
}
