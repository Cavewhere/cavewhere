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

// Ray straight down -Z through the middle of shot 0, at the origin.
QRay3D rayThroughFirstShot()
{
    return QRay3D(QVector3D(0.0f, 0.0f, 50.0f),
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

// Issue #575. The full centerline (every shot) is one intersecter Key,
// registered once in setGeometry(), so per-shot visibility can't use the
// whole-object gate. cwRenderLinePlot::setRangeVisible now twins the change
// into the intersecter's per-vertex mask: a segment whose endpoint vertex is
// hidden is skipped by the pick traversal, so the Distance measurement tool
// (a Kind::Lines pre-pick) no longer snaps to a keyword-hidden shot. No BVH
// rebuild — the toggle is synchronous.
TEST_CASE("cwRenderLinePlot hidden shot is not pickable",
          "[cwRenderLinePlot]")
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

    CHECK_FALSE(scene.geometryItersecter()->intersectsDetailed(
        rayThroughSecondShot(), linePickQuery()).hit());

    // Shot 0 stays pickable — the mask hides segments, not the object.
    CHECK(scene.geometryItersecter()->intersectsDetailed(
        rayThroughFirstShot(), linePickQuery()).hit());

    // Re-showing restores pickability.
    linePlot.setRangeVisible(2, 2, true);
    CHECK(scene.geometryItersecter()->intersectsDetailed(
        rayThroughSecondShot(), linePickQuery()).hit());
}

// Issue #549 (per-trip flavor). A hidden shot is excluded from
// cwScene::visibleFramingBounds(), so reset view frames only what is drawn.
// The masked object pays an O(vertices) walk only while partially hidden;
// re-showing everything collapses the mask back to the cached-box fast path.
TEST_CASE("cwRenderLinePlot hidden shot does not inflate framing bounds",
          "[cwRenderLinePlot]")
{
    cwScene scene;

    cwRenderLinePlot linePlot;
    linePlot.setScene(&scene);
    linePlot.setGeometry(twoShots());

    // Baseline: both shots contribute — the box reaches shot 1.
    REQUIRE(scene.visibleFramingBounds().maximum().y() >= kSecondShotY);

    linePlot.setRangeVisible(2, 2, false);

    // The framing box tightens to shot 0 (y == 0) while the raw registered
    // box keeps counting the hidden geometry.
    const QBox3D box = scene.visibleFramingBounds();
    CHECK(box.maximum().y() < kSecondShotY);
    CHECK_FALSE(box.isNull());
    CHECK(scene.geometryItersecter()->boundingBox().maximum().y() >= kSecondShotY);

    // Hiding every shot leaves nothing to frame. Derive the count — an
    // out-of-range range is a silent no-op, so a hardcoded literal that
    // drifted from twoShots() would quietly test nothing.
    const int vertexCount = static_cast<int>(twoShots().size());
    linePlot.setRangeVisible(0, vertexCount, false);
    CHECK(scene.visibleFramingBounds().isNull());

    // Re-showing everything restores the full framing box.
    linePlot.setRangeVisible(0, vertexCount, true);
    CHECK(scene.visibleFramingBounds().maximum().y() >= kSecondShotY);
}

// Out-of-range ranges are a silent no-op on both sides of the twin — nothing
// hides, nothing crashes. Exercises the intersecter's guard directly (the
// render-side guard would otherwise short-circuit the forward).
TEST_CASE("cwRenderLinePlot out-of-range visibility ranges are ignored",
          "[cwRenderLinePlot]")
{
    cwScene scene;

    cwRenderLinePlot linePlot;
    linePlot.setScene(&scene);
    linePlot.setGeometry(twoShots());
    scene.geometryItersecter()->waitForFinish();

    const cwGeometryItersecter::Key key{&linePlot, 0};
    const int vertexCount = static_cast<int>(twoShots().size());
    scene.geometryItersecter()->setRangeVisible(key, -1, 2, false);
    scene.geometryItersecter()->setRangeVisible(key, 0, 0, false);
    scene.geometryItersecter()->setRangeVisible(key, 0, -2, false);
    scene.geometryItersecter()->setRangeVisible(key, vertexCount - 1, 2, false);
    scene.geometryItersecter()->setRangeVisible(key, 0, vertexCount + 1, false);

    CHECK(scene.geometryItersecter()->intersectsDetailed(
        rayThroughFirstShot(), linePickQuery()).hit());
    CHECK(scene.geometryItersecter()->intersectsDetailed(
        rayThroughSecondShot(), linePickQuery()).hit());
    CHECK(scene.visibleFramingBounds().maximum().y() >= kSecondShotY);
}

// Hiding both shots through separate range calls, then re-showing them one
// at a time: each partial show restores exactly its own shot, and the final
// show restores the full framing box (the mask collapses back to the
// all-visible fast path only once BOTH ranges are shown).
TEST_CASE("cwRenderLinePlot partial ranges hide and show independently",
          "[cwRenderLinePlot]")
{
    cwScene scene;

    cwRenderLinePlot linePlot;
    linePlot.setScene(&scene);
    linePlot.setGeometry(twoShots());
    scene.geometryItersecter()->waitForFinish();

    linePlot.setRangeVisible(0, 2, false);
    linePlot.setRangeVisible(2, 2, false);
    CHECK(scene.visibleFramingBounds().isNull());

    linePlot.setRangeVisible(0, 2, true);
    CHECK(scene.geometryItersecter()->intersectsDetailed(
        rayThroughFirstShot(), linePickQuery()).hit());
    CHECK_FALSE(scene.geometryItersecter()->intersectsDetailed(
        rayThroughSecondShot(), linePickQuery()).hit());

    linePlot.setRangeVisible(2, 2, true);
    CHECK(scene.geometryItersecter()->intersectsDetailed(
        rayThroughSecondShot(), linePickQuery()).hit());
    CHECK(scene.visibleFramingBounds().maximum().y() >= kSecondShotY);
}

// The rotation-pivot anchor (AnchorPick) shares the leaf-loop mask gate with
// the exact pick, so a hidden shot can't capture the pivot either.
TEST_CASE("cwRenderLinePlot hidden shot does not anchor the rotation pivot",
          "[cwRenderLinePlot]")
{
    cwScene scene;

    cwRenderLinePlot linePlot;
    linePlot.setScene(&scene);
    linePlot.setGeometry(twoShots());
    scene.geometryItersecter()->waitForFinish();

    REQUIRE(scene.geometryItersecter()->nearestGeometryPoint(
                rayThroughSecondShot(), linePickQuery()).has_value());

    linePlot.setRangeVisible(2, 2, false);

    CHECK_FALSE(scene.geometryItersecter()->nearestGeometryPoint(
        rayThroughSecondShot(), linePickQuery()).has_value());
}

// Replacing the geometry resets the mask to all-visible on BOTH sides of the
// twin — new vertex indices mean the old ranges no longer apply, and the
// owner (cwLinePlotManager::reconcileTripKeywordItems) re-applies hidden
// ranges right after. This pins that contract: after setGeometry, a
// previously hidden shot is pickable again until ranges are re-applied.
TEST_CASE("cwRenderLinePlot geometry replacement resets the visibility mask",
          "[cwRenderLinePlot]")
{
    cwScene scene;

    cwRenderLinePlot linePlot;
    linePlot.setScene(&scene);
    linePlot.setGeometry(twoShots());
    scene.geometryItersecter()->waitForFinish();

    linePlot.setRangeVisible(2, 2, false);
    REQUIRE_FALSE(scene.geometryItersecter()->intersectsDetailed(
        rayThroughSecondShot(), linePickQuery()).hit());

    linePlot.setGeometry(twoShots());
    scene.geometryItersecter()->waitForFinish();

    CHECK(scene.geometryItersecter()->intersectsDetailed(
        rayThroughSecondShot(), linePickQuery()).hit());
    CHECK(scene.visibleFramingBounds().maximum().y() >= kSecondShotY);
}

// Contrast: whole render-object visibility is its own gate, independent of
// the per-vertex mask — hiding the entire line plot makes the same ray miss
// with no mask involved.
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
