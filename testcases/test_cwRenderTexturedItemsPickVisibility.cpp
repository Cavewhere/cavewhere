//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwScene.h"
#include "cwRenderTexturedItems.h"
#include "cwGeometry.h"
#include "cwGeometryItersecter.h"
#include "cwPickQuery.h"
#include "cwRayHit.h"

//Qt includes
#include <QVector2D>
#include <QVector3D>
#include <QVector>

//QMath3d
#include <qray3d.h>

using namespace Catch;

namespace {

// Each item is one triangle in the z=0 plane, centered on (0, y, 0) and wound
// counter-clockwise (+Z normal) so a ray fired down -Z front-faces it
// (cwGeometry culls backfaces by default). Two items far apart in Y so a pick
// aimed at one is unambiguous:
//   item A at y = 0
//   item B at y = kSecondItemY  (this is the one we hide)
constexpr float kSecondItemY = 100.0f;
constexpr float kTriangleExtent = 10.0f;

// World-space radius for the pivot-anchor query (nearestGeometryPoint needs
// an enabled tolerance to reach anything). Far smaller than the item gap, so
// a hidden item can't be "reached past" to a visible one.
constexpr double kAnchorToleranceConstant = 1.0;

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

// Ray straight down -Z through the center of item B at (0, kSecondItemY, 0).
QRay3D rayThroughSecondItem()
{
    return QRay3D(QVector3D(0.0f, kSecondItemY, 50.0f),
                  QVector3D(0.0f, 0.0f, -1.0f));
}

// Ray straight down -Z through the center of item A at the origin.
QRay3D rayThroughFirstItem()
{
    return QRay3D(QVector3D(0.0f, 0.0f, 50.0f),
                  QVector3D(0.0f, 0.0f, -1.0f));
}

uint32_t addTriangleItem(cwRenderTexturedItems& items, float y)
{
    cwRenderTexturedItems::Item item;
    item.geometry = triangleAt(y);
    return items.addItem(item);
}

} // namespace

// Issue #575 (textured-item flavor). Every scrap shares ONE
// cwRenderTexturedItems (cwScrapManager::m_renderScraps), one item id per
// scrap, and keyword-hiding a scrap toggles only that id — the shared parent
// render object stays visible, so the whole-object gate (isPickable ->
// parent->isVisible()) never fires. setItemVisible(id) now forwards to the
// intersecter's per-Key flag (each item id is its own Key{parent, id}), so a
// hidden scrap takes no picks. glTF/polycam models hide the same way, as a
// group of ids. No BVH rebuild — the toggle is synchronous.
TEST_CASE("cwRenderTexturedItems hidden item is not pickable",
          "[cwRenderTexturedItems]")
{
    cwScene scene;

    cwRenderTexturedItems items;
    items.setScene(&scene);
    addTriangleItem(items, 0.0f);
    const uint32_t hiddenId = addTriangleItem(items, kSecondItemY);
    scene.geometryItersecter()->waitForFinish();

    // Baseline: while visible, item B is pickable.
    REQUIRE(scene.geometryItersecter()->intersectsDetailed(
                rayThroughSecondItem(), cwPickQuery()).hit());

    // Hide item B via the real per-scrap visibility path.
    items.setItemVisible(hiddenId, false);

    CHECK_FALSE(scene.geometryItersecter()->intersectsDetailed(
        rayThroughSecondItem(), cwPickQuery()).hit());

    // The still-visible item A keeps taking picks — hiding one id must not
    // hide its siblings.
    CHECK(scene.geometryItersecter()->intersectsDetailed(
        rayThroughFirstItem(), cwPickQuery()).hit());

    // Re-showing restores pickability.
    items.setItemVisible(hiddenId, true);
    CHECK(scene.geometryItersecter()->intersectsDetailed(
        rayThroughSecondItem(), cwPickQuery()).hit());
}

