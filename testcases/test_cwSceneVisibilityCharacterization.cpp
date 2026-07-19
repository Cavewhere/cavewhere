/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Phase 0 of plans/SCENE_VISIBILITY_STORE_PLAN.html (issue #579): pin today's
// visibility behavior in the seams the store refactor will rewire, so Phases
// 1-3 are provably behavior-preserving. Three thin-coverage areas:
//
//   1. Whole-object visibility handoff to the RHI backend (pull-at-sync via
//      cwRhiScene::syncRenderObject). Observed through the stub harness from
//      test_cwScene_issue512.cpp — CPU-only, no QRhi. Note: the observable
//      here (cwRHIObject::isVisible after a sync) is the render thread's view
//      of visibility; Phase 3 replaces the mirror with snapshot reads and
//      re-expresses these assertions against the snapshot.
//
//   2. Visibility and geometry seeded BEFORE scene attach. Today the owner's
//      authoring state survives (front-state flag, object flag, line-plot
//      mask) but pick registration is silently dropped — addItem/setGeometry
//      only reach the intersecter when a scene is already wired, and nothing
//      re-registers on attach (cwRenderTexturedItems.cpp logs "picker WILL
//      NOT see this item"). These tests pin both halves: the drop, and the
//      recovery paths that re-register post-attach.
//
//   3. A keyword-pipeline burst (the tracker's initial full-model pass — the
//      same shape as a modelReset re-resolve) reaching every consumer
//      coherently through the real producer chain: model -> cwKeywordVisibility
//      -> cwRenderTexturedItemVisibility proxy -> setVisible(id).
//
// Per-item and per-vertex RENDER gating (cwRhiTexturedItems::gather,
// cwRHILinePlot's visibility vertex buffer) is not CPU-observable through
// production surfaces — gather() needs live RenderData — so its end-to-end
// coverage stays with the QML suite; the intersecter-side twins pinned here
// and in the two pick-visibility test files are the C++ net.

#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwGeometry.h"
#include "cwGeometryItersecter.h"
#include "cwKeywordItem.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordVisibility.h"
#include "cwPickQuery.h"
#include "cwRHIObject.h"
#include "cwRayHit.h"
#include "cwRenderLinePlot.h"
#include "cwRenderObject.h"
#include "cwRenderTexturedItems.h"
#include "cwRenderTexturedItemVisibility.h"
#include "cwRhiScene.h"
#include "cwScene.h"

#include "CwRhiSceneTestAccess.h"

//Qt includes
#include <QVector2D>
#include <QVector3D>
#include <QVector>

//QMath3d
#include <qray3d.h>

using namespace Catch;

namespace {

// Items sit one per row along Y, far enough apart that a pick aimed at one is
// unambiguous. Same fixture shape as test_cwRenderTexturedItemsPickVisibility.
constexpr float kItemSpacing = 100.0f;
constexpr float kTriangleExtent = 10.0f;

cwGeometry triangleAt(float y)
{
    cwGeometry geometry({
        { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 },
        { cwGeometry::Semantic::TexCoord0, cwGeometry::AttributeFormat::Vec2 }
    });

    geometry.resizeVertices(3);
    const auto* positionAttribute = geometry.attribute(cwGeometry::Semantic::Position);
    const auto* texCoordAttribute = geometry.attribute(cwGeometry::Semantic::TexCoord0);

    const QVector<QVector3D> positions = {
        QVector3D(-kTriangleExtent, y - kTriangleExtent, 0.0f),
        QVector3D( kTriangleExtent, y - kTriangleExtent, 0.0f),
        QVector3D( 0.0f,            y + kTriangleExtent, 0.0f),
    };
    for (int i = 0; i < positions.size(); ++i) {
        geometry.set(positionAttribute, i, positions.at(i));
        geometry.set(texCoordAttribute, i, QVector2D(0.0f, 0.0f));
    }

    geometry.setIndices({0u, 1u, 2u});
    geometry.setType(cwGeometry::Type::Triangles);
    return geometry;
}

// Ray straight down -Z through the center of the item at row `index`.
QRay3D rayThroughItem(int index)
{
    return QRay3D(QVector3D(0.0f, index * kItemSpacing, 50.0f),
                  QVector3D(0.0f, 0.0f, -1.0f));
}

uint32_t addTriangleItem(cwRenderTexturedItems& items, int index)
{
    cwRenderTexturedItems::Item item;
    item.geometry = triangleAt(index * kItemSpacing);
    return items.addItem(item);
}

bool picks(cwScene& scene, int index, const cwPickQuery& query = cwPickQuery())
{
    return scene.geometryItersecter()->intersectsDetailed(
        rayThroughItem(index), query).hit();
}

// Line-plot fixture: two shots (non-indexed line list, a shot owns vertices
// [2i, 2i+1]) on rows 0 and 1.
QVector<QVector3D> twoShots()
{
    return {
        QVector3D(-10.0f, 0.0f,         0.0f),
        QVector3D( 10.0f, 0.0f,         0.0f),
        QVector3D(-10.0f, kItemSpacing, 0.0f),
        QVector3D( 10.0f, kItemSpacing, 0.0f),
    };
}

// The centerline is 1-D, so line picks need a proximity radius.
constexpr double kLinePickToleranceConstant = 1.0;

cwPickQuery linePickQuery()
{
    cwPickQuery query;
    query.kinds = cwPickQuery::Kind::Lines;
    query.tolerance.constant = kLinePickToleranceConstant;
    return query;
}

// CPU-only stubs, same shape as test_cwScene_issue512.cpp: no GPU work, so
// cwRhiScene::synchroize needs no QRhi.
class StubRhiObject : public cwRHIObject
{
public:
    void initialize(const ResourceUpdateData&) override {}
    void synchronize(const SynchronizeData&) override {}
    void updateResources(const ResourceUpdateData&) override {}
    void render(const RenderData&) override {}
};

class StubRenderObject : public cwRenderObject
{
protected:
    cwRHIObject* createRHIObject() override { return new StubRhiObject(); }
};

} // namespace

// A render object hidden BEFORE it joins a scene must reach the render thread
// hidden: syncRenderObject pulls object->isVisible() when the Add drains, so
// the pre-attach flag rides the first sync. Also pins that hidden means
// gather-skipped, not unregistered — the backend still creates and maps the
// cwRHIObject.
TEST_CASE("visibility characterization: whole-object hide seeded before scene attach reaches the RHI at first sync",
          "[SceneVisibility][cwScene][cwRhiScene]")
{
    cwScene scene;
    cwRhiScene rhiScene;

    auto* object = new StubRenderObject();
    object->setVisible(false);              // seeded pre-attach
    object->setScene(&scene);
    object->setParent(nullptr);             // we manage its lifetime, not the scene

    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);

    cwRHIObject* rhiObject =
        CwRhiSceneTestAccess::renderObjectForId(rhiScene, object->renderObjectId());
    REQUIRE(rhiObject != nullptr);          // hidden != unregistered
    CHECK_FALSE(rhiObject->isVisible());

    delete object;
    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);   // frees the stub RHI object
}

