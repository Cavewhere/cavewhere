//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

// Std
#include <algorithm>
#include <cmath>
#include <functional>
#include <numeric>

// Qt
#include <QElapsedTimer>
#include <QList>
#include <QMatrix4x4>
#include <QVector3D>
#include <QRay3D>
#include <QLoggingCategory>
#include <QScopeGuard>

// SUT
#include "cwGeometryItersecter.h"
#include "cwPickingLog.h"
#include "cwPickQuery.h"
#include "cwRayHit.h"

using namespace Catch;

namespace {

// A polyline registered exactly the way cwRenderLinePlot::setGeometry does:
// non-indexed consecutive pairs with a synthesized iota index list, so each
// segment's first index equals its from-vertex index. Picks are tested at
// large world coordinates (~10^3) where float cancellation bites, matching
// the regime the double-precision ray-segment solver is built for.
cwGeometryItersecter::Object makeLineObject(uint64_t id,
                                            const QVector<QVector3D>& points,
                                            const QMatrix4x4& modelMatrix = QMatrix4x4())
{
    cwGeometry geometry {
        {cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3}
    };
    geometry.set(cwGeometry::Semantic::Position, points);

    QVector<uint32_t> indices(points.size());
    std::iota(indices.begin(), indices.end(), 0u);
    geometry.setIndices(std::move(indices));
    geometry.setType(cwGeometry::Type::Lines);

    return cwGeometryItersecter::Object({nullptr, id}, geometry, modelMatrix);
}

cwGeometryItersecter::Object makeTriangleObject(uint64_t id,
                                                const QVector3D& a,
                                                const QVector3D& b,
                                                const QVector3D& c)
{
    QVector<QVector3D> points;
    points << a << b << c;

    QVector<uint32_t> indices;
    indices << 0u << 1u << 2u;

    cwGeometry geometry {
        {cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3}
    };
    geometry.set(cwGeometry::Semantic::Position, points);
    geometry.setIndices(indices);
    geometry.setType(cwGeometry::Type::Triangles);
    geometry.setCullBackfaces(false); // Double-sided, like scrap carpets.

    return cwGeometryItersecter::Object({nullptr, id}, geometry, QMatrix4x4());
}

// A constant (ortho-style) tolerance: a fixed world-space pick radius.
cwPickQuery ortho(double radius, cwPickQuery::Kinds kinds = cwPickQuery::All)
{
    cwPickQuery query;
    query.tolerance.constant = radius;
    query.kinds = kinds;
    return query;
}

// A perspective-style tolerance: pick radius grows linearly with depth.
cwPickQuery perspective(double slope, cwPickQuery::Kinds kinds = cwPickQuery::All)
{
    cwPickQuery query;
    query.tolerance.slope = slope;
    query.kinds = kinds;
    return query;
}

// Surfaces = triangles | points (the measurement free-point fallback).
constexpr cwPickQuery::Kinds kSurfaces =
    cwPickQuery::Kind::Triangles | cwPickQuery::Kind::Points;

} // namespace

// A single segment along +X at large coordinates; a vertical -Z ray crosses
// over it offset by a known perpendicular distance d. With an ortho (fixed)
// tolerance R the segment is hit exactly when d <= R, and the snapped
// pointWorld lands on the segment.
TEST_CASE("Line pick: hit inside the radius, miss outside",
          "[cwGeometryItersecter][linePick]")
{
    const QVector3D a(1000.0f, 2000.0f, -500.0f);
    const QVector3D b(1020.0f, 2000.0f, -500.0f);

    cwGeometryItersecter intersector;
    intersector.addObject(makeLineObject(1, {a, b}));
    intersector.waitForFinish();

    const float xMid = 1010.0f;
    const float depth = 300.0f;
    const QVector3D dir(0.0f, 0.0f, -1.0f);

    const auto rayAtOffset = [&](float d) {
        return QRay3D(QVector3D(xMid, 2000.0f + d, -500.0f + depth), dir);
    };

    const cwPickQuery query = ortho(0.5);

    SECTION("inside the radius hits and snaps onto the segment") {
        const cwRayHit hit = intersector.intersectsDetailed(rayAtOffset(0.3f), query);
        REQUIRE(hit.hit());
        CHECK(hit.objectId() == 1u);
        CHECK(hit.tWorld() == Approx(depth).margin(1e-2));
        CHECK(hit.pointWorld().x() == Approx(xMid).margin(1e-3));
        CHECK(hit.pointWorld().y() == Approx(2000.0f).margin(1e-3));
        CHECK(hit.pointWorld().z() == Approx(-500.0f).margin(1e-3));
        // A line carries no parametric surface coordinate.
        CHECK(std::isnan(hit.u()));
        CHECK(std::isnan(hit.v()));
    }

    SECTION("outside the radius misses") {
        const cwRayHit hit = intersector.intersectsDetailed(rayAtOffset(0.8f), query);
        CHECK_FALSE(hit.hit());
    }

    SECTION("a ray straight through the segment hits at d=0") {
        const cwRayHit hit = intersector.intersectsDetailed(rayAtOffset(0.0f), query);
        REQUIRE(hit.hit());
        CHECK(hit.pointWorld().y() == Approx(2000.0f).margin(1e-3));
    }
}

