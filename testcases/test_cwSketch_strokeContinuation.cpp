//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Qt includes
#include <QPointF>
#include <QSignalSpy>

//Std includes
#include <cmath>

using Catch::Approx;

//Our includes
#include "cwSketch.h"
#include "cwSketchViewState.h"
#include "cwPenStroke.h"
#include "cwPenStrokeModel.h"
#include "cwPenPoint.h"
#include "cwScale.h"
#include "cwWorldToScreenMatrix.h"

namespace {

// Calibrate both scales so that:
//   pxPerMeter (for probation/blend pen-travel) = 1 at zoom 1.
//   paperPpm   (for stroke hit threshold)       = 1 independently.
// Each target has a different formula, so we tune them separately:
//   pxPerMeter = mapScaleRatio_matrix · 1000 · pixelDensity
//   paperPpm   = mapScaleRatio_sketch · kTargetDPI(200) · (1000/25.4)
struct UnitScale {
    cwWorldToScreenMatrix matrix;
    UnitScale() {
        matrix.scale()->setScale(1.0);
        matrix.setPixelDensity(0.001);
    }
};

void attach(cwSketch &sketch, UnitScale &unit)
{
    sketch.viewState()->setWorldToScreenMatrix(&unit.matrix);
    // Sketch's own map scale drives the paperPpm used for hit thresholds.
    // Pick ratio = 25.4 / (200 × 1000) so paperPpm = 1.
    sketch.mapScale()->setScale(25.4 / 200000.0);
    REQUIRE(sketch.viewState()->pixelsPerMeter() == Catch::Approx(1.0));
}

// Draws a horizontal stroke along y=`y` at x=0,1,2,... Returns its row index.
int drawHorizontalStroke(cwSketch &sketch, cwPenStroke::Kind kind,
                         double width, double y, int pointCount)
{
    const int row = sketch.beginStroke(kind, width, QColor("#abcdef"));
    for (int i = 0; i < pointCount; ++i) {
        sketch.appendPoint(row, cwPenPoint(QPointF(i, y), 0.5));
    }
    sketch.endStroke();
    return row;
}

} // namespace

TEST_CASE("findContinuationTarget returns -1 when no stroke is in proximity",
          "[cwSketch][continuation]") {
    cwSketch sketch;
    UnitScale unit;
    attach(sketch, unit);

    // No strokes at all.
    auto empty = sketch.findContinuationTarget(cwPenStroke::Wall, QPointF(0, 0));
    CHECK(empty.strokeIndex == -1);

    // Stroke width 1.0 → threshold 0.5m. Pen 5m away misses.
    drawHorizontalStroke(sketch, cwPenStroke::Wall, 1.0, 0.0, 5);
    auto farAway = sketch.findContinuationTarget(
        cwPenStroke::Wall, QPointF(2.0, 5.0));
    CHECK(farAway.strokeIndex == -1);
}

TEST_CASE("findContinuationTarget picks the nearest qualifying segment",
          "[cwSketch][continuation]") {
    cwSketch sketch;
    UnitScale unit;
    attach(sketch, unit);

    drawHorizontalStroke(sketch, cwPenStroke::Wall, 2.0, 0.0, 5);   // index 0, threshold 1.0m
    drawHorizontalStroke(sketch, cwPenStroke::Wall, 2.0, 0.5, 5);   // index 1 (closer)

    auto hit = sketch.findContinuationTarget(
        cwPenStroke::Wall, QPointF(2.0, 0.3));
    REQUIRE(hit.strokeIndex == 1);
    CHECK(hit.strokeWidth == Catch::Approx(2.0));
}

TEST_CASE("findContinuationTarget hit threshold is zoom-invariant in world meters",
          "[cwSketch][continuation]") {
    // stroke.width is a paper-pixel quantity, and its rendered on-screen
    // thickness grows with user zoom. The world-meter hit threshold must
    // therefore stay constant (= rendered-inner-half on screen) — if we
    // had divided by the zoom-aware pxPerMeter, the world threshold would
    // shrink as the user zoomed in, which is the opposite of what the
    // user expects when they see a fatter line on screen.
    cwSketch sketch;
    UnitScale unit;
    attach(sketch, unit);

    drawHorizontalStroke(sketch, cwPenStroke::Wall, 4.0, 0.0, 5);

    // World threshold = 0.5 × 4.0 / paperPpm(1.0) = 2.0 m at *any* zoom.
    // Pen 0.5 m off centerline must hit at both zoom 1 and zoom 4.
    sketch.viewState()->setZoom(1.0);
    auto hitAtOne = sketch.findContinuationTarget(
        cwPenStroke::Wall, QPointF(2.0, 0.5));
    CHECK(hitAtOne.strokeIndex == 0);

    sketch.viewState()->setZoom(4.0);
    auto hitAtFour = sketch.findContinuationTarget(
        cwPenStroke::Wall, QPointF(2.0, 0.5));
    CHECK(hitAtFour.strokeIndex == 0);
    // (With the old zoom-aware formula the threshold at zoom=4 would have
    // collapsed to 0.25 m and this same pen position would have missed.)
}

