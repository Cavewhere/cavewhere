// Real-QRhi test for the instanced sprite layer under the threaded render loop.
// The prototype is a 20 by 20 item whose top half is a solid blue rectangle and
// bottom half transparent, so one sprite checks orientation and the alpha
// cutoff at once: a sprite in front of a red wall shows blue over its top half
// and the wall through its bottom half, a sprite behind the wall is depth-tested
// away, a station label behind a sprite shows through the transparent half and
// is hidden under the ink, a label in front paints over the blue, the drawn
// extent is the prototype's size under both projections and scales with an
// offscreen job's device pixel ratio, a bottom-center anchor lifts the sprite
// above its position, and unbinding and rebinding the window drops and rebuilds
// the layer texture. A two-slot atlas of a solid blue and a
// solid red prototype draws each instance from its own slot, and re-skinning one
// slot changes only the instances that name it. Needs a GPU-backed window;
// skips under the offscreen QPA, which has no QRhi.
//
// Pixels are read back through cwScene::renderOffscreen with a fixed camera, so
// probes sit at known positions regardless of the live view.

#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwGeometry.h"
#include "cwOffscreenRenderParameters.h"
#include "cwRenderSpriteBillboards.h"
#include "cwRenderTextBillboards.h"
#include "cwRenderTexturedItems.h"
#include "cwRhiViewer.h"
#include "cwScene.h"
#include "cwSpriteBillboardHandle.h"
#include "TestHelper.h"

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
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QStringList>
#include <QThread>
#include <QVector2D>
#include <QVector3D>

//Std includes
#include <atomic>
#include <memory>

