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

    // Probation window 5m.
    sketch.armProbation(probationRow, target, 5.0);

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

    // Probation row is gone; candidate's overlap region has been replaced
    // by replayed blended samples.
    REQUIRE(sketch.strokes().size() == 1);
    const int newActiveIndex = committedSpy.takeFirst().at(0).toInt();
    CHECK(newActiveIndex == 0);

    const auto &cand = sketch.strokes()[0];
    (void)candidateOriginalSize;
    // prefixCount = firstHitSeg = 2 (first hit at (2, 0.1) projects onto
    // segment 2), and the 6 probation samples are replayed as blended
    // points → 2 + 6 = 8.
    CHECK(cand.points.size() == 8);

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
    sketch.armProbation(probationRow, target, 5.0);

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

TEST_CASE("probation-region blend lerps old centerline with raw pen samples",
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

    // Probation window 2m (= blend zone).
    sketch.armProbation(probationRow, target, 2.0);

    // Three on-stroke samples to clear the probation window:
    //   s_0 = 0   (sample 1 travel=0)
    //   s_1 = 1m  (sample 2 travel=1)
    //   s_2 = 2m  (sample 3 travel=2, window closes and commit fires)
    //
    // First hit is sample 1 at (1, 0.1). The segment scan picks the first
    // segment of equal distance (strict `<` in the tiebreak), so for
    // sample (1, 0.1) firstHitSeg = 1 [(0,0)→(1,0)], clamped projection
    // lands at (1, 0). Furthest forward is sample 3 at (3, 0.1);
    // furthestForwardSeg = 3, projection = (3, 0). overlapStart=(1, 0),
    // overlapEnd=(3, 0).
    //
    // Replayed blend math:
    //   t=0 : oldLine=(1,0); blended = lerp((1,0), (1,0.1), 0) = (1, 0)
    //   t=0.5: oldLine = 0.5·(1,0)+0.5·(3,0) = (2, 0);
    //          blended = 0.5·(2,0) + 0.5·(2, 0.1) = (2, 0.05)
    //   t=1  : oldLine=(3,0); blended = lerp((3,0), (3,0.1), 1) = (3, 0.1)
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(1.0, 0.1), 0.5));
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(2.0, 0.1), 0.5));
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(3.0, 0.1), 0.5));

    // prefixCount = firstHitSeg = 1 → candidate keeps [(0, 0)], then
    // appends 3 blended points → 4 total.
    REQUIRE(sketch.strokes().size() == 1);
    const auto &ptsAfterCommit = sketch.strokes()[0].points;
    REQUIRE(ptsAfterCommit.size() == 4);
    CHECK(ptsAfterCommit[0].position == QPointF(0.0, 0.0));
    CHECK(ptsAfterCommit[1].position.x() == Catch::Approx(1.0));
    CHECK(ptsAfterCommit[1].position.y() == Catch::Approx(0.0));
    CHECK(ptsAfterCommit[2].position.x() == Catch::Approx(2.0));
    CHECK(ptsAfterCommit[2].position.y() == Catch::Approx(0.05));
    CHECK(ptsAfterCommit[3].position.x() == Catch::Approx(3.0));
    CHECK(ptsAfterCommit[3].position.y() == Catch::Approx(0.1));

    // After commit, subsequent samples append raw (no further blend).
    const int activeIdx = 0;
    sketch.appendPoint(activeIdx, cwPenPoint(QPointF(3.5, 0.5), 0.5));
    sketch.appendPoint(activeIdx, cwPenPoint(QPointF(4.0, 1.0), 0.5));
    sketch.endStroke();

    const auto &pts = sketch.strokes()[0].points;
    REQUIRE(pts.size() == 6);
    CHECK(pts[4].position == QPointF(3.5, 0.5));
    CHECK(pts[5].position == QPointF(4.0, 1.0));
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
    sketch.armProbation(probationRow, target, 3.0);

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
    sketch.armProbation(probationRow, target, 3.0);

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
    // Probation window 2.5m (also the blend zone on commit).
    sketch.armProbation(probationRow, target, 2.5);

    // Pen lands near (4.5, 0) — first hit lies on segment 5 ([p4,p5]).
    // Then drags back along the stroke to (3.0, 0) (segment 3) and (1.5, 0)
    // (segment 2). Travel 0 → 1.5 → 3.0; window closes on the third sample.
    // Motion vector = (−3, 0); firstHitTangent = (1, 0) → dot −3 < 0 →
    // BACKWARD commit.
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(4.5, 0.1), 0.5));
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(3.0, 0.1), 0.5));
    sketch.appendPoint(probationRow, cwPenPoint(QPointF(1.5, 0.1), 0.5));

    // After commit: candidate is reversed so its original head (p0) is at
    // the tail of the array, then its overlap region is replaced by
    // replayed blended samples.
    //   prefixCount = N − firstHitSeg = 6 − 5 = 1 → prefix = [(5, 0)]
    //   overlapStart = firstHitWorld = (4.5, 0)
    //   overlapEnd   = furthestBackwardWorld = (1.5, 0)
    // Replayed samples:
    //   t=0   : oldLine=(4.5,0); blended = (4.5, 0)
    //   t=0.6 : oldLine = 0.4·(4.5,0) + 0.6·(1.5,0) = (2.7, 0);
    //           blended = 0.4·(2.7,0) + 0.6·(3, 0.1) = (2.88, 0.06)
    //   t=1   : oldLine=(1.5,0); blended = (1.5, 0.1)
    REQUIRE(sketch.strokes().size() == 1);
    const auto &cand = sketch.strokes()[0];
    REQUIRE(cand.points.size() == 4);
    CHECK(cand.points[0].position == QPointF(5.0, 0.0));
    CHECK(cand.points[1].position.x() == Catch::Approx(4.5));
    CHECK(cand.points[1].position.y() == Catch::Approx(0.0));
    CHECK(cand.points[2].position.x() == Catch::Approx(2.88));
    CHECK(cand.points[2].position.y() == Catch::Approx(0.06));
    CHECK(cand.points[3].position.x() == Catch::Approx(1.5));
    CHECK(cand.points[3].position.y() == Catch::Approx(0.1));

    // Draw one more raw point to confirm subsequent appends extend outward
    // past the original head.
    sketch.appendPoint(0, cwPenPoint(QPointF(0.0, 0.0), 0.5));
    sketch.endStroke();

    const auto &afterEnd = sketch.strokes()[0];
    REQUIRE(afterEnd.points.size() == 5);
    CHECK(afterEnd.points.last().position == QPointF(0.0, 0.0));
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
    sketch.armProbation(probationRow, target, 100.0);  // huge window

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
