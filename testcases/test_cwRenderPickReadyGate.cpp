//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwScene.h"
#include "cwSceneVisibility.h"
#include "cwGeometryItersecter.h"
#include "cwRenderLinePlot.h"
#include "cwRenderPointCloud.h"
#include "cwRenderTexturedItems.h"
#include "cwGeometry.h"

//Qt includes
#include <QVector>
#include <QVector2D>
#include <QVector3D>

// Issue #505 Phase 4, plans/PICKER_MUTATION_PIPELINE_PLAN.html. A heavy object
// registered with the intersecter stays render-hidden until its sub-BVH
// publishes, so "rendered ⇒ pickable" holds: the pick-ready gate publishes
// effective visibility = userVisible && pickReady into the scene visibility
// store, and releases on bvhReady(). These tests pin the gate lifecycle at the
// store level (the published truth the RHI backend reads).
//
// The gate arms synchronously in setGeometry/addItem, so the "hidden" state is
// observable immediately — no timing window. waitForFinish() then completes the
// async build and delivers bvhReady() on this thread, releasing the gate
// deterministically.

namespace {

QVector<QVector3D> twoShots()
{
    return {
        QVector3D(-10.0f, 0.0f,   0.0f),
        QVector3D( 10.0f, 0.0f,   0.0f),
        QVector3D(-10.0f, 100.0f, 0.0f),
        QVector3D( 10.0f, 100.0f, 0.0f),
    };
}

QVector<QVector3D> threeShots()
{
    QVector<QVector3D> shots = twoShots();
    shots.append(QVector3D(-10.0f, 200.0f, 0.0f));
    shots.append(QVector3D( 10.0f, 200.0f, 0.0f));
    return shots;
}

cwGeometry triangle()
{
    cwGeometry geometry({
        { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 },
        { cwGeometry::Semantic::TexCoord0, cwGeometry::AttributeFormat::Vec2 }
    });

    geometry.resizeVertices(3);
    const auto* positionAttribute = geometry.attribute(cwGeometry::Semantic::Position);
    const auto* texCoordAttribute = geometry.attribute(cwGeometry::Semantic::TexCoord0);

    const QVector<QVector3D> positions = {
        QVector3D(-10.0f, -10.0f, 0.0f),
        QVector3D( 10.0f, -10.0f, 0.0f),
        QVector3D(  0.0f,  10.0f, 0.0f),
    };
    for (int i = 0; i < positions.size(); ++i) {
        geometry.set(positionAttribute, i, positions.at(i));
        geometry.set(texCoordAttribute, i, QVector2D(0.0f, 0.0f));
    }

    geometry.setIndices({0u, 1u, 2u});
    geometry.setType(cwGeometry::Type::Triangles);
    return geometry;
}

cwRenderPointCloud::GeometryData smallCloud()
{
    cwGeometry geometry({
        { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 }
    });
    geometry.resizeVertices(4);
    const auto* positionAttribute = geometry.attribute(cwGeometry::Semantic::Position);
    const QVector<QVector3D> positions = {
        QVector3D(0.0f, 0.0f, 0.0f),
        QVector3D(1.0f, 0.0f, 0.0f),
        QVector3D(0.0f, 1.0f, 0.0f),
        QVector3D(1.0f, 1.0f, 0.0f),
    };
    for (int i = 0; i < positions.size(); ++i) {
        geometry.set(positionAttribute, i, positions.at(i));
    }
    geometry.setType(cwGeometry::Type::Points);

    cwRenderPointCloud::GeometryData data;
    data.geometry = geometry;
    data.bboxMin = QVector3D(0.0f, 0.0f, 0.0f);
    data.bboxMax = QVector3D(1.0f, 1.0f, 0.0f);
    data.meanSpacingXY = 1.0f;
    return data;
}

} // namespace

TEST_CASE("A line plot is hidden until its sub-BVH publishes",
          "[cwRenderObject][pickReadyGate]")
{
    cwScene scene;
    const auto id = [](const cwRenderObject& object) { return object.renderObjectId(); };

    cwRenderLinePlot linePlot;
    linePlot.setScene(&scene);

    // No geometry registered yet — nothing to gate, sparse default is visible.
    REQUIRE(scene.visibility()->snapshot().objectVisible(id(linePlot)));

    linePlot.setGeometry(twoShots());

    // The gate armed synchronously; the build is still in flight, so the plot
    // is render-hidden and not yet pickable.
    REQUIRE_FALSE(scene.visibility()->snapshot().objectVisible(id(linePlot)));
    REQUIRE_FALSE(scene.geometryItersecter()->isObjectPickReady(
        {id(linePlot), cwRenderLinePlot::kSubId}));

    scene.geometryItersecter()->waitForFinish();

    // bvhReady() released the gate: pickable and visible on the same edge.
    REQUIRE(scene.geometryItersecter()->isObjectPickReady(
        {id(linePlot), cwRenderLinePlot::kSubId}));
    REQUIRE(scene.visibility()->snapshot().objectVisible(id(linePlot)));
}

