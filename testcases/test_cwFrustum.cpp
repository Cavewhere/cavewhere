// test_cwFrustum.cpp
// Catch2 unit tests for cwFrustum's plane extraction and conservative AABB test.

#include <catch2/catch_test_macros.hpp>

#include "cwFrustum.h"

#include <QBox3D>
#include <QMatrix4x4>
#include <QVector3D>

namespace {

constexpr float kFieldOfView = 60.0f;
constexpr float kAspectRatio = 1.0f;
constexpr float kNearPlane = 1.0f;
constexpr float kFarPlane = 100.0f;

constexpr float kOrthoExtent = 10.0f;

//Half the width of the perspective frustum at z = -10, for a 60 degree
//vertical field of view and a square viewport: 10 * tan(30 degrees).
constexpr float kHalfWidthAtTen = 5.7735f;

QMatrix4x4 viewLookingDownNegativeZ(float eyeZ)
{
    QMatrix4x4 view;
    view.lookAt(QVector3D(0.0f, 0.0f, eyeZ),
                QVector3D(0.0f, 0.0f, eyeZ - 1.0f),
                QVector3D(0.0f, 1.0f, 0.0f));
    return view;
}

cwFrustum perspectiveFrustum(float eyeZ = 0.0f)
{
    QMatrix4x4 projection;
    projection.perspective(kFieldOfView, kAspectRatio, kNearPlane, kFarPlane);
    return cwFrustum::fromViewProjection(projection * viewLookingDownNegativeZ(eyeZ));
}

cwFrustum orthoFrustum(float eyeZ = 0.0f)
{
    QMatrix4x4 projection;
    projection.ortho(-kOrthoExtent, kOrthoExtent,
                     -kOrthoExtent, kOrthoExtent,
                     kNearPlane, kFarPlane);
    return cwFrustum::fromViewProjection(projection * viewLookingDownNegativeZ(eyeZ));
}

QBox3D boxAt(const QVector3D& center, float halfSize)
{
    const QVector3D half(halfSize, halfSize, halfSize);
    return QBox3D(center - half, center + half);
}

QBox3D pointBox(const QVector3D& point)
{
    return QBox3D(point, point);
}

constexpr float kQuarterTurnDegrees = 90.0f;
constexpr float kSkewAngleDegrees = 37.0f;
constexpr float kBoundsTolerance = 1e-4f;

bool fuzzyEquals(const QVector3D& left, const QVector3D& right)
{
    return (left - right).length() <= kBoundsTolerance;
}

//The reference the helper has to match: the AABB of the eight corners, built
//the long way around.
QBox3D boxOfTransformedCorners(const QBox3D& box, const QMatrix4x4& matrix)
{
    const QVector3D minimum = box.minimum();
    const QVector3D maximum = box.maximum();

    QBox3D result;
    for (const float x : {minimum.x(), maximum.x()}) {
        for (const float y : {minimum.y(), maximum.y()}) {
            for (const float z : {minimum.z(), maximum.z()}) {
                result.unite(matrix.map(QVector3D(x, y, z)));
            }
        }
    }

    return result;
}

}

TEST_CASE("cwFrustum: a default frustum is invalid and intersects everything", "[Frustum]") {
    const cwFrustum frustum;

    CHECK_FALSE(frustum.isValid());
    CHECK(frustum.intersects(boxAt(QVector3D(0.0f, 0.0f, -10.0f), 1.0f)));
    CHECK(frustum.intersects(boxAt(QVector3D(1000.0f, 1000.0f, 1000.0f), 1.0f)));
    CHECK(frustum.intersects(pointBox(QVector3D(0.0f, 0.0f, 500.0f))));
}

TEST_CASE("cwFrustum: an unusable matrix yields an invalid frustum", "[Frustum]") {
    const cwFrustum frustum = cwFrustum::fromViewProjection(QMatrix4x4(0.0f, 0.0f, 0.0f, 0.0f,
                                                                      0.0f, 0.0f, 0.0f, 0.0f,
                                                                      0.0f, 0.0f, 0.0f, 0.0f,
                                                                      0.0f, 0.0f, 0.0f, 0.0f));

    CHECK_FALSE(frustum.isValid());
    CHECK(frustum.intersects(boxAt(QVector3D(0.0f, 0.0f, -10.0f), 1.0f)));
}

TEST_CASE("cwFrustum: a perspective camera keeps boxes in front of it", "[Frustum]") {
    const cwFrustum frustum = perspectiveFrustum();

    REQUIRE(frustum.isValid());
    CHECK(frustum.intersects(boxAt(QVector3D(0.0f, 0.0f, -10.0f), 1.0f)));
    CHECK(frustum.intersects(boxAt(QVector3D(0.0f, 0.0f, -2.0f), 0.5f)));
}

