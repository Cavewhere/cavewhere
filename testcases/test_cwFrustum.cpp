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
