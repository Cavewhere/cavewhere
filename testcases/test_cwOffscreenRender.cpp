// CPU-only tests for the GUI-side lifecycle of the offscreen-render substrate
// (cwScene::renderOffscreen). The render-thread readback path is covered by the
// real-RHI offscreen render test (it needs a live QRhi device).

#include <catch2/catch_test_macros.hpp>

#include <QFuture>
#include <QImage>
#include <QSize>

#include "cwScene.h"
#include "cwOffscreenRenderParameters.h"

TEST_CASE("renderOffscreen starts the promise and finishes it on scene teardown",
          "[cwOffscreenRender]")
{
    // A job queued on the GUI thread but never drained by the render thread (no
    // QRhi in this test) must still resolve: renderOffscreen starts the promise
    // and returns its future, and ~cwScene finishes any un-handed-off job so the
    // future can't hang.
    QFuture<QImage> future;

    {
        cwScene scene;
        cwOffscreenRenderParameters parameters;
        parameters.outputSize = QSize(64, 64);
        future = scene.renderOffscreen(parameters);

        REQUIRE(future.isStarted());
        REQUIRE_FALSE(future.isFinished());
    } // scene destroyed without a render frame

    REQUIRE(future.isFinished());
    REQUIRE(future.resultCount() == 0); // never rendered → no image, but resolved
}

TEST_CASE("renderOffscreen returns a future the caller can cancel",
          "[cwOffscreenRender]")
{
    // The caller only ever holds the future, so the only thing it can do to the
    // result channel is observe or cancel it. This asserts the consumer-side API:
    // cancelling marks the future. The render-thread side honours it in
    // cwRhiScene::renderPendingOffscreen (skips the GPU work when isCanceled()),
    // which needs a live QRhi and is not asserted here. Teardown of the un-drained
    // job must not crash.
    cwScene scene;
    cwOffscreenRenderParameters parameters;
    parameters.outputSize = QSize(64, 64);

    QFuture<QImage> future = scene.renderOffscreen(parameters);
    future.cancel();

    REQUIRE(future.isCanceled());
}