// firstIndex must identify the hit segment. Under the iota index list it
// equals the segment's from-vertex index (2*segment), which Commit 2's
// station snap relies on.
TEST_CASE("Line pick: firstIndex maps to the hit segment",
          "[cwGeometryItersecter][linePick]")
{
    // Three independent shots, each its own from/to vertex pair (the line
    // plot lists shots non-indexed, so shot i owns vertices [2i, 2i+1]).
    // Shot 1 owns vertices 2,3 → segment index 1 → first index 2*1 = 2.
    QVector<QVector3D> pts {
        QVector3D(1000.0f, 2000.0f, -500.0f), QVector3D(1020.0f, 2000.0f, -500.0f), // shot 0
        QVector3D(1020.0f, 2040.0f, -500.0f), QVector3D(1060.0f, 2040.0f, -500.0f), // shot 1
        QVector3D(1060.0f, 2080.0f, -500.0f), QVector3D(1100.0f, 2080.0f, -500.0f)  // shot 2
    };

    cwGeometryItersecter intersector;
    intersector.addObject(makeLineObject(7, pts));
    intersector.waitForFinish();

    const cwPickQuery query = ortho(0.5);
    const QVector3D dir(0.0f, 0.0f, -1.0f);

    // Pick over the middle of shot 1: x in [1020,1060], y=2040.
    const QRay3D ray(QVector3D(1040.0f, 2040.0f, 0.0f), dir);
    const cwRayHit hit = intersector.intersectsDetailed(ray, query);

    REQUIRE(hit.hit());
    // Segment index 1 → first index 2*1 = 2 under the iota list.
    CHECK(hit.firstIndex() == 2);
    // The reported from-station is points[firstIndex].
    CHECK(pts.at(hit.firstIndex()).x() == Approx(1020.0f));
    CHECK(pts.at(hit.firstIndex()).y() == Approx(2040.0f));
    CHECK(hit.pointWorld().x() == Approx(1040.0f).margin(1e-3));
    CHECK(hit.pointWorld().y() == Approx(2040.0f).margin(1e-3));
}

// The pick radius is screen-space: a perspective tolerance grows with depth,
// so the same angular offset hits at any zoom and a wider angular offset
// misses at any zoom. A fixed world-space radius could not do both.
TEST_CASE("Line pick: screen-space radius is zoom invariant",
          "[cwGeometryItersecter][linePick]")
{
    const QVector3D a(1000.0f, 2000.0f, -500.0f);
    const QVector3D b(1040.0f, 2000.0f, -500.0f);

    cwGeometryItersecter intersector;
    intersector.addObject(makeLineObject(1, {a, b}));
    intersector.waitForFinish();

    const double slope = 0.01; // 1 world unit of radius per 100 of depth
    const cwPickQuery query = perspective(slope);
    const QVector3D dir(0.0f, 0.0f, -1.0f);
    const float xMid = 1020.0f;

    // radiusAt(depth) = slope*depth. Offset at ratio 0.5 of the radius hits;
    // ratio 1.5 misses — independent of depth.
    const auto pick = [&](float depth, double ratio) {
        const float d = float(ratio * slope * depth);
        const QRay3D ray(QVector3D(xMid, 2000.0f + d, -500.0f + depth), dir);
        return intersector.intersectsDetailed(ray, query);
    };

    for (float depth : {150.0f, 600.0f}) {
        INFO("depth=" << depth);
        CHECK(pick(depth, 0.5).hit());        // inside the cone at every depth
        CHECK_FALSE(pick(depth, 1.5).hit());  // outside the cone at every depth
    }
}

