// GUI-thread tests for cwRenderSpriteBillboards, the instanced sprite layer for
// lead markers. Everything here is observable without a QRhi: the stable-id
// bookkeeping, the instance snapshot the render thread uploads, the prototype's
// size and the subscene wiring. The render path itself is covered by
// test_cwRenderSpriteBillboards_render.cpp.

#include <catch2/catch_test_macros.hpp>

#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>

#include "cwRenderSpriteBillboards.h"
#include "cwRhiScene.h"
#include "cwScene.h"
#include "cwSpriteBillboardHandle.h"

#include "CwRhiSceneTestAccess.h"

namespace {

constexpr cwBillboardId kUnknownId{9999};
constexpr float kBias = 1.0f;
constexpr float kFadedOpacity = 0.6f;
const QSizeF kSpriteSize(20.0, 20.0);
const QSizeF kLargerSpriteSize(32.0, 24.0);

cwRenderSpriteBillboards::SpriteBillboard makeBillboard(const QVector3D& position,
                                                        float depthBias = kBias,
                                                        float opacity = 1.0f,
                                                        bool visible = true)
{
    cwRenderSpriteBillboards::SpriteBillboard billboard;
    billboard.worldPosition = position;
    billboard.depthBias = depthBias;
    billboard.opacity = opacity;
    billboard.visible = visible;
    return billboard;
}

} // namespace

TEST_CASE("cwRenderSpriteBillboards: add, update and remove keep stable ids", "[cwRenderSpriteBillboards]")
{
    cwRenderSpriteBillboards layer;

    const cwBillboardId a1 = layer.addBillboard(makeBillboard(QVector3D(0, 0, 0)));
    const cwBillboardId a2 = layer.addBillboard(makeBillboard(QVector3D(0, 10, 0)));

    REQUIRE(a1 != cwBillboardId{});
    REQUIRE(a2 != cwBillboardId{});
    REQUIRE(a1 != a2);
    REQUIRE(layer.billboardCount() == 2);
    REQUIRE(layer.hasBillboard(a1));
    REQUIRE(layer.hasBillboard(a2));

    SECTION("update keeps the id and changes the snapshot")
    {
        layer.updateBillboard(a1, makeBillboard(QVector3D(1, 2, 3), 2.0f, kFadedOpacity));
        REQUIRE(layer.billboardCount() == 2);
        REQUIRE(layer.hasBillboard(a1));

        const auto instances = layer.buildInstances();
        REQUIRE(instances.size() == 2);
        CHECK(instances.at(0).worldPosition == QVector3D(1, 2, 3));
        CHECK(instances.at(0).depthBias == 2.0f);
        CHECK(instances.at(0).opacity == kFadedOpacity);
    }

    SECTION("remove drops only that billboard and ids stay unique")
    {
        layer.removeBillboard(a1);
        REQUIRE(layer.billboardCount() == 1);
        REQUIRE_FALSE(layer.hasBillboard(a1));
        REQUIRE(layer.hasBillboard(a2));

        const cwBillboardId a3 = layer.addBillboard(makeBillboard(QVector3D(10, 10, 0)));
        REQUIRE(a3 != cwBillboardId{});
        REQUIRE(a3 != a1);
        REQUIRE(a3 != a2);
        REQUIRE(layer.billboardCount() == 2);

        // Removing an unknown or already-removed id is harmless.
        layer.removeBillboard(a1);
        REQUIRE(layer.billboardCount() == 2);
    }

    SECTION("updating an unknown id adds nothing")
    {
        layer.updateBillboard(kUnknownId, makeBillboard(QVector3D()));
        REQUIRE(layer.billboardCount() == 2);
        REQUIRE_FALSE(layer.hasBillboard(kUnknownId));
    }
}

TEST_CASE("cwRenderSpriteBillboards: an unchanged update requests no frame", "[cwRenderSpriteBillboards]")
{
    cwScene scene;
    cwRhiScene rhiScene;

    auto* layer = new cwRenderSpriteBillboards(&scene);
    layer->setScene(&scene);

    const auto billboard = makeBillboard(QVector3D(1, 2, 3));
    const cwBillboardId id = layer->addBillboard(billboard);
    REQUIRE(id != cwBillboardId{});

    // Drain the pending Add so the next real change queues an Update (and a frame).
    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);

    QSignalSpy frameSpy(&scene, &cwScene::needsRendering);

    layer->updateBillboard(id, billboard);
    CHECK(frameSpy.count() == 0);

    layer->updateBillboard(id, makeBillboard(QVector3D(1, 2, 3), kBias, kFadedOpacity));
    CHECK(frameSpy.count() > 0);

    // Hiding is an update too, not a remove.
    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);
    frameSpy.clear();
    layer->updateBillboard(id, makeBillboard(QVector3D(1, 2, 3), kBias, kFadedOpacity, false));
    CHECK(frameSpy.count() > 0);
    CHECK(layer->billboardCount() == 1);
}

