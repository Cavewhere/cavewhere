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
#include "cwLinePlotManager.h"
#include "cwFutureManagerModel.h"
#include "cwSurveyNetworkArtifact.h"
#include "cwSurvey2DGeometryArtifact.h"
#include "cwSurvey2DGeometry.h"
#include "cwSurveyNetwork.h"
#include "cwCenterlineSketchPainterModel.h"
#include "cwAbstractSketchPainterPathModel.h"
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
