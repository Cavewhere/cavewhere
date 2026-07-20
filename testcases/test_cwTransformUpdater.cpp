//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwTransformUpdater.h"
#include "cwCamera.h"
#include "cwSignalSpy.h"

//Qt includes
#include <QQuickItem>
#include <QVector3D>

namespace {

    constexpr int kViewportSize = 1000;

    /**
     * Minimal stand-in for a station delegate. cwTransformUpdater reads
     * "position3D" off the item and connects to position3DChanged().
     */
    class TestPositionItem : public QQuickItem
    {
        Q_OBJECT
        Q_PROPERTY(QVector3D position3D READ position3D WRITE setPosition3D NOTIFY position3DChanged)

    public:
        explicit TestPositionItem(const QVector3D& position3D = QVector3D())
            : m_position3D(position3D)
        {
        }

        QVector3D position3D() const { return m_position3D; }

        void setPosition3D(const QVector3D& position3D)
        {
            if(m_position3D == position3D) {
                return;
            }
            m_position3D = position3D;
            emit position3DChanged();
        }

    signals:
        void position3DChanged();

    private:
        QVector3D m_position3D;
    };

    /**
     * QQuickItem::isVisible() is the *effective* visibility, which is false for
     * any item without a parent. Real delegates are parented into the view, so
     * the tests need a visible root for setVisible() to mean anything.
     */
    class TestItemRoot : public QQuickItem
    {
    public:
        TestPositionItem* addPoint(const QVector3D& position3D)
        {
            auto item = new TestPositionItem(position3D);
            item->setParentItem(this);
            return item;
        }
    };

    cwCamera* makeCenteredCamera(QObject* parent)
    {
        auto camera = new cwCamera(parent);

        cwProjection projection;
        projection.setPerspective(55, 1.0, 1.0, 10000);

        camera->setProjection(projection);
        camera->setViewport(QRect(0, 0, kViewportSize, kViewportSize));
        camera->setViewMatrix(QMatrix4x4()); //Default view looking down the -z axis

        return camera;
    }
}

TEST_CASE("cwTransformUpdater shows points inside the viewport by default", "[cwTransformUpdater]") {
    QObject parent;
    cwTransformUpdater updater;
    updater.setCamera(makeCenteredCamera(&parent));

    CHECK(updater.pointsVisible() == true);

    TestItemRoot root;
    TestPositionItem* inFront = root.addPoint(QVector3D(0, 0, -10)); //Down the -z axis, dead center
    updater.addPointItem(inFront);

    CHECK(inFront->isVisible() == true);
}

TEST_CASE("cwTransformUpdater hides points behind the camera", "[cwTransformUpdater]") {
    QObject parent;
    cwTransformUpdater updater;
    updater.setCamera(makeCenteredCamera(&parent));

    TestItemRoot root;
    TestPositionItem* behind = root.addPoint(QVector3D(0, 0, 10)); //Behind the camera
    updater.addPointItem(behind);

    CHECK(behind->isVisible() == false);
}

TEST_CASE("cwTransformUpdater pointsVisible hides every point regardless of clipping", "[cwTransformUpdater]") {
    QObject parent;
    cwTransformUpdater updater;
    updater.setCamera(makeCenteredCamera(&parent));

    TestItemRoot root;
    TestPositionItem* inFront = root.addPoint(QVector3D(0, 0, -10));
    TestPositionItem* behind = root.addPoint(QVector3D(0, 0, 10));
    updater.addPointItem(inFront);
    updater.addPointItem(behind);

    REQUIRE(inFront->isVisible() == true);
    REQUIRE(behind->isVisible() == false);

    cwSignalSpy visibleSpy(&updater, &cwTransformUpdater::pointsVisibleChanged);

    updater.setPointsVisible(false);

    CHECK(visibleSpy.size() == 1);
    CHECK(inFront->isVisible() == false);
    CHECK(behind->isVisible() == false);

    //Turning it back on must restore clipping-derived visibility, not blanket-show
    updater.setPointsVisible(true);

    CHECK(visibleSpy.size() == 2);
    CHECK(inFront->isVisible() == true);
    CHECK(behind->isVisible() == false);
}

TEST_CASE("cwTransformUpdater setPointsVisible is a no-op when unchanged", "[cwTransformUpdater]") {
    QObject parent;
    cwTransformUpdater updater;
    updater.setCamera(makeCenteredCamera(&parent));

    cwSignalSpy visibleSpy(&updater, &cwTransformUpdater::pointsVisibleChanged);

    updater.setPointsVisible(true); //Already the default
    CHECK(visibleSpy.size() == 0);

    updater.setPointsVisible(false);
    updater.setPointsVisible(false);
    CHECK(visibleSpy.size() == 1);
}

TEST_CASE("cwTransformUpdater hides points added while pointsVisible is false", "[cwTransformUpdater]") {
    QObject parent;
    cwTransformUpdater updater;
    updater.setCamera(makeCenteredCamera(&parent));
    updater.setPointsVisible(false);

    TestItemRoot root;
    TestPositionItem* inFront = root.addPoint(QVector3D(0, 0, -10)); //Would be visible if not gated
    updater.addPointItem(inFront);

    CHECK(inFront->isVisible() == false);
}

TEST_CASE("cwTransformUpdater applies pointsVisible while disabled", "[cwTransformUpdater]") {
    QObject parent;
    cwTransformUpdater updater;
    updater.setCamera(makeCenteredCamera(&parent));

    TestItemRoot root;
    TestPositionItem* inFront = root.addPoint(QVector3D(0, 0, -10));
    updater.addPointItem(inFront);
    REQUIRE(inFront->isVisible() == true);

    //cwItem3DRepeater drives enabled from its own isVisible(), so hiding must
    //still take effect while updates are switched off - and unhiding must too.
    updater.setEnabled(false);

    updater.setPointsVisible(false);
    CHECK(inFront->isVisible() == false);

    updater.setPointsVisible(true);
    CHECK(inFront->isVisible() == true);
}

TEST_CASE("cwTransformUpdater applies pointsVisible without a camera", "[cwTransformUpdater]") {
    cwTransformUpdater updater; //No camera set

    TestItemRoot root;
    TestPositionItem* item = root.addPoint(QVector3D(0, 0, -10));
    updater.addPointItem(item);

    //update() bails without a camera, so hiding has to be applied directly
    updater.setPointsVisible(false);
    CHECK(item->isVisible() == false);
}

#include "test_cwTransformUpdater.moc"
