// Hidden benchmark for the station label text layer on the large synthetic cave
// (issue 714). Drives a scripted turntable orbit on the live camera (no real
// input), reads the text layer's per-frame Overlay pass time from
// cw.profile.render, and prints the median with the placed label count and the
// viewport size, so the per-label cost is a measurement. No hard bound.
//
// Generate the cave once with the [LargeCave] test, then run:
//   CAVEWHERE_LARGE_CAVE_INPUT=/abs/path/largeCave.cw cavewhere-test "[TextBillboardsBenchmark]"
//
// Profile line contract: the text backend writes one cw.profile.render line per
// live frame that draws labels, starting with "textBillboards" and carrying
// "overlayUs=<microseconds>", the render-thread time of its Overlay pass work
// (synchronize excluded, gather + prepare + render included).

#include <catch2/catch_test_macros.hpp>

//Our includes
#include "LoadProjectHelper.h"
#include "ProfileLogCapture.h"
#include "cwCamera.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwLinePlotLabelView.h"
#include "cwLinePlotManager.h"
#include "cwProject.h"
#include "cwRenderTextBillboards.h"
#include "cwRhiViewer.h"
#include "cwScene.h"
#include "cwStationPositionLookup.h"
#include "TestHelper.h"

//Qt includes
#include <QBox3D>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMatrix4x4>
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

namespace {

constexpr int kWindowWidth = 1600;
constexpr int kWindowHeight = 1000;

constexpr int kOrbitFrames = 60;
constexpr float kFullTurnDegrees = 360.0f;
constexpr float kOrbitPitchDegrees = -45.0f;

// Margin around the cave's bounds when framing it.
constexpr double kFramingMargin = 1.1;

constexpr int kWaitTimeoutMs = 60000;
constexpr int kSettleFrames = 10;

const QString kProfilePrefix = QStringLiteral("textBillboards");

QBox3D stationBounds(const cwCavingRegion* region)
{
    QBox3D bounds;
    for (const cwCave* cave : region->caves()) {
        const auto positions = cave->scopedPositions().positions();
        for (auto it = positions.cbegin(); it != positions.cend(); ++it) {
            bounds.unite(it.value());
        }
    }
    return bounds;
}

QMatrix4x4 orbitView(const QVector3D& center, float turnDegrees)
{
    QMatrix4x4 view;
    view.rotate(kOrbitPitchDegrees, 1.0f, 0.0f, 0.0f);
    view.rotate(turnDegrees, 0.0f, 0.0f, 1.0f);
    view.translate(-center);
    return view;
}

qint64 median(QVector<qint64> values)
{
    std::sort(values.begin(), values.end());
    return values.at(values.size() / 2);
}

} // namespace