// Priority is a runtime kind filter. The same ray, with a line and a surface
// both crossing it, returns different hits per Kinds — never via a depth bias.
TEST_CASE("Line pick: kind filter selects lines vs surfaces",
          "[cwGeometryItersecter][linePick]")
{
    const QVector3D a(1000.0f, 2000.0f, -500.0f);
    const QVector3D b(1020.0f, 2000.0f, -500.0f);

    // A -Z ray piercing the segment at (1010,2000,-500), depth 300.
    const QRay3D ray(QVector3D(1010.0f, 2000.0f, -200.0f), QVector3D(0.0f, 0.0f, -1.0f));
    const double lineDepth = 300.0;

    SECTION("surface nearer than the line") {
        cwGeometryItersecter intersector;
        intersector.addObject(makeLineObject(1, {a, b}));
        // Triangle at z=-450 → depth 250, in front of the line.
        intersector.addObject(makeTriangleObject(2,
            QVector3D(1005.0f, 1995.0f, -450.0f),
            QVector3D(1015.0f, 1995.0f, -450.0f),
            QVector3D(1010.0f, 2005.0f, -450.0f)));
        intersector.waitForFinish();

        // All: nearest wins → the surface.
        const cwRayHit all = intersector.intersectsDetailed(ray, ortho(0.5));
        REQUIRE(all.hit());
        CHECK(all.objectId() == 2u);

        // Kind::Lines: surface skipped → the line, even though it's farther.
        const cwRayHit lines = intersector.intersectsDetailed(ray,
            ortho(0.5, cwPickQuery::Kind::Lines));
        REQUIRE(lines.hit());
        CHECK(lines.objectId() == 1u);
        CHECK(lines.tWorld() == Approx(lineDepth).margin(1e-2));

        // Surfaces (Triangles | Points): line skipped → the surface.
        const cwRayHit surfaces = intersector.intersectsDetailed(ray,
            ortho(0.5, kSurfaces));
        REQUIRE(surfaces.hit());
        CHECK(surfaces.objectId() == 2u);
    }

    SECTION("surface behind the line") {
        cwGeometryItersecter intersector;
        intersector.addObject(makeLineObject(1, {a, b}));
        // Triangle at z=-550 → depth 350, behind the line.
        intersector.addObject(makeTriangleObject(2,
            QVector3D(1005.0f, 1995.0f, -550.0f),
            QVector3D(1015.0f, 1995.0f, -550.0f),
            QVector3D(1010.0f, 2005.0f, -550.0f)));
        intersector.waitForFinish();

        // All: nearest wins → the line.
        const cwRayHit all = intersector.intersectsDetailed(ray, ortho(0.5));
        REQUIRE(all.hit());
        CHECK(all.objectId() == 1u);

        // Surfaces (Triangles | Points): line skipped → the farther surface.
        const cwRayHit surfaces = intersector.intersectsDetailed(ray,
            ortho(0.5, kSurfaces));
        REQUIRE(surfaces.hit());
        CHECK(surfaces.objectId() == 2u);
    }
}

