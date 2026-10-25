// Real-QRhi smoke test for the station label text layer under the threaded
// render loop: three labels draw, a label behind an opaque wall is depth-tested
// away, and unbinding and rebinding the window drops and rebuilds the text nodes.
// Needs a GPU-backed window; skips under the offscreen QPA, which has no QRhi.
//
// Pixels are read back through cwScene::renderOffscreen with a fixed camera, so
// probes sit at known positions regardless of the live view. The batch count
// (two batches for N labels, none merged) is checked by hand with
// QSG_RENDERER_DEBUG=render, not here.

#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwGeometry.h"
#include "cwOffscreenRenderParameters.h"
#include "cwRenderTextBillboards.h"
#include "cwRenderTexturedItems.h"
#include "cwRhiViewer.h"
#include "cwScene.h"

//Qt includes
#include <QColor>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFuture>
#include <QGuiApplication>
#include <QImage>
#include <QMatrix4x4>
#include <QMutex>
#include <QMutexLocker>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QStringList>
#include <QThread>
#include <QVector2D>
#include <QVector3D>

//Std includes
#include <atomic>

namespace {

constexpr int kWindowSize = 400;
const QSize kOutputSize(256, 256);

// Orthographic camera on +z looking at the origin; kOrthoHalfExtent meters
// span half the output.
constexpr float kOrthoHalfExtent = 10.0f;
constexpr float kOrthoDepthExtent = 1000.0f;
constexpr float kEyeDistance = 50.0f;

constexpr int kLabelPixelSize = 20;
const QString kLabelText = QStringLiteral("WWW");
constexpr float kLabelDepthBias = 1.0f;

// Labels sit in the z = 0 plane. The wall sits in front of the right label by
// more than its depth bias, so the label is fully behind it.
const QVector3D kLeftLabel(-6.0f, 0.0f, 0.0f);
const QVector3D kMiddleLabel(0.0f, 0.0f, 0.0f);
const QVector3D kHiddenLabel(6.0f, 0.0f, 0.0f);
constexpr float kWallZ = 3.0f;
constexpr float kWallHalfExtent = 3.0f;
constexpr int kWallTextureSize = 4;

// Probe box around a label's projected center: inside the glyph run for a
// visible label, inside the wall for the hidden one.
constexpr int kProbeHalfWidth = 26;
constexpr int kProbeHalfHeight = 10;

constexpr int kWaitTimeoutMs = 30000;
constexpr int kPollIntervalMs = 2;
constexpr int kSettleFrames = 5;

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

// Keeps render-backend warnings so the test can require a clean frame. Other
// messages pass through to the previous handler.
class RenderWarningCapture
{
public:
    RenderWarningCapture()
    {
        {
            const QMutexLocker locker(&mutex());
            warnings().clear();
        }
        previousHandler() = qInstallMessageHandler(&RenderWarningCapture::keep);
    }

    ~RenderWarningCapture()
    {
        qInstallMessageHandler(previousHandler());
        previousHandler() = nullptr;
    }

    RenderWarningCapture(const RenderWarningCapture&) = delete;
    RenderWarningCapture& operator=(const RenderWarningCapture&) = delete;

    static QStringList captured()
    {
        const QMutexLocker locker(&mutex());
        return warnings();
    }

private:
    static void keep(QtMsgType type, const QMessageLogContext& context, const QString& message)
    {
        const QString category = QString::fromLatin1(context.category ? context.category : "");
        const bool renderCategory = category.startsWith(QStringLiteral("qt.rhi"))
                                    || category.startsWith(QStringLiteral("qt.scenegraph"));
        const bool severe = type == QtCriticalMsg || type == QtFatalMsg;
        if (severe || (type == QtWarningMsg && renderCategory)) {
            const QMutexLocker locker(&mutex());
            warnings().append(message);
        }
        if (previousHandler() != nullptr) {
            previousHandler()(type, context, message);
        }
    }

    static QStringList& warnings()
    {
        static QStringList kept;
        return kept;
    }

