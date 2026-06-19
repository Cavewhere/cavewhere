// cwScene::billboardLayer() returns the one shared overlay layer for the scene.
// Every overlay (leads, station labels, future overlays) feeds this single layer
// so their billboards sort back-to-front together (#538). The accessor's wiring
// runs entirely on the GUI thread, so no QRhi is needed here.

#include <catch2/catch_test_macros.hpp>

#include "cwScene.h"
#include "cwRenderBillboards.h"

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
