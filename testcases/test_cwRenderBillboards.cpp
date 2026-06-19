// GUI-thread lifecycle test for cwRenderBillboards. Without a live QRhi the
// render-thread backend never runs, so this covers the GUI side: the slot
// snapshot it hands to the render thread, that null content is skipped, and that
// clearing the list releases the referenced subscenes cleanly. The full
// materialize-and-occlude path is covered live by the app and the QML/offscreen
// suite.

#include <catch2/catch_test_macros.hpp>

#include <QQuickItem>
#include <QQuickWindow>
#include <QMatrix4x4>

#include "cwRenderBillboards.h"

namespace {
cwRenderBillboards::RenderSlot makeSlot(uint32_t id, const QVector3D& worldPosition)
{
    cwRenderBillboards::RenderSlot slot;
    slot.id = cwBillboardId{id};
    slot.worldPosition = worldPosition;
    return slot;
}
}

TEST_CASE("cwRenderBillboards: snapshot reflects the billboards", "[cwRenderBillboards]")
{
    QQuickWindow window;
    auto content = std::make_unique<QQuickItem>(window.contentItem());
    content->setSize(QSizeF(120, 40));

    cwRenderBillboards billboards;
    billboards.setWindow(&window);

    cwRenderBillboards::Billboard billboard;
    billboard.content = content.get();
    billboard.worldPosition = QVector3D(1, 2, 3);
    billboard.sizeMode = cwRenderBillboards::SizeMode::ScreenConstant;
    billboard.depthBias = 0.5f;
    const cwBillboardId id = billboards.addBillboard(billboard);
    REQUIRE(id != cwBillboardId{});
    REQUIRE(billboards.hasBillboard(id));
    REQUIRE(billboards.billboardCount() == 1);

    const auto renderSlots = billboards.buildRenderSlots();
    REQUIRE(renderSlots.size() == 1);
    REQUIRE(renderSlots.at(0).id == id);
    REQUIRE(renderSlots.at(0).contentSize == QSizeF(120, 40));
    REQUIRE(renderSlots.at(0).worldPosition == QVector3D(1, 2, 3));
    REQUIRE(renderSlots.at(0).sizeMode == cwRenderBillboards::SizeMode::ScreenConstant);
    REQUIRE(renderSlots.at(0).depthBias == 0.5f);

    // No scene-graph sync happens in this CPU-only test, so the content's node
    // subtree is not materialized: the handle stays null (the backend skips it).
    REQUIRE(renderSlots.at(0).rootNodeHandle == 0);
}

TEST_CASE("cwRenderBillboards: updateBillboard changes the snapshot", "[cwRenderBillboards]")
{
    QQuickWindow window;
    auto content = std::make_unique<QQuickItem>(window.contentItem());
    content->setSize(QSizeF(10, 10));

    cwRenderBillboards billboards;
    billboards.setWindow(&window);

    cwRenderBillboards::Billboard billboard;
    billboard.content = content.get();
    billboard.worldPosition = QVector3D(0, 0, 0);
    const cwBillboardId id = billboards.addBillboard(billboard);

    billboard.worldPosition = QVector3D(5, 6, 7);
    billboards.updateBillboard(id, billboard);

    const auto renderSlots = billboards.buildRenderSlots();
    REQUIRE(renderSlots.size() == 1);
    REQUIRE(renderSlots.at(0).id == id);
    REQUIRE(renderSlots.at(0).worldPosition == QVector3D(5, 6, 7));
}

TEST_CASE("cwRenderBillboards: sizeMode and depthBias survive into the snapshot", "[cwRenderBillboards]")
{
    QQuickWindow window;
    auto content = std::make_unique<QQuickItem>(window.contentItem());
    content->setSize(QSizeF(10, 10));

    cwRenderBillboards billboards;
    billboards.setWindow(&window);

    cwRenderBillboards::Billboard billboard;
    billboard.content = content.get();
    billboard.sizeMode = cwRenderBillboards::SizeMode::WorldSized;
    billboard.depthBias = 2.0f;
    const cwBillboardId id = billboards.addBillboard(billboard);

    auto renderSlots = billboards.buildRenderSlots();
    REQUIRE(renderSlots.size() == 1);
    REQUIRE(renderSlots.at(0).sizeMode == cwRenderBillboards::SizeMode::WorldSized);
    REQUIRE(renderSlots.at(0).depthBias == 2.0f);

    // Changing only depthBias must not be swallowed by updateBillboard's no-op
    // equality check (it compares sizeMode and depthBias too).
    billboard.depthBias = 5.0f;
    billboards.updateBillboard(id, billboard);
    renderSlots = billboards.buildRenderSlots();
    REQUIRE(renderSlots.at(0).depthBias == 5.0f);
}

TEST_CASE("cwRenderBillboards: invisible content is skipped", "[cwRenderBillboards]")
{
    QQuickWindow window;
    auto content = std::make_unique<QQuickItem>(window.contentItem());
    content->setSize(QSizeF(20, 20));

    cwRenderBillboards billboards;
    billboards.setWindow(&window);

    cwRenderBillboards::Billboard billboard;
    billboard.content = content.get();
    billboards.addBillboard(billboard);

    // Visible content produces a slot.
    REQUIRE(billboards.buildRenderSlots().size() == 1);

    // A hidden content item must NOT be drawn. The subscene renders the content
    // via its render node, which ignores the visible property, so buildRenderSlots
    // is where visibility is honoured — otherwise a completed, unselected lead
    // marker (visible == false) keeps drawing and overlaps live leads.
    content->setVisible(false);
    REQUIRE(billboards.buildRenderSlots().isEmpty());

    content->setVisible(true);
    REQUIRE(billboards.buildRenderSlots().size() == 1);
}