TEST_CASE("A geometry edit on a ready object never re-hides it",
          "[cwRenderObject][pickReadyGate]")
{
    cwScene scene;
    cwRenderLinePlot linePlot;
    linePlot.setScene(&scene);
    linePlot.setGeometry(twoShots());
    scene.geometryItersecter()->waitForFinish();
    REQUIRE(scene.visibility()->snapshot().objectVisible(linePlot.renderObjectId()));

    // Same Key (kSubId) re-registered on a survey edit. First-publish-only: the
    // gate must not re-arm, so the plot stays visible across the rebuild rather
    // than blinking (eventual consistency — old geometry stays shown).
    linePlot.setGeometry(threeShots());
    REQUIRE(scene.visibility()->snapshot().objectVisible(linePlot.renderObjectId()));

    scene.geometryItersecter()->waitForFinish();
    REQUIRE(scene.visibility()->snapshot().objectVisible(linePlot.renderObjectId()));
}

TEST_CASE("A point cloud is hidden until its sub-BVH publishes",
          "[cwRenderObject][pickReadyGate]")
{
    cwScene scene;
    cwRenderPointCloud cloud;
    cloud.setScene(&scene);

    cloud.setGeometry(smallCloud());
    REQUIRE_FALSE(scene.visibility()->snapshot().objectVisible(cloud.renderObjectId()));

    scene.geometryItersecter()->waitForFinish();
    REQUIRE(scene.geometryItersecter()->isObjectPickReady({cloud.renderObjectId(), 0}));
    REQUIRE(scene.visibility()->snapshot().objectVisible(cloud.renderObjectId()));
}

TEST_CASE("A point cloud re-arms its gate after clear and re-set",
          "[cwRenderObject][pickReadyGate]")
{
    cwScene scene;
    cwRenderPointCloud cloud;
    cloud.setScene(&scene);

    // Clear while the first build is still in flight (gate armed): dropping the
    // geometry cancels the intersecter's readiness promise for key 0, and the
    // cloud must republish visible rather than stay wedged hidden.
    cloud.setGeometry(smallCloud());
    REQUIRE_FALSE(scene.visibility()->snapshot().objectVisible(cloud.renderObjectId()));
    cloud.clear();
    REQUIRE(scene.visibility()->snapshot().objectVisible(cloud.renderObjectId()));

    // Re-setting the SAME key arms a fresh gate — hidden until the new build
    // publishes — proving the cancelled promise didn't leave the gate stuck.
    cloud.setGeometry(smallCloud());
    REQUIRE_FALSE(scene.visibility()->snapshot().objectVisible(cloud.renderObjectId()));

    scene.geometryItersecter()->waitForFinish();
    REQUIRE(scene.geometryItersecter()->isObjectPickReady({cloud.renderObjectId(), 0}));
    REQUIRE(scene.visibility()->snapshot().objectVisible(cloud.renderObjectId()));
}

TEST_CASE("A textured sub-item is hidden until its sub-BVH publishes",
          "[cwRenderObject][pickReadyGate]")
{
    cwScene scene;
    cwRenderTexturedItems items;
    items.setScene(&scene);

    cwRenderTexturedItems::Item item;
    item.geometry = triangle();
    const uint32_t itemId = items.addItem(item);

    // Sub-item gating: the object stays visible, but the item's sub entry is
    // hidden until its sub-BVH publishes.
    REQUIRE(scene.visibility()->snapshot().objectVisible(items.renderObjectId()));
    REQUIRE_FALSE(scene.visibility()->snapshot().subVisible(items.renderObjectId(), itemId));

    scene.geometryItersecter()->waitForFinish();
    REQUIRE(scene.geometryItersecter()->isObjectPickReady({items.renderObjectId(), itemId}));
    REQUIRE(scene.visibility()->snapshot().subVisible(items.renderObjectId(), itemId));
}

TEST_CASE("A textured item with no registered geometry is never gated",
          "[cwRenderObject][pickReadyGate]")
{
    cwScene scene;
    cwRenderTexturedItems items;
    items.setScene(&scene);

    // Empty geometry: registerPickable is never called, so the gate never arms
    // and the item renders under plain visibility (the default is visible).
    cwRenderTexturedItems::Item item;
    const uint32_t itemId = items.addItem(item);

    REQUIRE(scene.visibility()->snapshot().subVisible(items.renderObjectId(), itemId));
}

TEST_CASE("Removing and re-adding a textured item re-arms the gate",
          "[cwRenderObject][pickReadyGate]")
{
    cwScene scene;
    cwRenderTexturedItems items;
    items.setScene(&scene);

    cwRenderTexturedItems::Item item;
    item.geometry = triangle();
    const uint32_t itemId = items.addItem(item);
    scene.geometryItersecter()->waitForFinish();
    REQUIRE(scene.visibility()->snapshot().subVisible(items.renderObjectId(), itemId));

    items.removeItem(itemId);

    // A fresh item is a fresh sub-BVH, so its gate arms again: hidden until the
    // new build publishes.
    cwRenderTexturedItems::Item newItem;
    newItem.geometry = triangle();
    const uint32_t newItemId = items.addItem(newItem);
    REQUIRE_FALSE(scene.visibility()->snapshot().subVisible(items.renderObjectId(), newItemId));

    scene.geometryItersecter()->waitForFinish();
    REQUIRE(scene.visibility()->snapshot().subVisible(items.renderObjectId(), newItemId));
}
