/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwCamera.h"
#include "cwLazClipInteraction.h"
#include "cwProjection.h"

//Qt includes
#include <QMatrix4x4>
#include <QPointF>
#include <QRect>
#include <QVector3D>

namespace {

cwProjection makeOrthoProjection()
{
    cwProjection p;
    p.setOrtho(-5.0, 5.0, -5.0, 5.0, 0.1, 100.0);
    return p;
}

QMatrix4x4 lookAt(const QVector3D& eye, const QVector3D& center, const QVector3D& up)
{
    QMatrix4x4 m;
    m.lookAt(eye, center, up);
    return m;
}

void configureCamera(cwCamera* camera, const QMatrix4x4& viewMatrix)
{
    camera->setViewport(QRect(0, 0, 100, 100));
    camera->setProjection(makeOrthoProjection());
    camera->setViewMatrix(viewMatrix);
}

} // namespace

// Top-down (looking down -z): the canonical case. Ray intersects z = 0
// directly under the click.
TEST_CASE("cwLazClipInteraction accepts clicks under a top-down camera", "[cwLazClipInteraction]")
{
    cwCamera camera;
    configureCamera(&camera,
                    lookAt(QVector3D(0, 0, 10),
                           QVector3D(0, 0, 0),
                           QVector3D(0, 1, 0)));

    cwLazClipInteraction interaction;
    interaction.setCamera(&camera);

    REQUIRE(interaction.state() == cwLazClipInteraction::State::Idle);
    interaction.addPoint(QPointF(50.0, 50.0));

    CHECK(interaction.state() == cwLazClipInteraction::State::Drawing);
    CHECK(interaction.pointCount() == 1);
    CHECK(interaction.errorMessage().isEmpty());
}

// 45° tilt: ray hits the ground plane at a non-vertical angle. The
// polygon is the camera-projection of the screen-drawn shape onto z=0,
// so a tilted view is a supported case — the click must produce a
// valid vertex.
TEST_CASE("cwLazClipInteraction accepts clicks under a tilted camera", "[cwLazClipInteraction]")
{
    cwCamera camera;
    configureCamera(&camera,
                    lookAt(QVector3D(0.0f, -7.071f, 7.071f),
                           QVector3D(0, 0, 0),
                           QVector3D(0.0f, 0.7071f, 0.7071f)));

    cwLazClipInteraction interaction;
    interaction.setCamera(&camera);

    interaction.addPoint(QPointF(50.0, 50.0));

    CHECK(interaction.state() == cwLazClipInteraction::State::Drawing);
    CHECK(interaction.pointCount() == 1);
    CHECK(interaction.errorMessage().isEmpty());
}

// Steeper tilt (~75° off vertical) still produces a valid ground-plane
// hit — only an exactly-parallel ray should be rejected.
TEST_CASE("cwLazClipInteraction accepts clicks under a steeply tilted camera", "[cwLazClipInteraction]")
{
    cwCamera camera;
    configureCamera(&camera,
                    lookAt(QVector3D(0.0f, -9.659f, 2.588f),
                           QVector3D(0, 0, 0),
                           QVector3D(0.0f, 0.2588f, 0.9659f)));

    cwLazClipInteraction interaction;
    interaction.setCamera(&camera);

    interaction.addPoint(QPointF(50.0, 50.0));

    CHECK(interaction.state() == cwLazClipInteraction::State::Drawing);
    CHECK(interaction.pointCount() == 1);
    CHECK(interaction.errorMessage().isEmpty());
}

// Singularity: the camera looks exactly horizontally (profile view), so
// the ray runs parallel to the ground plane. There is no intersection
// — surface an error rather than placing a vertex at infinity.
TEST_CASE("cwLazClipInteraction rejects clicks when the ray is parallel to the ground", "[cwLazClipInteraction]")
{
    cwCamera camera;
    configureCamera(&camera,
                    lookAt(QVector3D(0, -10, 0),
                           QVector3D(0, 0, 0),
                           QVector3D(0, 0, 1)));

    cwLazClipInteraction interaction;
    interaction.setCamera(&camera);

    interaction.addPoint(QPointF(50.0, 50.0));

    CHECK(interaction.state() == cwLazClipInteraction::State::Idle);
    CHECK(interaction.pointCount() == 0);
    CHECK_FALSE(interaction.errorMessage().isEmpty());
}
