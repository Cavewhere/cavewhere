/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Qt includes
#include <QCoreApplication>
#include <QVector3D>

//Our includes
#include "cwTripLinePlotTask.h"
#include "cwTrip.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwTripCalibration.h"
#include "asyncfuture.h"

namespace {

cwShot straightShot(double distance = 10.0, double compass = 0.0, double clino = 0.0)
{
    cwShot shot;
    shot.setDistance(cwDistanceReading(QString::number(distance)));
    shot.setCompass(cwCompassReading(QString::number(compass)));
    shot.setBackCompass(cwCompassReading(QString::number(std::fmod(compass + 180.0, 360.0))));
    shot.setClino(cwClinoReading(QString::number(clino)));
    shot.setBackClino(cwClinoReading(QString::number(-clino)));
    return shot;
}

cwStation namedStation(const QString &name)
{
    cwStation s;
    s.setName(name);
    s.setLeft(cwDistanceReading("0"));
    s.setRight(cwDistanceReading("0"));
    s.setUp(cwDistanceReading("0"));
    s.setDown(cwDistanceReading("0"));
    return s;
}

// Build a single-trip, single-cave region; returns a unique_ptr so the
// test owns the QObjects. Caller adds chunks into the returned trip.
struct RegionFixture {
    std::unique_ptr<cwCavingRegion> region;
    cwCave *cave = nullptr;
    cwTrip *trip = nullptr;
};

RegionFixture makeRegion()
{
    RegionFixture f;
    f.region = std::make_unique<cwCavingRegion>();
    f.cave = new cwCave();
    f.cave->setName("Cave 1");
    f.region->addCave(f.cave);
    f.trip = new cwTrip();
    f.trip->setName("Trip 1");
    f.cave->addTrip(f.trip);
    return f;
}

cwSurveyChunk *addChunk(cwTrip *trip, const QStringList &stationNames, double distance = 10.0)
{
    auto *chunk = new cwSurveyChunk();
    trip->addChunk(chunk);
    REQUIRE(stationNames.size() >= 2);
    for (int i = 0; i < stationNames.size() - 1; ++i) {
        chunk->appendShot(namedStation(stationNames.at(i)),
                          namedStation(stationNames.at(i + 1)),
                          straightShot(distance, /*compass*/ 0.0, /*clino*/ 0.0));
    }
    return chunk;
}

QList<cwTripLinePlotTask::TripComponent> runAndWait(cwTrip *trip)
{
    auto input = cwTripLinePlotTask::buildInput(trip);
    auto future = cwTripLinePlotTask::run(std::move(input));
    REQUIRE(AsyncFuture::waitForFinished(future, 10000));
    return future.result();
}

} // namespace


TEST_CASE("Single linear chain: one component, no renames", "[TripLinePlotTask]")
{
    auto f = makeRegion();
    addChunk(f.trip, {"a1", "a2", "a3", "a4"});

    const auto components = runAndWait(f.trip);
    REQUIRE(components.size() == 1);
    const auto &c = components.first();
    CHECK(c.renameRemap.isEmpty());
    CHECK(c.canonicalAnchor.compare("a1", Qt::CaseInsensitive) == 0);

    const auto stations = c.network.stations();
    CHECK(stations.size() == 4);

    // Positions are anchored at a1=(0,0,0); north/0° compass → +Y
    CHECK(c.positions.position("a1") == QVector3D(0.0, 0.0, 0.0));
    CHECK(c.positions.position("a4").y() == Catch::Approx(30.0));
}

TEST_CASE("Chunk-boundary continuation does not rename shared station", "[TripLinePlotTask]")
{
    auto f = makeRegion();
    addChunk(f.trip, {"a1", "a2", "a3"});
    addChunk(f.trip, {"a3", "a4", "a5"});

    const auto components = runAndWait(f.trip);
    REQUIRE(components.size() == 1);
    const auto &c = components.first();
    CHECK(c.renameRemap.isEmpty());
    CHECK(c.network.stations().size() == 5);
    // No synthetic names
    const auto stations = c.network.stations();
    for (const auto &name : stations) {
        CHECK_FALSE(name.contains('#'));
    }
}

TEST_CASE("Loop break with single duplicate produces name#1", "[TripLinePlotTask]")
{
    auto f = makeRegion();
    // Chain 1: a1 → a2 → a3 → a4
    addChunk(f.trip, {"a1", "a2", "a3", "a4"});
    // Chunk 2 closes loop back to a1. Both endpoints a4 and a1 already exist
    // → 2 duplicates → a4 keeps (anchor), a1 is renamed to a1#1.
    addChunk(f.trip, {"a4", "a1"});

    const auto components = runAndWait(f.trip);
    REQUIRE(components.size() == 1);
    const auto &c = components.first();
    CHECK(c.renameRemap.size() == 1);
    REQUIRE(c.renameRemap.contains("a1#1"));
    CHECK(c.renameRemap.value("a1#1").compare("a1", Qt::CaseInsensitive) == 0);

    // Positions of a1 and a1#1 differ: misclose is visible.
    CHECK(c.positions.hasPosition("a1#1"));
    CHECK(c.positions.position("a1") != c.positions.position("a1#1"));
}