TEST_CASE("cwFrustum: a perspective camera culls boxes outside every plane", "[Frustum]") {
    const cwFrustum frustum = perspectiveFrustum();
    REQUIRE(frustum.isValid());

    SECTION("behind the camera") {
        CHECK_FALSE(frustum.intersects(boxAt(QVector3D(0.0f, 0.0f, 10.0f), 1.0f)));
    }

    SECTION("beyond the far plane") {
        CHECK_FALSE(frustum.intersects(boxAt(QVector3D(0.0f, 0.0f, -200.0f), 1.0f)));
    }

    SECTION("left of the frustum") {
        CHECK_FALSE(frustum.intersects(boxAt(QVector3D(-50.0f, 0.0f, -10.0f), 1.0f)));
    }

    SECTION("right of the frustum") {
        CHECK_FALSE(frustum.intersects(boxAt(QVector3D(50.0f, 0.0f, -10.0f), 1.0f)));
    }

    SECTION("above the frustum") {
        CHECK_FALSE(frustum.intersects(boxAt(QVector3D(0.0f, 50.0f, -10.0f), 1.0f)));
    }

    SECTION("below the frustum") {
        CHECK_FALSE(frustum.intersects(boxAt(QVector3D(0.0f, -50.0f, -10.0f), 1.0f)));
    }
}

TEST_CASE("cwFrustum: a perspective camera keeps straddling and containing boxes", "[Frustum]") {
    const cwFrustum frustum = perspectiveFrustum();
    REQUIRE(frustum.isValid());

    SECTION("straddling the left plane") {
        CHECK(frustum.intersects(boxAt(QVector3D(-kHalfWidthAtTen, 0.0f, -10.0f), 2.0f)));
    }

    SECTION("straddling the top plane") {
        CHECK(frustum.intersects(boxAt(QVector3D(0.0f, kHalfWidthAtTen, -10.0f), 2.0f)));
    }

    SECTION("containing the whole frustum") {
        CHECK(frustum.intersects(boxAt(QVector3D(0.0f, 0.0f, 0.0f), 1000.0f)));
    }
}

TEST_CASE("cwFrustum: a perspective camera handles point boxes", "[Frustum]") {
    const cwFrustum frustum = perspectiveFrustum();
    REQUIRE(frustum.isValid());

    CHECK(frustum.intersects(pointBox(QVector3D(0.0f, 0.0f, -10.0f))));
    CHECK_FALSE(frustum.intersects(pointBox(QVector3D(0.0f, 0.0f, 10.0f))));
    CHECK_FALSE(frustum.intersects(pointBox(QVector3D(-50.0f, 0.0f, -10.0f))));
}

TEST_CASE("cwFrustum: a moved perspective camera culls in world space", "[Frustum]") {
    const float eyeZ = 20.0f;
    const cwFrustum frustum = perspectiveFrustum(eyeZ);
    REQUIRE(frustum.isValid());

    CHECK(frustum.intersects(boxAt(QVector3D(0.0f, 0.0f, 0.0f), 1.0f)));
    CHECK_FALSE(frustum.intersects(boxAt(QVector3D(0.0f, 0.0f, eyeZ + 10.0f), 1.0f)));
    CHECK_FALSE(frustum.intersects(boxAt(QVector3D(0.0f, 0.0f, eyeZ - kFarPlane - 10.0f), 1.0f)));
}

TEST_CASE("cwFrustum: an ortho camera keeps boxes in front of it", "[Frustum]") {
    const cwFrustum frustum = orthoFrustum();

    REQUIRE(frustum.isValid());
    CHECK(frustum.intersects(boxAt(QVector3D(0.0f, 0.0f, -50.0f), 1.0f)));
    CHECK(frustum.intersects(boxAt(QVector3D(9.0f, 9.0f, -50.0f), 0.5f)));
}

TEST_CASE("cwFrustum: an ortho camera culls boxes outside every plane", "[Frustum]") {
    const cwFrustum frustum = orthoFrustum();
    REQUIRE(frustum.isValid());

    SECTION("behind the camera") {
        CHECK_FALSE(frustum.intersects(boxAt(QVector3D(0.0f, 0.0f, 10.0f), 1.0f)));
    }

    SECTION("beyond the far plane") {
        CHECK_FALSE(frustum.intersects(boxAt(QVector3D(0.0f, 0.0f, -200.0f), 1.0f)));
    }

    SECTION("left of the frustum") {
        CHECK_FALSE(frustum.intersects(boxAt(QVector3D(-50.0f, 0.0f, -50.0f), 1.0f)));
    }

    SECTION("right of the frustum") {
        CHECK_FALSE(frustum.intersects(boxAt(QVector3D(50.0f, 0.0f, -50.0f), 1.0f)));
    }

    SECTION("above the frustum") {
        CHECK_FALSE(frustum.intersects(boxAt(QVector3D(0.0f, 50.0f, -50.0f), 1.0f)));
    }

    SECTION("below the frustum") {
        CHECK_FALSE(frustum.intersects(boxAt(QVector3D(0.0f, -50.0f, -50.0f), 1.0f)));
    }
}

