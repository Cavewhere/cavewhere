//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

// Qt
#include <QMatrix4x4>
#include <QVector3D>
#include <QRay3D>

// SUT
#include "cwGeometryItersecter.h"
#include "cwRayHit.h"
#include "cwRenderObject.h"

using namespace Catch;

namespace {

constexpr float kPickRadius = 0.1f;

cwGeometryItersecter::Object makePointObject(cwRenderObject* parent,
                                             uint64_t id,
                                             const QVector<QVector3D>& points,
                                             const QMatrix4x4& modelMatrix = QMatrix4x4(),
                                             float pickRadius = kPickRadius)
{
    cwGeometry geometry {
        {cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3}
    };
    geometry.set(cwGeometry::Semantic::Position, points);
    geometry.setType(cwGeometry::Type::Points);

    return cwGeometryItersecter::Object({parent, id},
                                        geometry,
                                        modelMatrix,
                                        pickRadius);
}

cwGeometryItersecter::Object makeSinglePointObject(cwRenderObject* parent,
                                                   uint64_t id,
                                                   const QVector3D& position,
                                                   const QMatrix4x4& modelMatrix = QMatrix4x4(),
                                                   float pickRadius = kPickRadius)
{
    return makePointObject(parent, id, QVector<QVector3D>{position}, modelMatrix, pickRadius);
}

cwGeometryItersecter::Object makeTriangleAtZ(uint64_t id, float worldZ)
{
    QVector<QVector3D> points;
    points << QVector3D(-1.0f, -1.0f, 0.0f)
           << QVector3D( 1.0f, -1.0f, 0.0f)
           << QVector3D( 0.0f,  1.0f, 0.0f);

    QVector<uint32_t> indexes;
    indexes << 0u << 1u << 2u;

    cwGeometry geometry {
        {cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3}
    };
    geometry.set(cwGeometry::Semantic::Position, points);
    geometry.setIndices(indexes);
    geometry.setType(cwGeometry::Type::Triangles);

    QMatrix4x4 modelMatrix;
    modelMatrix.translate(0.0f, 0.0f, worldZ);
    return cwGeometryItersecter::Object({nullptr, id}, geometry, modelMatrix);
}

} // namespace

TEST_CASE("Ray straight through a point hits and reports correct fields", "[cwGeometryItersecter][points]")
{
    const QVector3D origin(0.0f, 0.0f, 10.0f);
    const QVector3D dir(0.0f, 0.0f, -1.0f);
    const QRay3D ray(origin, dir);

    cwGeometryItersecter intersector;
    intersector.addObject(makeSinglePointObject(nullptr, 1, QVector3D(0.0f, 0.0f, 0.0f)));
    intersector.waitForFinish();

    const cwRayHit hit = intersector.intersectsDetailed(ray);

    REQUIRE(hit.hit());
    REQUIRE(hit.firstIndex() == 0);
    REQUIRE(hit.pointWorld().z() == Approx(0.0f).margin(1e-4));
    // pointWorld is the vertex center (so callers can identify which point
    // was clicked). tWorld is the sphere-entry depth — distance from the
    // ray origin to where the ray first touches the point's pickRadius
    // sphere — which matches the triangle path's "tWorld is the surface
    // intersection distance" semantics and ensures consistent ranking when
    // points and triangles compete for the same hit. For a head-on ray
    // from z=10 to a point at z=0 with pickRadius=0.1, tWorld = 9.9.
    REQUIRE(hit.tWorld() == Approx(10.0 - kPickRadius).margin(1e-4));
}

TEST_CASE("Ray within pickRadius of a point still hits", "[cwGeometryItersecter][points]")
{
    // Ray runs parallel to Z, offset by half pickRadius in X.
    const float offset = kPickRadius * 0.5f;
    const QVector3D origin(offset, 0.0f, 10.0f);
    const QRay3D ray(origin, QVector3D(0.0f, 0.0f, -1.0f));

    cwGeometryItersecter intersector;
    intersector.addObject(makeSinglePointObject(nullptr, 1, QVector3D(0.0f, 0.0f, 0.0f)));
    intersector.waitForFinish();

    const cwRayHit hit = intersector.intersectsDetailed(ray);
    REQUIRE(hit.hit());
}

TEST_CASE("Ray outside the pick radius does not hit the point", "[cwGeometryItersecter][points]")
{
    // The pick sphere is the whole rule: outside it is a miss.
    //
    // Offset DIAGONALLY, and by only ~1.13x the radius, so this single case
    // pins two things a comfortable axis-aligned 10x would miss:
    //   - addPoints pads the object's broad-phase AABB by a full pickRadius,
    //     so an x-only offset above 1x is rejected by the BOX and never
    //     reaches the sphere test. (0.08, 0.08) is 0.113 from the centre — a
    //     true miss — yet sits inside that padded box, so only the ray/sphere
    //     test can reject it. That makes this the case that pins the reject
    //     boundary as a sphere rather than the box around it.
    //   - 1.13x is inside the 2.5x reach of the deleted near-miss fallback, so
    //     this hit before that fallback was removed.
    const float offset = kPickRadius * 0.8f;
    const QVector3D origin(offset, offset, 10.0f);
    const QRay3D ray(origin, QVector3D(0.0f, 0.0f, -1.0f));

    cwGeometryItersecter intersector;
    intersector.addObject(makeSinglePointObject(nullptr, 1, QVector3D(0.0f, 0.0f, 0.0f)));
    intersector.waitForFinish();

    const cwRayHit hit = intersector.intersectsDetailed(ray);
    REQUIRE_FALSE(hit.hit());
}