TEST_CASE("Three-plus duplicates in one chunk all rename except the first", "[TripLinePlotTask]")
{
    auto f = makeRegion();
    addChunk(f.trip, {"a", "b", "c", "d"});
    // Chunk 2 has [b, c, d] — all already seen. First duplicate b is the
    // anchor; c and d get renamed to c#1 and d#1.
    addChunk(f.trip, {"b", "c", "d"});

    const auto components = runAndWait(f.trip);
    REQUIRE(components.size() == 1);
    const auto &c = components.first();
    CHECK(c.renameRemap.size() == 2);
    CHECK(c.renameRemap.contains("c#1"));
    CHECK(c.renameRemap.contains("d#1"));
    CHECK_FALSE(c.renameRemap.contains("b#1")); // b stays (anchor)
}

TEST_CASE("Counter does not reset across loop-closing chunks", "[TripLinePlotTask]")
{
    auto f = makeRegion();
    // Build a fan: a1→a2, a1→a3, a1→a4. After chunk 1 seeds {a1,a2}, chunks
    // 2 and 3 each bring a1 as a duplicate but only 1 duplicate → no rename.
    // Now bring a chunk that duplicates both a1 and a2 — twice.
    addChunk(f.trip, {"a1", "a2", "a3", "a4"});
    // Chunk 2: closes loop via a2, a3 both existing
    addChunk(f.trip, {"a2", "a3"});
    // Chunk 3: closes another loop via a3, a4 both existing
    addChunk(f.trip, {"a3", "a4"});

    const auto components = runAndWait(f.trip);
    REQUIRE(components.size() == 1);
    const auto &c = components.first();
    // Chunk 2: anchor = a2, rename a3 → a3#1
    // Chunk 3: anchor = a3, rename a4 → a4#1
    // a3 is not renamed twice because chunk 3's duplicates are [a3 (index 0), a4 (index 1)]
    // — a3 is first-indexed, stays; a4 → a4#1.
    CHECK(c.renameRemap.size() == 2);
    CHECK(c.renameRemap.contains("a3#1"));
    CHECK(c.renameRemap.contains("a4#1"));
}

TEST_CASE("Same name duplicated across multiple loop-closing chunks yields #1, #2, ...", "[TripLinePlotTask]")
{
    auto f = makeRegion();
    addChunk(f.trip, {"a1", "a2", "x", "y"});
    // Each closing chunk brings x + one new station — only 1 duplicate per
    // chunk, so x is not renamed. We need ≥2 duplicates to force rename.
    // Bring x twice with another existing station.
    addChunk(f.trip, {"y", "x"});      // duplicates [y, x], anchor=y, x → x#1
    addChunk(f.trip, {"a1", "x"});     // duplicates [a1, x], anchor=a1, x → x#2

    const auto components = runAndWait(f.trip);
    REQUIRE(components.size() == 1);
    const auto &c = components.first();
    CHECK(c.renameRemap.contains("x#1"));
    CHECK(c.renameRemap.contains("x#2"));
}

TEST_CASE("Disconnected components are returned separately ordered by size", "[TripLinePlotTask]")
{
    auto f = makeRegion();
    addChunk(f.trip, {"a1", "a2", "a3", "a4", "a5"}); // 5 stations
    addChunk(f.trip, {"b1", "b2"});                    // 2 stations

    const auto components = runAndWait(f.trip);
    REQUIRE(components.size() == 2);
    // Ordered by size descending.
    CHECK(components.at(0).network.stations().size() == 5);
    CHECK(components.at(1).network.stations().size() == 2);

    // Components share no stations.
    const auto s0 = components.at(0).network.stations();
    const auto s1 = components.at(1).network.stations();
    for (const auto &name : s0) {
        CHECK_FALSE(s1.contains(name));
    }
}

TEST_CASE("Declination is stripped: 0° and 7° produce identical geometry", "[TripLinePlotTask]")
{
    auto f1 = makeRegion();
    addChunk(f1.trip, {"a1", "a2", "a3"});
    f1.trip->calibrations()->setDeclination(0.0);
    const auto r1 = runAndWait(f1.trip);

    auto f2 = makeRegion();
    addChunk(f2.trip, {"a1", "a2", "a3"});
    f2.trip->calibrations()->setDeclination(7.0);
    const auto r2 = runAndWait(f2.trip);

    REQUIRE(r1.size() == 1);
    REQUIRE(r2.size() == 1);

    auto eq = [](const QVector3D &a, const QVector3D &b) {
        return std::abs(a.x() - b.x()) < 1e-3
            && std::abs(a.y() - b.y()) < 1e-3
            && std::abs(a.z() - b.z()) < 1e-3;
    };

    CHECK(eq(r1.first().positions.position("a1"), r2.first().positions.position("a1")));
    CHECK(eq(r1.first().positions.position("a2"), r2.first().positions.position("a2")));
    CHECK(eq(r1.first().positions.position("a3"), r2.first().positions.position("a3")));
}

TEST_CASE("cwTripLinePlotTask returns empty for null trip", "[TripLinePlotTask]")
{
    auto input = cwTripLinePlotTask::buildInput(nullptr);
    auto future = cwTripLinePlotTask::run(std::move(input));
    REQUIRE(AsyncFuture::waitForFinished(future, 5000));
    CHECK(future.result().isEmpty());
}

TEST_CASE("Canonical anchor is the first station of the first chunk in the component", "[TripLinePlotTask]")
{
    auto f = makeRegion();
    addChunk(f.trip, {"start", "mid", "end"});
    addChunk(f.trip, {"p1", "p2", "p3"});

    const auto components = runAndWait(f.trip);
    REQUIRE(components.size() == 2);
    // Largest first — but both are size 3 here. Check both anchors are
    // present regardless of order.
    QStringList anchors;
    for (const auto &c : components) {
        anchors << c.canonicalAnchor;
    }
    CHECK(anchors.contains("start"));
    CHECK(anchors.contains("p1"));
}
