// Lifecycle test for cwBillboardHandle. The handle's bookkeeping runs entirely
// on the GUI thread (managing the layer's billboard map), so no QRhi is needed:
// billboardCount()/hasBillboard() observe every effect through the real public
// surface.

#include <catch2/catch_test_macros.hpp>

#include <QQuickItem>
#include <QQuickWindow>

#include "cwBillboardHandle.h"
#include "cwRenderBillboards.h"

namespace {
cwRenderBillboards::Billboard makeBillboard(QQuickItem* content, const QVector3D& position)
{
    cwRenderBillboards::Billboard billboard;
    billboard.content = content;
    billboard.worldPosition = position;
    return billboard;
}
}

TEST_CASE("cwBillboardHandle: set adds once then updates", "[cwBillboardHandle]")
{
    QQuickWindow window;
    auto content = std::make_unique<QQuickItem>(window.contentItem());

    cwRenderBillboards layer;
    layer.setWindow(&window);

    cwBillboardHandle handle;
    REQUIRE_FALSE(handle.isValid());

    handle.set(&layer, makeBillboard(content.get(), QVector3D(0, 0, 0)));
    REQUIRE(handle.isValid());
    REQUIRE(layer.billboardCount() == 1);
    const cwBillboardId firstId = handle.id();
    REQUIRE(layer.hasBillboard(firstId));

    // A second set() must update the same billboard, not add another.
    handle.set(&layer, makeBillboard(content.get(), QVector3D(1, 2, 3)));
    REQUIRE(layer.billboardCount() == 1);
    REQUIRE(handle.id() == firstId);
}

TEST_CASE("cwBillboardHandle: destruction removes the billboard", "[cwBillboardHandle]")
{
    QQuickWindow window;
    auto content = std::make_unique<QQuickItem>(window.contentItem());

    cwRenderBillboards layer;
    layer.setWindow(&window);

    {
        cwBillboardHandle handle;
        handle.set(&layer, makeBillboard(content.get(), QVector3D(0, 0, 0)));
        REQUIRE(layer.billboardCount() == 1);
    }
    // Handle went out of scope — its billboard is gone.
    REQUIRE(layer.billboardCount() == 0);
}

TEST_CASE("cwBillboardHandle: reset removes the billboard", "[cwBillboardHandle]")
{
    QQuickWindow window;
    auto content = std::make_unique<QQuickItem>(window.contentItem());

    cwRenderBillboards layer;
    layer.setWindow(&window);

    cwBillboardHandle handle;
    handle.set(&layer, makeBillboard(content.get(), QVector3D(0, 0, 0)));
    REQUIRE(layer.billboardCount() == 1);

    handle.reset();
    REQUIRE_FALSE(handle.isValid());
    REQUIRE(layer.billboardCount() == 0);

    // reset() on an empty handle is a harmless no-op.
    handle.reset();
    REQUIRE(layer.billboardCount() == 0);
}

TEST_CASE("cwBillboardHandle: move transfers ownership without leaking", "[cwBillboardHandle]")
{
    QQuickWindow window;
    auto content = std::make_unique<QQuickItem>(window.contentItem());

    cwRenderBillboards layer;
    layer.setWindow(&window);

    cwBillboardHandle source;
    source.set(&layer, makeBillboard(content.get(), QVector3D(0, 0, 0)));
    const cwBillboardId movedId = source.id();
    REQUIRE(layer.billboardCount() == 1);

    SECTION("move construction")
    {
        cwBillboardHandle moved(std::move(source));
        REQUIRE(moved.id() == movedId);
        REQUIRE_FALSE(source.isValid());  // NOLINT(bugprone-use-after-move)
        REQUIRE(layer.billboardCount() == 1);  // not duplicated, not dropped
    }

    SECTION("move assignment removes the destination's old billboard first")
    {
        auto otherContent = std::make_unique<QQuickItem>(window.contentItem());
        cwBillboardHandle destination;
        destination.set(&layer, makeBillboard(otherContent.get(), QVector3D(9, 9, 9)));
        REQUIRE(layer.billboardCount() == 2);

        destination = std::move(source);
        REQUIRE(destination.id() == movedId);
        REQUIRE_FALSE(source.isValid());  // NOLINT(bugprone-use-after-move)
        // destination's original billboard removed, source's transferred: one left.
        REQUIRE(layer.billboardCount() == 1);
        REQUIRE(layer.hasBillboard(movedId));
    }
}

TEST_CASE("cwBillboardHandle: set to a different layer moves the billboard", "[cwBillboardHandle]")
{
    QQuickWindow window;
    auto content = std::make_unique<QQuickItem>(window.contentItem());

    cwRenderBillboards layerA;
    cwRenderBillboards layerB;
    layerA.setWindow(&window);
    layerB.setWindow(&window);

    cwBillboardHandle handle;
    handle.set(&layerA, makeBillboard(content.get(), QVector3D(0, 0, 0)));
    REQUIRE(layerA.billboardCount() == 1);
    REQUIRE(layerB.billboardCount() == 0);

    handle.set(&layerB, makeBillboard(content.get(), QVector3D(0, 0, 0)));
    REQUIRE(layerA.billboardCount() == 0);  // removed from the old layer
    REQUIRE(layerB.billboardCount() == 1);

    // A null layer releases the billboard entirely.
    handle.set(nullptr, makeBillboard(content.get(), QVector3D(0, 0, 0)));
    REQUIRE_FALSE(handle.isValid());
    REQUIRE(layerB.billboardCount() == 0);
}

TEST_CASE("cwBillboardHandle: a vector of handles cleans up on erase", "[cwBillboardHandle]")
{
    QQuickWindow window;
    auto content = std::make_unique<QQuickItem>(window.contentItem());

    cwRenderBillboards layer;
    layer.setWindow(&window);

    std::vector<cwBillboardHandle> handles;
    for (int i = 0; i < 3; ++i) {
        cwBillboardHandle handle;
        handle.set(&layer, makeBillboard(content.get(), QVector3D(float(i), 0, 0)));
        handles.push_back(std::move(handle));
    }
    REQUIRE(layer.billboardCount() == 3);

    // Erasing one handle removes exactly its billboard.
    handles.erase(handles.begin());
    REQUIRE(layer.billboardCount() == 2);

    // Clearing the container removes the rest.
    handles.clear();
    REQUIRE(layer.billboardCount() == 0);
}
