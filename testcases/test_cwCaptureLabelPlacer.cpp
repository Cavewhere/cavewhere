// Our includes
#include "cwCaptureLabelPlacer.h"

// Catch includes
#include <catch2/catch_test_macros.hpp>

// Qt includes
#include <QElapsedTimer>
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

TEST_CASE("placeLabel prefers candidates whose leader avoids soft obstacles",
          "[cwCaptureLabelPlacer]")
{
    // Hard obstacle at the anchor forces a non-zero leader. A single soft
    // segment above the anchor would only be crossed by upward-going
    // leaders; the scorer should prefer a sideways or downward placement.
    cwCaptureLabelPlacer placer = makePlacer(QRectF(0, 0, 400, 400), 2.0);
    placer.addObstacleRect(QRectF(195.0, 195.0, 10.0, 10.0));
    placer.finalize();
    const QLineF softLine(150.0, 190.0, 250.0, 190.0);
    placer.addSoftLineObstacle(softLine, 1.0);

    const QSizeF labelSize(10.0, 10.0);
    cwCaptureLabelPlacer::LabelRequest req{
        QStringLiteral("S"), QPointF(200.0, 200.0), labelSize};
    const auto placement = placer.placeLabel(req);
    REQUIRE(placement.placed);

    const QLineF leader(placement.leaderStart, placement.leaderEnd);
    CHECK_FALSE(cwCaptureLabelPlacer::segmentsCross(leader, softLine));
}

TEST_CASE("placeLabel biases toward the DT-gradient direction",
          "[cwCaptureLabelPlacer]")
{
    // A wide obstacle band south of the anchor means the DT gradient
    // points north. With no soft obstacles and a symmetric viewport,
    // the scored search should pick a cell north of the anchor.
    cwCaptureLabelPlacer placer = makePlacer(QRectF(0, 0, 400, 400), 2.0);
    placer.addObstacleRect(QRectF(0.0, 210.0, 400.0, 190.0));
    placer.finalize();

    const QSizeF labelSize(10.0, 10.0);
    cwCaptureLabelPlacer::LabelRequest req{
        QStringLiteral("D"), QPointF(200.0, 210.0), labelSize};
    const auto placement = placer.placeLabel(req);
    REQUIRE(placement.placed);
    CHECK(placement.labelRect.center().y() < 210.0);
}

TEST_CASE("placeLabel culls anchors outside the viewport",
          "[cwCaptureLabelPlacer]")
{
    // The obstacle grid spans the whole page, but the export viewport is a
    // small window in its center. A station whose anchor sits outside that
    // window (plus the label-diagonal margin) must be dropped — it would
    // otherwise get an in-window label pointing off-screen at a dot that isn't
    // drawn. There are no obstacles, so clearance never blocks; placement is
    // governed purely by the viewport clamp, the spiral cap, and the cull.
    cwCaptureLabelPlacer placer;
    placer.setObstacleBounds(QRectF(0, 0, 400, 400), 2.0);
    placer.setViewportBounds(QRectF(150, 150, 100, 100));
    placer.setLabelMarginPaperPx(1.0);
    placer.finalize();

    const QSizeF labelSize(20.0, 10.0);

    // The cull margin is requiredClearanceParent = halfDiag(20x10)+margin ≈
    // 12.18, so cullRect ≈ (137.8, 137.8)-(262.2, 262.2). The two anchors below
    // sit 5 px apart, straddling that boundary at x≈262: both are outside the
    // viewport yet close enough that a clamp-valid label cell (~centered at
    // x=240) is well within the spiral cap. So the ONLY thing that differs
    // between them is which side of cullRect they land on — which isolates the
    // cull from the independent clamp-containment guard.
    SECTION("anchor inside the viewport is placed") {
        cwCaptureLabelPlacer::LabelRequest req{
            QStringLiteral("in"), QPointF(200.0, 200.0), labelSize};
        CHECK(placer.placeLabel(req).placed);
    }

    SECTION("anchor just outside the viewport but within the cull margin still places") {
        cwCaptureLabelPlacer::LabelRequest req{
            QStringLiteral("edge"), QPointF(260.0, 200.0), labelSize};
        CHECK(placer.placeLabel(req).placed);
    }

    SECTION("anchor just beyond the cull margin is culled") {
        // Only the cull rejects this: geometry is all but identical to the
        // section above (a reachable in-viewport cell exists), so a regression
        // that removed the cull would place it and fail this check.
        cwCaptureLabelPlacer::LabelRequest req{
            QStringLiteral("out"), QPointF(265.0, 200.0), labelSize};
        CHECK_FALSE(placer.placeLabel(req).placed);
    }
}

