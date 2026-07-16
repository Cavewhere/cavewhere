//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

// Std
#include <cmath>
#include <numeric>

// Qt
#include <QMatrix4x4>
#include <QVector3D>
#include <QRay3D>

// SUT
#include "cwGeometryItersecter.h"
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
