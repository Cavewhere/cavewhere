//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

// Std
#include <algorithm>
#include <functional>
#include <numeric>

// Qt
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QMatrix4x4>
#include <QVector3D>
#include <QRay3D>
#include <QScopeGuard>

// SUT
#include "cwBaseTurnTableInteraction.h"
#include "cwGeometryItersecter.h"
#include "cwPickingLog.h"
#include "cwPickQuery.h"

using namespace Catch;

namespace {

// A point cloud registered the way cwRenderPointCloud does: one Object holding
// every point, Type::Points, no indices, with the pick radius that pads each
// point's BVH box (cwRenderPointCloud.cpp uses 0.5 * meanSpacingXY).
cwGeometryItersecter::Object makePointCloudObject(uint64_t id,
                                                  const QVector<QVector3D>& points,
                                                  float pickRadius = 0.5f)
{
    cwGeometry geometry {
        {cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3}
    };
    geometry.set(cwGeometry::Semantic::Position, points);
    geometry.setType(cwGeometry::Type::Points);

    return cwGeometryItersecter::Object({nullptr, id}, geometry, QMatrix4x4(),
                                        pickRadius);
}

// A polyline registered the way cwRenderLinePlot does: non-indexed consecutive
// pairs with a synthesized iota index list.
cwGeometryItersecter::Object makeLineObject(uint64_t id,
                                            const QVector<QVector3D>& points)
{
    cwGeometry geometry {
        {cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3}
    };
    geometry.set(cwGeometry::Semantic::Position, points);

    QVector<uint32_t> indices(points.size());
    std::iota(indices.begin(), indices.end(), 0u);
    geometry.setIndices(std::move(indices));
    geometry.setType(cwGeometry::Type::Lines);

    return cwGeometryItersecter::Object({nullptr, id}, geometry);
}

cwGeometryItersecter::Object makeTriangleObject(uint64_t id,
                                                const QVector3D& a,
                                                const QVector3D& b,
                                                const QVector3D& c)
{
    cwGeometry geometry {
        {cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3}
    };
    geometry.set(cwGeometry::Semantic::Position, QVector<QVector3D>{a, b, c});
    geometry.setIndices(QVector<uint32_t>{0u, 1u, 2u});
    geometry.setType(cwGeometry::Type::Triangles);

    return cwGeometryItersecter::Object({nullptr, id}, geometry, QMatrix4x4());
}

// A constant (ortho-style) tolerance: a fixed world-space anchor radius.
cwPickQuery ortho(double radius)
{
    cwPickQuery query;
    query.tolerance.constant = radius;
    return query;
}

// A 3x3x2 grid of 18 points (> the 16-primitive leaf cap) centered on
// `center`, spaced 2 units apart.
QVector<QVector3D> cluster18(const QVector3D& center)
{
    QVector<QVector3D> points;
    for (int zi = 0; zi < 2; ++zi) {
        for (int yi = -1; yi <= 1; ++yi) {
            for (int xi = -1; xi <= 1; ++xi) {
                points.append(center + QVector3D(2.0f * xi, 2.0f * yi, 2.0f * zi));
            }
        }
    }
    return points;
}

// A vertical -Z ray over (x, y), starting above the test geometry.
QRay3D rayDownAt(float x, float y)
{
    return QRay3D(QVector3D(x, y, 100.0f), QVector3D(0.0f, 0.0f, -1.0f));
}

} // namespace

