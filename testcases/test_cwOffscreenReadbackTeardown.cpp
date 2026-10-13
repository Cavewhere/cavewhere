// Real-QRhi teardown of a window whose offscreen read-backs are still in flight.
// Metal and Vulkan write into every active read-back result while the QRhi is
// destroyed, so each result has to stay alive until then. Needs a GPU-backed
// window; skips under the offscreen QPA.

#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwOffscreenRenderParameters.h"
#include "cwRhiViewer.h"
#include "cwScene.h"

//Qt includes
#include <QColor>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFuture>
#include <QGuiApplication>
#include <QImage>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QThread>

//Std includes
#include <atomic>
#include <memory>

namespace {

constexpr int kWindowSize = 400;
const QSize kTileSize(64, 64);
constexpr int kRenderThreadHoldMs = 300;

constexpr int kWaitTimeoutMs = 60000;
constexpr int kPollIntervalMs = 2;

template <typename Predicate>
bool waitFor(Predicate predicate, int timeoutMs = kWaitTimeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents();
        if (predicate()) {
            return true;
        }
        QThread::msleep(kPollIntervalMs);
    }
    return predicate();
}

} // namespace

TEST_CASE("destroying the window with offscreen read-backs in flight resolves every job",
          "[OffscreenReadbackTeardown]")
{
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        SKIP("needs a GPU-backed window; the offscreen QPA has no QRhi");
    }

    cwScene scene;
    QFuture<QImage> job;

    // Outlives the window: the render thread still swaps frames while it closes.
    std::atomic<int> framesSwapped {0};
    std::atomic<bool> armed {false};
    std::atomic<bool> jobSynchronized {false};
    std::atomic<bool> renderThreadHeld {false};
    std::atomic<bool> jobFrameSwapped {false};

    {
        auto window = std::make_unique<QQuickWindow>();
        window->resize(kWindowSize, kWindowSize);

        auto* viewer = new cwRhiViewer();
        viewer->setParentItem(window->contentItem());
        viewer->setSize(QSizeF(kWindowSize, kWindowSize));
        viewer->setScene(&scene);

        QObject::connect(window.get(), &QQuickWindow::frameSwapped, window.get(),
                         [&framesSwapped]() { framesSwapped++; }, Qt::DirectConnection);

        window->show();
        REQUIRE(waitFor([&]() { return window->isExposed() && framesSwapped > 0; }));

        if (window->rendererInterface()->graphicsApi() == QSGRendererInterface::Software) {
            SKIP("needs a QRhi-backed scene graph");
        }

        cwOffscreenRenderParameters parameters;
        parameters.outputSize = kTileSize;
        parameters.backgroundColor = Qt::transparent;

        // The frame after the one that records the read-back is held at its
        // start, so the window closes before the backend's frame slot comes
        // around to complete it. The GUI thread is blocked during synchronizing,
        // so the first sync after arming is the one that hands the job over.
        QObject::connect(window.get(), &QQuickWindow::afterSynchronizing, window.get(),
                         [&]() { jobSynchronized = armed.load(); }, Qt::DirectConnection);
        QObject::connect(window.get(), &QQuickWindow::frameSwapped, window.get(),
                         [&]() { jobFrameSwapped = jobSynchronized.load(); }, Qt::DirectConnection);
        QObject::connect(window.get(), &QQuickWindow::beforeFrameBegin, window.get(),
                         [&]() {
            if (jobFrameSwapped && !renderThreadHeld.exchange(true)) {
                QThread::msleep(kRenderThreadHoldMs);
            }
        }, Qt::DirectConnection);

        armed = true;
        job = scene.renderOffscreen(parameters);
        REQUIRE(waitFor([&]() { return jobSynchronized.load(); }));

        QElapsedTimer timer;
        timer.start();
        while (!jobFrameSwapped && timer.elapsed() < kWaitTimeoutMs) {
            QThread::msleep(kPollIntervalMs);
        }
        REQUIRE(jobFrameSwapped);
    } // window destroyed with the read-back in flight: the render thread tears down its QRhi

    CHECK(job.isFinished());
}
