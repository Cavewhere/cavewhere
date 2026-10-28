// GUI-thread tests for cwRenderSpriteBillboards, the instanced sprite layer for
// lead markers. Everything here is observable without a QRhi: the stable-id
// bookkeeping, the instance snapshot the render thread uploads, the atlas slots
// with their shared size and subscene wiring. The render path itself is covered
// by test_cwRenderSpriteBillboards_render.cpp.

#include <catch2/catch_test_macros.hpp>

#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QStringList>

#include <memory>
#include <vector>

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
const QSizeF kWiderSpriteSize(40.0, 20.0);

// Mirrors kMaxSprites in cwRenderSpriteBillboards.cpp, the atlas slot cap.
constexpr int kMaxSprites = 16;

constexpr int kOpenSlot = 0;
constexpr int kSecondSlot = 1;
constexpr int kMissingSlot = 2;
constexpr int kNegativeSlot = -1;

// Keeps the warnings logged while it lives; other messages pass through.
class WarningCapture
{
public:
    WarningCapture()
    {
        warnings().clear();
        previousHandler() = qInstallMessageHandler(&WarningCapture::keep);
    }

    ~WarningCapture()
    {
        qInstallMessageHandler(previousHandler());
        previousHandler() = nullptr;
    }

    WarningCapture(const WarningCapture&) = delete;
    WarningCapture& operator=(const WarningCapture&) = delete;

    static int count() { return int(warnings().size()); }
    static void clear() { warnings().clear(); }
    static std::string joined() { return warnings().join(QStringLiteral("\n")).toStdString(); }

private:
    static void keep(QtMsgType type, const QMessageLogContext& context, const QString& message)
    {
        if (type == QtWarningMsg) {
            warnings().append(message);
        }
        if (previousHandler() != nullptr) {
            previousHandler()(type, context, message);
        }
    }

    static QStringList& warnings()
    {
        static QStringList kept;
        return kept;
    }

    static QtMessageHandler& previousHandler()
    {
        static QtMessageHandler handler = nullptr;
        return handler;
    }
};

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

cwRenderSpriteBillboards::SpriteBillboard makeSlotBillboard(const QVector3D& position, int spriteIndex)
{
    cwRenderSpriteBillboards::SpriteBillboard billboard = makeBillboard(position);
    billboard.spriteIndex = spriteIndex;
    return billboard;
}

