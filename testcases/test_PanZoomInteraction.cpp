// Pins PanZoomInteraction's pinch zoom (#747). A trackpad pinch used to jump:
// PinchHandler scaled from its own persistentScale, stale after any wheel zoom,
// and a mouse press mid-gesture either drove the scale to -DBL_MAX or
// deactivated the handler so later zoom steps restarted from position (0,0).
//
// Events are sent straight to an unshown window, so delivery is synchronous.

// Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

// Qt includes
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPointingDevice>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>
#include <QWheelEvent>

// Std includes
#include <cmath>
#include <memory>

using namespace Catch;

namespace {

constexpr int kWindowWidth = 800;
constexpr int kWindowHeight = 600;
constexpr qreal kImageX = 200.0;
constexpr qreal kImageY = 150.0;
constexpr qreal kImageWidth = 400.0;
constexpr qreal kImageHeight = 300.0;

const QPointF kCursor(300.0, 250.0);

// One mouse-wheel notch, in eighths of a degree
constexpr int kWheelNotch = 120;
constexpr int kWheelNotchCount = 5;

// The relative change one trackpad zoom step reports
constexpr qreal kZoomStep = 0.01;
constexpr int kTrackpadFingerCount = 2;

constexpr qreal kDragDistance = 100.0;
constexpr int kDragStepCount = 10;

constexpr qint64 kMouseSystemId = 747;
constexpr int kMouseButtonCount = 3;

constexpr qreal kPresetScale = 2.0;
constexpr qreal kPositionTolerance = 0.01;

QString panZoomQml()
{
    return QStringLiteral(
               "import QtQuick\n"
               "import cavewherelib\n"
               "Item {\n"
               "  width: %1; height: %2\n"
               "  ImageBackground {\n"
               "    id: background\n"
               "    objectName: \"background\"\n"
               "    x: %3; y: %4; width: %5; height: %6\n"
               "  }\n"
               "  PanZoomInteraction {\n"
               "    anchors.fill: parent\n"
               "    target: background\n"
               "    enabled: true\n"
               "  }\n"
               "}\n")
        .arg(kWindowWidth)
        .arg(kWindowHeight)
        .arg(kImageX)
        .arg(kImageY)
        .arg(kImageWidth)
        .arg(kImageHeight);
}

struct PanZoomFixture
{
    QQmlEngine engine;
    QQuickWindow window;
    std::unique_ptr<QObject> root;
    QQuickItem* image = nullptr;
    QPointingDevice* trackpad = QTest::createTouchDevice(QInputDevice::DeviceType::TouchPad);
    // Its own mouse: primaryPointingDevice() is whichever pointing device was
    // registered first, which can be the trackpad above
    QPointingDevice mouse{QStringLiteral("test mouse"), kMouseSystemId,
                          QInputDevice::DeviceType::Mouse, QPointingDevice::PointerType::Generic,
                          QInputDevice::Capability::Position | QInputDevice::Capability::Scroll,
                          1, kMouseButtonCount};

    PanZoomFixture()
    {
        window.resize(kWindowWidth, kWindowHeight);

        QQmlComponent component(&engine);
        component.setData(panZoomQml().toUtf8(), QUrl());
        root.reset(component.create());
        INFO(component.errorString().toStdString());
        REQUIRE(root != nullptr);
        auto* rootItem = qobject_cast<QQuickItem*>(root.get());
        REQUIRE(rootItem != nullptr);
        rootItem->setParentItem(window.contentItem());

        image = root->findChild<QQuickItem*>(QStringLiteral("background"));
        REQUIRE(image != nullptr);
    }

    void gesture(Qt::NativeGestureType type, qreal value = 0.0)
    {
        QNativeGestureEvent event(type, trackpad, kTrackpadFingerCount, kCursor, kCursor,
                                  window.mapToGlobal(kCursor), value, QPointF());
        QCoreApplication::sendEvent(&window, &event);
    }

    void zoomStep() { gesture(Qt::ZoomNativeGesture, kZoomStep); }

    void wheelNotch()
    {
        QWheelEvent event(kCursor, window.mapToGlobal(kCursor), QPoint(), QPoint(0, kWheelNotch),
                          Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false,
                          Qt::MouseEventNotSynthesized, &mouse);
        QCoreApplication::sendEvent(&window, &event);
    }

    void mouseEvent(QEvent::Type type, const QPointF& position)
    {
        const Qt::MouseButtons buttons = type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton;
        const Qt::MouseButton button = type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton;
        QMouseEvent event(type, position, position, window.mapToGlobal(position),
                          button, buttons, Qt::NoModifier, &mouse);
        QCoreApplication::sendEvent(&window, &event);
    }

    void dragRight()
    {
        mouseEvent(QEvent::MouseButtonPress, kCursor);
        for (int i = 1; i <= kDragStepCount; ++i) {
            mouseEvent(QEvent::MouseMove, kCursor + QPointF(kDragDistance * i / kDragStepCount, 0.0));
        }
        mouseEvent(QEvent::MouseButtonRelease, kCursor + QPointF(kDragDistance, 0.0));
    }

