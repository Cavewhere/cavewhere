// GUI-thread test for cwBillboardOverlayItem, the shared base that wires an
// overlay item to the scene-owned billboard layers. Exercised through a minimal
// concrete subclass (the base is abstract): no QRhi is needed because every
// effect — fetching a shared layer on first use, pushing a billboard, refetching
// on a scene swap — is observable on the GUI thread through the layers' public
// surfaces. The cross-overlay sharing case is the #538 invariant in miniature.

#include <catch2/catch_test_macros.hpp>

#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>

#include <memory>

#include "cwBillboardOverlayItem.h"
#include "cwBillboardHandle.h"
#include "cwCamera.h"
#include "cwRenderBillboards.h"
#include "cwRenderSpriteBillboards.h"
#include "cwRenderTextBillboards.h"
#include "cwScene.h"
#include "cwSpriteBillboardHandle.h"

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
    using cwBillboardOverlayItem::renderSpriteBillboards;
    using cwBillboardOverlayItem::renderTextBillboards;
    using cwBillboardOverlayItem::ensureRenderSpriteBillboards;
    using cwBillboardOverlayItem::ensureRenderTextBillboards;
    using cwBillboardOverlayItem::setBillboard;
    using cwBillboardOverlayItem::setSpriteBillboard;
    using cwBillboardOverlayItem::setTextBillboard;

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

const QString kPinKey = QStringLiteral("pins");
const QString kOtherKey = QStringLiteral("other");
const QString kExtraTextKey = QStringLiteral("pinStats");
const QSizeF kPrototypeSize(20.0, 20.0);
const QSizeF kResizedPrototypeSize(32.0, 24.0);

std::unique_ptr<QQuickItem> makePrototype(QQuickWindow& window, const QSizeF& size)
{
    auto prototype = std::make_unique<QQuickItem>(window.contentItem());
    prototype->setSize(size);
    return prototype;
}

