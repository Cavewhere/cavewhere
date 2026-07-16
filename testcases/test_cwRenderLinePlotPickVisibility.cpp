//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwScene.h"
#include "cwRenderLinePlot.h"
#include "cwGeometryItersecter.h"
#include "cwPickQuery.h"
#include "cwRayHit.h"

//Qt includes
#include <QVector3D>
#include <QVector>

//QMath3d
#include <qray3d.h>

using namespace Catch;

namespace {

// A drawn shot owns vertices [2i, 2i+1] (non-indexed line list). Two shots,
// placed far apart in Y so a pick aimed at one is unambiguous:
//   shot 0 at y = 0
//   shot 1 at y = kSecondShotY  (this is the one we hide)
constexpr float kSecondShotY = 100.0f;

// The centerline is 1-D, so a ray only "hits" it within a proximity radius.
// A constant (ortho-style) world radius is enough here.
constexpr double kPickToleranceConstant = 1.0;

QVector<QVector3D> twoShots()
{
    return {
        QVector3D(-10.0f, 0.0f,         0.0f),  // shot 0, vertex 0
        QVector3D( 10.0f, 0.0f,         0.0f),  // shot 0, vertex 1
        QVector3D(-10.0f, kSecondShotY, 0.0f),  // shot 1, vertex 2
        QVector3D( 10.0f, kSecondShotY, 0.0f),  // shot 1, vertex 3
    };
}

// Ray straight down -Z through the middle of shot 1, at (0, kSecondShotY, 0).
QRay3D rayThroughSecondShot()
{
    return QRay3D(QVector3D(0.0f, kSecondShotY, 50.0f),
                  QVector3D(0.0f, 0.0f, -1.0f));
}

cwPickQuery linePickQuery()
{
    cwPickQuery query;
    query.kinds = cwPickQuery::Kind::Lines;
    query.tolerance.constant = kPickToleranceConstant;
    return query;
}

} // namespace

// KNOWN BUG (issue #575). Per-shot visibility (cwRenderLinePlot::setRangeVisible)
// is render-side only and never reaches cwGeometryItersecter — the full
// centerline (every shot) is registered once in setGeometry(), and the
// intersecter's only visibility gate is whole render-object
// (isPickable -> parent->isVisible()). So a pick aimed at a HIDDEN shot still
// returns a hit, which the Distance measurement tool (a Kind::Lines pre-pick)
// wrongly snaps to.
//
// This test asserts the DESIRED behavior and is tagged [!shouldfail]: Catch2
// keeps the run green while it fails, and flags it (unexpected pass) once the
// bug is fixed so the tag can be removed.
TEST_CASE("cwRenderLinePlot hidden shot must not be pickable",
          "[cwRenderLinePlot][!shouldfail]")
{
    cwScene scene;

    cwRenderLinePlot linePlot;
    linePlot.setScene(&scene);
    linePlot.setGeometry(twoShots());
    scene.geometryItersecter()->waitForFinish();

    // Baseline: while visible, shot 1 (vertices [2, 3]) is pickable.
    REQUIRE(scene.geometryItersecter()->intersectsDetailed(
                rayThroughSecondShot(), linePickQuery()).hit());

    // Hide shot 1 via the real per-shot visibility path.
    linePlot.setRangeVisible(2, 2, false);

    // Desired: the hidden shot is no longer pickable. Currently fails — the hit
    // still lands on shot 1 at y == kSecondShotY.
    const cwRayHit hit = scene.geometryItersecter()->intersectsDetailed(
        rayThroughSecondShot(), linePickQuery());
    CHECK_FALSE(hit.hit());
}

// Contrast: whole render-object visibility IS the gate the intersecter honors,
// so hiding the entire line plot makes the same ray miss. This is the mechanism
// per-shot hiding is missing (issue #575).
TEST_CASE("cwRenderLinePlot whole-object hide removes it from intersecter picks",
          "[cwRenderLinePlot]")
{
    cwScene scene;

    cwRenderLinePlot linePlot;
    linePlot.setScene(&scene);
    linePlot.setGeometry(twoShots());
    scene.geometryItersecter()->waitForFinish();

    REQUIRE(scene.geometryItersecter()->intersectsDetailed(
                rayThroughSecondShot(), linePickQuery()).hit());

    linePlot.setVisible(false);

    const cwRayHit hit = scene.geometryItersecter()->intersectsDetailed(
        rayThroughSecondShot(), linePickQuery());
    CHECK_FALSE(hit.hit());
}
