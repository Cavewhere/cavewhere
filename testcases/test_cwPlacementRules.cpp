/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

// Our
#include "cwPlacementRule.h"
#include "cwPlacementRuleRegistry.h"
#include "cwPlacementRuleParams.h"
#include "cwStrokePath.h"
#include "cwDecorationLayer.h"
#include "cwStampPosition.h"

// Qt
#include <QPainterPath>
#include <QPointF>
#include <QPolygonF>
#include <QVector>

// Std
#include <cmath>
#include <limits>

using Catch::Approx;

// Per-rule unit tests: each rule is exercised in isolation, without the
// tessellation cache, seed, or the layout pipeline. A cwStrokePath, a
// cwDecorationLayer, and (for stamp rules) a glyph QPainterPath are built by hand
// so the exact geometry each rule produces can be asserted — coverage the layout
// integration test only checks as "non-empty". Pipeline composition lives in
// test_cwSketchDecorationLayout; rule resolution in test_cwPlacementRuleRegistry.

namespace {

const cwPlacementRule *rule(const QString &name)
{
    return cwPlacementRuleRegistry::instance().rule(name);
}

// glyph (0,0)->(1,0): a unit segment along +X, as moveTo + lineTo.
QPainterPath unitSegmentGlyph()
{
    QPainterPath path;
    path.moveTo(0.0, 0.0);
    path.lineTo(1.0, 0.0);
    return path;
}

QVector<QPointF> elementPoints(const QPainterPath &path)
{
    QVector<QPointF> points;
    for (int i = 0; i < path.elementCount(); ++i) {
        const QPainterPath::Element element = path.elementAt(i);
        points.append(QPointF(element.x, element.y));
    }
    return points;
}

void checkPoint(const QPointF &actual, const QPointF &expected)
{
    CHECK(actual.x() == Approx(expected.x()).margin(1e-9));
    CHECK(actual.y() == Approx(expected.y()).margin(1e-9));
}

// A quarter-circle polyline (radius r, sweeping +X then curving toward +Y),
// sampled finely so the bending rule has a smooth curve to follow.
QVector<QPointF> quarterArc(double radius, int segments)
{
    QVector<QPointF> points;
    for (int i = 0; i <= segments; ++i) {
        const double a = (M_PI / 2.0) * i / segments;
        points.append(QPointF(radius * std::sin(a), radius * (1.0 - std::cos(a))));
    }
    return points;
}

// Largest distance from any vertex of `path` to its nearest point on the stroke.
double maxDeviationFromStroke(const QPainterPath &path, const cwStrokePath &strokePath)
{
    double maxDeviation = 0.0;
    for (int i = 0; i < path.elementCount(); ++i) {
        const QPointF p(path.elementAt(i).x, path.elementAt(i).y);
        const QPointF onStroke = strokePath.pointAtArcLength(strokePath.arcLengthNearPoint(p));
        maxDeviation = std::max(maxDeviation, std::hypot(p.x() - onStroke.x(), p.y() - onStroke.y()));
    }
    return maxDeviation;
}

QPointF elementPoint(const QPainterPath &path, int i)
{
    return QPointF(path.elementAt(i).x, path.elementAt(i).y);
}

} // namespace

TEST_CASE("Uniform spacing seeds stamps at each arclength step", "[cwPlacementRule]")
{
    const QVector<QPointF> points = {QPointF(0.0, 0.0), QPointF(10.0, 0.0)};
    const cwStrokePath strokePath(points);

    cwDecorationLayer layer;
    layer.glyphName = QStringLiteral("g");
    // worldPerPaperMm = 1 -> the 2 mm default step is 2 world metres.
    const cwPlacementContext context{strokePath, layer, 1.0};

    QVector<cwStampPosition> positions;
    rule(QStringLiteral("Uniform spacing"))->apply(positions, context);

    // s = 0,2,4,6,8,10 along a 10 m stroke -> 6 stamps.
    REQUIRE(positions.size() == 6);
    checkPoint(positions.first().anchorWorld, QPointF(0.0, 0.0));
    checkPoint(positions.at(3).anchorWorld, QPointF(6.0, 0.0));
    checkPoint(positions.last().anchorWorld, QPointF(10.0, 0.0));
    CHECK(positions.first().glyphName == QStringLiteral("g"));
}