// The per-Key flag is identity state, not geometry state: replacing a hidden
// item's geometry (a scrap re-triangulating while keyword-hidden) must not
// resurrect it in picking.
TEST_CASE("cwRenderTexturedItems hidden item stays hidden across a geometry update",
          "[cwRenderTexturedItems]")
{
    cwScene scene;

    cwRenderTexturedItems items;
    items.setScene(&scene);
    const uint32_t hiddenId = addTriangleItem(items, kSecondItemY);
    scene.geometryItersecter()->waitForFinish();

    REQUIRE(scene.geometryItersecter()->intersectsDetailed(
                rayThroughSecondItem(), cwPickQuery()).hit());

    items.setItemVisible(hiddenId, false);
    items.updateGeometry(hiddenId, triangleAt(kSecondItemY));
    scene.geometryItersecter()->waitForFinish();

    // The raw box proves the geometry is still registered — the item is
    // hidden, not gone — so the miss below tests the flag, not a dropped
    // update.
    REQUIRE(scene.geometryItersecter()->boundingBox().maximum().y()
            >= kSecondItemY - kTriangleExtent);

    CHECK_FALSE(scene.geometryItersecter()->intersectsDetailed(
        rayThroughSecondItem(), cwPickQuery()).hit());
    CHECK(scene.visibleFramingBounds().isNull());

    // The surviving flag is still attached to a live registration:
    // re-showing resurrects the same Key.
    items.setItemVisible(hiddenId, true);
    CHECK(scene.geometryItersecter()->intersectsDetailed(
        rayThroughSecondItem(), cwPickQuery()).hit());
}

// Issue #549 (per-scrap flavor). Reset framing comes from
// cwScene::visibleFramingBounds(); the per-Key flag keeps a keyword-hidden
// scrap from inflating the framed box, so reset doesn't zoom out to include
// geometry the user hid.
TEST_CASE("cwRenderTexturedItems hidden item does not inflate framing bounds",
          "[cwRenderTexturedItems]")
{
    cwScene scene;

    cwRenderTexturedItems items;
    items.setScene(&scene);
    addTriangleItem(items, 0.0f);
    const uint32_t hiddenId = addTriangleItem(items, kSecondItemY);

    // Baseline: both items contribute — the box reaches item B.
    REQUIRE(scene.visibleFramingBounds().maximum().y()
            >= kSecondItemY - kTriangleExtent);

    items.setItemVisible(hiddenId, false);

    // Framing bounds shrink to exactly item A's apex while the raw
    // registered box keeps counting the hidden geometry. Exact compare is
    // safe: the box is the untransformed union of the fixture's literal
    // vertex positions.
    const QBox3D box = scene.visibleFramingBounds();
    CHECK(box.maximum().y() == kTriangleExtent);
    CHECK(scene.geometryItersecter()->boundingBox().maximum().y()
          >= kSecondItemY - kTriangleExtent);

    // Re-showing restores the full framing box.
    items.setItemVisible(hiddenId, true);
    CHECK(scene.visibleFramingBounds().maximum().y()
          >= kSecondItemY - kTriangleExtent);
}

// A hidden item whose geometry transiently goes EMPTY is unregistered via
// removeObject — which forgets the per-Key flag — and re-registered by the
// next valid update. updateGeometry re-seeds the flag from the render-side
// state, so the item comes back hidden, not silently pickable. (Empty pushes
// are a real path: failed/degenerate scrap triangulation.)
TEST_CASE("cwRenderTexturedItems hidden item stays hidden across an empty geometry cycle",
          "[cwRenderTexturedItems]")
{
    cwScene scene;

    cwRenderTexturedItems items;
    items.setScene(&scene);
    const uint32_t hiddenId = addTriangleItem(items, kSecondItemY);
    scene.geometryItersecter()->waitForFinish();

    REQUIRE(scene.geometryItersecter()->intersectsDetailed(
                rayThroughSecondItem(), cwPickQuery()).hit());

    items.setItemVisible(hiddenId, false);
    items.updateGeometry(hiddenId, cwGeometry());           // empty: unregisters
    items.updateGeometry(hiddenId, triangleAt(kSecondItemY)); // re-registers
    scene.geometryItersecter()->waitForFinish();

    // The raw box proves the geometry re-registered; the item must still be
    // hidden to picking and framing.
    REQUIRE(scene.geometryItersecter()->boundingBox().maximum().y()
            >= kSecondItemY - kTriangleExtent);
    CHECK_FALSE(scene.geometryItersecter()->intersectsDetailed(
        rayThroughSecondItem(), cwPickQuery()).hit());
    CHECK(scene.visibleFramingBounds().isNull());

    items.setItemVisible(hiddenId, true);
    CHECK(scene.geometryItersecter()->intersectsDetailed(
        rayThroughSecondItem(), cwPickQuery()).hit());
}

