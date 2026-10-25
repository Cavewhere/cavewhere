// cwScene::billboardLayer() returns the one shared overlay layer for the scene.
// Every overlay (leads, station labels, future overlays) feeds this single layer
// so their billboards sort back-to-front together (#538). The accessor's wiring
// runs entirely on the GUI thread, so no QRhi is needed here. The text layer
// (station labels) gets the same treatment, plus its draw order relative to the
// item and sprite layers, read from the render-thread registry after a CPU-only
// synchroize. The sprite layer (lead markers) is created only when requested and
// registers before the text layer whichever is requested first.

#include <catch2/catch_test_macros.hpp>

#include <QQuickWindow>

#include "cwScene.h"
#include "cwRenderBillboards.h"
#include "cwRenderSpriteBillboards.h"
#include "cwRenderTextBillboards.h"
#include "cwBillboardOverlayItem.h"
#include "cwRHIObject.h"
#include "cwRhiScene.h"

#include "CwRhiSceneTestAccess.h"

TEST_CASE("cwScene::billboardLayer returns one shared, scene-owned layer", "[cwScene]")
{
    cwScene scene;

    cwRenderBillboards* layer = scene.billboardLayer();
    REQUIRE(layer != nullptr);

    // Repeated requests hand back the same instance — one layer per scene.
    REQUIRE(scene.billboardLayer() == layer);

    // It's owned by the scene (registered + reparented through addItem), so it
    // tears down with the scene rather than leaking or needing manual deletion.
    REQUIRE(layer->parent() == &scene);
    REQUIRE(layer->scene() == &scene);
}

namespace {
// Minimal concrete overlay that exposes the protected text-layer accessors.
class TextLayerOverlay : public cwBillboardOverlayItem
{
public:
    using cwBillboardOverlayItem::cwBillboardOverlayItem;
    using cwBillboardOverlayItem::renderBillboards;
    using cwBillboardOverlayItem::renderTextBillboards;
    using cwBillboardOverlayItem::ensureRenderTextBillboards;

protected:
    void repositionBillboards() override {}
};

// Position of @a layer's render-thread object in the frame's gather order, or -1
// when it isn't registered.
int gatherIndex(cwRhiScene& rhiScene, cwRenderObject* layer)
{
    cwRHIObject* rhiObject = CwRhiSceneTestAccess::renderObjectForId(rhiScene, layer->renderObjectId());
    return CwRhiSceneTestAccess::renderObjects(rhiScene).indexOf(rhiObject);
}
}

TEST_CASE("cwScene::textBillboardLayer returns one shared, scene-owned layer", "[cwScene]")
{
    cwScene scene;

    cwRenderTextBillboards* layer = scene.textBillboardLayer();
    REQUIRE(layer != nullptr);
    REQUIRE(scene.textBillboardLayer() == layer);

    // A separate layer from the item layer, owned by the scene like it.
    REQUIRE(static_cast<cwRenderObject*>(layer) != static_cast<cwRenderObject*>(scene.billboardLayer()));
    REQUIRE(layer->parent() == &scene);
    REQUIRE(layer->scene() == &scene);
}

TEST_CASE("cwScene::textBillboardLayer is shared by every overlay in the scene", "[cwScene]")
{
    QQuickWindow window;
    TextLayerOverlay first;
    TextLayerOverlay second;
    first.setParentItem(window.contentItem());
    second.setParentItem(window.contentItem());

    cwScene scene;
    first.setScene(&scene);
    second.setScene(&scene);

    REQUIRE(first.renderTextBillboards() == nullptr);

    cwRenderTextBillboards* layer = first.ensureRenderTextBillboards();
    REQUIRE(layer != nullptr);
    REQUIRE(layer == scene.textBillboardLayer());
    REQUIRE(first.renderTextBillboards() == layer);
    REQUIRE(layer->window() == &window);

    REQUIRE(second.ensureRenderTextBillboards() == layer);
    REQUIRE(second.renderTextBillboards() == layer);
}

