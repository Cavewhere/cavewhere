// test_cwSketchPipeline.cpp

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Qt includes
#include <QSignalSpy>
#include <QCoreApplication>

//Our includes
#include "LoadProjectHelper.h"
#include "cwSketch.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwTripCalibration.h"
#include "cwLinePlotManager.h"
#include "cwFutureManagerModel.h"
#include "cwSurveyNetworkArtifact.h"
#include "cwSurvey2DGeometryArtifact.h"
#include "cwSurvey2DGeometry.h"
#include "cwSurveyNetwork.h"
#include "cwStationPositionLookup.h"
#include "cwCenterlineSketchPainterModel.h"
#include "cwAbstractSketchPainterPathModel.h"
#include "cwSketchManager.h"
#include "cwTripLinePlotTask.h"
#include "asyncfuture.h"


namespace {

// Drain pending queued signals/events so cwSketch's queued dataChanged
// coalescer and any AsyncFuture .context(this, ...) continuations have a
// chance to fire.
void drainEventLoop() {
    QCoreApplication::sendPostedEvents();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
}

// Waits for the sketch's 2D geometry artifact to produce a non-empty result.
// Returns the settled value or falls back to asserting via Catch2.
cwSurvey2DGeometry waitForGeometry(cwSketch *sketch, int timeoutMs = 2000) {
    auto future = sketch->survey2DGeometry()->geometryResult();
    REQUIRE(AsyncFuture::waitForFinished(future, timeoutMs));
    REQUIRE_FALSE(future.result().hasError());
    return future.result().value();
}

} // namespace


