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

    const cwRayHit hit = intersector.intersectsDetailed(ray);

    REQUIRE(hit.hit());
    REQUIRE(hit.firstIndex() == 0);
    REQUIRE(hit.pointWorld().z() == Approx(0.0f).margin(1e-4));
    // pointWorld is the vertex center (so callers can identify which point
    // was clicked), and tWorld is the distance from ray origin to that
    // center — 10.0 along a unit -Z ray from z=10 to z=0.
    REQUIRE(hit.tWorld() == Approx(10.0).margin(1e-4));
}

TEST_CASE("Ray within pickRadius of a point still hits", "[cwGeometryItersecter][points]")
{
    // Ray runs parallel to Z, offset by half pickRadius in X.
    const float offset = kPickRadius * 0.5f;
    const QVector3D origin(offset, 0.0f, 10.0f);
    const QRay3D ray(origin, QVector3D(0.0f, 0.0f, -1.0f));

    cwGeometryItersecter intersector;
    intersector.addObject(makeSinglePointObject(nullptr, 1, QVector3D(0.0f, 0.0f, 0.0f)));

    const cwRayHit hit = intersector.intersectsDetailed(ray);
    REQUIRE(hit.hit());
}

TEST_CASE("Ray beyond pickRadius does not hit the point", "[cwGeometryItersecter][points]")
{
    // Offset > pickRadius — ray misses the sphere entirely.
    const float offset = kPickRadius * 2.0f;
    const QVector3D origin(offset, 0.0f, 10.0f);
    const QRay3D ray(origin, QVector3D(0.0f, 0.0f, -1.0f));

    cwGeometryItersecter intersector;
    intersector.addObject(makeSinglePointObject(nullptr, 1, QVector3D(0.0f, 0.0f, 0.0f)));

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

        const cwRayHit hit = intersector.intersectsDetailed(ray);
        REQUIRE(hit.hit());
        REQUIRE(hit.objectId() == 2u);
    }

    SECTION("Triangle in front of point — triangle wins")
    {
        cwGeometryItersecter intersector;
        intersector.addObject(makeTriangleAtZ(1, 50.0f));
        intersector.addObject(makeSinglePointObject(nullptr, 2, QVector3D(0.0f, 0.0f, 10.0f)));

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

    const cwRayHit hit = intersector.intersectsDetailed(ray);
    REQUIRE_FALSE(hit.hit());
}
