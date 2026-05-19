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

// Top-down (looking down -z) is the only camera orientation that
// produces sensible screen→world projection for ground-plane polygons.
// Verify the supported case still works.
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

// Profile view (camera horizontal): ray runs parallel to ground plane.
// The polygon must not gain a point and an error must surface — otherwise
// the user gets a silent no-op.
TEST_CASE("cwLazClipInteraction rejects clicks under a profile camera", "[cwLazClipInteraction]")
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

// Regression: any camera orientation other than looking down -z produces
// a ray that *hits* the ground plane but at a skewed XY, silently giving
// a polygon that no longer matches what the user drew on screen. The
// interaction must reject the click rather than place a misplaced point.
TEST_CASE("cwLazClipInteraction rejects clicks under a tilted camera", "[cwLazClipInteraction]")
{
    cwCamera camera;
    // 45° tilt: camera at (0, -7.07, 7.07) looking at origin, up tilted
    // to match. The ray for the viewport center hits the ground plane at
    // a non-vertical angle.
    configureCamera(&camera,
                    lookAt(QVector3D(0.0f, -7.071f, 7.071f),
                           QVector3D(0, 0, 0),
                           QVector3D(0.0f, 0.7071f, 0.7071f)));

    cwLazClipInteraction interaction;
    interaction.setCamera(&camera);

    interaction.addPoint(QPointF(50.0, 50.0));

    CHECK(interaction.state() == cwLazClipInteraction::State::Idle);
    CHECK(interaction.pointCount() == 0);
    CHECK_FALSE(interaction.errorMessage().isEmpty());
}

// Slight float-precision wobble around exact top-down (e.g. after the
// pitch animation lands at 89.999°) must still be accepted; only
// meaningful tilt should be rejected.
TEST_CASE("cwLazClipInteraction tolerates tiny float-precision wobble near top-down", "[cwLazClipInteraction]")
{
    cwCamera camera;
    // ~0.1° off vertical
    QMatrix4x4 view;
    view.lookAt(QVector3D(0.0f, -0.0175f, 10.0f),
                QVector3D(0, 0, 0),
                QVector3D(0, 1, 0));
    configureCamera(&camera, view);

    cwLazClipInteraction interaction;
    interaction.setCamera(&camera);

    interaction.addPoint(QPointF(50.0, 50.0));

    CHECK(interaction.state() == cwLazClipInteraction::State::Drawing);
    CHECK(interaction.pointCount() == 1);
}