TEST_CASE("cwLinePlotManager exposes region-wide cwSurveyNetworkArtifact", "[cwSketchPipeline]") {
    auto project = fileToProject(testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
    auto region = project->cavingRegion();
    REQUIRE(region != nullptr);
    REQUIRE(region->caveCount() >= 1);

    cwFutureManagerModel futureManager;
    cwLinePlotManager linePlotManager;
    linePlotManager.setFutureManagerToken(futureManager.token());
    linePlotManager.setRegion(region);
    linePlotManager.waitToFinish();
    drainEventLoop();

    auto *artifact = linePlotManager.surveyNetworkArtifact();
    REQUIRE(artifact != nullptr);

    auto networkFuture = artifact->surveyNetwork();
    REQUIRE(AsyncFuture::waitForFinished(networkFuture, 2000));
    REQUIRE_FALSE(networkFuture.result().hasError());

    cwSurveyNetwork network = networkFuture.result().value();
    CHECK_FALSE(network.isEmpty());

    const QStringList stationNames = network.stations();
    // Phake Cave 3000 has 14 stations (a1..a14).
    REQUIRE(stationNames.size() == 14);

    // Station names in the line-plot network are prefixed by the cave's
    // integer index (cwLinePlotTask encodes cave names as "0", "1", ...),
    // so we search by suffix.
    auto findStation = [&](const QString &suffix) {
        for (const auto &name : stationNames) {
            if (name.endsWith(suffix)) {
                return name;
            }
        }
        return QString();
    };

    const QString a1 = findStation(".a1");
    REQUIRE_FALSE(a1.isEmpty());
    CHECK(network.hasPosition(a1));

    // Topology spot-checks: a1–a2, a10's three neighbours (a9, a11, a13).
    const QString a2  = findStation(".a2");
    const QString a9  = findStation(".a9");
    const QString a10 = findStation(".a10");
    const QString a11 = findStation(".a11");
    const QString a13 = findStation(".a13");
    REQUIRE_FALSE(a2.isEmpty());
    REQUIRE_FALSE(a10.isEmpty());

    auto a1Neighbors = network.neighbors(a1);
    CHECK(a1Neighbors.contains(a2));

    auto a10Neighbors = network.neighbors(a10);
    CHECK(a10Neighbors.size() == 3);
    CHECK(a10Neighbors.contains(a9));
    CHECK(a10Neighbors.contains(a11));
    CHECK(a10Neighbors.contains(a13));
}


TEST_CASE("cwSketch produces 2D geometry from the shared survey network artifact", "[cwSketchPipeline]") {
    auto project = fileToProject(testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
    auto region = project->cavingRegion();
    REQUIRE(region != nullptr);

    cwFutureManagerModel futureManager;
    cwLinePlotManager linePlotManager;
    linePlotManager.setFutureManagerToken(futureManager.token());
    linePlotManager.setRegion(region);
    linePlotManager.waitToFinish();
    drainEventLoop();

    // Attach a fresh sketch to the region's first cave's first trip. The
    // sketch defaults to Plan view (iteration 1 only supports Plan).
    REQUIRE(region->caveCount() >= 1);
    auto *cave = region->cave(0);
    REQUIRE(cave->tripCount() >= 1);
    auto *trip = cave->trip(0);

    cwSketch sketch(trip);
    CHECK(sketch.viewType() == cwSketch::Plan);

    // Before wiring the network, survey2DGeometry() exists but has no result.
    REQUIRE(sketch.survey2DGeometry() != nullptr);

    sketch.setSurveyNetworkArtifact(linePlotManager.surveyNetworkArtifact());
    drainEventLoop();

    const cwSurvey2DGeometry geometry = waitForGeometry(&sketch);
    CHECK_FALSE(geometry.stations.isEmpty());
    CHECK_FALSE(geometry.shotLines.isEmpty());
    CHECK(geometry.stations.size() == 14);
}


TEST_CASE("cwCenterlineSketchPainterModel populates three rows from survey2DGeometry", "[cwCenterlineSketchPainterModel]") {
    auto project = fileToProject(testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
    auto region = project->cavingRegion();

    cwFutureManagerModel futureManager;
    cwLinePlotManager linePlotManager;
    linePlotManager.setFutureManagerToken(futureManager.token());
    linePlotManager.setRegion(region);
    linePlotManager.waitToFinish();
    drainEventLoop();

    cwSketch sketch;
    sketch.setSurveyNetworkArtifact(linePlotManager.surveyNetworkArtifact());
    drainEventLoop();
    waitForGeometry(&sketch); // ensure the rule has settled

    cwCenterlineSketchPainterModel centerline;
    QSignalSpy resetSpy(&centerline, &cwCenterlineSketchPainterModel::modelReset);
    centerline.setSurvey2DGeometry(sketch.survey2DGeometry());

    // The painter model fans out to a worker thread to build QPainterPaths.
    // Spin the event loop until the model resets.
    QElapsedTimer timer;
    timer.start();
    while (centerline.rowCount() == 0 && timer.elapsed() < 2000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }

    REQUIRE(centerline.rowCount() == 3); // shotLines, stationDots, labels

    // Shot-lines row (row 0) should carry a non-empty QPainterPath; labels
    // row (row 2) must be flagged as a fill (stroke width 0.0).
    const auto shotLinesPath =
        centerline.data(centerline.index(0),
                        cwAbstractSketchPainterPathModel::PainterPathRole)
            .value<QPainterPath>();
    CHECK_FALSE(shotLinesPath.isEmpty());

    const double labelsWidth =
        centerline.data(centerline.index(2),
                        cwAbstractSketchPainterPathModel::StrokeWidthRole)
            .toDouble();
    CHECK(labelsWidth == Catch::Approx(0.0));
}

// ---------------- cwSketchManager per-trip line plot pipeline ----------------

namespace {

cwShot simpleShot(double distance = 10.0, double compass = 0.0)
{
    cwShot shot;
    shot.setDistance(cwDistanceReading(QString::number(distance)));
    shot.setCompass(cwCompassReading(QString::number(compass)));
    shot.setBackCompass(cwCompassReading(QString::number(std::fmod(compass + 180.0, 360.0))));
    shot.setClino(cwClinoReading("0"));
    shot.setBackClino(cwClinoReading("0"));
    return shot;
}

cwStation named(const QString &name)
{
    cwStation s;
    s.setName(name);
    s.setLeft(cwDistanceReading("0"));
    s.setRight(cwDistanceReading("0"));
    s.setUp(cwDistanceReading("0"));
    s.setDown(cwDistanceReading("0"));
    return s;
}

struct MiniRegion {
    std::unique_ptr<cwCavingRegion> region;
    cwCave *cave = nullptr;
    cwTrip *trip = nullptr;
};

MiniRegion buildRegionWithTrip()
{
    MiniRegion mr;
    mr.region = std::make_unique<cwCavingRegion>();
    mr.cave = new cwCave();
    mr.cave->setName("Cave 1");
    mr.region->addCave(mr.cave);
    mr.trip = new cwTrip();
    mr.trip->setName("Trip 1");
    mr.cave->addTrip(mr.trip);

    auto *chunk = new cwSurveyChunk();
    mr.trip->addChunk(chunk);
    chunk->appendShot(named("a1"), named("a2"), simpleShot(10.0));
    chunk->appendShot(named("a2"), named("a3"), simpleShot(10.0));
    return mr;
}

// Spin the event loop until `predicate` returns true or timeout elapses.
bool waitFor(std::function<bool()> predicate, int timeoutMs = 10000)
{
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    return predicate();
}

} // namespace


TEST_CASE("cwSketchManager acquireLinePlot creates a pipeline entry and populates components", "[cwSketchManager][TripLinePlot]") {
    auto mr = buildRegionWithTrip();
    cwSketchManager manager;

    CHECK(manager.latestLinePlot(mr.trip).isEmpty()); // no pipeline yet

    manager.acquireLinePlot(mr.trip);
    REQUIRE(waitFor([&](){
        return !manager.latestLinePlot(mr.trip).isEmpty();
    }));

    const auto components = manager.latestLinePlot(mr.trip);
    REQUIRE(components.size() == 1);
    CHECK(components.first().network.stations().size() == 3);

    manager.releaseLinePlot(mr.trip);
    // Refcount reached 0 → pipeline entry dropped.
    CHECK(manager.latestLinePlot(mr.trip).isEmpty());
}

TEST_CASE("cwSketchManager re-emits linePlotUpdated after a chunk edit", "[cwSketchManager][TripLinePlot]") {
    auto mr = buildRegionWithTrip();
    cwSketchManager manager;

    manager.acquireLinePlot(mr.trip);
    REQUIRE(waitFor([&](){
        return !manager.latestLinePlot(mr.trip).isEmpty();
    }));

    QSignalSpy spy(&manager, &cwSketchManager::linePlotUpdated);

    // Trigger a chunk edit — extend the existing chunk.
    auto *chunk = mr.trip->chunks().first();
    chunk->appendShot(named("a3"), named("a4"), simpleShot(10.0));

    REQUIRE(waitFor([&](){ return spy.count() >= 1; }));

    const auto components = manager.latestLinePlot(mr.trip);
    REQUIRE(components.size() == 1);
    CHECK(components.first().network.stations().size() == 4);

    manager.releaseLinePlot(mr.trip);
}

TEST_CASE("cwSketchManager refcounts shared acquisitions", "[cwSketchManager][TripLinePlot]") {
    auto mr = buildRegionWithTrip();
    cwSketchManager manager;

    manager.acquireLinePlot(mr.trip);
    manager.acquireLinePlot(mr.trip); // second consumer

    REQUIRE(waitFor([&](){
        return !manager.latestLinePlot(mr.trip).isEmpty();
    }));

    manager.releaseLinePlot(mr.trip);
    // Still held by the second consumer.
    CHECK_FALSE(manager.latestLinePlot(mr.trip).isEmpty());

    manager.releaseLinePlot(mr.trip);
    CHECK(manager.latestLinePlot(mr.trip).isEmpty());
}

TEST_CASE("cwTripLinePlotTask projects positions stably under other-trip edits", "[cwSketchPipeline][TripLinePlot]") {
    // Build a region with two trips.
    cwCavingRegion region;
    auto *cave = new cwCave();
    cave->setName("Cave 1");
    region.addCave(cave);

    auto *t1 = new cwTrip(); t1->setName("T1"); cave->addTrip(t1);
    auto *chunk1 = new cwSurveyChunk(); t1->addChunk(chunk1);
    chunk1->appendShot(named("t1a"), named("t1b"), simpleShot(10.0));
    chunk1->appendShot(named("t1b"), named("t1c"), simpleShot(10.0));

    auto *t2 = new cwTrip(); t2->setName("T2"); cave->addTrip(t2);
    auto *chunk2 = new cwSurveyChunk(); t2->addChunk(chunk2);
    chunk2->appendShot(named("t2a"), named("t2b"), simpleShot(5.0));

    // Snapshot T1's geometry.
    auto f1 = cwTripLinePlotTask::run(cwTripLinePlotTask::buildInput(t1));
    REQUIRE(AsyncFuture::waitForFinished(f1, 10000));
    const auto r1 = f1.result();
    REQUIRE(r1.size() == 1);
    const auto originalT1C = r1.first().positions.position("t1c");

    // Mutate T2 — must not affect T1.
    chunk2->appendShot(named("t2b"), named("t2c"), simpleShot(15.0));

    auto f2 = cwTripLinePlotTask::run(cwTripLinePlotTask::buildInput(t1));
    REQUIRE(AsyncFuture::waitForFinished(f2, 10000));
    const auto r2 = f2.result();
    REQUIRE(r2.size() == 1);
    CHECK(r2.first().positions.position("t1c") == originalT1C);
}