TEST_CASE("findContinuationTarget proximity is exactly 0.5 × strokeWidth / paperPpm",
          "[cwSketch][continuation]") {
    cwSketch sketch;
    UnitScale unit;
    attach(sketch, unit);

    // paperPpm is tuned to 1 in UnitScale; stroke width 4.0 → threshold
    // 2.0 m (= the outer edge of the rendered stroke).
    //  - 1.99m off → hit
    //  - 2.01m off → miss
    drawHorizontalStroke(sketch, cwPenStroke::Wall, 4.0, 0.0, 5);

    auto inside = sketch.findContinuationTarget(
        cwPenStroke::Wall, QPointF(2.0, 1.99));
    CHECK(inside.strokeIndex == 0);

    auto outside = sketch.findContinuationTarget(
        cwPenStroke::Wall, QPointF(2.0, 2.01));
    CHECK(outside.strokeIndex == -1);
}

TEST_CASE("findContinuationTarget ignores Eraser strokes",
          "[cwSketch][continuation]") {
    cwSketch sketch;
    UnitScale unit;
    attach(sketch, unit);

    drawHorizontalStroke(sketch, cwPenStroke::Eraser, 4.0, 0.0, 5);

    auto result = sketch.findContinuationTarget(
        cwPenStroke::Eraser, QPointF(2.0, 0.0));
    CHECK(result.strokeIndex == -1);
}

TEST_CASE("findContinuationTarget requires same kind",
          "[cwSketch][continuation]") {
    cwSketch sketch;
    UnitScale unit;
    attach(sketch, unit);

    drawHorizontalStroke(sketch, cwPenStroke::Wall, 4.0, 0.0, 5);

    auto wrongKind = sketch.findContinuationTarget(
        cwPenStroke::ScrapOutline, QPointF(2.0, 0.0));
    CHECK(wrongKind.strokeIndex == -1);

    auto sameKind = sketch.findContinuationTarget(
        cwPenStroke::Wall, QPointF(2.0, 0.0));
    CHECK(sameKind.strokeIndex == 0);
}

TEST_CASE("armProbation commits when hit rate exceeds threshold",
          "[cwSketch][continuation]") {
    cwSketch sketch;
    UnitScale unit;
    attach(sketch, unit);

    // Candidate stroke: width 2.0 → hit threshold 1.0m.
    const int candidateRow =
        drawHorizontalStroke(sketch, cwPenStroke::Wall, 2.0, 0.0, 6);
    REQUIRE(candidateRow == 0);
    const int candidateOriginalSize = sketch.strokes()[candidateRow].points.size();

    QSignalSpy committedSpy(&sketch, &cwSketch::continuationCommitted);
    QSignalSpy rejectedSpy(&sketch, &cwSketch::continuationRejected);

    cwSketchContinuationTarget target;
    target.strokeIndex = candidateRow;
    target.strokeWidth = 2.0;

    // Probation row.
    const int probationRow = sketch.beginStroke(cwPenStroke::Wall, 2.0);
    REQUIRE(probationRow == 1);

    // Probation window 5m, blend window 1m.
    sketch.armProbation(probationRow, target, 5.0, 1.0);

    // Walk along the candidate near (2,0)→(3,0)→(4,0)→(5,0)→(6,0). Each
    // sample lies within 1.0m of the candidate centerline. Travel between
    // consecutive samples = 1.0m; cumulative travel after sample 5 = 4m
    // (no last-raw on first sample). After sample 6 cumulative = 5m → window
    // closes; rate = 6/6 = 1.0 ≥ 0.6 → commit.
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(2.0, 0.1), 0.5));
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(3.0, 0.1), 0.5));
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(4.0, 0.1), 0.5));
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(5.0, 0.1), 0.5));
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(6.0, 0.1), 0.5));
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(7.0, 0.1), 0.5));

    REQUIRE(committedSpy.count() == 1);
    CHECK(rejectedSpy.count() == 0);

    // Probation row is gone; candidate has been truncated + grafted.
    REQUIRE(sketch.strokes().size() == 1);
    const int newActiveIndex = committedSpy.takeFirst().at(0).toInt();
    CHECK(newActiveIndex == 0);

    const auto &cand = sketch.strokes()[0];
    // Furthest hit was on the last segment (index 5), so we keep
    // points[0..4] then append the graft vertex → 6 points (same count as
    // original by coincidence; what matters is the last vertex changed).
    CHECK(cand.points.size() <= candidateOriginalSize);
    CHECK(cand.points.size() >= 2);

    sketch.endStroke();
}