TEST_CASE("Uniform spacing seeds nothing on a degenerate or zero-spacing layer",
          "[cwPlacementRule]")
{
    cwDecorationLayer layer;

    SECTION("degenerate stroke") {
        const QVector<QPointF> points = {QPointF(0.0, 0.0)};
        const cwStrokePath strokePath(points);
        const cwPlacementContext context{strokePath, layer, 1.0};
        QVector<cwStampPosition> positions;
        rule(QStringLiteral("Uniform spacing"))->apply(positions, context);
        CHECK(positions.isEmpty());
    }

    SECTION("zero world-per-paper-mm -> zero spacing") {
        const QVector<QPointF> points = {QPointF(0.0, 0.0), QPointF(10.0, 0.0)};
        const cwStrokePath strokePath(points);
        const cwPlacementContext context{strokePath, layer, 0.0};
        QVector<cwStampPosition> positions;
        rule(QStringLiteral("Uniform spacing"))->apply(positions, context);
        CHECK(positions.isEmpty());
    }
}

TEST_CASE("Explicit point seeds a single stamp at the stroke start", "[cwPlacementRule]")
{
    const QVector<QPointF> points = {QPointF(2.0, 3.0), QPointF(9.0, 3.0)};
    const cwStrokePath strokePath(points);

    cwDecorationLayer layer;
    layer.glyphName = QStringLiteral("g");
    const cwPlacementContext context{strokePath, layer, 1.0};

    QVector<cwStampPosition> positions;
    rule(QStringLiteral("Explicit point"))->apply(positions, context);

    REQUIRE(positions.size() == 1);
    checkPoint(positions.first().anchorWorld, QPointF(2.0, 3.0)); // arclength 0
    CHECK(positions.first().glyphName == QStringLiteral("g"));

    SECTION("degenerate stroke seeds nothing") {
        const QVector<QPointF> single = {QPointF(0.0, 0.0)};
        const cwStrokePath empty(single);
        const cwPlacementContext emptyContext{empty, layer, 1.0};
        QVector<cwStampPosition> none;
        rule(QStringLiteral("Explicit point"))->apply(none, emptyContext);
        CHECK(none.isEmpty());
    }
}

TEST_CASE("Align to tangent orients each stamp to its local tangent", "[cwPlacementRule]")
{
    // An L: a horizontal segment then a vertical one, so two stamps on different
    // segments get different tangent angles.
    const QVector<QPointF> points = {QPointF(0.0, 0.0), QPointF(10.0, 0.0), QPointF(10.0, 10.0)};
    const cwStrokePath strokePath(points);

    cwDecorationLayer layer;
    const cwPlacementContext context{strokePath, layer, 1.0};

    QVector<cwStampPosition> positions(2);
    positions[0].anchorWorld = QPointF(3.0, 0.0);   // on the horizontal segment
    positions[1].anchorWorld = QPointF(10.0, 7.0);  // on the vertical segment

    rule(QStringLiteral("Align to tangent"))->apply(positions, context);

    CHECK(positions.at(0).rotationRad == Approx(0.0).margin(1e-9));        // tangent +X
    CHECK(positions.at(1).rotationRad == Approx(M_PI / 2.0).margin(1e-9)); // tangent +Y
}

