/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwCamera.h"
#include "cwLazClipInteraction.h"
#include "cwProjection.h"

//Qt includes
#include <QList>
#include <QMatrix4x4>
#include <QPointF>
#include <QRect>
#include <QVector3D>

namespace {

constexpr double kEyeTol = 1e-3;
// Camera unprojection truncates to integer pixels; tolerate the drift.
constexpr double kScreenTol = 1.5;

cwProjection orthoProjection(double halfExtent)
{
    cwProjection p;
    p.setOrtho(-halfExtent, halfExtent, -halfExtent, halfExtent, 0.1, 100.0);
    return p;
}

QMatrix4x4 lookAt(const QVector3D& eye, const QVector3D& center, const QVector3D& up)
{
    QMatrix4x4 m;
    m.lookAt(eye, center, up);
    return m;
}

void configureCamera(cwCamera* camera,
                     const QMatrix4x4& viewMatrix,
                     double halfExtent = 5.0)
{
    camera->setViewport(QRect(0, 0, 100, 100));
    camera->setProjection(orthoProjection(halfExtent));
    camera->setViewMatrix(viewMatrix);
}

// Eye-XY is the classifier's actual frame; world-Z along the cursor
// ray is an implementation choice. Tests assert on the projection.
QPointF eyeXY(const QMatrix4x4& view, const QVector3D& worldPoint)
{
    const QVector3D e = view.map(worldPoint);
    return QPointF(double(e.x()), double(e.y()));
}

} // namespace

// Centre click → eye (0, 0) for any camera.
TEST_CASE("cwLazClipInteraction: screen-centre click projects to eye (0, 0)",
          "[cwLazClipInteraction]")
{
    cwCamera camera;
    const QMatrix4x4 view = lookAt(QVector3D(0, 0, 10),
                                   QVector3D(0, 0, 0),
                                   QVector3D(0, 1, 0));
    configureCamera(&camera, view);

    cwLazClipInteraction interaction;
    interaction.setCamera(&camera);

    REQUIRE(interaction.state() == cwLazClipInteraction::State::Idle);
    interaction.addPoint(QPointF(50.0, 50.0));

    REQUIRE(interaction.state() == cwLazClipInteraction::State::Drawing);
    REQUIRE(interaction.pointCount() == 1);
    CHECK(interaction.errorMessage().isEmpty());

    const QList<QVector3D> poly = interaction.polygonWorldXYZ();
    REQUIRE(poly.size() == 1);

    const QPointF eye = eyeXY(view, poly.first());
    CHECK(eye.x() == Catch::Approx(0.0).margin(kEyeTol));
    CHECK(eye.y() == Catch::Approx(0.0).margin(kEyeTol));
}

// Discriminator vs eye-space storage: translated camera stores world
// XY at the scene point, not (0, 0).
TEST_CASE("cwLazClipInteraction: vertex is stored in world coordinates",
          "[cwLazClipInteraction]")
{
    cwCamera camera;
    const QMatrix4x4 view = lookAt(QVector3D(100, 200, 10),
                                   QVector3D(100, 200, 0),
                                   QVector3D(0, 1, 0));
    configureCamera(&camera, view);

    cwLazClipInteraction interaction;
    interaction.setCamera(&camera);
    interaction.addPoint(QPointF(50.0, 50.0));

    const QList<QVector3D> poly = interaction.polygonWorldXYZ();
    REQUIRE(poly.size() == 1);
    CHECK(double(poly.first().x()) == Catch::Approx(100.0).margin(kEyeTol));
    CHECK(double(poly.first().y()) == Catch::Approx(200.0).margin(kEyeTol));

    const QPointF eye = eyeXY(view, poly.first());
    CHECK(eye.x() == Catch::Approx(0.0).margin(kEyeTol));
    CHECK(eye.y() == Catch::Approx(0.0).margin(kEyeTol));
}

TEST_CASE("cwLazClipInteraction: centre click projects to eye (0, 0) under a tilted camera",
          "[cwLazClipInteraction]")
{
    cwCamera camera;
    const QMatrix4x4 view = lookAt(QVector3D(0.0f, -7.071f, 7.071f),
                                   QVector3D(0, 0, 0),
                                   QVector3D(0.0f, 0.7071f, 0.7071f));
    configureCamera(&camera, view);

    cwLazClipInteraction interaction;
    interaction.setCamera(&camera);
    interaction.addPoint(QPointF(50.0, 50.0));

    REQUIRE(interaction.pointCount() == 1);
    CHECK(interaction.errorMessage().isEmpty());

    const QPointF eye = eyeXY(view, interaction.polygonWorldXYZ().first());
    CHECK(eye.x() == Catch::Approx(0.0).margin(kEyeTol));
    CHECK(eye.y() == Catch::Approx(0.0).margin(kEyeTol));
}

// Profile view was previously rejected (ray parallel to z=0). No ground
// plane in world-space storage, so any angle works.
TEST_CASE("cwLazClipInteraction: profile view accepts clicks",
          "[cwLazClipInteraction]")
{
    cwCamera camera;
    const QMatrix4x4 view = lookAt(QVector3D(0, -10, 0),
                                   QVector3D(0, 0, 0),
                                   QVector3D(0, 0, 1));
    configureCamera(&camera, view);

    cwLazClipInteraction interaction;
    interaction.setCamera(&camera);
    interaction.addPoint(QPointF(50.0, 50.0));

    CHECK(interaction.state() == cwLazClipInteraction::State::Drawing);
    REQUIRE(interaction.pointCount() == 1);
    CHECK(interaction.errorMessage().isEmpty());

    const QPointF eye = eyeXY(view, interaction.polygonWorldXYZ().first());
    CHECK(eye.x() == Catch::Approx(0.0).margin(kEyeTol));
    CHECK(eye.y() == Catch::Approx(0.0).margin(kEyeTol));
}