    static QMutex& mutex()
    {
        static QMutex guard;
        return guard;
    }

    static QtMessageHandler& previousHandler()
    {
        static QtMessageHandler handler = nullptr;
        return handler;
    }
};

QMatrix4x4 cameraView()
{
    QMatrix4x4 view;
    view.lookAt(QVector3D(0.0f, 0.0f, kEyeDistance), QVector3D(0.0f, 0.0f, 0.0f),
                QVector3D(0.0f, 1.0f, 0.0f));
    return view;
}

QMatrix4x4 cameraProjection()
{
    QMatrix4x4 projection;
    projection.ortho(-kOrthoHalfExtent, kOrthoHalfExtent, -kOrthoHalfExtent, kOrthoHalfExtent,
                     -kOrthoDepthExtent, kOrthoDepthExtent);
    return projection;
}

// Output pixel of a world point. The labels sit on y = 0, the center row, so
// a vertical flip in the readback leaves the probes in place.
QPoint toPixel(const QVector3D& worldPosition)
{
    const QVector3D ndc = (cameraProjection() * cameraView() * QVector4D(worldPosition, 1.0f))
                              .toVector3DAffine();
    return QPoint(int((ndc.x() + 1.0f) * 0.5f * float(kOutputSize.width())),
                  int((1.0f - ndc.y()) * 0.5f * float(kOutputSize.height())));
}

cwGeometry wallQuad()
{
    cwGeometry geometry(cwRenderTexturedItems::geometryLayout());
    geometry.resizeVertices(4);
    const auto* position = geometry.attribute(cwGeometry::Semantic::Position);
    const auto* texCoord = geometry.attribute(cwGeometry::Semantic::TexCoord0);

    const QVector3D center(kHiddenLabel.x(), kHiddenLabel.y(), kWallZ);
    const QVector<QVector3D> corners = {
        QVector3D(-kWallHalfExtent, -kWallHalfExtent, 0.0f),
        QVector3D( kWallHalfExtent, -kWallHalfExtent, 0.0f),
        QVector3D( kWallHalfExtent,  kWallHalfExtent, 0.0f),
        QVector3D(-kWallHalfExtent,  kWallHalfExtent, 0.0f),
    };
    const QVector<QVector2D> uvs = {
        QVector2D(0.0f, 0.0f), QVector2D(1.0f, 0.0f),
        QVector2D(1.0f, 1.0f), QVector2D(0.0f, 1.0f),
    };
    for (int i = 0; i < corners.size(); ++i) {
        geometry.set(position, i, center + corners.at(i));
        geometry.set(texCoord, i, uvs.at(i));
    }
    geometry.setIndices({0u, 1u, 2u, 0u, 2u, 3u});
    geometry.setType(cwGeometry::Type::Triangles);
    return geometry;
}

QImage solidImage(const QColor& color)
{
    QImage image(QSize(kWallTextureSize, kWallTextureSize), QImage::Format_RGBA8888);
    image.fill(color);
    return image;
}

cwRenderTextBillboards::TextBillboard makeLabel(const QVector3D& position)
{
    cwRenderTextBillboards::TextBillboard billboard;
    billboard.text = kLabelText;
    billboard.worldPosition = position;
    billboard.depthBias = kLabelDepthBias;
    return billboard;
}

// Number of pixels in the probe box around @a center that differ between the
// two renders.
int differingPixels(const QImage& a, const QImage& b, const QPoint& center)
{
    int count = 0;
    for (int y = center.y() - kProbeHalfHeight; y <= center.y() + kProbeHalfHeight; ++y) {
        for (int x = center.x() - kProbeHalfWidth; x <= center.x() + kProbeHalfWidth; ++x) {
            if (a.pixel(x, y) != b.pixel(x, y)) {
                ++count;
            }
        }
    }
    return count;
}

} // namespace