TEST_CASE("Align to tangent leaves stamps untouched on a degenerate stroke",
          "[cwPlacementRule]")
{
    const QVector<QPointF> points = {QPointF(0.0, 0.0)};
    const cwStrokePath strokePath(points);

    cwDecorationLayer layer;
    const cwPlacementContext context{strokePath, layer, 1.0};

    QVector<cwStampPosition> positions(1);
    positions[0].rotationRad = 0.5;
    rule(QStringLiteral("Align to tangent"))->apply(positions, context);

    CHECK(positions.at(0).rotationRad == Approx(0.5)); // unchanged
}

TEST_CASE("Rigid stamp transforms the glyph by scale, rotation, and anchor",
          "[cwPlacementRule]")
{
    const QVector<QPointF> points = {QPointF(0.0, 0.0), QPointF(1.0, 0.0)};
    const cwStrokePath strokePath(points); // unused by the rigid transform

    cwStampPosition position;
    position.anchorWorld = QPointF(5.0, 7.0);
    position.scale = 2.0;
    position.rotationRad = M_PI / 2.0; // 90 deg CCW (world +Y up)

    cwDecorationLayer layer;
    const cwPlacementContext context{strokePath, layer, 0.0};
    const QPainterPath placed =
        rule(QStringLiteral("Rigid stamp"))
            ->stampPath(position, unitSegmentGlyph(), context);

    // glyph +X unit -> rotated 90 deg CCW = +Y, scaled x2, translated to anchor:
    // (0,0)->(5,7); (1,0)->(5,9).
    const QVector<QPointF> got = elementPoints(placed);
    REQUIRE(got.size() == 2);
    checkPoint(got.at(0), QPointF(5.0, 7.0));
    checkPoint(got.at(1), QPointF(5.0, 9.0));
}

TEST_CASE("The stamp base copies the layer's collision priority onto each stamp",
          "[cwPlacementRule]")
{
    const QVector<QPointF> points = {QPointF(0.0, 0.0), QPointF(10.0, 0.0)};
    const cwStrokePath strokePath(points);

    cwDecorationLayer layer;
    layer.collisionPriority = 7;
    const cwPlacementContext context{strokePath, layer, 1.0};

    QVector<cwStampPosition> positions(2);
    rule(QStringLiteral("Rigid stamp"))->apply(positions, context);

    CHECK(positions.at(0).priority == 7);
    CHECK(positions.at(1).priority == 7);
}

TEST_CASE("Jointed stamp warps each glyph vertex along the stroke arclength",
          "[cwPlacementRule]")
{
    // A straight horizontal stroke: pointAtArcLength(s) = (s, 0), left normal = (0, 1),
    // so warp((gx, gy)) = (startArc + gx, gy) at scale 1.
    const QVector<QPointF> points = {QPointF(0.0, 0.0), QPointF(10.0, 0.0)};
    const cwStrokePath strokePath(points);

    cwStampPosition position;
    position.anchorWorld = QPointF(4.0, 0.0); // arclength 4 along the stroke
    position.scale = 1.0;

    QPainterPath glyph;
    glyph.moveTo(0.0, 0.0);
    glyph.lineTo(2.0, 0.0); // +X advances along arclength
    glyph.lineTo(0.0, 1.0); // +Y offsets along the normal

    cwDecorationLayer layer;
    const cwPlacementContext context{strokePath, layer, 0.0};
    const QPainterPath warped =
        rule(QStringLiteral("Jointed stamp"))
            ->stampPath(position, glyph, context);

    const QVector<QPointF> got = elementPoints(warped);
    REQUIRE(got.size() == 3);
    checkPoint(got.at(0), QPointF(4.0, 0.0)); // (0,0) -> arclength 4
    checkPoint(got.at(1), QPointF(6.0, 0.0)); // (2,0) -> arclength 6
    checkPoint(got.at(2), QPointF(4.0, 1.0)); // (0,1) -> arclength 4, +1 along normal
}