// A whole-object toggle reaches the render thread's view of visibility at the
// next sync, in both directions. (Deliberately no assertion on the pre-sync
// window: nothing reads the RHI mirror between syncs, and the pull-at-sync
// mechanism is what the store refactor's Phase 3 replaces.)
TEST_CASE("visibility characterization: whole-object toggles propagate to the RHI at the next sync",
          "[SceneVisibility][cwScene][cwRhiScene]")
{
    cwScene scene;
    cwRhiScene rhiScene;

    auto* object = new StubRenderObject();
    object->setScene(&scene);
    object->setParent(nullptr);             // we manage its lifetime, not the scene

    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);

    cwRHIObject* rhiObject =
        CwRhiSceneTestAccess::renderObjectForId(rhiScene, object->renderObjectId());
    REQUIRE(rhiObject != nullptr);
    REQUIRE(rhiObject->isVisible());

    object->setVisible(false);
    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);
    CHECK_FALSE(rhiObject->isVisible());

    object->setVisible(true);
    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);
    CHECK(rhiObject->isVisible());

    delete object;
    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);   // frees the stub RHI object
}

// Items added before the scene is wired never register with the picker —
// geometryItersecter() is null at addItem time and nothing re-registers on
// attach. The authoring state survives on the owner (front-state flag), so
// the first post-attach updateGeometry both registers the geometry AND
// re-seeds the hidden flag. This pins the drop and the recovery path.
TEST_CASE("visibility characterization: textured items added before scene attach are dropped from picking until a post-attach geometry update",
          "[SceneVisibility][cwRenderTexturedItems]")
{
    cwScene scene;

    cwRenderTexturedItems items;            // no scene yet
    const uint32_t id = addTriangleItem(items, 0);
    items.setVisible(id, false);            // pre-attach seed, kept in front state

    items.setScene(&scene);
    scene.geometryItersecter()->waitForFinish();

    // The pre-attach add never reached the picker: no geometry registered at
    // all — not "registered but hidden".
    CHECK(scene.geometryItersecter()->boundingBox().isNull());
    CHECK(scene.visibleFramingBounds().isNull());
    CHECK_FALSE(picks(scene, 0));

    // Recovery: a post-attach geometry update registers the item and re-seeds
    // visibility from the surviving front-state flag — it comes back hidden.
    items.updateGeometry(id, triangleAt(0));
    scene.geometryItersecter()->waitForFinish();

    REQUIRE_FALSE(scene.geometryItersecter()->boundingBox().isNull());
    CHECK_FALSE(picks(scene, 0));
    CHECK(scene.visibleFramingBounds().isNull());

    items.setVisible(id, true);
    CHECK(picks(scene, 0));
}

