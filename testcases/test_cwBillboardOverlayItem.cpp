// GUI-thread test for cwBillboardOverlayItem, the shared base that wires an
// overlay item to the scene-owned billboard layer. Exercised through a minimal
// concrete subclass (the base is abstract): no QRhi is needed because every
// effect — fetching the shared layer, pushing a billboard, refetching on a scene
// swap — is observable on the GUI thread through cwRenderBillboards' public
// surface. The cross-overlay sharing case is the #538 invariant in miniature.

#include <catch2/catch_test_macros.hpp>

#include <QQuickItem>
#include <QQuickWindow>

#include "cwBillboardOverlayItem.h"
#include "cwBillboardHandle.h"
#include "cwCamera.h"
#include "cwRenderBillboards.h"
#include "cwScene.h"

#include <QMatrix4x4>

namespace {
// Concrete subclass that exposes the protected helpers to the test and records
// how often the base asked it to reproject. The counter lives only in the test
// subclass, never in production API.
class TestOverlay : public cwBillboardOverlayItem
{
public:
    using cwBillboardOverlayItem::cwBillboardOverlayItem;
    using cwBillboardOverlayItem::renderBillboards;
    using cwBillboardOverlayItem::requestBillboardRender;
    using cwBillboardOverlayItem::setBillboard;

    int repositionCount = 0;

protected:
    void repositionBillboards() override { ++repositionCount; }
};

cwRenderBillboards::Billboard makeBillboard(QQuickItem* content)
{
    cwRenderBillboards::Billboard billboard;
    billboard.content = content;
    billboard.worldPosition = QVector3D(1, 2, 3);
    return billboard;
}

// A distinct view matrix per call so cwCamera::setViewMatrix's equality guard
// doesn't swallow the change and the viewMatrixChanged signal actually fires.
QMatrix4x4 translatedView(float x)
{
    QMatrix4x4 m;
    m.translate(x, 0, 0);
    return m;
}
}

TEST_CASE("cwBillboardOverlayItem: setScene fetches the scene's shared layer", "[cwBillboardOverlayItem]")
{
    QQuickWindow window;
    TestOverlay overlay;
    overlay.setParentItem(window.contentItem());
    // The whole wiring hinges on window() resolving once parented into a window's
    // content item (offscreen, no show()); assert it rather than assume it.
    REQUIRE(overlay.window() == &window);

    cwScene scene;
    REQUIRE(overlay.renderBillboards() == nullptr);

    overlay.setScene(&scene);
    REQUIRE(overlay.renderBillboards() == scene.billboardLayer());
}

TEST_CASE("cwBillboardOverlayItem: overlays sharing a scene share one layer", "[cwBillboardOverlayItem]")
{
    // The #538 fix in miniature: a lead-like and a label-like overlay in the same
    // scene must resolve to the SAME billboard layer so their billboards sort
    // back-to-front together.
    QQuickWindow window;
    TestOverlay leadLike;
    TestOverlay labelLike;
    leadLike.setParentItem(window.contentItem());
    labelLike.setParentItem(window.contentItem());

    cwScene scene;
    leadLike.setScene(&scene);
    labelLike.setScene(&scene);

    REQUIRE(leadLike.renderBillboards() != nullptr);
    REQUIRE(leadLike.renderBillboards() == labelLike.renderBillboards());
}

TEST_CASE("cwBillboardOverlayItem: setBillboard pushes into the shared layer", "[cwBillboardOverlayItem]")
{
    QQuickWindow window;
    auto content = std::make_unique<QQuickItem>(window.contentItem());

    TestOverlay overlay;
    overlay.setParentItem(window.contentItem());

    cwScene scene;
    overlay.setScene(&scene);

    cwBillboardHandle handle;
    overlay.setBillboard(handle, content.get(), QVector3D(1, 2, 3), 1.0f);

    REQUIRE(handle.isValid());
    REQUIRE(scene.billboardLayer()->billboardCount() == 1);
    REQUIRE(scene.billboardLayer()->hasBillboard(handle.id()));
}

TEST_CASE("cwBillboardOverlayItem: setBillboard is a no-op without a scene", "[cwBillboardOverlayItem]")
{
    QQuickWindow window;
    auto content = std::make_unique<QQuickItem>(window.contentItem());

    TestOverlay overlay;
    overlay.setParentItem(window.contentItem());
    // No scene set, so there is no layer to push into.

    cwBillboardHandle handle;
    overlay.setBillboard(handle, content.get(), QVector3D(1, 2, 3), 1.0f);
    REQUIRE_FALSE(handle.isValid());

    // requestBillboardRender must not crash when there is no layer.
    overlay.requestBillboardRender();
}

