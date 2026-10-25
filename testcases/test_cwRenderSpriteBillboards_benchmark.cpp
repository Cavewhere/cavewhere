// Benchmarks for the instanced sprite layer at ten thousand billboards (issue
// 714): the GUI-side instance snapshot stays under a millisecond, and the
// render thread reports one cw.profile.render line per live frame that draws
// sprites, read the way test_cwRenderTextBillboards_benchmark.cpp reads the text
// layer's. The profile part needs a GPU-backed window and skips under the
// offscreen QPA.
//
// Every case is hidden (a timing budget would flake in the parallel nightly
// suite). Run them with:
//   cavewhere-test "[SpriteBillboardsBenchmark]"
//
// Profile line contract: the sprite backend writes one cw.profile.render line
// per live frame that draws sprites, starting with "spriteBillboards" and
// carrying "instances=<count>" and "gatherUs=<microseconds>", the render-thread
// time of its gather (the layer texture updates, the atlas copies, pipeline and
// binding checks and the one drawable). The two-slot variant draws half the
// billboards from each atlas slot, so its gather includes a second layer.

#include <catch2/catch_test_macros.hpp>

//Our includes
#include "ProfileLogCapture.h"
#include "cwRenderSpriteBillboards.h"
#include "cwRhiViewer.h"
#include "cwScene.h"

//Qt includes
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QSGRendererInterface>
#include <QThread>

//Std includes
#include <algorithm>
#include <atomic>
#include <memory>
#include <vector>

namespace {

constexpr int kBillboardCount = 10000;
// A square grid of billboards, kGridSpacing meters apart.
constexpr int kGridSide = 100;
constexpr float kGridSpacing = 1.0f;
constexpr float kDepthBias = 1.0f;
// Every third billboard is hidden so the snapshot does some filtering.
constexpr int kHiddenStride = 3;

constexpr qint64 kSnapshotBudgetUs = 1000;
constexpr int kSnapshotRepeats = 5;

constexpr int kWindowSize = 400;
constexpr int kSpriteSize = 20;
constexpr int kProfileFrames = 20;
constexpr int kSettleFrames = 10;
constexpr int kWaitTimeoutMs = 60000;
constexpr int kPollIntervalMs = 2;

const QString kProfilePrefix = QStringLiteral("spriteBillboards");

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

cwRenderSpriteBillboards::SpriteBillboard gridBillboard(int index)
{
    cwRenderSpriteBillboards::SpriteBillboard billboard;
    billboard.worldPosition = QVector3D(float(index % kGridSide) * kGridSpacing,
                                        float(index / kGridSide) * kGridSpacing,
                                        0.0f);
    billboard.depthBias = kDepthBias;
    billboard.opacity = 1.0f;
    billboard.visible = index % kHiddenStride != 0;
    return billboard;
}

int visibleCount()
{
    int count = 0;
    for (int i = 0; i < kBillboardCount; ++i) {
        if (gridBillboard(i).visible) {
            ++count;
        }
    }
    return count;
}

qint64 median(QVector<qint64> values)
{
    std::sort(values.begin(), values.end());
    return values.at(values.size() / 2);
}

const QList<QByteArray> kOneSlotColors{"#0000ff"};
const QList<QByteArray> kTwoSlotColors{"#0000ff", "#ff0000"};

// Shows a window with a viewer, declares one solid prototype per entry in
// @a slotColors as the atlas slots, adds the grid with billboards spread across
// the slots, and reports the gather time the render thread profiles per frame.
void profileGather(const QList<QByteArray>& slotColors)
{
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        SKIP("needs a GPU-backed window; the offscreen QPA has no QRhi");
    }

    cwScene scene;

    QQmlEngine engine;
    QQuickWindow window;
    window.resize(kWindowSize, kWindowSize);

    auto* viewer = new cwRhiViewer();
    viewer->setParentItem(window.contentItem());
    viewer->setSize(QSizeF(kWindowSize, kWindowSize));
    viewer->setScene(&scene);

    std::vector<std::unique_ptr<QObject>> prototypeObjects;
    QList<QQuickItem*> prototypes;
    for (const QByteArray& color : slotColors) {
        const QByteArray prototypeQml =
            "import QtQuick\n"
            "Rectangle { width: " + QByteArray::number(kSpriteSize)
            + "; height: " + QByteArray::number(kSpriteSize) + "; color: \"" + color + "\" }\n";
        QQmlComponent component(&engine);
        component.setData(prototypeQml, QUrl());
        std::unique_ptr<QObject> prototypeObject(component.create());
        INFO(component.errorString().toStdString());
        REQUIRE(prototypeObject != nullptr);
        auto* prototype = qobject_cast<QQuickItem*>(prototypeObject.get());
        REQUIRE(prototype != nullptr);
        prototype->setParentItem(window.contentItem());
        prototypes.append(prototype);
        prototypeObjects.push_back(std::move(prototypeObject));
    }