// Backwards-compatibility guard: the default query carries a disabled
// tolerance, so a line never produces a hit — every pre-existing caller of
// intersectsDetailed(ray) is unaffected — while a surface still picks.
TEST_CASE("Line pick: default query yields no line hit but surfaces still hit",
          "[cwGeometryItersecter][linePick]")
{
    const QVector3D a(1000.0f, 2000.0f, -500.0f);
    const QVector3D b(1020.0f, 2000.0f, -500.0f);
    const QRay3D ray(QVector3D(1010.0f, 2000.0f, -200.0f), QVector3D(0.0f, 0.0f, -1.0f));

    SECTION("line-only object, default query misses") {
        cwGeometryItersecter intersector;
        intersector.addObject(makeLineObject(1, {a, b}));
        intersector.waitForFinish();

        CHECK_FALSE(intersector.intersectsDetailed(ray).hit());          // default {}
        CHECK(intersector.intersectsDetailed(ray, ortho(0.5)).hit());    // tolerance on
    }

    SECTION("triangle still picks under the default query") {
        cwGeometryItersecter intersector;
        intersector.addObject(makeTriangleObject(2,
            QVector3D(1005.0f, 1995.0f, -500.0f),
            QVector3D(1015.0f, 1995.0f, -500.0f),
            QVector3D(1010.0f, 2005.0f, -500.0f)));
        intersector.waitForFinish();

        const cwRayHit hit = intersector.intersectsDetailed(ray);
        REQUIRE(hit.hit());
        CHECK(hit.objectId() == 2u);
    }
}

// The solver names two degenerate cases; both must still pick cleanly.
TEST_CASE("Line pick: degenerate segments",
          "[cwGeometryItersecter][linePick]")
{
    const cwPickQuery query = ortho(0.5);

    SECTION("zero-length segment behaves like a point") {
        const QVector3D p(1000.0f, 2000.0f, -500.0f);
        cwGeometryItersecter intersector;
        intersector.addObject(makeLineObject(1, {p, p})); // from == to
        intersector.waitForFinish();

        // Ray passing 0.3 from p in y, straight down -Z.
        const QRay3D hitRay(QVector3D(1000.0f, 2000.3f, -200.0f), QVector3D(0.0f, 0.0f, -1.0f));
        const cwRayHit hit = intersector.intersectsDetailed(hitRay, query);
        REQUIRE(hit.hit());
        CHECK(hit.pointWorld().x() == Approx(1000.0f).margin(1e-3));
        CHECK(hit.pointWorld().y() == Approx(2000.0f).margin(1e-3));

        // Ray 0.8 away misses.
        const QRay3D missRay(QVector3D(1000.0f, 2000.8f, -200.0f), QVector3D(0.0f, 0.0f, -1.0f));
        CHECK_FALSE(intersector.intersectsDetailed(missRay, query).hit());
    }

    SECTION("ray parallel to the segment pins to the nearest endpoint") {
        const QVector3D a(1000.0f, 2000.0f, -500.0f);
        const QVector3D b(1040.0f, 2000.0f, -500.0f);
        cwGeometryItersecter intersector;
        intersector.addObject(makeLineObject(1, {a, b}));
        intersector.waitForFinish();

        // Ray also along +X, offset 0.3 in y, starting before A. Closest
        // approach is endpoint A at perpendicular distance 0.3.
        const QRay3D ray(QVector3D(980.0f, 2000.3f, -500.0f), QVector3D(1.0f, 0.0f, 0.0f));
        const cwRayHit hit = intersector.intersectsDetailed(ray, query);
        REQUIRE(hit.hit());
        CHECK(hit.pointWorld().x() == Approx(1000.0f).margin(1e-2));
        CHECK(hit.pointWorld().y() == Approx(2000.0f).margin(1e-3));
    }
}

// A segment behind the ray origin (negative ray-depth at the closest point)
// must be rejected, not picked. Same geometry, opposite ray direction, proves
// the tWorld <= 0 guard is what rejects it — not the distance test.
TEST_CASE("Line pick: a segment behind the ray origin is rejected",
          "[cwGeometryItersecter][linePick]")
{
    const QVector3D a(1000.0f, 2000.0f, -500.0f);
    const QVector3D b(1020.0f, 2000.0f, -500.0f);

    cwGeometryItersecter intersector;
    intersector.addObject(makeLineObject(1, {a, b}));
    intersector.waitForFinish();

    const cwPickQuery query = ortho(0.5);
    const QVector3D origin(1010.0f, 2000.0f, -500.3f); // 0.3 past the segment

    // Looking further away (-Z): the segment sits at negative ray-depth.
    const QRay3D away(origin, QVector3D(0.0f, 0.0f, -1.0f));
    CHECK_FALSE(intersector.intersectsDetailed(away, query).hit());

    // Looking back toward it (+Z): the same segment is now in front and hits.
    const QRay3D toward(origin, QVector3D(0.0f, 0.0f, 1.0f));
    const cwRayHit hit = intersector.intersectsDetailed(toward, query);
    REQUIRE(hit.hit());
    CHECK(hit.pointWorld().x() == Approx(1010.0f).margin(1e-2));
    CHECK(hit.pointWorld().z() == Approx(-500.0f).margin(1e-2));
}

