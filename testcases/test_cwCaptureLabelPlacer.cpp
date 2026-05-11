// Our includes
#include "cwCaptureLabelPlacer.h"

// Catch includes
#include <catch2/catch_test_macros.hpp>

// Qt includes
#include <QLineF>
#include <QPointF>
#include <QRectF>
#include <QSizeF>

TEST_CASE("segmentsCross flags crossing X but not endpoint touches",
          "[cwCaptureLabelPlacer]")
{
    SECTION("crossing X returns true") {
        const QLineF a(0.0, 0.0, 10.0, 10.0);
        const QLineF b(0.0, 10.0, 10.0, 0.0);
        CHECK(cwCaptureLabelPlacer::segmentsCross(a, b));
    }

    SECTION("disjoint segments return false") {
        const QLineF a(0.0, 0.0, 1.0, 1.0);
        const QLineF b(10.0, 10.0, 11.0, 11.0);
        CHECK_FALSE(cwCaptureLabelPlacer::segmentsCross(a, b));
    }

    SECTION("parallel non-overlapping segments return false") {
        const QLineF a(0.0, 0.0, 10.0, 0.0);
        const QLineF b(0.0, 1.0, 10.0, 1.0);
        CHECK_FALSE(cwCaptureLabelPlacer::segmentsCross(a, b));
    }

    SECTION("shared endpoint is a touch, not a cross") {
        const QLineF a(0.0, 0.0, 10.0, 0.0);
        const QLineF b(10.0, 0.0, 10.0, 10.0);
        CHECK_FALSE(cwCaptureLabelPlacer::segmentsCross(a, b));
    }

    SECTION("T-touch where endpoint lies on interior is not a cross") {
        // b's p1 sits in the middle of a; rigid X-crossing this is not.
        const QLineF a(0.0, 0.0, 10.0, 0.0);
        const QLineF b(5.0, 0.0, 5.0, 10.0);
        CHECK_FALSE(cwCaptureLabelPlacer::segmentsCross(a, b));
    }

    SECTION("collinear non-overlapping returns false") {
        const QLineF a(0.0, 0.0, 5.0, 0.0);
        const QLineF b(10.0, 0.0, 15.0, 0.0);
        CHECK_FALSE(cwCaptureLabelPlacer::segmentsCross(a, b));
    }
}

namespace {

cwCaptureLabelPlacer makePlacer(const QRectF& bounds, qreal cellSize)
{
    cwCaptureLabelPlacer placer;
    placer.setObstacleBounds(bounds, cellSize);
    placer.setViewportBounds(bounds);
    placer.setLabelMarginPaperPx(1.0);
    return placer;
}

} // namespace

TEST_CASE("placeLabel keeps leaders apart when a viable cell exists",
          "[cwCaptureLabelPlacer]")
{
    // A wide obstacle band forces both anchors' labels to spiral out far
    // enough that their leaders are non-trivial; the second placement must
    // not cross the first's already-registered leader.
    cwCaptureLabelPlacer placer = makePlacer(QRectF(0, 0, 400, 400), 2.0);
    placer.addObstacleRect(QRectF(0, 180, 400, 40));
    placer.finalize();

    const QSizeF labelSize(20.0, 10.0);

    cwCaptureLabelPlacer::LabelRequest reqA{
        QStringLiteral("A"), QPointF(100.0, 200.0), labelSize};
    const auto placementA = placer.placeLabel(reqA);
    REQUIRE(placementA.placed);
    REQUIRE(QLineF(placementA.leaderStart, placementA.leaderEnd).length() > 0.0);

    placer.addLineObstacle(
        QLineF(placementA.leaderStart, placementA.leaderEnd),
        1.0);

    cwCaptureLabelPlacer::LabelRequest reqB{
        QStringLiteral("B"), QPointF(300.0, 200.0), labelSize};
    const auto placementB = placer.placeLabel(reqB);
    REQUIRE(placementB.placed);

    const QLineF leaderA(placementA.leaderStart, placementA.leaderEnd);
    const QLineF leaderB(placementB.leaderStart, placementB.leaderEnd);
    CHECK_FALSE(cwCaptureLabelPlacer::segmentsCross(leaderA, leaderB));
}

TEST_CASE("placeLabel rejects candidates whose leader would cross an obstacle",
          "[cwCaptureLabelPlacer]")
{
    // Anchor sits inside a horizontal obstacle band, so the spiral picks a
    // cell well above or below the band. A line obstacle at y=60 blocks
    // upward leaders from the anchor at y=200 — but the spiral can still
    // find a cell below the band whose leader stays clear.
    cwCaptureLabelPlacer placer = makePlacer(QRectF(0, 0, 400, 400), 2.0);
    placer.addObstacleRect(QRectF(0, 180, 400, 40));
    placer.finalize();
    placer.addLineObstacle(QLineF(0.0, 60.0, 400.0, 60.0), 1.0);

    const QSizeF labelSize(20.0, 10.0);
    cwCaptureLabelPlacer::LabelRequest req{
        QStringLiteral("X"), QPointF(200.0, 200.0), labelSize};
    const auto placement = placer.placeLabel(req);
    REQUIRE(placement.placed);

    const QLineF leader(placement.leaderStart, placement.leaderEnd);
    const QLineF lineObstacle(0.0, 60.0, 400.0, 60.0);
    CHECK_FALSE(cwCaptureLabelPlacer::segmentsCross(leader, lineObstacle));
    // Resulting placement should be on the unblocked (downward) side.
    CHECK(placement.labelRect.top() > 60.0);
}

TEST_CASE("placeLabel drops a label when every candidate's leader must cross",
          "[cwCaptureLabelPlacer]")
{
    // Sandwich the anchor inside an obstacle band so every viable label
    // sits beyond the band edges. Line obstacles inside the band (between
    // the band edge and the anchor in y) force every candidate leader to
    // cross one of them on its way to the anchor.
    cwCaptureLabelPlacer placer = makePlacer(QRectF(0, 0, 400, 400), 2.0);
    placer.addObstacleRect(QRectF(0, 80, 400, 240));
    placer.finalize();
    placer.addLineObstacle(QLineF(0.0, 120.0, 400.0, 120.0), 1.0);
    placer.addLineObstacle(QLineF(0.0, 280.0, 400.0, 280.0), 1.0);

    const QSizeF labelSize(20.0, 10.0);
    cwCaptureLabelPlacer::LabelRequest req{
        QStringLiteral("Y"), QPointF(200.0, 200.0), labelSize};
    const auto placement = placer.placeLabel(req);
    CHECK_FALSE(placement.placed);
}
