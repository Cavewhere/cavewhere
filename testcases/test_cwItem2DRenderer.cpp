// Private-API tripwire for cwItem2DRenderer. Without a live QRhi the inline
// QSGRenderer can't be built, so this verifies the guard paths: construction,
// prepare() refusing an incomplete frame (returning false rather than crashing),
// render() being a safe no-op before a successful prepare, and clean teardown.
// The real inline-draw + depth-occlusion path is exercised live by the app and
// is covered end-to-end by the QML/offscreen suite.

#include <catch2/catch_test_macros.hpp>

#include <QQuickWindow>

#include "cwItem2DRenderer.h"

TEST_CASE("cwItem2DRenderer: null window refuses to prepare", "[cwItem2DRenderer]")
{
    cwItem2DRenderer renderer(nullptr);

    cwItem2DRenderer::Frame frame;
    REQUIRE_FALSE(renderer.prepare(frame));

    // render() before any successful prepare must be a safe no-op.
    renderer.render();
}

TEST_CASE("cwItem2DRenderer: incomplete frame is rejected", "[cwItem2DRenderer]")
{
    QQuickWindow window;
    cwItem2DRenderer renderer(&window);

    // Frame missing the render target / command buffer / root node handle: the
    // renderer must bail out instead of dereferencing the nulls.
    cwItem2DRenderer::Frame frame;
    frame.deviceRect = QSize(64, 64);
    frame.devicePixelRatio = 1.0f;
    REQUIRE_FALSE(renderer.prepare(frame));

    renderer.render();
}