// Two lines crossing the same ray: the nearer one wins by depth, regardless of
// insertion order (accept is `tWorld < best.tWorld()`, never a per-object bias).
TEST_CASE("Line pick: nearer of two lines wins by depth",
          "[cwGeometryItersecter][linePick]")
{
    // Near line at z=-450 (depth 250), far line at z=-500 (depth 300).
    const QVector<QVector3D> nearPts {QVector3D(1000.0f, 2000.0f, -450.0f),
                                      QVector3D(1020.0f, 2000.0f, -450.0f)};
    const QVector<QVector3D> farPts  {QVector3D(1000.0f, 2000.0f, -500.0f),
                                      QVector3D(1020.0f, 2000.0f, -500.0f)};

    const QRay3D ray(QVector3D(1010.0f, 2000.0f, -200.0f), QVector3D(0.0f, 0.0f, -1.0f));
    const cwPickQuery query = ortho(0.5);

    const auto pickNearestId = [&](bool nearFirst) {
        cwGeometryItersecter intersector;
        if (nearFirst) {
            intersector.addObject(makeLineObject(1, nearPts));
            intersector.addObject(makeLineObject(2, farPts));
        } else {
            intersector.addObject(makeLineObject(2, farPts));
            intersector.addObject(makeLineObject(1, nearPts));
        }
        intersector.waitForFinish();
        const cwRayHit hit = intersector.intersectsDetailed(ray, query);
        REQUIRE(hit.hit());
        CHECK(hit.tWorld() == Approx(250.0).margin(1e-2));
        return hit.objectId();
    };

    CHECK(pickNearestId(true) == 1u);   // near added first
    CHECK(pickNearestId(false) == 1u);  // near added last — still wins
}

// Regression: with cw.picking debug logging on, intersectsDetailed dumps every
// primitive in the hit leaf through dumpLeafPrimitive. That helper had no Line
// branch and read a segment as a triangle (three indices), overrunning the
// two-index buffer and aborting in QList::at (SIGABRT while loading + picking).
// Picking a single-segment line under the debug filter reproduces the crash on
// unfixed code; the fix reads the two endpoints instead.
TEST_CASE("Line pick: dumping a line leaf with picking debug on does not overrun the index buffer",
          "[cwGeometryItersecter][linePick]")
{
    // Flip the category's enabled flag rather than calling setFilterRules:
    // that clobbers any global rules a sibling test or QT_LOGGING_RULES
    // installed, and restoring it means wiping every rule rather than putting
    // the old ones back. The scope guard also survives an early REQUIRE exit,
    // which matters because a leaked cw.picking.debug=true would silently skew
    // the [performance] test's exact-pick baseline in a later file.
    QLoggingCategory& category = const_cast<QLoggingCategory&>(lcPick());
    const bool wasEnabled = category.isDebugEnabled();
    category.setEnabled(QtDebugMsg, true);
    auto restoreCategory = qScopeGuard([&category, wasEnabled]() {
        category.setEnabled(QtDebugMsg, wasEnabled);
    });

    const QVector3D a(1000.0f, 2000.0f, -500.0f);
    const QVector3D b(1020.0f, 2000.0f, -500.0f);

    cwGeometryItersecter intersector;
    intersector.addObject(makeLineObject(1, {a, b}));
    intersector.waitForFinish();

    // Straight through the segment so the leaf is visited and dumped. Before
    // the fix this aborts inside dumpLeafPrimitive; reaching the assertions
    // below means it no longer overruns.
    const QRay3D ray(QVector3D(1010.0f, 2000.0f, -200.0f), QVector3D(0.0f, 0.0f, -1.0f));
    const cwRayHit hit = intersector.intersectsDetailed(ray, ortho(0.5));

    CHECK(hit.hit());
    CHECK(hit.objectId() == 1u);
}