TEST_CASE("armProbation rejects when hit rate is below threshold",
          "[cwSketch][continuation]") {
    cwSketch sketch;
    UnitScale unit;
    attach(sketch, unit);

    const int candidateRow =
        drawHorizontalStroke(sketch, cwPenStroke::Wall, 2.0, 0.0, 6);
    const auto candidateBefore = sketch.strokes()[candidateRow];

    QSignalSpy committedSpy(&sketch, &cwSketch::continuationCommitted);
    QSignalSpy rejectedSpy(&sketch, &cwSketch::continuationRejected);

    cwSketchContinuationTarget target;
    target.strokeIndex = candidateRow;
    target.strokeWidth = 2.0;

    const int probationRow = sketch.beginStroke(cwPenStroke::Wall, 2.0);
    sketch.armProbation(probationRow, target, 5.0, 1.0);

    // First sample lands on the candidate; subsequent samples fly off into
    // the y-direction (well outside the 1.0m hit threshold). Hit rate ≈ 1/N
    // → reject.
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(2.0, 0.0), 0.5));
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(2.0, 2.0), 0.5));
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(2.0, 3.5), 0.5));
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(2.0, 5.0), 0.5));
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(2.0, 6.5), 0.5));

    CHECK(committedSpy.count() == 0);
    REQUIRE(rejectedSpy.count() == 1);

    // Candidate untouched; probation row preserved with its raw points.
    REQUIRE(sketch.strokes().size() == 2);
    const auto &candAfter = sketch.strokes()[candidateRow];
    REQUIRE(candAfter.points.size() == candidateBefore.points.size());
    for (int i = 0; i < candAfter.points.size(); ++i) {
        CHECK(candAfter.points[i].position == candidateBefore.points[i].position);
    }
    const auto &probAfter = sketch.strokes()[probationRow];
    CHECK(probAfter.points.size() == 5);

    sketch.endStroke();
}

TEST_CASE("post-commit blend lerps between candidate tangent and raw pen",
          "[cwSketch][continuation]") {
    cwSketch sketch;
    UnitScale unit;
    attach(sketch, unit);

    // Wide horizontal stroke so the hit threshold is generous.
    drawHorizontalStroke(sketch, cwPenStroke::Wall, 4.0, 0.0, 6);

    cwSketchContinuationTarget target;
    target.strokeIndex = 0;
    target.strokeWidth = 4.0;   // hit threshold 2.0m

    const int probationRow = sketch.beginStroke(cwPenStroke::Wall, 4.0);

    // Probation window 2m, post-commit blend window 2m.
    sketch.armProbation(probationRow, target, 2.0, 2.0);

    // Three on-stroke samples to clear the probation window. Travel:
    //   1→2: 1m, 2→3: 1m → cumulative 2m → window closes on sample 3 → commit.
    // Furthest hit = segment ending at index 3 → graft point ≈ (3, 0),
    // tangent (1, 0).
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(1.0, 0.0), 0.5));
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(2.0, 0.0), 0.5));
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(3.0, 0.0), 0.5));

    // After commit the probation row is gone; candidate is row 0.
    REQUIRE(sketch.strokes().size() == 1);

    const int activeIdx = 0;

    // Now the next pen samples flow into the candidate via the Blend phase.
    // Sample A: pen jumps to (3, 0.5). Travel = 0.5m, t = 0.25.
    //   extrap = (3 + 1·0.5, 0) = (3.5, 0)
    //   stored = lerp(extrap, raw, 0.25)
    //          = (0.75·3.5 + 0.25·3.0, 0.75·0 + 0.25·0.5)
    //          = (3.375, 0.125)
    sketch.appendPoint(activeIdx, cwPenPoint(QPointF(3.0, 0.5), 0.5));
    // Sample B: pen at (3, 1.0). Travel cumulative 1.0m, t = 0.5.
    //   extrap = (4.0, 0)
    //   stored = (0.5·4 + 0.5·3, 0.5·0 + 0.5·1) = (3.5, 0.5)
    sketch.appendPoint(activeIdx, cwPenPoint(QPointF(3.0, 1.0), 0.5));
    // Sample C: pen at (3, 2.5). Travel cumulative 2.5m → past window → raw.
    sketch.appendPoint(activeIdx, cwPenPoint(QPointF(3.0, 2.5), 0.5));

    sketch.endStroke();

    const auto &pts = sketch.strokes()[0].points;
    REQUIRE(pts.size() >= 3);

    // The last three appended samples are the blended/raw pen points.
    const QPointF blendA = pts[pts.size() - 3].position;
    const QPointF blendB = pts[pts.size() - 2].position;
    const QPointF rawC   = pts[pts.size() - 1].position;
    CHECK(blendA.x() == Catch::Approx(3.375));
    CHECK(blendA.y() == Catch::Approx(0.125));
    CHECK(blendB.x() == Catch::Approx(3.5));
    CHECK(blendB.y() == Catch::Approx(0.5));
    CHECK(rawC == QPointF(3.0, 2.5));
}