// The whole-object flag lives on the render object, not in the intersecter,
// and traversal reads it live — so a pre-attach whole-object hide gates
// picking as soon as geometry registers post-attach. Contrast with the per-id
// registration drop above: pre-attach STATE survives; pre-attach
// REGISTRATION does not.
TEST_CASE("visibility characterization: whole-object hide seeded before scene attach gates picking after attach",
          "[SceneVisibility][cwRenderTexturedItems]")
{
    cwScene scene;

    cwRenderTexturedItems items;            // no scene yet
    items.cwRenderObject::setVisible(false);

    items.setScene(&scene);
    addTriangleItem(items, 0);              // post-attach: registers
    scene.geometryItersecter()->waitForFinish();

    REQUIRE_FALSE(scene.geometryItersecter()->boundingBox().isNull());
    CHECK_FALSE(picks(scene, 0));
    CHECK(scene.visibleFramingBounds().isNull());

    items.cwRenderObject::setVisible(true);
    CHECK(picks(scene, 0));
    CHECK_FALSE(scene.visibleFramingBounds().isNull());
}

// Line-plot geometry set before the scene is wired never registers with the
// picker, and a pre-attach setRangeVisible publishes no mask. The first
// post-attach setGeometry registers everything all-visible — the reset-mask
// contract pinned in test_cwRenderLinePlotPickVisibility — so the pre-attach
// hidden range is forgotten until the owner re-applies it (which is what
// cwLinePlotManager::reconcileTripKeywordItems does after every solve).
TEST_CASE("visibility characterization: line-plot geometry and ranges set before scene attach are dropped from picking",
          "[SceneVisibility][cwRenderLinePlot]")
{
    cwScene scene;

    cwRenderLinePlot linePlot;              // no scene yet
    linePlot.setGeometry(twoShots());
    linePlot.setRangeVisible(2, 2, false);  // hide shot 1, pre-attach

    linePlot.setScene(&scene);
    scene.geometryItersecter()->waitForFinish();

    // Nothing registered: both shots miss and there is nothing to frame.
    CHECK(scene.geometryItersecter()->boundingBox().isNull());
    CHECK(scene.visibleFramingBounds().isNull());
    CHECK_FALSE(picks(scene, 0, linePickQuery()));
    CHECK_FALSE(picks(scene, 1, linePickQuery()));

    // Recovery: a post-attach setGeometry registers the plot — and resets the
    // mask to all-visible, dropping the pre-attach hidden range.
    linePlot.setGeometry(twoShots());
    scene.geometryItersecter()->waitForFinish();

    CHECK(picks(scene, 0, linePickQuery()));
    CHECK(picks(scene, 1, linePickQuery()));
    CHECK(scene.visibleFramingBounds().maximum().y() >= kItemSpacing);
}

// A keyword-pipeline burst must land coherently on every consumer. Filling
// the hide model BEFORE the tracker sees it makes setHideModel's initial
// full-model pass deliver the toggles — the same many-items-at-once shape a
// modelReset re-resolve produces. Drives the real producer chain
// (cwKeywordVisibility -> cwRenderTexturedItemVisibility -> setVisible(id))
// and asserts picking and framing agree after the burst in each direction.
TEST_CASE("visibility characterization: a keyword hide-model burst hides and restores every item coherently",
          "[SceneVisibility][cwKeywordVisibility][cwRenderTexturedItems]")
{
    constexpr int kItemCount = 8;

    cwScene scene;

    cwRenderTexturedItems items;
    items.setScene(&scene);

    QVector<uint32_t> ids;
    for (int i = 0; i < kItemCount; ++i) {
        ids.append(addTriangleItem(items, i));
    }
    scene.geometryItersecter()->waitForFinish();

    for (int i = 0; i < kItemCount; ++i) {
        REQUIRE(picks(scene, i));
    }

    // One keyword item + proxy per textured item, all in the hide model
    // before the tracker connects.
    cwKeywordItemModel hideModel;
    QVector<cwKeywordItem*> keywordItems;
    for (int i = 0; i < kItemCount; ++i) {
        auto* keywordItem = new cwKeywordItem();
        auto* proxy = new cwRenderTexturedItemVisibility(&items, ids.at(i), keywordItem);
        keywordItem->setObject(proxy);
        hideModel.addItem(keywordItem);     // model takes ownership
        keywordItems.append(keywordItem);
    }

    cwKeywordVisibility visibility;
    visibility.setHideModel(&hideModel);    // initial pass = the burst

    // Coherent hide: every item unpickable, nothing framed, raw registered
    // box unchanged.
    for (int i = 0; i < kItemCount; ++i) {
        CHECK_FALSE(picks(scene, i));
    }
    CHECK(scene.visibleFramingBounds().isNull());
    CHECK(scene.geometryItersecter()->boundingBox().maximum().y()
          >= (kItemCount - 1) * kItemSpacing - kTriangleExtent);

    // Burst back: removing every row re-resolves each item to visible
    // (rowsRemoved handling), restoring picks and the full framing box.
    for (cwKeywordItem* keywordItem : keywordItems) {
        hideModel.removeItem(keywordItem);
    }

    for (int i = 0; i < kItemCount; ++i) {
        CHECK(picks(scene, i));
    }
    CHECK(scene.visibleFramingBounds().maximum().y()
          >= (kItemCount - 1) * kItemSpacing - kTriangleExtent);
}