TEST_CASE("cwBillboardOverlayItem: a scene swap refetches the new scene's layer", "[cwBillboardOverlayItem]")
{
    QQuickWindow window;
    TestOverlay overlay;
    overlay.setParentItem(window.contentItem());

    cwScene sceneA;
    cwScene sceneB;

    overlay.setScene(&sceneA);
    cwRenderBillboards* layerA = overlay.renderBillboards();
    REQUIRE(layerA == sceneA.billboardLayer());

    overlay.setScene(&sceneB);
    REQUIRE(overlay.renderBillboards() == sceneB.billboardLayer());
    REQUIRE(overlay.renderBillboards() != layerA);
}

TEST_CASE("cwBillboardOverlayItem: setScene asks the subclass to reposition", "[cwBillboardOverlayItem]")
{
    QQuickWindow window;
    TestOverlay overlay;
    overlay.setParentItem(window.contentItem());

    cwScene scene;
    const int before = overlay.repositionCount;
    overlay.setScene(&scene);
    REQUIRE(overlay.repositionCount > before);
}

TEST_CASE("cwBillboardOverlayItem: becoming visible repositions, hiding does not", "[cwBillboardOverlayItem]")
{
    QQuickWindow window;
    TestOverlay overlay;
    overlay.setParentItem(window.contentItem());

    cwScene scene;
    overlay.setScene(&scene);

    overlay.setVisible(false);
    const int afterHide = overlay.repositionCount;

    // Going visible reprojects; going hidden must not (the connection guards on
    // isVisible()).
    overlay.setVisible(true);
    REQUIRE(overlay.repositionCount > afterHide);

    const int afterShow = overlay.repositionCount;
    overlay.setVisible(false);
    REQUIRE(overlay.repositionCount == afterShow);
}

TEST_CASE("cwBillboardOverlayItem: setBillboard carries depthBias into the layer", "[cwBillboardOverlayItem]")
{
    QQuickWindow window;
    auto content = std::make_unique<QQuickItem>(window.contentItem());

    TestOverlay overlay;
    overlay.setParentItem(window.contentItem());

    cwScene scene;
    overlay.setScene(&scene);

    cwBillboardHandle handle;
    const float depthBias = 2.5f;
    overlay.setBillboard(handle, content.get(), QVector3D(1, 2, 3), depthBias);

    const auto renderSlots = scene.billboardLayer()->buildRenderSlots();
    REQUIRE(renderSlots.size() == 1);
    REQUIRE(renderSlots.at(0).depthBias == depthBias);
}

TEST_CASE("cwBillboardOverlayItem: a camera change repositions the overlay", "[cwBillboardOverlayItem]")
{
    QQuickWindow window;
    TestOverlay overlay;
    overlay.setParentItem(window.contentItem());

    cwCamera camera;
    overlay.setCamera(&camera);
    REQUIRE(overlay.camera() == &camera);

    const int before = overlay.repositionCount;
    camera.setViewMatrix(translatedView(5));
    REQUIRE(overlay.repositionCount > before);

    const int afterView = overlay.repositionCount;
    camera.setCustomProjection(translatedView(7));
    REQUIRE(overlay.repositionCount > afterView);
}

TEST_CASE("cwBillboardOverlayItem: reassigning the camera disconnects the old one", "[cwBillboardOverlayItem]")
{
    // The reason camera wiring was lifted into the base: cwLabel3dView used to
    // never disconnect the old camera, so a swap left the stale camera still
    // driving relayouts. The single base implementation must disconnect.
    QQuickWindow window;
    TestOverlay overlay;
    overlay.setParentItem(window.contentItem());

    cwCamera cameraA;
    cwCamera cameraB;
    overlay.setCamera(&cameraA);
    overlay.setCamera(&cameraB);

    const int afterSwap = overlay.repositionCount;

    // The old camera is detached: its signals must no longer reposition us.
    cameraA.setViewMatrix(translatedView(11));
    REQUIRE(overlay.repositionCount == afterSwap);

    // The current camera still drives repositioning.
    cameraB.setViewMatrix(translatedView(13));
    REQUIRE(overlay.repositionCount > afterSwap);
}

TEST_CASE("cwBillboardOverlayItem: moving to a new window rebinds the shared layer", "[cwBillboardOverlayItem]")
{
    cwScene scene;
    TestOverlay overlay;

    {
        QQuickWindow window1;
        overlay.setParentItem(window1.contentItem());
        overlay.setScene(&scene);
        REQUIRE(overlay.renderBillboards() == scene.billboardLayer());
        REQUIRE(scene.billboardLayer()->window() == &window1);
    }

    // Re-parenting into a second window must drive the shared layer's window to
    // the new one through itemChange's already-have-a-layer branch.
    QQuickWindow window2;
    overlay.setParentItem(window2.contentItem());
    REQUIRE(overlay.renderBillboards() == scene.billboardLayer());
    REQUIRE(scene.billboardLayer()->window() == &window2);
}