// An item born hidden (Item::visible = false at addItem) must never be
// pickable — the seed reaches the intersecter with the initial registration.
TEST_CASE("cwRenderTexturedItems item born hidden is not pickable",
          "[cwRenderTexturedItems]")
{
    cwScene scene;

    cwRenderTexturedItems items;
    items.setScene(&scene);

    cwRenderTexturedItems::Item item;
    item.geometry = triangleAt(kSecondItemY);
    item.visible = false;
    const uint32_t id = items.addItem(item);
    scene.geometryItersecter()->waitForFinish();

    CHECK_FALSE(scene.geometryItersecter()->intersectsDetailed(
        rayThroughSecondItem(), cwPickQuery()).hit());
    CHECK(scene.visibleFramingBounds().isNull());

    items.setItemVisible(id, true);
    CHECK(scene.geometryItersecter()->intersectsDetailed(
        rayThroughSecondItem(), cwPickQuery()).hit());
}

// The per-Key flag and the whole-object gate are independent layers:
// re-showing the whole object must not resurrect an individually hidden id.
TEST_CASE("cwRenderTexturedItems per-id hide survives a whole-object hide/show cycle",
          "[cwRenderTexturedItems]")
{
    cwScene scene;

    cwRenderTexturedItems items;
    items.setScene(&scene);
    addTriangleItem(items, 0.0f);
    const uint32_t hiddenId = addTriangleItem(items, kSecondItemY);
    scene.geometryItersecter()->waitForFinish();

    items.setItemVisible(hiddenId, false);
    items.setVisible(false);
    items.setVisible(true);

    CHECK_FALSE(scene.geometryItersecter()->intersectsDetailed(
        rayThroughSecondItem(), cwPickQuery()).hit());
    CHECK(scene.geometryItersecter()->intersectsDetailed(
        rayThroughFirstItem(), cwPickQuery()).hit());
}

// The rotation-pivot anchor (AnchorPick) shares the traversal gate with the
// exact pick, so a hidden item can't capture the pivot either.
TEST_CASE("cwRenderTexturedItems hidden item does not anchor the rotation pivot",
          "[cwRenderTexturedItems]")
{
    cwScene scene;

    cwRenderTexturedItems items;
    items.setScene(&scene);
    const uint32_t hiddenId = addTriangleItem(items, kSecondItemY);
    scene.geometryItersecter()->waitForFinish();

    cwPickQuery query;
    query.tolerance.constant = kAnchorToleranceConstant;

    REQUIRE(scene.geometryItersecter()->nearestGeometryPoint(
                rayThroughSecondItem(), query).has_value());

    items.setItemVisible(hiddenId, false);

    CHECK_FALSE(scene.geometryItersecter()->nearestGeometryPoint(
        rayThroughSecondItem(), query).has_value());
}

// Contrast: whole render-object visibility is its own gate, independent of
// the per-id flag — hiding the entire items object makes the same ray miss
// even though every id stays individually visible.
TEST_CASE("cwRenderTexturedItems whole-object hide removes it from intersecter picks",
          "[cwRenderTexturedItems]")
{
    cwScene scene;

    cwRenderTexturedItems items;
    items.setScene(&scene);
    addTriangleItem(items, 0.0f);
    addTriangleItem(items, kSecondItemY);
    scene.geometryItersecter()->waitForFinish();

    REQUIRE(scene.geometryItersecter()->intersectsDetailed(
                rayThroughSecondItem(), cwPickQuery()).hit());

    items.setVisible(false);

    const cwRayHit hit = scene.geometryItersecter()->intersectsDetailed(
        rayThroughSecondItem(), cwPickQuery());
    CHECK_FALSE(hit.hit());
}

// Phase 1: whole-object visibility filters the framing bounds. The raw
// boundingBox() intentionally keeps counting hidden objects (it describes the
// registered geometry); visibleFramingBounds() is the box cameras frame.
TEST_CASE("cwRenderTexturedItems whole-object hide removes it from the visible framing bounds",
          "[cwRenderTexturedItems]")
{
    cwScene scene;

    cwRenderTexturedItems items;
    items.setScene(&scene);
    addTriangleItem(items, 0.0f);
    addTriangleItem(items, kSecondItemY);

    // Baseline: while visible, the framing bounds span both items.
    REQUIRE(scene.visibleFramingBounds().maximum().y()
            >= kSecondItemY - kTriangleExtent);

    items.setVisible(false);

    // The only render object is hidden, so there is nothing to frame — but
    // the raw box still reports the registered geometry.
    CHECK(scene.visibleFramingBounds().isNull());
    CHECK(scene.geometryItersecter()->boundingBox().maximum().y()
          >= kSecondItemY - kTriangleExtent);
}
