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
#include "cwStrokePath.h"

// Qt
#include <QPointF>
#include <QVector>

// Std
#include <cmath>

using Catch::Approx;

// Unit tests for cwStrokePath: the world-metre arclength sampler + nearest-point
// projector that the placement rules and the layout share. Each stroke's point
// vector is declared local to the test and outlives the cwStrokePath — the
// sampler borrows it by const reference.

namespace {

void checkPoint(const QPointF &actual, const QPointF &expected)
{
    CHECK(actual.x() == Approx(expected.x()).margin(1e-9));
    CHECK(actual.y() == Approx(expected.y()).margin(1e-9));
}

} // namespace

TEST_CASE("cwStrokePath reports total length and emptiness", "[cwStrokePath]")
{
    SECTION("a two-point polyline is not empty and measures its segment") {
        const QVector<QPointF> points = {QPointF(0.0, 0.0), QPointF(3.0, 4.0)};
        const cwStrokePath path(points);
        CHECK_FALSE(path.isEmpty());
        CHECK(path.totalLength() == Approx(5.0)); // 3-4-5
    }

    SECTION("total length sums every segment") {
        // (0,0)->(0,3)->(4,3): 3 + 4 = 7.
        const QVector<QPointF> points = {QPointF(0.0, 0.0), QPointF(0.0, 3.0), QPointF(4.0, 3.0)};
        const cwStrokePath path(points);
        CHECK(path.totalLength() == Approx(7.0));
    }

    SECTION("a single point is empty with zero length") {
        const QVector<QPointF> points = {QPointF(2.0, 2.0)};
        const cwStrokePath path(points);
        CHECK(path.isEmpty());
        CHECK(path.totalLength() == Approx(0.0));
    }

    SECTION("no points is empty with zero length") {
        const QVector<QPointF> points;
        const cwStrokePath path(points);
        CHECK(path.isEmpty());
        CHECK(path.totalLength() == Approx(0.0));
    }
}

TEST_CASE("cwStrokePath samples points along arclength", "[cwStrokePath]")
{
    const QVector<QPointF> points = {QPointF(0.0, 0.0), QPointF(10.0, 0.0)};
    const cwStrokePath path(points);

    checkPoint(path.pointAtArcLength(0.0), QPointF(0.0, 0.0));
    checkPoint(path.pointAtArcLength(2.5), QPointF(2.5, 0.0));
    checkPoint(path.pointAtArcLength(10.0), QPointF(10.0, 0.0));

    SECTION("samples cross a segment boundary") {
        // (0,0)->(0,4)->(6,4): arclength 4 is the corner, 7 is 3 m along leg 2.
        const QVector<QPointF> bent = {QPointF(0.0, 0.0), QPointF(0.0, 4.0), QPointF(6.0, 4.0)};
        const cwStrokePath bentPath(bent);
        checkPoint(bentPath.pointAtArcLength(4.0), QPointF(0.0, 4.0));
        checkPoint(bentPath.pointAtArcLength(7.0), QPointF(3.0, 4.0));
    }
}

TEST_CASE("cwStrokePath clamps arclength samples to the stroke ends", "[cwStrokePath]")
{
    const QVector<QPointF> points = {QPointF(1.0, 1.0), QPointF(11.0, 1.0)}; // length 10
    const cwStrokePath path(points);

    checkPoint(path.pointAtArcLength(-5.0), QPointF(1.0, 1.0));   // below 0 -> start
    checkPoint(path.pointAtArcLength(100.0), QPointF(11.0, 1.0)); // above length -> end
}

TEST_CASE("cwStrokePath degenerate strokes sample defensively", "[cwStrokePath]")
{
    SECTION("single point returns that point for any arclength") {
        const QVector<QPointF> points = {QPointF(2.0, 3.0)};
        const cwStrokePath path(points);
        checkPoint(path.pointAtArcLength(0.0), QPointF(2.0, 3.0));
        checkPoint(path.pointAtArcLength(5.0), QPointF(2.0, 3.0));
    }

    SECTION("no points returns the origin") {
        const QVector<QPointF> points;
        const cwStrokePath path(points);
        checkPoint(path.pointAtArcLength(1.0), QPointF(0.0, 0.0));
    }
}