// World-space storage doesn't see the projection matrix.
TEST_CASE("cwLazClipInteraction: polygon is invariant under ortho zoom",
          "[cwLazClipInteraction]")
{
    cwCamera camera;
    configureCamera(&camera,
                    lookAt(QVector3D(0, 0, 10),
                           QVector3D(0, 0, 0),
                           QVector3D(0, 1, 0)),
                    5.0);

    cwLazClipInteraction interaction;
    interaction.setCamera(&camera);

    interaction.addPoint(QPointF(25.0, 25.0));
    interaction.addPoint(QPointF(75.0, 25.0));
    interaction.addPoint(QPointF(75.0, 75.0));

    const QList<QVector3D> before = interaction.polygonWorldXYZ();
    REQUIRE(before.size() == 3);

    // Zoom out by widening the ortho frustum 4×.
    camera.setProjection(orthoProjection(20.0));

    const QList<QVector3D> after = interaction.polygonWorldXYZ();
    REQUIRE(after.size() == 3);
    for (qsizetype i = 0; i < before.size(); ++i) {
        CHECK(double(after.at(i).x()) ==
              Catch::Approx(double(before.at(i).x())).margin(kEyeTol));
        CHECK(double(after.at(i).y()) ==
              Catch::Approx(double(before.at(i).y())).margin(kEyeTol));
        CHECK(double(after.at(i).z()) ==
              Catch::Approx(double(before.at(i).z())).margin(kEyeTol));
    }
}

// Pan between clicks: prior vertices stay anchored to the scene.
TEST_CASE("cwLazClipInteraction: pan between clicks keeps prior vertices scene-anchored",
          "[cwLazClipInteraction]")
{
    cwCamera camera;
    configureCamera(&camera,
                    lookAt(QVector3D(0, 0, 10),
                           QVector3D(0, 0, 0),
                           QVector3D(0, 1, 0)));

    cwLazClipInteraction interaction;
    interaction.setCamera(&camera);
    interaction.addPoint(QPointF(50.0, 50.0)); // world (0, 0)

    const QVector3D first = interaction.polygonWorldXYZ().first();

    // Pan camera 3 units along +X.
    camera.setViewMatrix(lookAt(QVector3D(3, 0, 10),
                                QVector3D(3, 0, 0),
                                QVector3D(0, 1, 0)));

    // Click centre again — now lands at world (3, 0).
    interaction.addPoint(QPointF(50.0, 50.0));

    const QList<QVector3D> poly = interaction.polygonWorldXYZ();
    REQUIRE(poly.size() == 2);

    // First vertex unchanged.
    CHECK(double(poly.at(0).x()) == Catch::Approx(double(first.x())).margin(kEyeTol));
    CHECK(double(poly.at(0).y()) == Catch::Approx(double(first.y())).margin(kEyeTol));
    CHECK(double(poly.at(0).z()) == Catch::Approx(double(first.z())).margin(kEyeTol));

    // Second vertex reflects the panned camera.
    CHECK(double(poly.at(1).x()) == Catch::Approx(3.0).margin(kEyeTol));
    CHECK(double(poly.at(1).y()) == Catch::Approx(0.0).margin(kEyeTol));
}

TEST_CASE("cwLazClipInteraction: worldToScreen round-trips the original click",
          "[cwLazClipInteraction]")
{
    cwCamera camera;
    configureCamera(&camera,
                    lookAt(QVector3D(0, 0, 10),
                           QVector3D(0, 0, 0),
                           QVector3D(0, 1, 0)));

    cwLazClipInteraction interaction;
    interaction.setCamera(&camera);

    const QPointF click(72.0, 31.0);
    interaction.addPoint(click);

    const QList<QVector3D> poly = interaction.polygonWorldXYZ();
    REQUIRE(poly.size() == 1);

    const QPointF roundTrip = interaction.worldToScreen(poly.first());
    CHECK(roundTrip.x() == Catch::Approx(click.x()).margin(kScreenTol));
    CHECK(roundTrip.y() == Catch::Approx(click.y()).margin(kScreenTol));
}

TEST_CASE("cwLazClipInteraction: addWorldPoint stores the vertex unchanged",
          "[cwLazClipInteraction]")
{
    cwLazClipInteraction interaction;
    interaction.addWorldPoint(QVector3D(1.5f, -2.25f, 0.0f));
    interaction.addWorldPoint(QVector3D(3.0f, 4.0f, 0.5f));
    interaction.addWorldPoint(QVector3D(-1.0f, 0.0f, -0.25f));

    REQUIRE(interaction.state() == cwLazClipInteraction::State::Drawing);
    const QList<QVector3D> poly = interaction.polygonWorldXYZ();
    REQUIRE(poly.size() == 3);
    CHECK(poly.at(0) == QVector3D(1.5f, -2.25f, 0.0f));
    CHECK(poly.at(1) == QVector3D(3.0f, 4.0f, 0.5f));
    CHECK(poly.at(2) == QVector3D(-1.0f, 0.0f, -0.25f));
}
