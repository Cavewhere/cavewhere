//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

// Qt
#include <QMatrix4x4>
#include <QVector3D>
#include <QRay3D>

// SUT
#include "cwGeometryItersecter.h"

using namespace Catch;

cwGeometryItersecter::Object makeTriangleObject(uint64_t id,
                                                const QMatrix4x4& modelMatrix)
{
    QVector<QVector3D> points;
    points << QVector3D(0.0f, 0.0f, 0.0f)
           << QVector3D(1.0f, 0.0f, 0.0f)
           << QVector3D(0.0f, 1.0f, 0.0f);

    QVector<uint> indexes;
    indexes << 0u << 1u << 2u;

    // parent can be nullptr in tests; ids must be unique per parent
    return cwGeometryItersecter::Object(nullptr, id, points, indexes,
                                        cwGeometry::Triangles,
                                        modelMatrix);
}

TEST_CASE("Ray intersects a single triangle true triangle test", "[cwGeometryItersecter]")
{
    // Camera/ray in world space
    const QVector3D originWorld(0.25f, 0.25f, 100.0f);
    const QVector3D dirWorld(0.0f, 0.0f, -1.0f); // unit length
    const QRay3D ray(originWorld, dirWorld);

    // One triangle lying in model z=0, translated to world z=10
    QMatrix4x4 worldFromModel;
    worldFromModel.setToIdentity();
    worldFromModel.translate(0.0f, 0.0f, 10.0f);

    cwGeometryItersecter intersector;
    intersector.addObject(makeTriangleObject(1, worldFromModel));

    // Expected: hit at world z=10 directly below origin (inside triangle)
    // Direction is unit-length along -Z, so tWorld = 100 - 10 = 90.
    const double t = intersector.intersects(ray);

    REQUIRE(std::isfinite(t));
    REQUIRE(t == Approx(90.0).margin(1e-4));
}

TEST_CASE("Ray intersects multiple triangles and returns the closest hit", "[cwGeometryItersecter]")
{
    // Camera/ray in world space
    const QVector3D originWorld(0.25f, 0.25f, 100.0f);
    const QVector3D dirWorld(0.0f, 0.0f, -1.0f); // unit length
    const QRay3D ray(originWorld, dirWorld);

    // Two identical triangles in model space (z=0), placed at different world Z
    // Triangle A at world z=60  -> expected tA = 40
    // Triangle B at world z=30  -> expected tB = 70
    QMatrix4x4 tA; tA.setToIdentity(); tA.translate(0.0f, 0.0f, 60.0f);
    QMatrix4x4 tB; tB.setToIdentity(); tB.translate(0.0f, 0.0f, 30.0f);

    cwGeometryItersecter intersector;
    intersector.addObject(makeTriangleObject(100, tA));
    intersector.addObject(makeTriangleObject(200, tB));

    // Should pick the closer one (z=60), thus tWorld ≈ 40
    const double t = intersector.intersects(ray);

    REQUIRE(std::isfinite(t));
    REQUIRE(t == Approx(40.0).margin(1e-4));
}