// The anchor snaps to the actual data point — the "rotate around a point
// cloud point" ask of issue #562 — and respects the tolerance radius: inside
// anchors, outside returns nothing so the caller keeps the current pivot.
TEST_CASE("nearestGeometryPoint: anchors the exact cloud point within tolerance",
          "[cwGeometryItersecter][nearestPoint]")
{
    const QVector3D point(10.0f, 20.0f, -5.0f);

    cwGeometryItersecter intersecter;
    intersecter.addObject(makePointCloudObject(1, {point}));
    intersecter.waitForFinish();

    // Ray passes 2 units from the point in y.
    const QRay3D ray = rayDownAt(10.0f, 22.0f);

    SECTION("inside the radius returns the point itself") {
        const std::optional<QVector3D> anchor =
            intersecter.nearestGeometryPoint(ray, ortho(5.0));
        REQUIRE(anchor.has_value());
        CHECK(anchor->x() == Approx(point.x()));
        CHECK(anchor->y() == Approx(point.y()));
        CHECK(anchor->z() == Approx(point.z()));
    }

    SECTION("outside the radius anchors nothing") {
        CHECK_FALSE(intersecter.nearestGeometryPoint(ray, ortho(1.0)).has_value());
    }

    SECTION("a disabled tolerance anchors nothing") {
        CHECK_FALSE(intersecter.nearestGeometryPoint(ray, cwPickQuery{}).has_value());
    }
}

// The two entry points share one traversal (traverseBvh) but deliberately
// disagree about which kinds consult the screen-space tolerance, so the same
// scene, ray and query must answer differently on each. This is the assertion
// that fails if the split is ever collapsed:
//
//   - Widen the exact pick to accept points at the tolerance radius and the
//     off-ray snap deleted in d28581a0 comes back — picks landing on points
//     the ray never touched, which made leads silently unclickable (see
//     test_cwLeadView_occlusion).
//   - Narrow the anchor's reach to lines and the pivot stops finding cloud
//     points at all (verified: dropping Points from
//     AnchorPick::ToleranceKinds un-pads the point nodes and this test's
//     off-ray section fails).
//
// See cwPickTolerance, which documents the split this pins.
TEST_CASE("Points: the tolerance widens the anchor but never the exact pick",
          "[cwGeometryItersecter][nearestPoint]")
{
    const QVector3D point(10.0f, 20.0f, -5.0f);

    cwGeometryItersecter intersecter;
    intersecter.addObject(makePointCloudObject(1, {point}));
    intersecter.waitForFinish();

    // A 5-unit tolerance the point sits well inside, while staying 2 units
    // outside its own 0.5 pickRadius sphere.
    const cwPickQuery query = ortho(5.0);

    SECTION("off-ray: only the anchor reaches it") {
        const QRay3D ray = rayDownAt(10.0f, 22.0f);
        CHECK_FALSE(intersecter.intersectsDetailed(ray, query).hit());
        CHECK(intersecter.nearestGeometryPoint(ray, query).has_value());
    }

    SECTION("head-on: the ray touches the sphere, so both reach it") {
        const QRay3D ray = rayDownAt(point.x(), point.y());
        CHECK(intersecter.intersectsDetailed(ray, query).hit());
        CHECK(intersecter.nearestGeometryPoint(ray, query).has_value());
    }
}

// Lines are the one kind in BOTH policies' ToleranceKinds, so an off-ray
// segment inside the radius is reachable from either entry point. The mirror
// of the points case above: it fails if lines ever leave either set.
TEST_CASE("Lines: the tolerance widens the anchor and the exact pick alike",
          "[cwGeometryItersecter][nearestPoint]")
{
    cwGeometryItersecter intersecter;
    intersecter.addObject(makeLineObject(1, {QVector3D(10.0f, 20.0f, 0.0f),
                                             QVector3D(10.0f, 20.0f, -10.0f)}));
    intersecter.waitForFinish();

    // 2 units off the segment in y — no ray ever exactly intersects a line,
    // so the tolerance is the only thing that can accept it.
    const QRay3D ray = rayDownAt(10.0f, 22.0f);

    SECTION("inside the radius both reach it") {
        CHECK(intersecter.intersectsDetailed(ray, ortho(5.0)).hit());
        CHECK(intersecter.nearestGeometryPoint(ray, ortho(5.0)).has_value());
    }

    SECTION("outside the radius neither does") {
        CHECK_FALSE(intersecter.intersectsDetailed(ray, ortho(1.0)).hit());
        CHECK_FALSE(intersecter.nearestGeometryPoint(ray, ortho(1.0)).has_value());
    }
}