TEST_CASE("Jointed stamp leaves a long edge as a straight chord across a curve",
          "[cwPlacementRule]")
{
    // A long glyph edge over a bending stroke: jointed warps only the two
    // endpoints, so the connecting chord bows off the curve by the sagitta.
    const QVector<QPointF> points = quarterArc(10.0, 64);
    const cwStrokePath strokePath(points);

    QPainterPath glyph;
    glyph.moveTo(-3.0, 0.0);
    glyph.lineTo(3.0, 0.0); // y == 0 -> warped endpoints land on the stroke

    cwStampPosition position;
    position.anchorWorld = strokePath.pointAtArcLength(strokePath.totalLength() / 2.0);
    position.scale = 1.0;

    cwDecorationLayer layer;
    const cwPlacementContext context{strokePath, layer, 1.0};
    const QPainterPath warped =
        rule(QStringLiteral("Jointed stamp"))
            ->stampPath(position, glyph, context);

    // No subdivision: exactly the two warped vertices.
    REQUIRE(warped.elementCount() == 2);
    // The chord midpoint cuts inside the arc, deviating by the sagitta.
    const QPointF chordMid = 0.5 * (elementPoint(warped, 0) + elementPoint(warped, 1));
    const QPointF onStroke = strokePath.pointAtArcLength(strokePath.arcLengthNearPoint(chordMid));
    CHECK(std::hypot(chordMid.x() - onStroke.x(), chordMid.y() - onStroke.y()) > 0.05);
}

TEST_CASE("Bending stamp subdivides a long edge to follow a curved stroke",
          "[cwPlacementRule]")
{
    const QVector<QPointF> points = quarterArc(10.0, 64);
    const cwStrokePath strokePath(points);

    QPainterPath glyph;
    glyph.moveTo(-3.0, 0.0);
    glyph.lineTo(3.0, 0.0); // y == 0 -> every warped sample lies on the stroke

    cwStampPosition position;
    position.anchorWorld = strokePath.pointAtArcLength(strokePath.totalLength() / 2.0);
    position.scale = 1.0;

    cwDecorationLayer layer;
    const cwPlacementContext context{strokePath, layer, 1.0};
    const QPainterPath warped =
        rule(QStringLiteral("Bending stamp"))
            ->stampPath(position, glyph, context);

    // Subdivided into many samples (step 0.25 mm over a 6 m edge at scale 1).
    CHECK(warped.elementCount() > 10);
    // Every sample hugs the curve, where the jointed chord deviated by the sagitta.
    CHECK(maxDeviationFromStroke(warped, strokePath) < 0.05);
}

TEST_CASE("Bending stamp leaves a short edge as a single chord", "[cwPlacementRule]")
{
    const QVector<QPointF> points = quarterArc(10.0, 64);
    const cwStrokePath strokePath(points);

    QPainterPath glyph;
    glyph.moveTo(0.0, 0.0);
    glyph.lineTo(0.1, 0.0); // 0.1 m edge << 0.25 mm * worldPerPaperMm step

    cwStampPosition position;
    position.anchorWorld = strokePath.pointAtArcLength(strokePath.totalLength() / 2.0);
    position.scale = 1.0;

    cwDecorationLayer layer;
    const cwPlacementContext context{strokePath, layer, 1.0};
    const QPainterPath warped =
        rule(QStringLiteral("Bending stamp"))
            ->stampPath(position, glyph, context);

    CHECK(warped.elementCount() == 2); // N == 1: no extra samples
}