// Regression: QBox3D::intersection(ray) returns the box ENTRY parameter only
// while that is positive, and silently switches to the EXIT parameter once the
// ray origin is inside the box (QMath3d/qbox3d.cpp:403-416). The prune
// `tBox >= best.tWorld()` reads that value as a lower bound on the node's
// candidates, so an origin-inside node was compared on its far side and skipped
// while holding the nearest candidate. boxEntryDistance() clamps the entry to 0
// instead. The line tolerance pad is what makes this reachable: it inflates
// every box by the accept radius, which readily swallows a camera sitting
// inside the cave it is looking at.
//
// The same defect was found and fixed in nearestGeometryPoint first; this pins
// the exact path, which is the one leads, cwScenePick, and the measurement tool
// all run through.
TEST_CASE("Line pick: a node containing the ray origin is not pruned away",
          "[cwGeometryItersecter][linePick]")
{
    // The line runs the depth of the scene, so padding its box by the 10-unit
    // tolerance swallows the ray origin at z=100. Its exit parameter is then
    // ~210 — far behind the triangle at t=50 — so the unfixed prune drops the
    // whole line object after the triangle has already scored a hit.
    const QVector3D nearSpot(0.0f, 1.0f, 95.0f);  // 1 off-ray, depth 5
    cwGeometryItersecter intersector;
    intersector.addObject(makeTriangleObject(1,
                                             QVector3D(-10.0f, -10.0f, 50.0f),
                                             QVector3D(10.0f, -10.0f, 50.0f),
                                             QVector3D(0.0f, 10.0f, 50.0f)));
    intersector.addObject(makeLineObject(2, {nearSpot, QVector3D(0.0f, 1.0f, -100.0f)}));
    intersector.waitForFinish();

    // The triangle is hit exactly at depth 50; the line's near end is 1 off-ray
    // (inside the 10-unit radius) at depth 5, so the line must win.
    const QRay3D ray(QVector3D(0.0f, 0.0f, 100.0f), QVector3D(0.0f, 0.0f, -1.0f));
    const cwRayHit hit = intersector.intersectsDetailed(ray, ortho(10.0));

    REQUIRE(hit.hit());
    CHECK(hit.objectId() == 2u);
    CHECK(hit.tWorld() == Approx(5.0).margin(1e-2));
    CHECK(hit.pointWorld().z() == Approx(nearSpot.z()).margin(1e-3));
}

