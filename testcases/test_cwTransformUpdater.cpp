//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwTransformUpdater.h"
#include "cwCamera.h"

//Qt includes
#include <QQuickItem>
#include <QVector3D>

namespace {

    constexpr int kViewportSize = 1000;

    //One model unit sideways - moves the projection well past a pixel without
    //pushing the point off screen
    constexpr qreal kPanDistance = 1.0;

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

TEST_CASE("cwTransformUpdater stops following the camera while disabled", "[cwTransformUpdater]") {
    QObject parent;
    cwCamera* camera = makeCenteredCamera(&parent);

    cwTransformUpdater updater;
    updater.setCamera(camera);

    TestItemRoot root;
    TestPositionItem* inFront = root.addPoint(QVector3D(0, 0, -10));
    updater.addPointItem(inFront);

    REQUIRE(inFront->isVisible() == true);
    const QPointF originalPosition = inFront->position();

    //cwItem3DRepeater turns this off while the points are hidden, so panning the
    //camera behind a hidden note reprojects nothing.
    updater.setEnabled(false);

    QMatrix4x4 panned;
    panned.translate(kPanDistance, 0.0, 0.0);
    camera->setViewMatrix(panned);
    CHECK(inFront->position() == originalPosition);

    //Re-enabling has to resync everything the camera moved past
    updater.setEnabled(true);
    CHECK(inFront->position() != originalPosition);
}

TEST_CASE("cwTransformUpdater tolerates points added without a camera", "[cwTransformUpdater]") {
    cwTransformUpdater updater; //No camera set

    TestItemRoot root;
    TestPositionItem* item = root.addPoint(QVector3D(0, 0, -10));
    updater.addPointItem(item);

    //Nothing to project against yet, so the point keeps its declared position
    CHECK(item->position() == QPointF(0, 0));

    updater.setCamera(makeCenteredCamera(&updater));
    CHECK(item->isVisible() == true);
}

#include "test_cwTransformUpdater.moc"