TEST_CASE("Bending and jointed stamps agree on a straight stroke", "[cwPlacementRule]")
{
    const QVector<QPointF> points = {QPointF(0.0, 0.0), QPointF(20.0, 0.0)};
    const cwStrokePath strokePath(points);

    QPainterPath glyph;
    glyph.moveTo(-3.0, 0.5);
    glyph.lineTo(3.0, -0.5);

    cwStampPosition position;
    position.anchorWorld = QPointF(10.0, 0.0);
    position.scale = 1.0;
    cwDecorationLayer layer;
    const cwPlacementContext context{strokePath, layer, 1.0};

    const QPainterPath bent = rule(QStringLiteral("Bending stamp"))->stampPath(position, glyph, context);
    const QPainterPath jointed = rule(QStringLiteral("Jointed stamp"))->stampPath(position, glyph, context);

    // Same extent: subdivision lands on the same line, so the endpoints match.
    REQUIRE(bent.elementCount() >= 2);
    REQUIRE(jointed.elementCount() == 2);
    checkPoint(elementPoint(bent, 0), elementPoint(jointed, 0));
    checkPoint(elementPoint(bent, bent.elementCount() - 1), elementPoint(jointed, 1));

    // Every bending sample is collinear with the jointed chord (zero cross product).
    const QPointF a = elementPoint(jointed, 0);
    const QPointF b = elementPoint(jointed, 1);
    for (int i = 0; i < bent.elementCount(); ++i) {
        const QPointF p = elementPoint(bent, i);
        const double cross = (b.x() - a.x()) * (p.y() - a.y()) - (b.y() - a.y()) * (p.x() - a.x());
        CHECK(cross == Approx(0.0).margin(1e-9));
    }
}

TEST_CASE("Bending stamp guards against a degenerate (non-positive) scale",
          "[cwPlacementRule]")
{
    // The layout always supplies a positive worldPerPaperMm, but a directly-built
    // context could leave it 0 -> stepWorld == 0. The guard must fall back to a
    // single chord (N == 1, identical to jointed) instead of dividing by zero.
    const QVector<QPointF> points = quarterArc(10.0, 64);
    const cwStrokePath strokePath(points);

    QPainterPath glyph;
    glyph.moveTo(-3.0, 0.0);
    glyph.lineTo(3.0, 0.0);

    cwStampPosition position;
    position.anchorWorld = strokePath.pointAtArcLength(strokePath.totalLength() / 2.0);
    position.scale = 1.0;

    cwDecorationLayer layer;
    const cwPlacementContext context{strokePath, layer, 0.0}; // worldPerPaperMm = 0
    const QPainterPath bent =
        rule(QStringLiteral("Bending stamp"))->stampPath(position, glyph, context);
    const QPainterPath jointed =
        rule(QStringLiteral("Jointed stamp"))->stampPath(position, glyph, context);

    REQUIRE(bent.elementCount() == 2);
    REQUIRE(jointed.elementCount() == 2);
    checkPoint(elementPoint(bent, 0), elementPoint(jointed, 0));
    checkPoint(elementPoint(bent, 1), elementPoint(jointed, 1));
}

TEST_CASE("Bending stamp caps subdivisions at the defensive maximum",
          "[cwPlacementRule]")
{
    // An edge whose unclamped count (120000) exceeds the cap (100000). A long
    // straight stroke keeps every sample on a distinct point so the element count
    // reflects the loop bound directly (no QPainterPath dedup of coincident points).
    const QVector<QPointF> points = {QPointF(0.0, 0.0), QPointF(30000.0, 0.0)};
    const cwStrokePath strokePath(points);

    QPainterPath glyph;
    glyph.moveTo(0.0, 0.0);
    glyph.lineTo(30000.0, 0.0); // 30000 m / (0.25 mm * 1) step = 120000 > cap

    cwStampPosition position;
    position.anchorWorld = QPointF(0.0, 0.0);
    position.scale = 1.0;

    cwDecorationLayer layer;
    const cwPlacementContext context{strokePath, layer, 1.0};
    const QPainterPath warped =
        rule(QStringLiteral("Bending stamp"))->stampPath(position, glyph, context);

    // Clamped to kMaxBendingSubdivisions (100000) lineTos + the moveTo, not the
    // uncapped 120001 — and never an overflowed double->int cast.
    CHECK(warped.elementCount() == 100001);
}