// Among candidates inside the tolerance the front-most wins — the same
// nearest-by-depth rule as every other pick — even when a deeper candidate
// passes closer to the ray.
TEST_CASE("nearestGeometryPoint: the front-most in-tolerance candidate wins",
          "[cwGeometryItersecter][nearestPoint]")
{
    const QVector3D nearPoint(0.0f, 2.0f, 40.0f);   // 2 off-ray, depth 60
    const QVector3D farPoint(0.0f, 1.0f, -40.0f);   // 1 off-ray, depth 140

    // Both insertion orders: with only one, a "first candidate found wins"
    // implementation — i.e. dropping the depth prune in considerCandidate —
    // passes and the pivot silently lands on the far side of the cave.
    const auto anchorZ = [&](bool nearFirst) {
        cwGeometryItersecter intersecter;
        if (nearFirst) {
            intersecter.addObject(makePointCloudObject(1, {nearPoint}));
            intersecter.addObject(makePointCloudObject(2, {farPoint}));
        } else {
            intersecter.addObject(makePointCloudObject(1, {farPoint}));
            intersecter.addObject(makePointCloudObject(2, {nearPoint}));
        }
        intersecter.waitForFinish();

        const std::optional<QVector3D> anchor =
            intersecter.nearestGeometryPoint(rayDownAt(0.0f, 0.0f), ortho(5.0));
        REQUIRE(anchor.has_value());
        return anchor->z();
    };

    CHECK(anchorZ(true) == Approx(nearPoint.z()));
    CHECK(anchorZ(false) == Approx(nearPoint.z()));  // near added last — still wins
}

// A near-miss beside the centerline anchors ONTO the line — the closest point
// of the closest segment, not a segment midpoint or a box center.
TEST_CASE("nearestGeometryPoint: snaps onto the nearest spot of a line segment",
          "[cwGeometryItersecter][nearestPoint]")
{
    cwGeometryItersecter intersecter;
    intersecter.addObject(makeLineObject(1, {
        QVector3D(-20.0f, 0.0f, 10.0f),
        QVector3D( 20.0f, 0.0f, 10.0f),
    }));
    intersecter.waitForFinish();

    // 3 units to the side of the line at x=15.
    const std::optional<QVector3D> anchor =
        intersecter.nearestGeometryPoint(rayDownAt(15.0f, 3.0f), ortho(5.0));
    REQUIRE(anchor.has_value());
    CHECK(anchor->x() == Approx(15.0f).margin(1e-3));
    CHECK(anchor->y() == Approx(0.0f).margin(1e-3));
    CHECK(anchor->z() == Approx(10.0f).margin(1e-3));
}

// Triangles anchor at full tolerance reach, unlike the exact pick: a ray
// through the interior anchors the intersection, a near-miss beside the
// triangle anchors the nearest boundary point, and a far miss anchors nothing.
TEST_CASE("nearestGeometryPoint: triangle anchors on the exact hit or the nearest edge",
          "[cwGeometryItersecter][nearestPoint]")
{
    // Right triangle in the z=10 plane with its long edge on y=0, x in [-10, 10].
    cwGeometryItersecter intersecter;
    intersecter.addObject(makeTriangleObject(1,
        QVector3D(-10.0f, 0.0f, 10.0f),
        QVector3D( 10.0f, 0.0f, 10.0f),
        QVector3D(  0.0f, 10.0f, 10.0f)));
    intersecter.waitForFinish();

    SECTION("interior hit anchors the intersection point") {
        const std::optional<QVector3D> anchor =
            intersecter.nearestGeometryPoint(rayDownAt(0.0f, 2.0f), ortho(5.0));
        REQUIRE(anchor.has_value());
        CHECK(anchor->x() == Approx(0.0f).margin(1e-3));
        CHECK(anchor->y() == Approx(2.0f).margin(1e-3));
        CHECK(anchor->z() == Approx(10.0f).margin(1e-3));
    }

    SECTION("near-miss beside the edge anchors the closest edge point") {
        // 3 units outside the y=0 edge at x=5.
        const std::optional<QVector3D> anchor =
            intersecter.nearestGeometryPoint(rayDownAt(5.0f, -3.0f), ortho(5.0));
        REQUIRE(anchor.has_value());
        CHECK(anchor->x() == Approx(5.0f).margin(1e-3));
        CHECK(anchor->y() == Approx(0.0f).margin(1e-3));
        CHECK(anchor->z() == Approx(10.0f).margin(1e-3));
    }

    SECTION("far miss anchors nothing") {
        CHECK_FALSE(intersecter.nearestGeometryPoint(
            rayDownAt(5.0f, -8.0f), ortho(5.0)).has_value());
    }
}