TEST_CASE("cwRenderBillboards: null content is skipped", "[cwRenderBillboards]")
{
    QQuickWindow window;
    cwRenderBillboards billboards;
    billboards.setWindow(&window);

    cwRenderBillboards::Billboard billboard;  // content left null
    billboards.addBillboard(billboard);

    REQUIRE(billboards.buildRenderSlots().isEmpty());
}

TEST_CASE("cwSortBillboardSlotsBackToFront orders billboards farthest-first", "[cwRenderBillboards]")
{
    // Camera at +z=10 looking down -z, so smaller world z is farther away. The
    // sort must put the farthest billboard first regardless of insertion order,
    // under both projection kinds — the bite only depends on draw order vs. true
    // depth order, which is what this verifies.
    QMatrix4x4 view;
    view.lookAt(QVector3D(0, 0, 10), QVector3D(0, 0, 0), QVector3D(0, 1, 0));

    SECTION("orthographic")
    {
        QMatrix4x4 projection;
        projection.ortho(-10, 10, -10, 10, 0.1f, 100);
        const QMatrix4x4 viewProjection = projection * view;

        QVector<cwRenderBillboards::RenderSlot> renderSlots{
            makeSlot(1, QVector3D(0, 0, 5)),    // nearest
            makeSlot(2, QVector3D(0, 0, -5)),   // farthest
            makeSlot(3, QVector3D(0, 0, 0)),    // middle
        };
        cwSortBillboardSlotsBackToFront(renderSlots, viewProjection);

        REQUIRE(renderSlots.size() == 3);
        REQUIRE(renderSlots.at(0).id == cwBillboardId{2});
        REQUIRE(renderSlots.at(1).id == cwBillboardId{3});
        REQUIRE(renderSlots.at(2).id == cwBillboardId{1});
    }

    SECTION("perspective")
    {
        QMatrix4x4 projection;
        projection.perspective(60.0f, 1.0f, 0.1f, 100.0f);
        const QMatrix4x4 viewProjection = projection * view;

        QVector<cwRenderBillboards::RenderSlot> renderSlots{
            makeSlot(1, QVector3D(2, 0, 0)),     // middle (off-axis)
            makeSlot(2, QVector3D(-3, 0, -8)),   // farthest
            makeSlot(3, QVector3D(1, 0, 7)),     // nearest
        };
        cwSortBillboardSlotsBackToFront(renderSlots, viewProjection);

        REQUIRE(renderSlots.size() == 3);
        REQUIRE(renderSlots.at(0).id == cwBillboardId{2});
        REQUIRE(renderSlots.at(1).id == cwBillboardId{1});
        REQUIRE(renderSlots.at(2).id == cwBillboardId{3});
    }

    SECTION("equal depth resolves to a deterministic id order")
    {
        QMatrix4x4 projection;
        projection.ortho(-10, 10, -10, 10, 0.1f, 100);
        const QMatrix4x4 viewProjection = projection * view;

        // Same depth plane, different x, fed in non-id order. Equal-depth ties
        // resolve by ascending id (a total order), so the result is deterministic
        // regardless of incoming order — no reliance on a stable sort.
        QVector<cwRenderBillboards::RenderSlot> renderSlots{
            makeSlot(12, QVector3D(4, 0, 2)),
            makeSlot(10, QVector3D(-4, 0, 2)),
            makeSlot(11, QVector3D(0, 0, 2)),
        };
        cwSortBillboardSlotsBackToFront(renderSlots, viewProjection);

        REQUIRE(renderSlots.size() == 3);
        REQUIRE(renderSlots.at(0).id == cwBillboardId{10});
        REQUIRE(renderSlots.at(1).id == cwBillboardId{11});
        REQUIRE(renderSlots.at(2).id == cwBillboardId{12});
    }

    SECTION("fewer than two slots is a no-op")
    {
        QVector<cwRenderBillboards::RenderSlot> empty;
        cwSortBillboardSlotsBackToFront(empty, QMatrix4x4());
        REQUIRE(empty.isEmpty());

        QVector<cwRenderBillboards::RenderSlot> one{makeSlot(7, QVector3D(0, 0, 0))};
        cwSortBillboardSlotsBackToFront(one, QMatrix4x4());
        REQUIRE(one.size() == 1);
        REQUIRE(one.at(0).id == cwBillboardId{7});
    }
}

TEST_CASE("cwRenderBillboards: clearing billboards releases subscenes", "[cwRenderBillboards]")
{
    QQuickWindow window;
    auto content = std::make_unique<QQuickItem>(window.contentItem());

    cwRenderBillboards billboards;
    billboards.setWindow(&window);

    cwRenderBillboards::Billboard billboard;
    billboard.content = content.get();
    const cwBillboardId id = billboards.addBillboard(billboard);
    REQUIRE(billboards.buildRenderSlots().size() == 1);

    // Removing the billboard must deref its subscene without crashing.
    billboards.removeBillboard(id);
    REQUIRE(billboards.billboardCount() == 0);
    REQUIRE(billboards.buildRenderSlots().isEmpty());

    // The content item outlives the render object cleanly.
    REQUIRE(content != nullptr);
}