TEST_CASE("cwScene gathers the item billboard layer before the text layer", "[cwScene]")
{
    // The layer drawn second is clipped by the first layer's quad depth, so leads
    // (item layer) must draw before labels (text layer) whichever overlay asked
    // for its layer first.
    QQuickWindow window;
    TextLayerOverlay labelLike;
    TextLayerOverlay leadLike;
    labelLike.setParentItem(window.contentItem());
    leadLike.setParentItem(window.contentItem());

    cwScene scene;
    cwRhiScene rhiScene;

    SECTION("text layer requested first")
    {
        cwRenderTextBillboards* textLayer = scene.textBillboardLayer();
        REQUIRE(textLayer != nullptr);
        cwRenderBillboards* itemLayer = scene.billboardLayer();

        CwRhiSceneTestAccess::synchroize(rhiScene, &scene);

        const int itemIndex = gatherIndex(rhiScene, itemLayer);
        const int textIndex = gatherIndex(rhiScene, textLayer);
        REQUIRE(itemIndex >= 0);
        REQUIRE(textIndex >= 0);
        CHECK(itemIndex < textIndex);
    }

    SECTION("item layer requested first")
    {
        cwRenderBillboards* itemLayer = scene.billboardLayer();
        cwRenderTextBillboards* textLayer = scene.textBillboardLayer();
        REQUIRE(textLayer != nullptr);

        CwRhiSceneTestAccess::synchroize(rhiScene, &scene);

        const int itemIndex = gatherIndex(rhiScene, itemLayer);
        const int textIndex = gatherIndex(rhiScene, textLayer);
        REQUIRE(itemIndex >= 0);
        REQUIRE(textIndex >= 0);
        CHECK(itemIndex < textIndex);
    }

    SECTION("through the overlays' own wiring")
    {
        // The app's path: the label overlay attaches and fetches its text layer
        // before the lead overlay attaches.
        labelLike.setScene(&scene);
        cwRenderTextBillboards* textLayer = labelLike.ensureRenderTextBillboards();
        REQUIRE(textLayer != nullptr);
        leadLike.setScene(&scene);
        cwRenderBillboards* itemLayer = leadLike.renderBillboards();
        REQUIRE(itemLayer != nullptr);

        CwRhiSceneTestAccess::synchroize(rhiScene, &scene);

        const int itemIndex = gatherIndex(rhiScene, itemLayer);
        const int textIndex = gatherIndex(rhiScene, textLayer);
        REQUIRE(itemIndex >= 0);
        REQUIRE(textIndex >= 0);
        CHECK(itemIndex < textIndex);
    }
}

TEST_CASE("cwScene::spriteBillboardLayer returns one shared, scene-owned layer", "[cwScene]")
{
    cwScene scene;

    cwRenderSpriteBillboards* layer = scene.spriteBillboardLayer();
    REQUIRE(layer != nullptr);
    REQUIRE(scene.spriteBillboardLayer() == layer);
    REQUIRE(layer->parent() == &scene);
    REQUIRE(layer->scene() == &scene);

    // Requesting the sprite layer creates no item layer: a scene with only leads
    // and labels never needs a per-item renderer.
    CHECK(scene.findChildren<cwRenderBillboards*>().isEmpty());
    CHECK(scene.findChildren<cwRenderTextBillboards*>().isEmpty());

    // A separate layer from the others once they exist.
    REQUIRE(static_cast<cwRenderObject*>(layer) != static_cast<cwRenderObject*>(scene.billboardLayer()));
    REQUIRE(static_cast<cwRenderObject*>(layer) != static_cast<cwRenderObject*>(scene.textBillboardLayer()));
}

TEST_CASE("cwScene gathers the sprite layer before the text layer", "[cwScene]")
{
    // Labels depth-test against the lead markers, so the sprite layer must draw
    // first whichever layer is requested first.
    cwScene scene;
    cwRhiScene rhiScene;

    SECTION("text layer requested first")
    {
        cwRenderTextBillboards* textLayer = scene.textBillboardLayer();
        REQUIRE(textLayer != nullptr);
        cwRenderSpriteBillboards* spriteLayer = scene.spriteBillboardLayer();

        CwRhiSceneTestAccess::synchroize(rhiScene, &scene);

        const int spriteIndex = gatherIndex(rhiScene, spriteLayer);
        const int textIndex = gatherIndex(rhiScene, textLayer);
        REQUIRE(spriteIndex >= 0);
        REQUIRE(textIndex >= 0);
        CHECK(spriteIndex < textIndex);
    }

    SECTION("sprite layer requested first")
    {
        cwRenderSpriteBillboards* spriteLayer = scene.spriteBillboardLayer();
        cwRenderTextBillboards* textLayer = scene.textBillboardLayer();
        REQUIRE(textLayer != nullptr);

        CwRhiSceneTestAccess::synchroize(rhiScene, &scene);

        const int spriteIndex = gatherIndex(rhiScene, spriteLayer);
        const int textIndex = gatherIndex(rhiScene, textLayer);
        REQUIRE(spriteIndex >= 0);
        REQUIRE(textIndex >= 0);
        CHECK(spriteIndex < textIndex);
    }

    SECTION("the item layer stays ahead of both")
    {
        cwRenderTextBillboards* textLayer = scene.textBillboardLayer();
        cwRenderSpriteBillboards* spriteLayer = scene.spriteBillboardLayer();
        cwRenderBillboards* itemLayer = scene.billboardLayer();

        CwRhiSceneTestAccess::synchroize(rhiScene, &scene);

        const int itemIndex = gatherIndex(rhiScene, itemLayer);
        const int spriteIndex = gatherIndex(rhiScene, spriteLayer);
        const int textIndex = gatherIndex(rhiScene, textLayer);
        REQUIRE(itemIndex >= 0);
        REQUIRE(spriteIndex >= 0);
        REQUIRE(textIndex >= 0);
        CHECK(itemIndex < spriteIndex);
        CHECK(spriteIndex < textIndex);
    }
}
