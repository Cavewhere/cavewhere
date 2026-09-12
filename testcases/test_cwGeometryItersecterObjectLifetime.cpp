//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwGeometryItersecter.h"
#include "cwRenderLinePlot.h"
#include "cwScene.h"

//Qt includes
#include <QVector3D>
#include <QVector>

namespace {

// Two shots in the z = 0 plane, enough for the intersecter to build a sub-BVH.
QVector<QVector3D> twoShots()
{
    return {
        QVector3D(-10.0f, 0.0f, 0.0f),
        QVector3D( 10.0f, 0.0f, 0.0f),
        QVector3D(-10.0f, 100.0f, 0.0f),
        QVector3D( 10.0f, 100.0f, 0.0f),
    };
}

} // namespace

// A Node that outlives its render object keeps serving picks for deleted
// geometry — still a use-after-free, since a hit fills cwRayHit::object()
// from the Object's raw attribution pointer. This pins the purge in
// cwScene::removeItem() that prevents it; see that function for why it lives
// there.
//
// A point cloud used to stand in for the heavy registrant here. Between Q2 and
// Q4 of the point octree plan a streamed cloud registers nothing with the
// intersecter, so the line plot carries the case; Q4 restores the point-cloud
// variant, along with the LAZ-layer one that lived beside it.
TEST_CASE("cwGeometryItersecter forgets a deleted render object",
          "[cwGeometryItersecter][cwRenderLinePlot]")
{
    cwScene scene;
    auto* intersecter = scene.geometryItersecter();

    auto* linePlot = new cwRenderLinePlot();
    linePlot->setScene(&scene);
    linePlot->setGeometry(twoShots());

    // Captured while the plot is alive; the id-based Key stays a usable probe
    // after the delete below — ids are never recycled (issue #512).
    const cwGeometryItersecter::Key key{linePlot->renderObjectId(),
                                        cwRenderLinePlot::kSubId};

    intersecter->waitForFinish();
    REQUIRE_FALSE(intersecter->boundingBox(key).isNull());
    REQUIRE_FALSE(intersecter->visibleBoundingBox().isNull());

    // Exactly what a scene node does when its layer is removed or disabled.
    // Note setScene(nullptr) runs first, which nulls m_scene — so
    // geometryItersecter() returns nullptr from here on, and a purge written in
    // ~cwRenderObject() would silently do nothing.
    linePlot->setScene(nullptr);
    delete linePlot;

    REQUIRE(intersecter->boundingBox(key).isNull());
    CHECK(intersecter->visibleBoundingBox().isNull());
}