TEST_CASE("Station label Overlay pass time on the large cave", "[.][Benchmark][TextBillboardsBenchmark]")
{
    const QString inputPath = qEnvironmentVariable("CAVEWHERE_LARGE_CAVE_INPUT");
    if (inputPath.isEmpty()) {
        SKIP("set CAVEWHERE_LARGE_CAVE_INPUT to a cave written by the [LargeCave] test");
    }
    REQUIRE(QFileInfo::exists(inputPath));

    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        SKIP("needs a GPU-backed window; the offscreen QPA has no QRhi");
    }

    // Load a private copy so concurrent runs never share the project file.
    auto project = std::make_unique<cwProject>();
    addTokenManager(project.get());
    project->loadOrConvert(copyToTempFolder(inputPath));
    project->waitLoadToFinish();

    cwCavingRegion* region = project->cavingRegion();
    REQUIRE(region->caveCount() > 0);

    auto plotManager = std::make_unique<cwLinePlotManager>();
    plotManager->setRegion(region);
    plotManager->waitToFinish();

    const QBox3D bounds = stationBounds(region);
    REQUIRE_FALSE(bounds.isNull());

    cwScene scene;

    QQmlEngine engine;
    QQuickWindow window;
    window.resize(kWindowWidth, kWindowHeight);

    auto* viewer = new cwRhiViewer();
    viewer->setParentItem(window.contentItem());
    viewer->setSize(QSizeF(kWindowWidth, kWindowHeight));
    viewer->setScene(&scene);

    const char* rootQml =
        "import QtQuick\n"
        "import cavewherelib\n"
        "Item {\n"
        "  LinePlotLabelView { objectName: \"labelView\"; anchors.fill: parent }\n"
        "}\n";
    QQmlComponent component(&engine);
    component.setData(QByteArray(rootQml), QUrl());
    std::unique_ptr<QObject> root(component.create());
    REQUIRE(root != nullptr);
    auto* rootItem = qobject_cast<QQuickItem*>(root.get());
    REQUIRE(rootItem != nullptr);
    rootItem->setParentItem(window.contentItem());
    rootItem->setSize(QSizeF(kWindowWidth, kWindowHeight));

    auto* labelView = root->findChild<cwLinePlotLabelView*>(QStringLiteral("labelView"));
    REQUIRE(labelView != nullptr);

    std::atomic<int> framesSwapped {0};
    QObject::connect(&window, &QQuickWindow::frameSwapped, &window,
                     [&framesSwapped]() { framesSwapped++; }, Qt::DirectConnection);

    window.show();
    REQUIRE(waitUntil([&]() { return window.isExposed() && framesSwapped > 0; }, kWaitTimeoutMs));

    if (window.rendererInterface()->graphicsApi() == QSGRendererInterface::Software) {
        SKIP("needs a QRhi-backed scene graph");
    }

    const auto framesFrom = [&](int count) {
        const int target = framesSwapped + count;
        return waitUntil([&]() {
            viewer->update();
            return framesSwapped >= target;
        }, kWaitTimeoutMs);
    };

    cwCamera* camera = viewer->camera();
    REQUIRE(camera != nullptr);
    REQUIRE(waitUntil([&]() { return !camera->viewport().isEmpty(); }, kWaitTimeoutMs));
    const QSize viewportSize = camera->viewport().size();

    const QVector3D center = bounds.center();
    const QVector3D extent = bounds.size();
    const double largestExtent = std::max(extent.x(), extent.y());
    camera->setZoomScale(largestExtent * kFramingMargin
                         / std::min(viewportSize.width(), viewportSize.height()));
    camera->setProjection(camera->orthoProjectionDefault());
    camera->setViewMatrix(orbitView(center, 0.0f));

    labelView->setScene(&scene);
    labelView->setCamera(camera);
    labelView->setRegion(region);

    REQUIRE(framesFrom(kSettleFrames));

    cwRenderTextBillboards* layer = scene.textBillboardLayer();
    REQUIRE(layer != nullptr);

    QVector<int> placedCounts;
    {
        ProfileLogCapture capture(QStringLiteral("cw.profile.render.debug=true"));

        for (int frame = 0; frame < kOrbitFrames; ++frame) {
            const float turn = kFullTurnDegrees * float(frame) / float(kOrbitFrames);
            camera->setViewMatrix(orbitView(center, turn));
            placedCounts.append(layer->billboardCount());
            REQUIRE(framesFrom(1));
        }
        REQUIRE(framesFrom(kSettleFrames));

        const QStringList lines = ProfileLogCapture::linesStartingWith(kProfilePrefix);
        const QRegularExpression overlayPattern(QStringLiteral("overlayUs=(\\d+)"));

        QVector<qint64> overlayUs;
        for (const QString& line : lines) {
            const QRegularExpressionMatch match = overlayPattern.match(line);
            if (match.hasMatch()) {
                overlayUs.append(match.captured(1).toLongLong());
            }
        }

        INFO("captured " << lines.size() << " " << kProfilePrefix.toStdString() << " lines");
        REQUIRE(overlayUs.size() >= kOrbitFrames);

        QVector<qint64> placed;
        for (const int count : std::as_const(placedCounts)) {
            placed.append(count);
        }

        WARN("Station label Overlay pass over " << overlayUs.size() << " frames"
             << "\n  median: " << median(overlayUs) << " us"
             << "\n  max: " << *std::max_element(overlayUs.cbegin(), overlayUs.cend()) << " us"
             << "\n  placed labels (median): " << median(placed)
             << "\n  viewport: " << viewportSize.width() << "x" << viewportSize.height()
             << " logical px, dpr " << window.effectiveDevicePixelRatio());
    }

    window.hide();
}