TEST_CASE("cwRenderTextBillboards renders labels depth-tested against the scene",
          "[cwRenderTextBillboards][rhi]")
{
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        SKIP("needs a GPU-backed window; the offscreen QPA has no QRhi");
    }

    RenderWarningCapture warnings;

    cwScene scene;

    QQuickWindow window;
    window.resize(kWindowSize, kWindowSize);

    auto* viewer = new cwRhiViewer();
    viewer->setParentItem(window.contentItem());
    viewer->setSize(QSizeF(kWindowSize, kWindowSize));
    viewer->setScene(&scene);

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

    const auto renderImage = [&]() {
        cwOffscreenRenderParameters parameters;
        parameters.viewMatrix = cameraView();
        parameters.projectionMatrix = cameraProjection();
        parameters.outputSize = kOutputSize;
        parameters.backgroundColor = QColor::fromRgbF(0.0, 0.0, 0.0, 1.0);
        QFuture<QImage> future = scene.renderOffscreen(parameters);
        const bool finished = waitFor([&]() {
            viewer->update();
            return future.isFinished();
        });
        REQUIRE(finished);
        REQUIRE(future.resultCount() == 1);
        const QImage image = future.result().convertToFormat(QImage::Format_ARGB32);
        REQUIRE(image.size() == kOutputSize);
        return image;
    };

    auto* wall = new cwRenderTexturedItems();
    wall->setScene(&scene);
    cwRenderMaterialState material = cwRenderTexturedItems::scrapMaterial();
    material.cullMode = cwRenderMaterialState::CullMode::None;
    cwRenderTexturedItems::Item wallItem;
    wallItem.geometry = wallQuad();
    wallItem.texture = solidImage(Qt::red);
    wallItem.material = material;
    wallItem.uniformBlock = cwRenderTexturedItems::emptyItemBlock();
    wall->addItem(wallItem);

    REQUIRE(framesFrom(kSettleFrames));
    const QImage baseline = renderImage();

    // The wall reached the image, so the hidden-label probe really is over it.
    const QRgb background = qRgb(0, 0, 0);
    REQUIRE(baseline.pixel(toPixel(kHiddenLabel)) != background);
    REQUIRE(baseline.pixel(toPixel(kLeftLabel)) == background);

    cwRenderTextBillboards* layer = scene.textBillboardLayer();
    REQUIRE(layer != nullptr);
    layer->setWindow(&window);
    cwRenderTextBillboards::Style style;
    style.font.setPixelSize(kLabelPixelSize);
    style.color = Qt::white;
    style.outlineColor = Qt::black;
    layer->setStyle(style);

    layer->addBillboard(makeLabel(kLeftLabel));
    layer->addBillboard(makeLabel(kMiddleLabel));
    layer->addBillboard(makeLabel(kHiddenLabel));
    REQUIRE(layer->billboardCount() == 3);

    REQUIRE(framesFrom(kSettleFrames));
    const QImage withLabels = renderImage();

    SECTION("visible labels draw and the label behind the wall does not")
    {
        CHECK(differingPixels(withLabels, baseline, toPixel(kLeftLabel)) > 0);
        CHECK(differingPixels(withLabels, baseline, toPixel(kMiddleLabel)) > 0);
        CHECK(differingPixels(withLabels, baseline, toPixel(kHiddenLabel)) == 0);
    }

    SECTION("unbinding and rebinding the window drops and rebuilds the text nodes")
    {
        layer->setWindow(nullptr);
        REQUIRE(framesFrom(kSettleFrames));
        const QImage unbound = renderImage();
        CHECK(differingPixels(unbound, baseline, toPixel(kLeftLabel)) == 0);

        layer->setWindow(&window);
        REQUIRE(framesFrom(kSettleFrames));
        const QImage rebound = renderImage();
        CHECK(differingPixels(rebound, baseline, toPixel(kLeftLabel)) > 0);
        CHECK(differingPixels(rebound, baseline, toPixel(kHiddenLabel)) == 0);
    }

    const QStringList captured = RenderWarningCapture::captured();
    INFO("render warnings: " << captured.join(QStringLiteral("\n")).toStdString());
    CHECK(captured.isEmpty());

    window.hide();
}
