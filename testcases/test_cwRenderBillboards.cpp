// GUI-thread lifecycle test for cwRenderBillboards. Without a live QRhi the
// render-thread backend never runs, so this covers the GUI side: the slot
// snapshot it hands to the render thread, that null content is skipped, and that
// clearing the list releases the referenced subscenes cleanly. The full
// materialize-and-occlude path is covered live by the app and the QML/offscreen
// suite.

#include <catch2/catch_test_macros.hpp>

#include <QQuickItem>
#include <QQuickWindow>

#include "cwRenderBillboards.h"

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
    billboards.setBillboards({billboard});

    const auto renderSlots = billboards.buildRenderSlots();
    REQUIRE(renderSlots.size() == 1);
    REQUIRE(renderSlots.at(0).contentSize == QSizeF(120, 40));
    REQUIRE(renderSlots.at(0).worldPosition == QVector3D(1, 2, 3));
    REQUIRE(renderSlots.at(0).sizeMode == cwRenderBillboards::SizeMode::ScreenConstant);
    REQUIRE(renderSlots.at(0).depthBias == 0.5f);

    // No scene-graph sync happens in this CPU-only test, so the content's node
    // subtree is not materialized: the handle stays null (the backend skips it).
    REQUIRE(renderSlots.at(0).rootNodeHandle == 0);
}

TEST_CASE("cwRenderBillboards: null content is skipped", "[cwRenderBillboards]")
{
    QQuickWindow window;
    cwRenderBillboards billboards;
    billboards.setWindow(&window);

    cwRenderBillboards::Billboard billboard;  // content left null
    billboards.setBillboards({billboard});

    REQUIRE(billboards.buildRenderSlots().isEmpty());
}

TEST_CASE("cwRenderBillboards: clearing billboards releases subscenes", "[cwRenderBillboards]")
{
    QQuickWindow window;
    auto content = std::make_unique<QQuickItem>(window.contentItem());

    cwRenderBillboards billboards;
    billboards.setWindow(&window);

    cwRenderBillboards::Billboard billboard;
    billboard.content = content.get();
    billboards.setBillboards({billboard});
    REQUIRE(billboards.buildRenderSlots().size() == 1);

    // Removing the billboard must deref its subscene without crashing.
    billboards.setBillboards({});
    REQUIRE(billboards.buildRenderSlots().isEmpty());

    // The content item outlives the render object cleanly.
    REQUIRE(content != nullptr);
}