void appendSprite(cwBillboardOverlayItem& overlay, QQuickItem* prototype)
{
    QQmlListProperty<QQuickItem> sprites = overlay.sprites();
    sprites.append(&sprites, prototype);
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

TEST_CASE("cwBillboardOverlayItem: the first setBillboard fetches the scene's shared layer", "[cwBillboardOverlayItem]")
{
    QQuickWindow window;
    auto content = std::make_unique<QQuickItem>(window.contentItem());
    TestOverlay overlay;
    overlay.setParentItem(window.contentItem());
    // The whole wiring hinges on window() resolving once parented into a window's
    // content item (offscreen, no show()); assert it rather than assume it.
    REQUIRE(overlay.window() == &window);

    cwScene scene;
    REQUIRE(overlay.renderBillboards() == nullptr);

    // Attaching the scene fetches nothing: the item layer exists only for
    // overlays that draw QQuickItem content.
    overlay.setScene(&scene);
    REQUIRE(overlay.renderBillboards() == nullptr);
    REQUIRE(scene.findChildren<cwRenderBillboards*>().isEmpty());

    cwBillboardHandle handle;
    overlay.setBillboard(handle, content.get(), QVector3D(1, 2, 3), 1.0f);
    REQUIRE(overlay.renderBillboards() == scene.billboardLayer());
    REQUIRE(scene.billboardLayer()->window() == &window);
}

TEST_CASE("cwBillboardOverlayItem: an overlay that never calls setBillboard leaves the scene without an item layer", "[cwBillboardOverlayItem]")
{
    // A sprite-only overlay (the lead view) must cost the scene no per-item
    // renderers: pushing sprites fetches the sprite layer and nothing else. (A
    // text overlay in the same scene pulls in an empty item layer for draw
    // order; that is pinned in test_cwScene_billboardLayer.cpp.)
    QQuickWindow window;
    TestOverlay overlay;
    overlay.setParentItem(window.contentItem());

    cwScene scene;
    overlay.setScene(&scene);

    cwSpriteBillboardHandle handle;
    overlay.setSpriteBillboard(handle, QVector3D(1, 2, 3), 1.0f, 1.0f, true);
    REQUIRE(handle.isValid());
    REQUIRE(overlay.renderSpriteBillboards() == scene.spriteBillboardLayer());

    CHECK(overlay.renderBillboards() == nullptr);
    CHECK(scene.findChildren<cwRenderBillboards*>().isEmpty());
}

TEST_CASE("cwBillboardOverlayItem: overlays sharing a scene share one layer", "[cwBillboardOverlayItem]")
{
    // The #538 fix in miniature: two item-drawing overlays in the same scene
    // must resolve to the SAME billboard layer so their billboards sort
    // back-to-front together.
    QQuickWindow window;
    auto contentA = std::make_unique<QQuickItem>(window.contentItem());
    auto contentB = std::make_unique<QQuickItem>(window.contentItem());
    TestOverlay first;
    TestOverlay second;
    first.setParentItem(window.contentItem());
    second.setParentItem(window.contentItem());

    cwScene scene;
    first.setScene(&scene);
    second.setScene(&scene);

    cwBillboardHandle handleA;
    cwBillboardHandle handleB;
    first.setBillboard(handleA, contentA.get(), QVector3D(1, 2, 3), 1.0f);
    second.setBillboard(handleB, contentB.get(), QVector3D(4, 5, 6), 1.0f);

    REQUIRE(first.renderBillboards() != nullptr);
    REQUIRE(first.renderBillboards() == second.renderBillboards());
    REQUIRE(scene.billboardLayer()->billboardCount() == 2);
}

TEST_CASE("cwBillboardOverlayItem: setSpriteBillboard adds then updates", "[cwBillboardOverlayItem]")
{
    QQuickWindow window;
    TestOverlay overlay;
    overlay.setParentItem(window.contentItem());

    cwScene scene;
    overlay.setScene(&scene);

    // Attaching the scene fetches nothing; the first push fetches the sprite
    // layer and binds it to the window, the way setBillboard does for items.
    REQUIRE(overlay.renderSpriteBillboards() == nullptr);
    REQUIRE(scene.findChildren<cwRenderSpriteBillboards*>().isEmpty());

    cwSpriteBillboardHandle handle;
    const float depthBias = 1.5f;
    overlay.setSpriteBillboard(handle, QVector3D(1, 2, 3), depthBias, 1.0f, true);
    REQUIRE(handle.isValid());

    cwRenderSpriteBillboards* layer = overlay.renderSpriteBillboards();
    REQUIRE(layer != nullptr);
    REQUIRE(layer == scene.spriteBillboardLayer());
    REQUIRE(layer->window() == &window);
    REQUIRE(overlay.ensureRenderSpriteBillboards() == layer);
    REQUIRE(layer->billboardCount() == 1);
    REQUIRE(layer->hasBillboard(handle.id()));

    auto instances = layer->buildInstances();
    REQUIRE(instances.size() == 1);
    CHECK(instances.at(0).worldPosition == QVector3D(1, 2, 3));
    CHECK(instances.at(0).depthBias == depthBias);
    CHECK(instances.at(0).opacity == 1.0f);

    // A second push updates the same instance in place.
    const float fadedOpacity = 0.6f;
    const cwBillboardId id = handle.id();
    overlay.setSpriteBillboard(handle, QVector3D(4, 5, 6), depthBias, fadedOpacity, true);
    REQUIRE(handle.id() == id);
    REQUIRE(layer->billboardCount() == 1);
    instances = layer->buildInstances();
    REQUIRE(instances.size() == 1);
    CHECK(instances.at(0).worldPosition == QVector3D(4, 5, 6));
    CHECK(instances.at(0).opacity == fadedOpacity);

    // Hiding keeps the instance in the layer and out of the draw.
    overlay.setSpriteBillboard(handle, QVector3D(4, 5, 6), depthBias, fadedOpacity, false);
    REQUIRE(layer->billboardCount() == 1);
    CHECK(layer->buildInstances().isEmpty());
}

TEST_CASE("cwBillboardOverlayItem: setSpriteBillboard carries the atlas slot", "[cwBillboardOverlayItem]")
{
    QQuickWindow window;
    auto openSprite = std::make_unique<QQuickItem>(window.contentItem());
    auto secondSprite = std::make_unique<QQuickItem>(window.contentItem());
    const QSizeF spriteSize(20.0, 20.0);
    openSprite->setSize(spriteSize);
    secondSprite->setSize(spriteSize);

    TestOverlay overlay;
    overlay.setParentItem(window.contentItem());

    cwScene scene;
    overlay.setScene(&scene);

    cwRenderSpriteBillboards* layer = scene.spriteBillboardLayer();
    layer->setSprites({openSprite.get(), secondSprite.get()});
    CHECK(layer->spriteCount() == 2);

    const int secondSlot = 1;
    cwSpriteBillboardHandle defaulted;
    cwSpriteBillboardHandle second;
    overlay.setSpriteBillboard(defaulted, QVector3D(1, 2, 3), 1.0f, 1.0f, true);
    overlay.setSpriteBillboard(second, QVector3D(4, 5, 6), 1.0f, 1.0f, true, secondSlot);
    REQUIRE(layer->billboardCount() == 2);

    // Instances upload in id order, so the defaulted one comes first.
    auto instances = layer->buildInstances();
    REQUIRE(instances.size() == 2);
    CHECK(instances.at(0).spriteIndex == 0u);
    CHECK(instances.at(1).worldPosition == QVector3D(4, 5, 6));
    CHECK(instances.at(1).spriteIndex == uint32_t(secondSlot));

    // Changing only the slot is an update of the same instance.
    const cwBillboardId id = second.id();
    overlay.setSpriteBillboard(second, QVector3D(4, 5, 6), 1.0f, 1.0f, true, 0);
    REQUIRE(second.id() == id);
    instances = layer->buildInstances();
    REQUIRE(instances.size() == 2);
    CHECK(instances.at(1).spriteIndex == 0u);
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
}

TEST_CASE("cwBillboardOverlayItem: a scene swap refetches the new scene's layer", "[cwBillboardOverlayItem]")
{
    QQuickWindow window;
    auto content = std::make_unique<QQuickItem>(window.contentItem());
    TestOverlay overlay;
    overlay.setParentItem(window.contentItem());

    cwScene sceneA;
    cwScene sceneB;

    overlay.setScene(&sceneA);
    cwBillboardHandle handle;
    overlay.setBillboard(handle, content.get(), QVector3D(1, 2, 3), 1.0f);
    cwRenderBillboards* layerA = overlay.renderBillboards();
    REQUIRE(layerA == sceneA.billboardLayer());

    // The swap drops the old layer; the next push fetches the new scene's.
    overlay.setScene(&sceneB);
    REQUIRE(overlay.renderBillboards() == nullptr);
    overlay.setBillboard(handle, content.get(), QVector3D(1, 2, 3), 1.0f);
    REQUIRE(overlay.renderBillboards() == sceneB.billboardLayer());
    REQUIRE(overlay.renderBillboards() != layerA);
    // The handle moved its billboard across with it.
    REQUIRE(sceneA.billboardLayer()->billboardCount() == 0);
    REQUIRE(sceneB.billboardLayer()->billboardCount() == 1);
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
    cwBillboardHandle handle;

    {
        QQuickWindow window1;
        auto content = std::make_unique<QQuickItem>(window1.contentItem());
        overlay.setParentItem(window1.contentItem());
        overlay.setScene(&scene);
        overlay.setBillboard(handle, content.get(), QVector3D(1, 2, 3), 1.0f);
        REQUIRE(overlay.renderBillboards() == scene.billboardLayer());
        REQUIRE(scene.billboardLayer()->window() == &window1);
        // The content lives in window1, so release its billboard before the
        // window goes away.
        handle.reset();
    }

    // Re-parenting into a second window must drive the shared layer's window to
    // the new one through itemChange's already-have-a-layer branch.
    QQuickWindow window2;
    overlay.setParentItem(window2.contentItem());
    REQUIRE(overlay.renderBillboards() == scene.billboardLayer());
    REQUIRE(scene.billboardLayer()->window() == &window2);
}

TEST_CASE("cwBillboardOverlayItem: an overlay's sprites reach its keyed sprite layer in order",
          "[cwBillboardOverlayItem]")
{
    QQuickWindow window;
    auto first = makePrototype(window, kPrototypeSize);
    auto second = makePrototype(window, kPrototypeSize);
    cwScene scene;

    SECTION("listed before the scene arrives")
    {
        TestOverlay overlay(kPinKey);
        REQUIRE(overlay.layerKey() == kPinKey);
        overlay.setParentItem(window.contentItem());
        appendSprite(overlay, first.get());
        appendSprite(overlay, second.get());

        overlay.setScene(&scene);

        cwRenderSpriteBillboards* layer = scene.spriteBillboardLayer(kPinKey);
        CHECK(layer->sprites() == QList<QQuickItem*>({first.get(), second.get()}));
        CHECK(layer->window() == &window);
        CHECK(overlay.renderSpriteBillboards() == layer);
        CHECK(scene.spriteBillboardLayer()->sprites().isEmpty());
    }

    SECTION("listed after the scene arrives")
    {
        TestOverlay overlay(kPinKey);
        overlay.setParentItem(window.contentItem());
        overlay.setScene(&scene);

        QSignalSpy spritesSpy(&overlay, &cwBillboardOverlayItem::spritesChanged);
        appendSprite(overlay, first.get());
        appendSprite(overlay, second.get());
        CHECK(spritesSpy.count() > 0);

        cwRenderSpriteBillboards* layer = scene.spriteBillboardLayer(kPinKey);
        CHECK(layer->sprites() == QList<QQuickItem*>({first.get(), second.get()}));
        CHECK(scene.spriteBillboardLayer()->sprites().isEmpty());
    }
}

TEST_CASE("cwBillboardOverlayItem: spriteSize follows slot 0", "[cwBillboardOverlayItem]")
{
    QQuickWindow window;
    auto first = makePrototype(window, kPrototypeSize);
    auto second = makePrototype(window, kResizedPrototypeSize);

    TestOverlay overlay(kPinKey);
    overlay.setParentItem(window.contentItem());
    cwScene scene;
    overlay.setScene(&scene);

    CHECK(overlay.spriteSize() == QSizeF());

    appendSprite(overlay, first.get());
    appendSprite(overlay, second.get());
    CHECK(overlay.spriteSize() == kPrototypeSize);

    QSignalSpy sizeSpy(&overlay, &cwBillboardOverlayItem::spriteSizeChanged);
    first->setSize(kResizedPrototypeSize);
    CHECK(sizeSpy.count() > 0);
    CHECK(overlay.spriteSize() == kResizedPrototypeSize);
}

TEST_CASE("cwBillboardOverlayItem: overlays on different keys hand their own lists to their own layers",
          "[cwBillboardOverlayItem]")
{
    QQuickWindow window;
    auto pinOpen = makePrototype(window, kPrototypeSize);
    auto pinClosed = makePrototype(window, kPrototypeSize);
    auto other = makePrototype(window, kResizedPrototypeSize);

    cwScene scene;
    TestOverlay pins(kPinKey);
    TestOverlay others(kOtherKey);
    pins.setParentItem(window.contentItem());
    others.setParentItem(window.contentItem());
    pins.setScene(&scene);
    others.setScene(&scene);

    appendSprite(pins, pinOpen.get());
    appendSprite(pins, pinClosed.get());
    appendSprite(others, other.get());

    cwRenderSpriteBillboards* pinLayer = scene.spriteBillboardLayer(kPinKey);
    cwRenderSpriteBillboards* otherLayer = scene.spriteBillboardLayer(kOtherKey);
    REQUIRE(pinLayer != otherLayer);
    CHECK(pinLayer->sprites() == QList<QQuickItem*>({pinOpen.get(), pinClosed.get()}));
    CHECK(otherLayer->sprites() == QList<QQuickItem*>({other.get()}));
    CHECK(pinLayer->spriteSize() == kPrototypeSize);
    CHECK(otherLayer->spriteSize() == kResizedPrototypeSize);
    CHECK(pins.spriteSize() == kPrototypeSize);
    CHECK(others.spriteSize() == kResizedPrototypeSize);
}

TEST_CASE("cwBillboardOverlayItem: a keyed overlay pushes billboards into its keyed layers",
          "[cwBillboardOverlayItem]")
{
    QQuickWindow window;
    TestOverlay overlay(kPinKey);
    overlay.setParentItem(window.contentItem());
    cwScene scene;
    overlay.setScene(&scene);

    cwSpriteBillboardHandle handle;
    overlay.setSpriteBillboard(handle, QVector3D(1, 2, 3), 1.0f, 1.0f, true);
    REQUIRE(handle.isValid());
    CHECK(overlay.renderSpriteBillboards() == scene.spriteBillboardLayer(kPinKey));
    CHECK(scene.spriteBillboardLayer(kPinKey)->billboardCount() == 1);
    CHECK(scene.spriteBillboardLayer()->billboardCount() == 0);

    cwRenderTextBillboards* textLayer = overlay.ensureRenderTextBillboards();
    CHECK(textLayer == scene.textBillboardLayer(kPinKey));
    CHECK(textLayer != scene.textBillboardLayer());
    CHECK(textLayer->window() == &window);
}

TEST_CASE("cwBillboardOverlayItem: a destroyed overlay hands its layer an empty list",
          "[cwBillboardOverlayItem]")
{
    QQuickWindow window;
    auto first = makePrototype(window, kPrototypeSize);
    cwScene scene;

    auto overlay = std::make_unique<TestOverlay>(kPinKey);
    overlay->setParentItem(window.contentItem());
    overlay->setScene(&scene);
    appendSprite(*overlay, first.get());

    cwRenderSpriteBillboards* layer = scene.spriteBillboardLayer(kPinKey);
    REQUIRE(layer->spriteCount() == 1);

    overlay.reset();
    CHECK(layer->spriteCount() == 0);
}

TEST_CASE("cwBillboardOverlayItem: a further keyed text layer is wired like the overlay's own",
          "[cwBillboardOverlayItem]")
{
    cwScene scene;
    TestOverlay overlay(kPinKey);
    cwTextBillboardHandle handle;

    {
        QQuickWindow window1;
        overlay.setParentItem(window1.contentItem());
        overlay.setScene(&scene);

        // The overlay's own key gives its own text layer.
        CHECK(overlay.ensureRenderTextBillboards(kPinKey) == overlay.ensureRenderTextBillboards());

        cwRenderTextBillboards* extra = overlay.ensureRenderTextBillboards(kExtraTextKey);
        REQUIRE(extra == scene.textBillboardLayer(kExtraTextKey));
        CHECK(extra != scene.textBillboardLayer(kPinKey));
        CHECK(extra->window() == &window1);

        overlay.setTextBillboard(extra, handle, QStringLiteral("B7"), QVector3D(1, 2, 3), 1.0f,
                                 QPointF(0.0, 5.0));
        CHECK(handle.isValid());
        CHECK(extra->billboardCount() == 1);
        CHECK(scene.textBillboardLayer(kPinKey)->billboardCount() == 0);
        handle.reset();
        CHECK(extra->billboardCount() == 0);
    }

    // Moving to a second window rebinds the further layer with no new fetch.
    QQuickWindow window2;
    overlay.setParentItem(window2.contentItem());
    CHECK(scene.textBillboardLayer(kExtraTextKey)->window() == &window2);

    // A scene swap fetches the further layer from the new scene.
    cwScene otherScene;
    overlay.setScene(&otherScene);
    cwRenderTextBillboards* swapped = overlay.ensureRenderTextBillboards(kExtraTextKey);
    CHECK(swapped == otherScene.textBillboardLayer(kExtraTextKey));
    CHECK(swapped->window() == &window2);

    // A null layer is a no-op.
    overlay.setTextBillboard(nullptr, handle, QStringLiteral("B7"), QVector3D(1, 2, 3), 1.0f);
    CHECK_FALSE(handle.isValid());
}