namespace {

constexpr int kWindowSize = 400;
const QSize kOutputSize(256, 256);

// Orthographic camera on +z looking at the origin; kOrthoHalfExtent meters
// span half the output. The perspective camera shares the eye.
constexpr float kOrthoHalfExtent = 10.0f;
constexpr float kOrthoDepthExtent = 1000.0f;
constexpr float kEyeDistance = 50.0f;
constexpr float kFieldOfViewDegrees = 60.0f;
constexpr float kAspectRatio = 1.0f;
constexpr float kNearPlane = 1.0f;
constexpr float kFarPlane = 1000.0f;

constexpr int kSpriteSize = 20;
constexpr int kSpriteHalfSize = kSpriteSize / 2;
constexpr float kSpriteDepthBias = 1.0f;
constexpr float kFadedOpacity = 0.6f;

constexpr int kLabelPixelSize = 20;
const QString kLabelText = QStringLiteral("WWW");

// The wall sits at kWallZ facing +z; a sprite at kWallFrontZ is in front of it,
// one at kWallBehindZ stays behind it by more than its depth bias.
constexpr float kWallZ = 3.0f;
constexpr float kWallHalfExtent = 3.0f;
constexpr int kWallTextureSize = 4;
const QVector3D kWallCenter(6.0f, 0.0f, kWallZ);
constexpr float kWallFrontZ = 5.0f;
constexpr float kWallBehindZ = 0.0f;

// The open sprite sits where nothing else draws; labels go a little behind or
// in front of it.
const QVector3D kOpenSprite(0.0f, 0.0f, 0.0f);

// The second atlas slot's instance sits left of the open one, clear of the wall
// and of the open sprite's extent.
const QVector3D kSecondSlotSprite(-6.0f, 0.0f, 0.0f);
constexpr int kOpenSlot = 0;
constexpr int kSecondSlot = 1;
constexpr float kLabelBehindZ = -2.0f;
constexpr float kLabelFrontZ = 2.0f;

// Probe rows inside the sprite's two halves, measured from its center, and the
// margin kept from every edge so the probes stay clear of sampling seams.
constexpr int kHalfProbeOffset = 5;
constexpr int kEdgeMargin = 2;
constexpr int kProbeHalfWidth = kSpriteHalfSize - kEdgeMargin;
constexpr int kProbeHalfHeight = kSpriteHalfSize / 2 - kEdgeMargin;

// The device pixel ratio of the scaled offscreen job: the sprite doubles to 40
// pixels, so a probe 15 pixels from its center lands inside the top half at
// this ratio and outside the sprite at ratio 1.
constexpr float kScaledDevicePixelRatio = 2.0f;
constexpr int kScaledProbeOffset = 15;

// A channel above this is lit, below it is dark.
constexpr int kLitChannel = 128;
constexpr int kDarkChannel = 64;
constexpr int kMaxChannel = 255;

constexpr int kWaitTimeoutMs = 30000;
constexpr int kSettleFrames = 5;

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

QMatrix4x4 orthoProjection()
{
    QMatrix4x4 projection;
    projection.ortho(-kOrthoHalfExtent, kOrthoHalfExtent, -kOrthoHalfExtent, kOrthoHalfExtent,
                     -kOrthoDepthExtent, kOrthoDepthExtent);
    return projection;
}

QMatrix4x4 perspectiveProjection()
{
    QMatrix4x4 projection;
    projection.perspective(kFieldOfViewDegrees, kAspectRatio, kNearPlane, kFarPlane);
    return projection;
}

// Output pixel of a world point under @a projection, y down.
QPoint toPixel(const QVector3D& worldPosition, const QMatrix4x4& projection)
{
    const QVector3D ndc = (projection * cameraView() * QVector4D(worldPosition, 1.0f))
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
        geometry.set(position, i, kWallCenter + corners.at(i));
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

cwRenderSpriteBillboards::SpriteBillboard makeSprite(const QVector3D& position, float depthBias = 0.0f,
                                                     float opacity = 1.0f)
{
    cwRenderSpriteBillboards::SpriteBillboard billboard;
    billboard.worldPosition = position;
    billboard.depthBias = depthBias;
    billboard.opacity = opacity;
    billboard.visible = true;
    return billboard;
}

cwRenderSpriteBillboards::SpriteBillboard makeSlotSprite(const QVector3D& position, int spriteIndex)
{
    cwRenderSpriteBillboards::SpriteBillboard billboard = makeSprite(position);
    billboard.spriteIndex = spriteIndex;
    return billboard;
}

// A solid square prototype of the sprite size in @a color.
QByteArray solidPrototypeQml(const QByteArray& color)
{
    return "import QtQuick\n"
           "Rectangle {\n"
           "  width: " + QByteArray::number(kSpriteSize) + "\n"
           "  height: " + QByteArray::number(kSpriteSize) + "\n"
           "  color: \"" + color + "\"\n"
           "}\n";
}

cwRenderTextBillboards::TextBillboard makeLabel(const QVector3D& position)
{
    cwRenderTextBillboards::TextBillboard billboard;
    billboard.text = kLabelText;
    billboard.worldPosition = position;
    billboard.depthBias = 0.0f;
    return billboard;
}

bool isBlue(QRgb pixel)
{
    return qBlue(pixel) > kLitChannel && qRed(pixel) < kDarkChannel && qGreen(pixel) < kDarkChannel;
}

bool isRed(QRgb pixel)
{
    return qRed(pixel) > kLitChannel && qBlue(pixel) < kDarkChannel && qGreen(pixel) < kDarkChannel;
}

bool isGreen(QRgb pixel)
{
    return qGreen(pixel) > kLitChannel && qRed(pixel) < kDarkChannel && qBlue(pixel) < kDarkChannel;
}

bool isBackground(QRgb pixel)
{
    return pixel == qRgb(0, 0, 0);
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

// Centers of the probe boxes over the sprite's two halves.
QPoint topHalf(const QPoint& spriteCenter)
{
    return spriteCenter + QPoint(0, -kHalfProbeOffset);
}

QPoint bottomHalf(const QPoint& spriteCenter)
{
    return spriteCenter + QPoint(0, kHalfProbeOffset);
}

} // namespace

TEST_CASE("cwRenderSpriteBillboards renders one instanced, silhouette-depth sprite layer",
          "[cwRenderSpriteBillboards][rhi]")
{
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        SKIP("needs a GPU-backed window; the offscreen QPA has no QRhi");
    }

    RenderWarningCapture warnings;

    cwScene scene;

    QQmlEngine engine;
    QQuickWindow window;
    window.resize(kWindowSize, kWindowSize);

    auto* viewer = new cwRhiViewer();
    viewer->setParentItem(window.contentItem());
    viewer->setSize(QSizeF(kWindowSize, kWindowSize));
    viewer->setScene(&scene);

    // The prototype: blue over its top half, transparent below.
    const QByteArray prototypeQml =
        "import QtQuick\n"
        "Item {\n"
        "  width: " + QByteArray::number(kSpriteSize) + "\n"
        "  height: " + QByteArray::number(kSpriteSize) + "\n"
        "  Rectangle { width: parent.width; height: parent.height / 2; color: \"#0000ff\" }\n"
        "}\n";
    QQmlComponent component(&engine);
    component.setData(prototypeQml, QUrl());
    std::unique_ptr<QObject> prototypeObject(component.create());
    INFO(component.errorString().toStdString());
    REQUIRE(prototypeObject != nullptr);
    auto* prototype = qobject_cast<QQuickItem*>(prototypeObject.get());
    REQUIRE(prototype != nullptr);
    prototype->setParentItem(window.contentItem());

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

    const auto renderImage = [&](const QMatrix4x4& projection, float devicePixelRatio = 1.0f) {
        cwOffscreenRenderParameters parameters;
        parameters.viewMatrix = cameraView();
        parameters.projectionMatrix = projection;
        parameters.outputSize = kOutputSize;
        parameters.devicePixelRatio = devicePixelRatio;
        parameters.backgroundColor = QColor::fromRgbF(0.0, 0.0, 0.0, 1.0);
        QFuture<QImage> future = scene.renderOffscreen(parameters);
        const bool finished = waitUntil([&]() {
            viewer->update();
            return future.isFinished();
        }, kWaitTimeoutMs);
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
    const QMatrix4x4 ortho = orthoProjection();
    const QImage baseline = renderImage(ortho);

    // The wall reached the image and the open spot is empty.
    const QPoint wallPixel = toPixel(kWallCenter, ortho);
    const QPoint openPixel = toPixel(kOpenSprite, ortho);
    REQUIRE(isRed(baseline.pixel(wallPixel)));
    REQUIRE(isBackground(baseline.pixel(openPixel)));

    cwRenderSpriteBillboards* layer = scene.spriteBillboardLayer();
    REQUIRE(layer != nullptr);
    layer->setWindow(&window);
    layer->setSprite(prototype);
    REQUIRE(layer->spriteSize() == QSizeF(kSpriteSize, kSpriteSize));

    cwRenderTextBillboards* textLayer = scene.textBillboardLayer();
    REQUIRE(textLayer != nullptr);
    textLayer->setWindow(&window);
    cwRenderTextBillboards::Style style;
    style.font.setPixelSize(kLabelPixelSize);
    style.color = Qt::white;
    style.outlineColor = Qt::black;
    textLayer->setStyle(style);

    SECTION("a sprite in front of the wall shows blue over its top half and the wall through its bottom half")
    {
        cwSpriteBillboardHandle sprite;
        sprite.set(layer, makeSprite(QVector3D(kWallCenter.x(), kWallCenter.y(), kWallFrontZ)));
        REQUIRE(framesFrom(kSettleFrames));
        const QImage image = renderImage(ortho);

        CHECK(isBlue(image.pixel(topHalf(wallPixel))));
        CHECK(isRed(image.pixel(bottomHalf(wallPixel))));
    }

    SECTION("a sprite behind the wall is depth-tested away")
    {
        cwSpriteBillboardHandle sprite;
        sprite.set(layer, makeSprite(QVector3D(kWallCenter.x(), kWallCenter.y(), kWallBehindZ),
                                     kSpriteDepthBias));
        REQUIRE(framesFrom(kSettleFrames));
        const QImage image = renderImage(ortho);

        CHECK(isRed(image.pixel(topHalf(wallPixel))));
        CHECK(isRed(image.pixel(bottomHalf(wallPixel))));
        CHECK(differingPixels(image, baseline, wallPixel) == 0);
    }

    SECTION("a label behind a sprite shows through the transparent half and hides under the ink")
    {
        cwSpriteBillboardHandle sprite;
        sprite.set(layer, makeSprite(kOpenSprite));
        REQUIRE(framesFrom(kSettleFrames));
        const QImage spriteOnly = renderImage(ortho);
        REQUIRE(isBlue(spriteOnly.pixel(topHalf(openPixel))));
        REQUIRE(isBackground(spriteOnly.pixel(bottomHalf(openPixel))));

        const cwBillboardId label = textLayer->addBillboard(
            makeLabel(QVector3D(kOpenSprite.x(), kOpenSprite.y(), kLabelBehindZ)));
        REQUIRE(framesFrom(kSettleFrames));
        const QImage withLabel = renderImage(ortho);

        // The glyphs span both halves; under the transparent half they show, under
        // the blue they lose the depth test.
        CHECK(differingPixels(withLabel, spriteOnly, bottomHalf(openPixel)) > 0);
        CHECK(differingPixels(withLabel, spriteOnly, topHalf(openPixel)) == 0);
        textLayer->removeBillboard(label);
    }

    SECTION("a label in front of a sprite paints over the blue")
    {
        cwSpriteBillboardHandle sprite;
        sprite.set(layer, makeSprite(kOpenSprite));
        REQUIRE(framesFrom(kSettleFrames));
        const QImage spriteOnly = renderImage(ortho);
        REQUIRE(isBlue(spriteOnly.pixel(topHalf(openPixel))));

        const cwBillboardId label = textLayer->addBillboard(
            makeLabel(QVector3D(kOpenSprite.x(), kOpenSprite.y(), kLabelFrontZ)));
        REQUIRE(framesFrom(kSettleFrames));
        const QImage withLabel = renderImage(ortho);

        CHECK(differingPixels(withLabel, spriteOnly, topHalf(openPixel)) > 0);
        textLayer->removeBillboard(label);
    }

    SECTION("the drawn extent is the prototype's size under both projections")
    {
        cwSpriteBillboardHandle sprite;
        sprite.set(layer, makeSprite(kOpenSprite));
        REQUIRE(framesFrom(kSettleFrames));

        const QMatrix4x4 perspective = perspectiveProjection();
        for (const QMatrix4x4& projection : {ortho, perspective}) {
            const QImage image = renderImage(projection);
            const QPoint center = toPixel(kOpenSprite, projection);
            const int insideX = kSpriteHalfSize - kEdgeMargin;
            const int outsideX = kSpriteHalfSize + kEdgeMargin;
            const int topRow = center.y() - kHalfProbeOffset;
            INFO("orthographic: " << (projection == ortho));
            CHECK(isBlue(image.pixel(center.x() - insideX, topRow)));
            CHECK(isBlue(image.pixel(center.x() + insideX, topRow)));
            CHECK(isBackground(image.pixel(center.x() - outsideX, topRow)));
            CHECK(isBackground(image.pixel(center.x() + outsideX, topRow)));
            // The bias moves the sprite toward the viewer, never sideways.
            CHECK(isBackground(image.pixel(center.x(), center.y() + kSpriteHalfSize + kEdgeMargin)));
            CHECK(isBackground(image.pixel(center.x(), center.y() - kSpriteHalfSize - kEdgeMargin)));
        }
    }

    SECTION("a bottom-center anchor puts the sprite's bottom edge on its position")
    {
        // The blue top half then spans one to one-half sprite heights above the
        // station, and nothing draws below it, under both projections.
        const QPointF bottomCenter(0.5, 1.0);
        layer->setAnchor(bottomCenter);
        cwSpriteBillboardHandle sprite;
        sprite.set(layer, makeSprite(kOpenSprite));
        REQUIRE(framesFrom(kSettleFrames));

        const int blueRowAbove = kSpriteSize - kHalfProbeOffset;
        const int clearRowBelow = kEdgeMargin;
        const QMatrix4x4 perspective = perspectiveProjection();
        for (const QMatrix4x4& projection : {ortho, perspective}) {
            const QImage image = renderImage(projection);
            const QPoint station = toPixel(kOpenSprite, projection);
            INFO("orthographic: " << (projection == ortho));
            CHECK(isBlue(image.pixel(station.x(), station.y() - blueRowAbove)));
            CHECK(isBackground(image.pixel(station.x(), station.y() + clearRowBelow)));
            CHECK(isBackground(image.pixel(station.x(), station.y() - kSpriteSize - kEdgeMargin)));
        }
    }

    SECTION("an offscreen job's device pixel ratio scales the sprite's pixel extent")
    {
        cwSpriteBillboardHandle sprite;
        sprite.set(layer, makeSprite(kOpenSprite));
        REQUIRE(framesFrom(kSettleFrames));

        const QPoint probe = openPixel + QPoint(kScaledProbeOffset, -kScaledProbeOffset);
        const QImage unscaled = renderImage(ortho);
        CHECK(isBackground(unscaled.pixel(probe)));

        const QImage scaled = renderImage(ortho, kScaledDevicePixelRatio);
        CHECK(isBlue(scaled.pixel(probe)));
        CHECK(isBlue(scaled.pixel(topHalf(openPixel))));
    }

    SECTION("opacity fades the sprite")
    {
        cwSpriteBillboardHandle sprite;
        sprite.set(layer, makeSprite(kOpenSprite, 0.0f, kFadedOpacity));
        REQUIRE(framesFrom(kSettleFrames));
        const QImage image = renderImage(ortho);

        const QRgb pixel = image.pixel(topHalf(openPixel));
        CHECK(qBlue(pixel) > kLitChannel);
        CHECK(qBlue(pixel) < kMaxChannel - kDarkChannel);
        CHECK(qRed(pixel) < kDarkChannel);
    }

    SECTION("a two-slot atlas draws each instance from its own slot")
    {
        const auto createSolid = [&](const QByteArray& color) {
            QQmlComponent solidComponent(&engine);
            solidComponent.setData(solidPrototypeQml(color), QUrl());
            std::unique_ptr<QObject> object(solidComponent.create());
            INFO(solidComponent.errorString().toStdString());
            REQUIRE(object != nullptr);
            auto* item = qobject_cast<QQuickItem*>(object.get());
            REQUIRE(item != nullptr);
            item->setParentItem(window.contentItem());
            return object;
        };
        std::unique_ptr<QObject> blueObject = createSolid("#0000ff");
        std::unique_ptr<QObject> redObject = createSolid("#ff0000");
        auto* blue = qobject_cast<QQuickItem*>(blueObject.get());
        auto* red = qobject_cast<QQuickItem*>(redObject.get());

        layer->setSprites({blue, red});
        CHECK(layer->spriteCount() == 2);
        REQUIRE(layer->spriteSize() == QSizeF(kSpriteSize, kSpriteSize));

        cwSpriteBillboardHandle openInstance;
        cwSpriteBillboardHandle secondInstance;
        openInstance.set(layer, makeSlotSprite(kOpenSprite, kOpenSlot));
        secondInstance.set(layer, makeSlotSprite(kSecondSlotSprite, kSecondSlot));
        REQUIRE(framesFrom(kSettleFrames));

        const QPoint secondPixel = toPixel(kSecondSlotSprite, ortho);
        const QImage image = renderImage(ortho);
        CHECK(isBlue(image.pixel(openPixel)));
        CHECK(isRed(image.pixel(secondPixel)));

        // Re-skinning slot 1 re-renders its cell; slot 0's instance is unchanged.
        red->setProperty("color", QColor(Qt::green));
        REQUIRE(framesFrom(kSettleFrames));
        const QImage reskinned = renderImage(ortho);
        CHECK(isGreen(reskinned.pixel(secondPixel)));
        CHECK(isBlue(reskinned.pixel(openPixel)));
        CHECK(differingPixels(reskinned, image, openPixel) == 0);

        // Hand the layer back its single prototype before these items go.
        layer->setSprite(prototype);
    }

    SECTION("unbinding and rebinding the window drops and rebuilds the layer texture")
    {
        cwSpriteBillboardHandle sprite;
        sprite.set(layer, makeSprite(kOpenSprite));
        REQUIRE(framesFrom(kSettleFrames));
        const QImage bound = renderImage(ortho);
        REQUIRE(isBlue(bound.pixel(topHalf(openPixel))));

        layer->setWindow(nullptr);
        REQUIRE(framesFrom(kSettleFrames));
        const QImage unbound = renderImage(ortho);
        CHECK(isBackground(unbound.pixel(topHalf(openPixel))));

        layer->setWindow(&window);
        REQUIRE(framesFrom(kSettleFrames));
        const QImage rebound = renderImage(ortho);
        CHECK(isBlue(rebound.pixel(topHalf(openPixel))));
    }

    const QStringList captured = RenderWarningCapture::captured();
    INFO("render warnings: " << captured.join(QStringLiteral("\n")).toStdString());
    CHECK(captured.isEmpty());

    window.hide();
}