TEST_CASE("committed continuation produces a single 'Continue Stroke' undo entry",
          "[cwSketch][continuation]") {
    cwSketch sketch;
    UnitScale unit;
    attach(sketch, unit);

    drawHorizontalStroke(sketch, cwPenStroke::Wall, 2.0, 0.0, 6);
    const auto snapshot = sketch.strokes();
    const int undoBefore = sketch.undoStack()->count();

    cwSketchContinuationTarget target;
    target.strokeIndex = 0;
    target.strokeWidth = 2.0;

    const int probationRow = sketch.beginStroke(cwPenStroke::Wall, 2.0);
    sketch.armProbation(probationRow, target, 3.0, 1.0);

    // Stay on the line.
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(1.0, 0.0), 0.5));
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(2.0, 0.0), 0.5));
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(3.0, 0.0), 0.5));
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(4.0, 0.0), 0.5));

    sketch.endStroke();

    REQUIRE(sketch.undoStack()->count() == undoBefore + 1);
    const QString label = sketch.undoStack()->command(undoBefore)->text();
    CHECK(label == QStringLiteral("Continue Stroke"));

    sketch.undoStack()->undo();
    REQUIRE(sketch.strokes().size() == snapshot.size());
    REQUIRE(sketch.strokes().first().points.size() == snapshot.first().points.size());
    for (int i = 0; i < snapshot.first().points.size(); ++i) {
        CHECK(sketch.strokes().first().points[i].position
              == snapshot.first().points[i].position);
    }
}

TEST_CASE("rejected continuation produces a single 'Draw Stroke' undo entry",
          "[cwSketch][continuation]") {
    cwSketch sketch;
    UnitScale unit;
    attach(sketch, unit);

    drawHorizontalStroke(sketch, cwPenStroke::Wall, 2.0, 0.0, 6);
    const auto snapshot = sketch.strokes();
    const int undoBefore = sketch.undoStack()->count();

    cwSketchContinuationTarget target;
    target.strokeIndex = 0;
    target.strokeWidth = 2.0;

    const int probationRow = sketch.beginStroke(cwPenStroke::Wall, 2.0);
    sketch.armProbation(probationRow, target, 3.0, 1.0);

    // Diverge immediately → reject.
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(2.0, 5.0), 0.5));
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(2.0, 7.0), 0.5));
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(2.0, 9.0), 0.5));

    sketch.endStroke();

    REQUIRE(sketch.undoStack()->count() == undoBefore + 1);
    const QString label = sketch.undoStack()->command(undoBefore)->text();
    CHECK(label == QStringLiteral("Draw Stroke"));

    sketch.undoStack()->undo();
    REQUIRE(sketch.strokes().size() == snapshot.size());
}