    // The touchscreen PinchHandler. Its own transform math is stock Qt; what
    // PanZoomInteraction owes it is a scale basis that follows the live scale.
    QObject* touchPinchHandler() const
    {
        const QList<QObject*> objects = root->findChildren<QObject*>();
        for (QObject* object : objects) {
            if (object->inherits("QQuickPinchHandler")
                && object->property("acceptedDevices").toInt() == int(QInputDevice::DeviceType::TouchScreen)) {
                return object;
            }
        }
        return nullptr;
    }

    // The image point under the cursor; a zoom step must leave it there
    QPointF imagePointUnderCursor() const { return image->mapFromScene(kCursor); }
};

void requireUnderCursor(const PanZoomFixture& fixture, const QPointF& imagePoint)
{
    const QPointF scenePoint = fixture.image->mapToScene(imagePoint);
    CHECK(scenePoint.x() == Approx(kCursor.x()).margin(kPositionTolerance));
    CHECK(scenePoint.y() == Approx(kCursor.y()).margin(kPositionTolerance));
}

void requireFiniteTransform(const QQuickItem* image)
{
    REQUIRE(std::isfinite(image->scale()));
    REQUIRE(image->scale() > 0.0);
    REQUIRE(std::isfinite(image->x()));
    REQUIRE(std::isfinite(image->y()));
}

} // namespace

TEST_CASE("A trackpad pinch continues from a wheel zoom", "[PanZoomInteraction]")
{
    PanZoomFixture fixture;

    for (int i = 0; i < kWheelNotchCount; ++i) {
        fixture.wheelNotch();
    }
    const qreal wheelScale = fixture.image->scale();
    REQUIRE(wheelScale > 1.0);

    const QPointF anchor = fixture.imagePointUnderCursor();
    fixture.gesture(Qt::BeginNativeGesture);
    fixture.zoomStep();

    CHECK(fixture.image->scale() == Approx(wheelScale * (1.0 + kZoomStep)));
    requireUnderCursor(fixture, anchor);

    fixture.gesture(Qt::EndNativeGesture);
}

TEST_CASE("A mouse click during a trackpad pinch keeps the zoom continuous", "[PanZoomInteraction]")
{
    PanZoomFixture fixture;

    fixture.gesture(Qt::BeginNativeGesture);
    fixture.zoomStep();
    const qreal pinchScale = fixture.image->scale();

    SECTION("A press keeps the transform finite and in place")
    {
        const QPointF positionBeforePress = fixture.image->position();
        fixture.mouseEvent(QEvent::MouseButtonPress, kCursor);

        requireFiniteTransform(fixture.image);
        CHECK(fixture.image->scale() == Approx(pinchScale));
        CHECK(fixture.image->x() == Approx(positionBeforePress.x()));
        CHECK(fixture.image->y() == Approx(positionBeforePress.y()));

        fixture.mouseEvent(QEvent::MouseButtonRelease, kCursor);
    }

    SECTION("A drag pans, and the next zoom step starts from the panned position")
    {
        const QPointF positionBeforeDrag = fixture.image->position();
        fixture.dragRight();
        CHECK(fixture.image->x() == Approx(positionBeforeDrag.x() + kDragDistance));
        CHECK(fixture.image->y() == Approx(positionBeforeDrag.y()));

        const QPointF anchor = fixture.imagePointUnderCursor();
        fixture.zoomStep();

        requireFiniteTransform(fixture.image);
        CHECK(fixture.image->scale() == Approx(pinchScale * (1.0 + kZoomStep)));
        requireUnderCursor(fixture, anchor);
    }

    fixture.gesture(Qt::EndNativeGesture);
}

TEST_CASE("The touchscreen pinch starts from the live scale", "[PanZoomInteraction]")
{
    // A touchscreen pinch scales from its persistentScale, so a stale value
    // snaps the image back to it on the pinch's first step
    PanZoomFixture fixture;
    QObject* touchPinch = fixture.touchPinchHandler();
    REQUIRE(touchPinch != nullptr);
    const auto basis = [touchPinch] { return touchPinch->property("persistentScale").toDouble(); };

    SECTION("After a wheel zoom")
    {
        fixture.wheelNotch();
        CHECK(basis() == Approx(fixture.image->scale()));
    }

    SECTION("After a trackpad pinch")
    {
        fixture.gesture(Qt::BeginNativeGesture);
        fixture.zoomStep();
        fixture.gesture(Qt::EndNativeGesture);
        CHECK(basis() == Approx(fixture.image->scale()));
    }

    SECTION("After another tool sets the scale")
    {
        fixture.image->setScale(kPresetScale);
        CHECK(basis() == Approx(kPresetScale));
    }
}