// The point-cloud regression behind issue #562's follow-up: a whole cloud is
// ONE Object, so any node-center scheme could resolve a gap click to a
// far-away aggregate centroid at the wrong depth. The nearest-point anchor has
// no such failure mode: a click down the gap between two far-apart clusters
// finds nothing within tolerance and the caller keeps the current pivot.
TEST_CASE("nearestGeometryPoint: a gap ray through a large point cloud anchors nothing",
          "[cwGeometryItersecter][nearestPoint]")
{
    QVector<QVector3D> points = cluster18(QVector3D(-50.0f, -50.0f, 40.0f));
    points += cluster18(QVector3D(50.0f, 50.0f, -40.0f));

    cwGeometryItersecter intersecter;
    intersecter.addObject(makePointCloudObject(1, points));
    intersecter.waitForFinish();

    // Straight down the middle of the gap: ~70 units from either cluster.
    CHECK_FALSE(intersecter.nearestGeometryPoint(
        rayDownAt(0.0f, 0.0f), ortho(5.0)).has_value());

    // The same ray with a radius that reaches the clusters anchors a real
    // point of the nearer cluster, never a centroid in the gap. The near
    // cluster's points sit at z = 40 and z = 42, so front-most pins z to
    // exactly 42: a whole-cloud centroid (z ~ 1), the cluster's own box
    // centre (z = 41), and any interpolated spot in the gap all fail. The
    // x/y bounds pin it into the cluster rather than the gap the ray flies
    // down — the exact (x, y) among the nine z = 42 points is a traversal-
    // order tie, so bound it instead of over-fitting to one.
    const std::optional<QVector3D> anchor =
        intersecter.nearestGeometryPoint(rayDownAt(0.0f, 0.0f), ortho(100.0));
    REQUIRE(anchor.has_value());
    CHECK(anchor->z() == Approx(42.0f).margin(1e-3));
    CHECK(anchor->x() <= -48.0f);
    CHECK(anchor->x() >= -52.0f);
    CHECK(anchor->y() <= -48.0f);
    CHECK(anchor->y() >= -52.0f);
}

