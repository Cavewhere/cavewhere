// cwScene::billboardLayer() returns the one shared item layer for the scene.
// Every overlay that draws QQuickItem content feeds this single layer so their
// billboards sort back-to-front together (#538); it is created when such an
// overlay pushes a billboard, or by the text layer to pin the draw order (the
// item layer draws first). The accessor's wiring runs entirely on the GUI
// thread, so no QRhi is needed here. The text layer (station labels) gets the
// same treatment, plus its draw order relative to the item and sprite layers,
// read from the render-thread registry after a CPU-only synchroize. The sprite
// layer (lead markers) is created only when requested and registers before the
// text layer whichever is requested first.

#include <catch2/catch_test_macros.hpp>

#include <QQuickItem>
#include <QQuickWindow>

#include <memory>

#include "cwBillboardHandle.h"
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
    using cwBillboardOverlayItem::setBillboard;

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

    // Requesting the text layer pulls the item and sprite layers in first, so
    // they register (and gather) ahead of it whichever is asked for later.
    REQUIRE(scene.findChildren<cwRenderBillboards*>().size() == 1);
    REQUIRE(scene.findChildren<cwRenderSpriteBillboards*>().size() == 1);

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
    // The layer drawn second is clipped by the first layer's quad depth, so item
    // billboards must draw before labels (text layer) whichever overlay asked
    // for its layer first.
    QQuickWindow window;
    auto content = std::make_unique<QQuickItem>(window.contentItem());
    TextLayerOverlay labelLike;
    TextLayerOverlay itemLike;
    labelLike.setParentItem(window.contentItem());
    itemLike.setParentItem(window.contentItem());

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
        // The label overlay attaches and fetches its text layer before the
        // item-drawing overlay pushes its first billboard.
        labelLike.setScene(&scene);
        cwRenderTextBillboards* textLayer = labelLike.ensureRenderTextBillboards();
        REQUIRE(textLayer != nullptr);
        itemLike.setScene(&scene);
        cwBillboardHandle handle;
        itemLike.setBillboard(handle, content.get(), QVector3D(1, 2, 3), 1.0f);
        cwRenderBillboards* itemLayer = itemLike.renderBillboards();
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

namespace {
const QString kPinKey = QStringLiteral("pins");
const QString kOtherKey = QStringLiteral("other");
}

TEST_CASE("cwScene keyed billboard layers are one scene-owned layer per key", "[cwScene]")
{
    cwScene scene;

    cwRenderSpriteBillboards* pinSprites = scene.spriteBillboardLayer(kPinKey);
    cwRenderSpriteBillboards* otherSprites = scene.spriteBillboardLayer(kOtherKey);
    REQUIRE(pinSprites != nullptr);
    REQUIRE(otherSprites != nullptr);
    CHECK(pinSprites != otherSprites);
    CHECK(pinSprites != scene.spriteBillboardLayer());
    CHECK(scene.spriteBillboardLayer(kPinKey) == pinSprites);
    CHECK(pinSprites->parent() == &scene);
    CHECK(pinSprites->scene() == &scene);

    cwRenderTextBillboards* pinText = scene.textBillboardLayer(kPinKey);
    cwRenderTextBillboards* otherText = scene.textBillboardLayer(kOtherKey);
    REQUIRE(pinText != nullptr);
    REQUIRE(otherText != nullptr);
    CHECK(pinText != otherText);
    CHECK(pinText != scene.textBillboardLayer());
    CHECK(scene.textBillboardLayer(kPinKey) == pinText);
    CHECK(pinText->parent() == &scene);
    CHECK(pinText->scene() == &scene);

    // The empty key is the default layer the no-argument forms return.
    CHECK(scene.spriteBillboardLayer(QString()) == scene.spriteBillboardLayer());
    CHECK(scene.textBillboardLayer(QString()) == scene.textBillboardLayer());
}

TEST_CASE("cwScene gathers a keyed sprite layer before its keyed text layer, after the defaults", "[cwScene]")
{
    // Registration order is draw order: a keyed pair draws sprite then text, as
    // the default pair does, and every keyed layer draws after the default ones.
    cwScene scene;
    cwRhiScene rhiScene;

    SECTION("keyed text layer requested first")
    {
        cwRenderTextBillboards* pinText = scene.textBillboardLayer(kPinKey);
        cwRenderSpriteBillboards* pinSprites = scene.spriteBillboardLayer(kPinKey);
        cwRenderSpriteBillboards* defaultSprites = scene.spriteBillboardLayer();
        cwRenderTextBillboards* defaultText = scene.textBillboardLayer();

        CwRhiSceneTestAccess::synchroize(rhiScene, &scene);

        const int pinSpriteIndex = gatherIndex(rhiScene, pinSprites);
        const int pinTextIndex = gatherIndex(rhiScene, pinText);
        const int defaultSpriteIndex = gatherIndex(rhiScene, defaultSprites);
        const int defaultTextIndex = gatherIndex(rhiScene, defaultText);
        REQUIRE(pinSpriteIndex >= 0);
        REQUIRE(pinTextIndex >= 0);
        REQUIRE(defaultSpriteIndex >= 0);
        REQUIRE(defaultTextIndex >= 0);
        CHECK(pinSpriteIndex < pinTextIndex);
        CHECK(defaultSpriteIndex < pinSpriteIndex);
        CHECK(defaultTextIndex < pinSpriteIndex);
    }

    SECTION("keyed sprite layer requested alone, defaults requested later")
    {
        cwRenderSpriteBillboards* pinSprites = scene.spriteBillboardLayer(kPinKey);
        cwRenderSpriteBillboards* defaultSprites = scene.spriteBillboardLayer();
        cwRenderTextBillboards* defaultText = scene.textBillboardLayer();

        CwRhiSceneTestAccess::synchroize(rhiScene, &scene);

        const int pinSpriteIndex = gatherIndex(rhiScene, pinSprites);
        const int defaultSpriteIndex = gatherIndex(rhiScene, defaultSprites);
        const int defaultTextIndex = gatherIndex(rhiScene, defaultText);
        REQUIRE(pinSpriteIndex >= 0);
        REQUIRE(defaultSpriteIndex >= 0);
        REQUIRE(defaultTextIndex >= 0);
        CHECK(defaultSpriteIndex < pinSpriteIndex);
        CHECK(defaultTextIndex < pinSpriteIndex);
    }
}