// The exact pick's BVH has to prune, not scan — and unlike the anchor next
// door, this path runs once per mouse-move rather than once per press. This is
// a performance contract with no behavioural signature to test instead: the
// scene-global tolerance pad this path originally shipped with returned exactly
// the same answers, but padded every node by the accept radius at the far
// corner of the WHOLE scene. A centerline near the camera was then inflated by
// a radius belonging to the far end of the survey, the ray crossed every leaf,
// and the descent degenerated into a linear scan.
//
// Absolute timings mean nothing across machines, build configs, or a CI box
// running several test processes at once, so this measures intersectsDetailed
// against nearestGeometryPoint on the SAME centerline with the SAME query in
// the SAME run. The anchor is the control: on a line-only scene the two paths
// apply an identical accept rule to every segment, so once their pads agree
// they do the same work and the ratio is ~1. The anchor's per-node pad is
// already pinned by its own [performance] test.
//
// The existing point-cloud control could not catch this: the exact path folds
// the pad in only for line objects, so a cloud scene never exercises it.
TEST_CASE("Line pick: the exact pick prunes instead of scanning the whole centerline",
          "[cwGeometryItersecter][linePick][performance]")
{
    // Only intersectsDetailed dumps per-primitive debug output — an inherited
    // cw.picking.debug=true would inflate the measured path alone and turn the
    // ratio below into a guaranteed failure.
    QLoggingCategory& category = const_cast<QLoggingCategory&>(lcPick());
    const bool wasEnabled = category.isDebugEnabled();
    category.setEnabled(QtDebugMsg, false);
    auto restoreCategory = qScopeGuard([&category, wasEnabled]() {
        category.setEnabled(QtDebugMsg, wasEnabled);
    });

    // A dense centerline beside the ray and close to the camera (depth 10-20),
    // plus one far shot that drags the scene's far corner out to depth ~5000.
    // The gap between those depths is the whole lever: a scene-global pad is
    // radiusAt(farDepth) ~ 79, which is ~250x the radius the cluster's own
    // depth earns and swallows a ray that misses it by 8 units.
    // An EVEN vertex count: the line plot lists shots as disjoint vertex pairs,
    // and addLines rejects an odd index list outright.
    constexpr int kVertices = 5000;
    constexpr float kMeanderRadius = 1.75f;
    constexpr float kMeanderCenterX = 10.0f;

    QVector<QVector3D> centerline;
    centerline.reserve(kVertices);
    for (int i = 0; i < kVertices; ++i) {
        const float t = float(i) / float(kVertices);
        const float angle = t * 40.0f * float(M_PI);
        centerline.append(QVector3D(kMeanderCenterX + kMeanderRadius * std::cos(angle),
                                    kMeanderRadius * std::sin(angle),
                                    -10.0f * t));
    }

    cwGeometryItersecter intersecter;
    intersecter.addObject(makeLineObject(1, centerline));
    intersecter.addObject(makeLineObject(2, {QVector3D(100.0f, 0.0f, -5000.0f),
                                             QVector3D(100.0f, 0.0f, -4990.0f)}));
    intersecter.waitForFinish();

    // A 4mm perspective tolerance derived exactly as cwCamera::pickQuery does:
    // slope = pixelRadius * 2 / (p11 * viewportHeight), with the 96dpi headless
    // fallback (3.78 px/mm), p11 = 1.921 for a 55 degree fov and a 1000px
    // viewport.
    const double pixelRadius = 4.0 * (96.0 / 25.4);
    const cwPickQuery query = perspective(pixelRadius * 2.0 / (1.921 * 1000.0));

    // Straight down the z axis, missing both objects by far more than the
    // radius either one's depth earns. A miss is the case that matters: it is
    // what every mouse-move over empty space costs, and it leaves the
    // prune-by-best path out of the measurement so only the pads decide.
    const QRay3D ray(QVector3D(0.0f, 0.0f, 10.0f), QVector3D(0.0f, 0.0f, -1.0f));

    // Preconditions — without these the timings measure the wrong thing and the
    // test would pass vacuously. Assert what the BVH actually holds, not what
    // was handed to addObject: addLines drops a malformed object with only a
    // qDebug, and a scene that silently lost its centerline still misses the ray
    // and still times fast, which reads exactly like a pass.
    const cwGeometryItersecter::DebugStatistics stats = intersecter.debugStatistics();
    REQUIRE(stats.lineSourceNodes == 2);
    REQUIRE(stats.totalPrimitives > 1000);
    REQUIRE_FALSE(intersecter.intersectsDetailed(ray, query).hit());
    REQUIRE_FALSE(intersecter.nearestGeometryPoint(ray, query).has_value());

    constexpr int kPicksPerSample = 500;
    const auto medianMs = [](const std::function<void()>& work) {
        constexpr int kRuns = 5;
        QList<double> samples;
        for (int i = 0; i < kRuns; ++i) {
            QElapsedTimer timer;
            timer.start();
            for (int pick = 0; pick < kPicksPerSample; ++pick) {
                work();
            }
            samples.append(double(timer.nsecsElapsed()) / 1e6);
        }
        std::sort(samples.begin(), samples.end());
        return samples.at(samples.size() / 2);
    };

    const double exactMs = medianMs([&]{ intersecter.intersectsDetailed(ray, query); });
    const double anchorMs = medianMs([&]{ intersecter.nearestGeometryPoint(ray, query); });

    // Guard against a degenerate control: if the anchor is too fast to time,
    // the ratio is noise rather than signal.
    REQUIRE(anchorMs > 0.0);

    INFO("exact " << exactMs << " ms, anchor " << anchorMs << " ms, ratio "
         << (exactMs / anchorMs) << "x over " << centerline.size()
         << " vertices, " << stats.totalPrimitives << " segments, " << kPicksPerSample << " picks per sample");
    CHECK(exactMs < anchorMs * 10.0);
}