// The depth prune must not drop a node just because the ray starts inside it.
// QBox3D::intersection() returns the ray's ENTRY parameter only while that is
// positive; once the origin is inside the box it returns the EXIT parameter
// instead. The prune reads that value as a lower bound on every candidate in
// the subtree, so an origin-inside node is compared on its far side and can be
// skipped while holding the nearest candidate. The tolerance pad makes this
// reachable: it inflates every box by the accept radius at the scene's far
// depth, which routinely swallows the camera.
TEST_CASE("nearestGeometryPoint: a node containing the ray origin is not pruned away",
          "[cwGeometryItersecter][nearestPoint]")
{
    // The line's own box spans nearly the whole scene, so padding it swallows
    // the ray origin at z = 100 and its exit parameter is ~210 — far behind
    // the point cloud's t = 50.
    const QVector3D nearSpot(0.0f, 1.0f, 95.0f);   // 1 off-ray, depth 5
    const QVector3D farPoint(0.0f, 4.0f, 50.0f);   // 4 off-ray, depth 50

    cwGeometryItersecter intersecter;
    intersecter.addObject(makePointCloudObject(1, {farPoint}));
    intersecter.addObject(makeLineObject(2, {nearSpot, QVector3D(0.0f, 1.0f, -100.0f)}));
    intersecter.waitForFinish();

    // Both are inside the radius; the line's near end is 10x nearer the
    // camera, so it must win.
    const std::optional<QVector3D> anchor =
        intersecter.nearestGeometryPoint(rayDownAt(0.0f, 0.0f), ortho(10.0));
    REQUIRE(anchor.has_value());
    CHECK(anchor->z() == Approx(nearSpot.z()).margin(1e-3));
    CHECK(anchor->y() == Approx(nearSpot.y()).margin(1e-3));
}

// A perspective-style tolerance (radius grows with depth) accepts by the
// radius at the candidate's depth.
TEST_CASE("nearestGeometryPoint: perspective tolerance scales with depth",
          "[cwGeometryItersecter][nearestPoint]")
{
    const QVector3D point(0.0f, 2.0f, -200.0f);  // 2 off-ray at depth 300

    cwGeometryItersecter intersecter;
    intersecter.addObject(makePointCloudObject(1, {point}));
    intersecter.waitForFinish();

    const auto slopeQuery = [](double slope) {
        cwPickQuery query;
        query.tolerance.slope = slope;
        return query;
    };

    // radiusAt(300) = 3 > 2 → anchors; radiusAt(300) = 1.5 < 2 → nothing.
    CHECK(intersecter.nearestGeometryPoint(
        rayDownAt(0.0f, 0.0f), slopeQuery(0.01)).has_value());
    CHECK_FALSE(intersecter.nearestGeometryPoint(
        rayDownAt(0.0f, 0.0f), slopeQuery(0.005)).has_value());
}

