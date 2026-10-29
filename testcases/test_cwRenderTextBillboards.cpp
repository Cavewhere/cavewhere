// GUI-thread tests for cwRenderTextBillboards, the one-renderer text layer for
// station labels. Text layout runs on the GUI thread, so everything here is
// observable without a QRhi: the stable-id bookkeeping, the per-string layout
// cache, style invalidation, and the slot snapshot the render thread consumes.
// The render path itself is covered by test_cwRenderTextBillboards_render.cpp.

#include <catch2/catch_test_macros.hpp>

#include <QFont>
#include <QSignalSpy>
#include <QTextLayout>

#include "cwRenderTextBillboards.h"
#include "cwRhiScene.h"
#include "cwScene.h"

#include "CwRhiSceneTestAccess.h"

namespace {

constexpr int kBasePixelSize = 16;
constexpr int kLargePixelSize = 32;
constexpr cwBillboardId kUnknownId{9999};

cwRenderTextBillboards::TextBillboard makeLabel(const QString& text, const QVector3D& position,
                                                float depthBias = 1.0f)
{
    cwRenderTextBillboards::TextBillboard billboard;
    billboard.text = text;
    billboard.worldPosition = position;
    billboard.depthBias = depthBias;
    return billboard;
}

cwRenderTextBillboards::Style makeStyle(int pixelSize)
{
    cwRenderTextBillboards::Style style;
    style.font.setPixelSize(pixelSize);
    style.color = Qt::white;
    style.outlineColor = Qt::black;
    return style;
}

const cwRenderTextBillboards::RenderSlot* slotFor(
    const QVector<cwRenderTextBillboards::RenderSlot>& renderSlots, cwBillboardId id)
{
    for (const cwRenderTextBillboards::RenderSlot& slot : renderSlots) {
        if (slot.id == id) {
            return &slot;
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE("cwRenderTextBillboards: add, update and remove keep stable ids", "[cwRenderTextBillboards]")
{
    cwRenderTextBillboards layer;
    layer.setStyle(makeStyle(kBasePixelSize));

    const cwBillboardId a1 = layer.addBillboard(makeLabel(QStringLiteral("a1"), QVector3D(0, 0, 0)));
    const cwBillboardId a2 = layer.addBillboard(makeLabel(QStringLiteral("a2"), QVector3D(0, 10, 0)));

    REQUIRE(a1 != cwBillboardId{});
    REQUIRE(a2 != cwBillboardId{});
    REQUIRE(a1 != a2);
    REQUIRE(layer.billboardCount() == 2);
    REQUIRE(layer.hasBillboard(a1));
    REQUIRE(layer.hasBillboard(a2));

    SECTION("update keeps the id and changes the snapshot")
    {
        layer.updateBillboard(a1, makeLabel(QStringLiteral("a1b"), QVector3D(1, 2, 3), 2.0f));
        REQUIRE(layer.billboardCount() == 2);
        REQUIRE(layer.hasBillboard(a1));

        const auto renderSlots = layer.buildRenderSlots();
        const auto* slot = slotFor(renderSlots, a1);
        REQUIRE(slot != nullptr);
        REQUIRE(slot->layout != nullptr);
        CHECK(slot->layout->text() == QStringLiteral("a1b"));
        CHECK(slot->worldPosition == QVector3D(1, 2, 3));
        CHECK(slot->depthBias == 2.0f);
    }

    SECTION("remove drops only that billboard and ids stay unique")
    {
        layer.removeBillboard(a1);
        REQUIRE(layer.billboardCount() == 1);
        REQUIRE_FALSE(layer.hasBillboard(a1));
        REQUIRE(layer.hasBillboard(a2));

        const cwBillboardId a3 = layer.addBillboard(makeLabel(QStringLiteral("a3"), QVector3D(10, 10, 0)));
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
        layer.updateBillboard(kUnknownId, makeLabel(QStringLiteral("x"), QVector3D()));
        REQUIRE(layer.billboardCount() == 2);
        REQUIRE_FALSE(layer.hasBillboard(kUnknownId));
    }
}

TEST_CASE("cwRenderTextBillboards: an unchanged update requests no frame", "[cwRenderTextBillboards]")
{
    cwScene scene;
    cwRhiScene rhiScene;

    auto* layer = new cwRenderTextBillboards(&scene);
    layer->setScene(&scene);
    layer->setStyle(makeStyle(kBasePixelSize));

    const cwRenderTextBillboards::TextBillboard label = makeLabel(QStringLiteral("a1"), QVector3D(1, 2, 3));
    const cwBillboardId id = layer->addBillboard(label);
    REQUIRE(id != cwBillboardId{});

    // Drain the pending Add so the next real change queues an Update (and a frame).
    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);

    QSignalSpy frameSpy(&scene, &cwScene::needsRendering);

    layer->updateBillboard(id, label);
    CHECK(frameSpy.count() == 0);

    layer->updateBillboard(id, makeLabel(QStringLiteral("a1"), QVector3D(1, 2, 4)));
    CHECK(frameSpy.count() > 0);
}

TEST_CASE("cwRenderTextBillboards: textSize is measured and layouts are cached per string",
          "[cwRenderTextBillboards]")
{
    cwRenderTextBillboards layer;
    layer.setStyle(makeStyle(kBasePixelSize));

    const QSizeF size = layer.textSize(QStringLiteral("A12"));
    CHECK(size.width() > 0.0);
    CHECK(size.height() > 0.0);

    // A longer string with the same font lays out wider.
    CHECK(layer.textSize(QStringLiteral("A12345678")).width() > size.width());

    // Repeated queries return the cached measurement.
    CHECK(layer.textSize(QStringLiteral("A12")) == size);

    // Two labels with the same text share one layout.
    const cwBillboardId first = layer.addBillboard(makeLabel(QStringLiteral("A12"), QVector3D(0, 0, 0)));
    const cwBillboardId second = layer.addBillboard(makeLabel(QStringLiteral("A12"), QVector3D(5, 0, 0)));
    const cwBillboardId other = layer.addBillboard(makeLabel(QStringLiteral("B7"), QVector3D(9, 0, 0)));

    const auto renderSlots = layer.buildRenderSlots();
    REQUIRE(renderSlots.size() == 3);
    const auto* firstSlot = slotFor(renderSlots, first);
    const auto* secondSlot = slotFor(renderSlots, second);
    const auto* otherSlot = slotFor(renderSlots, other);
    REQUIRE(firstSlot != nullptr);
    REQUIRE(secondSlot != nullptr);
    REQUIRE(otherSlot != nullptr);

    REQUIRE(firstSlot->layout != nullptr);
    CHECK(firstSlot->layout.get() == secondSlot->layout.get());
    CHECK(firstSlot->layout.get() != otherSlot->layout.get());

    // The slot's content size is the measured text size.
    CHECK(firstSlot->contentSize == size);
}

TEST_CASE("cwRenderTextBillboards: a style change invalidates the layouts", "[cwRenderTextBillboards]")
{
    cwRenderTextBillboards layer;
    layer.setStyle(makeStyle(kBasePixelSize));

    const cwBillboardId id = layer.addBillboard(makeLabel(QStringLiteral("A12"), QVector3D(0, 0, 0)));
    const QSizeF baseSize = layer.textSize(QStringLiteral("A12"));

    const auto before = layer.buildRenderSlots();
    const auto* beforeSlot = slotFor(before, id);
    REQUIRE(beforeSlot != nullptr);
    REQUIRE(beforeSlot->layout != nullptr);
    const std::shared_ptr<const QTextLayout> oldLayout = beforeSlot->layout;

    layer.setStyle(makeStyle(kLargePixelSize));
    REQUIRE(layer.style().font.pixelSize() == kLargePixelSize);

    const QSizeF largeSize = layer.textSize(QStringLiteral("A12"));
    CHECK(largeSize.width() > baseSize.width());
    CHECK(largeSize.height() > baseSize.height());

    const auto after = layer.buildRenderSlots();
    const auto* afterSlot = slotFor(after, id);
    REQUIRE(afterSlot != nullptr);
    REQUIRE(afterSlot->layout != nullptr);
    CHECK(afterSlot->layout.get() != oldLayout.get());
    CHECK(afterSlot->layout->font().pixelSize() == kLargePixelSize);
    CHECK(afterSlot->contentSize == largeSize);
}

TEST_CASE("cwRenderTextBillboards: buildRenderSlots carries every billboard", "[cwRenderTextBillboards]")
{
    cwRenderTextBillboards layer;
    layer.setStyle(makeStyle(kBasePixelSize));

    struct Expected {
        QString text;
        QVector3D position;
        float depthBias;
    };
    const QVector<Expected> expected{
        {QStringLiteral("a1"), QVector3D(0, 0, 0), 1.0f},
        {QStringLiteral("a2"), QVector3D(0, 10, 0), 0.5f},
        {QStringLiteral("a3"), QVector3D(10, 10, -2), 0.0f},
    };

    QVector<cwBillboardId> ids;
    for (const Expected& label : expected) {
        ids.append(layer.addBillboard(makeLabel(label.text, label.position, label.depthBias)));
    }

    const auto renderSlots = layer.buildRenderSlots();
    REQUIRE(renderSlots.size() == expected.size());

    for (int i = 0; i < expected.size(); ++i) {
        INFO("label " << expected.at(i).text.toStdString());
        const auto* slot = slotFor(renderSlots, ids.at(i));
        REQUIRE(slot != nullptr);
        REQUIRE(slot->layout != nullptr);
        CHECK(slot->layout->text() == expected.at(i).text);
        CHECK(slot->worldPosition == expected.at(i).position);
        CHECK(slot->depthBias == expected.at(i).depthBias);
        CHECK(slot->contentSize.width() > 0.0);
        CHECK(slot->contentSize.height() > 0.0);
        CHECK(slot->sortDepth == 0.0f);
    }
}

TEST_CASE("cwRenderTextBillboards: removing every billboard empties the snapshot", "[cwRenderTextBillboards]")
{
    cwRenderTextBillboards layer;
    layer.setStyle(makeStyle(kBasePixelSize));

    const cwBillboardId id = layer.addBillboard(makeLabel(QStringLiteral("a1"), QVector3D()));
    REQUIRE(layer.buildRenderSlots().size() == 1);

    layer.removeBillboard(id);
    REQUIRE(layer.billboardCount() == 0);
    REQUIRE(layer.buildRenderSlots().isEmpty());
}

TEST_CASE("cwRenderTextBillboards: buildRenderSlots carries each billboard's pixel offset",
          "[cwRenderTextBillboards]")
{
    const QPointF raised(0.0, 24.0);
    const QPointF shifted(-6.5, 3.25);

    cwRenderTextBillboards layer;
    layer.setStyle(makeStyle(kBasePixelSize));

    cwRenderTextBillboards::TextBillboard raisedLabel = makeLabel(QStringLiteral("a1"), QVector3D(0, 0, 0));
    raisedLabel.pixelOffset = raised;
    const cwBillboardId raisedId = layer.addBillboard(raisedLabel);
    cwRenderTextBillboards::TextBillboard shiftedLabel = makeLabel(QStringLiteral("a2"), QVector3D(0, 10, 0));
    shiftedLabel.pixelOffset = shifted;
    const cwBillboardId shiftedId = layer.addBillboard(shiftedLabel);
    const cwBillboardId plainId = layer.addBillboard(makeLabel(QStringLiteral("a3"), QVector3D(10, 10, 0)));

    const auto renderSlots = layer.buildRenderSlots();
    const auto* raisedSlot = slotFor(renderSlots, raisedId);
    const auto* shiftedSlot = slotFor(renderSlots, shiftedId);
    const auto* plainSlot = slotFor(renderSlots, plainId);
    REQUIRE(raisedSlot != nullptr);
    REQUIRE(shiftedSlot != nullptr);
    REQUIRE(plainSlot != nullptr);
    CHECK(raisedSlot->pixelOffset == raised);
    CHECK(shiftedSlot->pixelOffset == shifted);
    CHECK(plainSlot->pixelOffset == QPointF());
}

TEST_CASE("cwRenderTextBillboards: an offset-only change is an update", "[cwRenderTextBillboards]")
{
    const QPointF raised(0.0, 24.0);

    cwScene scene;
    cwRhiScene rhiScene;

    auto* layer = new cwRenderTextBillboards(&scene);
    layer->setScene(&scene);
    layer->setStyle(makeStyle(kBasePixelSize));

    const cwRenderTextBillboards::TextBillboard label = makeLabel(QStringLiteral("a1"), QVector3D(1, 2, 3));
    const cwBillboardId id = layer->addBillboard(label);
    REQUIRE(id != cwBillboardId{});

    // Drain the pending Add so the next real change queues an Update (and a frame).
    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);
    QSignalSpy frameSpy(&scene, &cwScene::needsRendering);

    cwRenderTextBillboards::TextBillboard moved = label;
    moved.pixelOffset = raised;
    layer->updateBillboard(id, moved);
    CHECK(frameSpy.count() > 0);

    const auto renderSlots = layer->buildRenderSlots();
    const auto* slot = slotFor(renderSlots, id);
    REQUIRE(slot != nullptr);
    CHECK(slot->pixelOffset == raised);
    CHECK(slot->worldPosition == label.worldPosition);
}

TEST_CASE("cwRenderTextBillboards: the depth mode defaults to one depth-tested pass and round-trips",
          "[cwRenderTextBillboards]")
{
    constexpr float kXRayOpacity = 0.3f;
    constexpr float kFullOpacity = 1.0f;
    const cwBillboardDepthMode xRay{true, true, kXRayOpacity};
    const cwBillboardDepthMode overEverything{false, false, kFullOpacity};

    cwRenderTextBillboards layer;
    CHECK(layer.depthMode() == cwBillboardDepthMode());
    CHECK(layer.depthMode().depthTest);
    CHECK_FALSE(layer.depthMode().xRay);

    layer.setDepthMode(xRay);
    CHECK(layer.depthMode() == xRay);

    layer.setDepthMode(overEverything);
    CHECK(layer.depthMode() == overEverything);

    layer.setDepthMode(cwBillboardDepthMode());
    CHECK(layer.depthMode() == cwBillboardDepthMode());
}

TEST_CASE("cwRenderTextBillboards: a depth mode change asks for a frame and an unchanged one does not",
          "[cwRenderTextBillboards]")
{
    constexpr float kXRayOpacity = 0.3f;
    constexpr float kFainterXRayOpacity = 0.1f;
    const cwBillboardDepthMode xRay{true, true, kXRayOpacity};
    const cwBillboardDepthMode fainterXRay{true, true, kFainterXRayOpacity};

    cwScene scene;
    cwRhiScene rhiScene;
    auto* layer = new cwRenderTextBillboards(&scene);
    layer->setScene(&scene);

    // Drain the pending Add so each change below queues its own Update (and a frame).
    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);
    QSignalSpy frameSpy(&scene, &cwScene::needsRendering);

    layer->setDepthMode(xRay);
    CHECK(frameSpy.count() > 0);

    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);
    frameSpy.clear();
    layer->setDepthMode(xRay);
    CHECK(frameSpy.count() == 0);

    // An opacity change alone is a change too.
    layer->setDepthMode(fainterXRay);
    CHECK(frameSpy.count() > 0);
}