TEST_CASE("cwRenderSpriteBillboards: buildInstances carries the visible billboards in id order",
          "[cwRenderSpriteBillboards]")
{
    cwRenderSpriteBillboards layer;

    struct Expected {
        QVector3D position;
        float depthBias;
        float opacity;
        bool visible;
    };
    const QVector<Expected> expected{
        {QVector3D(0, 0, 0), 1.0f, 1.0f, true},
        {QVector3D(0, 10, 0), 0.5f, kFadedOpacity, true},
        {QVector3D(5, 5, 5), 0.0f, 1.0f, false},
        {QVector3D(10, 10, -2), 0.0f, 1.0f, true},
    };

    QVector<cwBillboardId> ids;
    for (const Expected& billboard : expected) {
        ids.append(layer.addBillboard(makeBillboard(billboard.position, billboard.depthBias,
                                                    billboard.opacity, billboard.visible)));
    }
    REQUIRE(layer.billboardCount() == expected.size());

    // The hidden billboard stays in the store and is left out of the upload.
    const auto instances = layer.buildInstances();
    REQUIRE(instances.size() == 3);

    int instance = 0;
    for (int i = 0; i < expected.size(); ++i) {
        if (!expected.at(i).visible) {
            REQUIRE(layer.hasBillboard(ids.at(i)));
            continue;
        }
        INFO("billboard " << i);
        CHECK(instances.at(instance).worldPosition == expected.at(i).position);
        CHECK(instances.at(instance).depthBias == expected.at(i).depthBias);
        CHECK(instances.at(instance).opacity == expected.at(i).opacity);
        ++instance;
    }

    // Showing it again puts it back in place, between its neighbors by id.
    layer.updateBillboard(ids.at(2), makeBillboard(QVector3D(5, 5, 5), 0.0f, 1.0f, true));
    const auto shown = layer.buildInstances();
    REQUIRE(shown.size() == 4);
    CHECK(shown.at(2).worldPosition == QVector3D(5, 5, 5));
    CHECK(shown.at(3).worldPosition == QVector3D(10, 10, -2));

    // Removing a billboard in the middle keeps the others in order.
    layer.removeBillboard(ids.at(1));
    const auto afterRemove = layer.buildInstances();
    REQUIRE(afterRemove.size() == 3);
    CHECK(afterRemove.at(0).worldPosition == QVector3D(0, 0, 0));
    CHECK(afterRemove.at(1).worldPosition == QVector3D(5, 5, 5));
    CHECK(afterRemove.at(2).worldPosition == QVector3D(10, 10, -2));
}

TEST_CASE("cwRenderSpriteBillboards: removing every billboard empties the snapshot",
          "[cwRenderSpriteBillboards]")
{
    cwRenderSpriteBillboards layer;

    const cwBillboardId id = layer.addBillboard(makeBillboard(QVector3D()));
    REQUIRE(layer.buildInstances().size() == 1);

    layer.removeBillboard(id);
    REQUIRE(layer.billboardCount() == 0);
    REQUIRE(layer.buildInstances().isEmpty());
}

TEST_CASE("cwRenderSpriteBillboards: spriteSize follows the prototype", "[cwRenderSpriteBillboards]")
{
    QQuickWindow window;
    auto prototype = std::make_unique<QQuickItem>(window.contentItem());
    prototype->setSize(kSpriteSize);

    cwScene scene;
    cwRhiScene rhiScene;
    auto* layer = new cwRenderSpriteBillboards(&scene);
    layer->setScene(&scene);

    REQUIRE(layer->sprite() == nullptr);
    CHECK(layer->spriteSize() == QSizeF());

    // Drain the pending Add so each change below queues its own Update (and a frame).
    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);

    QSignalSpy frameSpy(&scene, &cwScene::needsRendering);
    layer->setSprite(prototype.get());
    CHECK(frameSpy.count() > 0);
    REQUIRE(layer->sprite() == prototype.get());
    CHECK(layer->spriteSize() == kSpriteSize);

    // A resize of the prototype asks for a frame so the backend re-snapshots it.
    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);
    frameSpy.clear();
    prototype->setSize(kLargerSpriteSize);
    CHECK(layer->spriteSize() == kLargerSpriteSize);
    CHECK(frameSpy.count() > 0);

    // Setting the same prototype again changes nothing.
    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);
    frameSpy.clear();
    layer->setSprite(prototype.get());
    CHECK(frameSpy.count() == 0);

    // A destroyed prototype leaves the layer with no sprite and an empty size.
    prototype.reset();
    CHECK(layer->sprite() == nullptr);
    CHECK(layer->spriteSize() == QSizeF());
}

