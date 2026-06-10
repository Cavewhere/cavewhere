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

} // namespace

TEST_CASE("Uniform spacing seeds stamps at each arclength step", "[cwPlacementRule]")
{
    const QVector<QPointF> points = {QPointF(0.0, 0.0), QPointF(10.0, 0.0)};
    const cwStrokePath strokePath(points);

    cwDecorationLayer layer;
    layer.glyphName = QStringLiteral("g");
    // worldPerPaperMm = 1 -> the 2 mm default step is 2 world metres.
    const cwPlacementContext context{strokePath, layer, 1.0, nullptr};

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
        const cwPlacementContext context{strokePath, layer, 1.0, nullptr};
        QVector<cwStampPosition> positions;
        rule(QStringLiteral("Uniform spacing"))->apply(positions, context);
        CHECK(positions.isEmpty());
    }

    SECTION("zero world-per-paper-mm -> zero spacing") {
        const QVector<QPointF> points = {QPointF(0.0, 0.0), QPointF(10.0, 0.0)};
        const cwStrokePath strokePath(points);
        const cwPlacementContext context{strokePath, layer, 0.0, nullptr};
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
    const cwPlacementContext context{strokePath, layer, 1.0, nullptr};

    QVector<cwStampPosition> positions;
    rule(QStringLiteral("Explicit point"))->apply(positions, context);

    REQUIRE(positions.size() == 1);
    checkPoint(positions.first().anchorWorld, QPointF(2.0, 3.0)); // arclength 0
    CHECK(positions.first().glyphName == QStringLiteral("g"));

    SECTION("degenerate stroke seeds nothing") {
        const QVector<QPointF> single = {QPointF(0.0, 0.0)};
        const cwStrokePath empty(single);
        const cwPlacementContext emptyContext{empty, layer, 1.0, nullptr};
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
    const cwPlacementContext context{strokePath, layer, 1.0, nullptr};

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
    const cwPlacementContext context{strokePath, layer, 1.0, nullptr};

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

    const QPainterPath placed =
        rule(QStringLiteral("Rigid stamp"))
            ->stampPath(position, cwStampGeometry{unitSegmentGlyph(), strokePath});

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
    const cwPlacementContext context{strokePath, layer, 1.0, nullptr};

    QVector<cwStampPosition> positions(2);
    rule(QStringLiteral("Rigid stamp"))->apply(positions, context);

    CHECK(positions.at(0).priority == 7);
    CHECK(positions.at(1).priority == 7);
}

TEST_CASE("Bending stamp warps the glyph along the stroke arclength", "[cwPlacementRule]")
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

    const QPainterPath warped =
        rule(QStringLiteral("Bending stamp"))
            ->stampPath(position, cwStampGeometry{glyph, strokePath});

    const QVector<QPointF> got = elementPoints(warped);
    REQUIRE(got.size() == 3);
    checkPoint(got.at(0), QPointF(4.0, 0.0)); // (0,0) -> arclength 4
    checkPoint(got.at(1), QPointF(6.0, 0.0)); // (2,0) -> arclength 6
    checkPoint(got.at(2), QPointF(4.0, 1.0)); // (0,1) -> arclength 4, +1 along normal
}

TEST_CASE("Trace offset polyline returns the stroke and seeds no stamps", "[cwPlacementRule]")
{
    const QVector<QPointF> strokeWorld = {QPointF(0.0, 0.0), QPointF(1.0, 1.0), QPointF(2.0, 0.0)};
    const cwStrokePath strokePath(strokeWorld);

    cwDecorationLayer layer;
    const cwPlacementContext context{strokePath, layer, 1.0, nullptr};

    const cwPlacementRule *trace = rule(QStringLiteral("Trace offset polyline"));

    // Iter 1: offset 0 -> the traced polyline is the stroke itself.
    const QVector<QPolygonF> polylines = trace->tracePolylines(strokeWorld, context);
    REQUIRE(polylines.size() == 1);
    CHECK(polylines.first() == QPolygonF(strokeWorld));

    // It does not seed the per-stamp pipeline.
    QVector<cwStampPosition> positions;
    trace->apply(positions, context);
    CHECK(positions.isEmpty());
}