std::unique_ptr<QQuickItem> makePrototype(QQuickWindow& window, const QSizeF& size)
{
    auto prototype = std::make_unique<QQuickItem>(window.contentItem());
    prototype->setSize(size);
    return prototype;
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
    CHECK(layer.spriteRootNodeHandle(0) == 0);

    // With a sprite and a window the prototype is referenced into the window's
    // scene graph, and the handle builds the node subtree itself when no sync has
    // reached it yet, so it is set at once (and stable across reads).
    layer.setWindow(&window);
    CHECK(layer.window() == &window);
    const quintptr firstHandle = layer.spriteRootNodeHandle(0);
    CHECK(firstHandle != 0);
    CHECK(layer.spriteRootNodeHandle(0) == firstHandle);

    SECTION("swapping the sprite re-references the new item")
    {
        auto other = std::make_unique<QQuickItem>(window.contentItem());
        other->setSize(kLargerSpriteSize);
        layer.setSprite(other.get());
        CHECK(layer.sprite() == other.get());
        CHECK(layer.spriteSize() == kLargerSpriteSize);
        const quintptr otherHandle = layer.spriteRootNodeHandle(0);
        CHECK(otherHandle != 0);
        CHECK(otherHandle != firstHandle);

        layer.setSprite(nullptr);
        CHECK(layer.sprite() == nullptr);
        CHECK(layer.spriteRootNodeHandle(0) == 0);
    }

    SECTION("unbinding the window releases the reference and keeps the sprite")
    {
        layer.setWindow(nullptr);
        CHECK(layer.window() == nullptr);
        CHECK(layer.sprite() == prototype.get());
        CHECK(layer.spriteRootNodeHandle(0) == 0);

        layer.setWindow(&window);
        CHECK(layer.sprite() == prototype.get());
        CHECK(layer.spriteRootNodeHandle(0) != 0);
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

TEST_CASE("cwRenderSpriteBillboards: setSprites declares the atlas slots in order",
          "[cwRenderSpriteBillboards]")
{
    QQuickWindow window;
    auto open = makePrototype(window, kSpriteSize);
    auto second = makePrototype(window, kSpriteSize);

    cwRenderSpriteBillboards layer;
    layer.setWindow(&window);
    CHECK(layer.spriteCount() == 0);

    layer.setSprites({open.get(), second.get()});
    CHECK(layer.spriteCount() == 2);
    CHECK(layer.sprites() == QList<QQuickItem*>({open.get(), second.get()}));
    CHECK(layer.sprite() == open.get());
    CHECK(layer.spriteSize() == kSpriteSize);

    // Each slot is referenced into the window's scene graph on its own.
    const quintptr openHandle = layer.spriteRootNodeHandle(kOpenSlot);
    const quintptr secondHandle = layer.spriteRootNodeHandle(kSecondSlot);
    CHECK(openHandle != 0);
    CHECK(secondHandle != 0);
    CHECK(secondHandle != openHandle);
    CHECK(layer.spriteRootNodeHandle(kMissingSlot) == 0);
    CHECK(layer.spriteRootNodeHandle(kNegativeSlot) == 0);

    SECTION("unbinding the window releases every slot's reference")
    {
        layer.setWindow(nullptr);
        CHECK(layer.spriteCount() == 2);
        CHECK(layer.spriteRootNodeHandle(kOpenSlot) == 0);
        CHECK(layer.spriteRootNodeHandle(kSecondSlot) == 0);

        layer.setWindow(&window);
        CHECK(layer.spriteRootNodeHandle(kSecondSlot) != 0);
    }

    SECTION("a destroyed prototype keeps its slot so the other indices stay put")
    {
        open.reset();
        CHECK(layer.spriteCount() == 2);
        CHECK(layer.sprites() == QList<QQuickItem*>({nullptr, second.get()}));
        CHECK(layer.sprite() == nullptr);
        CHECK(layer.spriteSize() == QSizeF());
        CHECK(layer.spriteRootNodeHandle(kOpenSlot) == 0);
        CHECK(layer.spriteRootNodeHandle(kSecondSlot) != 0);
    }
}

TEST_CASE("cwRenderSpriteBillboards: setSprite is setSprites with one slot", "[cwRenderSpriteBillboards]")
{
    QQuickWindow window;
    auto prototype = makePrototype(window, kSpriteSize);

    cwRenderSpriteBillboards single;
    cwRenderSpriteBillboards listed;
    single.setWindow(&window);
    listed.setWindow(&window);

    single.setSprite(prototype.get());
    listed.setSprites({prototype.get()});
    CHECK(single.spriteCount() == 1);
    CHECK(listed.spriteCount() == 1);
    CHECK(single.sprites() == listed.sprites());
    CHECK(single.sprite() == listed.sprite());
    CHECK(single.spriteSize() == listed.spriteSize());

    // Clearing through either form leaves no slots and is quiet.
    WarningCapture warnings;
    single.setSprite(nullptr);
    listed.setSprites({});
    CHECK(single.spriteCount() == 0);
    CHECK(listed.spriteCount() == 0);
    CHECK(single.sprites().isEmpty());
    INFO("warnings: " << WarningCapture::joined());
    CHECK(WarningCapture::count() == 0);
}

TEST_CASE("cwRenderSpriteBillboards: every slot shares slot 0's size", "[cwRenderSpriteBillboards]")
{
    QQuickWindow window;
    auto open = makePrototype(window, kSpriteSize);
    auto larger = makePrototype(window, kLargerSpriteSize);
    auto wider = makePrototype(window, kWiderSpriteSize);

    cwRenderSpriteBillboards layer;
    layer.setWindow(&window);

    WarningCapture warnings;
    layer.setSprites({open.get(), larger.get()});
    CHECK(layer.spriteCount() == 2);
    CHECK(layer.spriteSize() == kSpriteSize);
    INFO("warnings: " << WarningCapture::joined());
    CHECK(WarningCapture::count() == 1);

    // The mismatch is reported once per store.
    WarningCapture::clear();
    layer.setSprites({open.get(), wider.get()});
    CHECK(layer.spriteCount() == 2);
    CHECK(layer.spriteSize() == kSpriteSize);
    CHECK(WarningCapture::count() == 0);
}

TEST_CASE("cwRenderSpriteBillboards: setSprites skips null entries and caps the slot count",
          "[cwRenderSpriteBillboards]")
{
    QQuickWindow window;
    cwRenderSpriteBillboards layer;
    layer.setWindow(&window);

    SECTION("a null entry is dropped with a warning and the rest apply in order")
    {
        auto open = makePrototype(window, kSpriteSize);
        auto second = makePrototype(window, kSpriteSize);

        WarningCapture warnings;
        layer.setSprites({open.get(), nullptr, second.get()});
        CHECK(layer.spriteCount() == 2);
        CHECK(layer.sprites() == QList<QQuickItem*>({open.get(), second.get()}));
        CHECK(WarningCapture::count() > 0);
    }

    SECTION("a list over the cap is truncated with a warning")
    {
        std::vector<std::unique_ptr<QQuickItem>> prototypes;
        QList<QQuickItem*> list;
        for (int i = 0; i < kMaxSprites + 1; ++i) {
            prototypes.push_back(makePrototype(window, kSpriteSize));
            list.append(prototypes.back().get());
        }

        WarningCapture warnings;
        layer.setSprites(list);
        CHECK(layer.spriteCount() == kMaxSprites);
        CHECK(layer.sprites() == list.first(kMaxSprites));
        CHECK(WarningCapture::count() > 0);
    }
}

TEST_CASE("cwRenderSpriteBillboards: a slot change asks for a frame and an unchanged list does not",
          "[cwRenderSpriteBillboards]")
{
    QQuickWindow window;
    auto open = makePrototype(window, kSpriteSize);
    auto second = makePrototype(window, kSpriteSize);
    auto replacement = makePrototype(window, kSpriteSize);

    cwScene scene;
    cwRhiScene rhiScene;
    auto* layer = new cwRenderSpriteBillboards(&scene);
    layer->setScene(&scene);
    layer->setWindow(&window);
    layer->setSprites({open.get(), second.get()});
    CHECK(layer->spriteCount() == 2);

    // Drain the pending Add so each change below queues its own Update (and a frame).
    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);
    QSignalSpy frameSpy(&scene, &cwScene::needsRendering);

    layer->setSprites({open.get(), second.get()});
    CHECK(frameSpy.count() == 0);

    layer->setSprites({open.get(), replacement.get()});
    CHECK(frameSpy.count() > 0);
    CHECK(layer->sprites() == QList<QQuickItem*>({open.get(), replacement.get()}));

    // A later slot's resize re-snapshots too, since it drives that slot's layer.
    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);
    frameSpy.clear();
    replacement->setSize(kLargerSpriteSize);
    CHECK(frameSpy.count() > 0);
}