TEST_CASE("cwFrustum: an ortho camera keeps straddling and containing boxes", "[Frustum]") {
    const cwFrustum frustum = orthoFrustum();
    REQUIRE(frustum.isValid());

    SECTION("straddling the left plane") {
        CHECK(frustum.intersects(boxAt(QVector3D(-kOrthoExtent, 0.0f, -50.0f), 2.0f)));
    }

    SECTION("straddling the near plane") {
        CHECK(frustum.intersects(boxAt(QVector3D(0.0f, 0.0f, -kNearPlane), 2.0f)));
    }

    SECTION("containing the whole frustum") {
        CHECK(frustum.intersects(boxAt(QVector3D(0.0f, 0.0f, 0.0f), 1000.0f)));
    }
}

TEST_CASE("cwFrustum: an ortho camera handles point boxes", "[Frustum]") {
    const cwFrustum frustum = orthoFrustum();
    REQUIRE(frustum.isValid());

    CHECK(frustum.intersects(pointBox(QVector3D(0.0f, 0.0f, -50.0f))));
    CHECK_FALSE(frustum.intersects(pointBox(QVector3D(0.0f, 0.0f, 10.0f))));
    CHECK_FALSE(frustum.intersects(pointBox(QVector3D(0.0f, 50.0f, -50.0f))));
}

TEST_CASE("transformedBounds: contains every transformed corner", "[Frustum]") {
    const QBox3D box(QVector3D(-1.0f, -2.0f, -3.0f), QVector3D(4.0f, 5.0f, 6.0f));

    QMatrix4x4 matrix;

    SECTION("identity leaves the box alone") {
        const QBox3D result = transformedBounds(box, matrix);

        CHECK(fuzzyEquals(result.minimum(), box.minimum()));
        CHECK(fuzzyEquals(result.maximum(), box.maximum()));
    }

    SECTION("pure translation slides the box") {
        const QVector3D offset(10.0f, -20.0f, 30.0f);
        matrix.translate(offset);

        const QBox3D result = transformedBounds(box, matrix);

        CHECK(fuzzyEquals(result.minimum(), box.minimum() + offset));
        CHECK(fuzzyEquals(result.maximum(), box.maximum() + offset));
    }

    SECTION("a 90 degree rotation swaps extents") {
        matrix.rotate(kQuarterTurnDegrees, 0.0f, 0.0f, 1.0f);

        const QBox3D result = transformedBounds(box, matrix);

        //Rotating about z by 90 degrees maps (x, y) to (-y, x).
        CHECK(fuzzyEquals(result.minimum(), QVector3D(-5.0f, -1.0f, -3.0f)));
        CHECK(fuzzyEquals(result.maximum(), QVector3D(2.0f, 4.0f, 6.0f)));
    }

    SECTION("nonuniform scale stretches each axis") {
        matrix.scale(2.0f, 3.0f, -1.0f);

        const QBox3D result = transformedBounds(box, matrix);

        CHECK(fuzzyEquals(result.minimum(), QVector3D(-2.0f, -6.0f, -6.0f)));
        CHECK(fuzzyEquals(result.maximum(), QVector3D(8.0f, 15.0f, 3.0f)));
    }

    SECTION("a full affine transform still contains all eight corners") {
        matrix.translate(3.0f, -4.0f, 5.0f);
        matrix.rotate(kSkewAngleDegrees, 1.0f, 2.0f, 3.0f);
        matrix.scale(1.5f, 0.25f, 2.0f);

        const QBox3D result = transformedBounds(box, matrix);
        const QBox3D corners = boxOfTransformedCorners(box, matrix);

        CHECK(fuzzyEquals(result.minimum(), corners.minimum()));
        CHECK(fuzzyEquals(result.maximum(), corners.maximum()));
    }
}

TEST_CASE("transformedBounds: passes degenerate boxes through", "[Frustum]") {
    QMatrix4x4 matrix;
    matrix.translate(1.0f, 2.0f, 3.0f);

    SECTION("a null box stays null") {
        CHECK(transformedBounds(QBox3D(), matrix).isNull());
    }

    SECTION("a point box moves as a point") {
        const QVector3D point(1.0f, 1.0f, 1.0f);
        const QBox3D result = transformedBounds(pointBox(point), matrix);

        CHECK(fuzzyEquals(result.minimum(), matrix.map(point)));
        CHECK(fuzzyEquals(result.maximum(), matrix.map(point)));
    }
}