TEST_CASE("Bending stamp subdivides every segment of a multi-vertex glyph",
          "[cwPlacementRule]")
{
    const QVector<QPointF> points = quarterArc(10.0, 64);
    const cwStrokePath strokePath(points);

    // A peak: two long segments meeting at an off-axis vertex (0, 1).
    QPainterPath glyph;
    glyph.moveTo(-3.0, 0.0);
    glyph.lineTo(0.0, 1.0);
    glyph.lineTo(3.0, 0.0);

    cwStampPosition position;
    position.anchorWorld = strokePath.pointAtArcLength(strokePath.totalLength() / 2.0);
    position.scale = 1.0;

    cwDecorationLayer layer;
    const cwPlacementContext context{strokePath, layer, 1.0};
    const QPainterPath bent = rule(QStringLiteral("Bending stamp"))->stampPath(position, glyph, context);

    // Both ~3.16 m segments subdivide at the 0.25 m step, far beyond the 3 glyph
    // vertices; jointed keeps exactly 3.
    CHECK(bent.elementCount() > 20);
    CHECK(rule(QStringLiteral("Jointed stamp"))->stampPath(position, glyph, context).elementCount() == 3);

    // The shared peak vertex (0, 1) must land at the analytic warp — on the stroke
    // at the anchor arclength, pushed 1 m along the normal. It being there proves
    // the second segment resumes from the right previous glyph point.
    const double s = strokePath.arcLengthNearPoint(position.anchorWorld);
    const QPointF expectedPeak = strokePath.pointAtArcLength(s) + strokePath.normalAtArcLength(s);
    double nearest = std::numeric_limits<double>::max();
    for (int i = 0; i < bent.elementCount(); ++i) {
        const QPointF p = elementPoint(bent, i);
        nearest = std::min(nearest, std::hypot(p.x() - expectedPeak.x(), p.y() - expectedPeak.y()));
    }
    CHECK(nearest == Approx(0.0).margin(1e-9));
}

TEST_CASE("Stamp warp scales the glyph's arclength advance and lateral offset",
          "[cwPlacementRule]")
{
    // Straight stroke so the warp is exactly (S + scale*x, scale*y).
    const QVector<QPointF> points = {QPointF(0.0, 0.0), QPointF(10.0, 0.0)};
    const cwStrokePath strokePath(points);

    cwStampPosition position;
    position.anchorWorld = QPointF(4.0, 0.0); // arclength 4
    position.scale = 2.0;

    QPainterPath glyph;
    glyph.moveTo(0.0, 0.0);
    glyph.lineTo(1.0, 0.0); // +X * scale -> +2 arclength
    glyph.lineTo(0.0, 1.0); // +Y * scale -> +2 along the normal

    cwDecorationLayer layer;
    const cwPlacementContext context{strokePath, layer, 0.0};
    const QPainterPath warped =
        rule(QStringLiteral("Jointed stamp"))
            ->stampPath(position, glyph, context);

    const QVector<QPointF> got = elementPoints(warped);
    REQUIRE(got.size() == 3);
    checkPoint(got.at(0), QPointF(4.0, 0.0)); // (0,0) -> arclength 4
    checkPoint(got.at(1), QPointF(6.0, 0.0)); // (1,0) -> arclength 4 + 2
    checkPoint(got.at(2), QPointF(4.0, 2.0)); // (0,1) -> arclength 4, +2 normal
}

TEST_CASE("Trace polyline returns the stroke and seeds no stamps", "[cwPlacementRule]")
{
    const QVector<QPointF> strokeWorld = {QPointF(0.0, 0.0), QPointF(1.0, 1.0), QPointF(2.0, 0.0)};
    const cwStrokePath strokePath(strokeWorld);

    cwDecorationLayer layer;
    const cwPlacementContext context{strokePath, layer, 1.0};

    const cwPlacementRule *trace = rule(QStringLiteral("Trace polyline"));

    // Iter 1: offset 0 -> the traced polyline is the stroke itself.
    const QVector<QPolygonF> polylines = trace->tracePolylines(strokeWorld, context);
    REQUIRE(polylines.size() == 1);
    CHECK(polylines.first() == QPolygonF(strokeWorld));

    // It does not seed the per-stamp pipeline.
    QVector<cwStampPosition> positions;
    trace->apply(positions, context);
    CHECK(positions.isEmpty());
}