TEST_CASE("cwRenderSpriteBillboards: buildInstances carries each billboard's slot and drops a missing one",
          "[cwRenderSpriteBillboards]")
{
    QQuickWindow window;
    auto open = makePrototype(window, kSpriteSize);
    auto second = makePrototype(window, kSpriteSize);

    cwRenderSpriteBillboards layer;
    layer.setWindow(&window);
    layer.setSprites({open.get(), second.get()});
    CHECK(layer.spriteCount() == 2);

    layer.addBillboard(makeSlotBillboard(QVector3D(0, 0, 0), kOpenSlot));
    layer.addBillboard(makeSlotBillboard(QVector3D(0, 10, 0), kSecondSlot));
    layer.addBillboard(makeSlotBillboard(QVector3D(0, 20, 0), kMissingSlot));
    layer.addBillboard(makeSlotBillboard(QVector3D(0, 30, 0), kNegativeSlot));
    REQUIRE(layer.billboardCount() == 4);

    WarningCapture warnings;
    const auto instances = layer.buildInstances();
    REQUIRE(instances.size() == 2);
    CHECK(instances.at(0).worldPosition == QVector3D(0, 0, 0));
    CHECK(instances.at(0).spriteIndex == uint32_t(kOpenSlot));
    CHECK(instances.at(1).worldPosition == QVector3D(0, 10, 0));
    CHECK(instances.at(1).spriteIndex == uint32_t(kSecondSlot));
    CHECK(WarningCapture::count() == 1);

    // The store warns once, not on every snapshot.
    WarningCapture::clear();
    CHECK(layer.buildInstances().size() == 2);
    CHECK(WarningCapture::count() == 0);

    // Dropping a slot drops the billboards that name it.
    layer.setSprites({open.get()});
    const auto oneSlot = layer.buildInstances();
    REQUIRE(oneSlot.size() == 1);
    CHECK(oneSlot.at(0).spriteIndex == uint32_t(kOpenSlot));
}

TEST_CASE("cwRenderSpriteBillboards: the anchor defaults to the center and round-trips",
          "[cwRenderSpriteBillboards]")
{
    const QPointF center(0.5, 0.5);
    const QPointF bottomCenter(0.5, 1.0);

    cwRenderSpriteBillboards layer;
    CHECK(layer.anchor() == center);

    layer.setAnchor(bottomCenter);
    CHECK(layer.anchor() == bottomCenter);

    layer.setAnchor(center);
    CHECK(layer.anchor() == center);
}

TEST_CASE("cwRenderSpriteBillboards: an anchor change asks for a frame and an unchanged one does not",
          "[cwRenderSpriteBillboards]")
{
    const QPointF bottomCenter(0.5, 1.0);

    cwScene scene;
    cwRhiScene rhiScene;
    auto* layer = new cwRenderSpriteBillboards(&scene);
    layer->setScene(&scene);

    // Drain the pending Add so each change below queues its own Update (and a frame).
    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);
    QSignalSpy frameSpy(&scene, &cwScene::needsRendering);

    layer->setAnchor(bottomCenter);
    CHECK(frameSpy.count() > 0);

    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);
    frameSpy.clear();
    layer->setAnchor(bottomCenter);
    CHECK(frameSpy.count() == 0);
}