TEST_CASE("cwRenderSpriteBillboards: the subscene follows the window and the sprite",
          "[cwRenderSpriteBillboards]")
{
    QQuickWindow window;
    auto prototype = std::make_unique<QQuickItem>(window.contentItem());
    prototype->setSize(kSpriteSize);

    cwRenderSpriteBillboards layer;
    layer.setSizeMode(cwBillboardSizeMode::ScreenConstant);
    CHECK(layer.sizeMode() == cwBillboardSizeMode::ScreenConstant);

    // No window yet: the prototype is held, nothing is referenced into a scene
    // graph, so there is no root node to hand out.
    layer.setSprite(prototype.get());
    CHECK(layer.window() == nullptr);
    CHECK(layer.spriteRootNodeHandle() == 0);

    // With a sprite and a window the prototype is referenced into the window's
    // scene graph, and the handle builds the node subtree itself when no sync has
    // reached it yet, so it is set at once (and stable across reads).
    layer.setWindow(&window);
    CHECK(layer.window() == &window);
    const quintptr firstHandle = layer.spriteRootNodeHandle();
    CHECK(firstHandle != 0);
    CHECK(layer.spriteRootNodeHandle() == firstHandle);

    SECTION("swapping the sprite re-references the new item")
    {
        auto other = std::make_unique<QQuickItem>(window.contentItem());
        other->setSize(kLargerSpriteSize);
        layer.setSprite(other.get());
        CHECK(layer.sprite() == other.get());
        CHECK(layer.spriteSize() == kLargerSpriteSize);
        const quintptr otherHandle = layer.spriteRootNodeHandle();
        CHECK(otherHandle != 0);
        CHECK(otherHandle != firstHandle);

        layer.setSprite(nullptr);
        CHECK(layer.sprite() == nullptr);
        CHECK(layer.spriteRootNodeHandle() == 0);
    }

    SECTION("unbinding the window releases the reference and keeps the sprite")
    {
        layer.setWindow(nullptr);
        CHECK(layer.window() == nullptr);
        CHECK(layer.sprite() == prototype.get());
        CHECK(layer.spriteRootNodeHandle() == 0);

        layer.setWindow(&window);
        CHECK(layer.sprite() == prototype.get());
        CHECK(layer.spriteRootNodeHandle() != 0);
    }

    SECTION("the size mode is a store setting")
    {
        layer.setSizeMode(cwBillboardSizeMode::WorldSized);
        CHECK(layer.sizeMode() == cwBillboardSizeMode::WorldSized);
    }

    // The prototype outlives the layer here and the layer outlives the window in
    // other producers; both orders tear down without touching freed state.
}

TEST_CASE("cwRenderSpriteBillboards: a handle reset removes its billboard", "[cwRenderSpriteBillboards]")
{
    cwRenderSpriteBillboards layer;

    {
        cwSpriteBillboardHandle handle;
        REQUIRE_FALSE(handle.isValid());

        handle.set(&layer, makeBillboard(QVector3D(1, 2, 3)));
        REQUIRE(handle.isValid());
        REQUIRE(layer.billboardCount() == 1);
        REQUIRE(layer.hasBillboard(handle.id()));

        // A second set updates in place.
        handle.set(&layer, makeBillboard(QVector3D(4, 5, 6), kBias, kFadedOpacity, false));
        REQUIRE(layer.billboardCount() == 1);
        CHECK(layer.buildInstances().isEmpty());

        handle.reset();
        CHECK_FALSE(handle.isValid());
        CHECK(layer.billboardCount() == 0);

        // Dropping the handle with a billboard removes it too.
        handle.set(&layer, makeBillboard(QVector3D()));
        REQUIRE(layer.billboardCount() == 1);
    }
    CHECK(layer.billboardCount() == 0);
}