    std::atomic<int> framesSwapped {0};
    QObject::connect(&window, &QQuickWindow::frameSwapped, &window,
                     [&framesSwapped]() { framesSwapped++; }, Qt::DirectConnection);

    window.show();
    REQUIRE(waitFor([&]() { return window.isExposed() && framesSwapped > 0; }));

    if (window.rendererInterface()->graphicsApi() == QSGRendererInterface::Software) {
        SKIP("needs a QRhi-backed scene graph");
    }

    const auto framesFrom = [&](int count) {
        const int target = framesSwapped + count;
        return waitFor([&]() {
            viewer->update();
            return framesSwapped >= target;
        });
    };

    cwRenderSpriteBillboards* layer = scene.spriteBillboardLayer();
    REQUIRE(layer != nullptr);
    layer->setWindow(&window);
    layer->setSprites(prototypes);
    REQUIRE(layer->spriteCount() == int(slotColors.size()));
    for (int i = 0; i < kBillboardCount; ++i) {
        cwRenderSpriteBillboards::SpriteBillboard billboard = gridBillboard(i);
        billboard.spriteIndex = i % int(slotColors.size());
        layer->addBillboard(billboard);
    }
    REQUIRE(framesFrom(kSettleFrames));

    {
        ProfileLogCapture capture(QStringLiteral("cw.profile.render.debug=true"));

        for (int frame = 0; frame < kProfileFrames; ++frame) {
            REQUIRE(framesFrom(1));
        }

        const QStringList lines = ProfileLogCapture::linesStartingWith(kProfilePrefix);
        const QRegularExpression instancesPattern(QStringLiteral("instances=(\\d+)"));
        const QRegularExpression gatherPattern(QStringLiteral("gatherUs=(\\d+)"));

        QVector<qint64> gatherUs;
        for (const QString& line : lines) {
            const QRegularExpressionMatch instances = instancesPattern.match(line);
            const QRegularExpressionMatch gather = gatherPattern.match(line);
            REQUIRE(instances.hasMatch());
            REQUIRE(gather.hasMatch());
            CHECK(instances.captured(1).toInt() == visibleCount());
            gatherUs.append(gather.captured(1).toLongLong());
        }

        INFO("captured " << lines.size() << " " << kProfilePrefix.toStdString() << " lines");
        REQUIRE(gatherUs.size() >= kProfileFrames);

        WARN("Sprite billboard gather over " << gatherUs.size() << " frames"
             << "\n  slots: " << slotColors.size()
             << "\n  median: " << median(gatherUs) << " us"
             << "\n  max: " << *std::max_element(gatherUs.cbegin(), gatherUs.cend()) << " us"
             << "\n  instances: " << visibleCount()
             << "\n  viewport: " << kWindowSize << "x" << kWindowSize
             << " logical px, dpr " << window.effectiveDevicePixelRatio());
    }

    window.hide();
}

} // namespace

TEST_CASE("cwRenderSpriteBillboards: ten thousand billboards snapshot under a millisecond",
          "[.][Benchmark][SpriteBillboardsBenchmark]")
{
    cwRenderSpriteBillboards layer;
    for (int i = 0; i < kBillboardCount; ++i) {
        layer.addBillboard(gridBillboard(i));
    }
    REQUIRE(layer.billboardCount() == kBillboardCount);

    QVector<qint64> elapsedUs;
    for (int repeat = 0; repeat < kSnapshotRepeats; ++repeat) {
        QElapsedTimer timer;
        timer.start();
        const auto instances = layer.buildInstances();
        elapsedUs.append(timer.nsecsElapsed() / 1000);
        REQUIRE(instances.size() == visibleCount());
    }

    const qint64 medianUs = median(elapsedUs);
    WARN("buildInstances over " << kBillboardCount << " billboards: median " << medianUs << " us");
    CHECK(medianUs < kSnapshotBudgetUs);
}

TEST_CASE("cwRenderSpriteBillboards: the render thread profiles one gather per frame",
          "[.][Benchmark][SpriteBillboardsBenchmark][rhi]")
{
    profileGather(kOneSlotColors);
}

TEST_CASE("cwRenderSpriteBillboards: a two-slot atlas profiles one gather per frame",
          "[.][Benchmark][SpriteBillboardsBenchmark][rhi]")
{
    profileGather(kTwoSlotColors);
}