TEST_CASE("Point cloud picks at translated modelMatrix", "[cwGeometryItersecter][points]")
{
    const QVector3D origin(5.0f, 5.0f, 100.0f);
    const QRay3D ray(origin, QVector3D(0.0f, 0.0f, -1.0f));

    QMatrix4x4 modelMatrix;
    modelMatrix.translate(5.0f, 5.0f, 0.0f);

    cwGeometryItersecter intersector;
    intersector.addObject(makeSinglePointObject(nullptr, 1,
                                                QVector3D(0.0f, 0.0f, 20.0f),
                                                modelMatrix));
    intersector.waitForFinish();

    const cwRayHit hit = intersector.intersectsDetailed(ray);
    REQUIRE(hit.hit());
    // World position of the point is (5,5,20).
    REQUIRE(hit.pointWorld().x() == Approx(5.0f).margin(1e-4));
    REQUIRE(hit.pointWorld().y() == Approx(5.0f).margin(1e-4));
    REQUIRE(hit.pointWorld().z() == Approx(20.0f).margin(1e-4));
}

TEST_CASE("Closest of two point clouds is returned", "[cwGeometryItersecter][points]")
{
    const QVector3D origin(0.0f, 0.0f, 100.0f);
    const QRay3D ray(origin, QVector3D(0.0f, 0.0f, -1.0f));

    cwGeometryItersecter intersector;
    intersector.addObject(makeSinglePointObject(nullptr, 1, QVector3D(0.0f, 0.0f, 0.0f)));
    intersector.addObject(makeSinglePointObject(nullptr, 2, QVector3D(0.0f, 0.0f, 50.0f)));
    intersector.waitForFinish();

    const cwRayHit hit = intersector.intersectsDetailed(ray);
    REQUIRE(hit.hit());
    REQUIRE(hit.objectId() == 2u); // z=50 is closer to origin at z=100
}

TEST_CASE("Mixed triangle and point — closer one wins regardless of type", "[cwGeometryItersecter][points]")
{
    const QVector3D origin(0.0f, 0.0f, 100.0f);
    const QRay3D ray(origin, QVector3D(0.0f, 0.0f, -1.0f));

    SECTION("Point in front of triangle — point wins")
    {
        cwGeometryItersecter intersector;
        intersector.addObject(makeTriangleAtZ(1, 10.0f));
        intersector.addObject(makeSinglePointObject(nullptr, 2, QVector3D(0.0f, 0.0f, 50.0f)));
        intersector.waitForFinish();

        const cwRayHit hit = intersector.intersectsDetailed(ray);
        REQUIRE(hit.hit());
        REQUIRE(hit.objectId() == 2u);
    }

    SECTION("Triangle in front of point — triangle wins")
    {
        cwGeometryItersecter intersector;
        intersector.addObject(makeTriangleAtZ(1, 50.0f));
        intersector.addObject(makeSinglePointObject(nullptr, 2, QVector3D(0.0f, 0.0f, 10.0f)));
        intersector.waitForFinish();

        const cwRayHit hit = intersector.intersectsDetailed(ray);
        REQUIRE(hit.hit());
        REQUIRE(hit.objectId() == 1u);
    }
}

TEST_CASE("Hidden render object is skipped during picking", "[cwGeometryItersecter][points]")
{
    cwRenderObject hiddenOwner;
    hiddenOwner.setVisible(false);

    cwRenderObject visibleOwner;
    REQUIRE(visibleOwner.isVisible());

    const QVector3D origin(0.0f, 0.0f, 100.0f);
    const QRay3D ray(origin, QVector3D(0.0f, 0.0f, -1.0f));

    cwGeometryItersecter intersector;
    // Hidden cloud is closer (z=50), visible cloud is further (z=10).
    // If visibility is honored, the visible (further) cloud wins.
    intersector.addObject(makeSinglePointObject(&hiddenOwner, 1, QVector3D(0.0f, 0.0f, 50.0f)));
    intersector.addObject(makeSinglePointObject(&visibleOwner, 2, QVector3D(0.0f, 0.0f, 10.0f)));
    intersector.waitForFinish();

    const cwRayHit hit = intersector.intersectsDetailed(ray);
    REQUIRE(hit.hit());
    REQUIRE(hit.object() == &visibleOwner);
}

TEST_CASE("Ray with origin inside a point's sphere does not produce a hit", "[cwGeometryItersecter][points]")
{
    // Origin sits exactly at the point center → inside the sphere.
    const QVector3D origin(0.0f, 0.0f, 0.0f);
    const QRay3D ray(origin, QVector3D(0.0f, 0.0f, -1.0f));

    cwGeometryItersecter intersector;
    intersector.addObject(makeSinglePointObject(nullptr, 1, QVector3D(0.0f, 0.0f, 0.0f)));
    intersector.waitForFinish();

    const cwRayHit hit = intersector.intersectsDetailed(ray);
    REQUIRE_FALSE(hit.hit());
}