// The anchor's BVH has to prune, not scan. This is a performance contract with
// no behavioural signature to test instead: the scene-global tolerance pad this
// query originally shipped with returned exactly the same answers, but padded
// every node by the accept radius at the far corner of the WHOLE scene. Near
// nodes were then inflated by a radius belonging to the far end of the cave,
// the ray crossed nearly every leaf, and the query degenerated into a linear
// scan — measured at ~40x slower on a million points, growing linearly with
// cloud size while the exact pick stayed flat.
//
// Absolute timings mean nothing across machines, build configs, or a CI box
// running several test processes at once, so this measures the anchor against
// intersectsDetailed on the SAME cloud in the SAME run. The exact pick is the
// control: its BVH is known to prune, so the ratio divides out machine speed
// and load. Measured on this cloud: ~1.3x with a per-node pad, ~34x with the
// scene-global one. The 10x threshold sits well clear of both.
TEST_CASE("nearestGeometryPoint: the anchor prunes instead of scanning the whole cloud",
          "[cwGeometryItersecter][nearestPoint][performance]")
{
    // Pin cw.picking debug OFF for the duration. Only intersectsDetailed dumps
    // per-primitive debug output — nearestGeometryPoint has no such branch — so
    // an inherited cw.picking.debug=true (QT_LOGGING_RULES, or a sibling test
    // that leaked one) inflates the exact-pick baseline alone and silently
    // turns the ratio below into a free pass.
    QLoggingCategory& category = const_cast<QLoggingCategory&>(lcPick());
    const bool wasEnabled = category.isDebugEnabled();
    category.setEnabled(QtDebugMsg, false);
    auto restoreCategory = qScopeGuard([&category, wasEnabled]() {
        category.setEnabled(QtDebugMsg, wasEnabled);
    });

    // A cloud with a wide DEPTH spread — that's what the scene-global pad
    // over-inflates (the blow-up factor is farDepth/nodeDepth, so it needs
    // depth range, not distance). An empty tube down the z axis keeps the
    // exact pick missing, which is the only path that reaches the anchor.
    constexpr int kSide = 63;             // ~239k points once the tube is cut
    constexpr float kVoidRadius = 12.0f;
    constexpr float kSpacingXY = 100.0f / float(kSide);

    QVector<QVector3D> points;
    points.reserve(kSide * kSide * kSide);
    for (int zi = 0; zi < kSide; ++zi) {
        const float z = -500.0f + 500.0f * (float(zi) / float(kSide));
        for (int yi = 0; yi < kSide; ++yi) {
            const float y = -50.0f + 100.0f * (float(yi) / float(kSide));
            for (int xi = 0; xi < kSide; ++xi) {
                const float x = -50.0f + 100.0f * (float(xi) / float(kSide));
                if (x * x + y * y < kVoidRadius * kVoidRadius) {
                    continue;
                }
                points.append(QVector3D(x, y, z));
            }
        }
    }

    cwGeometryItersecter intersecter;
    // cwRenderPointCloud registers a cloud as ONE object with 0.5*meanSpacingXY.
    intersecter.addObject(makePointCloudObject(1, points, 0.5f * kSpacingXY));
    intersecter.waitForFinish();

    // Perspective tolerances exactly as cwCamera::pickQuery derives them:
    // slope = pixelRadius * 2 / (p11 * viewportHeight), with the 96dpi headless
    // fallback (3.78 px/mm), p11 = 1.921 for a 55 degree fov, 1000px viewport.
    const auto perspectiveQuery = [](double millimeters) {
        const double pixelRadius = millimeters * (96.0 / 25.4);
        cwPickQuery query;
        query.tolerance.slope = pixelRadius * 2.0 / (1.921 * 1000.0);
        return query;
    };
    // 4.0 is kPivotPickRadiusMillimeters, which is anonymous-namespace in
    // cwBaseTurnTableInteraction.cpp and so unavailable here; the anchor radius
    // is public, so read it rather than restate it.
    const cwPickQuery exact = perspectiveQuery(4.0);
    const cwPickQuery anchor =
        perspectiveQuery(cwBaseTurnTableInteraction::PivotAnchorRadiusMillimeters);

    // Down the empty tube from the near end: the exact pick finds nothing, the
    // anchor reaches the tube wall once radiusAt(t) grows past kVoidRadius.
    const QRay3D ray(QVector3D(0.0f, 0.0f, 50.0f), QVector3D(0.0f, 0.0f, -1.0f));

    // Preconditions — without these the timings below would be measuring the
    // wrong thing and the test would pass vacuously.
    REQUIRE(points.size() > 200000);
    REQUIRE_FALSE(intersecter.intersectsDetailed(ray, exact).hit());
    REQUIRE(intersecter.nearestGeometryPoint(ray, anchor).has_value());

    const auto medianMs = [](const std::function<void()>& work) {
        constexpr int kRuns = 5;
        QList<double> samples;
        for (int i = 0; i < kRuns; ++i) {
            QElapsedTimer timer;
            timer.start();
            work();
            samples.append(double(timer.nsecsElapsed()) / 1e6);
        }
        std::sort(samples.begin(), samples.end());
        return samples.at(samples.size() / 2);
    };

    const double exactMs = medianMs([&]{ intersecter.intersectsDetailed(ray, exact); });
    const double anchorMs = medianMs([&]{ intersecter.nearestGeometryPoint(ray, anchor); });

    // Guard against a degenerate control: if the exact pick is too fast to
    // time, the ratio is noise rather than signal.
    REQUIRE(exactMs > 0.0);

    INFO("exact " << exactMs << " ms, anchor " << anchorMs << " ms, ratio "
         << (anchorMs / exactMs) << "x over " << points.size() << " points");
    CHECK(anchorMs < exactMs * 10.0);
}