namespace {

// Run the Offset stroke rule's transformStroke over `points` with offsetMm
// (worldPerPaperMm = 1 -> offsetMm maps 1:1 to world metres).
QVector<QPointF> offsetStroke(const QVector<QPointF> &points, double offsetMm)
{
    const cwStrokePath strokePath(points);
    cwDecorationLayer layer;
    cwPlacementContext context{strokePath, layer, 1.0};
    context.ruleParameters = QVariant::fromValue(cwOffsetStrokeParams{offsetMm});
    return rule(cwOffsetStrokeRuleName())->transformStroke(points, context);
}

// Perpendicular distance from p to the nearest point on `path`.
double distanceToPath(const cwStrokePath &path, const QPointF &p)
{
    const QPointF nearest = path.pointAtArcLength(path.arcLengthNearPoint(p));
    return std::hypot(p.x() - nearest.x(), p.y() - nearest.y());
}

} // namespace

TEST_CASE("Offset stroke pushes a straight stroke by +d / -d along the left normal",
          "[cwPlacementRule]")
{
    // Tangent +X -> left normal +Y, so +d lands on +Y and -d on -Y.
    const QVector<QPointF> points = {QPointF(0.0, 0.0), QPointF(10.0, 0.0)};

    const QVector<QPointF> plus = offsetStroke(points, 3.0);
    REQUIRE(plus.size() == points.size());
    checkPoint(plus.at(0), QPointF(0.0, 3.0));
    checkPoint(plus.at(1), QPointF(10.0, 3.0));

    const QVector<QPointF> minus = offsetStroke(points, -3.0);
    REQUIRE(minus.size() == points.size());
    checkPoint(minus.at(0), QPointF(0.0, -3.0));
    checkPoint(minus.at(1), QPointF(10.0, -3.0));
}

TEST_CASE("Offset stroke leaves the stroke unchanged at offset 0 or with absent params",
          "[cwPlacementRule]")
{
    const QVector<QPointF> points = {QPointF(0.0, 0.0), QPointF(1.0, 1.0), QPointF(2.0, 0.0)};
    CHECK(offsetStroke(points, 0.0) == points);

    // Null / wrong-typed params fall to the 0 mm default -> stroke untouched.
    const cwStrokePath strokePath(points);
    cwDecorationLayer layer;
    const cwPlacementContext nullCtx{strokePath, layer, 1.0};
    CHECK(rule(cwOffsetStrokeRuleName())->transformStroke(points, nullCtx) == points);
}

TEST_CASE("Offset stroke tracks a curve at a constant perpendicular distance",
          "[cwPlacementRule]")
{
    // Finely sampled so the parallel offset hugs the curve (d far below the
    // radius — the regime where the documented self-intersection limit doesn't bite).
    const QVector<QPointF> points = quarterArc(5.0, 64);
    const cwStrokePath strokePath(points);
    const double d = 0.5;

    const QVector<QPointF> offset = offsetStroke(points, d);
    REQUIRE(offset.size() >= points.size());
    for (const QPointF &p : offset) {
        CHECK(distanceToPath(strokePath, p) == Approx(d).margin(d * 0.05));
    }
}

TEST_CASE("Offset stroke rejects a non-finite offset from a crafted file", "[cwPlacementRule]")
{
    // offsetMm is file-driven; +Inf / NaN must leave the stroke untouched rather
    // than produce NaN coordinates downstream.
    const QVector<QPointF> points = {QPointF(0.0, 0.0), QPointF(10.0, 0.0)};
    CHECK(offsetStroke(points, std::numeric_limits<double>::infinity()) == points);
    CHECK(offsetStroke(points, std::numeric_limits<double>::quiet_NaN()) == points);
}