TEST_CASE("armProbation extends from the head when pen motion is backward",
          "[cwSketch][continuation]") {
    cwSketch sketch;
    UnitScale unit;
    attach(sketch, unit);

    // Horizontal candidate from (0,0) to (5,0): 6 points, 5 segments, width 2
    // → hit threshold 1.0m.
    drawHorizontalStroke(sketch, cwPenStroke::Wall, 2.0, 0.0, 6);
    const auto candidateBefore = sketch.strokes()[0];
    REQUIRE(candidateBefore.points.size() == 6);

    cwSketchContinuationTarget target;
    target.strokeIndex = 0;
    target.strokeWidth = 2.0;

    const int probationRow = sketch.beginStroke(cwPenStroke::Wall, 2.0);
    // Probation 2.5m, blend 1.0m.
    sketch.armProbation(probationRow, target, 2.5, 1.0);

    // Pen lands near (4.5, 0) — first hit lies on segment 5 ([p4,p5]).
    // Then drags back along the stroke to (3.0, 0) (segment 3 or 4) and
    // (1.5, 0) (segment 2). Cumulative travel = 3m → window closes on the
    // third sample. Motion vector = (−3, 0); firstHitTangent = (1, 0) →
    // dot product −3 < 0 → BACKWARD commit.
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(4.5, 0.1), 0.5));
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(3.0, 0.1), 0.5));
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(1.5, 0.1), 0.5));

    // After commit the probation row is gone and the candidate is reversed
    // + truncated. furthestBackwardSeg = 2 (projection of last sample onto
    // segment 2 = (1.5, 0)). N = 6 → reversed-frame furthestSeg = 6 − 2 =
    // 4 → keep reversed[0..3] = [(5,0),(4,0),(3,0),(2,0)], then append
    // graft (1.5, 0) → 5 points total.
    REQUIRE(sketch.strokes().size() == 1);
    const auto &cand = sketch.strokes()[0];
    REQUIRE(cand.points.size() == 5);
    CHECK(cand.points[0].position == QPointF(5.0, 0.0));
    CHECK(cand.points[1].position == QPointF(4.0, 0.0));
    CHECK(cand.points[2].position == QPointF(3.0, 0.0));
    CHECK(cand.points[3].position == QPointF(2.0, 0.0));
    CHECK(cand.points[4].position.x() == Catch::Approx(1.5));
    CHECK(cand.points[4].position.y() == Catch::Approx(0.0));

    // Draw one more point in the Blend phase to confirm the stored array is
    // extending outward from the original head. Blend tangent = −(1,0) =
    // (−1, 0). After 1m of travel past the graft we are clearly past x=1.5.
    sketch.appendPoint(0, cwPenPoint(QPointF(0.0, 0.0), 0.5));
    sketch.endStroke();

    // End of the committed stroke is to the left of the graft, i.e., the
    // new drawing extended past the original head (x=0).
    const auto &afterEnd = sketch.strokes()[0];
    REQUIRE(afterEnd.points.size() >= 6);
    CHECK(afterEnd.points.last().position.x() <= 1.5);
}

TEST_CASE("pen-up before probation closes leaves a normal short stroke",
          "[cwSketch][continuation]") {
    cwSketch sketch;
    UnitScale unit;
    attach(sketch, unit);

    drawHorizontalStroke(sketch, cwPenStroke::Wall, 2.0, 0.0, 6);

    QSignalSpy committedSpy(&sketch, &cwSketch::continuationCommitted);
    QSignalSpy rejectedSpy(&sketch, &cwSketch::continuationRejected);

    cwSketchContinuationTarget target;
    target.strokeIndex = 0;
    target.strokeWidth = 2.0;

    const int probationRow = sketch.beginStroke(cwPenStroke::Wall, 2.0);
    sketch.armProbation(probationRow, target, 100.0, 1.0);  // huge window

    sketch.appendPoint(probationRow, cwPenPoint(QPointF(1.0, 0.0), 0.5));
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(2.0, 0.0), 0.5));
    sketch.endStroke();

    // Window never closed → no commit/reject signal.
    CHECK(committedSpy.count() == 0);
    CHECK(rejectedSpy.count() == 0);
    // Probation row survives intact.
    REQUIRE(sketch.strokes().size() == 2);
    CHECK(sketch.strokes()[1].points.size() == 2);
}

TEST_CASE("findContinuationTarget returns -1 when the matrix is unset",
          "[cwSketch][continuation]") {
    cwSketch sketch;
    drawHorizontalStroke(sketch, cwPenStroke::Wall, 1.0, 0.0, 5);

    auto result = sketch.findContinuationTarget(
        cwPenStroke::Wall, QPointF(2.0, 0.0));
    CHECK(result.strokeIndex == -1);
}
