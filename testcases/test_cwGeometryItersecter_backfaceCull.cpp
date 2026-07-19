//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

// Qt
#include <QMatrix4x4>
#include <QVector3D>
#include <QRay3D>

// SUT
#include "cwGeometryItersecter.h"
#include "TestGeometryBuilders.h"

using namespace Catch;

namespace {

// A single triangle in the model-space z=0 plane, wound CCW so its geometric
// normal (cross(b-a, c-a)) points along +Z. This is the winding glTF meshes
// arrive with (cwGltfLoader preserves the CCW front-face convention), so it
// mirrors what a .glb lidar note registers with the picker.
cwGeometryItersecter::Object makeTriangle(uint64_t id,
                                          bool cullBackfaces,
                                          const QMatrix4x4& modelMatrix = QMatrix4x4())
{
    QVector<QVector3D> points;
    points << QVector3D(-1.0f, -1.0f, 0.0f)
           << QVector3D( 3.0f, -1.0f, 0.0f)   // CCW: normal = +Z
           << QVector3D(-1.0f,  3.0f, 0.0f);

    return cwGeometryItersecter::Object(
        nullptr, id, cwTestGeometry::triangles(points, {0u, 1u, 2u}, cullBackfaces),
        modelMatrix);
}

// A ray that strikes the triangle at (0.25, 0.25) from the +Z side, travelling
// toward -Z. It hits the CCW/front face — the face the GPU keeps under
// CullMode::Back + FrontFace::CCW.
QRay3D frontRay()
{
    return QRay3D(QVector3D(0.25f, 0.25f, 100.0f), QVector3D(0.0f, 0.0f, -1.0f));
}

// The same strike point approached from the -Z side, travelling toward +Z. It
// hits the back side of the triangle — the face the GPU culls (never drawn).
QRay3D backRay()
{
    return QRay3D(QVector3D(0.25f, 0.25f, -100.0f), QVector3D(0.0f, 0.0f, 1.0f));
}

} // namespace

// Issue #563: "the measuring tool picks on backfaces". The measuring tool's
// pick path is cwScenePick::snappedPoint -> intersectsDetailed -> ExactPick,
// which culls backfaces when the geometry asks it to. glTF meshes keep the
// default cullBackfaces == true, so a ray hitting the back side of a lidar-mesh
// triangle must be skipped — exactly the face the renderer culls.
TEST_CASE("Backface-culled triangle is not picked from behind",
          "[cwGeometryItersecter][backface]")
{
    cwGeometryItersecter intersector;
    intersector.addObject(makeTriangle(1, /*cullBackfaces=*/true));
    intersector.waitForFinish();

    // Front face is pickable (sanity: the geometry and ray are set up right).
    const double front = intersector.intersects(frontRay());
    REQUIRE(std::isfinite(front));
    REQUIRE(front == Approx(100.0).margin(1e-4));

    // Back face must be skipped — this is the #563 assertion.
    const double back = intersector.intersects(backRay());
    REQUIRE_FALSE(std::isfinite(back));
}

// Control: with culling disabled (as cwTriangulateTask sets on double-sided
// scraps) the same back-side ray DOES hit. This pins the cull flag as the thing
// that gates the behaviour above, so a regression can't quietly pass by making
// every triangle double-sided.
TEST_CASE("Double-sided triangle is pickable from behind",
          "[cwGeometryItersecter][backface]")
{
    cwGeometryItersecter intersector;
    intersector.addObject(makeTriangle(1, /*cullBackfaces=*/false));
    intersector.waitForFinish();

    const double back = intersector.intersects(backRay());
    REQUIRE(std::isfinite(back));
    REQUIRE(back == Approx(100.0).margin(1e-4));
}

// A placed lidar note carries a model matrix built from a proper rotation and a
// positive uniform scale (cwNoteLiDARTransformation), i.e. determinant +1. Such
// a transform preserves handedness, so the picker's model-space cull must still
// agree with the renderer's screen-space cull: the back face stays unpickable.
TEST_CASE("Backface cull survives a proper-rotation model matrix",
          "[cwGeometryItersecter][backface]")
{
    QMatrix4x4 placed;
    placed.translate(10.0f, -5.0f, 3.0f);
    placed.rotate(37.0f, QVector3D(0.2f, 0.9f, 0.4f).normalized());
    placed.scale(2.0f); // uniform, positive -> det > 0

    cwGeometryItersecter intersector;
    intersector.addObject(makeTriangle(1, /*cullBackfaces=*/true, placed));
    intersector.waitForFinish();

    // Build front/back rays in world space by pushing the model-space strike
    // point and its two approach directions through the same transform.
    const QVector3D hitWorld = placed.map(QVector3D(0.25f, 0.25f, 0.0f));
    const QVector3D frontDirWorld =
        placed.mapVector(QVector3D(0.0f, 0.0f, -1.0f)).normalized();
    const QVector3D backDirWorld =
        placed.mapVector(QVector3D(0.0f, 0.0f, 1.0f)).normalized();

    const QRay3D placedFrontRay(hitWorld - frontDirWorld * 100.0f, frontDirWorld);
    const QRay3D placedBackRay(hitWorld - backDirWorld * 100.0f, backDirWorld);

    REQUIRE(std::isfinite(intersector.intersects(placedFrontRay)));
    REQUIRE_FALSE(std::isfinite(intersector.intersects(placedBackRay)));
}
