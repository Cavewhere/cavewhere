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

uint32_t addTriangleItem(cwRenderTexturedItems& items, float y)
{
    cwRenderTexturedItems::Item item;
    item.geometry = triangleAt(y);
    return items.addItem(item);
}

} // namespace

// KNOWN BUG (same class as issue #575, textured-item flavor). Per-id
// visibility (cwRenderTexturedItems::setVisible(id, ...)) is render-side only
// and never reaches cwGeometryItersecter. Every scrap shares ONE
// cwRenderTexturedItems (cwScrapManager::m_renderScraps), one item id per
// scrap, and keyword-hiding a scrap toggles only that id — the shared parent
// render object stays visible, so the intersecter's whole-object gate
// (isPickable -> parent->isVisible()) never fires. A pick aimed at a HIDDEN
// scrap still returns a hit. glTF/polycam models (cwNoteLiDARManager) hide the
// same way, as a group of ids.
//
// Unlike the line plot, each item id is already registered as its own
// intersecter Key{parent, id}, so the fix is a per-Key visible flag, not a
// per-primitive mask (see plans/INTERSECTER_VISIBILITY_MASK_PLAN.html).
//
// This test asserts the DESIRED behavior and is tagged [!shouldfail]: Catch2
// keeps the run green while it fails, and flags it (unexpected pass) once the
// bug is fixed so the tag can be removed.
TEST_CASE("cwRenderTexturedItems hidden item must not be pickable",
          "[cwRenderTexturedItems][!shouldfail]")
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
    items.setVisible(hiddenId, false);

    // Desired: the hidden item is no longer pickable. Currently fails — the
    // hit still lands on item B at y == kSecondItemY.
    const cwRayHit hit = scene.geometryItersecter()->intersectsDetailed(
        rayThroughSecondItem(), cwPickQuery());
    CHECK_FALSE(hit.hit());
}

// KNOWN BUG (issue #549, per-scrap flavor). The bounds resetView frames come
// from the intersecter, which today has no visibility filter at all — a hidden
// scrap still inflates the framed box, so reset zooms out to include geometry
// the user hid. The planned fix routes framing through a visible-only bounds
// query that skips per-Key-hidden items; when it lands, point this test at
// that API and drop [!shouldfail].
TEST_CASE("cwRenderTexturedItems hidden item must not inflate framing bounds",
          "[cwRenderTexturedItems][!shouldfail]")
{
    cwScene scene;

    cwRenderTexturedItems items;
    items.setScene(&scene);
    addTriangleItem(items, 0.0f);
    const uint32_t hiddenId = addTriangleItem(items, kSecondItemY);

    // Baseline: both items contribute — the box reaches item B.
    REQUIRE(scene.geometryItersecter()->boundingBox().maximum().y()
            >= kSecondItemY - kTriangleExtent);

    items.setVisible(hiddenId, false);

    // Desired: framing bounds shrink to item A (max y ~= kTriangleExtent).
    // Currently fails — the box still spans to the hidden item B.
    const QBox3D box = scene.geometryItersecter()->boundingBox();
    CHECK(box.maximum().y() < kSecondItemY - kTriangleExtent);
}

// Contrast: whole render-object visibility IS the gate the intersecter honors
// for picking, so hiding the entire items object makes the same ray miss. This
// is the mechanism per-id hiding is missing.
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

    // The per-id overload hides the base-class setVisible(bool); qualify to
    // reach the whole-object gate.
    items.cwRenderObject::setVisible(false);

    const cwRayHit hit = scene.geometryItersecter()->intersectsDetailed(
        rayThroughSecondItem(), cwPickQuery());
    CHECK_FALSE(hit.hit());
}