TEST_CASE("placeLabel cull is a no-op when the viewport covers the whole page",
          "[cwCaptureLabelPlacer]")
{
    // The common whole-cave export path sets viewportBounds == bounds. The cull
    // must never fire there: a station near the page edge still gets its label.
    cwCaptureLabelPlacer placer;
    const QRectF page(0, 0, 400, 400);
    placer.setObstacleBounds(page, 2.0);
    placer.setViewportBounds(page);
    placer.setLabelMarginPaperPx(1.0);
    placer.finalize();

    const QSizeF labelSize(20.0, 10.0);
    cwCaptureLabelPlacer::LabelRequest req{
        QStringLiteral("edge"), QPointF(395.0, 200.0), labelSize};
    CHECK(placer.placeLabel(req).placed);
}

TEST_CASE("placeLabel still places when a clear cell exists within the spiral cap",
          "[cwCaptureLabelPlacer]")
{
    // Guards against the spiral cap (kMaxSearchClearanceMultiple) being set too
    // tight. The anchor is buried in a 120px-tall obstacle band, so its label
    // must escape ~37 cells to the nearest clear cell — comfortably inside the
    // ~50-cell cap for this label, but well beyond what a much smaller cap
    // would reach. Every other cap test asserts placed==false; this is the one
    // that fails if the cap is lowered below what real passages need.
    cwCaptureLabelPlacer placer;
    const QRectF page(0, 0, 400, 400);
    placer.setObstacleBounds(page, 2.0);
    placer.setViewportBounds(page);
    placer.setLabelMarginPaperPx(1.0);
    placer.addObstacleRect(QRectF(0.0, 140.0, 400.0, 120.0));
    placer.finalize();

    const QSizeF labelSize(20.0, 10.0);
    cwCaptureLabelPlacer::LabelRequest req{
        QStringLiteral("band"), QPointF(200.0, 200.0), labelSize};
    const auto placement = placer.placeLabel(req);
    REQUIRE(placement.placed);
    // The winning cell must sit clear of the band (above or below it).
    CHECK((placement.labelRect.bottom() < 140.0 || placement.labelRect.top() > 260.0));
}

TEST_CASE("placeLabel gives up quickly for a buried anchor on a huge page",
          "[cwCaptureLabelPlacer]")
{
    // Regression guard for the export hang: an anchor buried in a large
    // obstacle on a very large page used to spiral out to the full page
    // diagonal (m_maskW + m_maskH cells), rescanning obstacles at every ring.
    // The spiral is now capped near the label, so the hopeless placement is
    // dropped in O(label size). placed==false holds with OR without the cap, so
    // the timer is the only signal that distinguishes them: the capped path is
    // sub-millisecond while the regressed full-page spiral here takes seconds to
    // tens of seconds. The 5 s ceiling (matching the modest-survey guard above)
    // stays discriminating while being robust to a loaded CI box.
    cwCaptureLabelPlacer placer;
    placer.setObstacleBounds(QRectF(0, 0, 20000, 20000), 2.0);
    placer.setViewportBounds(QRectF(0, 0, 20000, 20000));
    placer.setLabelMarginPaperPx(1.0);
    // A large solid obstacle around the anchor: no clear cell exists within
    // any sane radius, so the label cannot be placed.
    placer.addObstacleRect(QRectF(9000, 9000, 2000, 2000));
    placer.finalize();

    const QSizeF labelSize(20.0, 10.0);
    cwCaptureLabelPlacer::LabelRequest req{
        QStringLiteral("buried"), QPointF(10000.0, 10000.0), labelSize};

    QElapsedTimer timer;
    timer.start();
    const auto placement = placer.placeLabel(req);
    const qint64 elapsedMs = timer.elapsed();

    CHECK_FALSE(placement.placed);
    CHECK(elapsedMs < 5000);
}