TEST_CASE("cwStrokePath yields unit tangents and left normals", "[cwStrokePath]")
{
    // (0,0)->(10,0)->(10,10): horizontal leg then vertical leg.
    const QVector<QPointF> points = {QPointF(0.0, 0.0), QPointF(10.0, 0.0), QPointF(10.0, 10.0)};
    const cwStrokePath path(points);

    SECTION("tangent follows the local direction of travel") {
        checkPoint(path.tangentAtArcLength(5.0), QPointF(1.0, 0.0));   // horizontal leg -> +X
        checkPoint(path.tangentAtArcLength(15.0), QPointF(0.0, 1.0));  // vertical leg -> +Y
    }

    SECTION("normal is the tangent rotated +90 deg (left normal)") {
        checkPoint(path.normalAtArcLength(5.0), QPointF(0.0, 1.0));    // +X -> +Y
        checkPoint(path.normalAtArcLength(15.0), QPointF(-1.0, 0.0));  // +Y -> -X
    }

    SECTION("tangents are unit length on a diagonal") {
        const QVector<QPointF> diagonal = {QPointF(0.0, 0.0), QPointF(3.0, 4.0)};
        const cwStrokePath diagonalPath(diagonal);
        const QPointF tangent = diagonalPath.tangentAtArcLength(2.5);
        checkPoint(tangent, QPointF(0.6, 0.8));
        CHECK(std::hypot(tangent.x(), tangent.y()) == Approx(1.0));
    }
}

TEST_CASE("cwStrokePath degenerate strokes report a default tangent and normal",
          "[cwStrokePath]")
{
    const QVector<QPointF> points = {QPointF(4.0, 4.0)};
    const cwStrokePath path(points);

    checkPoint(path.tangentAtArcLength(0.0), QPointF(1.0, 0.0)); // default tangent
    checkPoint(path.normalAtArcLength(0.0), QPointF(0.0, 1.0));  // left normal of (1,0)
}

TEST_CASE("cwStrokePath projects nearby world points onto the stroke", "[cwStrokePath]")
{
    // (0,0)->(10,0)->(10,10): an L. Probes sit off each leg.
    const QVector<QPointF> points = {QPointF(0.0, 0.0), QPointF(10.0, 0.0), QPointF(10.0, 10.0)};
    const cwStrokePath path(points);

    SECTION("tangent near a point uses the closest segment") {
        checkPoint(path.tangentNearPoint(QPointF(3.0, 0.5)), QPointF(1.0, 0.0));  // near horizontal leg
        checkPoint(path.tangentNearPoint(QPointF(10.5, 7.0)), QPointF(0.0, 1.0)); // near vertical leg
    }

    SECTION("arclength near a point recovers the projected distance") {
        // Off the horizontal leg at x=4: projects to arclength 4.
        CHECK(path.arcLengthNearPoint(QPointF(4.0, 2.0)) == Approx(4.0));
        // Off the vertical leg 3 m up from the corner: 10 (leg 1) + 3 = 13.
        CHECK(path.arcLengthNearPoint(QPointF(12.0, 3.0)) == Approx(13.0));
    }

    SECTION("a point past the end clamps to the final vertex") {
        // Beyond the vertical leg's top -> projects to its end, arclength 20.
        CHECK(path.arcLengthNearPoint(QPointF(10.0, 50.0)) == Approx(20.0));
    }
}

TEST_CASE("cwStrokePath round-trips arclength through projection", "[cwStrokePath]")
{
    const QVector<QPointF> points = {QPointF(0.0, 0.0), QPointF(0.0, 4.0), QPointF(6.0, 4.0)};
    const cwStrokePath path(points);

    // A point sampled at a known arclength projects back to that arclength.
    const double arc = 7.0;
    const QPointF sampled = path.pointAtArcLength(arc);
    CHECK(path.arcLengthNearPoint(sampled) == Approx(arc));
}

TEST_CASE("cwStrokePath degenerate strokes project defensively", "[cwStrokePath]")
{
    const QVector<QPointF> points = {QPointF(5.0, 5.0)};
    const cwStrokePath path(points);

    checkPoint(path.tangentNearPoint(QPointF(9.0, 9.0)), QPointF(1.0, 0.0)); // default tangent
    CHECK(path.arcLengthNearPoint(QPointF(9.0, 9.0)) == Approx(0.0));
}
